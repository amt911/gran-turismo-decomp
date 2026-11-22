// New batch 19: 72..76-instruction functions written by hand from `tools.disasm`. Two large families first (21 holder
// constructors that differ only in the vtable they store, 16 list walkers that differ only in the two getters they call),
// then singletons. Delay slots are read as "before the branch / call" (a likely branch's delay slot only when taken).
// The callee-saved registers the original keeps values in are set where the asm sets them (a callee may read them) and put
// back at the end; v0 ends exactly as the asm leaves it on every path. No names: none has evidence.
//
// Dead stack: the harness compares RAM outside the frame window below the entry sp, so stores to locals nothing reads back
// are not reproduced; the frames a callee reads or writes are (`GuestFrame`, offsets as in the original).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_19 {
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

// A guest call that leaves v0 / v1 / f0 in the context like the original's `jal` (a callee that sets no v0 hands the
// previous one on, so the context's v0 is what the code after the call sees).
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
// A virtual call as the compiler issues it: v1 = the vtable, v0 = the slot's function, then `jalr v0`.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments);
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
// MIPS `ins rt, rs, pos, size`: `size` low bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

u32 sra1(u32 value) { return static_cast<u32>(static_cast<s32>(value) >> 1); }
bool signed_less(u32 left, u32 right) { return static_cast<s32>(left) < static_cast<s32>(right); }

// `lw v1, 0(object); lw v0, offset(v1); jalr v0` with a null check site that asserts first.
GuestResult call_virtual_checked(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, u32 site,
                                 std::initializer_list<u32> arguments) {
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {site});
    return call_virtual(rt, ctx, object, offset, arguments);
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

// --- holder constructors (21 functions) -----------------------------------------------------------------------------
// a0 = self, a1 = text, a2 = a pair of words, a3 / t0 = two more pairs. 0x08A5BC20(0x2C, 4) allocates the holder,
// 0x08A98220(holder, &temp) initialises it, the vtable goes in word 0, a2[0] -> +0x14, a2[1] -> +0x18, a3[1] -> +0x20,
// a3[0] -> +0x1C, t0[1] -> +0x28, t0[0] -> +0x24; 0x08A69D68(&temp) finishes the temporary, the holder is stored into the
// caller's slot (a stack word) by 0x08A5D414(&slot, &temp), 0x08A7C5C8(self, &slot) takes it and 0x08A5B9EC(&slot) releases
// it. The original's branch is degenerate: its 0x08B867AC / zero test picks values that are both plain copies of a2's words.
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
    const u32 first = word(rt, pair);
    const u32 second = word(rt, pair + 4u);
    const u32 third_first = word(rt, third);
    const u32 fourth_first = word(rt, fourth);
    const u32 third_second = word(rt, third + 4u);
    const u32 fourth_second = word(rt, fourth + 4u);
    ctx.gpr[30] = frame.at(0u);  // fp = sp
    ctx.gpr[16] = temp;
    ctx.gpr[22] = second;
    ctx.gpr[19] = second;
    ctx.gpr[23] = third_first;
    ctx.gpr[21] = third_second;
    ctx.gpr[18] = fourth_second;
    call_set(rt, ctx, 0x08A69B3Cu, {temp, text});
    const u32 object = call_set_value(rt, ctx, 0x08A5BC20u, {0x2Cu, 4u});
    ctx.gpr[17] = object;
    call_set(rt, ctx, 0x08A98220u, {object, temp});
    put(rt, object, vtable);
    put(rt, object + 0x14u, first);
    put(rt, object + 0x18u, second);
    put(rt, object + 0x20u, third_second);
    put(rt, object + 0x1Cu, third_first);
    put(rt, object + 0x28u, fourth_second);
    put(rt, object + 0x24u, fourth_first);
    call_set(rt, ctx, 0x08A69D68u, {temp});
    put(rt, frame.at(0u), object);
    call_set(rt, ctx, 0x08A5D414u, {slot, frame.at(0u)});
    call_set(rt, ctx, 0x08A7C5C8u, {self, slot});
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
}

void func_08BEA858(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CCDC90u); }
void func_08BF3848(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CCE4B0u); }
void func_08BF5E6C(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CCE838u); }
void func_08BFEFAC(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CCEF20u); }
void func_08C06C3C(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CCF6A0u); }
void func_08C0B6BC(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CCFC00u); }
void func_08C127F0(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CD0110u); }
void func_08C14B28(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CD0350u); }
void func_08C29EE4(Runtime &rt, AllegrexContext &ctx) { make_holder(rt, ctx, 0x08CD4228u); }

// --- list walkers (16 functions) ------------------------------------------------------------------------------------
// a0 = self. `first_call(self)` prepares, `list_call(0)` answers the head of a singly linked list (next at +0x0C); for
// every node, a name is built from two stack temporaries (0x08B85EB8(&text, node->word, &flag) then
// 0x08B88F44(&name, &text)) and handed to 0x08BE9D90(self, &name), then both temporaries are released: when a temporary is
// not the shared empty 0x08D74FD0, 0x08B60804(&temporary - 4, -1) drops a reference and a non-positive answer goes on to
// 0x08B8542C(&temporary - 0xC). The 16 functions differ only in the two getters they call.
void walk_list(Runtime &rt, AllegrexContext &ctx, u32 first_call, u32 list_call) {
    constexpr u32 kEmptyTemporary = 0x08D74FD0u;
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u});
    GuestFrame frame(ctx, 0x50u);
    const u32 text = frame.at(0x4u);
    const u32 flag = frame.at(0x2u);
    const u32 name = frame.at(0x10u);
    ctx.gpr[21] = self;
    ctx.gpr[19] = text;
    ctx.gpr[22] = flag;
    ctx.gpr[20] = kEmptyTemporary;
    ctx.gpr[18] = name;
    call_set(rt, ctx, first_call, {self});
    u32 node = call_set_value(rt, ctx, list_call, {0u});
    ctx.gpr[17] = node;
    if (node == 0u) return;
    for (;;) {
        call_set(rt, ctx, 0x08B85EB8u, {text, word(rt, node), flag});
        call_set(rt, ctx, 0x08B88F44u, {name, text});
        call_set(rt, ctx, 0x08BE9D90u, {self, name});
        const u32 temporary = word(rt, name);
        if (temporary - 0xCu != kEmptyTemporary) {
            if (call_set_value(rt, ctx, 0x08B60804u, {temporary - 4u, 0xFFFFFFFFu}) <= 0) {
                call_set(rt, ctx, 0x08B8542Cu, {temporary - 0xCu});
            }
        }
        const u32 other = word(rt, text);
        if (other - 0xCu != kEmptyTemporary) {
            if (call_set_value(rt, ctx, 0x08B60804u, {other - 4u, 0xFFFFFFFFu}) <= 0) {
                call_set(rt, ctx, 0x08B8542Cu, {other - 0xCu});
            }
        }
        node = word(rt, node + 0xCu);
        if (node == 0u) return;
        ctx.gpr[17] = node;
    }
}


// 0x08B945C4 (a0 = a string holder, a1 = a source begin, a2 = a count): assigns the count bytes at `begin` to the
// string. The count above 0x3FFFFFFC asserts (0x08B5ADDC). When the source is outside the string's own buffer or the
// buffer reports spare room at word(data - 4), 0x08B94540(self, 0, word(data - 0xC), begin, count) takes over (tail call).
// Otherwise the bytes are copied (0x08B79A88, or 0x08B79BAC when the source overlaps and the count is above one, or one
// byte inline), word(data - 4) = 0, word(data - 0xC) = count, byte(data + count) = the global terminator at 0x08CBF760,
// and v0 = self.
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
    if (count > kLimit) call_set(rt, ctx, 0x08B5ADDCu, {0x08C9F6DCu});
    const u32 data = word(rt, self);
    const u32 length = word(rt, data - 0xCu);
    if (begin < data || data + length < begin || word(rt, data - 4u) > 0u) {
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
    put8(rt, end, byte_u(rt, kTerminator));
    ctx.gpr[2] = self;
}

// 0x08BDD140 (a0 = this): the vtable word 0x08CC5EE0 is stored and three runs of three 0x2C-byte embedded objects are
// destroyed through the first entry of each one's vtable (with the object's address), top-down: at this + 0x148, then
// +0xC4, then +0x40. 0x08925F98(this) runs and 0x08B64654(this) finishes (tail call).
void destroy_embedded_run(Runtime &rt, AllegrexContext &ctx, u32 self, u32 first_offset) {
    for (int index = 2; index >= 0; --index) {
        const u32 slot = self + first_offset + static_cast<u32>(index) * 0x2Cu;
        ctx.gpr[17] = slot + 0x2Cu;  // s1 walks the run downwards
        ctx.gpr[16] = slot;
        ctx.gpr[19] = 3u;
        call_virtual(rt, ctx, slot, 0u, {slot});
    }
}
void func_08BDD140(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u});
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[18] = self;
    ctx.gpr[21] = self + 0x148u;
    put(rt, self, 0x08CC5EE0u);
    destroy_embedded_run(rt, ctx, self, 0x148u);
    destroy_embedded_run(rt, ctx, self, 0xC4u);
    destroy_embedded_run(rt, ctx, self, 0x40u);
    call_set(rt, ctx, 0x08925F98u, {self});
    tail_call(ctx, 0x08B64654u, {self});
}

// 0x0897B5C0 (a0 = this, a1 = a holder): destroys the first entry of the list at this + 0x18 (0x08B344E0 locks this + 0x20,
// 0x08B3457C unlocks it) and walks the rest: each entry's payload at + 8 is compared with the holder through its virtual at
// +0x110, and while that answers nonzero the entry is released (0x08B5C094, 0x08A5B9EC of + 8, 0x08A5BC54(entry, 0xC, 4)).
// v0 = 1 once the list was non-empty, 0 when it was empty.

// 0x08A03ED8 (a0 = an output array of floats, a1 = a holder): 1.0f at +0..+0xC, -1 at +0x10, then for each of the
// word(a1) object's +0x7C virtual answers the element fetched by its +0x84 virtual (into a stack holder) has its +0x54
// float stored at a0 + index * 4; the holder is released each round. v0 = a0.

// 0x08A392C8 (a1 = passed on, a2 = a mode, a3 = a pair of holders): three modes. 1 takes the float of the +0x54 virtual
// of word(a3) as the second argument; below 2 the pair is (0, 1.0f); 2 and above takes the same float for the second
// argument and word(a3 + 4)'s for the first. 0x08A391F0(&holder, a1) answers an object that 0x08A38F98 receives with the
// two floats, then the holder is released.

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

// 0x08B2EAC8: a global section (0x08D690C0 / C4 / F0 / F4) is taken and drained: while byte(0x08D690F4) is set, the
// 0x08D690C4 lock is taken (0x08B34888, 0x08B34B2C when 0x08D690C0 is clear), the list at 0x08D690C0 is detached and each
// node released (its callback at +0x30 with word(node + 0x34), then 0x08B28C74(node + 4)); 0x08C7A9F0 closes the section.
// 0x08C7AA60 takes over at the end (tail call) with the handle of 0x08C7AA48(0x1046A, 0x08B2E964, 0).

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

// 0x08C2E1DC (a0 = this, a1/a2 = two holders): three word slots on the stack are cleared (0x08A76AA0), a1's value is
// copied into the first and a2's into the second (0x08A5BAD8, skipped when the holder is the slot), the third is cleared,
// and word(this + 4)'s +0xCC virtual (third slot, 2, first slot) fills the third; its +0x30 virtual answers v0, after
// which the third, second and first slots are released.

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

// 0x08937574 and 0x0893769C (a0 = this, a1 = an index): a name is built on the stack (0x08B7BB6C from this + 0xC, then
// 0x08B7B9AC with the word of the 0x08CDDE40 table at the index), 0x08B62344(size) allocates and `init` prepares it, it is
// stored at this + index * 4 + offset, and 0x08936498(this, index) decides: a nonzero answer takes 0x089372C8(this) and
// 0x08937AFC(0x10, answer), appends two more names (the 0x08CDDE0C table at word(this + 8), then the same literal) and
// calls `install` with the stored object, the buffer and both answers; otherwise one name and `fallback`. The two functions
// differ in the size, the two calls and the stored offset only.
void register_indexed_object(Runtime &rt, AllegrexContext &ctx, u32 size, u32 init, u32 store_offset, u32 name, u32 install,
                             u32 fallback) {
    const u32 self = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u});
    GuestFrame frame(ctx, 0x120u);
    ctx.gpr[18] = self;
    ctx.gpr[17] = index;
    ctx.gpr[20] = 0x08CA4000u;  // the relocated lui half behind the name literals
    call_set(rt, ctx, 0x08B7BB6Cu, {frame.at(0u), self + 0xCu});
    const u32 table_entry = word(rt, 0x08CDDE40u + index * 4u);
    ctx.gpr[19] = self + index * 4u;
    call_set(rt, ctx, 0x08B7B9ACu, {frame.at(0u), table_entry, name});
    const u32 created = call_set_value(rt, ctx, 0x08B62344u, {size});
    ctx.gpr[16] = created;
    call_set(rt, ctx, init, {created});
    put(rt, self + index * 4u + store_offset, created);
    if (call_set_value(rt, ctx, 0x08936498u, {self, index}) == 0u) {
        call_set(rt, ctx, 0x08B7B9ACu, {frame.at(0u), name, name});
        call_set(rt, ctx, fallback, {word(rt, self + index * 4u + store_offset), frame.at(0u)});
        return;
    }
    const u32 first = call_set_value(rt, ctx, 0x089372C8u, {self});
    const u32 second = call_set_value(rt, ctx, 0x08937AFCu, {0x10u, first});
    const u32 table2 = word(rt, 0x08CDDE0Cu + word(rt, self + 8u) * 4u);
    call_set(rt, ctx, 0x08B7B9ACu, {frame.at(0u), table2, name});
    call_set(rt, ctx, 0x08B7B9ACu, {frame.at(0u), name, name});
    call_set(rt, ctx, install, {word(rt, self + index * 4u + store_offset), frame.at(0u), second, first});
}

// 0x089C8680 (a0 = this, a1 = a key, a2 = a value): 0x08C009CC(word(this + 0x2C) + 0x18, &key) answers an entry; the
// sentinel word(this + 0x2C) + 0x1C, or a key below the entry's word +0x10, inserts one: a string temporary is built from
// the key (0x08B88F44 with a pair holding the shared empty 0x08D74FD0 + 0xC and the key), 0x08C0154C(container, entry, &key)
// answers the new entry, and both temporaries are released. 0x08B855E0(entry + 0x14, value) ends it either way.

// 0x08A2ADB4 (a0 = this, a1 = a mode, a2 = a holder): 0x08ACC220(&state) sets the state up. With mode 1 and a holder
// holding an object, the object's +0xC virtual writes a stack temporary, 0x08ACBEB0(&state, temporary) answers, and the
// temporary is released. Any other case answers 0x08ACBEB0(&state, 0). A zero answer converts to the stack holder at sp +
// 8 (0x08A97A6C with 0) and a nonzero one takes 0x08A2AA84(&state) and the holder at sp + 4 (0x08A97A6C with 1); the
// chosen holder is handed to this (0x08A5BAD8) and released, and 0x08ACC0F4(&state) ends the call.

// 0x08A38418 (a0 = this, a1 = a holder, a2 = a count, a3 = an object): with a positive count 0x08A85940(&entry, a3) takes
// an entry, its +0x3C virtual is asked, 0x08A38038(&element, holder) takes an element and 0x08B1ED7C(element + 0x14, answer)
// receives it; both temporaries are released. Without a count the element's word +0x9D8 is copied into a stack holder
// (0x08A85B1C) that this receives (0x08A5BAD8) and releases.

// 0x08A8D1E0 (a0 = this, a1 = a value, a2 = a second value): four stack holders. 0x08A9DD14(&first) answers an object,
// 0x08A9BC30(object) a property that 0x08A7C0F0(&second, property) holds; 0x08A7B544(&third, 0x08CF6F04, &second) builds
// the third, whose +0x28 virtual receives (this); 0x08A8CE2C(&fourth, this, a2) answers a holder whose +0x1C virtual
// receives a1, and all three are released.

// 0x08AEEFD4 (a0 = the target word, a1 = whether to wait): takes the 0x08D41AD0 lock (0x08C7AA20), scans up to
// word(+0x510) entries of the table (word(+0x500) + ((word(+0x50C) + index) mod word(+0x504)) * 32) for the target word,
// and releases it (0x08C7AA08). v0 = 1 when the target was found or word(+0x514) holds it; with a nonzero a1 it then
// signals 0x08C7AA10(word(+0x4FC), 1 << target, 0x20, 0, 0) and starts over, otherwise it returns that answer.
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
            const u32 divisor = word(rt, kState + 0x504u);
            const u32 table = word(rt, kState + 0x500u);
            u32 index = 0u;
            while (divisor != 0u) {
                const u32 slot = table + ((base + index) % divisor) * 32u;
                ++index;
                if (word(rt, slot) == target) {
                    found = 1u;
                    break;
                }
                if (!signed_less(index, limit)) break;
            }
        }
        ctx.gpr[18] = found;
        const u32 answer = word(rt, kState + 0x514u) == target ? 1u : found;
        call_set(rt, ctx, 0x08C7AA08u, {word(rt, kState + 0x4F8u), 1u});
        if (answer != 0u && wait) {
            call_set(rt, ctx, 0x08C7AA10u, {word(rt, kState + 0x4FCu), 1u << (target & 31u), 0x20u, 0u, 0u});
            continue;
        }
        ctx.gpr[2] = answer;
        return;
    }
}

// 0x08C1C304 (a0 = this, a1 = a begin node, a2 = an end node): when word(this + 0xC) is the begin and a2 is this + 4 the
// whole list is dropped: 0x08B87AB0(this, word(this + 8)), the sentinel words are reset and v0 = that call's answer.
// Otherwise, unless begin == end, 0x08B5F100(begin) is called and 0x08B5F444(begin, this + 4) answers the next node; its
// +0x14 is released (0x08A5B9EC), its word +0x10 either belongs to the shared empty 0x08D74FD0 (skipped) or is dropped by
// 0x08B60804 / 0x08B8542C, the node is released (0x08A5BC54 with 0x18 and 4), word(this + 0x14) is decremented and the walk
// ends at a2. v0 = the decremented count.

// 0x08A510B4 (a0 = a hash table, a1 = a value record): probes the 16-bit key table at word(this + 0x10) from
// word(this + 0x20) (the count), wrapping at word(this + 0xC) to 0 and stopping at the slot word(this + 0x20) - 1 mod
// word(this + 0x1C) or an empty slot. A match answers word(this + 0xC) + index * lhu(this + 0x1E); otherwise the count
// becomes (count - 1) mod capacity (word(this + 0x20)), the block at word(this + 0xC) + count * element size is zeroed
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
    const u32 previous = first < capacity ? first : count - 1u;
    const u32 wrap = word(rt, self + 0xCu);
    u32 index = count;
    ctx.gpr[16] = index;
    for (;;) {
        const u32 candidate = half_u(rt, keys + index * 2u);
        if (candidate == key) {
            ctx.gpr[2] = wrap + index * half_u(rt, self + 0x1Eu);
            return;
        }
        if (candidate == 0u || previous == index) break;
        index = index == wrap ? 0u : index + 1u;
        ctx.gpr[16] = index;
    }
    const u32 next_count = count == 0u ? capacity - 1u : count - 1u;
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

// 0x08BD6274 / 0x08BD64D4 (a0 = this): a reference-counted object. 0x08B34888 takes the lock on word(this + 0x10) and the
// count at +0x2C decides: with none, the lock is released (0x08B34B24), word(+0x38) is destroyed through its vtable's
// first entry with word(+0x34), both cleared, and 0x08B28BE0 / 0x08B64654 finish. Otherwise the count drops by one; when it
// reaches zero the held pair (+0x34 / +0x38) is detached, byte(+0x30) selects 0x08B34C1C, and after 0x08B34B24 the pair's
// +8 and first virtuals run. 0x08BAEF88(this) takes over in both cases (tail call).
void release_shared_object(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u});
    GuestFrame frame(ctx, 0x20u);
    const u32 inner = word(rt, self + 0x10u);
    ctx.gpr[17] = self;
    ctx.gpr[16] = inner;
    put(rt, self, vtable);
    call_set(rt, ctx, 0x08B34888u, {inner});
    if (word(rt, inner + 0x2Cu) == 0u) {
        call_set(rt, ctx, 0x08B34B24u, {inner});
        const u32 held = word(rt, inner + 0x38u);
        if (held != 0u) {
            call_virtual(rt, ctx, held, 0u, {held, word(rt, inner + 0x34u)});
            put(rt, inner + 0x38u, 0u);
            put(rt, inner + 0x34u, 0u);
        }
        call_set(rt, ctx, 0x08B28BE0u, {inner});
        call_set(rt, ctx, 0x08B64654u, {inner});
    } else {
        put(rt, inner + 0x2Cu, word(rt, inner + 0x2Cu) - 1u);
        if (word(rt, inner + 0x2Cu) == 0u) {
            const u32 object = word(rt, inner + 0x34u);
            if (object != 0u) {
                ctx.gpr[19] = word(rt, inner + 0x38u);
                ctx.gpr[18] = object;
                put(rt, inner + 0x34u, 0u);
                put(rt, inner + 0x38u, 0u);
            }
            if (byte_u(rt, inner + 0x30u) != 0u) call_set(rt, ctx, 0x08B34C1Cu, {inner});
        }
        call_set(rt, ctx, 0x08B34B24u, {inner});
        if (ctx.gpr[18] != 0u) call_virtual(rt, ctx, ctx.gpr[18], 8u, {ctx.gpr[18]});
        if (ctx.gpr[19] != 0u) call_virtual(rt, ctx, ctx.gpr[19], 0u, {ctx.gpr[19], ctx.gpr[18]});
    }
    tail_call(ctx, 0x08BAEF88u, {self});
}

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

// 0x08C4FD6C (a0 = passed to the callback, a1 = a count, a2 = a source, a3 = a callback, t0 = a pair of bounds): for
// (count - 1) rounds (nothing when that is zero, v0 then = count - word(t0 + 4)), 0x08B1EF70(source, count) answers a
// scale that round r (from the last index down) multiplies by (index + 1), converted to an integer the signed way; the
// integer wraps by word(t0 + 4) when below word(t0) and the callback receives (a0, the wrapped index, it, a3).

// 0x088F574C: initialises four 0x84-byte records at 0x08CE9C88 and their names at 0x08CDD6E4: each name is cleared
// (0x0893A814 with 0), each record is zeroed (0x08B36018 with 0x84) and three floats are set from the bytes of the table at
// 0x08CDD700 (0x0893A854, 0x0893A8D8, 0x0893A9CC): byte / 164000.0f, then the next two bytes the same way.
void func_088F574C(Runtime &rt, AllegrexContext &ctx) {
    const float scale = std::bit_cast<float>(0x451F6000u);
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 21u});
    GuestFrame frame(ctx, 0x30u);
    u32 name = 0x08CDD6E4u;
    for (u32 index = 0u; index != 4u; ++index) {
        ctx.gpr[17] = name;
        ctx.gpr[16] = index;
        call_set(rt, ctx, 0x0893A814u, {name, 0u});
        name += 0x28u;
    }
    ctx.gpr[18] = 0x08CDD700u;
    ctx.gpr[19] = 0u;
    ctx.gpr[21] = 4u;
    u32 record = 0x08CE9C88u;
    name = 0x08CDD6E4u;
    u32 table = 0x08CDD700u;
    u32 step = 0u;
    for (;;) {
        call_set(rt, ctx, 0x08B36018u, {record, 0u, 0x84u});
        ctx.fpr[12] = std::bit_cast<float>(0x3CC6F2D5u);
        ctx.fpr[13] = static_cast<float>(byte_u(rt, table)) / scale;
        call_set(rt, ctx, 0x0893A854u, {record, name, 0u});
        ctx.fpr[12] = std::bit_cast<float>(0x3C5BE3F9u);
        ctx.fpr[13] = static_cast<float>(byte_u(rt, table + 1u)) / scale;
        ctx.fpr[14] = static_cast<float>(byte_u(rt, table + 2u)) / scale;
        call_set(rt, ctx, 0x0893A8D8u, {record, name, 0u});
        call_set(rt, ctx, 0x0893A9CCu, {record, name});
        ++step;
        record += 0x84u;
        name += 0x28u;
        table += 0x28u;
        if (step == 4u) break;
    }
}

// 0x08805578: 0x08A9DD14(&temporary) takes the item at the global 0x08CE28A0 into a temporary that is stored there and
// released. 0x08B36278 answers a count and 0x08B36284 a word array: for each of them a name is built from the array word
// (0x08B85EB8 into a stack temporary) and given to the global's item (0x08A9C650), then released. 0x08A5BCF4 ends it.
void func_08805578(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kItem = 0x08CE28A0u;
    constexpr u32 kEmpty = 0x08D74FD0u;
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u, 30u});
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[30] = 0x08CDC500u;
    call_set(rt, ctx, 0x08A9DD14u, {frame.at(0x8u)});
    call_set(rt, ctx, 0x08A5BAD8u, {kItem, word(rt, frame.at(0x8u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
    const u32 count = call_set_value(rt, ctx, 0x08B36278u, {});
    ctx.gpr[19] = count;
    const u32 list = call_set_value(rt, ctx, 0x08B36284u, {});
    if (!signed_less(0u, count)) {
        call_set(rt, ctx, 0x08A5BCF4u, {});
        return;
    }
    ctx.gpr[23] = kEmpty;
    ctx.gpr[17] = list;
    ctx.gpr[20] = frame.at(0x4u);
    ctx.gpr[22] = frame.at(0x1u);
    ctx.gpr[21] = 0x08C94000u;
    u32 cursor = list;
    for (u32 index = 0u;;) {
        const u32 object = word(rt, kItem);
        ctx.gpr[16] = object;
        ++index;
        ctx.gpr[18] = index;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97C9Cu});
        ctx.gpr[17] = cursor;
        call_set(rt, ctx, 0x08B85EB8u, {frame.at(0x4u), word(rt, cursor), frame.at(0x1u)});
        call_set(rt, ctx, 0x08A9C650u, {object, frame.at(0x4u)});
        cursor += 4u;
        release_temporary(rt, ctx, word(rt, frame.at(0x4u)), frame.at(0u));
        if (count == index) break;
    }
    call_set(rt, ctx, 0x08A5BCF4u, {});
}

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

// 0x08830A78 (a0 = a holder): 0x088297A8(&temporary) fills it (assert 0x08C9A2F8) and it is released. The chain
// 0x08B0979C(temporary) -> 0x08B0980C -> 0x08B089D4 -> 0x08B2B5E0 -> 0x08B36018(last, 0, third) ends in a buffer: when the
// word at the fourth answer is nonzero, 0x08B79A88 copies it and 0x08A44D9C(&result, buffer, third, 1) builds the result
// that this receives; otherwise 0x08B35254 releases the buffer and 0x08A76AAC(&result, 0x08CF6ED8) makes the empty one.
// The chosen temporary is released either way.

// 0x088424D8 (a0 = passed on, a1 = a key, a2 = a count, a3 = a pair of holders): 0x088418B4(&temporary, a1) takes an entry
// (assert 0x08C9C584) that 0x0884177C answers and the temporary is released; 0x0888EB74(entry) answers a first value that
// a positive count replaces with the +0x3C virtual of word(a3), and two or more units add the same virtual of
// word(a3 + 4) as a third argument. 0x0888EBF4(entry, first, third) decides whether 0x0884F674(&result) is built into this.

// 0x08A1C5CC (a0 = this): a name is built on the stack from the literal 0x08CAA224 (0x08B85EB8 with a flag byte), the
// object's vtable word +0x108 handles (this, name) and the temporary is released. 0x08A76A94 answers a value that
// 0x08A5BA08 retains; the old word(this + 0x38) is released (0x08A5B930) and the new one stored. 0x08B8850C(this,
// 0x08CF6F14, 0x08A1CA40) and two 0x08B87108 registrations (0x08CAA278 / 0x08A1C704, 0x08CAA28C / 0x08A1C3B0) follow, and
// 0x08B8D51C(this, 0x08CAA2A0, 0x08A1C408, 0).
void func_08A1C5CC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16u, 17u, 18u});
    GuestFrame frame(ctx, 0x20u);
    const u32 name = frame.at(0x4u);
    ctx.gpr[18] = self;
    const u32 handler = word(rt, word(rt, self) + 0x108u);
    call_set(rt, ctx, 0x08B85EB8u, {name, 0x08CAA224u, frame.at(0x1u)});
    call_set(rt, ctx, handler, {self, name});
    release_temporary(rt, ctx, word(rt, name), frame.at(0u));
    const u32 value = call_set_value(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = value;
    if (value != 0u) call_set(rt, ctx, 0x08A5BA08u, {value});
    const u32 old = word(rt, self + 0x38u);
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, self + 0x38u, value);
    call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A1CA40u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAA278u, 0x08A1C704u});
    call_set(rt, ctx, 0x08B87108u, {self, 0x08CAA28Cu, 0x08A1C3B0u});
    call_set(rt, ctx, 0x08B8D51Cu, {self, 0x08CAA2A0u, 0x08A1C408u, 0u});
}

// 0x08A3397C / 0x08A33AA8 (a0 = this, a1 = a holder): two 64-bit answers are combined and stored. 0x08B2DFB4(this, holder)
// answers the left pair, 0x08A33604(&temporary, holder) an entry whose +0x14 0x08B1B668 answers as the right pair, and
// 0x08A337A8(&target) the destination object; 0x08B1B740 combines the halves (a sum, or a difference with the borrow) and
// its pair goes to target + 0x14 / + 0x18 and four stack words. `this` receives the target (0x08A5BAD8) unless it is the
// temporary, which is released.
void combine_half_pairs(Runtime &rt, AllegrexContext &ctx, bool subtract) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 21u, 22u});
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[22] = self;
    ctx.gpr[16] = source;
    ctx.gpr[21] = frame.at(0x4u);
    const GuestResult left = call_set(rt, ctx, 0x08B2DFB4u, {self, source});
    ctx.gpr[18] = left.v0;
    ctx.gpr[19] = left.v1;
    call_set(rt, ctx, 0x08A33604u, {frame.at(0u), source});
    const u32 holder = word(rt, frame.at(0u));
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CACCECu});
    const GuestResult right = call_set(rt, ctx, 0x08B1B668u, {holder + 0x14u});
    ctx.gpr[16] = right.v0;
    ctx.gpr[17] = right.v1;
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call_set(rt, ctx, 0x08A337A8u, {frame.at(0x4u)});
    const u32 target = word(rt, frame.at(0x4u));
    if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CACCECu});
    u32 low;
    u32 high;
    if (subtract) {
        low = right.v0 - left.v0;
        high = right.v1 - left.v1 - (right.v0 < low ? 1u : 0u);
    } else {
        low = left.v0 + right.v0;
        high = left.v1 + right.v1 + (low < left.v0 ? 1u : 0u);
    }
    const GuestResult sum = call_set(rt, ctx, 0x08B1B740u, {low, high});
    put(rt, target + 0x14u, sum.v0);
    put(rt, target + 0x18u, sum.v1);
    put(rt, frame.at(0x10u), sum.v0);
    put(rt, frame.at(0x14u), sum.v1);
    put(rt, frame.at(0x8u), sum.v0);
    put(rt, frame.at(0xCu), sum.v1);
    if (self != frame.at(0x4u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0x4u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
}

// 0x08C0FC84 (a0 = this, a1 = a holder): two dispatch words at this + 0x18 and this + 0x20 select a handler: bit 0 set
// means the word is an index into the table the holder's word points at (handler = word(word(table + index) + base),
// base = word(this + 0x14) / word(this + 0x1C)), clear means the base itself is the handler. Each handler is called with
// a0 = the table + index and the second one with the first handler's answer as its table. The answer becomes a name
// (0x08B88F44 then 0x08BE9EEC into a second stack temporary) that 0x08A5BAD8 hands to the holder unless the holder is
// that temporary; both temporaries are released and v0 ends as the last release's answer (or the empty sentinel).

// 0x08808690 (a0 = this, a1 = a pointer to a text): 0x08B10204 builds a 0x50-byte value on the stack from word(a1); its
// 15 words are copied 60 bytes further down and, when the first word is nonzero, 0x08A40C14(self, 0x08CF6ED8) runs and
// v0 = self. Otherwise 0x08A32A94(&length) publishes a length, 0x08A32350(block, item, word(sp + 0x18), 1) attaches the
// item, 0x08B35254(item) finishes and 0x08A40C24(self, item + 0x14) is called; v0 = self either way.

// 0x08828374 (a0 = this, a1/a2 = a range, a3 = an object): 0x088248E4 fills a stack temporary from (a1, a2); its item
// (word + 0x14) is kept and the temporary released. With a non-positive count 0x0885BBD8(item) answers a word pair that
// 0x08A5DEE8 copies into a second temporary, which is handed to this (0x08A5BAD8) and released. With a positive count an
// inner temporary is taken (0x08847008), released (0x08846E50, 0x08A5B9EC), and the object's virtual at +0x50 decides:
// 0x0885BBC8(item, ?, its answer, its vtable) is called. v0 = that call's answer.

// 0x088BF82C (a0 = this): 0x088C50B8(&list, word(this + 0x1C), byte(word(this + 0x24) + 0x10)) fills a list, its first
// word is the count; the float at 0x088DF13C(word(this + 8), word(this + 0xC))[0] is scaled by the entry picked there.
// While the count is positive, the first entry (0x088C50C4(&list, i)) whose float at +0x14 exceeds the scale sets the
// selection byte at word(this + 0x24) + 0x11; the entry at (count + selection - 1) mod count is taken (0x088C52A0 then
// 0x088C50C4), its byte stored at +0x13 of the same object, and the float at this + 4 word 0x1360 or
// 0x088BDEE0(this, entry) goes to that object's +0x1C. 0x088BE160(this) finishes.

// 0x088EB410 (a0 = this): a descriptor on the stack (twelve halfwords of 0xFFFF at +0..+0x16, three zero bytes at
// +0x18..+0x1A, 0x120 zero bytes at +0x20) is filled by two virtual calls at +0x68 of word(this + 0x538) (0x08CA0AD8 /
// 0x08CA0B14, offsets this + 0x57C / this + 0x580, the descriptor as t0, 1 as t1), each followed by the object's +0x70
// virtual; the answers land at this + 0x55C / +0x11C and +0x558. 0x088ED6F4(this, answer) runs and the +0x84 virtual
// takes the sign-extended halfword of the third answer.
void func_088EB410(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, {16u, 17u});
    GuestFrame frame(ctx, 0x150u);
    const u32 descriptor = frame.at(0u);
    const u32 buffer = frame.at(0x20u);
    ctx.gpr[17] = self;
    for (u32 offset = 0u; offset < 0x18u; offset += 2u) put_half(rt, descriptor + offset, 0xFFFFFFFFu);
    put8(rt, descriptor + 0x18u, 0u);
    put8(rt, descriptor + 0x19u, 0u);
    put8(rt, descriptor + 0x1Au, 0u);
    const u32 host = word(rt, self + 0x538u);
    ctx.gpr[16] = buffer;
    const GuestResult first = call_virtual(rt, ctx, host, 0x68u,
                                           {host, sext16(half_u(rt, self + 0x57Cu)), 0x08CA0AD8u, 0u, descriptor, 1u});
    put(rt, self + 0x55Cu, first.v0);
    const u32 host2 = word(rt, self + 0x538u);
    const GuestResult second = call_virtual(rt, ctx, host2, 0x70u, {host2, first.v0});
    put(rt, self + 0x11Cu, second.v0);
    call_set(rt, ctx, 0x08B36018u, {buffer, 0u, 0x120u});
    const u32 host3 = word(rt, self + 0x538u);
    const GuestResult third = call_virtual(rt, ctx, host3, 0x68u,
                                           {host3, sext16(half_u(rt, self + 0x580u)), 0x08CA0B14u, 0u, buffer, 1u});
    ctx.gpr[16] = third.v0;
    put(rt, self + 0x558u, third.v0);
    const u32 host4 = word(rt, self + 0x538u);
    const GuestResult fourth = call_virtual(rt, ctx, host4, 0x70u, {host4, third.v0});
    call_set(rt, ctx, 0x088ED6F4u, {self, fourth.v0});
    const u32 host5 = word(rt, self + 0x538u);
    call_virtual(rt, ctx, host5, 0x84u, {host5, sext16(third.v0)});
}

// 0x08927F9C (a0 = this, a1 = a list): 0x089273D8(self) answers an object whose +0x2C virtual decides whether the list is
// walked: with a nonzero answer the loop calls 0x088DD0E4(self, index) and, when that answers nonzero,
// 0x088E1B90(list, index) is asked and a nonzero answer sets the flag; 0x088D9F80(list) is the count and the index stops
// below it. 0x088D6358(list + 4) == 7 asks 0x089273B4(self); that, or the loop flag, toggles byte this + 0x1FCE. Finally
// the object's +0xC virtual (0x80) increments a wrap-around counter at this + 0x1FC0 (0..2) and v0 = its new value.

// 0x08AC0F08 (a0 = this, a1 = ?, a2 = ?, a3 = a pair, t0 = a pair, t1 = a flag byte): 0x08B0EA48(0) allocates. With an
// allocation, byte(allocated + 0x68) = 1, word + 0x7C = 0, the a3 pair goes to (allocated + 0x128 header) + 0x28 / +0x2C,
// the flag byte to +0x10C, the t0 pair to allocated + 0x6C..0x78, then 0x08B0DE84(allocated, a2, t0[2], t0[3]) runs and
// 0x08B0E560(this, allocated) finishes (tail call). Without one, the function at t0[2] takes over (tail call with
// t0[3]) when it is nonzero, else v0 = t0 + 8.

} // namespace
} // namespace new_batch_19
} // namespace gt::game

#define NEW19(address, name, kind) GT_DECOMP(address, gt::game::new_batch_19::name, gt::ReturnKind::kind)
NEW19(0x08BEA858u, func_08BEA858, Word);
NEW19(0x08BF3848u, func_08BF3848, Word);
NEW19(0x08BF5E6Cu, func_08BF5E6C, Word);
NEW19(0x08BFEFACu, func_08BFEFAC, Word);
NEW19(0x08C06C3Cu, func_08C06C3C, Word);
NEW19(0x08C0B6BCu, func_08C0B6BC, Word);
NEW19(0x08C127F0u, func_08C127F0, Word);
NEW19(0x08C14B28u, func_08C14B28, Word);
NEW19(0x08C29EE4u, func_08C29EE4, Word);
NEW19(0x088EB410u, func_088EB410, Word);
NEW19(0x08B945C4u, func_08B945C4, Word);
NEW19(0x08BDD140u, func_08BDD140, Word);
NEW19(0x08A3B9E8u, func_08A3B9E8, Word);
NEW19(0x08B2EEB8u, func_08B2EEB8, Word);
NEW19(0x08C7240Cu, func_08C7240C, Word);
NEW19(0x08AEEFD4u, func_08AEEFD4, Word);
NEW19(0x08B65CB4u, func_08B65CB4, Word);
NEW19(0x08B65DE0u, func_08B65DE0, Word);
NEW19(0x08C2EA68u, func_08C2EA68, Word);
NEW19(0x08A510B4u, func_08A510B4, Word);
NEW19(0x088F574Cu, func_088F574C, Word);
NEW19(0x08805578u, func_08805578, Word);
NEW19(0x08818680u, func_08818680, Word);
NEW19(0x08A1C5CCu, func_08A1C5CC, Word);
