// New batch main 2084 (main session): redrafts of functions the gate mismatched, and some new ones, written as structured C
// from `tools.disasm`. Unverified: the scripts verify them.
//  - 0x08B9xxxx / 0x08BAxxxx (seven): one-time initialisers of a small table, guarded by a byte flag each (0x08B6441C
//    decides, 0x08B64494 ends); they share `init_family`.
//  - 0x08A7E4F0 / 0x08A7E5E8: the same node copy (a vtable, a serial, two child lists).
//  - 0x08A8C828 / 0x08A8C72C: the same walk over a vector of pointers, then a fixed tail (they differ only in the pointer slot).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2084 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The callee-saved registers a function keeps values in: stored into its frame where its prologue stores them (a callee can
// read the stack below its caller's frame), given back when it returns.
class Saved {
public:
    Saved(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, std::initializer_list<std::pair<unsigned, u32>> slots)
        : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
        for (const auto &[reg, offset] : slots) put(rt, frame.at(offset), ctx.gpr[reg]);
    }
    ~Saved() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    Saved(const Saved &) = delete;
    Saved &operator=(const Saved &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// The one-time init of the object at `object`: nothing when its byte flag is set, else 0x08B6441C(object) decides.
bool once_begin(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (rt.memory().load8(object) != 0) return false;
    return call(rt, ctx, 0x08B6441Cu, {object}) != 0u;
}
// The end of a one-time init: 0x08B64494(object).
void once_end(Runtime &rt, AllegrexContext &ctx, u32 object) { call(rt, ctx, 0x08B64494u, {object}); }

// A node filled by an init: the vtable at +8, the payload at +4, and +0 cleared (in the original, a delay slot before the call).
void init_node(Runtime &rt, u32 node, u32 vtable, u32 payload) {
    put(rt, node + 8u, vtable);
    put(rt, node + 4u, payload);
    put(rt, node, 0u);
}

// The family of seven: the init of `g` first initialises node 0x08D752A8 (guarded by the flag at 0x08D750F0), stores that
// node at `tail`, then the init of `x` fills node `y`. v0 ends as `y`.
void init_family(Runtime &rt, AllegrexContext &ctx, u32 g, u32 tail, u32 x, u32 y, u32 y_vtable, u32 y_payload) {
    constexpr u32 kP = 0x08D750F0u;
    constexpr u32 kA = 0x08D752A8u;
    if (once_begin(rt, ctx, g)) {
        if (once_begin(rt, ctx, kP)) {
            init_node(rt, kA, 0x08CBF670u, 0x08D752B4u);
            once_end(rt, ctx, kP);
        }
        put(rt, tail, kA);
        once_end(rt, ctx, g);
    }
    if (once_begin(rt, ctx, x)) {
        init_node(rt, y, y_vtable, y_payload);
        once_end(rt, ctx, x);
    }
    ctx.gpr[2] = y;
}

// 0x08BA4064: the family with its tail at 0x08D75DE0 and the second node at 0x08D75DD4.
void func_08BA4064(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    init_family(rt, ctx, 0x08D750D8u, 0x08D75DE0u, 0x08D750D0u, 0x08D75DD4u, 0x08CC0FD8u, 0x08D75DE0u);
}

// 0x08B980D0: the family with its tail at 0x08D753E0 and the second node at 0x08D753D4.
void func_08B980D0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    init_family(rt, ctx, 0x08D750B8u, 0x08D753E0u, 0x08D750B0u, 0x08D753D4u, 0x08CBFCD0u, 0x08D753E0u);
}

// 0x08BA6A68: the family with its tail at 0x08D75E64 and the second node at 0x08D75E58.
void func_08BA6A68(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    init_family(rt, ctx, 0x08D750E8u, 0x08D75E64u, 0x08D750E0u, 0x08D75E58u, 0x08CC1850u, 0x08D75E64u);
}

// 0x08B95B58: the family with its tail at 0x08D7531C and the second node at 0x08D75310.
void func_08B95B58(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    init_family(rt, ctx, 0x08D750C8u, 0x08D7531Cu, 0x08D750C0u, 0x08D75310u, 0x08CBF92Cu, 0x08D7531Cu);
}

// 0x08B96C44: the family with its tail at 0x08D7538C and the second node at 0x08D75380.
void func_08B96C44(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    init_family(rt, ctx, 0x08D75078u, 0x08D7538Cu, 0x08D75070u, 0x08D75380u, 0x08CBF9F0u, 0x08D7538Cu);
}

// 0x08B95D48: the family with its tail at 0x08D75344 and the second node at 0x08D75338.
void func_08B95D48(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    init_family(rt, ctx, 0x08D75098u, 0x08D75344u, 0x08D75090u, 0x08D75338u, 0x08CBF934u, 0x08D75344u);
}

// 0x08B9363C: the family with its tail at 0x08D752C4 and the second node at 0x08D752B8.
void func_08B9363C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    init_family(rt, ctx, 0x08D75068u, 0x08D752C4u, 0x08D75060u, 0x08D752B8u, 0x08CBF670u, 0x08D752C4u);
}

// 0x08A7E4F0 / 0x08A7E5E8 (a0 = the new node, a1 = the node copied from): a vtable at +0, a serial from the counter at
// 0x08CFF05C, the copies of the children (0x08A6972C, 0x08A5BA08), then one list built from the source's list at +0x28 (walked
// through +8 and +0xC) and the source's word at +0x34.
void copy_node(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
    const u32 dst = ctx.gpr[4];
    const u32 src = ctx.gpr[5];
    call(rt, ctx, 0x08A782D8u, {dst, src});
    put(rt, dst, 0x08CB6DA8u);
    const u32 serial = word(rt, 0x08CFF05Cu);
    put(rt, 0x08CFF05Cu, serial + 1u);
    put(rt, dst + 0x14u, serial);
    call(rt, ctx, 0x08A6972Cu, {dst + 0x18u, src + 0x18u});
    const u32 extra = word(rt, src + 0x1Cu);
    put(rt, dst + 0x1Cu, extra);
    if (extra != 0u) call(rt, ctx, 0x08A5BA08u, {extra});
    const u32 s0 = dst + 0x20u;
    const u32 s2 = src + 0x20u;
    put(rt, s0 + 0x8u, 0u);
    const u32 a1 = word(rt, s2 + 0x8u);
    put(rt, s0 + 0x14u, 0u);
    put(rt, s0 + 0x4u, 0u);
    put(rt, s0 + 0xCu, dst + 0x24u);
    put(rt, s0 + 0x10u, dst + 0x24u);
    if (a1 != 0u) {
        const u32 head = call(rt, ctx, 0x08C27F70u, {s0});
        put(rt, dst + 0x28u, head);
        u32 last = head;
        while (word(rt, last + 0x8u) != 0u) last = word(rt, last + 0x8u);
        put(rt, dst + 0x2Cu, last);
        u32 chain = head;
        while (word(rt, chain + 0xCu) != 0u) chain = word(rt, chain + 0xCu);
        put(rt, s0 + 0x10u, chain);
        ctx.gpr[2] = word(rt, s2 + 0x14u);
        put(rt, s0 + 0x14u, ctx.gpr[2]);
    }
}


// 0x08A8C828 / 0x08A8C72C (a0 = this, a1 = the argument): for each element of the vector at (*(this + slot) + 0xC) call
// 0x08A65A54(arg, element) and 0x08A8C5D0(this, *element, result); then build the pointed-to record's list and return the
// result of 0x08A5BC54.
void walk_list(Runtime &rt, AllegrexContext &ctx, u32 s5_off) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame,
                {{21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x28u}});
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    const u32 vec = word(rt, self + s5_off + 0x4u) + 0xCu;
    u32 i = 0;
    for (;;) {
        const u32 begin = word(rt, vec);
        const u32 end = word(rt, vec + 0x4u);
        const u32 elem = begin + i * 4u;
        const u32 count = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
        const bool more = i < count;
        ++i;
        if (!more) break;
        const u32 value = word(rt, elem);
        const u32 result = call(rt, ctx, 0x08A65A54u, {arg, elem});
        call(rt, ctx, 0x08A8C5D0u, {self, value, result});
    }
    const u32 p = word(rt, self + s5_off + 0x4u);
    call(rt, ctx, 0x08B5C094u, {p});
    const u32 q = p + 0xCu;
    call(rt, ctx, 0x08C2AA40u, {word(rt, p + 0xCu), word(rt, q + 0x4u), ctx.gpr[29]});
    const u32 begin2 = word(rt, p + 0xCu);
    const s32 diff = static_cast<s32>(word(rt, q + 0x8u) - begin2) >> 2;
    if (begin2 != 0u) call(rt, ctx, 0x08A5BC54u, {begin2, static_cast<u32>(diff) << 2, 4u});
    call(rt, ctx, 0x08A69D68u, {p + 0x8u});
    call(rt, ctx, 0x08A5BC54u, {p, 0x18u, 4u});
}

void func_08A8C828(Runtime &rt, AllegrexContext &ctx) { walk_list(rt, ctx, 0x3Cu); }

// 0x08BE78E8 (a0 = the destination elements, a1 = how many, a2 = the element copied from): each element is 0x60 bytes —
// words 4 and 8, words 0xC..0x1C, the vtable 0x08CCFF88 at 0 — and each half (destination + 0x20 and + 0x40) is copied with
// 0x0896418C, the halves' word 0x10 taken from the source's 0x30 and 0x50. v0 ends as the source's 0x50 word of the last
// element copied; with no element copied it is the `lui` value 0x08CD4000 (or unchanged when the count is 0).
void func_08BE78E8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame,
                {{23u, 0x1Cu}, {17u, 0x4u}, {31u, 0x24u}, {30u, 0x20u}, {22u, 0x18u}, {21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu},
                 {18u, 0x8u}, {16u, 0x0u}});
    const u32 dest = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    if (count != 0u) {
        ctx.gpr[2] = 0x08CD4000u;
        for (u32 i = 0; i < count; ++i) {
            const u32 element = dest + i * 0x60u;
            if (element == 0u) continue;
            put(rt, element + 4u, word(rt, source + 4u));
            put(rt, element + 8u, word(rt, source + 8u));
            for (u32 offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, element + offset, word(rt, source + offset));
            const u32 first = element + 0x20u;
            const u32 second = element + 0x40u;
            const u32 source_first = source + 0x20u;
            const u32 source_second = source + 0x40u;
            put(rt, element, 0x08CCFF88u);
            call(rt, ctx, 0x0896418Cu, {first, source_first});
            put(rt, first + 0x10u, word(rt, source_first + 0x10u));
            call(rt, ctx, 0x0896418Cu, {second, source_second});
            ctx.gpr[2] = word(rt, source_second + 0x10u);
            put(rt, second + 0x10u, ctx.gpr[2]);
        }
    }
}

// 0x0881CAEC (a0 = this): the object at this+0 gets its vtable; a 0x448-byte block is allocated and cleared (30 calls to
// 0x088A79AC on its table at +0x354), stored at this+0x14, and the one-time table entry at 0x08D75130 goes to this+0x18.

// 0x08998560 (a0 = this): sets the vtables at +0 and +0x38, the float fields 0.3 and 10.0 at +0x124 / +0x128, the flags word at
// +0x12C (bits 0..4 become 0x10), clears 0x120..0x154 and the 2 records of 3 words at +0x158, copies two words from 0x08C8C260,
// then calls 0x08A76AA0(this + 0x180) and 0x089D8A54(this + 0x184) (v0 = the last one's result).
void func_08998560(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}});
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x089946BCu, {self});
    put(rt, self, 0x08CCBE20u);
    const u32 flags = word(rt, self + 0x12Cu);
    put(rt, self + 0x38u, 0x08CCC078u);
    put(rt, self + 0x124u, 0x3E99999Au);
    put(rt, self + 0x128u, 0x41200000u);
    put(rt, self + 0x12Cu, (flags & ~(0x7u | 0x18u)) | 0x10u);
    put(rt, self + 0x120u, 0u);
    for (u32 offset = 0x130u; offset <= 0x154u; offset += 4u) put(rt, self + offset, 0u);
    u32 record = self + 0x158u;
    for (u32 k = 0; k < 2u; ++k) {
        put(rt, record, 0u);
        put(rt, record + 4u, 0u);
        put(rt, record + 8u, 0u);
        record += 0xCu;
    }
    const u32 word_a = word(rt, 0x08C8C260u + 0x4u);
    const u32 word_b = word(rt, 0x08C8C260u);
    put(rt, self + 0x174u, word_a);
    put(rt, self + 0x170u, word_b);
    put(rt, self + 0x178u, 0x3F800000u);
    put(rt, self + 0x17Cu, 0u);
    call(rt, ctx, 0x08A76AA0u, {self + 0x180u});
    call(rt, ctx, 0x089D8A54u, {self + 0x184u});
    put(rt, self + 0x188u, 0u);
}

// 0x0892BD4C (a0 = this): the object at this+0x260 (0x9C bytes, vtable 0x08CC6740) is set up; the one-time entry 0x08D76A20
// gets its vtable on the first init; the record's word 0x98 becomes -1 and v0 = -1.
void func_0892BD4C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    Saved saved(rt, ctx, frame, {{31u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08B225FCu, {self});
    put(rt, self, 0x08CC6740u);
    const u32 node = call(rt, ctx, 0x08B62344u, {0x9Cu});
    call(rt, ctx, 0x0892991Cu, {node + 0x4u});
    call(rt, ctx, 0x0892B758u, {node + 0x78u});
    put(rt, node + 0x90u, 0u);
    put(rt, node + 0x94u, 0u);
    const u32 obj = 0x08D76A20u;
    put(rt, self + 0x260u, node);
    if (once_begin(rt, ctx, 0x08D76A18u)) {
        put(rt, obj, 0x08CC6768u);
        once_end(rt, ctx, 0x08D76A18u);
    }
    const u32 head = word(rt, self + 0x260u);
    put(rt, self + 0x260u + 0x4u, obj);
    put(rt, head, 0u);
    call(rt, ctx, 0x08929980u, {head + 0x4u});
    call(rt, ctx, 0x08B20BA0u, {head + 0x40u});
    call(rt, ctx, 0x08B20BA0u, {head + 0x5Cu});
    put(rt, head + 0x98u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x0887F600 (a0 = this): the vtables at +0 and +0x70, a 0x54-byte object at +0x74 built by four calls, the one-time entry
// 0x08D75E00 (vtable 0x08CC1198 on the first init) at +0x78; then two calls on this and the object, and a tail call.
void func_0887F600(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    Saved saved(rt, ctx, frame, {{31u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x0886AD04u, {self});
    put(rt, self, 0x08CC1130u);
    put(rt, self + 0x70u, 0x08CC116Cu);
    const u32 node = call(rt, ctx, 0x08B62344u, {0x54u});
    call(rt, ctx, 0x08884420u, {node});
    call(rt, ctx, 0x0886FC04u, {node + 0x14u});
    call(rt, ctx, 0x088710DCu, {node + 0x28u});
    call(rt, ctx, 0x08884AC8u, {node + 0x3Cu});
    put(rt, self + 0x74u, node);
    const u32 obj = 0x08D75E00u;
    if (once_begin(rt, ctx, 0x08D75DF8u)) {
        put(rt, obj, 0x08CC1198u);
        once_end(rt, ctx, 0x08D75DF8u);
    }
    put(rt, self + 0x74u + 0x4u, obj);
    call(rt, ctx, 0x0886AAA4u, {self, word(rt, self + 0x74u) + 0x3Cu});
    call(rt, ctx, 0x0886AAA4u, {self, word(rt, self + 0x74u)});
    tail_call(ctx, 0x0886AAA4u, {self, word(rt, self + 0x74u) + 0x28u});
}

// 0x088FF120 (a0 = out): a 0x64-byte object (vtable at its +0 from the one-time entry 0x08D76988 / 0x08D76990) is cleared
// (memset 0x08B36018 on its +0x44, 0x20 bytes), stored at *out, and the object's flags are set; then a tail call 0x08B28DCC.
void func_088FF120(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    Saved saved(rt, ctx, frame, {{31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
    const u32 self = ctx.gpr[4];
    const u32 node = call(rt, ctx, 0x08B62344u, {0x64u});
    put(rt, node + 0x30u, 0xFFFFFFFFu);
    for (u32 offset = 0; offset <= 0x2Cu; offset += 4u) put(rt, node + offset, 0u);
    put(rt, node + 0x34u, 0u);
    put(rt, node + 0x38u, 0u);
    put(rt, node + 0x3Cu, 0u);
    put(rt, node + 0x40u, 0u);
    call(rt, ctx, 0x08B36018u, {node + 0x44u, 0u, 0x20u});
    put(rt, self, node);
    const u32 obj = 0x08D76990u;
    if (once_begin(rt, ctx, 0x08D76988u)) {
        put(rt, obj, 0x08CC5370u);
        once_end(rt, ctx, 0x08D76988u);
    }
    put(rt, self + 0x4u, obj);
    rt.memory().store8(self + 0x9u, 1u);
    rt.memory().store8(self + 0x8u, 0u);
    rt.memory().store8(self + 0xAu, 0u);
    rt.memory().store8(self + 0xBu, 0u);
    rt.memory().store8(self + 0xCu, 0u);
    tail_call(ctx, 0x08B28DCCu, {self + 0x10u});
}

// 0x089E67FC (a0 = this): the vtables at +0 and +0x38; a check of the 0x1E0 pointer (a 0x08B60804 / 0x08B8542C pair unless it is
// the sentinel 0x08D74FD0); four calls 0x08A5B9EC on this's members; then a walk over the 12-byte entries of 0x168..0x180 with
// 0x08C02614 and 0x08A5BC54; v0 = the result of 0x0899AFCC(this).

// 0x08AA2E28 (a0 = this, a1 = count): a local word at sp+4 is set by 0x08AAB5DC; for each of the count entries 0x08AABA8C(local, .)
// (the second and later also repeat the first with 0x08CB2B58) and 0x08AAB8A0(result, index); then 0x08AAB540(local) feeds 0x08B88F44
// (this, result); a 0x08B60804 check on the local, and v0 = this.
void func_08AA2E28(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame,
                {{17u, 0x14u}, {21u, 0x24u}, {18u, 0x18u}, {31u, 0x28u}, {20u, 0x20u}, {19u, 0x1Cu}, {16u, 0x10u}});
    const u32 self = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 local = ctx.gpr[29] + 4u;
    call(rt, ctx, 0x08AAB5DCu, {local});
    if (count > 0) {
        const u32 limit = static_cast<u32>(count);
        for (u32 k = 0; k != limit; ++k) {
            if (k != 0u) call(rt, ctx, 0x08AABA8Cu, {local, 0x08CB2B58u});
            const u32 r = call(rt, ctx, 0x08AABA8Cu, {local, 0x08CB2B5Cu});
            call(rt, ctx, 0x08AAB8A0u, {r, k});
        }
    }
    const u32 r2 = call(rt, ctx, 0x08AAB540u, {local});
    call(rt, ctx, 0x08B88F44u, {self, r2});
    const u32 w = word(rt, local);
    const u32 s0 = w - 0xCu;
    if (s0 != 0x08D74FD0u) {
        const s32 result = static_cast<s32>(call(rt, ctx, 0x08B60804u, {w - 4u, 0xFFFFFFFFu}));
        if (result <= 0) call(rt, ctx, 0x08B8542Cu, {s0, ctx.gpr[29]});
    }
    ctx.gpr[2] = self;
}

} // namespace
} // namespace new_batch_main_2084
} // namespace gt::game

#define NEW2084(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2084::name, gt::ReturnKind::kind)
NEW2084(0x08AA2E28u, func_08AA2E28, Word);
NEW2084(0x08BA4064u, func_08BA4064, Word);
NEW2084(0x08B980D0u, func_08B980D0, Word);
NEW2084(0x08BA6A68u, func_08BA6A68, Word);
NEW2084(0x08B95B58u, func_08B95B58, Word);
NEW2084(0x08B96C44u, func_08B96C44, Word);
NEW2084(0x08B95D48u, func_08B95D48, Word);
NEW2084(0x08B9363Cu, func_08B9363C, Word);
NEW2084(0x08A8C828u, func_08A8C828, Word);
NEW2084(0x08BE78E8u, func_08BE78E8, Void);
NEW2084(0x08998560u, func_08998560, Word);
NEW2084(0x0892BD4Cu, func_0892BD4C, Word);
NEW2084(0x0887F600u, func_0887F600, Word);
NEW2084(0x088FF120u, func_088FF120, Word);
