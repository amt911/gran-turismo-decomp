// New batch main-2086: 19 functions drafted again after the gate mismatched their earlier drafts (or they were skipped as too
// hard). Structured C from `tools.disasm`; each body was checked against the asm once more: delay slots before the call or
// branch, every argument register a callee reads, the callee-saved registers restored on every path, signed compares where
// the asm uses `slt` / `bgtz` / `div`, and v0 as the asm leaves it. No names: none has evidence.
//
// The holder constructors (3) differ only in the vtable they store. Their asm sets a1 / a2 / v1 / v0 before the 0x08A69D68
// call (a1 = the first word of the pair, a2 = 0x08B80000, v1 = 0x08B867AC, v0 = the second word); those are set here too.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2086 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// Records the callee-saved registers the body writes and puts the caller's values back when it ends (a C++ scope does
// what the original's prologue/epilogue pair does).
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx), count_(registers.size()) {
        unsigned index = 0u;
        for (const unsigned reg : registers) {
            registers_[index] = reg;
            saved_[index] = ctx.gpr[reg];
            ++index;
        }
    }
    ~SavedRegisters() {
        for (unsigned index = 0u; index < count_; ++index) ctx_.gpr[registers_[index]] = saved_[index];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned count_;
    std::array<unsigned, 10> registers_{};
    std::array<u32, 10> saved_{};
};

// A guest call that leaves v0 / v1 / f0 in the context like the original's `jal`.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, target, arguments, {});
}
// The argument registers set as the original sets them before the jal; the ones the call leaves out keep the caller's value.
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments);
}
u32 call_set_value(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_set(rt, ctx, target, arguments).v0;
}
// A call that only needs v0 back (the arguments are the ones listed).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, target, arguments).v0;
}
// A virtual call as the compiler issues it: v1 = the vtable, v0 = the slot's function, then `jalr v0`.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments);
}
// `lw v1, 0(object); lw v0, offset(v1); jalr v0` with a null check site that asserts first.
GuestResult call_virtual_checked(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, u32 site,
                                 std::initializer_list<u32> arguments) {
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {site});
    return call_virtual(rt, ctx, object, offset, arguments);
}

u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
bool signed_less(u32 left, u32 right) { return static_cast<s32>(left) < static_cast<s32>(right); }
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

// The reference-count release the string helpers use: a temporary that is not the shared empty 0x08D74FD0 is passed to
// 0x08B60804(&temporary - 4, -1) and, when that answers not-positive, to 0x08B8542C(&temporary - 0xC, scratch).
void release_temporary(Runtime &rt, AllegrexContext &ctx, u32 text, u32 scratch) {
    constexpr u32 kEmpty = 0x08D74FD0u;
    const u32 object = text - 0xCu;
    ctx.gpr[2] = kEmpty;
    if (object == kEmpty) return;
    if (call_set_value(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}) <= 0) {
        call_set(rt, ctx, 0x08B8542Cu, {object, scratch});
    }
}

// --- holder constructors (3 functions) ------------------------------------------------------------------------------------
// a0 = self, a1 = text, a2 = a pair of words, a3 / t0 = two more pairs. 0x08A5BC20(0x2C, 4) allocates the holder,
// 0x08A98220(holder, &temp) initialises it, the vtable goes in word 0, a2[0] -> +0x14, a2[1] -> +0x18, a3[1] -> +0x20,
// a3[0] -> +0x1C, t0[1] -> +0x28, t0[0] -> +0x24; 0x08A69D68(&temp) finishes the temporary, the holder is stored into the
// caller's slot (a stack word) by 0x08A5D414(&slot, &temp), 0x08A7C5C8(self, &slot) takes it and 0x08A5B9EC(&slot) releases
// it. Before 0x08A69D68 the asm leaves a1 = a2[0], a2 = 0x08B80000, v1 = 0x08B867AC and v0 = a2[1].
void make_holder(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 pair = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 30u});
    GuestFrame frame(ctx, 0x50u);
    const u32 temp = frame.at(0x8u);
    const u32 slot = frame.at(0x4u);
    put(rt, frame.at(0x10u), self);  // the asm spills a0 here and reloads it for the last call
    const u32 first = word(rt, pair);
    const u32 second = word(rt, pair + 4u);
    const u32 third_first = word(rt, third);
    const u32 third_second = word(rt, third + 4u);
    const u32 fourth_first = word(rt, fourth);
    const u32 fourth_second = word(rt, fourth + 4u);
    ctx.gpr[16] = temp;
    ctx.gpr[18] = first;
    ctx.gpr[19] = second;
    ctx.gpr[21] = third_second;
    ctx.gpr[22] = fourth_second;
    ctx.gpr[23] = third_first;
    ctx.gpr[30] = fourth_first;
    call_set(rt, ctx, 0x08A69B3Cu, {temp, text});
    const u32 object = call_set_value(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    ctx.gpr[17] = object;
    call_set(rt, ctx, 0x08A98220u, {object, temp});
    put(rt, object, vtable);
    put(rt, object + 0x14u, first);
    put(rt, object + 0x18u, second);
    put(rt, object + 0x20u, third_second);
    put(rt, object + 0x1Cu, third_first);
    put(rt, object + 0x24u, fourth_first);
    put(rt, object + 0x28u, fourth_second);
    ctx.gpr[5] = first;        // a1 = s2 (set by the asm before the branch, read by the callee)
    ctx.gpr[6] = 0x08B80000u;  // a2 = the lui half of 0x08B867AC
    ctx.gpr[3] = 0x08B867ACu;  // v1 = a2 + 0x27AC
    ctx.gpr[2] = second;       // v0 = s3 on every path of the branch
    call_set(rt, ctx, 0x08A69D68u, {temp});
    put(rt, frame.at(0u), object);
    call_set(rt, ctx, 0x08A5D414u, {slot, frame.at(0u)});
    call_set(rt, ctx, 0x08A7C5C8u, {word(rt, frame.at(0x10u)), slot});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
}

// --- 0x08B945C4 -------------------------------------------------------------------------------------------------------------
// 0x08B945C4 (a0 = a string holder, a1 = a source begin, a2 = a count): assigns the count bytes at `begin` to the
// string. The count above 0x3FFFFFFC asserts (0x08B5ADDC). When the source is outside the string's own buffer or the
// buffer reports spare room at word(data - 4) (a signed test), 0x08B94540(self, 0, word(data - 0xC), begin, count) takes
// over (tail call). Otherwise the bytes are copied (0x08B79A88, or 0x08B79BAC when the source overlaps and the count is
// above one, or one byte inline), word(data - 4) = 0, word(data - 0xC) = count, byte(data + count) = the global terminator
// at 0x08CBF760, and v0 = self.
void func_08B945C4(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kLimit = 0x3FFFFFFCu;
    constexpr u32 kTerminator = 0x08CBF760u;
    const u32 self = ctx.gpr[4];
    const u32 begin = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    SavedRegisters saved(ctx, {16u, 17u});
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = count;
    const u32 data = word(rt, self);
    const u32 length = word(rt, data - 0xCu);
    if (count > kLimit) call_set(rt, ctx, 0x08B5ADDCu, {0x08C9F6DCu});
    if (begin < data || data + length < begin || static_cast<s32>(word(rt, data - 4u)) > 0) {
        tail_call(ctx, 0x08B94540u, {self, 0u, length, begin, count});
        return;
    }
    const u32 offset = begin - data;
    if (offset >= count) {
        if (count == 1u) {
            put8(rt, data, byte_u(rt, begin));
        } else {
            call_set(rt, ctx, 0x08B79A88u, {data, begin, count});
        }
    } else if (offset != 0u) {
        if (count == 1u) {
            put8(rt, data, byte_u(rt, begin));
        } else {
            call_set(rt, ctx, 0x08B79BACu, {data, begin, count});
        }
    }
    put(rt, data - 4u, 0u);
    const u32 end = data + count;
    put(rt, data - 0xCu, count);
    const u32 terminator = byte_u(rt, kTerminator);
    put8(rt, end, terminator);
    ctx.gpr[2] = self;
    ctx.gpr[3] = end;
    ctx.gpr[4] = 0x08CC0000u;
    ctx.gpr[5] = terminator;
}

// --- 0x08B5xxxx / 0x08A3B9E8 ------------------------------------------------------------------------------------------------
// 0x08A3B9E8 (a1 = passed on, a2 = a count, a3 = a pair of holders): a count of zero or less returns with v0 untouched.
// The float argument starts at 3.0f and with two or more units becomes the +0x54 float of word(a3 + 4). word(a3)'s +0xC
// virtual fills a stack temporary, 0x08A3A9D0(&holder, a1) answers an object that 0x08A3A3E4 receives with the temporary's
// word and the float, and both temporaries are released.
void func_08A3B9E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 argument = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 holders = ctx.gpr[7];
    SavedRegisters saved(ctx, {16u, 17u, 18u});
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[18] = argument;
    ctx.gpr[17] = holders;
    if (!signed_less(0u, count)) return;
    float value = 3.0f;
    if (!signed_less(count, 2u)) {
        const u32 upper = word(rt, holders + 4u);
        value = call_virtual_checked(rt, ctx, upper, 0x54u, 0x08CAD518u, {upper}).f0;
    }
    const u32 object = word(rt, holders);
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAD518u});
    call_virtual(rt, ctx, object, 0xCu, {frame.at(0x8u), object});
    call_set(rt, ctx, 0x08A3A9D0u, {frame.at(0x4u), argument});
    const u32 holder = word(rt, frame.at(0x4u));
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAD544u});
    ctx.fpr[12] = value;
    call_set(rt, ctx, 0x08A3A3E4u, {holder, word(rt, frame.at(0x8u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
    release_temporary(rt, ctx, word(rt, frame.at(0x8u)), frame.at(0u));
}

// --- 0x08B2EEB8 -------------------------------------------------------------------------------------------------------------
// 0x08B2EEB8 (a0 = a state object): three states. Word 0 is 1: word(this + 8) is turned into a path in a 0x100-byte stack
// buffer (0x08B23928 with 0x08CB5488, 0x08B23A70 with word(this + 8), 0x08B1CD8C with 0x08C93A74, then 0x08B0EB48 copies
// it to sp), the path is opened by 0x08C7A900(sp, 0, 0) - failures retry after 0x08B2E064(failure) - the handle goes to
// this + 0x10 (0x08C7A8E8 with zeros), byte(this + 4) = 1 and v0 = that call's answer; word(this + 8) == 0 returns 0.
// Word 0 is 2: 0x08C7B168(word(this + 0xC)) answers a code - 0x80111102 returns 0x80020000, 0x80020139 returns itself, a
// non-negative code sets byte(this + 4) = 1 and returns 0x80020139. Any other state returns 2.
void func_08B2EEB8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16u, 17u});
    GuestFrame frame(ctx, 0x220u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = 1u;
    const u32 state = word(rt, self);
    if (state == 2u) {
        const u32 code = call_set_value(rt, ctx, 0x08C7B168u, {word(rt, self + 0xCu)});
        if (code == 0x80111102u) {
            ctx.gpr[2] = 0x80020000u;
            return;
        }
        ctx.gpr[2] = 0x80020139u;
        if (code == 0x80020139u) return;
        if (static_cast<s32>(code) >= 0) put8(rt, self + 4u, 1u);
        return;
    }
    if (state != 1u) {
        ctx.gpr[2] = 2u;
        return;
    }
    if (word(rt, self + 8u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 builder = frame.at(0x100u);
    put(rt, builder + 4u, 0x100u);
    put(rt, builder, frame.at(0x10Cu));
    put(rt, builder + 8u, 0u);
    put8(rt, frame.at(0x10Cu), 0u);
    call_set(rt, ctx, 0x08B23928u, {builder, 0x08CB5488u});
    call_set(rt, ctx, 0x08B23A70u, {builder, word(rt, self + 8u)});
    call_set(rt, ctx, 0x08B1CD8Cu, {builder, 0x08C93A74u});
    call_set(rt, ctx, 0x08B0EB48u, {word(rt, builder), frame.at(0u), 0x100u});
    u32 handle;
    for (;;) {
        const u32 opened = call_set_value(rt, ctx, 0x08C7A900u, {frame.at(0u), 0u, 0u});
        if (signed_less(0u, opened)) {
            handle = opened;
            break;
        }
        call_set(rt, ctx, 0x08B2E064u, {opened});
    }
    put(rt, self + 0x10u, handle);
    call_set(rt, ctx, 0x08C7A8E8u, {handle, 0u, 0u, 0u, 0u});
    put8(rt, self + 4u, 1u);
}

// --- 0x08C7240C -------------------------------------------------------------------------------------------------------------
// 0x08C7240C (a0 = this, a1 = passed to the virtual, a2 = a key): 0x08C70314(&probe, this, key) settles whether a lookup
// happened. With byte(probe) set, the object at word(base + 0x78) (base = this + word(word(this) - 0xC), the
// this-adjustment) is asked through its +0x30 virtual (a1, key); when the answer differs from key the adjustment's word
// +0x14 with bit 0 set goes to 0x08C54CFC. The rest asks word(probe + 4): unless its +0xC word has bit 0x2000 the function
// returns; otherwise 0x08B62318 must answer zero, word(base + 0x78) is asked through +0x18 and a -1 answer makes
// 0x08C54CFC set the adjustment's +0x14 bit 0. v0 = this on every path.
void func_08C7240C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    SavedRegisters saved(ctx, {16u, 17u, 18u});
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = argument;
    ctx.gpr[18] = key;
    call_set(rt, ctx, 0x08C70314u, {frame.at(0u), self, key});
    if (byte_u(rt, frame.at(0u)) != 0u) {
        const u32 holder = word(rt, self);
        const u32 base = self + word(rt, holder - 0xCu);
        const u32 target = word(rt, base + 0x78u);
        const u32 answer = call_virtual(rt, ctx, target, 0x30u, {target, argument, key}).v0;
        if (answer != key) {
            const u32 holder2 = word(rt, self);
            const u32 base2 = self + word(rt, holder2 - 0xCu);
            call_set(rt, ctx, 0x08C54CFCu, {base2, word(rt, base2 + 0x14u) | 1u});
        }
    }
    const u32 probe = word(rt, frame.at(0x4u));
    const u32 probe_base = probe + word(rt, word(rt, probe) - 0xCu);
    if ((word(rt, probe_base + 0xCu) & 0x2000u) != 0u && call_set_value(rt, ctx, 0x08B62318u, {}) == 0u) {
        const u32 again = word(rt, frame.at(0x4u));
        const u32 again_base = again + word(rt, word(rt, again) - 0xCu);
        const u32 target = word(rt, again_base + 0x78u);
        if (target != 0u && call_virtual(rt, ctx, target, 0x18u, {target}).v0 == 0xFFFFFFFFu) {
            const u32 last = word(rt, frame.at(0x4u));
            const u32 last_base = last + word(rt, word(rt, last) - 0xCu);
            call_set(rt, ctx, 0x08C54CFCu, {last_base, word(rt, last_base + 0x14u) | 1u});
        }
    }
    ctx.gpr[2] = self;
}

// --- 0x089A5DD0 -------------------------------------------------------------------------------------------------------------
// 0x089A5DD0 (a0 = this): 0x089A2528(this) first; the vtables 0x08CBAD20 (word 0) and 0x08CBAF78 (+0x38) are stored; the
// name 0x08CA4F10 is built into this + 0x128 (0x08B85EB8 with a stack word); the flag word at +0x138 gets bits 4 and 8
// set and bits 0..3, 5, 6 and 9 cleared, with bit 7 taken from byte 0x08CF38F0; this + 0x12C / +0x130 point at the empty
// string 0x08D74FDC; 0x089F09DC(this + 0x150, 0x08CF6ED8, that byte, this + 0x13C) runs; two small lists are linked at
// +0x154 and +0x16C; word(this + 0x110) gets bit 0 and 0x0896A824(this, 0) runs; word(this + 0x60) gets bit 3 and is the
// answer.
void func_089A5DD0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16u});
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x089A2528u, {self});
    put(rt, self, 0x08CBAD20u);
    put(rt, self + 0x38u, 0x08CBAF78u);
    put(rt, self + 0x120u, 0u);
    put(rt, self + 0x124u, 0u);
    call_set(rt, ctx, 0x08B85EB8u, {self + 0x128u, 0x08CA4F10u, frame.at(0u)});
    const u32 mode = ((word(rt, self + 0x138u) & ~0xFu) | 0x10u) & ~0x60u;
    put(rt, self + 0x138u, mode);
    const u32 empty = 0x08D74FDCu;
    put(rt, self + 0x130u, empty);
    put(rt, self + 0x12Cu, empty);
    const u32 flag = byte_u(rt, 0x08CF38F0u);
    const u32 block = self + 0x13Cu;
    put(rt, self + 0x134u, 0u);
    put(rt, self + 0x13Cu, block);
    put(rt, self + 0x138u, (insert_bits(mode, flag, 7u, 1u) | 0x100u) & ~0x200u);
    put(rt, self + 0x140u, block);
    put(rt, self + 0x144u, 0u);
    put(rt, self + 0x148u, 0u);
    put(rt, self + 0x14Cu, 0u);
    call_set(rt, ctx, 0x089F09DCu, {self + 0x150u, 0x08CF6ED8u, flag, block});
    const u32 list = self + 0x154u;
    const u32 list_end = self + 0x158u;
    const u32 other = self + 0x16Cu;
    const u32 other_end = self + 0x170u;
    put(rt, list + 0x14u, 0u);
    put(rt, list + 0x10u, list_end);
    put(rt, list + 0x4u, 0u);
    put(rt, list + 0x8u, 0u);
    put(rt, list + 0xCu, list_end);
    put(rt, other + 0x10u, other_end);
    put(rt, other + 0x4u, 0u);
    put(rt, other + 0x14u, 0u);
    put(rt, other + 0x8u, 0u);
    put(rt, other + 0xCu, other_end);
    put(rt, self + 0x110u, word(rt, self + 0x110u) | 1u);
    call_set(rt, ctx, 0x0896A824u, {self, 0u});
    const u32 result = word(rt, self + 0x60u) | 8u;
    put(rt, self + 0x60u, result);
    ctx.gpr[2] = result;
}

// --- 0x08AEEFD4 -------------------------------------------------------------------------------------------------------------
// 0x08AEEFD4 (a0 = the target word, a1 = whether to wait): takes the 0x08D41AD0 lock (0x08C7AA20), scans up to
// word(+0x510) entries of the table (word(+0x500) + (((word(+0x50C) + index) as signed) mod word(+0x504)) * 32) for the
// target word, and releases it (0x08C7AA08). v0 = 1 when the target was found or word(+0x514) holds it; with a nonzero a1
// it then signals 0x08C7AA10(word(+0x4FC), 1 << target, 0x20, 0, 0) and starts over, otherwise it returns that answer.
// The modulo is the signed `div` remainder of the asm. A zero divisor traps in the original; it is treated as not found.
void func_08AEEFD4(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kState = 0x08D41AD0u;
    const u32 target = ctx.gpr[4];
    const bool wait = ctx.gpr[5] != 0u;
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u});
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[22] = 0x00060000u;
    ctx.gpr[20] = kState;
    ctx.gpr[23] = 1u;
    ctx.gpr[21] = wait ? 1u : 0u;
    ctx.gpr[19] = kState;
    ctx.gpr[16] = target;
    for (;;) {
        call_set(rt, ctx, 0x08C7AA20u, {word(rt, kState + 0x4F8u), 1u, 0u});
        u32 found = 0u;
        const u32 limit = word(rt, kState + 0x510u);
        if (signed_less(0u, limit)) {
            const u32 base = word(rt, kState + 0x50Cu);
            const s32 divisor = static_cast<s32>(word(rt, kState + 0x504u));
            const u32 table = word(rt, kState + 0x500u);
            u32 index = 0u;
            do {
                const s32 position = static_cast<s32>(base + index);
                s32 remainder = 0;
                if (divisor != 0 && divisor != -1) remainder = position % divisor;
                if (divisor == 0) break;
                const u32 slot = table + (static_cast<u32>(remainder) << 5);
                ++index;
                if (word(rt, slot) == target) {
                    found = 1u;
                    break;
                }
            } while (signed_less(index, limit));
        }
        const u32 answer = word(rt, kState + 0x514u) == target ? 1u : found;
        ctx.gpr[18] = answer;
        call_set(rt, ctx, 0x08C7AA08u, {word(rt, kState + 0x4F8u), 1u});
        if (answer != 0u && wait) {
            call_set(rt, ctx, 0x08C7AA10u, {word(rt, kState + 0x4FCu), 1u << (target & 31u), 0x20u, 0u, 0u});
            continue;
        }
        ctx.gpr[2] = answer;
        return;
    }
}

// --- 0x08A510B4 -------------------------------------------------------------------------------------------------------------
// 0x08A510B4 (a0 = a hash table, a1 = a value record): probes the 16-bit key table at word(this + 0x10) from word(this + 0x20)
// (the count), stepping forward and wrapping from capacity - 1 to 0, and stops at the slot before the count (a signed
// compare picks it) or at an empty slot. A match answers word(this + 0xC) + index * lhu(this + 0x1E); otherwise the count
// becomes (count - 1) or capacity - 1 when it is zero, the block at word(this + 0xC) + count * element size is zeroed
// (0x08B36018), the key goes into the table at the new count, byte(source + 4) * 2 into word(this + 0x28) at the new count
// and byte(this + 0x5D) = 1; v0 = the block.
void func_08A510B4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx, {16u, 17u, 18u});
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = source;
    ctx.gpr[17] = self;
    const u32 capacity = half_u(rt, self + 0x1Cu);
    const u32 count = word(rt, self + 0x20u);
    const u32 keys = word(rt, self + 0x10u);
    const u32 key = half_u(rt, source);
    const u32 first = count + capacity - 1u;
    const u32 previous = signed_less(first, capacity) ? first : count - 1u;
    const u32 last_index = capacity - 1u;
    u32 index = count;
    ctx.gpr[16] = index;
    for (;;) {
        const u32 candidate = half_u(rt, keys + index * 2u);
        if (candidate == key) {
            ctx.gpr[2] = word(rt, self + 0xCu) + index * half_u(rt, self + 0x1Eu);
            return;
        }
        if (candidate == 0u || previous == index) break;
        index = index == last_index ? 0u : index + 1u;
        ctx.gpr[16] = index;
    }
    const u32 next_count = count != 0u ? count - 1u : capacity - 1u;
    put(rt, self + 0x20u, next_count);
    const u32 size = half_u(rt, self + 0x1Eu);
    const u32 block = word(rt, self + 0xCu) + next_count * size;
    ctx.gpr[16] = block;
    call_set(rt, ctx, 0x08B36018u, {block, 0u, size});
    put_half(rt, word(rt, self + 0x10u) + word(rt, self + 0x20u) * 2u, half_u(rt, source));
    put8(rt, word(rt, self + 0x28u) + word(rt, self + 0x20u), byte_u(rt, source + 4u) * 2u);
    put8(rt, self + 0x5Du, 1u);
    ctx.gpr[2] = block;
}

// --- 0x08C2EA68 -------------------------------------------------------------------------------------------------------------
// 0x08C2EA68 (a0 = a slot array, a1 = its size, a2 = a begin index, a3 = a holder, t0 = a comparator): a binary search.
// The middle probe is array + middle * 4 (middle starts at (size - 1) / 2) and the comparator's +8 virtual receives it
// with the holder: a zero answer takes the bound array + high * 4 as the answer slot, a nonzero one copies the probe into
// the bound (0x08A5BAD8) when they differ and halves again while begin < the new high. When the holder is not the answer
// slot, 0x08A5BAD8(slot, word(holder)) takes over (tail call).
void func_08C2EA68(Runtime &rt, AllegrexContext &ctx) {
    const u32 array = ctx.gpr[4];
    const u32 size = ctx.gpr[5];
    const u32 begin = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    const u32 comparator = ctx.gpr[8];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u});
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = array;
    ctx.gpr[22] = begin;
    ctx.gpr[21] = comparator;
    ctx.gpr[20] = holder;
    u32 middle = static_cast<u32>((static_cast<s32>(size) - 1) / 2);
    u32 high = size;
    u32 slot = array + size * 4u;
    ctx.gpr[16] = slot;
    u32 result = (size - 1u) + ((size - 1u) >> 31);
    if (signed_less(begin, size)) {
        for (;;) {
            slot = array + middle * 4u;
            ctx.gpr[16] = slot;
            ctx.gpr[18] = middle;
            ctx.gpr[17] = high;
            const u32 probe = call_virtual(rt, ctx, comparator, 8u, {comparator, slot, holder}).v0;
            const u32 bound = array + high * 4u;
            if (probe == 0u) {
                slot = bound;
                result = 0u;
                break;
            }
            if (bound != slot) call_set(rt, ctx, 0x08A5BAD8u, {bound, word(rt, slot)});
            high = middle;
            middle = static_cast<u32>((static_cast<s32>(middle) - 1) / 2);
            result = middle;
            if (!signed_less(begin, high)) break;
        }
    }
    ctx.gpr[18] = middle;
    ctx.gpr[17] = high;
    if (holder != slot) {
        tail_call(ctx, 0x08A5BAD8u, {slot, word(rt, holder)});
        return;
    }
    ctx.gpr[2] = result;
}

// --- 0x08B65CB4 / 0x08B65DE0 ------------------------------------------------------------------------------------------------
// 0x08B65CB4 / 0x08B65DE0 (a0:a1 = a 64-bit double, result in v0:v1): reduce it. The magnitude word (a1 without its sign)
// below 0x3FE921FB goes straight to one of the two kernels; above 0x7FEFFFFF it is 0x08B712B0's job. Otherwise
// 0x08B6975C(a0, a1, &quadrant) picks one of the four kernels from its word & 3, the flipped one xors the high word with
// 0x80000000. The two functions differ in which kernel each quadrant takes (sin vs cos), so which one goes first.
GuestResult reduce_quadrant(Runtime &rt, AllegrexContext &ctx, bool swapped) {
    const u32 low = ctx.gpr[4];
    const u32 high = ctx.gpr[5];
    const u32 magnitude = high & 0x7FFFFFFFu;
    GuestFrame frame(ctx, 0x20u);
    if (!signed_less(0x3FE921FBu, magnitude)) {
        if (swapped) return call_set(rt, ctx, 0x08B6D284u, {low, high, 0u, 0u, 0u});
        return call_set(rt, ctx, 0x08B6C158u, {low, high, 0u, 0u});
    }
    if (signed_less(0x7FEFFFFFu, magnitude)) return call_set(rt, ctx, 0x08B712B0u, {low, high, low, high});
    call_set(rt, ctx, 0x08B6975Cu, {low, high, frame.at(0u)});
    const u32 kind = ctx.gpr[2] & 3u;
    const u32 a = word(rt, frame.at(0u));
    const u32 b = word(rt, frame.at(4u));
    const u32 c = word(rt, frame.at(8u));
    const u32 d = word(rt, frame.at(0xCu));
    GuestResult result{};
    if (!swapped) {
        if (kind == 0u) {
            result = call_set(rt, ctx, 0x08B6C158u, {a, b, c, d});
        } else if (kind == 1u) {
            result = call_set(rt, ctx, 0x08B6D284u, {a, b, c, d, 1u});
            result.v1 ^= 0x80000000u;
        } else if (kind == 2u) {
            result = call_set(rt, ctx, 0x08B6C158u, {a, b, c, d});
            result.v1 ^= 0x80000000u;
        } else {
            result = call_set(rt, ctx, 0x08B6D284u, {a, b, c, d, 1u});
        }
    } else {
        if (kind == 0u) {
            result = call_set(rt, ctx, 0x08B6D284u, {a, b, c, d, 1u});
        } else if (kind == 1u) {
            result = call_set(rt, ctx, 0x08B6C158u, {a, b, c, d});
        } else if (kind == 2u) {
            result = call_set(rt, ctx, 0x08B6D284u, {a, b, c, d, 1u});
            result.v1 ^= 0x80000000u;
        } else {
            result = call_set(rt, ctx, 0x08B6C158u, {a, b, c, d});
            result.v1 ^= 0x80000000u;
        }
    }
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    return result;
}
void func_08B65CB4(Runtime &rt, AllegrexContext &ctx) { reduce_quadrant(rt, ctx, false); }
void func_08B65DE0(Runtime &rt, AllegrexContext &ctx) { reduce_quadrant(rt, ctx, true); }

// --- 0x08805578 -------------------------------------------------------------------------------------------------------------
// 0x08805578: 0x08A9DD14(&temporary) takes the item at the global 0x08CE28A0 into a temporary that is stored there and
// released. 0x08B36278 answers a count and 0x08B36284 a word array: for each of them a name is built from the array word
// (0x08B85EB8 into a stack temporary) and given to the global's item (0x08A9C650), then released. 0x08A5BCF4 ends it.

// --- 0x08818680 -------------------------------------------------------------------------------------------------------------
// 0x08818680 (a0 = a holder, a1 = a mode, a2 = a triple of holders): a mode below 3 makes a default-named empty holder
// (0x08A76AAC with 0x08CF6ED8) that this receives. Otherwise the +0x3C virtual of each of the three objects answers, and
// 0x08B1B2F4(second, third, first) then 0x08A5FA8C(&temporary, answer) build the result, which this receives and the
// temporary is released.
void func_08818680(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 mode = ctx.gpr[5];
    const u32 holders = ctx.gpr[6];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u});
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = out;
    ctx.gpr[17] = holders;
    if (signed_less(mode, 3u)) {
        call_set(rt, ctx, 0x08A76AACu, {frame.at(0u), 0x08CF6ED8u});
        if (out != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        return;
    }
    const u32 first_object = word(rt, holders);
    const u32 first = call_virtual_checked(rt, ctx, first_object, 0x3Cu, 0x08C98D34u, {first_object}).v0;
    ctx.gpr[20] = first;
    const u32 second_object = word(rt, holders + 4u);
    const u32 second = call_virtual_checked(rt, ctx, second_object, 0x3Cu, 0x08C98D34u, {second_object}).v0;
    ctx.gpr[18] = second;
    const u32 third_object = word(rt, holders + 8u);
    const u32 third = call_virtual_checked(rt, ctx, third_object, 0x3Cu, 0x08C98D34u, {third_object}).v0;
    call_set(rt, ctx, 0x08B1B2F4u, {first, second, third});
    call_set(rt, ctx, 0x08A5FA8Cu, {frame.at(0x4u), ctx.gpr[2]});
    if (out != frame.at(0x4u)) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x4u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
}

// --- 0x08A1C5CC -------------------------------------------------------------------------------------------------------------
// 0x08A1C5CC (a0 = this): a name is built on the stack from the literal 0x08CAA224 (0x08B85EB8 with a flag byte), the
// object's vtable word +0x108 handles (this, name) and the temporary is released. 0x08A76A94 answers a value that
// 0x08A5BA08 retains; the old word(this + 0x38) is released (0x08A5B930) and the new one stored. 0x08B8850C(this,
// 0x08CF6F14, 0x08A1CA40) and two 0x08B87108 registrations (0x08CAA278 / 0x08A1C704, 0x08CAA28C / 0x08A1C3B0) follow, and
// 0x08B8D51C(this, 0x08CAA2A0, 0x08A1C408, 0).

// --- 0x08AD31E0 -------------------------------------------------------------------------------------------------------------
// 0x08AD31E0 (a0 = this): 0x08AD16C0(this) and 0x08AD1098(this + 0x1D0) reset the members; the float 50.0 goes to
// this + 0x260, 1.0 to +0x250..+0x268, the byte 1 to this + 0x27A and zeros to this + 0x26C..+0x279, then
// 0x08AD0FAC(this + 0x27C). 0x08B62344(0xA0) allocates, 0x08AD6AB8(allocation, 0x80, 0x18) sets it up and it goes to
// this + 0x29C with the table 0x08D77268 at + 4 (the first use stores 0x08CD6CF0 there under 0x08B6441C/0x08B64494).
// 0x08C3DECC(this + 0x2A4), 0x08B28DCC(this + 0x40B0) and the float 1.0 at this + 0x240 finish; v0 is the last call's.
void func_08AD31E0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    constexpr u32 kTable = 0x08D77268u;
    constexpr u32 kFlag = 0x08D77260u;
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u});
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 member = self + 0x29Cu;
    ctx.gpr[19] = member;
    call_set(rt, ctx, 0x08AD16C0u, {self});
    call_set(rt, ctx, 0x08AD1098u, {self + 0x1D0u});
    put_float(rt, self + 0x260u, 50.0f);
    put_float(rt, self + 0x268u, 1.0f);
    put_float(rt, self + 0x250u, 1.0f);
    put_float(rt, self + 0x254u, 1.0f);
    put_float(rt, self + 0x258u, 1.0f);
    put_float(rt, self + 0x25Cu, 1.0f);
    put_float(rt, self + 0x264u, 1.0f);
    rt.memory().store8(self + 0x27Au, 1u);
    put(rt, self + 0x26Cu, 0u);
    rt.memory().store16(self + 0x270u, 0u);
    rt.memory().store16(self + 0x272u, 0u);
    rt.memory().store16(self + 0x274u, 0u);
    rt.memory().store16(self + 0x276u, 0u);
    rt.memory().store8(self + 0x278u, 0u);
    rt.memory().store8(self + 0x279u, 0u);
    call_set(rt, ctx, 0x08AD0FACu, {self + 0x27Cu});
    const u32 allocation = call_set_value(rt, ctx, 0x08B62344u, {0xA0u});
    ctx.gpr[16] = allocation;
    call_set(rt, ctx, 0x08AD6AB8u, {allocation, 0x80u, 0x18u});
    put(rt, member, allocation);
    if (static_cast<std::int8_t>(byte_u(rt, kFlag)) == 0) {
        if (call_set_value(rt, ctx, 0x08B6441Cu, {kFlag}) != 0u) {
            put(rt, kTable, 0x08CD6CF0u);
            call_set(rt, ctx, 0x08B64494u, {kFlag});
        }
    }
    ctx.gpr[18] = kTable;
    put(rt, member + 4u, kTable);
    call_set(rt, ctx, 0x08C3DECCu, {self + 0x2A4u});
    call_set(rt, ctx, 0x08B28DCCu, {self + 0x40B0u});
    put_float(rt, self + 0x240u, 1.0f);
}

// --- 0x08A74B34 -------------------------------------------------------------------------------------------------------------
// 0x08A74B34 (a0 = an object, a1 = a formatter state): builds the string at sp + 4 with 0x08AAB5DC(sp + 4, a1) and
// 0x08AABA8C(sp + 4, 0x08CB0C94). The word at a1 + 0x14 is resolved (a zero one is reported through
// 0x08A5BB7C(0x08CB0CA4) first) to its word + 0x14, which 0x08A7BDB0 turns into text; the pieces are appended with
// 0x08AABA8C, 0x08A5D454(a1) and 0x08AABA8C, and 0x08B88F44(a0, 0x08AAB540(sp + 4)) finishes. The word at sp + 4, unless
// it is the empty-string marker 0x08D74FD0, is released with 0x08B60804(word - 4, -1) and, when that answers <= 0,
// 0x08B8542C(word - 0xC, sp). Answers a0.
void func_08A74B34(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u});
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 state = ctx.gpr[5];
    ctx.gpr[20] = self;
    ctx.gpr[18] = state;
    const u32 buffer = frame.at(4u);
    ctx.gpr[19] = buffer;
    call(rt, ctx, 0x08AAB5DCu, {buffer, state});
    const u32 text = call(rt, ctx, 0x08AABA8Cu, {buffer, 0x08CB0C94u});
    const u32 held = word(rt, state + 0x14u);
    if (held == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0CA4u});
    u32 object = word(rt, held + 0x14u);
    if (object != 0u && word(rt, object + 4u) == 0u) object = 0u;
    const u32 first = call(rt, ctx, 0x08A7BDB0u, {object});
    const u32 second = call(rt, ctx, 0x08AABA8Cu, {text, first});
    const u32 third = call(rt, ctx, 0x08AABA8Cu, {second, 0x08CB0CACu});
    const u32 fourth = call(rt, ctx, 0x08A5D454u, {state});
    const u32 fifth = call(rt, ctx, 0x08AABA8Cu, {third, fourth});
    call(rt, ctx, 0x08AABA8Cu, {fifth, 0x08CB0CB0u});
    const u32 seventh = call(rt, ctx, 0x08AAB540u, {buffer});
    call(rt, ctx, 0x08B88F44u, {self, seventh});
    const u32 head = word(rt, frame.at(4u));
    if (head - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {head - 0xCu, frame.at(0u)});
    }
    ctx.gpr[2] = self;
}

// --- 0x089396F8 -------------------------------------------------------------------------------------------------------------
// 0x089396F8 (a0 = a reader, a1 = a packed bit source, a2 = the byte destination): the top two bits of the byte at a1
// pick the path. Kind 1 ends in a jump to 0x08B79A88(destination, a1 + 1, word(reader's + 4's + 0x18)); kinds 0 and 2
// first ask 0x089396D8(reader, byte & 0x3F) for a value and pass it on the same way. Kind 2 then unpacks that many bits
// of a1 + 1, copying the bytes at a1 + 1 + ceil(count / 8) whose bit is set, and answers 0 (or (count & 7) != 0 when
// count <= 0); any other kind answers 2. The saved registers are restored before the tail jump.

// --- holder wrappers ---------------------------------------------------------------------------------------------------------
void func_08BFEFAC(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CCEF20u); }

} // namespace
} // namespace new_batch_main_2086
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2086::name, gt::ReturnKind::kind)
NEWM(0x08B945C4u, func_08B945C4, Word);
NEWM(0x08BFEFACu, func_08BFEFAC, Word);
NEWM(0x08A3B9E8u, func_08A3B9E8, Word);
NEWM(0x08C7240Cu, func_08C7240C, Word);
NEWM(0x089A5DD0u, func_089A5DD0, Word);
NEWM(0x08B2EEB8u, func_08B2EEB8, Word);
NEWM(0x08AEEFD4u, func_08AEEFD4, Word);
NEWM(0x08A510B4u, func_08A510B4, Word);
NEWM(0x08C2EA68u, func_08C2EA68, Word);
NEWM(0x08B65CB4u, func_08B65CB4, Word);
NEWM(0x08B65DE0u, func_08B65DE0, Word);
NEWM(0x08818680u, func_08818680, Word);
NEWM(0x08AD31E0u, func_08AD31E0, Word);
NEWM(0x08A74B34u, func_08A74B34, Word);
