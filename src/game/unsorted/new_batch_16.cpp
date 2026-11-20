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
void func_08B0A9F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 result = 0u;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        const u32 lock = self + 0x48u;
        ctx.gpr[16] = self;
        ctx.gpr[18] = lock;
        ctx.gpr[17] = 0u;
        call_keep(rt, ctx, 0x08B344E0u, {lock});
        u32 index = word(rt, self + 4u);
        const u32 end = word(rt, self);
        for (;;) {
            const u32 next = (index + 1u) & 0xFu;
            result = word(rt, self + index * 4u + 8u);
            ctx.gpr[17] = result;
            if (next == end) break;
            index = next;
        }
        call_keep(rt, ctx, 0x08B3457Cu, {lock});
    }
    ctx.gpr[2] = result;
}

// 0x08B2DEE4: three guest calls on three stack objects (0x08C7B008(&first), 0x08C7B000(&first),
// 0x08C7B010(&third, &second)); a nonzero answer either time returns -1 in v0 and v1, everything zero goes on in
// 0x08B2DE94(&third) and returns its answer.
void func_08B2DEE4(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 first = frame.at(0u);
    const u32 second = frame.at(8u);
    const u32 third = frame.at(0x10u);
    ctx.gpr[18] = third;
    ctx.gpr[16] = second;
    if (call_set(rt, ctx, 0x08C7B008u, {first}) != 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[3] = 0xFFFFFFFFu;
        return;
    }
    call_set(rt, ctx, 0x08C7B000u, {first});
    if (call_set(rt, ctx, 0x08C7B010u, {third, second}) != 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[3] = 0xFFFFFFFFu;
        return;
    }
    call_keep(rt, ctx, 0x08B2DE94u, {third});
}

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
void func_08B56D4C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 flags = word(rt, self + 0xD1Cu);
    if ((flags & 0x400u) != 0u) {
        put(rt, self + 0xD1Cu, flags & ~0x400u);  // the jal's delay slot
        const u32 matrix = call_set(rt, ctx, 0x08B56A88u, {self});
        for (unsigned column = 0u; column < 4u; ++column)
            gt::vfpu::load_quad(rt, ctx, 8u + column, matrix + 0x10u * column);
        const u32 other = word(rt, self + 0xD00u);
        for (unsigned column = 0u; column < 4u; ++column)
            gt::vfpu::load_quad(rt, ctx, column, other + 0x10u * column);
        gt::vfpu::matrix_multiply(ctx, 0u, 8u, 4u, 4u);
        for (unsigned column = 0u; column < 4u; ++column)
            gt::vfpu::store_quad(rt, ctx, column, self + 0x840u + 0x10u * column);
    }
    ctx.gpr[2] = self + 0x800u;
}

// 0x08B62498 (a0 = a block + 0x50, a1 / a2 = two words): fills the exception record at a0 - 0x50 (vtable 0x08B62450,
// the two words, the two globals 0x08CE0E08 / 0x08CE0E54 and the constant 0x432B2B00 / 0x474E5543), then calls
// 0x08B758A4(record + 0x30, a1, a2, record), 0x08B62198(record + 0x30) and 0x08B6459C(). The last two do not return.
void func_08B62498(Runtime &rt, AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 record = block - 0x50u;
    const u32 inner = block - 0x20u;
    ctx.gpr[16] = inner;
    put(rt, block - 0x50u, first);
    put(rt, record + 4u, second);
    put(rt, record + 8u, word(rt, 0x08CE0E08u));
    put(rt, record + 0xCu, word(rt, 0x08CE0E54u));
    put(rt, record + 0x30u, 0x432B2B00u);
    put(rt, record + 0x34u, 0x474E5543u);
    put(rt, record + 0x38u, 0x08B62450u);
    call_set(rt, ctx, 0x08B758A4u, {inner, first, second, record});
    call_set(rt, ctx, 0x08B62198u, {inner});
    call_keep(rt, ctx, 0x08B6459Cu, {});
}

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
void func_08B7B34C(Runtime &rt, AllegrexContext &ctx) {
    const u32 source = ctx.gpr[4];
    for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = word(rt, source + 4u * i);
    ctx.gpr[29] = word(rt, source + 0x20u);
    ctx.gpr[30] = word(rt, source + 0x24u);
    ctx.gpr[31] = word(rt, source + 0x28u);
    for (unsigned i = 0u; i < 12u; ++i) ctx.fpr[20u + i] = float_at(rt, source + 0x2Cu + 4u * i);
    ctx.gpr[2] = ctx.gpr[5] != 0u ? ctx.gpr[5] : 1u;
}

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
void func_08BE6D44(Runtime &rt, AllegrexContext &ctx) {
    const u32 count = ctx.gpr[5];
    if (count == 0u) {
        ctx.gpr[2] = 0x4D0000u;
        return;
    }
    u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[6];
    ctx.gpr[2] = 0x4D0000u;
    u32 index = 0u;
    do {
        if (destination != 0u) {
            const u32 second = word(rt, source + 4u);
            const u32 third = word(rt, source + 8u);
            const float f0 = float_at(rt, source + 0xCu);
            const float f1 = float_at(rt, source + 0x10u);
            const float f2 = float_at(rt, source + 0x14u);
            const float f3 = float_at(rt, source + 0x18u);
            const float f4 = float_at(rt, source + 0x1Cu);
            const float f5 = float_at(rt, source + 0x20u);
            const float f6 = float_at(rt, source + 0x24u);
            put(rt, destination + 4u, second);
            put(rt, destination + 8u, third);
            put_float(rt, destination + 0xCu, f0);
            put_float(rt, destination + 0x10u, f1);
            put_float(rt, destination + 0x14u, f2);
            put_float(rt, destination + 0x18u, f3);
            put_float(rt, destination + 0x1Cu, f4);
            put(rt, destination, 0x08CCE6E0u);
            put_float(rt, destination + 0x20u, f5);
            put_float(rt, destination + 0x24u, f6);
            ctx.gpr[2] = second;
        }
        ++index;
        destination += 0x28u;  // the loop branch's delay slot
    } while (count != index);
}

// 0x08C2C46C (a0 = first, a1 = end, a2 = output): copies 0x0C-byte records from a0 up to a1 into a2 (0x08A6972C
// prepares each, its word +4 is copied and 0x08A76ABC copies +8). The output pointer is returned; a destination of
// zero skips the copy but still advances, and a0 == a1 returns the output untouched.
void func_08C2C46C(Runtime &rt, AllegrexContext &ctx) {
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    u32 source = begin;
    u32 destination = out;
    if (begin != end) {
        do {
            ctx.gpr[20] = destination;
            ctx.gpr[17] = source;
            if (destination != 0u) {
                call_set(rt, ctx, 0x08A6972Cu, {destination, source});
                const u32 word4 = word(rt, source + 4u);
                put(rt, destination + 4u, word4);
                call_set(rt, ctx, 0x08A76ABCu, {destination + 8u, source + 8u});
            }
            source += 0xCu;       // the loop branch's delay slot
            destination += 0xCu;
        } while (end != source);
    }
    ctx.gpr[2] = destination;
}

// 0x08C48FC4 (a0 = the end of a halfword array, a1 = an index, a2 = two pointers): insertion of a1 into the sorted
// halfword run that ends at a0, using the floats at 0x98 of the records *(a2) and *(a2 + 4) index by record index.
// The run is walked backwards; the answer's slot is the first one whose float is not below the new one's.
void func_08C48FC4(Runtime &rt, AllegrexContext &ctx) {
    const u32 end = ctx.gpr[4];
    const u32 index = sext16(ctx.gpr[5]);
    const u32 records = word(rt, ctx.gpr[6]);
    const u32 words = word(rt, ctx.gpr[6] + 4u);
    const u32 new_offset = index * 0xF0u;  // (index << 8) - (index << 4)
    u32 write_at = end;
    u32 position = end - 2u;
    for (;;) {
        const u32 neighbour = sext16(word(rt, position));
        const u32 table = word(rt, words);  // reloaded every pass, as the original does
        const u32 neighbour_offset = neighbour * 0xF0u;
        const float theirs = float_at(rt, table + neighbour_offset + records * 4u + 0x98u);
        const float ours = float_at(rt, table + new_offset + records * 4u + 0x98u);
        rt.memory().store16(write_at, static_cast<std::uint16_t>(neighbour));  // the branch's delay slot
        if (!(ours < theirs)) break;
        write_at = position;
        position -= 2u;
    }
    rt.memory().store16(write_at, static_cast<std::uint16_t>(index));  // the jr's delay slot
}

// 0x08C7A730 (a0 = object): vtables 0x08CDC454 (at 0), 0x08CDC468 (at 8) and 0x08CDC47C (at 0xB0), 0x08C786B4(object + 0xC),
// 0x08C553B0(object, 0x08CDC424), vtable 0x08CDB088 at 0xB0, and 0x08B6543C(object + 0xB0) finishes.
void func_08C7A730(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CDC454u);
    put(rt, self + 8u, 0x08CDC468u);
    put(rt, self + 0xB0u, 0x08CDC47Cu);
    call_set(rt, ctx, 0x08C786B4u, {self + 0xCu});
    call_set(rt, ctx, 0x08C553B0u, {self, 0x08CDC424u});
    put(rt, self + 0xB0u, 0x08CDB088u);
    ctx.gpr[2] = 0x08CDB088u;
    tail_call(ctx, 0x08B6543Cu, {self + 0xB0u});
}

// 0x0885E940 (a0 = this): ten steps on the object's word + 0x5AC, each `setter` taking the previous `getter`'s
// answer, then 0x08866FA8(*this) finishes: 0x088D6E9C(word(*this + 0x5AC)), 0x088CBFD4, 0x088CBE6C(answer),
// 0x088CA548, 0x088C9780(answer), 0x08916474, 0x08916194(answer), 0x088C7F64, 0x088C78B4(answer), 0x08B1C47C.
void func_0885E940(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    u32 argument = word(rt, word(rt, self) + 0x5ACu);
    call_set(rt, ctx, 0x088D6E9Cu, {argument});
    call_set(rt, ctx, 0x088CBFD4u, {argument});
    argument = ctx.gpr[2];
    call_set(rt, ctx, 0x088CBE6Cu, {argument});
    call_set(rt, ctx, 0x088CA548u, {argument});
    argument = ctx.gpr[2];
    call_set(rt, ctx, 0x088C9780u, {argument});
    call_set(rt, ctx, 0x08916474u, {argument});
    argument = ctx.gpr[2];
    call_set(rt, ctx, 0x08916194u, {argument});
    call_set(rt, ctx, 0x088C7F64u, {argument});
    argument = ctx.gpr[2];
    call_set(rt, ctx, 0x088C78B4u, {argument});
    call_set(rt, ctx, 0x08B1C47Cu, {argument});
    tail_call(ctx, 0x08866FA8u, {word(rt, self)});
}

// 0x0886DF90 (a0 = this, a1 = a value): 0x0886A9A0(this, 6) gates a preparatory 0x0886AAA4(this, *(this + 0x70) + 0x28);
// then 0x08874BE0(*(this + 0x70) + 0x28, value) answers 1, or answers 0 after 0x0886AA0C(this, 6). v0 = that 1 or 0.
void func_0886DF90(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = value;
    ctx.gpr[16] = self;
    if (call_set(rt, ctx, 0x0886A9A0u, {self, 6u}) == 0u)
        call_set(rt, ctx, 0x0886AAA4u, {self, word(rt, self + 0x70u) + 0x28u});
    const u32 answer = call_set(rt, ctx, 0x08874BE0u, {word(rt, self + 0x70u) + 0x28u, value});
    ctx.gpr[3] = 1u;  // the branch's delay slot
    if (answer == 0u) {
        call_set(rt, ctx, 0x0886AA0Cu, {self, 6u});
        ctx.gpr[3] = 0u;
    }
    ctx.gpr[2] = ctx.gpr[3];
}

// 0x08882928 (a0 = object): stores vtable 0x08CC1460, 0x08882854(*(object + 0xC), *(object + 8)) prepares the member
// at +0xC, the helper at +0x10 is released through its first virtual (0x08A5BA08-like arguments: its table and
// *(object + 0xC)) and both words cleared, vtable 0x08CBF428 is stored and 0x08B64654(object) finishes.
void func_08882928(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 0xCu;
    put(rt, self, 0x08CC1460u);  // the jal's delay slot
    ctx.gpr[2] = 0x08CC1460u;
    call_set(rt, ctx, 0x08882854u, {word(rt, self + 0xCu), word(rt, self + 8u)});
    const u32 helper = word(rt, self + 0x10u);
    if (helper != 0u) {
        ctx.gpr[3] = word(rt, helper);
        call_keep(rt, ctx, word(rt, word(rt, helper)), {helper, word(rt, self + 0xCu)});
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0xCu, 0u);
    }
    put(rt, self, 0x08CBF428u);
    ctx.gpr[2] = 0x08CBF428u;
    tail_call(ctx, 0x08B64654u, {self});
}

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
void func_088CFA9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float scale = ctx.fpr[12];
    const float other = ctx.fpr[13];
    SavedRegisters saved(ctx);
    SavedFpr saved_fpr(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 block = word(rt, word(rt, self) + 0x61Cu);
    ctx.gpr[16] = block;
    ctx.fpr[20] = scale;
    ctx.fpr[13] = 0.0f;
    call_setf(rt, ctx, 0x088D390Cu, {block, frame.at(0u), frame.at(0xCu), 6u}, {other, 0.0f});
    ctx.fpr[12] = scale - float_at(rt, frame.at(0x10u));
    call_setf(rt, ctx, 0x088D2DE0u, {block}, {ctx.fpr[12]});
    float value = ctx.fpr[0];
    if (value < 0.0f) value = -value;
    ctx.fpr[0] = value;
}

// 0x088E798C (a0 = this, a1 = a value, f13 = a float): 0x088D390C(*(this + 0x61C), &first, &second, 6) primes a pair,
// then 0x088E7894(this, value, &first, &second) runs with f12 = the float argument.
void func_088E798C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const float argument = ctx.fpr[13];
    const float first_float = ctx.fpr[12];
    SavedRegisters saved(ctx);
    SavedFpr saved_fpr(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 block = word(rt, word(rt, self) + 0x61Cu);
    ctx.gpr[18] = self;
    ctx.gpr[17] = value;
    ctx.gpr[16] = frame.at(0xCu);
    ctx.fpr[20] = argument;
    ctx.fpr[13] = 0.0f;
    call_setf(rt, ctx, 0x088D390Cu, {block, frame.at(0u), frame.at(0xCu), 6u}, {first_float, 0.0f});
    call_setf(rt, ctx, 0x088E7894u, {self, value, frame.at(0u), frame.at(0xCu)}, {argument});
}

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
void func_089CFD28(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 other = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 local = frame.at(4u);
    ctx.gpr[18] = value;
    ctx.gpr[17] = self;
    ctx.gpr[16] = local;
    put(rt, frame.at(0u), other);
    call_set(rt, ctx, 0x08969188u, {local, frame.at(0u)});
    call_set(rt, ctx, 0x089CDDF8u, {self, value, local});
    call_set(rt, ctx, 0x08A5B9ECu, {local});
    const u32 state = call_set(rt, ctx, 0x089CFCCCu, {self, word(rt, self + 0x198u)});
    put(rt, self + 0x1C0u, 2u);
    put(rt, self + 0x1A0u, state);
}

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
void func_08A3DBDC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 0x14u;
    put(rt, self, 0x08CD13B0u);  // the jal's delay slot
    ctx.gpr[2] = 0x08CD13B0u;
    call_set(rt, ctx, 0x08A3D718u, {word(rt, self + 0x14u)});
    const u32 helper = word(rt, self + 0x18u);
    if (helper != 0u) {
        const u32 table = word(rt, helper);
        ctx.gpr[3] = table;
        ctx.gpr[2] = word(rt, table);
        call_keep(rt, ctx, word(rt, table), {helper, word(rt, self + 0x14u)});
        put(rt, self + 0x18u, 0u);
        put(rt, self + 0x14u, 0u);
    }
    call_set(rt, ctx, 0x08A780B0u, {self});
    tail_call(ctx, 0x08A5BC54u, {self, 0x1Cu, 4u});
}

// 0x08A41414 (a0 passed on, a1 / a3 = two values): 0x08A40E90(&local, a3) and 0x08A40CF8(&object, a1) prepare a pair,
// an empty object is reported (0x08A5BB7C(0x08CADBCC)), 0x08A413B0(object, &local) runs and both locals are released.
void func_08A41414(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 local = frame.at(4u);
    const u32 slot = frame.at(0u);
    ctx.gpr[17] = local;
    ctx.gpr[16] = first;
    call_set(rt, ctx, 0x08A40E90u, {local, second});
    call_set(rt, ctx, 0x08A40CF8u, {slot, first});
    const u32 object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADBCCu});
    call_set(rt, ctx, 0x08A413B0u, {object, local});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    call_set(rt, ctx, 0x08A5B9ECu, {local});
}

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
void func_08A56F08(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = value;
    ctx.gpr[17] = self;
    const u32 helper = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = helper;
    if (helper == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF778u});
    const u32 table = word(rt, helper);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x5Cu);
    call_keep(rt, ctx, word(rt, table + 0x5Cu), {frame.at(0u), helper});
    call_set(rt, ctx, 0x08A565C0u, {self, frame.at(0u), value});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
}

// 0x08A5DF58 (a0 = out, a2 = a holder): the helper at *holder (reported when null through 0x08A5BB7C(0x08CB0080))
// answers through slot 0x50, 0x08A5DEE8(&word, a1, answer, table) builds a stack word, 0x08A76ABC(out, &word) copies it
// and 0x08A5B9EC(&word) releases it. v0 = out.
void func_08A5DF58(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = out;
    const u32 helper = word(rt, ctx.gpr[6]);
    ctx.gpr[16] = helper;
    if (helper == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0080u});
    const u32 table = word(rt, helper);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x50u);
    const u32 answer = call_keep(rt, ctx, word(rt, table + 0x50u), {helper});
    call_set(rt, ctx, 0x08A5DEE8u, {frame.at(0u), other, answer, table});
    call_set(rt, ctx, 0x08A76ABCu, {out, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = out;
}

// 0x08A78F54 (a0 = this, a1 = a holder): the helper at *holder (reported when null through 0x08A5BB7C(0x08CB0E38))
// answers through slot 0x40, 0x08A83160(&word, a1, answer, table) builds a stack word, and unless this is the stack
// itself 0x08A5BAD8(this, word) copies it; 0x08A5B9EC(&word) releases it.
void func_08A78F54(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 helper = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = helper;
    if (helper == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
    const u32 table = word(rt, helper);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x40u);
    const u32 answer = call_keep(rt, ctx, word(rt, table + 0x40u), {helper});
    call_set(rt, ctx, 0x08A83160u, {frame.at(0u), other, answer, table});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08A8B108 (a0 = out, a2 = a holder): the helper at *holder (reported when null through 0x08A5BB7C(0x08CB1BE8))
// answers through slot 0x58, 0x08A8B098(&word, a1, answer, table) builds a stack word, 0x08A76ABC(out, &word) copies it
// and 0x08A5B9EC(&word) releases it. v0 = out.
void func_08A8B108(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = out;
    const u32 helper = word(rt, ctx.gpr[6]);
    ctx.gpr[16] = helper;
    if (helper == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1BE8u});
    const u32 table = word(rt, helper);
    const u32 function = word(rt, table + 0x58u);
    ctx.gpr[2] = table;
    ctx.gpr[5] = function;  // `jalr a1`
    const u32 answer = call_keep(rt, ctx, function, {helper});
    call_set(rt, ctx, 0x08A8B098u, {frame.at(0u), other, answer, table});
    call_set(rt, ctx, 0x08A76ABCu, {out, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = out;
}

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
void func_08A989B4(Runtime &rt, AllegrexContext &ctx) {
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

// 0x08A9BED4 (a0 = this, a1 / a2 = two values): under the lock at this + 0x4C, the object at *(this + 0x38)
// (reported when null through 0x08A5BB7C(0x08CB2570)) runs 0x08A7CBA8(object, a1, a2); v0 = the unlock's answer.
void func_08A9BED4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 lock = self + 0x4Cu;
    ctx.gpr[17] = lock;
    ctx.gpr[16] = self;
    ctx.gpr[19] = second;
    ctx.gpr[18] = first;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    const u32 object = word(rt, self + 0x38u);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
    call_set(rt, ctx, 0x08A7CBA8u, {object, first, second});
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// 0x08AC2BD8: the three registered static teardown slots (0x08CFFCA8, 0x08CFFCA4, 0x08CFFCA0). The first one's
// function pointer (with its word +4 in a0) runs; when the second is set 0x08AC2BAC(second) finishes; otherwise the
// third object's slot +0x58 finishes (with the object in a0).
void func_08AC2BD8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 first = word(rt, 0x08CFFCA8u);
    if (first != 0u) {
        const u32 function = word(rt, first);
        if (function != 0u) {
            ctx.gpr[2] = first;
            ctx.gpr[3] = function;
            call_keep(rt, ctx, function, {word(rt, first + 4u)});
        }
    }
    const u32 second = word(rt, 0x08CFFCA4u);
    if (second != 0u) {
        tail_call(ctx, 0x08AC2BACu, {second});
        return;
    }
    const u32 third = word(rt, 0x08CFFCA0u);
    if (third == 0u) return;
    const u32 table = word(rt, third);
    ctx.gpr[2] = table;
    ctx.gpr[3] = word(rt, table + 0x58u);
    tail_call(ctx, word(rt, table + 0x58u), {third});
}

// 0x08AC7F94 (a0 = this): under the lock at this + 8 (0x08B34888 / 0x08B34B24), the byte at +0x566 becomes 1 and a
// one-time record is built through 0x08B2E314(6, 1), 0x08B2E134(answer) and 0x08AC79B0(&block, answer) with the block
// {0x08AC8484, this}; the answer is kept at +0x56C.
void func_08AC7F94(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = self + 8u;
    call_set(rt, ctx, 0x08B34888u, {self + 8u});
    put8(rt, self + 0x566u, 1u);
    put(rt, frame.at(4u), self);
    put(rt, frame.at(0u), 0x08AC8484u);
    const u32 first = call_set(rt, ctx, 0x08B2E314u, {6u, 1u});
    const u32 second = call_set(rt, ctx, 0x08B2E134u, {first});
    const u32 result = call_set(rt, ctx, 0x08AC79B0u, {frame.at(0u), second});
    put(rt, self + 0x56Cu, result);
    call_set(rt, ctx, 0x08B34B24u, {self + 8u});
}

// 0x08AF2D20: rotates the three words of the record 0x08AF296C() answers through the three stack words at sp, then
// 0x08C3F8BC(first, second) runs and a nonzero first word is released (0x08B35254).
void func_08AF2D20(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call_keep(rt, ctx, 0x08AF296Cu, {self});
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0xCu), 0u);
    const u32 first = word(rt, object);
    put(rt, frame.at(4u), first);
    put(rt, object, 0u);
    const u32 second = word(rt, object + 4u);
    const u32 saved_second = word(rt, frame.at(8u));
    put(rt, frame.at(8u), second);
    put(rt, object + 4u, saved_second);
    const u32 third = word(rt, object + 8u);
    const u32 saved_third = word(rt, frame.at(0xCu));
    put(rt, frame.at(0xCu), third);
    put(rt, object + 8u, saved_third);
    call_set(rt, ctx, 0x08C3F8BCu, {first, second});
    const u32 pending = word(rt, frame.at(4u));
    if (pending != 0u) call_set(rt, ctx, 0x08B35254u, {pending});
}

// 0x08B1297C (a0 = this, a1 = a holder): the state record at *(holder + 0x10); a non-negative first word is released
// (0x08B33A24), a positive word +0x10 adds this[0x254] to word +4, and 0x08B11F8C(this, this + 0xAC,
// word + 4 + word + 8) answers: word +0x10 becomes -1 and word 0 the answer.
void func_08B1297C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    const u32 state = word(rt, ctx.gpr[5] + 0x10u);
    ctx.gpr[16] = state;
    const s32 handle = static_cast<s32>(word(rt, state));
    if (handle >= 0) call_set(rt, ctx, 0x08B33A24u, {static_cast<u32>(handle)});
    u32 size = word(rt, state + 4u);
    if (static_cast<s32>(word(rt, state + 0x10u)) > 0) {
        size += word(rt, self + 0x254u);
        put(rt, state + 4u, size);
    }
    const u32 answer = call_set(rt, ctx, 0x08B11F8Cu, {self, self + 0xACu, size + word(rt, state + 8u)});
    put(rt, state + 0x10u, 0xFFFFFFFFu);
    put(rt, state, answer);
}

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
void func_08B628BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = self;
    ctx.gpr[18] = third;
    ctx.gpr[17] = second;
    ctx.gpr[16] = first;
    ctx.gpr[3] = 1u;  // the branch's delay slot
    if (call_set(rt, ctx, 0x08B62884u, {self, first, second, third}) == 0u) {
        const u32 object = word(rt, self + 8u);
        const u32 table = word(rt, object);
        ctx.gpr[3] = table;
        ctx.gpr[2] = word(rt, table + 0x18u);
        ctx.gpr[3] = call_keep(rt, ctx, word(rt, table + 0x18u), {object});
    }
    ctx.gpr[2] = ctx.gpr[3];
}

// 0x08B75204 (a0 = table, a1 = this): 0x08B7450C(table) first; the index at this + 0x9BC below 0x133 selects a byte of
// 0x08D72BB4: kind 4 clears bit 0 of the word at table + index * 4 and stores it at table + 0x4D0 and back; every other
// index or kind goes on in 0x08B77B04 (which does not return).
void func_08B75204(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 state = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = state;
    ctx.gpr[17] = self;
    call_set(rt, ctx, 0x08B7450Cu, {self, state});
    const u32 index = word(rt, state + 0x9BCu);
    if (static_cast<s32>(index) >= 0x133) {
        call_keep(rt, ctx, 0x08B77B04u, {});
        return;
    }
    const u32 kind = byte_u(rt, 0x08D72BB4u + index);
    const u32 slot = self + (index << 2u);
    const u32 value = word(rt, slot);
    if (kind != 4u) {
        call_keep(rt, ctx, 0x08B77B04u, {});
        return;
    }
    const u32 flags = value & ~1u;
    put(rt, self + 0x4D0u, flags);
    put(rt, slot, flags);
}

// 0x08C201AC (a0 passed on, a1 = a value): 0x08A6158C(value) twice; the object at the second answer's word 0 (reported
// when null through 0x08A5BB7C(0x08CB0284)) runs slot 0xC4 (object, first answer), then 0x08A61584(value, first
// answer) finishes.
void func_08C201AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = value;
    call_set(rt, ctx, 0x08A6158Cu, {value});
    const u32 first = call_set(rt, ctx, 0x08A6158Cu, {value});
    ctx.gpr[18] = first;
    const u32 object = word(rt, first);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0xC4u);
    call_keep(rt, ctx, word(rt, table + 0xC4u), {object, first});
    tail_call(ctx, 0x08A61584u, {value, first});
}

// 0x08C79ED8 (a0 = object): vtables 0x08CDC374 (at 0) and 0x08CDC388 (at 0xA8), 0x08C786B4(object + 4), then the two
// words at 0x08CDC35C / 0x08CDC360 are used to fix the object's own table: word 0 = first, word at first - 0xC = second,
// vtable 0x08CDB088 at 0xA8 and 0x08B6543C(object + 0xA8) finishes.
void func_08C79ED8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    put(rt, self, 0x08CDC374u);
    put(rt, self + 0xA8u, 0x08CDC388u);
    call_set(rt, ctx, 0x08C786B4u, {self + 4u});
    const u32 first = word(rt, 0x08CDC35Cu);
    const u32 second = word(rt, 0x08CDC360u);
    const u32 offset = word(rt, first - 0xCu);
    put(rt, self, first);
    put(rt, self + offset, second);
    put(rt, self + 0xA8u, 0x08CDB088u);
    ctx.gpr[2] = 0x08CDB088u;
    tail_call(ctx, 0x08B6543Cu, {self + 0xA8u});
}

// 0x088056F8 (a0 / a1 passed on): the teardown chain 0x088057CC(a0), 0x08805B1C(a0, a1), 0x08805774, 0x08805B88,
// 0x08805B90, 0x08AC2BD8, 0x088057A8, 0x08805578, 0x088057F8, 0x08ABCE2C and 0x08AC4398 finishes.
void func_088056F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = value;
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x088057CCu, {self});
    call_set(rt, ctx, 0x08805B1Cu, {self, value});
    call_set(rt, ctx, 0x08805774u, {self, value});
    call_set(rt, ctx, 0x08805B88u, {self, value});
    call_set(rt, ctx, 0x08805B90u, {self, value});
    call_set(rt, ctx, 0x08AC2BD8u, {self, value});
    call_set(rt, ctx, 0x088057A8u, {self, value});
    call_set(rt, ctx, 0x08805578u, {self, value});
    call_set(rt, ctx, 0x088057F8u, {self, value});
    call_set(rt, ctx, 0x08ABCE2Cu, {self, value});
    tail_call(ctx, 0x08AC4398u, {self, value});
}

// 0x08805C70: the start-up sequence 0x08B2DBF0(0x14D), FPU control 31 cleared, then 0x08AD0974, 0x08AD0900,
// 0x08B33218, 0x08AD08A4, 0x08AD0820, 0x08B2DD2C, 0x08B33688, 0x08B34060, 0x08AD083C, 0x08B0EBF0, 0x08AB9B04 and
// 0x08B1F178 finishes.
void func_08805C70(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    call_set(rt, ctx, 0x08B2DBF0u, {0x14Du});
    ctx.fcr31 = 0u;  // ctc1 zero, $31
    call_set(rt, ctx, 0x08AD0974u, {0x14Du});
    call_set(rt, ctx, 0x08AD0900u, {0x14Du});
    call_set(rt, ctx, 0x08B33218u, {0x14Du});
    call_set(rt, ctx, 0x08AD08A4u, {0x14Du});
    call_set(rt, ctx, 0x08AD0820u, {0x14Du});
    call_set(rt, ctx, 0x08B2DD2Cu, {0x14Du});
    call_set(rt, ctx, 0x08B33688u, {0x14Du});
    call_set(rt, ctx, 0x08B34060u, {0x14Du});
    call_set(rt, ctx, 0x08AD083Cu, {0x14Du});
    call_set(rt, ctx, 0x08B0EBF0u, {0x14Du});
    call_set(rt, ctx, 0x08AB9B04u, {0x14Du});
    tail_call(ctx, 0x08B1F178u, {0x14Du});
}

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
void func_08860500(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6] & 0xFFu;
    const u32 third = ctx.gpr[7] & 0xFFu;  // `andi a1, a3, 0xFF` before the 0x08860080 call
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = first;
    ctx.gpr[18] = second;
    ctx.gpr[17] = self;
    const u32 block = word(rt, self);
    put8(rt, block + 0x3FDBu, 1u);
    const u32 inner = word(rt, block + 0x5ACu);
    ctx.gpr[16] = inner;
    call_set(rt, ctx, 0x08860080u, {block, third});
    call_set(rt, ctx, 0x088D7DFCu, {inner, first, second, word(rt, self) + 0x3E7Cu});
    put8(rt, word(rt, self) + 0x3FDBu, 0u);
}

// 0x0887B060 (a0 = object): 0x08B9AB4C(&pair) and 0x08B9A958(object + 0x10, &pair), 0x08B8ED98(&other), then
// 0x08B8FE44(pair[0], pair[1], sp) and a nonzero pair[0] is released (0x08B35254); 0x0896275C(object) runs and the
// words at +0x30 / +0x2C are cleared.
void func_0887B060(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 pair = frame.at(4u);
    ctx.gpr[16] = pair;
    ctx.gpr[17] = self;
    call_set(rt, ctx, 0x08B9AB4Cu, {pair});
    call_set(rt, ctx, 0x08B9A958u, {self + 0x10u, pair});
    call_set(rt, ctx, 0x08B8ED98u, {frame.at(0x10u)});
    call_set(rt, ctx, 0x08B8FE44u, {word(rt, frame.at(4u)), word(rt, frame.at(8u)), frame.at(0u)});
    const u32 pending = word(rt, frame.at(4u));
    if (pending != 0u) call_set(rt, ctx, 0x08B35254u, {pending});
    call_set(rt, ctx, 0x0896275Cu, {self});
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x2Cu, 0u);
}

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
void func_088E43D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = second;
    ctx.gpr[17] = first;
    ctx.gpr[16] = self;
    const u32 object = word(rt, self);
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x1Cu);
    call_keep(rt, ctx, word(rt, table + 0x1Cu), {object, first});
    const u32 helper = word(rt, self + 4u);
    if (helper == 0u) return;
    const u32 helper_table = word(rt, helper);
    ctx.gpr[2] = helper_table;
    ctx.gpr[3] = word(rt, helper_table + 0x1Cu);
    tail_call(ctx, word(rt, helper_table + 0x1Cu), {helper, first, second});
}

// 0x088FE99C (a0 = object): under the lock at +0x10 (0x08B344E0 / 0x08B3457C): the byte at +0xB is cleared,
// 0x088FE6BC(object) and 0x088FE308(object) run and 0x08B28DC4(object + 0x10) finishes; then the helper at +4 is
// released through its first virtual (with *object) and both words cleared.
void func_088FE99C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 lock = self + 0x10u;
    ctx.gpr[16] = lock;
    ctx.gpr[17] = self;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    put8(rt, self + 0xBu, 0u);  // the jal's delay slot
    call_set(rt, ctx, 0x088FE6BCu, {self});
    call_set(rt, ctx, 0x088FE308u, {self});
    call_set(rt, ctx, 0x08B3457Cu, {lock});
    call_set(rt, ctx, 0x08B28DC4u, {lock});
    const u32 helper = word(rt, self + 4u);
    if (helper != 0u) {
        const u32 table = word(rt, helper);
        ctx.gpr[3] = table;
        ctx.gpr[2] = word(rt, table);
        call_keep(rt, ctx, word(rt, table), {helper, word(rt, self)});
        put(rt, self, 0u);
        put(rt, self + 4u, 0u);
    }
}

// 0x08927A0C (a0 = this, a1 = a value): 0x08927120() answers an object whose slot 0x24 answers a number, which
// 0x08B4E734(value, number) receives; f12 becomes pi / 2 and, when the byte at this + 0x2107 is set, 0x08B4F47C(value)
// finishes.
void func_08927A0C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = value;
    const u32 object = call_keep(rt, ctx, 0x08927120u, {self});
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x24u);
    const u32 answer = call_keep(rt, ctx, word(rt, table + 0x24u), {object});
    call_set(rt, ctx, 0x08B4E734u, {value, answer});
    ctx.fpr[12] = std::bit_cast<float>(0x3FC90FDBu);
    if (byte_u(rt, self + 0x2107u) != 0u) tail_call(ctx, 0x08B4F47Cu, {value});
}

// 0x08986058 (a0 = object): stores vtable 0x08CCE1A8, releases the string at +0x30 unless it is the shared empty
// representation 0x08D74FD0 (0x08B60804(data - 4, -1); at zero or below also 0x08B8542C(data - 0xC, sp)), then
// 0x089ECC78(object) and 0x08B64654(object).
void func_08986058(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    const u32 data = word(rt, self + 0x30u);
    const u32 count = data - 0xCu;
    ctx.gpr[17] = count;
    put(rt, self, 0x08CCE1A8u);
    ctx.gpr[3] = 0x08CCE1A8u;  // the asm keeps the vtable in v1 (the store's source)
    if (count != 0x08D74FD0u) {
        const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
        if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {count, frame.at(0u)});
    }
    call_set(rt, ctx, 0x089ECC78u, {self});
    call_set(rt, ctx, 0x08B64654u, {self});
}

// 0x08A2E748 (a0 = object): stores vtable 0x08CBE590 and, when both the pointer at 0x08CF6648 and the byte at
// 0x08CF664C are set, 0x08B097AC(object) and 0x08B64654(object) run and both globals are cleared; then
// 0x08A780B0(object) finishes.
void func_08A2E748(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    put(rt, self, 0x08CBE590u);
    const u32 object = word(rt, 0x08CF6648u);
    ctx.gpr[16] = object;
    if (object != 0u && byte_u(rt, 0x08CF664Cu) != 0u) {
        call_set(rt, ctx, 0x08B097ACu, {object});
        call_set(rt, ctx, 0x08B64654u, {object});
        put(rt, 0x08CF6648u, 0u);
        put8(rt, 0x08CF664Cu, 0u);
    }
    tail_call(ctx, 0x08A780B0u, {self});
}

// 0x08A4D308 (a0 = object): the member at +0x98, when set, runs 0x08B61FC0; the member at +0x78 is released
// (0x08A5058C, and 0x08C1CC2C(member + 0x14, member[0x1C]) then 0x08B64654(member)), cleared, and 0x08B2EE2C(
// 0x08CF68F0) runs before 0x08A50AB0(object) finishes.
void func_08A4D308(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    const u32 first = word(rt, self + 0x98u);
    if (first != 0u) call_set(rt, ctx, 0x08B61FC0u, {first});
    call_set(rt, ctx, 0x08A5058Cu, {word(rt, self + 0x78u)});
    const u32 object = word(rt, self + 0x78u);
    ctx.gpr[16] = object;
    if (object != 0u) {
        call_set(rt, ctx, 0x08C1CC2Cu, {object + 0x14u, word(rt, object + 0x1Cu)});
        call_set(rt, ctx, 0x08B64654u, {object});
    }
    put(rt, self + 0x78u, 0u);
    call_set(rt, ctx, 0x08B2EE2Cu, {0x08CF68F0u});
    tail_call(ctx, 0x08A50AB0u, {self});
}

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
void func_08AA3568(Runtime &rt, AllegrexContext &ctx) {
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
    ctx.fpr[12] = ctx.fpr[0];  // the jal's delay slot
    call_setf(rt, ctx, 0x08B66524u, {}, {ctx.fpr[0]});
    ctx.fpr[12] = ctx.fpr[0];
    call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {ctx.fpr[12]});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08AA3758: the same as 0x08AA3568 with slot 0x54 and 0x08B66D54 in place of 0x08B66524.
void func_08AA3758(Runtime &rt, AllegrexContext &ctx) {
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
    call_setf(rt, ctx, 0x08B66D54u, {}, {ctx.fpr[0]});
    ctx.fpr[12] = ctx.fpr[0];
    call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {ctx.fpr[12]});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08AA37D4: the same as 0x08AA3568 with slot 0x54 and 0x08B65F0C in place of 0x08B66524.
void func_08AA37D4(Runtime &rt, AllegrexContext &ctx) {
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
    call_setf(rt, ctx, 0x08B65F0Cu, {}, {ctx.fpr[0]});
    ctx.fpr[12] = ctx.fpr[0];
    call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {ctx.fpr[12]});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x08AA3CD8: the same as 0x08AA3568 with slot 0x54 and 0x08B6759C.
void func_08AA3CD8(Runtime &rt, AllegrexContext &ctx) {
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
    call_setf(rt, ctx, 0x08B6759Cu, {}, {ctx.fpr[0]});
    ctx.fpr[12] = ctx.fpr[0];
    call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {ctx.fpr[12]});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

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
void func_08AA3E4C(Runtime &rt, AllegrexContext &ctx) {
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
    call_setf(rt, ctx, 0x08B664A0u, {}, {ctx.fpr[0]});
    ctx.fpr[12] = ctx.fpr[0];
    call_setf(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {ctx.fpr[12]});
    if (self != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

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
void func_08ACC070(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    ctx.gpr[17] = self;
    if (call_set(rt, ctx, 0x08B319A4u, {0x08D69170u}) == 0u) {
        if (call_set(rt, ctx, 0x08B327ACu, {0x08D69170u, 1u}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    if (call_set(rt, ctx, 0x08ACB910u, {self, frame.at(0u)}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, frame.at(0x30u), 0u);
    call_set(rt, ctx, 0x08ACBC78u, {self, frame.at(0u)});
}

// 0x08AF1124 (a0 = a begin / end pair): the element count ((end - begin) >> 3) is halved to one; twice the number of
// halvings goes to 0x08C3EEE0(begin, end, twice), which 0x08C3E9B0(begin, end, twice) finishes. An empty range
// returns at once.
void func_08AF1124(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 begin = word(rt, self);
    const u32 end = word(rt, self + 4u);
    ctx.gpr[16] = begin;
    ctx.gpr[17] = end;
    if (begin == end) {
        ctx.gpr[2] = 0u;  // the `subu v0, end, begin` in the beq's delay slot
        return;
    }
    s32 count = static_cast<s32>(end - begin) >> 3;
    u32 halvings = 0u;
    while (count != 1) {
        count >>= 1;
        ++halvings;
    }
    const u32 twice = halvings << 1u;
    call_set(rt, ctx, 0x08C3EEE0u, {begin, end, twice});
    tail_call(ctx, 0x08C3E9B0u, {begin, end, twice});
}

// 0x08B0A13C (a0 = object): under the lock at +0x48, packs the bytes of the 16-slot ring at object + 8 from the
// index at +4 up to the one at +0 into a word (byte n << 8 * n), unlocks and returns it.
void func_08B0A13C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 lock = self + 0x48u;
    ctx.gpr[18] = lock;
    ctx.gpr[16] = self;
    ctx.gpr[17] = 0u;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    u32 index = word(rt, self + 4u);
    const u32 end = word(rt, self);
    u32 result = 0u;
    if (index != end) {
        for (;;) {
            const u32 next = (index + 1u) & 0xFu;
            result = (result << 8u) | byte_u(rt, self + index * 4u + 8u);
            ctx.gpr[17] = result;
            if (next == end) break;
            index = next;
        }
    }
    call_keep(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[2] = result;
}

// 0x08B10B38 (a0 = this, a1 = other): locks this + 0x14 and other + 4 (0x08B34888), 0x08B1FCD4(this + 0x4C,
// other + 0x3C) moves the pair, other + 0xA4 is cleared, the second lock is released (0x08B34B24), 0x08B10A7C(this,
// other) runs and 0x08B34B24(this + 0x14) finishes.
void func_08B10B38(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 first_lock = self + 0x14u;
    const u32 second_lock = other + 4u;
    ctx.gpr[19] = first_lock;
    ctx.gpr[18] = second_lock;
    ctx.gpr[17] = self;
    ctx.gpr[16] = other;
    call_set(rt, ctx, 0x08B34888u, {first_lock});
    call_set(rt, ctx, 0x08B34888u, {second_lock});
    call_set(rt, ctx, 0x08B1FCD4u, {self + 0x4Cu, other + 0x3Cu});
    put(rt, other + 0xA4u, 0u);  // the jal's delay slot
    call_set(rt, ctx, 0x08B34B24u, {second_lock});
    call_set(rt, ctx, 0x08B10A7Cu, {self, other});
    tail_call(ctx, 0x08B34B24u, {first_lock});
}

// 0x08B1728C (a0 = object): 0x08B0CC04(sp) primes a 0x130-byte block, then the object's state words are set (-1 at
// +0x19F8, sp at +0x19E0, bytes +0x19DC / +0x19EC / +0x19F0 / +0x19FC zero) and the six members at +0x18D4 .. +0x19B0
// are primed (0x08B28C74); 0x08B0C808(sp) finishes.
void func_08B1728C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x130u);
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x08B0CC04u, {frame.at(0u)});
    put(rt, self + 0x19F8u, 0xFFFFFFFFu);
    put(rt, self + 0x19E0u, frame.at(0u));
    put8(rt, self + 0x19DCu, 0u);
    put(rt, self + 0x19FCu, 0u);
    put(rt, self + 0x19F0u, 0u);
    put(rt, self + 0x19ECu, 0u);  // the jal's delay slot
    call_set(rt, ctx, 0x08B28C74u, {self + 0x18D4u});
    call_set(rt, ctx, 0x08B28C74u, {self + 0x1900u});
    call_set(rt, ctx, 0x08B28C74u, {self + 0x192Cu});
    call_set(rt, ctx, 0x08B28C74u, {self + 0x1958u});
    call_set(rt, ctx, 0x08B28C74u, {self + 0x1984u});
    call_set(rt, ctx, 0x08B28C74u, {self + 0x19B0u});
    call_set(rt, ctx, 0x08B0C808u, {frame.at(0u)});
}

// 0x08B2DFB4: 0x08B2DEE4() answers a 64-bit value in v0 / v1 and 0x08B2DF58() another; the loop repeats until the
// 64-bit difference is the same twice in a row, then returns the low word in v0 and the high one in v1. The previous
// difference starts at -1 / -1.
void func_08B2DFB4(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    u32 low = 0xFFFFFFFFu;
    u32 high = 0xFFFFFFFFu;
    for (;;) {
        const GuestResult first = call_keep(rt, ctx, 0x08B2DEE4u, {}, {});
        ctx.gpr[16] = first.v0;
        ctx.gpr[17] = first.v1;
        const GuestResult second = call_keep(rt, ctx, 0x08B2DF58u, {}, {});
        const u32 delta_low = first.v0 - second.v0;
        const u32 borrow = first.v0 < delta_low ? 1u : 0u;
        const u32 delta_high = first.v1 - second.v1 - borrow;
        if (low == delta_low) {
            ctx.gpr[2] = low;  // the delay slot of the comparison
            if (high == delta_high) {
                ctx.gpr[3] = high;
                return;
            }
        }
        low = delta_low;
        high = delta_high;
    }
}

// 0x08B323E4 (a0 = out): 0x08B2E0B0(), the lock 0x08B35214(), under it 0x08B344E0(lock) and 0x08C7AF08(), then
// 0x08B318E4(0x2C00) answers a block kept at *out, the lock is released (0x08B3457C) and 0x08B2E0A4(lock) finishes.
// v0 = the 0x08C7AF08 answer.
void func_08B323E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = out;
    call_keep(rt, ctx, 0x08B2E0B0u, {});
    const u32 lock = call_keep(rt, ctx, 0x08B35214u, {});
    ctx.gpr[19] = lock;
    ctx.gpr[16] = lock;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    const u32 value = call_keep(rt, ctx, 0x08C7AF08u, {});
    ctx.gpr[17] = value;
    const u32 block = call_set(rt, ctx, 0x08B318E4u, {0x2C00u});
    put(rt, out, block);
    call_set(rt, ctx, 0x08B3457Cu, {lock});
    call_set(rt, ctx, 0x08B2E0A4u, {lock});
    ctx.gpr[2] = value;
}

// 0x08B33218: 0x08C7ABE8(1) and 0x08C7ABF8(0), then the one-time block at 0x08D691D8: when its word +8 is zero,
// 0x08B2E314(3, 0), 0x08B2E134(answer) and 0x08B1BA54(&block, answer) build it (the answer is kept at +8) and
// 0x08B2E214(answer) runs with the bytes at +0xC cleared.
void func_08B33218(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08C7ABE8u, {1u});
    call_set(rt, ctx, 0x08C7ABF8u, {0u});
    const u32 state = 0x08D691D8u;
    if (word(rt, state + 8u) != 0u) return;
    put(rt, frame.at(0u), 0x08B33534u);
    put8(rt, state + 0xCu, 0u);
    put(rt, frame.at(4u), state);
    const u32 first = call_set(rt, ctx, 0x08B2E314u, {3u, 0u});
    const u32 second = call_set(rt, ctx, 0x08B2E134u, {first});
    const u32 third = call_set(rt, ctx, 0x08B1BA54u, {frame.at(0u), second});
    put(rt, state + 8u, third);  // the jal's delay slot
    call_set(rt, ctx, 0x08B2E214u, {third});
}

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
void func_08B5D6B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 state = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = out;
    ctx.gpr[17] = value;
    const u32 index = call_set(rt, ctx, 0x08B5C2B0u, {value});
    ctx.gpr[16] = state;
    const u32 count = word(rt, state + 8u) - 1u;
    if (count < index) {
        call_keep(rt, ctx, 0x08B5AFDCu, {0x08CB57ACu});
        return;
    }
    const u32 entry = word(rt, word(rt, state + 4u) + (index << 2u));
    if (entry == 0u) {
        call_keep(rt, ctx, 0x08B5AFDCu, {0x08CB57ACu});
        return;
    }
    tail_call(ctx, 0x08B5D3DCu, {out, value, entry});
}

// 0x08B85948 (a0 = this, a1 = a holder, a2 = a value): the object at *holder (reported when null through
// 0x08A5BB7C(0x08C97BA4)) answers through slot 0x4C, and the function at this[0x88] finishes (this, answer, value).
void func_08B85948(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = value;
    ctx.gpr[17] = self;
    const u32 entry = word(rt, word(rt, self) + 0x88u);
    const u32 object = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = object;
    ctx.gpr[19] = entry;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97BA4u});
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x4Cu);
    const u32 answer = call_keep(rt, ctx, word(rt, table + 0x4Cu), {object});
    tail_call(ctx, entry, {self, answer, value});
}

// 0x08B89BB0: the same as 0x08B85948 with the report message 0x08C98D34.
void func_08B89BB0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = value;
    ctx.gpr[17] = self;
    const u32 entry = word(rt, word(rt, self) + 0x88u);
    const u32 object = word(rt, ctx.gpr[5]);
    ctx.gpr[16] = object;
    ctx.gpr[19] = entry;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    ctx.gpr[2] = word(rt, table + 0x4Cu);
    const u32 answer = call_keep(rt, ctx, word(rt, table + 0x4Cu), {object});
    tail_call(ctx, entry, {self, answer, value});
}

// ---- end of batch 16 bodies ----
} // namespace
} // namespace new_batch_16
} // namespace gt::game

#define NEW16(address, name, kind) GT_DECOMP(address, gt::game::new_batch_16::name, gt::ReturnKind::kind)
NEW16(0x08AD6854u, func_08AD6854, Word);
NEW16(0x08B0A9F4u, func_08B0A9F4, Word);
NEW16(0x08B2DEE4u, func_08B2DEE4, Word);
NEW16(0x08B2F060u, func_08B2F060, Word);
NEW16(0x08B56D4Cu, func_08B56D4C, Word);
NEW16(0x08B62498u, func_08B62498, Word);
NEW16(0x08B643A8u, func_08B643A8, Word);
NEW16(0x08B7B34Cu, func_08B7B34C, Word);
NEW16(0x08B862D8u, func_08B862D8, Word);
NEW16(0x08BD3008u, func_08BD3008, Word);
NEW16(0x08BD311Cu, func_08BD311C, Word);
NEW16(0x08BE6D44u, func_08BE6D44, Word);
NEW16(0x08C2C46Cu, func_08C2C46C, Word);
NEW16(0x08C48FC4u, func_08C48FC4, Word);
NEW16(0x08C7A730u, func_08C7A730, Word);
NEW16(0x0885E940u, func_0885E940, Word);
NEW16(0x0886DF90u, func_0886DF90, Word);
NEW16(0x08882928u, func_08882928, Word);
NEW16(0x0889844Cu, func_0889844C, Word);
NEW16(0x088CFA9Cu, func_088CFA9C, Float);
NEW16(0x088E798Cu, func_088E798C, Word);
NEW16(0x089B0044u, func_089B0044, Word);
NEW16(0x089CFD28u, func_089CFD28, Word);
NEW16(0x089DA4E0u, func_089DA4E0, Float);
NEW16(0x08A3DBDCu, func_08A3DBDC, Word);
NEW16(0x08A41414u, func_08A41414, Word);
NEW16(0x08A56630u, func_08A56630, Word);
NEW16(0x08A56F08u, func_08A56F08, Word);
NEW16(0x08A5DF58u, func_08A5DF58, Word);
NEW16(0x08A78F54u, func_08A78F54, Word);
NEW16(0x08A8B108u, func_08A8B108, Word);
NEW16(0x08A93500u, func_08A93500, Word);
NEW16(0x08A9893Cu, func_08A9893C, Word);
NEW16(0x08A989B4u, func_08A989B4, Word);
NEW16(0x08A9BED4u, func_08A9BED4, Word);
NEW16(0x08AC2BD8u, func_08AC2BD8, Word);
NEW16(0x08AC7F94u, func_08AC7F94, Word);
NEW16(0x08AF2D20u, func_08AF2D20, Word);
NEW16(0x08B1297Cu, func_08B1297C, Word);
NEW16(0x08B32228u, func_08B32228, Word);
NEW16(0x08B38C78u, func_08B38C78, Word);
NEW16(0x08B628BCu, func_08B628BC, Word);
NEW16(0x08B75204u, func_08B75204, Word);
NEW16(0x08C201ACu, func_08C201AC, Word);
NEW16(0x08C79ED8u, func_08C79ED8, Word);
NEW16(0x088056F8u, func_088056F8, Word);
NEW16(0x08805C70u, func_08805C70, Word);
NEW16(0x0880AF30u, func_0880AF30, Word);
NEW16(0x08860500u, func_08860500, Word);
NEW16(0x0887B060u, func_0887B060, Word);
NEW16(0x088A117Cu, func_088A117C, Word);
NEW16(0x088D2C8Cu, func_088D2C8C, Float);
NEW16(0x088E43D0u, func_088E43D0, Word);
NEW16(0x088FE99Cu, func_088FE99C, Word);
NEW16(0x08927A0Cu, func_08927A0C, Word);
NEW16(0x08B10B38u, func_08B10B38, Word);
NEW16(0x08B1728Cu, func_08B1728C, Word);
NEW16(0x08B2DFB4u, func_08B2DFB4, Word);
NEW16(0x08B323E4u, func_08B323E4, Word);
NEW16(0x08B33218u, func_08B33218, Word);
NEW16(0x08B35E3Cu, func_08B35E3C, Word);
NEW16(0x08B5D6B0u, func_08B5D6B0, Word);
NEW16(0x08B85948u, func_08B85948, Word);
NEW16(0x08B89BB0u, func_08B89BB0, Word);
NEW16(0x08986058u, func_08986058, Word);
NEW16(0x08A2E748u, func_08A2E748, Word);
NEW16(0x08A4D308u, func_08A4D308, Word);
NEW16(0x08A70A48u, func_08A70A48, Word);
NEW16(0x08AA3568u, func_08AA3568, Word);
NEW16(0x08AA3758u, func_08AA3758, Word);
NEW16(0x08AA37D4u, func_08AA37D4, Word);
NEW16(0x08AA3CD8u, func_08AA3CD8, Word);
NEW16(0x08AA3DD0u, func_08AA3DD0, Word);
NEW16(0x08AA3E4Cu, func_08AA3E4C, Word);
NEW16(0x08AB4724u, func_08AB4724, Word);
NEW16(0x08ACC070u, func_08ACC070, Word);
NEW16(0x08AF1124u, func_08AF1124, Word);
NEW16(0x08B0A13Cu, func_08B0A13C, Word);
// func_08B54104 skipped: a VFPU ray/segment computation (lwl/lwr scalars into S010-S012, vhtfm4.q / vtfm4.q /
// vadd.q / vrcp.s / vscl.t / vsub.t / vdot.t / vsqrt.s) whose intermediate matrix layout the vfpu helpers cannot
// express without inventing it; not readable structured C.
// func_08B17104 skipped: its first five instructions hand this + 0x2C to 0x08B7B34C, which reloads sp/ra from that
// block and returns through the restored ra (never to the next instruction); the remaining 24 instructions are a
// separate constructor body (no functions.tsv entry at 0x08B17118) that the recompiler's boundary swept into this unit.
