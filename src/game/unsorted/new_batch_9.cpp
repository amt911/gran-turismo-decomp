// New batch 9 (drafted by the delegate LLM, reviewed against the asm): constructors and destructors the extra seeds made emittable.
// Delay slots are read as "before the branch / call"; callee-saved registers are set where the asm sets them and put back; `v0` ends
// as the asm leaves it. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_9 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// Which callee-saved registers a body keeps values in (bit n = s_n): SavedRegisters restores exactly those on exit.
constexpr u32 kS0 = 0x1u;
constexpr u32 kS0S1 = 0x3u;
constexpr u32 kS0S1S2 = 0x7u;
constexpr u32 kS0S2S3 = 0xDu;
constexpr u32 kS0S1S2S3 = 0xFu;

// The mask of the Allegrex `ins r, zero, 0, 10`: the low ten bits of the register, cleared.
constexpr u32 kLow10Bits = 0x3FFu;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
struct SavedS0 {
    AllegrexContext &ctx;
    u32 value;
    explicit SavedS0(AllegrexContext &c) : ctx(c), value(c.gpr[16]) {}
    ~SavedS0() { ctx.gpr[16] = value; }
};

// Saves the caller's callee-saved registers selected by `mask` (bit n = s_n) and restores them on scope exit.
struct SavedRegisters {
    AllegrexContext &ctx;
    u32 mask;
    u32 saved[8];
    SavedRegisters(AllegrexContext &c, u32 m) : ctx(c), mask(m) {
        for (u32 i = 0; i < 8; ++i)
            saved[i] = c.gpr[16 + i];
    }
    ~SavedRegisters() {
        for (u32 i = 0; i < 8; ++i)
            if (mask & (1u << i))
                ctx.gpr[16 + i] = saved[i];
    }
};

// Looks up an id/handle pair and dispatches to the handler; returns 1 on success, 0 otherwise.
void func_0880ABAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    SavedRegisters saved(ctx, kS0S1S2); // s0, s1, s2
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = arg;  // s2
    ctx.gpr[17] = self; // s1
    const u32 handle = word(rt, arg);
    ctx.gpr[16] = handle; // s0
    if (handle == 0) {
        ctx.gpr[2] = 0;
        return;
    }
    const u32 key = call(rt, ctx, 0x0880AA4Cu, {self, arg});
    const u32 found = call(rt, ctx, 0x08A76F20u, {handle, key});
    if (found == 0) {
        ctx.gpr[2] = 0;
        return;
    }
    call(rt, ctx, 0x08A5BAD8u, {self, word(rt, arg)});
    ctx.gpr[2] = 1;
}

// Destructor: installs the vtable, releases the pointer range held by the object, and finishes the base teardown.
void func_08A989B4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0S1); // s0, s1
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self; // s1
    const u32 s0 = self + 0x14u;
    ctx.gpr[16] = s0; // s0
    put(rt, self, 0x08CB7108u);
    ctx.gpr[2] = 0x08CB7108u;
    call(rt, ctx, 0x08B856B0u, {word(rt, s0), word(rt, s0 + 0x4u), frame.at(0)});
    const u32 begin = word(rt, s0);
    const u32 end = word(rt, s0 + 0x8u);
    const u32 bytes = static_cast<u32>(static_cast<s32>(end - begin) >> 2) << 2;  // `sra` then `sll`: an arithmetic shift
    ctx.gpr[2] = begin;
    if (begin != 0) {
        call(rt, ctx, 0x08A5BC54u, {begin, bytes, 0x4u});
    }
    call(rt, ctx, 0x08A780B0u, {self});
}

// Initializes the object: vtable, base subobject, two embedded subobjects, and default state fields.
void func_088F1F84(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CC3B30u);
    ctx.gpr[2] = 0x08CC3B30u;
    call(rt, ctx, 0x08B28DCCu, {self + 0x4u});
    put(rt, self + 0x2Cu, 0);
    const u32 sub_vtable = 0x08CC3BA8u;
    u32 elem = self + 0x30u;
    for (u32 i = 2; i != 0; --i) {
        put(rt, elem, sub_vtable);
        put(rt, elem + 0x404u, 0);
        elem += 0x408u;
    }
    put(rt, self + 0x840u, 0x12345678u);
    ctx.gpr[2] = 0x12345678u;
    call(rt, ctx, 0x088F5AFCu, {self + 0x844u});
    put(rt, self + 0x9C8u, 0xFFFFFFFFu);
    rt.memory().store8(self + 0x9CCu, 0);
    put(rt, self + 0x9B8u, 0);
    put(rt, self + 0x9BCu, 0);
    rt.memory().store8(self + 0x9C4u, 0);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// Destructor: frees the global singleton when both its pointer and live flag are set, then chains to the base destructor.
void func_08A2E748(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, kS0S1S2S3); // s0, s1, s2, s3
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[18] = 0x08CF0000u; // s2 and s3 hold the relocated `lui` half of 0x08CF6648 / 0x08CF664C
        ctx.gpr[19] = 0x08CF0000u;
        put(rt, self, 0x08CBE590u);
        ctx.gpr[2] = 0x08CBE590u;
        const u32 owned = word(rt, 0x08CF6648u);
        ctx.gpr[16] = owned; // s0
        ctx.gpr[17] = self;  // s1
        if (owned != 0) {
            const u32 live = rt.memory().load8(0x08CF664Cu);
            ctx.gpr[2] = live;
            if (live != 0) {
                call(rt, ctx, 0x08B097ACu, {owned});
                call(rt, ctx, 0x08B64654u, {owned});
                put(rt, 0x08CF6648u, 0);
                rt.memory().store8(0x08CF664Cu, 0);
            }
        }
    }
    tail_call(ctx, 0x08A780B0u, {self});
}

// Constructor: installs the vtable, initializes two members, clears two 44-word arrays, and copies an 8-byte default block.
void func_08936C84(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedS0 saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CC67B8u);
    ctx.gpr[2] = 0x08CC67B8u;
    call(rt, ctx, 0x08938EACu, {self + 0x1F4u});
    call(rt, ctx, 0x08938EACu, {self + 0x2D0u});
    put(rt, self + 0x8u, 0xFFFFFFFFu);
    put(rt, self + 0x4u, 0);
    for (u32 i = 0; i < 0x2Cu; ++i) {
        put(rt, self + 0x8Cu + i * 4u, 0);
        put(rt, self + 0x13Cu + i * 4u, 0);
    }
    put(rt, self + 0x1ECu, 0);
    put(rt, self + 0x1F0u, 0);
    const u32 defaults = 0x08CA21E4u; // an 8-byte default block, copied here by the original's lwl/lwr pair
    put(rt, self + 0x0Cu, word(rt, defaults));
    put(rt, self + 0x10u, word(rt, defaults + 4u));
    ctx.gpr[2] = self + 0x0Cu;
}

// --- family: release the object a slot holds (object at +offset+4, its companion value at +offset) ---

// The object held at +offset+4 is released through the first virtual of its vtable, called with the object and the
// companion word at +offset; both words are then cleared. No object, nothing to do (v1 keeps the word read, as the asm).
void release_member(Runtime &rt, AllegrexContext &ctx, u32 self, u32 offset) {
    const u32 object = word(rt, self + offset + 4u);
    ctx.gpr[3] = object;
    if (object == 0u) return;
    const u32 function = word(rt, word(rt, object));
    call_indirect(rt, ctx, function, {object, word(rt, self + offset)});
    put(rt, self + offset + 4u, 0u);
    put(rt, self + offset, 0u);
}

// Destructor: stores the vtable word, releases the member at +offset, then 0x08A780B0(this) finishes.
void release_member_then_base(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 offset) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, kS0S1); // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + offset;
        put(rt, self, vtable);
        ctx.gpr[2] = vtable;
        release_member(rt, ctx, self, offset);
    }
    tail_call(ctx, 0x08A780B0u, {self});
}

// --- constructors and destructors ---

// 0x08AB9F60 (a0 = this, a1 = a value): vtable 0x08CD5FB8; the object joins the list whose head is the global word
// 0x08D77180 (its link word at +4, its own address at +8), the member at +0xC is initialized, a1 lands at +0x18.
// v0 = the list helper's result.
void func_08AB9F60(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    SavedRegisters saved(ctx, kS0S1); // s0, s1
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = value;
    const u32 previous = word(rt, 0x08D77180u);
    put(rt, self, 0x08CD5FB8u);
    ctx.gpr[2] = self + 4u; // v0 = the link word, stored as the new list head in the call's delay slot
    put(rt, self + 8u, self);
    put(rt, self + 4u, previous);
    put(rt, 0x08D77180u, self + 4u);
    call(rt, ctx, 0x08C3C138u, {self + 0x0Cu, self});
    put(rt, self + 0x18u, value);
}

// 0x08B21BF8 (a0 = this): vtable 0x08CD9538, the member at +4 initialized, the pairs at +0x2C/+0x30 cleared, +0x34 and
// +0x40 initialized, and the four words around +0x5C (+0x5C, +0x60 and the sub-vtable at +0x5C's neighbour) set up.
void func_08B21BF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0); // s0
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CD9538u);
    ctx.gpr[2] = 0x08CD9538u;
    call(rt, ctx, 0x08B28DCCu, {self + 4u});
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x30u, 0u);
    call(rt, ctx, 0x08C4FB18u, {self + 0x34u, self});
    call(rt, ctx, 0x08C4FB30u, {self + 0x40u});
    put(rt, self + 0x60u, 0x08D68B7Cu);
    put(rt, self + 0x6Cu, 0u);
    put(rt, self + 0x5Cu, 0x08D68B7Cu);
    put(rt, self + 0x64u, 0u);
    put(rt, self + 0x68u, 0u);
    ctx.gpr[2] = 0x08D68B7Cu;
}

// 0x08B2FEB4 (a0 = this): vtable 0x08CDA720, the global word 0x08D69160 cleared, 0x08B35214(this) answers an object that
// 0x08B344E0 initializes; a second object at +0x964C, 0x08B35238(0) and 0x08B3457C(first), then 0x08B28DC4(this + 0x8FC)
// finishes (tail call).
void func_08B2FEB4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 first;
    {
        SavedRegisters saved(ctx, kS0S1); // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CDA720u);
        ctx.gpr[2] = 0x08CDA720u;
        put(rt, 0x08D69160u, 0u); // the call's delay slot
        first = call(rt, ctx, 0x08B35214u, {self});
        ctx.gpr[17] = first;
        call(rt, ctx, 0x08B344E0u, {first});
        call(rt, ctx, 0x08B2F1B4u, {self + 0x964Cu});
        call(rt, ctx, 0x08B35238u, {0u});
        call(rt, ctx, 0x08B3457Cu, {first});
    }
    tail_call(ctx, 0x08B28DC4u, {self + 0x8FCu});
}

// 0x08B118A4 (a0 = this): vtable 0x08CD7CA8, then each member initialized in turn: +4 (with this), +0x14, +0x40, +0x4C,
// +0x58, +0x64; the member at +0x10 and the four words +0x90..+0x9C cleared. v0 = the last initializer's result.
void func_08B118A4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0); // s0
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CD7CA8u);
    ctx.gpr[2] = 0x08CD7CA8u;
    call(rt, ctx, 0x08C4C88Cu, {self + 4u, self});
    put(rt, self + 0x10u, 0u);
    call(rt, ctx, 0x08B28D34u, {self + 0x14u});
    call(rt, ctx, 0x08C4C82Cu, {self + 0x40u});
    call(rt, ctx, 0x08C4C82Cu, {self + 0x4Cu});
    call(rt, ctx, 0x08C4C82Cu, {self + 0x58u});
    call(rt, ctx, 0x08B28D34u, {self + 0x64u});
    rt.memory().store8(self + 0x9Cu, 0);
    put(rt, self + 0x90u, 0u);
    put(rt, self + 0x94u, 0u);
    put(rt, self + 0x98u, 0u);
}

// 0x089BB87C (a0 = this, the second argument is the constant 0x08CF6FDC): the subobject is built first, then the two
// vtable words (+0 and +0x38) are stored, the words +0x3C..+0x54 cleared, +0x58 initialized, +0x5C cleared and the low
// ten bits of the word at +0x60 dropped (`ins v0, zero, 0, 10`). v0 = that word.
void func_089BB87C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0); // s0
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    call(rt, ctx, 0x08A7E88Cu, {self, 0x08CF6FDCu});
    put(rt, self, 0x08CB8F58u);
    put(rt, self + 0x38u, 0x08CB90F4u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x40u, 0u);
    put(rt, self + 0x44u, 0u);
    put(rt, self + 0x48u, 0u);
    put(rt, self + 0x4Cu, 0u);
    put(rt, self + 0x50u, 0u);
    put(rt, self + 0x54u, 0u);
    call(rt, ctx, 0x08A696E4u, {self + 0x58u});
    put(rt, self + 0x5Cu, 0u);
    const u32 flags = word(rt, self + 0x60u) & ~kLow10Bits;
    put(rt, self + 0x60u, flags);
    ctx.gpr[2] = flags;
}

// 0x08AA98E4 (a0 = this): vtable 0x08CD5358, the member at +4 initialized, then six empty-link triples: each of the groups
// at +8, +0x20 and +0x38 gets {next = 0, previous-free = 0, back-pointer to its own first word}. No call, v0 = this + 8.
void func_08AA98E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0); // s0
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CD5358u);
    ctx.gpr[2] = 0x08CD5358u;
    call(rt, ctx, 0x08A71E28u, {self + 4u});
    const u32 head = self + 8u;     // v0
    const u32 tail = self + 0x20u;  // v1
    const u32 last = self + 0x38u;  // a2
    put(rt, head + 0x14u, 0u);
    put(rt, head + 0x10u, head + 4u);
    put(rt, head + 4u, 0u);
    put(rt, head + 8u, 0u);
    put(rt, head + 0xCu, head + 4u);
    put(rt, tail + 0x10u, tail + 4u);
    put(rt, tail + 4u, 0u);
    put(rt, tail + 0x14u, 0u);
    put(rt, tail + 8u, 0u);
    put(rt, tail + 0xCu, tail + 4u);
    put(rt, last, 0u);
    put(rt, last + 8u, 0u);
    put(rt, last + 4u, 0u);
    ctx.gpr[2] = head;
}

// 0x08ABFC8C (a0 = this, a1 and a2 passed on): vtable 0x08CD6450, the member at +4 initialized, two subobjects built by
// 0x08ABFC48 (at +0x30 with a1, at +0x44 with a2, each with this as its second argument and 0 as the fourth), then the
// words at +0x5C..+0x5D and +0x58 cleared. v0 = the second construction's result.
void func_08ABFC8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    SavedRegisters saved(ctx, kS0S1S2); // s0, s1, s2
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    ctx.gpr[17] = first;
    ctx.gpr[18] = second;
    put(rt, self, 0x08CD6450u);
    ctx.gpr[2] = 0x08CD6450u;
    call(rt, ctx, 0x08B28D34u, {self + 4u});
    call(rt, ctx, 0x08ABFC48u, {self + 0x30u, self, first, 0u});
    call(rt, ctx, 0x08ABFC48u, {self + 0x44u, self, second, 0u});
    rt.memory().store8(self + 0x5Du, 0);
    put(rt, self + 0x58u, 0u);
    rt.memory().store8(self + 0x5Cu, 0);
}

// 0x088F200C (a0 = this): vtable 0x08CC3B48, 0x088F1C98(this), the member at +8 released (+0xC holds the object, +8 its
// value), and the base vtable 0x08CBF518 is stored. v0 = 0x08CBF518.
void func_088F200C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0S1); // s0, s1
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 slot = self + 8u; // s1
    ctx.gpr[17] = slot;
    put(rt, self, 0x08CC3B48u);
    ctx.gpr[2] = 0x08CC3B48u;
    call(rt, ctx, 0x088F1C98u, {self});
    const u32 object = word(rt, slot + 4u);
    ctx.gpr[3] = object;
    if (object != 0u) {
        const u32 function = word(rt, word(rt, object));
        call_indirect(rt, ctx, function, {object, word(rt, slot)});
        put(rt, slot, 0u);
        put(rt, slot + 4u, 0u);
    }
    put(rt, self, 0x08CBF518u);
    ctx.gpr[2] = 0x08CBF518u;
}

// 0x0886F1F4 (a0 = this): vtable 0x08CBF7D8, the member built twice (0x0886AA0C(this, 5) then 0x0886AA0C(this, 2)), the
// object held at +0x70 released, then 0x0886A858(this) finishes (tail call).
void func_0886F1F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, kS0S1); // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 0x70u;
        put(rt, self, 0x08CBF7D8u);
        ctx.gpr[2] = 0x08CBF7D8u;
        call(rt, ctx, 0x0886AA0Cu, {self, 5u});
        call(rt, ctx, 0x0886AA0Cu, {self, 2u});
        release_member(rt, ctx, self, 0x70u);
    }
    tail_call(ctx, 0x0886A858u, {self});
}

// 0x0887EA1C (a0 = this): vtable 0x08CC10C8, the member built (0x0886AA0C(this, 2)), the object held at +0x70 released,
// then 0x0886A858(this) finishes (tail call).
void func_0887EA1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, kS0S1); // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 0x70u;
        put(rt, self, 0x08CC10C8u);
        ctx.gpr[2] = 0x08CC10C8u;
        call(rt, ctx, 0x0886AA0Cu, {self, 2u});
        release_member(rt, ctx, self, 0x70u);
    }
    tail_call(ctx, 0x0886A858u, {self});
}

// 0x08A23E9C (a0 = this): vtable 0x08CD06F8, 0x08A23A2C(this), the object held at +0x14 released (object at +0x18), then
// 0x08A780B0(this) finishes (tail call).
void func_08A23E9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, kS0S1); // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 0x14u;
        ctx.gpr[2] = 0x08CD06F8u;
        put(rt, self, 0x08CD06F8u); // the call's delay slot
        call(rt, ctx, 0x08A23A2Cu, {self});
        release_member(rt, ctx, self, 0x14u);
    }
    tail_call(ctx, 0x08A780B0u, {self});
}

// 0x08A1C49C (a0 = this): vtable 0x08CD0B38, the object held at +0x14 released (object at +0x18), 0x08A780B0(this) finishes.
void func_08A1C49C(Runtime &rt, AllegrexContext &ctx) { release_member_then_base(rt, ctx, 0x08CD0B38u, 0x14u); }

// 0x089F1120 (a0 = this): vtable 0x08CBA5F8, the pointer range at +0x20..+0x28 released (0x08B856B0, then the range with
// 0x08A5BC54 when non-empty), the member at +0x1C released and 0x089FDE00(this) finishes. v0 = its result.
void func_089F1120(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0S1); // s0, s1
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    const u32 range = self + 0x20u; // s0
    ctx.gpr[16] = range;
    put(rt, self, 0x08CBA5F8u);
    ctx.gpr[2] = 0x08CBA5F8u;
    call(rt, ctx, 0x08B856B0u, {word(rt, range), word(rt, range + 4u), frame.at(0u)});
    const u32 begin = word(rt, range);
    const u32 end = word(rt, range + 8u);
    const u32 bytes = static_cast<u32>(static_cast<s32>(end - begin) >> 2) << 2; // `sra` then `sll`
    ctx.gpr[2] = begin;
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x1Cu});
    call(rt, ctx, 0x089FDE00u, {self});
}

// 0x0891FD68 (a0 = this): vtable 0x08CC5DC8; thirteen words at +0x250 step by 4 are each released, when present, through
// the second virtual of their object's vtable (called with the object) and cleared; 0x08925FF8(this) finishes (tail call).
void func_0891FD68(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx, kS0S1S2S3); // s0, s1, s2, s3
        GuestFrame frame(ctx, 0x20u);
        ctx.gpr[19] = self;
        ctx.gpr[18] = 13u;
        ctx.gpr[17] = 0u;
        put(rt, self, 0x08CC5DC8u);
        u32 slot = self;
        ctx.gpr[16] = slot;
        for (u32 visited = 0;;) {
            const u32 object = word(rt, slot + 0x250u);
            ++visited;
            ctx.gpr[17] = visited;
            if (object != 0u) {
                const u32 vtable = word(rt, object);
                ctx.gpr[2] = vtable; // v0 holds the vtable the `jalr $v1` jumps through
                ctx.gpr[4] = object; // the call's delay slot
                call(rt, ctx, word(rt, vtable + 4u), {object});
                put(rt, slot + 0x250u, 0u);
            }
            if (visited == 13u) break;
            slot += 4u;
            ctx.gpr[16] = slot;
        }
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08925FF8u, {self});
}

// --- small helpers with a flag word ---

// 0x089CC068 (a0 passed to 0x089CC040, which answers an object in the local at sp + 4): 0x08B1ABB0(object) answers the
// result; then the block at object - 0xC is freed (or given back to the pool): nothing when it is the shared empty block
// 0x08D74FD0, otherwise only when 0x08B60804(object - 4, -1) is <= 0 (0x08B8542C(block, sp)). v0 = that first result.
void func_089CC068(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0S1); // s0, s1
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x089CC040u, {frame.at(4u), self});
    const u32 result = call(rt, ctx, 0x08B1ABB0u, {word(rt, frame.at(4u))});
    ctx.gpr[17] = result;
    const u32 pointer = word(rt, frame.at(4u));
    const u32 block = pointer - 0x0Cu;
    ctx.gpr[16] = block;
    if (block != 0x08D74FD0u && static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {block, frame.at(0u)});
    }
    ctx.gpr[2] = result;
}

// 0x08A6B518 (a0 = this): vtable 0x08CD2338, 0x08A6AFBC(this), then the block held at +0x1C is freed the same way as in
// func_089CC068 (block = word(this + 0x1C) - 0xC, skipped when it is 0x08D74FD0, freed when 0x08B60804(...) <= 0), and
// 0x08A72308(this) finishes. v0 = its result.
void func_08A6B518(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0S1); // s0, s1
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[17] = self;
    put(rt, self, 0x08CD2338u);
    ctx.gpr[2] = 0x08CD2338u;
    call(rt, ctx, 0x08A6AFBCu, {self});
    const u32 pointer = word(rt, self + 0x1Cu);
    const u32 block = pointer - 0x0Cu;
    ctx.gpr[16] = block;
    if (block != 0x08D74FD0u && static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {block, frame.at(0u)});
    }
    call(rt, ctx, 0x08A72308u, {self});
}

// 0x08A19D58 (a0 = this): vtable 0x08CD0658, the member at +0x24 released (0x08A5B9EC), the block held at +4 freed as in
// func_089CC068, and 0x0897D548(this) finishes. v0 = its result.
void func_08A19D58(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, kS0S1); // s0, s1
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CD0658u);
    ctx.gpr[2] = 0x08CD0658u;
    call(rt, ctx, 0x08A5B9ECu, {self + 0x24u});
    const u32 pointer = word(rt, self + 4u);
    const u32 block = pointer - 0x0Cu;
    ctx.gpr[17] = block;
    if (block != 0x08D74FD0u && static_cast<s32>(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {block, frame.at(0u)});
    }
    call(rt, ctx, 0x0897D548u, {self});
}

// 0x08B3A584 (a0, a1 passed on): 0x08B3DF80(this, a1, 2, 1) builds it; when the hook word 0x08D694F0 is set, it runs
// (this, a1, 1) as a tail call; v0 is the relocated `lui` half 0x08D70000 either way.
void func_08B3A584(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        SavedRegisters saved(ctx, kS0S1); // s0, s1
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        call(rt, ctx, 0x08B3DF80u, {self, argument, 2u, 1u});
    }
    const u32 hook = word(rt, 0x08D694F0u);
    ctx.gpr[2] = 0x08D70000u;  // `lui v0, 0x9 [HI]` (relocated): the call's result is not what v0 ends as
    if (hook != 0u) tail_call(ctx, hook, {self, argument, 1u});
}

// 0x08C73A28 (a0 = a holder whose word is the buffer, a1 = a character (sign-extended), a2 = an offset): looks the byte up
// in the rest of the buffer (0x08B79998) and answers its index relative to the buffer, or -1 when the offset is past the
// length stored at buffer - 0xC or the byte is not there.
void func_08C73A28(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const s32 character = static_cast<std::int8_t>(ctx.gpr[5]);
    const u32 offset = ctx.gpr[6];
    u32 result = 0xFFFFFFFFu;
    {
        SavedRegisters saved(ctx, kS0); // s0
        GuestFrame frame(ctx, 0x10u);
        const u32 buffer = word(rt, holder);
        ctx.gpr[16] = buffer;
        const u32 length = word(rt, buffer - 0x0Cu);
        if (offset < length) {
            const u32 found = call(rt, ctx, 0x08B79998u, {buffer + offset, static_cast<u32>(character), length - offset});
            if (found != 0u) result = found - buffer;
        }
    }
    ctx.gpr[2] = result;
}

// 0x088E642C (a0 = this holding the object, a2 and a3 passed on): the object's member at +0xEC is released, then its
// virtual at +0x50 (called with the object and -1) answers a record that receives a2 at +0x30 and a3 at +0x34; the
// object's member at +0xEC is released again (0x08B3457C, tail call).
void func_088E642C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    u32 owner;
    {
        SavedRegisters saved(ctx, kS0S2S3); // s0, s2, s3
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[18] = first;
        ctx.gpr[19] = second;
        call(rt, ctx, 0x08B344E0u, {word(rt, self) + 0xECu});
        owner = word(rt, self);
        const u32 function = word(rt, word(rt, owner) + 0x50u);
        const u32 record = call_indirect(rt, ctx, function, {owner, 0xFFFFFFFFu});
        if (record != 0u) {
            put(rt, record + 0x30u, first);
            put(rt, record + 0x34u, second);
        }
        owner = word(rt, self);
    }
    tail_call(ctx, 0x08B3457Cu, {owner + 0xECu});
}

} // namespace
} // namespace new_batch_9
} // namespace gt::game

#define NEW9(address, name, kind) GT_DECOMP(address, gt::game::new_batch_9::name, gt::ReturnKind::kind)
NEW9(0x0880ABACu, func_0880ABAC, Word);
NEW9(0x08A989B4u, func_08A989B4, Word);
NEW9(0x088F1F84u, func_088F1F84, Word);
NEW9(0x08A2E748u, func_08A2E748, Word);
NEW9(0x08936C84u, func_08936C84, Word);
NEW9(0x08AB9F60u, func_08AB9F60, Word);
NEW9(0x08B21BF8u, func_08B21BF8, Word);
NEW9(0x08B2FEB4u, func_08B2FEB4, Word);
NEW9(0x08B118A4u, func_08B118A4, Word);
NEW9(0x089BB87Cu, func_089BB87C, Word);
NEW9(0x08AA98E4u, func_08AA98E4, Word);
NEW9(0x08ABFC8Cu, func_08ABFC8C, Word);
NEW9(0x088F200Cu, func_088F200C, Word);
NEW9(0x0886F1F4u, func_0886F1F4, Word);
NEW9(0x0887EA1Cu, func_0887EA1C, Word);
NEW9(0x08A23E9Cu, func_08A23E9C, Word);
NEW9(0x08A1C49Cu, func_08A1C49C, Word);
NEW9(0x089F1120u, func_089F1120, Word);
NEW9(0x0891FD68u, func_0891FD68, Word);
NEW9(0x089CC068u, func_089CC068, Word);
NEW9(0x08A6B518u, func_08A6B518, Word);
NEW9(0x08A19D58u, func_08A19D58, Word);
NEW9(0x08B3A584u, func_08B3A584, Word);
NEW9(0x08C73A28u, func_08C73A28, Word);
NEW9(0x088E642Cu, func_088E642C, Word);
