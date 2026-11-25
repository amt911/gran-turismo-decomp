// Main-session batch 888: constructor-shaped bodies the earlier seeds emitted but nobody could compare, plus a few
// one-call forwarders. Structured C from tools.disasm, hand-written (no recompiler output).
// The bulk is one shape: base constructor over this, the vtable word stored at +0 and two words zeroed; v0 ends as
// the vtable constant, because the lui/addiu pair that materialises it is the last write to v0. The address the
// vtable constant describes is never read here (only stored), so the constant itself is what the gate sees in v0.
// Two functions (0x0882214C, 0x08822184) build a one-word stack record from a fixed .data word and hand it to the
// constructor as a1; that word is real game data, so the shapes file does not describe it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_888 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// s0 / s1 as the originals keep them: every body here saves them in its own frame slot and puts them back before it
// returns (or before its tail call), so the caller sees them untouched.
struct Saved {
    AllegrexContext &ctx;
    u32 s0;
    u32 s1;
    explicit Saved(AllegrexContext &c) : ctx(c), s0(c.gpr[16]), s1(c.gpr[17]) {}
    ~Saved() {
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
    }
};

// The constructor shape: base(this), then the vtable word at +0 and two zeroed members (the higher address first,
// as the asm stores them). v0 ends as the vtable constant; the base call's result is overwritten by the lui/addiu.
void construct(Runtime &rt, AllegrexContext &ctx, u32 base, u32 vtable, u32 zero_high, u32 zero_low,
               bool high_is_byte = false) {
    const u32 self = ctx.gpr[4];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, base, {self});
        put(rt, self + 0u, vtable);
        if (high_is_byte) {
            rt.memory().store8(self + zero_high, 0u);
        } else {
            put(rt, self + zero_high, 0u);
        }
        put(rt, self + zero_low, 0u);
    }
    ctx.gpr[2] = vtable;
}

// 0x0882214C / 0x08822184: the fixed .data word goes to the first stack slot, a1 = sp, then the base constructor
// takes (this, sp); v0 = this.
void construct_with_stack_word(Runtime &rt, AllegrexContext &ctx, u32 base, u32 source_word) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = self;
    put(rt, frame.at(0u), word(rt, source_word));
    call(rt, ctx, base, {self, frame.at(0u)});
    ctx.gpr[2] = self;
}

void func_0882214C(Runtime &rt, AllegrexContext &ctx) { construct_with_stack_word(rt, ctx, 0x08A53224u, 0x08CE2B1Cu); }
void func_08822184(Runtime &rt, AllegrexContext &ctx) { construct_with_stack_word(rt, ctx, 0x08A53224u, 0x08CE2B18u); }

void func_08854CAC(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x08A76ACCu, 0x08CBD238u, 0x18u, 0x14u); }

// The 0x0887E1E0 family: vtable at +0, members at +0x0C / +0x10.
void func_0886FA20(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0887E1E0u, 0x08CBF840u, 0x10u, 0x0Cu); }
void func_0886FBCC(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0887E1E0u, 0x08CBF868u, 0x10u, 0x0Cu); }
void func_0886FD78(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0887E1E0u, 0x08CBF890u, 0x10u, 0x0Cu); }
void func_0886FF24(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0887E1E0u, 0x08CBF8B8u, 0x10u, 0x0Cu); }
void func_0886FF5C(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0887E1E0u, 0x08CBF8B8u, 0x10u, 0x0Cu); }
void func_088700D0(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0887E1E0u, 0x08CBF8E0u, 0x10u, 0x0Cu); }
void func_08870108(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0887E1E0u, 0x08CBF8E0u, 0x10u, 0x0Cu); }

// The 0x0886AD04 family: vtable at +0, members at +0x70 / +0x74.
void func_0887E528(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0886AD04u, 0x08CC1020u, 0x74u, 0x70u); }
void func_0887E560(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0886AD04u, 0x08CC0FE8u, 0x74u, 0x70u); }
void func_0887E598(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0886AD04u, 0x08CC0FE8u, 0x74u, 0x70u); }
void func_0887E788(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0886AD04u, 0x08CC1058u, 0x74u, 0x70u); }
void func_0887E8B8(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x0886AD04u, 0x08CC1090u, 0x74u, 0x70u); }

// 0x0891C5DC: a bigger object, members at +0x1E0 / +0x1FC.
void func_0891C5DC(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x08926D60u, 0x08CC5D18u, 0x1FCu, 0x1E0u); }

// 0x08991634: the +0x18 member is a byte (`sb`), +0x14 a word.
void func_08991634(Runtime &rt, AllegrexContext &ctx) {
    construct(rt, ctx, 0x08A76ACCu, 0x08CCC620u, 0x18u, 0x14u, true);
}

void func_089FD9B0(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x08A76ACCu, 0x08CB8D28u, 0x18u, 0x14u); }
void func_08A31848(Runtime &rt, AllegrexContext &ctx) { construct(rt, ctx, 0x08A76ACCu, 0x08CD14C0u, 0x18u, 0x14u); }

// 0x089FAF9C: base(a1) — the `addu a0, a1, zero` sits in the call's delay slot, so the callee sees a0 = a1 and a1
// unchanged — then (this, base's v0), and v0 = this.
void func_089FAF9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 built = call(rt, ctx, 0x089FAEC0u, {argument});
    call(rt, ctx, 0x089F9ED4u, {self, built});
    ctx.gpr[2] = self;
}

// 0x08A17C10: base(this), then five words zeroed (+0x14 first); v0 stays the base call's result.
void func_08A17C10(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 result = call(rt, ctx, 0x08A17B28u, {self});
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x04u, 0u);
    put(rt, self + 0x08u, 0u);
    put(rt, self + 0x0Cu, 0u);
    put(rt, self + 0x10u, 0u);
    ctx.gpr[2] = result;
}

// 0x08A26D30: base(this), then +0x34 and +0x28 zeroed as words, +0x2C / +0x2D as bytes and +0x30 as a word; v0 is
// the base call's result.
void func_08A26D30(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 result = call(rt, ctx, 0x08B28DCCu, {self});
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x28u, 0u);
    rt.memory().store8(self + 0x2Cu, 0u);
    rt.memory().store8(self + 0x2Du, 0u);
    put(rt, self + 0x30u, 0u);
    ctx.gpr[2] = result;
}

// 0x0880BF34: three member destructors (+0x208, +0x30, +0x28 of this), then the frame is dropped and the object
// destructor at 0x08B28DC4 runs as a tail call.

// 0x0885E25C: the a2 byte is masked before the call (`andi s1, a2, 0xFF`), then the call's v0 becomes a0 of the tail
// call, with the saved a1 and the masked a2 behind it.
void func_0885E25C(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 third = ctx.gpr[6] & 0xFFu;
    u32 result;
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = second;
        ctx.gpr[17] = third;
        result = call(rt, ctx, 0x088CA548u, {first});
    }
    tail_call(ctx, 0x088C97A4u, {result, second, third});
}

// 0x0888FEB4: the callee's v0 is the base of a table at +0xDC whose entries are 32 bytes per a1 and byte-swapped
// both ends (`wsbw v1, v1` on the base and `wsbw v0, v0` in the return delay slot).
void func_0888FEB4(Runtime &rt, AllegrexContext &ctx) {
    const u32 index = ctx.gpr[5];
    const u32 base = [&] {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = index;
        return call(rt, ctx, 0x0888F550u, {ctx.gpr[4]});
    }();
    const u32 table = __builtin_bswap32(word(rt, base + 0xDCu));
    ctx.gpr[2] = __builtin_bswap32(word(rt, table + (index << 5)));
}

// 0x08892B68: memset-shaped base(this, 0, 0x26D0), then two -1 sentinels at +0x22A8 / +0x22AC; v0 is the base's.
void func_08892B68(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[16] = self;
    const u32 result = call(rt, ctx, 0x08B36018u, {self, 0u, 0x26D0u});
    put(rt, self + 0x22A8u, 0xFFFFFFFFu);
    put(rt, self + 0x22ACu, 0xFFFFFFFFu);
    ctx.gpr[2] = result;
}

// 0x0890B0B8: s0 = word(a0) is the object whose bit `call(a1)` names is tested; v0 = (bit != 0), one or zero.
void func_0890B0B8(Runtime &rt, AllegrexContext &ctx) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 object = word(rt, ctx.gpr[4]);
    ctx.gpr[16] = object;
    const u32 bit = call(rt, ctx, 0x0890B06Cu, {ctx.gpr[5]});
    const u32 bits = word(rt, object);
    const u32 mask = 1u << (bit & 31u);
    ctx.gpr[2] = (bits & mask) != 0u ? 1u : 0u;
}

// 0x0892A614 / 0x0892A650 (same body): base(this + 4), then the tail call gets (this, a1) again.
void func_0892A614(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        call(rt, ctx, 0x0892A604u, {self + 0x04u});
    }
    tail_call(ctx, 0x0892A58Cu, {self, argument});
}

void func_0892A650(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        ctx.gpr[17] = argument;
        call(rt, ctx, 0x0892A604u, {self + 0x04u});
    }
    tail_call(ctx, 0x0892A58Cu, {self, argument});
}

// 0x089326F4: the object (+0x40, +0x44) is turned into an index by 0x089363FC, then 0x08936584 fills the stack
// record at sp from (that index, the a1 the callee left, the two members) and the byte at +0x24 of the record is
// the answer. a1 has to come from the first callee — the original never reloads it between the two calls.
void func_089326F4(Runtime &rt, AllegrexContext &ctx) {
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[16] = ctx.gpr[4];
    const GuestResult keyed = call_guest(rt, ctx, 0x089363FCu, {ctx.gpr[4]}, {});
    const u32 first = word(rt, ctx.gpr[16] + 0x40u);
    const u32 second = word(rt, ctx.gpr[16] + 0x44u);
    call_guest(rt, ctx, 0x08936584u, {keyed.v0, keyed.a1, first, second, frame.at(0u)}, {});
    ctx.gpr[2] = rt.memory().load8(frame.at(0x24u));
}

} // namespace
} // namespace new_batch_main_888
} // namespace gt::game

#define NEW888(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_888::name, gt::ReturnKind::kind)
NEW888(0x0882214Cu, func_0882214C, Word);
NEW888(0x08822184u, func_08822184, Word);
NEW888(0x08854CACu, func_08854CAC, Word);
NEW888(0x0886FA20u, func_0886FA20, Word);
NEW888(0x0886FBCCu, func_0886FBCC, Word);
NEW888(0x0886FD78u, func_0886FD78, Word);
NEW888(0x0886FF24u, func_0886FF24, Word);
NEW888(0x0886FF5Cu, func_0886FF5C, Word);
NEW888(0x088700D0u, func_088700D0, Word);
NEW888(0x08870108u, func_08870108, Word);
NEW888(0x0887E528u, func_0887E528, Word);
NEW888(0x0887E560u, func_0887E560, Word);
NEW888(0x0887E598u, func_0887E598, Word);
NEW888(0x0887E788u, func_0887E788, Word);
NEW888(0x0887E8B8u, func_0887E8B8, Word);
NEW888(0x0891C5DCu, func_0891C5DC, Word);
NEW888(0x08991634u, func_08991634, Word);
NEW888(0x089FAF9Cu, func_089FAF9C, Word);
NEW888(0x089FD9B0u, func_089FD9B0, Word);
NEW888(0x08A17C10u, func_08A17C10, Word);
NEW888(0x08A26D30u, func_08A26D30, Word);
NEW888(0x08A31848u, func_08A31848, Word);
NEW888(0x0885E25Cu, func_0885E25C, Word);
NEW888(0x0888FEB4u, func_0888FEB4, Word);
NEW888(0x08892B68u, func_08892B68, Word);
NEW888(0x0890B0B8u, func_0890B0B8, Word);
NEW888(0x0892A614u, func_0892A614, Word);
NEW888(0x0892A650u, func_0892A650, Word);
NEW888(0x089326F4u, func_089326F4, Word);
