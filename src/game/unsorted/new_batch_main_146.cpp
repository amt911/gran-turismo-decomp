// New batch main-146 (draft, unverified): one function of the five assigned; the other four (0x0888329C, 0x08C48488, 0x08AF6474,
// 0x088AFBA4) are not registered (float/FPU-flag heavy, jumps into other functions, 500+ instructions each).
// Written by hand from `tools.disasm` as structured C. A `jal`'s delay slot runs before the callee; callee-saved s0-s6 are put back at the end;
// `v0` ends as the asm leaves it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_146 {
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

// s0-s6 are callee-saved: the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 7u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 7u; ++i) ctx_.gpr[16u + i] = saved_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[7];
};

constexpr u32 kFatalNull = 0x08A5BB7Cu;       // called with a message when a pointer is null (the original goes on after it)
constexpr u32 kMessageNoOwner = 0x08CB293Cu;
constexpr u32 kMessageNoText = 0x08CB2944u;
constexpr u32 kStringDestroy = 0x08A5B9ECu;
constexpr u32 kEmptyRep = 0x08D74FD0u;        // the shared empty representation of the reference-counted string

// lw reg, 0(s4); a null owner reports kMessageNoOwner and the code goes on with the value it loaded.
u32 owner_of(Runtime &rt, AllegrexContext &ctx, u32 self) {
    const u32 owner = word(rt, self);
    if (owner == 0u) call(rt, ctx, kFatalNull, {kMessageNoOwner});
    return owner;
}

// The inline destructor of a reference-counted string whose data pointer is at `slot`: the representation sits 0xC before it; unless it is the
// shared empty one, a decrement of the count (at data - 4) that reaches 0 or less frees it (a1 = `scratch`, a stack slot).
void release_string(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 scratch) {
    const u32 data = word(rt, slot);
    const u32 rep = data - 0xCu;
    if (rep == kEmptyRep) return;
    const u32 count = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {rep, scratch});
}

// Sets up one named entry: a string built from a literal (a1 of the builder), then the owner's lookup with it.
// 0x08AA0DAC (a0 = result object, which is returned): fills a table of 38 callee results at 0x08CFF710 once (a guard byte at 0x08CFF708),
// registers each with the owner of a0, then makes a series of string-keyed registrations on the owner of a0.
void func_08AA0DAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x70u);
    const u32 text_a = frame.at(0x3Cu);
    const u32 text_b = frame.at(0x40u);

    call(rt, ctx, 0x08A9DD14u, {text_a});
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    call(rt, ctx, 0x08A7E914u, {object, 0x08CF6F38u});
    put(rt, frame.at(8u), object);
    call(rt, ctx, 0x08A76AB4u, {self, frame.at(8u)});

    // first name: 0x08A9BB40(out, text_a's data) -> 0x08C3055C(owner, 0x08CF7054, out)
    {
        const u32 owner = owner_of(rt, ctx, self);
        const u32 data = word(rt, text_a);
        if (data == 0u) call(rt, ctx, kFatalNull, {kMessageNoText});
        const u32 out = frame.at(0x38u);
        call(rt, ctx, 0x08A9BB40u, {out, data});
        call(rt, ctx, 0x08C3055Cu, {owner, 0x08CF7054u, out});
        call(rt, ctx, kStringDestroy, {out});
    }
    {
        const u32 owner = owner_of(rt, ctx, self);
        const u32 data = word(rt, text_a);
        if (data == 0u) call(rt, ctx, kFatalNull, {kMessageNoText});
        const u32 out = frame.at(0x34u);
        call(rt, ctx, 0x08A9BB98u, {out, data});
        call(rt, ctx, 0x08C3055Cu, {owner, 0x08CF6FF0u, out});
        call(rt, ctx, kStringDestroy, {out});
    }
    // two strings built from the literal at 0x08CB294C (1 byte at sp+5) and released, then registered
    {
        const u32 owner = owner_of(rt, ctx, self);
        const u32 built = frame.at(0x2Cu);
        const u32 out = frame.at(0x30u);
        call(rt, ctx, 0x08B85EB8u, {built, 0x08CB294Cu, frame.at(5u)});
        call(rt, ctx, 0x08A6D2D4u, {out, built});
        call(rt, ctx, 0x08C3062Cu, {owner, 0x08CB2950u, out});
        call(rt, ctx, kStringDestroy, {out});
        release_string(rt, ctx, built, frame.at(3u));
    }
    {
        const u32 owner = owner_of(rt, ctx, self);
        const u32 built = frame.at(0x24u);
        const u32 out = frame.at(0x28u);
        call(rt, ctx, 0x08B85EB8u, {built, 0x08CB295Cu, frame.at(4u)});
        call(rt, ctx, 0x08A6D2D4u, {out, built});
        call(rt, ctx, 0x08C3062Cu, {owner, 0x08CB2968u, out});
        call(rt, ctx, kStringDestroy, {out});
        release_string(rt, ctx, built, frame.at(2u));
    }
    {
        const u32 built = frame.at(0x20u);
        call(rt, ctx, 0x08B85EB8u, {built, 0x08CB2970u, frame.at(1u)});
        call(rt, ctx, 0x08A6D2D4u, {text_b, built});
        release_string(rt, ctx, built, frame.at(0u));
        const u32 owner = owner_of(rt, ctx, self);
        const u32 out = frame.at(0x1Cu);
        call(rt, ctx, 0x08A6C358u, {out, text_b});
        call(rt, ctx, 0x08C3062Cu, {owner, 0x08CB2974u, out});
        call(rt, ctx, kStringDestroy, {out});
    }

    // the once-only table (a function-local static: guard byte, acquire, fill, release)
    constexpr u32 kGuard = 0x08CFF708u;
    constexpr u32 kTable = 0x08CFF710u;
    static const u32 kProviders[] = {
        0x08A98330u, 0x08A97338u, 0x08A95A28u, 0x08A94454u, 0x08A9219Cu, 0x08A8A7F4u, 0x08A898C8u, 0x08A87684u, 0x08A86D80u, 0x08A85448u,
        0x08A82A5Cu, 0x08A80ED0u, 0x08A7BC7Cu, 0x08A76A94u, 0x08AB3D48u, 0x08A72CB4u, 0x08A6BF34u, 0x08A6ACA8u, 0x08A69514u, 0x08A61578u,
        0x08AB12F8u, 0x08A5F374u, 0x08A5D70Cu, 0x08AAFB74u, 0x08A5A484u, 0x08AAC464u, 0x08A59FBCu, 0x08A59AF4u, 0x08A59630u, 0x08A59160u,
        0x08A588CCu, 0x08A55878u, 0x08A55140u, 0x08A53074u, 0x08AABF9Cu, 0x08A52158u, 0x08A51C88u, 0x08AABAD4u,
    };
    constexpr unsigned kProviderCount = sizeof(kProviders) / sizeof(kProviders[0]);
    if (static_cast<std::int8_t>(rt.memory().load8(kGuard)) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {kGuard}) != 0u) {
            // each result is stored in the delay slot of the NEXT call, before it runs
            u32 result = call(rt, ctx, kProviders[0]);
            for (unsigned i = 1u; i < kProviderCount; ++i) {
                put(rt, kTable + 4u * (i - 1u), result);
                result = call(rt, ctx, kProviders[i]);
            }
            put(rt, kTable + 4u * (kProviderCount - 1u), result);
            call(rt, ctx, 0x08B64494u, {kGuard});
        }
    }
    ctx.gpr[2] = word(rt, kTable);
    if (ctx.gpr[2] != 0u) {
        u32 entry = kTable;
        u32 owner = word(rt, self);
        while (true) {
            if (owner == 0u) call(rt, ctx, kFatalNull, {kMessageNoOwner});
            const u32 current = entry;
            entry += 4u;
            call(rt, ctx, 0x08A7CC30u, {owner, word(rt, current)});
            ctx.gpr[2] = word(rt, current + 4u);
            if (ctx.gpr[2] == 0u) break;
            owner = word(rt, self);
        }
    }
    {
        u32 owner = word(rt, self);
        if (owner == 0u) call(rt, ctx, kFatalNull, {kMessageNoOwner});
        const u32 value = call(rt, ctx, 0x08A94454u);
        call(rt, ctx, 0x08A7CAD4u, {owner, 0x08CB297Cu, value});
    }
    // four objects built in place and handed to the owner
    static const u32 kBuilders[][2] = {
        {0x08AB69B0u, 0x18u}, {0x08AA30ECu, 0x14u}, {0x08AA5944u, 0x10u}, {0x08AA6D68u, 0x0Cu},
    };
    for (const auto &builder : kBuilders) {
        const u32 owner = owner_of(rt, ctx, self);
        const u32 out = frame.at(builder[1]);
        call(rt, ctx, builder[0], {out});
        call(rt, ctx, 0x08A7CE50u, {owner, out});
        call(rt, ctx, kStringDestroy, {out});
    }
    // string-keyed registrations on the owner: 0x08BE6154(owner, key, 0, handler) and 0x08B86FB0(owner, key, handler)
    static const u32 kWithZero[][2] = {
        {0x08CB2988u, 0x08AA0C74u}, {0x08CB299Cu, 0x08AA0C7Cu}, {0x08CB29B0u, 0x08AA0C84u},
    };
    for (const auto &entry : kWithZero) {
        const u32 owner = owner_of(rt, ctx, self);
        call(rt, ctx, 0x08BE6154u, {owner, entry[0], 0u, entry[1]});
    }
    static const u32 kPlain[][2] = {
        {0x08CB29C4u, 0x08AA0C8Cu}, {0x08CB29DCu, 0x08AA0C94u}, {0x08CB29F4u, 0x08AA0C9Cu}, {0x08CB2A04u, 0x08AA0CA4u}, {0x08CB2A14u, 0x08AA17F8u},
        {0x08CB2A20u, 0x08AA0CACu}, {0x08CB2A28u, 0x08AA0D0Cu}, {0x08CB2A30u, 0x08AA0D94u}, {0x08CB2A38u, 0x08AA15C0u},
    };
    for (const auto &entry : kPlain) {
        const u32 owner = owner_of(rt, ctx, self);
        call(rt, ctx, 0x08B86FB0u, {owner, entry[0], entry[1]});
    }
    call(rt, ctx, kStringDestroy, {text_b});
    call(rt, ctx, kStringDestroy, {text_a});
    ctx.gpr[2] = self;
}
} // namespace
} // namespace new_batch_main_146
} // namespace gt::game

GT_DECOMP(0x08AA0DACu, gt::game::new_batch_main_146::func_08AA0DAC, gt::ReturnKind::Word);
