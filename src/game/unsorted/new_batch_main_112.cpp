// New batch main-session 112: rewrites of functions whose earlier drafts mismatched the original in the synthetic gate (or
// faulted). Structured C from `tools.disasm`; a `jal` / `jalr` delay slot runs BEFORE the call; callee-saved registers are set
// where the asm sets them (a callee may read them) and put back at the end; v0 (and v1 / f0 where the asm leaves them) end as
// the asm leaves them on every path. Draft, unverified: the writer does not run the gate. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_112 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using GuestResult = gt::GuestResult;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u64 = std::uint64_t;

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
// `jalr` through the object's vtable: v1 = the table, v0 = the slot, then the call.
inline u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_set(rt, ctx, function, arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
inline u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
inline u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

// s0-s7, fp and ra are callee-saved: the body sets them where the asm sets them and the caller's values come back.
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

// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
inline s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// The ll/sc decrement of a shared holder's use count: stores old - 1, leaves v1 = the address, a0 = the old value and v0 = 1 (the
// successful sc), and answers whether the count reached zero.
inline bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old == 1u;
}
// `lw v1, 0(holder); lw v0, offset(v1); jalr v0` with a0 = the holder; v0 / v1 / f0 afterwards are the callee's.
inline u32 call_vtable(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 offset) {
    const u32 table = word(rt, holder);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    ctx.gpr[4] = holder;
    const GuestResult result = call_guest(rt, ctx, function, {holder}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// One member of a chain of shared holders (s0 = the holder): release the count at +4 and, when that was the last reference, run the
// object's vtable +8 virtual and then the second count at +8 and the +0xC virtual.
inline void release_holder(Runtime &rt, AllegrexContext &ctx, u32 holder) {
    ctx.gpr[16] = holder;
    if (holder == 0u) return;
    if (!count_down(rt, ctx, holder + 4u)) return;
    call_vtable(rt, ctx, holder, 8u);
    if (!count_down(rt, ctx, holder + 8u)) return;
    call_vtable(rt, ctx, holder, 0xCu);
}
// A string-ish block: its header sits 0xC bytes before the data (the shared empty header 0x08D75298 is never freed), and the
// reference count 4 bytes before the data decides whether 0x08B93554 frees it.
inline void release_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 scratch) {
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    if (header == 0x08D75298u) return;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    ctx.gpr[4] = header;
    ctx.gpr[5] = scratch;
    call_set(rt, ctx, 0x08B93554u, {header, scratch});
}

// 0x08BE834C (a0 = destination of `a1` 0x40-byte records, a2 = source): a count of zero returns at once (v0 untouched); else each
// record: unless the destination is null, words +4 .. +0x1C copied, vtable 0x08CCFC90 at +0, then 0x089FA280 copies +0x20 and +0x30.
// The source stays the same record. s0 = destination, s1 = source, s2 = index, s3 = count, s4 = the vtable.
void func_08BE834C(Runtime &rt, AllegrexContext &ctx) {
    const u32 count = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = count;
    ctx.gpr[17] = ctx.gpr[6];
    if (count == 0u) return;
    ctx.gpr[2] = 0x08CD0000u;
    ctx.gpr[16] = ctx.gpr[4];
    ctx.gpr[20] = 0x08CCFC90u;
    ctx.gpr[18] = 0u;
    do {
        ctx.gpr[18] += 1u;
        ctx.gpr[4] = ctx.gpr[16] + 0x20u;
        ctx.gpr[5] = ctx.gpr[17] + 0x20u;
        if (ctx.gpr[16] != 0u) {
            const u32 destination = ctx.gpr[16];
            const u32 source = ctx.gpr[17];
            ctx.gpr[2] = word(rt, source + 4u);
            ctx.gpr[3] = word(rt, source + 8u);
            for (unsigned i = 0u; i < 5u; ++i) ctx.fpr[i] = float_at(rt, source + 0xCu + 4u * i);
            put(rt, destination + 4u, ctx.gpr[2]);
            put(rt, destination + 8u, ctx.gpr[3]);
            for (u32 offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
            put(rt, destination, ctx.gpr[20]);
            call_keep(rt, ctx, 0x089FA280u, {ctx.gpr[4], ctx.gpr[5]});
            ctx.gpr[4] = destination + 0x30u;
            ctx.gpr[5] = source + 0x30u;
            call_keep(rt, ctx, 0x089FA280u, {ctx.gpr[4], ctx.gpr[5]});
        }
        ctx.gpr[16] += 0x40u;
    } while (ctx.gpr[19] != ctx.gpr[18]);
}

// 0x08BF6664 (a0 = invoker, a1 = pointer to a word, a2 = value): two delegate steps, the second one a tail call.
// Step 1: tag = word(+0x18), base = word(+0x14), this = *a1 + (tag >> 1); bit 0 of the tag set: the function is the word at
// base + *this, else it is `base` itself. It gets (this, *a1, base, tag). Step 2: tag = word(+0x28); s0 = result + (tag >> 1);
// bit 0 set: the function is the word at word(+0x24) + *s0, else it is the word at +0x24. 0x08B8C590(value) gives a1; the tail
// call is function(s0, answer).
void func_08BF6664(Runtime &rt, AllegrexContext &ctx) {
    const u32 invoker = ctx.gpr[4];
    const u32 value = ctx.gpr[6];
    u32 function;
    u32 pointer;
    u32 answer;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[18] = value;
        ctx.gpr[17] = invoker;
        const u32 tag = word(rt, invoker + 0x18u);
        const u32 target = word(rt, ctx.gpr[5]);
        const u32 base = word(rt, invoker + 0x14u);
        const u32 adjusted = target + (static_cast<u32>(static_cast<s32>(tag) >> 1));
        ctx.gpr[3] = tag & 1u;
        ctx.gpr[2] = base;
        if ((tag & 1u) != 0u) {
            ctx.gpr[2] = word(rt, adjusted);
            ctx.gpr[2] = word(rt, ctx.gpr[2] + base);
        }
        const u32 first = call_set(rt, ctx, ctx.gpr[2], {adjusted, target, base, tag});
        ctx.gpr[4] = first;
        const u32 tag2 = word(rt, invoker + 0x28u);
        const u32 shifted = static_cast<u32>(static_cast<s32>(tag2) >> 1);
        ctx.gpr[3] = shifted;
        ctx.gpr[16] = first + shifted;
        if ((tag2 & 1u) != 0u) {
            ctx.gpr[3] = word(rt, invoker + 0x24u);
            ctx.gpr[2] = word(rt, ctx.gpr[16]) + ctx.gpr[3];
            ctx.gpr[17] = word(rt, ctx.gpr[2]);
        } else {
            ctx.gpr[2] = tag2 & 1u;
            ctx.gpr[17] = word(rt, invoker + 0x24u);
        }
        answer = call_set(rt, ctx, 0x08B8C590u, {value});
        ctx.gpr[5] = answer;
        ctx.gpr[4] = ctx.gpr[16];
        ctx.gpr[25] = ctx.gpr[17];
        function = ctx.gpr[17];
        pointer = ctx.gpr[16];
    }
    ctx.gpr[2] = answer;
    tail_call(ctx, function, {pointer, answer});
}

// 0x08B34B2C (a0 = object): s1 = the answer of 0x08B2E6DC(object) (the `jal 0x08C7A888` delay slot copies v0 BEFORE that call);
// current = 0x08C7A888(). When the word at +0x20 is non-zero and s1 equals the word at +0x18, the record (s1, +0x1C, +0x20, +0x24)
// is saved on the stack, linked into the list at +8 / +0xC and the fields reset (+0x20 = 1, +0x24 = +0x1C = -1), s1 = 0; else s1 = 1.
// After 0x08C7A8A8(), a saved record is restored through 0x08B349C4 / 0x08C7AA98 / 0x08B34748; v0 ends as the original leaves it.
void func_08B34B2C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = call_set(rt, ctx, 0x08B2E6DCu, {self});
    const u32 current = call_keep(rt, ctx, 0x08C7A888u, {});
    ctx.gpr[5] = word(rt, self + 0x20u);
    ctx.gpr[4] = current;
    if (ctx.gpr[5] == 0u) {
        ctx.gpr[17] = 1u;
    } else {
        ctx.gpr[2] = word(rt, self + 0x18u);
        if (ctx.gpr[17] != ctx.gpr[2]) {
            ctx.gpr[17] = 1u;
        } else {
            const u32 second = word(rt, self + 0x1Cu);
            const u32 fourth = word(rt, self + 0x24u);
            const u32 link = word(rt, self + 0xCu);
            put(rt, frame.at(4u), ctx.gpr[17]);
            put(rt, frame.at(8u), second);
            put(rt, frame.at(0xCu), ctx.gpr[5]);
            put(rt, frame.at(0x10u), fourth);
            put(rt, frame.at(0u), 0u);
            if (link != 0u) put(rt, link, frame.at(0u));
            else put(rt, self + 8u, frame.at(0u));
            ctx.gpr[3] = 0xFFFFFFFFu;
            ctx.gpr[2] = 1u;
            ctx.gpr[17] = 0u;
            put(rt, self + 0x20u, 1u);
            put(rt, self + 0xCu, frame.at(0u));
            put(rt, self + 0x24u, 0xFFFFFFFFu);
            put(rt, self + 0x1Cu, 0xFFFFFFFFu);
        }
    }
    call_keep(rt, ctx, 0x08C7A8A8u, {});
    if (ctx.gpr[17] != 0u) return;
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0u;
    if (call_keep(rt, ctx, 0x08B349C4u, {self, 0u}) != 0u) {
        ctx.gpr[4] = self;
        call_keep(rt, ctx, 0x08C7AA98u, {});
    }
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0u;
    call_keep(rt, ctx, 0x08B34748u, {self, 0u});
    ctx.gpr[4] = word(rt, frame.at(0xCu));
    ctx.gpr[3] = word(rt, frame.at(0x10u));
    ctx.gpr[2] = word(rt, frame.at(8u));
    put(rt, self + 0x20u, ctx.gpr[4]);
    put(rt, self + 0x1Cu, ctx.gpr[2]);
    put(rt, self + 0x24u, ctx.gpr[3]);
}

// 0x08B1F2B8 (a0 = object): loops: current = 0x08B1F67C(object + 0x10, &out); length = 0x08B1F4DC(current); a failed
// 0x08B1F4F4(object + 0x10, current, out) retries; a zero length answers 0; a failed 0x08B1F5DC(object + 0x10, current, length, out, out + 1)
// retries; next = 0x08B1F67C(object, &out) and, when it is the same entry as current, 0x08B1F5DC(object, next, length, out, out + 1);
// a current that is not the end marker (object + 0x20) answers current, else 0x08B1F1C4(object) and again.
void func_08B1F2B8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = self;
    ctx.gpr[18] = self + 0x10u;
    ctx.gpr[20] = self + 0x20u;
    for (;;) {
        ctx.gpr[5] = frame.at(0u);
        ctx.gpr[16] = call_set(rt, ctx, 0x08B1F67Cu, {ctx.gpr[18], frame.at(0u)});
        ctx.gpr[4] = ctx.gpr[16];
        ctx.gpr[17] = call_set(rt, ctx, 0x08B1F4DCu, {ctx.gpr[16]});
        ctx.gpr[6] = word(rt, frame.at(0u));
        const u32 flag = call_set(rt, ctx, 0x08B1F4F4u, {ctx.gpr[18], ctx.gpr[16], ctx.gpr[6]});
        ctx.gpr[2] = flag & 0xFFu;
        if (ctx.gpr[2] == 0u) continue;
        if (ctx.gpr[17] == 0u) {
            ctx.gpr[16] = 0u;
            break;
        }
        ctx.gpr[7] = word(rt, frame.at(0u));
        const u32 more = call_set(rt, ctx, 0x08B1F5DCu, {ctx.gpr[18], ctx.gpr[16], ctx.gpr[17], ctx.gpr[7], ctx.gpr[7] + 1u});
        ctx.gpr[2] = more & 0xFFu;
        if (ctx.gpr[2] == 0u) continue;
        const u32 next = call_set(rt, ctx, 0x08B1F67Cu, {ctx.gpr[19], frame.at(0u)});
        if (ctx.gpr[16] == next) {
            ctx.gpr[7] = word(rt, frame.at(0u));
            call_set(rt, ctx, 0x08B1F5DCu, {ctx.gpr[19], next, ctx.gpr[17], ctx.gpr[7], ctx.gpr[7] + 1u});
        }
        ctx.gpr[5] = ctx.gpr[16];
        ctx.gpr[4] = ctx.gpr[19];
        if (ctx.gpr[16] != ctx.gpr[20]) break;
        call_set(rt, ctx, 0x08B1F1C4u, {ctx.gpr[19]});
    }
    ctx.gpr[2] = ctx.gpr[16];
}

// Copies the 0x2C-byte record at `from` to `to` (the original moves it as words / floats; the bits are the same).
void copy_record_2C(Runtime &rt, u32 to, u32 from) {
    for (u32 offset = 0u; offset < 0x2Cu; offset += 4u) put(rt, to + offset, word(rt, from + offset));
}

// 0x08C1667C (a0 = first record, a1 = end, a2 = reference, a3 = key): a partition of 0x2C-byte records. left (s1) advances while
// 0x08A15D54(key, left, reference) is non-zero; right (s0) steps back once and then while 0x08A15D54(key, reference, right) is
// non-zero; while left < right the two records swap (through the stack record at sp) and left moves on. v0 = left.
void func_08C1667C(Runtime &rt, AllegrexContext &ctx) {
    const u32 reference_in = ctx.gpr[6];
    const u32 key_in = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[19] = reference_in;
    ctx.gpr[18] = key_in;
    ctx.gpr[17] = ctx.gpr[4];
    ctx.gpr[16] = ctx.gpr[5];
    for (;;) {
        for (;;) {
            if (call_set(rt, ctx, 0x08A15D54u, {ctx.gpr[18], ctx.gpr[17], ctx.gpr[19]}) == 0u) break;
            ctx.gpr[17] += 0x2Cu;
        }
        for (;;) {
            ctx.gpr[16] -= 0x2Cu;
            if (call_set(rt, ctx, 0x08A15D54u, {ctx.gpr[18], ctx.gpr[19], ctx.gpr[16]}) == 0u) break;
        }
        if (!(ctx.gpr[17] < ctx.gpr[16])) break;
        copy_record_2C(rt, frame.at(0u), ctx.gpr[17]);
        copy_record_2C(rt, ctx.gpr[17], ctx.gpr[16]);
        copy_record_2C(rt, ctx.gpr[16], frame.at(0u));
        ctx.gpr[17] += 0x2Cu;
    }
    ctx.gpr[2] = ctx.gpr[17];
}

// 0x08C3D12C (a0 = vector object {+0 begin, +4 end, +8 capacity end}, a1 = position, a2 = pointer to the value): an insert of one
// word. With room left: the last element moves up (when end is non-null), end += 4, the tail from `position` is moved up by
// 0x08B79BAC(end - bytes, position, bytes) and the value is stored at position. Without room: length_error 0x08B5ADDC(0x08CB4918)
// when the size is 0x3FFFFFFF elements, a new block of twice the elements (1 for an empty vector) from 0x08B62344, the head and
// tail copied with 0x08B79BAC around the new value, the old block released with 0x08B64654 and the three words updated.
void func_08C3D12C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 value_pointer = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[21] = value_pointer;
    ctx.gpr[19] = position;
    ctx.gpr[18] = self;
    ctx.gpr[7] = word(rt, self + 4u);
    ctx.gpr[2] = word(rt, self + 8u);
    if (ctx.gpr[7] != ctx.gpr[2]) {
        ctx.gpr[4] = ctx.gpr[7];
        if (ctx.gpr[7] != 0u) {
            ctx.gpr[2] = word(rt, ctx.gpr[7] - 4u);
            put(rt, ctx.gpr[7], ctx.gpr[2]);
            ctx.gpr[7] = word(rt, self + 4u);
            ctx.gpr[4] = ctx.gpr[7];
        }
        ctx.gpr[6] = ctx.gpr[4] - 4u;
        ctx.gpr[7] += 4u;
        put(rt, self + 4u, ctx.gpr[7]);
        ctx.gpr[6] = (ctx.gpr[6] - position) & ~3u;
        ctx.gpr[16] = word(rt, value_pointer);
        call_set(rt, ctx, 0x08B79BACu, {ctx.gpr[4] - ctx.gpr[6], ctx.gpr[5], ctx.gpr[6]});
        put(rt, position, ctx.gpr[16]);
        return;
    }
    const u32 begin = word(rt, self);
    const u32 elements = static_cast<u32>(static_cast<s32>(ctx.gpr[7] - begin) >> 2);
    ctx.gpr[2] = elements;
    ctx.gpr[3] = 0x3FFFFFFFu;
    if (elements == 0x3FFFFFFFu) {
        ctx.gpr[4] = 0x08CB4918u;
        call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4918u});
        ctx.gpr[3] = 0x3FFFFFFFu;
    }
    u32 capacity = 1u;
    if (elements != 0u) {
        capacity = elements << 1;
        if (capacity < elements) capacity = 0x3FFFFFFFu;
    }
    ctx.gpr[4] = capacity;
    ctx.gpr[22] = capacity << 2;
    if (0x3FFFFFFFu < capacity) call_set(rt, ctx, 0x08B5A9C8u, {capacity});
    const u32 block = call_set(rt, ctx, 0x08B62344u, {ctx.gpr[22]});
    ctx.gpr[5] = word(rt, self);
    ctx.gpr[20] = block;
    ctx.gpr[16] = position - ctx.gpr[5];
    ctx.gpr[17] = block + ctx.gpr[16];
    call_set(rt, ctx, 0x08B79BACu, {block, ctx.gpr[5], ctx.gpr[16]});
    if (ctx.gpr[17] != 0u) put(rt, ctx.gpr[17], word(rt, value_pointer));
    ctx.gpr[16] = word(rt, self + 4u);
    ctx.gpr[17] += 4u;
    ctx.gpr[16] -= position;
    call_set(rt, ctx, 0x08B79BACu, {ctx.gpr[17], position, ctx.gpr[16]});
    ctx.gpr[4] = word(rt, self);
    ctx.gpr[16] = ctx.gpr[17] + ctx.gpr[16];
    if (ctx.gpr[4] != 0u) call_set(rt, ctx, 0x08B64654u, {ctx.gpr[4]});
    ctx.gpr[2] = ctx.gpr[20] + ctx.gpr[22];
    put(rt, self + 8u, ctx.gpr[2]);
    put(rt, self + 4u, ctx.gpr[16]);
    put(rt, self, ctx.gpr[20]);
}

// 0x08B1B420 (a0 = a day count): a calendar split. day = a0 + 0xFFE5BBAE; cycle (s2) = day / 146097; the rest / 36524 is the
// century index (s1; 4 becomes 3 with the day-in-century 0x5B4 and the year-in-century 0x18), else s3 = rest' / 1461 and s4 =
// rest' % 1461 (rest' = the rest % 36524). 0x08B1B268(s4, table 0x08C932A0, 5) finds the leap-cycle slot; a second search over the
// month table at 0x08C9326C (13 halves per row, row from the choice bit) gives the month. Packs two halves: v0 = low | high << 16,
// low = ((cycle * 4 + s1) * 25 + s3) * 4 + slot + 1, high = the bytes (month, day) stored at sp + 2 / sp + 3.
void func_08B1B420(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const s32 day = static_cast<s32>(ctx.gpr[4] + 0xFFE5BBAEu);
    const s32 cycle = day / 146097;
    const s32 rest = day % 146097;
    ctx.gpr[18] = static_cast<u32>(cycle);
    ctx.gpr[20] = 0x5B4u;
    ctx.gpr[19] = 0x18u;
    ctx.gpr[17] = static_cast<u32>(rest / 36524);
    const s32 rest_in_century = rest % 36524;
    if (ctx.gpr[17] == 4u) {
        ctx.gpr[17] = 3u;
    } else {
        ctx.gpr[19] = static_cast<u32>(rest_in_century / 1461);
        ctx.gpr[20] = static_cast<u32>(rest_in_century % 1461);
    }
    const u32 century = ctx.gpr[17];
    const u32 extra = ctx.gpr[19];
    const u32 cycle_day = ctx.gpr[20];
    ctx.gpr[16] = 0x08C932A0u;
    const u32 first = call_set(rt, ctx, 0x08B1B268u, {cycle_day, 0x08C932A0u, 5u});
    u32 choice = ((first ^ 3u) < 1u) ? 1u : 0u;
    if ((extra ^ 0x18u) == 0u) choice = (choice != 0u && (century ^ 3u) < 1u) ? 1u : 0u;
    const u32 index = century + (static_cast<u32>(cycle) << 2);
    const u32 day_of_year = cycle_day - half_u(rt, 0x08C932A0u + first * 2u);
    const u32 row = choice * 13u;
    const u32 packed = ((index * 25u + extra) << 2) + first;
    ctx.gpr[19] = 0x08C9326Cu;
    ctx.gpr[18] = day_of_year;
    ctx.gpr[17] = packed;
    ctx.gpr[16] = row;
    const u32 month = call_set(rt, ctx, 0x08B1B268u, {day_of_year, 0x08C9326Cu + row * 2u, 0xDu});
    const u32 day_in_month = day_of_year - half_u(rt, 0x08C9326Cu + (row + month) * 2u) + 1u;
    put8(rt, frame.at(2u), month + 1u);
    put8(rt, frame.at(3u), day_in_month);
    const u32 low = sext16(packed + 1u) & 0xFFFFu;
    ctx.gpr[3] = half_u(rt, frame.at(2u));
    ctx.gpr[2] = low | (ctx.gpr[3] << 16);
}

// 0x08C35F34 (a0 = base of 12-byte records {+0 word, +4 holder pointer, +8 byte}, a1 = hole index, a2 = top index, a3 = pointer to
// the value record): a sift-up. While the hole is above the top, the parent (index - 1) / 2 is compared with the value by the
// holder's word at +0x14 (signed `slt`; equal keys ask 0x08B35FD8(parent word, value word), negative = the parent is smaller);
// a smaller parent moves down into the hole. Then the value record is stored at the hole. A null holder calls 0x08A5BB7C(0x08CB39CC).
// v0 = the value's byte at +8.
void func_08C35F34(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    u32 &s0 = ctx.gpr[16], &s1 = ctx.gpr[17], &s2 = ctx.gpr[18], &s3 = ctx.gpr[19], &s4 = ctx.gpr[20], &s5 = ctx.gpr[21],
        &s6 = ctx.gpr[22], &s7 = ctx.gpr[23], &fp = ctx.gpr[30];
    const u32 index = ctx.gpr[5];
    ctx.gpr[2] = index - 1u;
    ctx.gpr[3] = (ctx.gpr[2] >> 31) + ctx.gpr[2];
    s3 = index;
    ctx.gpr[5] = signed_of(ctx.gpr[6]) < signed_of(index) ? 1u : 0u;
    fp = ctx.gpr[6];
    s7 = ctx.gpr[4];
    s6 = static_cast<u32>(signed_of(ctx.gpr[3]) >> 1);
    s4 = ctx.gpr[7];
    const auto holder_key = [&]() {  // s1 = the key of the holder at +4 of the parent record, s0 = the value's holder
        s1 = word(rt, s2 + 4u);
        if (s1 == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
        s0 = word(rt, s4 + 4u);
        s1 = word(rt, s1 + 0x14u);
        if (s0 == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
        ctx.gpr[2] = word(rt, s0 + 0x14u);
    };
    if (ctx.gpr[5] == 0u) {
        s2 = s3 * 12u + s7;
        s1 = word(rt, s4);
        s0 = s2 + 4u;
    } else {
        for (;;) {
            s2 = s6 * 12u + s7;
            holder_key();
            bool move_parent = signed_of(s1) < signed_of(ctx.gpr[2]);
            if (move_parent) {
                s5 = word(rt, s2);
            } else {
                holder_key();
                if (signed_of(ctx.gpr[2]) < signed_of(s1)) {
                    s1 = word(rt, s4);
                    s2 = s3 * 12u + s7;
                    s0 = s2 + 4u;
                    break;
                }
                s5 = word(rt, s2);
                s1 = word(rt, s4);
                ctx.gpr[4] = s5;
                ctx.gpr[5] = s1;
                ctx.gpr[2] = call_keep(rt, ctx, 0x08B35FD8u, {s5, s1});
                if (signed_of(ctx.gpr[2]) >= 0) {
                    s2 = s3 * 12u + s7;
                    s0 = s2 + 4u;
                    break;
                }
            }
            s1 = s3 * 12u + s7;
            ctx.gpr[4] = s1 + 4u;
            s0 = s2 + 4u;
            put(rt, s1, s5);
            if (ctx.gpr[4] != s0) call_set(rt, ctx, 0x08A5BAD8u, {ctx.gpr[4], word(rt, s2 + 4u)});
            ctx.gpr[3] = s6 - 1u;
            ctx.gpr[5] = byte_u(rt, s2 + 8u);
            ctx.gpr[2] = (ctx.gpr[3] >> 31) + ctx.gpr[3];
            ctx.gpr[4] = signed_of(fp) < signed_of(s6) ? 1u : 0u;
            s3 = s6;
            put8(rt, s1 + 8u, ctx.gpr[5]);
            if (ctx.gpr[4] == 0u) {
                s1 = word(rt, s4);
                break;
            }
            s6 = static_cast<u32>(signed_of(ctx.gpr[2]) >> 1);
        }
    }
    put(rt, s2, s1);
    ctx.gpr[2] = s4 + 4u;
    if (s0 != ctx.gpr[2]) call_set(rt, ctx, 0x08A5BAD8u, {s0, word(rt, s4 + 4u)});
    ctx.gpr[2] = byte_u(rt, s4 + 8u);
    put8(rt, s2 + 8u, ctx.gpr[2]);
}

// 0x08C4FD6C (a0 = object, a1 = count, a2 = source, a3 = callback, t0 = bounds {+0 limit, +4 span}): a count not below the limit
// loses the span; rounds = count - 1. Round k (index = rounds - k, wrapped = index + span; s0 = index when index < limit, else
// wrapped): scale = 0x08B1EF70(source, a1) times (index + 1) as an unsigned-aware float, truncated (values from 2^31 up go through
// the subtract / or 0x80000000 path), moved up by the span when not below the limit; callback(object, s0, target, callback).
// v0 = (index < limit) after the last round; with no rounds v0 = count - span.
void func_08C4FD6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    u32 &s0 = ctx.gpr[16], &s1 = ctx.gpr[17], &s2 = ctx.gpr[18], &s3 = ctx.gpr[19], &s4 = ctx.gpr[20], &s5 = ctx.gpr[21],
        &s6 = ctx.gpr[22], &s7 = ctx.gpr[23], &fp = ctx.gpr[30];
    const u32 bounds = ctx.gpr[8];
    s6 = word(rt, bounds + 4u);
    s3 = word(rt, bounds);
    ctx.gpr[2] = ctx.gpr[5] - s6;
    ctx.gpr[3] = ctx.gpr[5] < s3 ? 1u : 0u;
    if (ctx.gpr[3] == 0u) ctx.gpr[5] = ctx.gpr[2];
    s5 = ctx.gpr[5] - 1u;
    fp = ctx.gpr[6];
    s7 = ctx.gpr[7];
    put(rt, frame.at(0u), self);
    if (s5 == 0u) return;
    s2 = s5 + s6;
    s1 = s5;
    s4 = 0u;
    do {
        ctx.gpr[2] = s1 < s3 ? 1u : 0u;
        ctx.gpr[4] = fp;
        s0 = s1;
        if (ctx.gpr[2] == 0u) s0 = s2;
        const GuestResult scale_result = call_keep(rt, ctx, 0x08B1EF70u, {fp, ctx.gpr[5]}, {});
        const u32 next = s1 + 1u;
        ctx.gpr[2] = next;
        ctx.fpr[1] = scale_result.f0;
        s4 += 1u;
        ctx.gpr[5] = s0;
        ctx.gpr[4] = self;
        s2 -= 1u;
        s1 -= 1u;
        float factor;
        if (signed_of(next) >= 0) {
            factor = static_cast<float>(signed_of(next));
        } else {
            const u32 halved = (next >> 1) | (next & 1u);
            const float half = static_cast<float>(signed_of(halved));
            factor = half + half;
        }
        const float product = psp_mul(scale_result.f0, factor);
        u32 converted;
        if (2147483648.0f <= product) {
            converted = static_cast<u32>(trunc_w_s(product - 2147483648.0f)) | 0x80000000u;
        } else {
            converted = static_cast<u32>(trunc_w_s(product));
        }
        ctx.gpr[6] = converted;
        ctx.gpr[3] = converted < s3 ? 1u : 0u;
        ctx.gpr[2] = converted + s6;
        if (ctx.gpr[3] == 0u) ctx.gpr[6] = ctx.gpr[2];
        const GuestResult callback_result = call_keep(rt, ctx, s7, {self, s0, ctx.gpr[6], ctx.gpr[7]}, {});
        ctx.gpr[5] = callback_result.a1;
        ctx.gpr[2] = s1 < s3 ? 1u : 0u;
    } while (s5 != s4);
}

// 0x08B03CE4 (a0 = object {+0 rows, +0x60 list base}, a1 = row, a2 = argument, a3 = holder whose halfword at +4 is the first
// index, t0 = visitor object): a negative first index answers it (v0). Else walks the 0x54-byte entries from the list base +
// 84 * index: each entry (s0) with a type byte at +0x50 below 3 first calls the visitor's slot +8 with (visitor, row slot, a2,
// entry + 0x18, entry + 0xC, entry) and f12..f14 = floats at +0x34 / +0x24 / +0x2C; the next entry is the halfword at +0x52
// (negative ends: v0 = 12 * it) of the list at +0x60 (list base + 84 * next). v0 on the other exits is the last value loaded.

// 0x08AF5164 (a0 = object, a1 = flag byte): with the byte at +0x426 set and a zero flag, the virtual slot 0x14 of the sub-objects at
// +0x1A8 and +0x27C runs; with it clear and a non-zero flag, both are set up with 0x08AFAD8C(sub, 0x1E0, 0x110, 3, 0, 0), their
// slot 0x18 runs, the object's slot 0x3C and 0x08AF5040(object, 0, 0 and the floats 0, 0, 480, 272, 0, 0, 1, 1) follow, then the
// slot 0x3C of the sub-object at +0xD4 and 0x08AF5040 again. Then 0x08B344E0(0x08D470C4), the flag stored at +0x427, and a tail call
// of 0x08B3457C(0x08D470C4) after the frame is popped (s0 = 0x08D50000 on every path).
void func_08AF5164(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        u32 &s0 = ctx.gpr[16], &s1 = ctx.gpr[17], &s2 = ctx.gpr[18], &s3 = ctx.gpr[19];
        s2 = ctx.gpr[5] & 0xFFu;
        s1 = self;
        const auto floats = {0.0f, 0.0f, 480.0f, 272.0f, 0.0f, 0.0f, 1.0f, 1.0f};
        if (byte_u(rt, self + 0x426u) != 0u) {
            ctx.gpr[4] = self + 0x1A8u;
            if (s2 == 0u) {
                s0 = 0x08D50000u;
                call_virtual(rt, ctx, self + 0x1A8u, 0x14u, {self + 0x1A8u});
                ctx.gpr[5] = word(rt, s1 + 0x27Cu);
                ctx.gpr[2] = word(rt, ctx.gpr[5] + 0x14u);
                call_set(rt, ctx, ctx.gpr[2], {s1 + 0x27Cu, ctx.gpr[5]});
            }
        } else {
            s0 = self + 0x1A8u;
            ctx.gpr[4] = s0;
            ctx.gpr[5] = 0x1E0u;
            ctx.gpr[6] = 0x110u;
            ctx.gpr[7] = 3u;
            ctx.gpr[8] = 0u;
            ctx.gpr[9] = 0u;
            s3 = s1 + 0x27Cu;
            if (s2 != 0u) {
                call_set(rt, ctx, 0x08AFAD8Cu, {s0, 0x1E0u, 0x110u, 3u, 0u, 0u});
                call_set(rt, ctx, 0x08AFAD8Cu, {s3, 0x1E0u, 0x110u, 3u, 0u, 0u});
                call_virtual(rt, ctx, s0, 0x18u, {s0});
                call_virtual(rt, ctx, s1, 0x3Cu, {s1, 0u, 0u});
                {
                    const GuestResult r = call_guest(rt, ctx, 0x08AF5040u, {s1, 0u, 0u}, floats);
                    take(ctx, r);
                }
                call_virtual(rt, ctx, s3, 0x18u, {s3});
                call_virtual(rt, ctx, s1 + 0xD4u, 0x3Cu, {s1 + 0xD4u, 0u, 0u});
                const GuestResult r = call_guest(rt, ctx, 0x08AF5040u, {s1 + 0xD4u, 0u, 0u}, floats);
                take(ctx, r);
            }
        }
        s0 = 0x08D50000u;
        call_set(rt, ctx, 0x08B344E0u, {0x08D470C4u});
        put8(rt, s1 + 0x427u, s2);
        ctx.gpr[4] = 0x08D470C4u;
    }
    tail_call(ctx, 0x08B3457Cu, {0x08D470C4u});
}

// Copies 0x3C bytes (the original moves four words per round, then three): `from` to `to`.
void copy_block_3C(Runtime &rt, u32 to, u32 from) {
    for (u32 offset = 0u; offset < 0x3Cu; offset += 4u) put(rt, to + offset, word(rt, from + offset));
}

// 0x08B48A74 (a0 = object, a1 = argument, a2 = first (0 = none yet), a3 = limit): a set word at +0 answers 1. Else 0x08B0EC60 fills
// the block at sp + 0x80 (copied to sp); s2 = its word at +0x10. A zero `first` becomes 0x08B35DA0(s2) and sets the byte at +4;
// else a limit below s2 answers 0. 0x08B100D0(sp + 0xC0, argument, sp, 1) after storing first / s2 at sp + 0 / 4 (copied to sp + 0x40):
// a non-zero word at +0x40 answers 0; else 0x08B48A68(object, first) and 1. v0 = v1 as the original leaves it.
void func_08B48A74(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x120u);
    u32 &s0 = ctx.gpr[16], &s1 = ctx.gpr[17], &s2 = ctx.gpr[18], &s3 = ctx.gpr[19], &s4 = ctx.gpr[20], &s5 = ctx.gpr[21];
    ctx.gpr[3] = 1u;
    s5 = ctx.gpr[5];
    s4 = ctx.gpr[7];
    s3 = ctx.gpr[6];
    s1 = self;
    if (word(rt, self) == 0u) {
        s0 = frame.at(0x80u);
        call_set(rt, ctx, 0x08B0EC60u, {s0});
        copy_block_3C(rt, frame.at(0u), frame.at(0x80u));
        s0 = frame.at(0xB0u) + 0x10u;
        s2 = word(rt, frame.at(0x10u));
        ctx.gpr[3] = 0u;
        bool proceed = true;
        if (s3 == 0u) {
            s3 = call_set(rt, ctx, 0x08B35DA0u, {s2});
            put8(rt, s1 + 4u, 1u);
        } else {
            ctx.gpr[2] = signed_of(s4) < signed_of(s2) ? 1u : 0u;
            if (ctx.gpr[2] != 0u) proceed = false;
        }
        if (proceed) {
            s0 = frame.at(0xC0u);
            put(rt, frame.at(4u), s2);
            put(rt, frame.at(0u), s3);
            call_set(rt, ctx, 0x08B100D0u, {s0, s5, frame.at(0u), 1u});
            copy_block_3C(rt, frame.at(0x40u), frame.at(0xC0u));
            s0 = frame.at(0xF0u) + 0x10u;
            ctx.gpr[2] = word(rt, frame.at(0x40u));
            ctx.gpr[3] = 0u;
            if (ctx.gpr[2] == 0u) {
                call_set(rt, ctx, 0x08B48A68u, {s1, s3});
                ctx.gpr[3] = 1u;
            }
        }
    }
    ctx.gpr[2] = ctx.gpr[3];
}

// 0x08B65728 (a0 = vector-like object {+0x10 / +0x14 flag words, +0x1C / +0x20 slots, +0x24 inline table of 8 entries, +0x64 capacity,
// +0x68 table pointer}, a1 = wanted index, a2 = flag): an index below 8 uses the inline table (capacity 8). 0x7FFFFFFF marks the
// special slot: bit 0 is set in the word at +0x14 and a bit shared with +0x10 reports through 0x08B5AA30(0x08CB5BA8); else the slot
// at +0x20 (flag set) or +0x1C is cleared and v0 = object + 0x1C. Any other large index allocates (index + 1) 8-byte entries with
// 0x08B6461C, zeroes them, copies the old table over, frees a non-inline old table with 0x08B61FC0 and stores capacity / table.
// v0 = the table + index * 8.

// 0x08B56D4C (a0 = object): when bit 10 of the word at +0xD1C is set, it is cleared and the matrix 0x08B56A88(object) (M200) and the
// one at *(object + 0xD00) (M100) are multiplied (vmmul.q M000, M200, M100); the four columns of M000 are stored at +0x840 .. +0x870.
// v0 = object + 0x800 either way.

// 0x08BE241C (a0 = the object): the deleting destructor of a class holding a chain of shared holders: vtable 0x08CC6868, the holders
// at 0x84, 0x7C and 0x74 are released, then the string block at 0x50, then 0x4C, 0x44, 0x3C; the vtable moves to 0x08CC6888, the holder at
// 0x2C is released, the last one at 0x24 only releases its +8 count through the +0xC virtual; the vtable ends as 0x08CC3000 and
// 0x08B64654(object) frees the object. v0 = its answer.
void func_08BE241C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    put(rt, self, 0x08CC6868u);
    release_holder(rt, ctx, word(rt, self + 0x84u));
    release_holder(rt, ctx, word(rt, self + 0x7Cu));
    release_holder(rt, ctx, word(rt, self + 0x74u));
    release_string(rt, ctx, word(rt, self + 0x50u), frame.at(0u));
    release_holder(rt, ctx, word(rt, self + 0x4Cu));
    ctx.gpr[18] = self + 0x40u;
    release_holder(rt, ctx, word(rt, self + 0x44u));
    release_holder(rt, ctx, word(rt, self + 0x3Cu));
    put(rt, self, 0x08CC6888u);
    release_holder(rt, ctx, word(rt, self + 0x2Cu));
    const u32 last = word(rt, self + 0x24u);
    ctx.gpr[4] = last;
    if (last != 0u && count_down(rt, ctx, last + 8u)) call_vtable(rt, ctx, last, 0xCu);
    put(rt, self, 0x08CC3000u);
    ctx.gpr[4] = self;
    call_keep(rt, ctx, 0x08B64654u, {self});
}

// 0x08BE6D44 (a0 = destination of `a1` 0x28-byte records, a2 = source record): the `lui v0` in the first branch's delay slot always
// runs, so v0 = 0x08CD0000 even for a zero count. Each record: unless the destination is null, words +4 / +8 and the floats +0xC ..
// +0x1C and +0x20 / +0x24 of the source are copied and the vtable 0x08CCE6E0 stored at +0; the source stays the same record.
void func_08BE6D44(Runtime &rt, AllegrexContext &ctx) {
    ctx.gpr[2] = 0x08CD0000u;
    if (ctx.gpr[5] == 0u) return;
    ctx.gpr[8] = 0x08CCE6E0u;
    ctx.gpr[7] = 0u;
    do {
        ctx.gpr[7] += 1u;
        if (ctx.gpr[4] != 0u) {
            const u32 source = ctx.gpr[6];
            const u32 destination = ctx.gpr[4];
            ctx.gpr[2] = word(rt, source + 4u);
            ctx.gpr[3] = word(rt, source + 8u);
            put(rt, destination + 4u, ctx.gpr[2]);
            put(rt, destination + 8u, ctx.gpr[3]);
            for (u32 offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
            put(rt, destination, ctx.gpr[8]);
            put(rt, destination + 0x20u, word(rt, source + 0x20u));
            put(rt, destination + 0x24u, word(rt, source + 0x24u));
        }
        ctx.gpr[4] += 0x28u;
    } while (ctx.gpr[5] != ctx.gpr[7]);
}

// 0x08B7E014 (a0 = errno word, a1 = text, a2 = end pointer or 0, a3 = base): strtoll. The class table at 0x08C96BD9 (bit 8 space,
// bit 4 digit, bits 1 / 2 upper / lower case letters) skips spaces and reads digits; a sign, a base of 0 or 16 with "0x" / "0X", and a
// base of 0 meaning 8 after a leading '0' else 10, as the asm decides. The limit is 2^63 (negative) or 2^63 - 1; the guest
// helpers 0x08B708C4 (remainder, the low word kept at sp) and 0x08B702B4 (quotient) split it by the base, and each digit
// accumulates as a 64-bit acc * base + digit unless it would pass the limit (the status t1 then stays -1: v0:v1 is the limit and
// errno = 0x22). The end pointer gets the position after the last digit (the text itself when no digit was read).
void func_08B7E014(Runtime &rt, AllegrexContext &ctx) {
    const u32 errno_out = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 end_out = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    u32 &s0 = ctx.gpr[16], &s1 = ctx.gpr[17], &s2 = ctx.gpr[18], &s3 = ctx.gpr[19], &s4 = ctx.gpr[20], &s5 = ctx.gpr[21],
        &s6 = ctx.gpr[22], &s7 = ctx.gpr[23], &fp = ctx.gpr[30];
    constexpr u32 table = 0x08C96BD9u;
    s7 = text;
    s6 = ctx.gpr[7];
    put(rt, frame.at(4u), errno_out);
    put(rt, frame.at(8u), end_out);
    ctx.gpr[5] = table;
    u32 cursor = s7;
    for (;;) {
        s0 = sext8(byte_u(rt, cursor));
        s1 = cursor + 1u;
        if ((byte_u(rt, s0 + table) & 8u) == 0u) break;
        cursor = s1;
    }
    fp = 0u;
    if (s0 == 0x2Du) {
        s0 = sext8(byte_u(rt, s1));
        fp = 1u;
        s1 = cursor + 2u;
    } else if (s0 == 0x2Bu) {
        s0 = sext8(byte_u(rt, s1));
        s1 = cursor + 2u;
    }
    // The base: 0 or 16 look for a "0x" prefix; 0 becomes 8 after a '0', else 10.
    bool base_known = false;
    if (s6 == 0u || s6 == 0x10u) {
        if (s0 == 0x30u) {
            const u32 after = sext8(byte_u(rt, s1));
            if (after == 0x78u || after == 0x58u) {
                s0 = sext8(byte_u(rt, s1 + 1u));
                s6 = 0x10u;
                s1 += 2u;
                s4 = 0x10u;
                s5 = 0u;
                base_known = true;
            }
        }
        if (!base_known && s6 == 0u) {
            s6 = 8u;
            s4 = 8u;
            s5 = 0u;
            if (s0 != 0x30u) {
                s6 = 10u;
                s4 = 10u;
            }
            base_known = true;
        }
    }
    if (!base_known) {
        s4 = s6;
        s5 = static_cast<u32>(signed_of(s6) >> 31);
    }
    s2 = 0u;
    s3 = 0x80000000u;
    if (fp == 0u) {
        s3 = 0x7FFFFFFFu;
        s2 = 0xFFFFFFFFu;
    }
    const u32 remainder_low = call_set(rt, ctx, 0x08B708C4u, {s2, s3, s4, s5});
    put(rt, frame.at(0u), remainder_low);
    call_set(rt, ctx, 0x08B702B4u, {s2, s3, s4, s5});
    u32 cutoff_low = ctx.gpr[2];
    u32 cutoff_high = ctx.gpr[3];
    u32 low = 0u;
    u32 high = 0u;
    u32 status = 0u;  // t1: 0 nothing yet, 1 a digit, -1 overflow
    for (;;) {
        const u32 cls = byte_u(rt, s0 + table);
        u32 digit = s0 - 0x30u;
        if ((cls & 4u) == 0u) {
            if ((cls & 3u) == 0u) break;
            digit = s0 - ((cls & 1u) != 0u ? 0x37u : 0x57u);
        }
        if (!(signed_of(digit) < signed_of(s6))) break;
        bool overflow = false;
        if (signed_of(status) >= 0) {
            if (cutoff_high < high) {
                overflow = true;
            } else if (cutoff_high == high && cutoff_low < low) {
                overflow = true;
            } else if (low == cutoff_low && high == cutoff_high && signed_of(word(rt, frame.at(0u))) < signed_of(digit)) {
                overflow = true;
            }
        } else {
            overflow = true;
        }
        s0 = sext8(byte_u(rt, s1));
        s1 += 1u;
        if (overflow) {
            status = 0xFFFFFFFFu;
            continue;
        }
        status = 1u;
        const u64 base64 = (static_cast<u64>(s5) << 32) | s4;
        const u64 digit64 = static_cast<u64>(static_cast<std::int64_t>(signed_of(digit)));
        const u64 accumulated = ((static_cast<u64>(high) << 32) | low) * base64 + digit64;
        low = static_cast<u32>(accumulated);
        high = static_cast<u32>(accumulated >> 32);
    }
    u32 result_low;
    u32 result_high;
    if (signed_of(status) < 0) {
        result_low = 0u;
        result_high = 0x80000000u;
        if (fp == 0u) {
            result_low = 0xFFFFFFFFu;
            result_high = 0x7FFFFFFFu;
        }
        put(rt, errno_out, 0x22u);
    } else {
        result_low = low;
        result_high = high;
        if (fp != 0u) {
            result_low = 0u - low;
            result_high = (0u - high) - (result_low != 0u ? 1u : 0u);
        }
    }
    if (end_out != 0u) {
        if (status != 0u) s7 = s1 - 1u;
        put(rt, end_out, s7);
    }
    ctx.gpr[2] = result_low;
    ctx.gpr[3] = result_high;
}

// 0x08B7E564 (a0 = errno word, a1 = text, a2 = end pointer or 0, a3 = base): strtoull. The scan, sign and base rules of 0x08B7E014
// with the limit 2^64 - 1: cutoff = 0x08B702B4(-1, -1, base) (s2 low, s3 high), remainder low = 0x08B708C4(...) (t6). A digit that
// would pass the limit (high / low compared unsigned, an equal cutoff compared with the remainder as signed words) keeps consuming and
// leaves status -1: v0:v1 = -1 and errno = 0x22. A minus sign (the word at sp) negates the 64-bit value. The end pointer gets
// the position after the last digit (when the status is non-zero), else the text.
void func_08B7E564(Runtime &rt, AllegrexContext &ctx) {
    const u32 errno_out = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 end_out = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    u32 &s0 = ctx.gpr[16], &s1 = ctx.gpr[17], &s2 = ctx.gpr[18], &s3 = ctx.gpr[19], &s4 = ctx.gpr[20], &s5 = ctx.gpr[21],
        &s6 = ctx.gpr[22], &s7 = ctx.gpr[23], &fp = ctx.gpr[30];
    constexpr u32 table = 0x08C96BD9u;
    s7 = text;
    fp = end_out;
    s6 = ctx.gpr[7];
    put(rt, frame.at(4u), errno_out);
    ctx.gpr[5] = table;
    u32 cursor = s7;
    for (;;) {
        s0 = sext8(byte_u(rt, cursor));
        s1 = cursor + 1u;
        if ((byte_u(rt, s0 + table) & 8u) == 0u) break;
        cursor = s1;
    }
    put(rt, frame.at(0u), 0u);
    if (s0 == 0x2Du) {
        s0 = sext8(byte_u(rt, s1));
        s1 = cursor + 2u;
        put(rt, frame.at(0u), 1u);
    } else if (s0 == 0x2Bu) {
        s0 = sext8(byte_u(rt, s1));
        s1 = cursor + 2u;
    }
    bool base_known = false;
    if (s6 == 0u || s6 == 0x10u) {
        if (s0 == 0x30u) {
            const u32 after = sext8(byte_u(rt, s1));
            if (after == 0x78u || after == 0x58u) {
                s0 = sext8(byte_u(rt, s1 + 1u));
                s6 = 0x10u;
                s1 += 2u;
                s4 = 0x10u;
                s5 = 0u;
                base_known = true;
            }
        }
        if (!base_known && s6 == 0u) {
            s6 = 8u;
            s4 = 8u;
            s5 = 0u;
            if (s0 != 0x30u) {
                s6 = 10u;
                s4 = 10u;
            }
            base_known = true;
        }
    }
    if (!base_known) {
        s4 = s6;
        s5 = static_cast<u32>(signed_of(s6) >> 31);
    }
    ctx.gpr[4] = 0xFFFFFFFFu;
    ctx.gpr[5] = 0xFFFFFFFFu;
    call_set(rt, ctx, 0x08B702B4u, {0xFFFFFFFFu, 0xFFFFFFFFu, s4, s5});
    s3 = ctx.gpr[3];
    s2 = ctx.gpr[2];
    call_set(rt, ctx, 0x08B708C4u, {0xFFFFFFFFu, 0xFFFFFFFFu, s4, s5});
    const u32 remainder_low = ctx.gpr[2];
    u32 low = 0u;
    u32 high = 0u;
    u32 status = 0u;  // t1
    for (;;) {
        const u32 cls = byte_u(rt, s0 + table);
        u32 digit = s0 - 0x30u;
        if ((cls & 4u) == 0u) {
            if ((cls & 3u) == 0u) break;
            digit = s0 - ((cls & 1u) != 0u ? 0x37u : 0x57u);
        }
        if (!(signed_of(digit) < signed_of(s6))) break;
        bool overflow = signed_of(status) < 0;
        if (!overflow) {
            if (s3 < high) {
                overflow = true;
            } else if (s3 == high && s2 < low) {
                overflow = true;
            } else if (low == s2 && high == s3 && signed_of(remainder_low) < signed_of(digit)) {
                overflow = true;
            }
        }
        s0 = sext8(byte_u(rt, s1));
        s1 += 1u;
        if (overflow) {
            status = 0xFFFFFFFFu;
            continue;
        }
        status = 1u;
        const u64 base64 = (static_cast<u64>(s5) << 32) | s4;
        const u64 digit64 = static_cast<u64>(static_cast<std::int64_t>(signed_of(digit)));
        const u64 accumulated = ((static_cast<u64>(high) << 32) | low) * base64 + digit64;
        low = static_cast<u32>(accumulated);
        high = static_cast<u32>(accumulated >> 32);
    }
    if (signed_of(status) < 0) {
        put(rt, errno_out, 0x22u);
        low = 0xFFFFFFFFu;
        high = 0xFFFFFFFFu;
    } else if (word(rt, frame.at(0u)) != 0u) {
        const u32 negated = 0u - low;
        high = (0u - high) - (negated != 0u ? 1u : 0u);
        low = negated;
    }
    if (fp != 0u) {
        if (status != 0u) s7 = s1 - 1u;
        put(rt, fp, s7);
    }
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

void func_08B73410(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 end = ctx.gpr[5];
    const u32 state = ctx.gpr[6];
    const u32 out = ctx.gpr[7];
    ctx.gpr[17] = out;
    ctx.gpr[18] = ctx.gpr[4] + 1u;  // s2, the next read position
    ctx.gpr[19] = state;
    ctx.gpr[20] = end;
    ctx.gpr[21] = 0u;  // s5, the node case 0x0B pops
    put(rt, out + 0x998u, 0u);  // the prologue stores it even when the first test returns
    u32 pos = ctx.gpr[4];
    if (!(pos < end)) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 accumulator = word(rt, out + 0x9ACu);  // a2, reloaded before every round except after case 0x2D
    if (!(accumulator < word(rt, state + 0x4D0u))) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 node = ctx.gpr[21];
    for (;;) {
        const u32 opcode = rt.memory().load8(pos);
        const u32 family = opcode & 0xC0u;
        u32 next = pos + 1u;
        if (family == 0x40u) {
            // `mtlo a2; madd (opcode & 0x3F), word(out+0x9B8)`: the low word of the sum.
            accumulator += (opcode & 0x3Fu) * word(rt, out + 0x9B8u);
            put(rt, out + 0x9ACu, accumulator);
        } else if (family == 0x80u) {
            accumulator = opcode & 0x3Fu;
            ctx.gpr[6] = accumulator;
            put(rt, frame.at(0u), accumulator);
            const u32 decoded = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(4u)});
            const u32 entry = out + (word(rt, frame.at(0u)) << 3);
            put(rt, entry + 4u, 1u);
            put(rt, entry, word(rt, out + 0x9B4u) * word(rt, frame.at(4u)));
            next = decoded;
        } else if (family == 0xC0u) {
            accumulator = opcode & 0x3Fu;
            ctx.gpr[6] = accumulator;
            put(rt, frame.at(0u), accumulator);
            put(rt, out + accumulator * 8u + 4u, 0u);
        } else if (opcode >= 0x30u) {
            // The unknown-opcode handler gets a0 = the family (0 here), a1 = end, a2 = the accumulator and
            // a3 = out, as the registers stand at 0x08B73498.
            ctx.gpr[2] = call_set(rt, ctx, 0x08B77B04u, {0u, end, accumulator, out});
            return;
        } else {
            // The 48-entry jump table at 0x08C967FC maps opcode 0x00..0x2F to the cases below in order; the
            // labels are decimal (the table is data, not a value the disassembly view prints) and 0x14..0x2C all
            // land on the unknown-opcode handler (the default).
            s32 jump_case = 0;  // 0: the ordinary round tail, 1: case 0x2D's (a2 not reloaded)
            switch (opcode) {
            case 0u:
                break;  // nothing but the tail
            case 1u: {
                const u32 mode = rt.memory().load8(out + 0x9C0u);
                u32 mode_value = mode;
                ctx.gpr[16] = mode_value;
                const u32 first = call_set(rt, ctx, 0x08B72704u, {mode_value, state});
                next = call_set(rt, ctx, 0x08B72830u, {mode_value, first, next, out + 0x9ACu});
                break;
            }
            case 2u:
                accumulator += rt.memory().load8(next) * word(rt, out + 0x9B8u);
                put(rt, out + 0x9ACu, accumulator);
                next = pos + 2u;
                break;
            case 3u: {
                const u32 half = rt.memory().load8(next) | (rt.memory().load8(next + 1u) << 8u);
                accumulator += half * word(rt, out + 0x9B8u);
                put(rt, out + 0x9ACu, accumulator);
                next = pos + 3u;
                break;
            }
            case 4u:
                accumulator += word(rt, next) * word(rt, out + 0x9B8u);
                put(rt, out + 0x9ACu, accumulator);
                next = pos + 5u;
                break;
            case 5u: {
                const u32 first = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(0u)});
                const u32 decoded = call_set(rt, ctx, 0x08B727A4u, {first, frame.at(4u)});
                const u32 entry = out + (word(rt, frame.at(0u)) << 3);
                put(rt, entry + 4u, 1u);
                put(rt, entry, word(rt, out + 0x9B4u) * word(rt, frame.at(4u)));
                next = decoded;
                break;
            }
            case 6u:
            case 7u:
            case 8u: {
                const u32 decoded = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(0u)});
                put(rt, out + (word(rt, frame.at(0u)) << 3) + 4u, 0u);
                next = decoded;
                break;
            }
            case 9u: {
                const u32 first = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(0u)});
                const u32 decoded = call_set(rt, ctx, 0x08B727A4u, {first, frame.at(0xCu)});
                const u32 entry = out + (word(rt, frame.at(0u)) << 3);
                put(rt, entry, word(rt, frame.at(0xCu)));
                put(rt, entry + 4u, 2u);
                next = decoded;
                break;
            }
            case 10u: {
                // Pushes the whole 0x99C-byte object at `out` onto the linked list kept at out+0x998: reuse
                // the popped node (no allocation) or use a fresh 0x9B0-byte block below the current sp, which
                // stays lowered for the rest of the function as in the original.
                if (node != 0u) {
                    const u32 reused = node;
                    node = word(rt, reused + 0x998u);
                    for (u32 offset = 0u; offset < 0x99Cu; offset += 4u) put(rt, reused + offset, word(rt, out + offset));
                    put(rt, out + 0x998u, reused);
                } else {
                    ctx.gpr[29] = (ctx.gpr[29] - 0x9B0u) & ~15u;
                    const u32 fresh = ctx.gpr[29];
                    for (u32 offset = 0u; offset < 0x99Cu; offset += 4u) put(rt, fresh + offset, word(rt, out + offset));
                    put(rt, out + 0x998u, fresh);
                }
                break;
            }
            case 11u: {
                // Pops: copies the node at out+0x998 back over `out`, links the node to the previous top and
                // keeps it as the node the next 0x0A reuses.
                const u32 popped = word(rt, out + 0x998u);
                for (u32 offset = 0u; offset < 0x99Cu; offset += 4u) put(rt, out + offset, word(rt, popped + offset));
                put(rt, popped + 0x998u, node);
                node = popped;
                break;
            }
            case 12u: {
                const u32 first = call_set(rt, ctx, 0x08B727A4u, {next, out + 0x9A0u});
                const u32 decoded = call_set(rt, ctx, 0x08B727A4u, {first, frame.at(4u)});
                put(rt, out + 0x9A8u, 1u);
                put(rt, out + 0x99Cu, word(rt, frame.at(4u)));
                next = decoded;
                break;
            }
            case 13u: {
                next = call_set(rt, ctx, 0x08B727A4u, {next, out + 0x9A0u});
                put(rt, out + 0x9A8u, 1u);
                break;
            }
            case 14u: {
                next = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(4u)});
                put(rt, out + 0x99Cu, word(rt, frame.at(4u)));
                break;
            }
            case 15u: {
                put(rt, out + 0x9A4u, next);
                put(rt, out + 0x9A8u, 2u);
                const u32 decoded = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(4u)});
                next = decoded + word(rt, frame.at(4u));
                break;
            }
            case 16u: {
                // The entry's value is the first decode's returned pointer, its type 3; both stores happen
                // before the second decode (they sit in its delay slot).
                const u32 first = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(0u)});
                const u32 entry = out + (word(rt, frame.at(0u)) << 3);
                put(rt, entry + 4u, 3u);
                put(rt, entry, first);
                const u32 decoded = call_set(rt, ctx, 0x08B727A4u, {first, frame.at(4u)});
                next = decoded + word(rt, frame.at(4u));
                break;
            }
            case 17u: {
                const u32 first = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(0u)});
                const u32 decoded = call_set(rt, ctx, 0x08B727D8u, {first, frame.at(8u)});
                const u32 entry = out + (word(rt, frame.at(0u)) << 3);
                put(rt, entry + 4u, 1u);
                put(rt, entry, word(rt, out + 0x9B4u) * word(rt, frame.at(8u)));
                next = decoded;
                break;
            }
            case 18u: {
                const u32 first = call_set(rt, ctx, 0x08B727A4u, {next, out + 0x9A0u});
                next = call_set(rt, ctx, 0x08B727D8u, {first, out + 0x99Cu});
                put(rt, out + 0x9A8u, 1u);
                break;
            }
            case 19u:
                next = call_set(rt, ctx, 0x08B727D8u, {next, out + 0x99Cu});
                break;
            case 45u: {
                // Fills entries 0x11..0x20 with {4 * (index - 0x11), type 1} and remembers the last index in
                // the frame word at sp+0. a2 is left at 0x20, so the next round's accumulator starts there.
                u32 index = 0x10u;
                u32 value = 0u;
                do {
                    ++index;
                    put(rt, out + index * 8u + 4u, 1u);
                    put(rt, out + index * 8u, value);
                    value += 4u;
                    put(rt, frame.at(0u), index);
                } while (index < 0x20u);
                next = pos + 1u;
                accumulator = 0x20u;
                jump_case = 1;
                break;
            }
            case 46u:
                next = call_set(rt, ctx, 0x08B727A4u, {next, state + 0x4E4u});
                break;
            case 47u: {
                const u32 first = call_set(rt, ctx, 0x08B727A4u, {next, frame.at(0u)});
                const u32 decoded = call_set(rt, ctx, 0x08B727A4u, {first, frame.at(4u)});
                const u32 entry = out + (word(rt, frame.at(0u)) << 3);
                put(rt, entry + 4u, 1u);
                put(rt, entry, 0u - word(rt, out + 0x9B4u) * word(rt, frame.at(4u)));
                next = decoded;
                break;
            }
            default:
                // 0x14..0x2C: the unknown-opcode handler (same registers as above), then the epilogue.
                ctx.gpr[2] = call_set(rt, ctx, 0x08B77B04u, {0u, end, accumulator, out});
                return;
            }
            if (jump_case != 0) {
                // Case 0x2D jumps to the exit test directly: the accumulator is not reloaded from out+0x9AC.
                if (!(next < end)) {
                    ctx.gpr[2] = 0u;
                    return;
                }
                ctx.gpr[8] = next;
                if (!(accumulator < word(rt, state + 0x4D0u))) {
                    ctx.gpr[2] = 0u;
                    return;
                }
                pos = next;
                ctx.gpr[21] = node;
                continue;
            }
        }
        // 0x08B7352C: the round tail (v0 = 0 on both exits).
        if (!(next < end)) {
            ctx.gpr[2] = 0u;
            return;
        }
        accumulator = word(rt, out + 0x9ACu);
        ctx.gpr[8] = next;
        if (!(accumulator < word(rt, state + 0x4D0u))) {
            ctx.gpr[2] = 0u;
            return;
        }
        pos = next;
        ctx.gpr[21] = node;
    }
}

// 0x08B907F8 (a0 = object): a constructor of a 0x46A8-byte object: the base 0x08AD023C(object), the vtable 0x08CBF5E8 and the fields, then the
// members built in place by their own constructors (+0xA0, +0xD8, +0x340, +0x5BC, ... +0x4670), two 0x1C38-byte units at +0x608, four
// 8-byte records at +0x3FEC, a list at +0x4584 that gets the members and four table answers, and the small tables at +0x3E7C / +0x3E84 /
// +0x3E8C / +0x4260. Stores are words unless the asm uses sb / sh (0x9C is a byte); the delay slot of each jal runs before it.

// The shared body of 0x08C70FE8, 0x08C71D00 (one extra argument, kept in s5; 0x08C70FE8 masks it to a byte) and 0x08C71544, 0x08C717C8,
// 0x08C71A7C (two extra arguments, a2 -> s4 and a3 -> s5): a0 = this. 0x08C70314 fills a guard at sp + 0x10 (its byte says whether
// the stream is usable, its word at +0x14 is the object the guard watches). When usable, the object derived from the vptr
// (`this + word(vptr - 0xC)`, s0) hands its word +0x80 (s1; null calls 0x08B5A9FC, which does not return) to the virtual at `slot`
// with (s1, &{word +0x78, empty flag} at sp + 8, s0, the byte at +0x74, extra...). The byte at +0x74 is filled by 0x08C5477C(s0, 0x20)
// the first time (flag +0x75). A non-zero low byte of the virtual's v1 sets bit 0 of the flags at +0x14 through 0x08C54CFC. Then
// the guard's object: with bit 0x2000 of its +0xC word set and 0x08B62318() zero, its +0x78 object's virtual +0x18 answering -1
// raises the flags again. v0 = this on every path that returns.
void update_through_vtable(Runtime &rt, AllegrexContext &ctx, u32 slot, unsigned extras, bool mask_byte) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    u32 &s0 = ctx.gpr[16], &s1 = ctx.gpr[17], &s2 = ctx.gpr[18], &s3 = ctx.gpr[19], &s4 = ctx.gpr[20], &s5 = ctx.gpr[21],
        &s6 = ctx.gpr[22];
    if (extras == 1u) {
        s2 = self;
        s5 = mask_byte ? (ctx.gpr[5] & 0xFFu) : ctx.gpr[5];
    } else {
        ctx.gpr[5] = self;
        s2 = self;
        s5 = ctx.gpr[7];
        s4 = ctx.gpr[6];
    }
    ctx.gpr[4] = frame.at(0x10u);
    ctx.gpr[5] = s2;
    call_keep(rt, ctx, 0x08C70314u, {frame.at(0x10u), s2});
    ctx.gpr[2] = byte_u(rt, frame.at(0x10u));
    ctx.gpr[3] = word(rt, frame.at(0x14u));
    if (ctx.gpr[2] != 0u) {
        ctx.gpr[2] = word(rt, s2);
        ctx.gpr[3] = word(rt, ctx.gpr[2] - 0xCu);
        s0 = s2 + ctx.gpr[3];
        s1 = word(rt, s0 + 0x80u);
        if (s1 == 0u) {
            call_keep(rt, ctx, 0x08B5A9FCu, {});
            return;
        }
        s3 = word(rt, s0 + 0x78u);
        ctx.gpr[2] = byte_u(rt, s0 + 0x75u);
        u32 &empty = (extras == 1u) ? s4 : s6;
        empty = s3 < 1u ? 1u : 0u;
        if (ctx.gpr[2] == 0u) {
            ctx.gpr[4] = s0;
            ctx.gpr[5] = 0x20u;
            ctx.gpr[2] = call_keep(rt, ctx, 0x08C5477Cu, {s0, 0x20u});
            put8(rt, s0 + 0x74u, ctx.gpr[2]);
            ctx.gpr[2] = 1u;
            put8(rt, s0 + 0x75u, 1u);
        }
        ctx.gpr[3] = word(rt, s1);
        ctx.gpr[7] = sext8(byte_u(rt, s0 + 0x74u));
        put8(rt, frame.at(0xCu), empty);
        put(rt, frame.at(8u), s3);
        ctx.gpr[6] = s0;
        ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
        const GuestResult result =
            extras == 1u ? call_keep(rt, ctx, ctx.gpr[2], {s1, frame.at(8u), s0, ctx.gpr[7], s5}, {})
                         : call_keep(rt, ctx, ctx.gpr[2], {s1, frame.at(8u), s0, ctx.gpr[7], s4, s5}, {});
        put(rt, frame.at(0x24u), result.v1);
        ctx.gpr[3] = byte_u(rt, frame.at(0x24u));
        put(rt, frame.at(0u), result.v0);
        put(rt, frame.at(0x20u), result.v0);
        put8(rt, frame.at(4u), ctx.gpr[3]);
        if (ctx.gpr[3] != 0u) {
            ctx.gpr[2] = word(rt, s2);
            ctx.gpr[4] = s2 + word(rt, ctx.gpr[2] - 0xCu);
            ctx.gpr[5] = word(rt, ctx.gpr[4] + 0x14u);
            call_keep(rt, ctx, 0x08C54CFCu, {ctx.gpr[4], ctx.gpr[5] | 1u});
        }
        ctx.gpr[3] = word(rt, frame.at(0x14u));
    }
    ctx.gpr[2] = word(rt, ctx.gpr[3]);
    ctx.gpr[4] = word(rt, ctx.gpr[2] - 0xCu);
    ctx.gpr[3] += ctx.gpr[4];
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu) & 0x2000u;
    if (ctx.gpr[2] != 0u) {
        if (call_keep(rt, ctx, 0x08B62318u, {}) == 0u) {
            ctx.gpr[2] = word(rt, frame.at(0x14u));
            ctx.gpr[3] = word(rt, ctx.gpr[2]);
            ctx.gpr[4] = word(rt, ctx.gpr[3] - 0xCu);
            ctx.gpr[2] += ctx.gpr[4];
            ctx.gpr[4] = word(rt, ctx.gpr[2] + 0x78u);
            if (ctx.gpr[4] != 0u) {
                const u32 held = ctx.gpr[4];
                ctx.gpr[3] = word(rt, held);
                ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x18u);
                call_keep(rt, ctx, ctx.gpr[2], {held});
                ctx.gpr[3] = 0xFFFFFFFFu;
                if (ctx.gpr[2] == 0xFFFFFFFFu) {
                    ctx.gpr[4] = word(rt, frame.at(0x14u));
                    ctx.gpr[2] = word(rt, ctx.gpr[4]);
                    ctx.gpr[3] = word(rt, ctx.gpr[2] - 0xCu);
                    ctx.gpr[4] += ctx.gpr[3];
                    ctx.gpr[5] = word(rt, ctx.gpr[4] + 0x14u);
                    call_keep(rt, ctx, 0x08C54CFCu, {ctx.gpr[4], ctx.gpr[5] | 1u});
                }
            }
        }
    }
    ctx.gpr[2] = s2;
}

// The end of the stream operations: the guard object (word at `guard_slot`) with bit 0x2000 of its derived +0xC word set and
// 0x08B62318() zero asks its +0x78 object's virtual +0x18; -1 raises the flags of the guard's object again (0x08C54CFC). v0 = self.
void guard_tail(Runtime &rt, AllegrexContext &ctx, u32 guard_slot, u32 self) {
    ctx.gpr[3] = word(rt, guard_slot);
    ctx.gpr[2] = word(rt, ctx.gpr[3]);
    ctx.gpr[4] = word(rt, ctx.gpr[2] - 0xCu);
    ctx.gpr[3] += ctx.gpr[4];
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xCu) & 0x2000u;
    if (ctx.gpr[2] != 0u && call_keep(rt, ctx, 0x08B62318u, {}) == 0u) {
        ctx.gpr[2] = word(rt, guard_slot);
        ctx.gpr[3] = word(rt, ctx.gpr[2]);
        ctx.gpr[4] = word(rt, ctx.gpr[3] - 0xCu);
        ctx.gpr[2] += ctx.gpr[4];
        ctx.gpr[4] = word(rt, ctx.gpr[2] + 0x78u);
        if (ctx.gpr[4] != 0u) {
            const u32 held = ctx.gpr[4];
            ctx.gpr[3] = word(rt, held);
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x18u);
            call_keep(rt, ctx, ctx.gpr[2], {held});
            ctx.gpr[3] = 0xFFFFFFFFu;
            if (ctx.gpr[2] == 0xFFFFFFFFu) {
                ctx.gpr[4] = word(rt, guard_slot);
                ctx.gpr[2] = word(rt, ctx.gpr[4]);
                ctx.gpr[3] = word(rt, ctx.gpr[2] - 0xCu);
                ctx.gpr[4] += ctx.gpr[3];
                ctx.gpr[5] = word(rt, ctx.gpr[4] + 0x14u);
                call_keep(rt, ctx, 0x08C54CFCu, {ctx.gpr[4], ctx.gpr[5] | 1u});
            }
        }
    }
    ctx.gpr[2] = self;
}

// 0x08C7125C (a0 = this, a2 / a3 = two extra arguments): like the four-argument stream operations, with the guard at sp + 0x20 / +0x24.
// The derived object's flags & 0x4A choose the virtual: 0x40 or 0x08 slot 0x18 (record at sp + 0x18), else slot 0x14 (record at
// sp + 8); both get (host, record, derived, the byte at +0x74, a2, a3). A null host at +0x80 calls 0x08B5A9FC (no return).

// 0x08C78E68 (a0 = this, a1 = a registry whose table has a count at +8 and entries at +4): finds an entry by the index 0x08B5C2B0(0x08D721C8)
// (s3 = the entry, 0 when the first lookup misses; a miss of the second lookup raises 0x08B5A9FC). When 0x08B61534(this + 0x24) says so,
// the stream object at +0x90 is asked through its virtuals (+0x14, +0x18) and the entry through its +0x18; the paths end by storing
// s3 (or 0) into +0x90, resetting the read area (0x08C76B7C), filling it (the object's +0x1C virtual, a compaction through
// 0x08B79BAC and the 0x30 .. 0x44 words copied to +0x60 / +0x48) or setting it up (this' virtual +0x10 writing an eight-word descriptor
// at sp + 0x20, copied to sp). v0 is as the original leaves it on each path.

// Copies `count` words from `from` to `to`.
void copy_words_112(Runtime &rt, u32 to, u32 from, u32 count) {
    for (u32 i = 0u; i < count; ++i) put(rt, to + 4u * i, word(rt, from + 4u * i));
}

// 0x08B02938 (a0 = container {+4 end, +8 capacity end, +0xC selector half, +0x14 table owner, +0x1C two index halves, +0x20 count,
// +0x24 index owner, +0x2C list {+4 end, +8 capacity end}}, a1 = a 0xF0-byte record): inserts a copy of the record. Selector -1:
// appended at the end (0x08C48488 grows when full; a null end skips the copy); a zero element answers 0. Any other selector
// takes the slot selector * 0xF0 of the table (a negative one gives 0) through the iterator record built at sp (0x08C48120) and
// copies the record there. Then the element's half at +0xE8 gets the old half at +0x1C, +0x1C (and +0x1E when the old one was -1)
// the element's index in the index owner's block (-1 for a null element), the count at +0x20 grows, and a record whose half at
// +0xE2 has bit 7 clear appends its index (as a half) to the list at +0x2C. v0 = the element.
void func_08B02938(Runtime &rt, AllegrexContext &ctx) {
    const u32 container = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x90u);
    u32 &s0 = ctx.gpr[16], &s1 = ctx.gpr[17], &s4 = ctx.gpr[20], &s5 = ctx.gpr[21];
    s5 = container;
    s4 = value;
    ctx.gpr[8] = container + 0xCu;
    const u32 selector = sext16(half_u(rt, container + 0xCu));
    if (selector == 0xFFFFFFFFu) {
        u32 end = word(rt, container + 4u);
        ctx.gpr[2] = word(rt, container + 8u);
        if (end == ctx.gpr[2]) {
            ctx.gpr[5] = end;
            call_keep(rt, ctx, 0x08C48488u, {container, end, value});
            end = word(rt, container + 4u);
        } else {
            if (end != 0u) copy_words_112(rt, end, value, 0xF0u / 4u);
            end = word(rt, container + 4u) + 0xF0u;
            put(rt, container + 4u, end);
        }
        s0 = end - 0xF0u;
        if (s0 == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        ctx.gpr[2] = word(rt, container);
    } else {
        ctx.gpr[6] = selector;
        s1 = 0u;
        const u32 table = word(rt, container + 0x14u);
        if (signed_of(selector) >= 0) {
            ctx.gpr[4] = table;
            s1 = word(rt, table) + selector * 0xF0u;
        }
        ctx.gpr[7] = word(rt, ctx.gpr[8] + 8u);
        ctx.gpr[2] = word(rt, frame.at(0x44u));
        put(rt, frame.at(0x50u), 0u);
        s0 = s1;
        ctx.gpr[3] = 0u;
        put(rt, frame.at(4u), ctx.gpr[2]);
        put(rt, frame.at(0x4Cu), s1);
        put(rt, frame.at(0x48u), ctx.gpr[7]);
        put(rt, frame.at(8u), ctx.gpr[7]);
        put(rt, frame.at(0xCu), s1);
        put(rt, frame.at(0x10u), 0u);
        if (signed_of(selector) >= 0) ctx.gpr[3] = word(rt, ctx.gpr[7]) + selector * 0xF0u;
        ctx.gpr[2] = word(rt, frame.at(0x34u));
        put(rt, frame.at(0x14u), ctx.gpr[2]);
        put(rt, frame.at(0x18u), ctx.gpr[7]);
        put(rt, frame.at(0x1Cu), ctx.gpr[3]);
        put(rt, frame.at(0x3Cu), ctx.gpr[3]);
        put(rt, frame.at(0x38u), ctx.gpr[7]);
        put(rt, frame.at(0x40u), 0u);
        put(rt, frame.at(0x20u), 0u);
        call_set(rt, ctx, 0x08C48120u, {frame.at(0x60u), container + 0xCu, frame.at(0x14u)});
        ctx.gpr[7] = s1;
        put(rt, frame.at(0x24u), word(rt, frame.at(0x60u)));
        put(rt, frame.at(0x2Cu), word(rt, frame.at(0x68u)));
        put(rt, frame.at(0x28u), word(rt, frame.at(0x64u)));
        put(rt, frame.at(0x30u), word(rt, frame.at(0x6Cu)));
        copy_words_112(rt, s1, value, 0xF0u / 4u);
        ctx.gpr[2] = word(rt, container);
    }
    const u32 multiplier = 0xEEEEEEEFu;
    const u32 first_index = (static_cast<u32>(signed_of(s0 - ctx.gpr[2]) >> 4)) * multiplier;
    ctx.gpr[7] = first_index;
    put16(rt, s0 + 0xE8u, half_u(rt, container + 0x1Cu));
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (s0 != 0u) {
        ctx.gpr[3] = word(rt, container + 0x24u);
        ctx.gpr[2] = word(rt, ctx.gpr[3]);
        ctx.gpr[2] = sext16((static_cast<u32>(signed_of(s0 - ctx.gpr[2]) >> 4)) * multiplier);
    }
    const u32 indices = container + 0x1Cu;
    ctx.gpr[3] = sext16(half_u(rt, indices));
    ctx.gpr[5] = sext16(ctx.gpr[2]);
    if (ctx.gpr[3] == 0xFFFFFFFFu) put16(rt, indices + 2u, ctx.gpr[5]);
    ctx.gpr[3] = word(rt, indices + 4u);
    put16(rt, indices, ctx.gpr[5]);
    put(rt, indices + 4u, ctx.gpr[3] + 1u);
    ctx.gpr[2] = half_u(rt, value + 0xE2u) & 0x80u;
    if (ctx.gpr[2] == 0u) {
        const u32 list = container + 0x2Cu;
        ctx.gpr[5] = word(rt, list + 4u);
        ctx.gpr[2] = word(rt, list + 8u);
        ctx.gpr[3] = sext16(first_index);
        put16(rt, frame.at(0u), ctx.gpr[3]);
        if (ctx.gpr[5] == ctx.gpr[2]) {
            call_keep(rt, ctx, 0x08C47EA4u, {list, ctx.gpr[5], frame.at(0u)});
        } else if (ctx.gpr[5] == 0u) {
            put(rt, list + 4u, 2u);
        } else {
            put16(rt, ctx.gpr[5], ctx.gpr[3]);
            put(rt, list + 4u, word(rt, list + 4u) + 2u);
        }
    }
    ctx.gpr[2] = s0;
}

} // namespace
} // namespace new_batch_main_112
} // namespace gt::game

#define NEW112(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_112::name, gt::ReturnKind::kind)
NEW112(0x08AF5164u, func_08AF5164, Word);
NEW112(0x08B02938u, func_08B02938, Word);
NEW112(0x08B1B420u, func_08B1B420, Word);
NEW112(0x08B1F2B8u, func_08B1F2B8, Word);
NEW112(0x08B34B2Cu, func_08B34B2C, Word);
NEW112(0x08B48A74u, func_08B48A74, Word);
NEW112(0x08B73410u, func_08B73410, Word);
NEW112(0x08B7E014u, func_08B7E014, Word);
NEW112(0x08B7E564u, func_08B7E564, Word);
NEW112(0x08BE241Cu, func_08BE241C, Word);
NEW112(0x08BE6D44u, func_08BE6D44, Word);
NEW112(0x08BE834Cu, func_08BE834C, Word);
NEW112(0x08BF6664u, func_08BF6664, Word);
NEW112(0x08C1667Cu, func_08C1667C, Word);
NEW112(0x08C35F34u, func_08C35F34, Word);
NEW112(0x08C3D12Cu, func_08C3D12C, Word);
NEW112(0x08C4FD6Cu, func_08C4FD6C, Word);
