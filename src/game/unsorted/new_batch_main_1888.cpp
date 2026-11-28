// Main-session batch 922: functions previously skipped twice by drafters ("no comparable synthetic case yet") or
// whose older draft differed from the asm, rewritten as structured C from `tools/disasm`. Shapes: virtual-call
// forwarders (the Adhoc "as class" pair), destructor-shaped vtable stores with member calls and a tail call,
// global-state resets, small array/ring loops, and a VFPU column copy. No names: none has evidence.
// DRAFT ONLY: the differential gate has not run yet.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1888 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; v0 afterwards holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// s0-s1 are callee-saved: the body sets them like the original and the caller's values come back at return.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx), s0_(ctx.gpr[16]), s1_(ctx.gpr[17]) {}
    ~SavedRegisters() {
        ctx_.gpr[16] = s0_;
        ctx_.gpr[17] = s1_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 s0_;
    u32 s1_;
};

// ---- 0x08B19FB0 (a0 = this): 0x08B34888(this), the byte at this + 0x2C set to 1, 0x08B34CA0(this), then
// 0x08B34B24(this) finishing. ----

// ---- 0x08B617A8 (a0-a2): 0x08B61664(a0, a1, a2), then 0x08B61484(its result, a1, a2) finishing; s0 keeps a1,
// s1 keeps a2. ----

// ---- 0x0883A45C: when the byte at 0x08CE2C20 is zero, 0x08930864() and 0x08930844() run and the byte is set to
// 1; the `lui s0, 0x0` the two 0x6720 accesses are relative to holds 0x08CDC500 (the .data base). v0 = the byte
// when it was already set, else the second call's result. ----

// ---- 0x088BBE0C (a0 ignored, a1 = key): 0x088BBC40(key); when it finds something, 0x088BBDB8(key) answers an
// object and f0 = its float at +0x14, otherwise f0 = 0. ----

// ---- 0x08999AD4 (a0 = out holder, a1 = source): 0x089FC5D0(out, 0x08CF6ED8) resets the output, then
// 0x08999A28(out, source) finishing. ----

// ---- 0x08A1C370: copy 0x08A76AB4, cast 0x08A1C264. ----

// ---- 0x08A23C8C: copy 0x08A76AB4, cast 0x08A23BE0. ----

// ---- 0x08A61280 (a0 = this): stores the vtable word 0x08CD2468, 0x08C1F510(this + 0x18), releases the holder at
// this + 0x14 and 0x08A780B0(this) finishes. ----

// ---- 0x08A8DB4C (a0 = this): fills a 0x20-byte local (0x08A75BDC(local, a1)), 0x08A8DAB4(this, local, 1), then
// releases the local (0x08A75A88(local)); v0 is the last call's. ----
void func_08A8DB4C(Runtime &rt, AllegrexContext &ctx) { GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    const u32 local = frame.at(0u);
    call(rt, ctx, 0x08A75BDCu, {local, ctx.gpr[5]});
    call(rt, ctx, 0x08A8DAB4u, {self, local, 1u});
    call(rt, ctx, 0x08A75A88u, {local});
}

// ---- 0x08AB57A4: copy 0x08A7026C, cast 0x08AB56F8. ----

// ---- 0x08B2E064: r = 0x08C7A9E8(); r == 0x80020064 tails to 0x08B32A34(0x7D0), otherwise 0x08C7A988(0x7D0).
// The `addiu a0, 0x7D0` sits in the `bne` delay slot, so a0 = 0x7D0 on both paths. ----

// ---- 0x08B2E9F8: releases the global at 0x08D690F8 (flag 0x08D690F4 cleared first) and clears the pointer; the
// `lui v0, 0x9` leaves 0x00090000 in v0 when the release is skipped. ----

// ---- 0x08B2F0D4: when the byte at 0x08D6915C is set, clear it and release the word at 0x08D69158; v0 = the flag
// when it was clear, else the release's result. ----

// ---- 0x08B336F0: when the word at 0x08D69208 is positive, 0x08C7B138(it) and 0x08C7A9C0(reloaded it), then
// clear it; v0 = the count when it was not positive, else the second call's result. ----

// ---- 0x08BF97DC (a0 = this): vtable 0x08CCED68; 0x08A5B9EC(this + 0x1C); 0x089FD82C(this); tail 0x08B64654(this). ----

// ---- 0x08C3D818 (a0 = this): vtable 0x08CD6A18; 0x08B28BE0(this + 0xA8); 0x08AC0194(this); tail 0x08B64654(this). ----

// ---- 0x089378D4 (a0 = this, a2 kept in s1): 0x0893641C(a0, a1, a2); 0x08936944(this, its result); tail
// 0x089377C4(this, a2). ----

// ---- 0x08A215F0 (a0 = this, a1 kept in s0): 0x08A2159C(this); r = 0x08ABC318(a1, 1); this->+0x14 = r;
// v0 = (r != 0). ----

// ---- 0x08A66C60 (a0 = this): obj = 0x08A5BC14(0x14); 0x08A66C34(obj, this); v0 = obj. ----

// ---- 0x08AD0900: 0x08B1A8BC(0x08AD093C); 0x08B2ECAC(0x08CB4A18, 0x08AD08D8, 0); tail 0x08C7A8C0(its result). ----

// ---- 0x08B32350: when 0x08CE0C88 is not negative 0x08C7AF28(it); 0x08C7AF40(); 0x08C7AF58(); tail
// 0x08B2EE2C(0x08D690FC). The `bltz` delay slot sets a0 = the value on both paths. ----

// ---- 0x08B32994 (a0 = this): vtable 0x08CDA730; 0x08B3252C(this); 0x08B31918(this + 0x18); vtable 0x08CDA7A8;
// v0 = it. ----

// ---- 0x08B567B0 (a0 = holder, a1 = destination): copies the 0x40-byte record a0->+0xD10 selects at
// a0 + 0xD00 to a1 (16 bytes per column), leaving C000-C030 holding it; v0 = a0 + the index * 4. ----

// ---- 0x08BBC0A0 (a0 = this): releases every node of the ring this->0 (0x08B35254 each) until it comes back to
// this. The next link is read before the release, as the delay slot does. ----

// ---- 0x08C24F98 (a0 = begin, a1 = end): 0x08A69D68 on every word from a0 up to (not including) a1. ----

// ---- 0x08C2AD88: 0x08A5B9EC on every word from a0 up to (not including) a1. ----

// ---- 0x08C35154: 0x08A5B9EC on every word from a0 up to (not including) a1. ----

// ---- 0x08C3BB10: 0x08A5B9EC on every word from a0 up to (not including) a1. ----

// ---- 0x08C723C8 (a0 = this): 0x08C5477C(this + *(vtable - 0xC), 0xA); 0x08C72194(this, its result); tail
// 0x08C7030C(its result). ----

// ---- 0x0886BD00 (a0 = this, a1 kept in s1): 0x0887D364(*(a0 + 4) + 0x88, *(a1) + 0x64); tail
// 0x08B1FC48(*(a0 + 4), a1). ----

} // namespace
} // namespace new_batch_main_1888
} // namespace gt::game

#define NEW922(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1888::name, gt::ReturnKind::kind)
NEW922(0x08A8DB4Cu, func_08A8DB4C, Word);
