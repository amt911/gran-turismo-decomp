// Main-session batch 2082 (src/game/unsorted/new_batch_main_2082.cpp): structured C from tools.disasm, drafted, unverified.
// Families: the guarded singleton-slot initialisers (base ctor, vtable, owned block, one-time guard at a global), the
// get-or-create singletons, and a handful of one-off constructors and refcount helpers.
// Skipped (not drafted): 0x08AE16EC (dynamic stack alloca), 0x08B40F28 (indirect loop, v1 clobbered by the callee),
// 0x089A8F34 (s0 not restored on one exit), 0x0885EAFC (a0 read after a call), 0x08AE1498 (large float frame).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2082 {
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

// Saves the caller's callee-saved registers selected by `mask` (bit n = s_n) and restores them on scope exit.
class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, u32 mask) : ctx_(ctx), mask_(mask) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i)
            if (mask_ & (1u << i)) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 mask_;
    u32 saved_[8];
};

// The one-time guard: a byte at `guard` (signed, lb) says the slot is set up; otherwise the guard is acquired (0x08B6441C),
// the slot gets its vtable and the guard is released (0x08B64494). `hi` is the lui half v0 keeps when nothing is acquired.
struct GuardedInit {
    u32 base;           // base constructor called with self (0 = none)
    u32 vtable;         // word stored at self
    u32 size;           // allocation size (a0 of the allocator)
    u32 inner;          // call on the block (or block + inner_adjust)
    u32 inner_adjust;
    u32 block_offset;   // the block is stored at self + block_offset, the slot at block_offset + 4
    u32 guard;
    u32 slot;
    u32 slot_vtable;
    bool zero_second;   // self + 4 is cleared before the allocation
};

void guarded_init(Runtime &rt, AllegrexContext &ctx, const GuardedInit &spec) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 7u);  // s0, s1, s2
    GuestFrame frame(ctx, 0x10u);
    if (spec.base != 0u) call(rt, ctx, spec.base, {self});
    put(rt, self, spec.vtable);
    if (spec.zero_second) put(rt, self + 4u, 0u);
    const u32 block = call(rt, ctx, 0x08B62344u, {spec.size});
    call(rt, ctx, spec.inner, {block + spec.inner_adjust});
    put(rt, self + spec.block_offset, block);
    if (static_cast<std::int8_t>(rt.memory().load8(spec.guard)) != 0) {
        ctx.gpr[2] = spec.slot & 0xFFFF0000u;  // lui half of the slot address, left in v0
    } else if (call(rt, ctx, 0x08B6441Cu, {spec.guard}) != 0u) {
        ctx.gpr[2] = spec.slot_vtable;         // lui + addiu leave the vtable in v0
        put(rt, spec.slot, spec.slot_vtable);
        call(rt, ctx, 0x08B64494u, {spec.guard});
    }
    put(rt, self + spec.block_offset + 4u, spec.slot);
}

// Get-or-create singleton: when the slot at `slot` is empty, a block is allocated, built by `inner`, stored, registered
// (0x08A5BA28), created through 0x08A9DD14 into a local; a null result asserts `assertion`; 0x08A9BACC finishes it with
// `argument`. Then the slot's value goes to 0x08A76AB4 with the caller's object.
struct SingletonSpec {
    u32 slot;
    u32 size;
    u32 inner;
    u32 assertion;
    u32 argument;
};

void singleton_join(Runtime &rt, AllegrexContext &ctx, const SingletonSpec &spec) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 7u);  // s0, s1, s2
    GuestFrame frame(ctx, 0x20u);
    if (word(rt, spec.slot) == 0u) {
        const u32 block = call(rt, ctx, 0x08A5BC20u, {spec.size, 4u});
        call(rt, ctx, spec.inner, {block});
        put(rt, spec.slot, block);
        call(rt, ctx, 0x08A5BA28u, {block});
        call(rt, ctx, 0x08A9DD14u, {frame.at(0u)});
        const u32 created = word(rt, frame.at(0u));
        if (created == 0u) call(rt, ctx, 0x08A5BB7Cu, {spec.assertion});
        call(rt, ctx, 0x08A9BACCu, {created, spec.argument});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    }
    put(rt, frame.at(4u), word(rt, spec.slot));
    call(rt, ctx, 0x08A76AB4u, {self, frame.at(4u)});
}

}  // namespace

// 0x08AB4950: a key record filled by 0x08B85EB8, a 0x28-byte block built by 0x08AB481C, the object's first word compared
// with 0x08D74FD0; the release through 0x08B8542C runs when 0x08B60804 does not answer positive.

// 0x08AE25BC: counts the live entries of the list headed at word(0x08D4067C) that 0x08ADDCD4 accepts; when a0 is set
// the accepted entries are written to it. v0 = the count.
void func_08AE25BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    SavedRegisters saved(ctx, 0xFu);  // s0, s1, s2, s3
    GuestFrame frame(ctx, 0x20u);
    u32 count = 0u;
    call(rt, ctx, 0x08AEB808u, {out});
    const u32 head = word(rt, 0x08D4067Cu);
    u32 node = static_cast<s32>(word(rt, head + 8u)) < 0 ? 0u : head;
    u32 cursor = out;
    while (node != 0u) {
        if (call(rt, ctx, 0x08ADDCD4u, {word(rt, node + 8u), 0u}) != 0u) {
            ++count;
            if (out != 0u) put(rt, cursor, word(rt, node + 8u));
            cursor += 4u;
        }
        node = word(rt, node + 4u);
        if (static_cast<s32>(word(rt, node + 8u)) < 0) node = 0u;
    }
    call(rt, ctx, 0x08AEB82Cu, {});
    ctx.gpr[2] = count;
}

// 0x08A250A8: the guarded singleton at 0x08D77138 owning an 0x8C-byte block (0x08CD06F8 vtable at the object).
void func_08A250A8(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CD06F8u, 0x8Cu, 0x08A24D04u, 0u, 0x14u, 0x08D77138u, 0x08D77140u, 0x08CD0DE8u, false});
}

// 0x08A30968: get-or-create singleton at 0x08D77154 (0x14 bytes).
void func_08A30968(Runtime &rt, AllegrexContext &ctx) {
    singleton_join(rt, ctx, SingletonSpec{0x08D77154u, 0x14u, 0x08A2DED0u, 0x08CAC388u, 0x08C1A988u});
}

// 0x08843180: guarded init, 8-byte block, slot 0x08D75188.
void func_08843180(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBD9A8u, 8u, 0x0888EC10u, 0u, 0x14u, 0x08D75180u, 0x08D75188u, 0x08CBEBD8u, false});
}

// 0x0883A194: guarded init, 0x20-byte block, slot 0x08D75178.
void func_0883A194(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBDAB8u, 0x20u, 0x08931B74u, 0u, 0x14u, 0x08D75170u, 0x08D75178u, 0x08CBEBA8u, false});
}

// 0x089289D0 (a0 = self, a1 = record): the chain of predicates 0x088D4C9C, 0x089273B4, 0x089277BC, 0x089275A8 (each must
// answer, the last non-negative), then 0x088DA5F4(record); when that word at +0x10 is zero the call becomes 0x089281E4(self, 0).
void func_089289D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 record = ctx.gpr[5];
    bool tail = false;
    {
        SavedRegisters saved(ctx, 3u);  // s0, s1
        GuestFrame frame(ctx, 0x10u);
        u32 answer = call(rt, ctx, 0x088D4C9Cu, {record + 4u});
        if (answer != 0u) {
            answer = call(rt, ctx, 0x089273B4u, {self});
            if (answer != 0u) {
                answer = call(rt, ctx, 0x089277BCu, {self});
                if (answer != 0u) {
                    answer = call(rt, ctx, 0x089275A8u, {self});
                    if (static_cast<s32>(answer) >= 0) {
                        const u32 pointer = call(rt, ctx, 0x088DA5F4u, {record, answer});
                        if (pointer != 0u) {
                            ctx.gpr[5] = 0u;
                            tail = word(rt, pointer + 0x10u) == 0u;
                        }
                    }
                }
            }
        }
    }
    if (tail) tail_call(ctx, 0x089281E4u, {self, 0u});
}

// 0x08845524: guarded init, 8-byte block, slot 0x08D751A8.
void func_08845524(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBD788u, 8u, 0x08893F7Cu, 0u, 0x14u, 0x08D751A0u, 0x08D751A8u, 0x08CBEC38u, false});
}

// 0x088462C4: guarded init, 8-byte block, slot 0x08D751B8.
void func_088462C4(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBD678u, 8u, 0x08894444u, 0u, 0x14u, 0x08D751B0u, 0x08D751B8u, 0x08CBED68u, false});
}

// 0x08848C50: guarded init, 8-byte block, slot 0x08D751C8.
void func_08848C50(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBD568u, 8u, 0x088D65DCu, 0u, 0x14u, 0x08D751C0u, 0x08D751C8u, 0x08CBED98u, false});
}

// 0x088443C8: guarded init, 0x88-byte block, slot 0x08D75198.
void func_088443C8(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBD898u, 0x88u, 0x088D441Cu, 0u, 0x14u, 0x08D75190u, 0x08D75198u, 0x08CBEC08u, false});
}

// 0x0881A504: guarded init, 0x39E4-byte block (its inner call takes block + 4), slot 0x08D75120.
void func_0881A504(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBE1F0u, 0x39E4u, 0x08B89CD4u, 4u, 0x14u, 0x08D75118u, 0x08D75120u, 0x08CBE310u, false});
}

// 0x0884F50C: guarded init, 0x30-byte block, slot 0x08D751D8.
void func_0884F50C(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBD018u, 0x30u, 0x08897B8Cu, 0u, 0x14u, 0x08D751D0u, 0x08D751D8u, 0x08CBEDC8u, false});
}

// 0x088745AC: guarded init on the 0x0887E1E0 base, 0x1C-byte block stored at +0xC, slot 0x08D753D0.
void func_088745AC(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x0887E1E0u, 0x08CBFA98u, 0x1Cu, 0x08874338u, 0u, 0xCu, 0x08D753C8u, 0x08D753D0u, 0x08CBFAC0u, false});
}

// 0x088812F8: guarded init on the 0x0887E1E0 base, 0x40-byte block stored at +0xC, slot 0x08D75E20.
void func_088812F8(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x0887E1E0u, 0x08CC12A0u, 0x40u, 0x088811F8u, 0u, 0xCu, 0x08D75E18u, 0x08D75E20u, 0x08CC1378u, false});
}

// 0x08A2DB78: guarded init, 0x28-byte block, slot 0x08D77150.
void func_08A2DB78(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CBE6A0u, 0x28u, 0x08B09F98u, 0u, 0x14u, 0x08D77148u, 0x08D77150u, 0x08CD1E00u, false});
}

// 0x08A3E74C: guarded init, 0x1C-byte block, slot 0x08D77178.
void func_08A3E74C(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0x08A76ACCu, 0x08CD13B0u, 0x1Cu, 0x08B0B06Cu, 0u, 0x14u, 0x08D77170u, 0x08D77178u, 0x08CD1E80u, false});
}

// 0x088F2678: no base constructor; self + 4 cleared before the 0x9D0-byte block (stored at +8), slot 0x08D76010 at +0xC.
void func_088F2678(Runtime &rt, AllegrexContext &ctx) {
    guarded_init(rt, ctx, GuardedInit{0u, 0x08CC3B48u, 0x9D0u, 0x088F1BB8u, 0u, 8u, 0x08D76008u, 0x08D76010u, 0x08CC3B78u, true});
}

// 0x089C9B84: get-or-create singleton at 0x08D76DA4 (0x80 bytes).
void func_089C9B84(Runtime &rt, AllegrexContext &ctx) {
    singleton_join(rt, ctx, SingletonSpec{0x08D76DA4u, 0x80u, 0x089C99DCu, 0x08CA6398u, 0x08C01CE4u});
}

// 0x08A36B8C: get-or-create singleton at 0x08D7715C (0x28 bytes).
void func_08A36B8C(Runtime &rt, AllegrexContext &ctx) {
    singleton_join(rt, ctx, SingletonSpec{0x08D7715Cu, 0x28u, 0x08A35594u, 0x08CACEFCu, 0x08C1B2BCu});
}

// 0x0886A3A4 (a0 = self, a1 = other pointer pair): the held reference of other is taken on the 0x4238 member, the call
// 0x0886BB40 takes it over, and the old word is released (the vtable's +8, then +0xC when its count also drops).

// 0x08ABF24C (a0 = self, a1 = value): the object's vtable and a 1.0 at +8, 16 zeroed 16-byte rows from +0x1C, the two
// members 0x08B28DCC(self + 0x11C) and 0x08ABF694(self + 0x144), the word at +0x154 cleared, 0x08B344E0(self + 0x11C), then
// 16 rows get `value` in their first word, and 0x08B3457C(self + 0x11C) is the tail call.
void func_08ABF24C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 rows = self + 0x1Cu;
    const u32 pair = self + 0x11Cu;
    {
        SavedRegisters saved(ctx, 0xFu);  // s0, s1, s2, s3
        GuestFrame frame(ctx, 0x20u);
        put(rt, self, 0x08CD6390u);
        put(rt, self + 8u, 0x3F800000u);  // 1.0f
        put(rt, self + 4u, 0u);
        for (u32 row = 0u; row < 16u; ++row) {
            for (u32 word_index = 0u; word_index < 4u; ++word_index) put(rt, rows + row * 0x10u + word_index * 4u, 0u);
        }
        call(rt, ctx, 0x08B28DCCu, {pair});
        call(rt, ctx, 0x08ABF694u, {self + 0x144u});
        put(rt, self + 0x154u, 0u);
        call(rt, ctx, 0x08B344E0u, {pair});
        for (u32 row = 0u; row < 16u; ++row) put(rt, rows + row * 0x10u, value);
    }
    tail_call(ctx, 0x08B3457Cu, {pair});
}

// 0x088B4CF4 (a0 = self): clears the object's words and the three 0x48-byte rows at +0x304 (their +0x40 word), then
// 0x088B5D24(self + 0x260) with the loop's last row in a1 and -1 in a2 and a3; v0 = its answer.
void func_088B4CF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx, 3u);  // s0, s1
    GuestFrame frame(ctx, 0x10u);
    put(rt, self, 0x08CC31B8u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, self + 0x14u, 0xFFFFFFFFu);
    put(rt, self + 0xCu, 0u);
    put(rt, self + 0x10u, 0u);
    for (u32 offset = 0x18u; offset <= 0x34u; offset += 4u) put(rt, self + offset, 0u);
    put(rt, self + 0x38u, 0u);
    u32 row = self + 0x304u;
    for (u32 i = 0u; i < 3u; ++i, row += 0x48u) put(rt, row + 0x40u, 0u);
    const u32 record = self + 0x260u;
    put(rt, record + 0x188u, 0u);
    call(rt, ctx, 0x088B5D24u, {record, row, 0xFFFFFFFFu, 0xFFFFFFFFu});
    put(rt, record + 0x198u, 0u);
    put(rt, record + 0x18Cu, 0u);
    rt.memory().store8(self + 0x3FCu, 0u);
}

// 0x089FA130 (a0 = self, a1 = other): the object is set to a 0x10-byte block 0x08B35840(0x10, 0x20) with the end at +8,
// then 0x08BE6028(block, 1, four 1.0f floats, sp) with -1 in its stack argument; the object's links are restored and,
// when the other object differs, 0x089641C4(s2, other) runs and its +0x10 word is copied.
void func_089FA130(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    SavedRegisters saved(ctx, 0xFu);  // s0, s1, s2, s3
    GuestFrame frame(ctx, 0x50u);
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    const u32 block = call(rt, ctx, 0x08B35840u, {0x10u, 0x20u});
    put(rt, self, block);
    put(rt, self + 4u, block);
    put(rt, self + 8u, block + 0x20u);
    put(rt, frame.at(0x10u), 0x3F800000u);
    put(rt, frame.at(0x1Cu), 0x3F800000u);
    put(rt, frame.at(0x18u), 0x3F800000u);
    put(rt, frame.at(0x14u), 0x3F800000u);
    put(rt, frame.at(0x20u), 0xFFFFFFFFu);
    call(rt, ctx, 0x08BE6028u, {block, 1u, frame.at(0x10u), frame.at(0u)});
    const u32 links = word(rt, self);
    put(rt, self + 0xCu, 0xFFFFFFFFu);
    ctx.gpr[2] = links + 0x20u;
    put(rt, self + 4u, links + 0x20u);
    if (links != other) {
        call(rt, ctx, 0x089641C4u, {links, other});
        put(rt, links + 0x10u, word(rt, other + 0x10u));
    }
}

// 0x08A99AB4 (a0 = self, a1 = other): self's vtable 0x08CB7108, then the byte range of other's [+0x14, +0x18) is copied into
// a block from 0x08A5BC20 (size rounded down to 4) and 0x08C2DC4C(source, end, block, sp) runs; the flag at +0x20 is cleared.

// Registration: one entry per address.
#define NEW2082(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2082::name, gt::ReturnKind::kind)
NEW2082(0x08AE25BCu, func_08AE25BC, Word);
NEW2082(0x08A250A8u, func_08A250A8, Word);
NEW2082(0x08A30968u, func_08A30968, Word);
NEW2082(0x08843180u, func_08843180, Word);
NEW2082(0x0883A194u, func_0883A194, Word);
NEW2082(0x089289D0u, func_089289D0, Word);
NEW2082(0x088462C4u, func_088462C4, Word);
NEW2082(0x088812F8u, func_088812F8, Word);
NEW2082(0x08A2DB78u, func_08A2DB78, Word);
NEW2082(0x089C9B84u, func_089C9B84, Word);
NEW2082(0x08A36B8Cu, func_08A36B8C, Word);
NEW2082(0x088443C8u, func_088443C8, Word);
NEW2082(0x08848C50u, func_08848C50, Word);
NEW2082(0x0881A504u, func_0881A504, Word);
NEW2082(0x0884F50Cu, func_0884F50C, Word);
NEW2082(0x08845524u, func_08845524, Word);
NEW2082(0x088745ACu, func_088745AC, Word);
NEW2082(0x088F2678u, func_088F2678, Word);
NEW2082(0x08A3E74Cu, func_08A3E74C, Word);
NEW2082(0x08ABF24Cu, func_08ABF24C, Word);
NEW2082(0x088B4CF4u, func_088B4CF4, Word);
NEW2082(0x089FA130u, func_089FA130, Word);

}  // namespace
}  // namespace gt::game
