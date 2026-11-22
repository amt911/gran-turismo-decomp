// New batch, main session 42: nine long functions (279-287 instructions) the recompiler emits and nothing had taken, written as
// structured C from `tools.disasm` (every branch target read, the delay slots of `jal` / `jalr` read as "before the call" and only
// a `bnel` / `beql` delay slot as "when the branch is taken"). Drafted, not verified: no gate ran on them here.
//
// Written (registered at the end of this file): func_088C22C8 (per-frame state update), func_088FD508 (installs a counted pair
// and a delegate), func_0891ABC4 (steering floats), func_08935640 (fills the record a later lookup is keyed by), func_08C21D48
// (splices counted-pointer sequences), func_08BCD6A8 (destructor of a composite object), func_088EE1EC (the record queue step
// under a try-lock), func_08940BF8 (initialiser of the agent's race state) and func_0894A600 (speed / progress state machine).
// Pitched and left out, not registered: func_08C5F3E4, a transcoder whose block tree is entered from the middle in six places
// (0x08C5F5F8, 0x08C5F670, 0x08C5F768, 0x08C5F7E0 and the two jumps at 0x08C5F6E4 / 0x08C5F7D8) and whose 256-byte stack
// table plus the signed-byte compares against the object's bytes 0x48 / 0x4D / 0x95 / 0x75 I could not pin down with
// confidence; a wrong guess there would be a silent behaviour change, so it stays recompiled.
//
// The roles below come from the accesses, not from the game: `self` is a0, `record` / `entry` a fixed-size element reached
// through a per-index stride, `counted` a reference-counted object whose strong counter sits at +4 and weak one at +8 and whose
// vtable words at offsets 8 and 0xC run when a counter reaches zero (the ll / sc pairs read as a plain load and store, as
// `src/game/unsorted/hot_batch_90.cpp` does: one thread, and the sc's success flag is 1 in v0 either way). No name is claimed:
// none of these functions has evidence for one.
//
// Every call goes through `call_keep` (v0, v1 and f0 stay where the original's `jal` leaves them) and passes the argument
// registers the original has set at that point; the callee-saved registers are restored on the way out (SavedRegs) and the
// floating ones the original saves (f20-f25) are kept there too (SavedFloat). Frames are the original's own sizes, so the
// addresses a callee writes through (`sp + N`) land in the original's slots. `tools.sp_loads` lists 27 `lw $sN, off($sp)` in this
// file whose register was never saved to that offset: each is a data load out of the frame the original just filled (the record
// copies of 0x08935640, the holder words of 0x08C21D48, the delegate pair of 0x088FD508) and each is modelled as
// `word(rt, frame.at(off))` there.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_42 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call: v0 / v1 / f0 of the callee stay in the registers as after the original's `jal`, and a callee that never writes
// v0 hands the caller's own v0 on.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments, {}).v0;
}

// A float constant exactly as the original's `lui` / `ori` pair (or bare `lui`) builds it.
float f32(u32 bits) { return std::bit_cast<float>(bits); }

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
// The callee-saved registers s0-s7 / fp as they were at entry.
class SavedRegs {
public:
    explicit SavedRegs(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_fp_ = ctx.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_fp_;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 saved_fp_;
};

// A floating-point register as it was at entry (the original saves $f20-$f25 in its frame and restores them before returning).
class SavedFloat {
public:
    SavedFloat(AllegrexContext &ctx, unsigned index) : ctx_(ctx), index_(index), saved_(ctx.fpr[index]) {}
    ~SavedFloat() { ctx_.fpr[index_] = saved_; }
    SavedFloat(const SavedFloat &) = delete;
    SavedFloat &operator=(const SavedFloat &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned index_;
    float saved_;
};

// The ll / sc counters (one thread: a plain load and store). A decrement leaves the old value in a0, v0 = 1 (the sc's success
// flag) and v1 = the address.
u32 atomic_decrement(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}

u32 atomic_increment(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
    return 1u;
}

// The reference-counted release the original repeats: the strong counter at +4 down; when it was 1 the vtable word at +8 runs
// with the object, then the weak counter at +8 down; when that was 1 the vtable word at +0xC runs. A null object does nothing.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (atomic_decrement(rt, ctx, object + 4u) != 1u) return;
    (void)call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (atomic_decrement(rt, ctx, object + 8u) != 1u) return;
    (void)call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08940BF8 (a0 = self): the initialiser of the agent's race state. Four per-lane records, each fetched with 0x08955300(self, lane),
// get their counters cleared (0x28 / 0x2C / 0x30 / 0x34 / 0x38 = 1.0 / 0x53 / 0x70 / 0x74 / 0x78 / 0x7C / 0x88 / 0x8C, and later their
// 0x84 and 0xA0 words), the two sub-objects at self+8 and self+0xC take a per-index flag derived from a byte of the other
// (bytes 0x60C+ i from bytes 0x284+ i: 1 when (b - 3) < 2; byte 0x5FA from byte 0x270: 1 for x-4 < 3 or x in {9, 0xA, 0xB}; and the
// word 0x5FC+4i cleared), the scales at self+0x528 / 0x52C take -/+ the float at other+0x78, and the object's own fields from
// 0x500 to 0x760 are zeroed with 1.0 at the unit scales. v0 ends at word(self+0x14) + 0xB0 (the last loop's pointer).
void func_08940BF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegs saved(ctx);
    SavedFloat f20(ctx, 20);
    GuestFrame frame(ctx, 0x30u);
    const u32 main = word(rt, self + 8u);      // s0
    const u32 other = word(rt, self + 0xCu);   // s3
    for (u32 lane = 0; lane < 4u; ++lane) {
        const u32 record = call(rt, ctx, 0x08955300u, {self, lane});
        put(rt, record + 0x7Cu, 0u);
        put(rt, record + 0x28u, 0u);
        put(rt, record + 0x70u, 0u);
        put(rt, record + 0x78u, 0u);
        put(rt, record + 0x74u, 0u);
        put(rt, record + 0x88u, 0u);
        put(rt, record + 0x8Cu, 0u);
        put(rt, record + 0x30u, 0u);
        put(rt, record + 0x2Cu, 0u);
        put(rt, record + 0x34u, 0u);
        put8(rt, record + 0x53u, 0u);
        put_float(rt, record + 0x38u, 1.0f);
    }
    for (u32 i = 0; i < 2u; ++i) {
        const u32 flag = static_cast<std::uint8_t>(byte_u(rt, other + 0x284u + i) - 3u) < 2u ? 1u : 0u;
        put8(rt, main + 0x60Cu + i, flag);
        put(rt, main + 0x5FCu + 4u * i, 0u);
    }
    put8(rt, main + 0x5FAu, 0u);
    const u32 kind = byte_u(rt, other + 0x270u);
    const bool special = static_cast<std::uint8_t>(kind - 4u) < 3u || kind == 9u || kind == 0xAu || kind == 0xBu;
    put8(rt, main + 0x5FAu, special ? 1u : 0u);
    put_float(rt, main + 0x4F0u, 1.0f);
    put(rt, main + 0x5F4u, 0u);
    put(rt, main + 0x6F8u, 0u);
    put(rt, main + 0x6F0u, 0u);
    put(rt, main + 0x6F4u, 0u);
    put(rt, main + 0x6E4u, 0u);
    put(rt, main + 0x6E8u, 0u);
    put(rt, main + 0x6ECu, 0u);
    put(rt, main + 0x51Cu, 0u);
    put(rt, main + 0x520u, 0u);
    const float turn = float_at(rt, other + 0x78u);
    put_float(rt, main + 0x528u, -turn);
    put(rt, main + 0x524u, 0u);
    put_float(rt, main + 0x52Cu, turn);
    (void)call(rt, ctx, 0x089588BCu, {self});
    (void)call(rt, ctx, 0x0895BABCu, {self});
    put8(rt, main + 0x732u, 0xFFu);
    put(rt, main + 0x760u, 0u);
    put(rt, main + 0x75Cu, 0u);
    put(rt, main + 0x758u, 0u);
    put(rt, main + 0x750u, 0u);
    put(rt, main + 0x754u, 0u);
    put8(rt, main + 0x74Fu, 0u);
    put8(rt, main + 0x733u, 0u);
    put(rt, main + 0x6B0u, 0u);
    put(rt, main + 0x67Cu, 0u);
    put(rt, main + 0x680u, 0u);
    put(rt, main + 0x6CCu, 0u);
    put(rt, main + 0x6D0u, 0u);
    put(rt, main + 0x6D4u, 0u);
    put(rt, main + 0x518u, 0u);
    put16(rt, main + 0x508u, 0u);
    put(rt, main + 0x530u, 0u);
    put(rt, main + 0x534u, 0u);
    put(rt, main + 0x5E0u, 0u);
    put(rt, main + 0x538u, 0u);
    put(rt, main + 0x53Cu, 0u);
    for (u32 lane = 0; lane < 4u; ++lane) {
        const u32 record = call(rt, ctx, 0x08955300u, {self, lane});
        put(rt, record + 0x84u, 0u);
    }
    put8(rt, main + 0x50Au, 0u);
    put_float(rt, main + 0x5A4u, 0.0f);
    put_float(rt, main + 0x59Cu, 0.0f);
    put_float(rt, main + 0x584u, 0.0f);
    put_float(rt, main + 0x5A0u, 0.0f);
    put_float(rt, main + 0x57Cu, 0.0f);
    put_float(rt, main + 0x580u, 0.0f);
    put_float(rt, main + 0x5A8u, 0.0f);
    put_float(rt, main + 0x5ACu, 0.0f);
    put_float(rt, main + 0x5B0u, 0.0f);
    put16(rt, main + 0x58Au, 0u);
    put16(rt, main + 0x588u, 0u);
    put16(rt, main + 0x58Cu, 0u);
    put8(rt, main + 0x57Bu, 0u);
    put8(rt, main + 0x579u, 0u);
    (void)call_keep(rt, ctx, 0x0894020Cu, {self}, {0.0f});
    (void)call(rt, ctx, 0x0895DCACu, {self, 1u});
    put16(rt, main + 0x58Eu, 0u);
    put8(rt, main + 0x730u, 0u);
    put8(rt, main + 0x731u, 0u);
    put(rt, main + 0x744u, 0u);
    put(rt, main + 0x740u, 0u);
    put8(rt, main + 0x748u, 0u);
    put8(rt, main + 0x749u, 0u);
    put8(rt, main + 0x74Au, 0u);
    put(rt, main + 0x738u, 0u);
    put(rt, main + 0x73Cu, 0u);
    const u32 holder = call(rt, ctx, 0x08949AF4u, {self});
    (void)call(rt, ctx, 0x08949AC4u, {holder});
    put8(rt, main + 0x735u, 0x7Fu);
    put16(rt, main + 0x500u, 0u);
    put16(rt, main + 0x502u, 0u);
    put16(rt, main + 0x504u, 0u);
    put16(rt, main + 0x506u, 0u);
    put8(rt, main + 0x734u, 0x7Fu);
    put8(rt, main + 0x736u, 0u);
    for (u32 lane = 0; lane < 4u; ++lane) {
        const u32 record = call(rt, ctx, 0x08955300u, {self, lane});
        put(rt, record + 0x108u, 0u);
        put(rt, record + 0x20u, 0u);
        put(rt, record + 0x4Cu, 0u);
        put(rt, record + 0x24u, 0u);
        put(rt, record + 0x3Cu, 0u);
        put(rt, record + 0x68u, 0u);
        put(rt, record + 0xC4u, 0u);
        put(rt, record + 0xD4u, 0u);
        put(rt, record + 0xDCu, 0u);
        put(rt, record + 0xE0u, 0u);
        put(rt, record + 0xF8u, 0u);
        put(rt, record + 0xFCu, 0u);
        put(rt, record + 0x6Cu, 0u);
        put(rt, record + 0xF0u, 0u);
        put(rt, record + 0xF4u, 0u);
        put(rt, record + 0x80u, 0u);
        put(rt, record + 0x44u, 0u);
        put(rt, record + 0x48u, 0u);
        put(rt, record + 0x40u, 0u);
        put(rt, record + 0xC0u, 0u);
        put(rt, record + 0x100u, 0u);
        put(rt, record + 0x104u, 0u);
    }
    put_float(rt, main + 0x6D8u, float_at(rt, main + 0x704u));
    put8(rt, main + 0x512u, 0x0Cu);
    put(rt, main + 0x6C4u, 0u);
    put(rt, main + 0x6C8u, 0u);
    put8(rt, main + 0x5F9u, 0u);
    put(rt, main + 0x5ECu, 0u);
    put(rt, main + 0x590u, 0u);
    put8(rt, main + 0x50Eu, 0u);
    put8(rt, main + 0x50Fu, 0u);
    put(rt, main + 0x6DCu, 0u);
    put8(rt, main + 0x69Cu, 0u);
    put8(rt, main + 0x50Bu, 0u);
    put8(rt, main + 0x50Cu, 0u);
    put8(rt, main + 0x50Du, 0u);
    put(rt, main + 0x684u, 0u);
    put(rt, main + 0x688u, 0u);
    put(rt, main + 0x68Cu, 0u);
    put(rt, main + 0x6B4u, 0u);
    put(rt, main + 0x6B8u, 0u);
    put(rt, main + 0x6BCu, 0u);
    put(rt, main + 0x6C0u, 0u);
    put(rt, main + 0x6E0u, 0u);
    put(rt, main + 0x6A4u, 0u);
    put(rt, main + 0x6ACu, 0u);
    for (u32 lane = 0; lane < 4u; ++lane) {
        const u32 record = call(rt, ctx, 0x08955300u, {self, lane});
        put(rt, record + 0xA0u, 0u);
    }
    put_float(rt, main + 0x564u, 1.0f);
    put16(rt, main + 0x74Cu, 0u);
    put(rt, main + 0x550u, 0u);
    put(rt, main + 0x558u, 0u);
    put(rt, main + 0x554u, 0u);
    put(rt, main + 0x55Cu, 0u);
    put(rt, main + 0x560u, 0u);
    put8(rt, main + 0x74Eu, 0u);
    put8(rt, main + 0x74Bu, 0u);
    put(rt, main + 0x698u, 0u);
    (void)call(rt, ctx, 0x0895F41Cu, {self});
    const u32 block = word(rt, self + 0x14u);
    for (u32 i = 0; i < 2u; ++i) put(rt, block + 4u * i, 0u);
    for (u32 i = 0; i < 3u; ++i) put(rt, block + 0x14u + 4u * i, 0u);
    put_float(rt, block + 8u, 1.0f);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    u32 entry = block + 0x20u;
    for (u32 i = 0; i < 4u; ++i) {
        put(rt, entry + 0x00u, 0u);
        put_float(rt, entry + 0x04u, 1.0f);
        put(rt, entry + 0x08u, 0u);
        put(rt, entry + 0x0Cu, 0u);
        put_float(rt, entry + 0x10u, 1.0f);
        put(rt, entry + 0x14u, 0u);
        put(rt, entry + 0x18u, 0u);
        put(rt, entry + 0x1Cu, 0u);
        put8(rt, entry + 0x20u, 0u);
        put8(rt, entry + 0x21u, 0u);
        entry += 0x24u;
    }
    ctx.gpr[2] = entry;  // the loop's last pointer increment, as the original leaves v0
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08BCD6A8 (a0 = self): a destructor of a composite object. The owner at self+4 is locked (0x08B34888), its 0x30 byte marks the
// "destroying" state around 0x08B34B2C when its 0x2C counter is not zero (0x08B34B24 unlocks in both cases), self+0x540 is told
// about it (0x08BBC0A0), the counted object at self+0x53C is released, the pair at self+0x528 / 0x52C is released through its
// vtable's first word, the sub-object vtables at 0x3C / 0x6C / 0x70 / 0x74 are stored (0x74 twice, the last store wins) and the
// counted objects at 0x38 / 0x2C / 0x1C / 0x14 / 0xC / 4 are released. Then the owner's own teardown: with its 0x2C counter not
// zero it is decremented and, when it reaches zero, the block at 0x34 / 0x38 is taken over (released through its vtable at +8) and
// the pair (block, word 0x38) is passed to a tail call through the vtable of the former; otherwise the block at 0x38 is released
// and 0x08B64654(owner) is the tail call. v0 is the last call's.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088C22C8 (a0 = self): the per-frame state update. 0x088BCA5C(self) runs first; the state block at self+0x24 has a mode byte at
// +0xA5: 7 and 9 just run 0x088BD0C4(self, 0.0) and return its answer. With the byte at word(self+8)+3 set the object is ended
// (a value of 2 runs 0x088C4BB0(self)). Otherwise the mode picks the work: mode 0 asks the member at self+0x1C for index 7 and
// reports a different answer through 0x088BF94C; modes 1/2/4/5 look the entry up with 0x088DF13C(word(self+12), word(self+16)),
// and an entry whose byte +0x25 is set (or the state's halfword +2 having bit 1) runs a 3-or-7 sequence of the member's virtual
// words 0x18 / 0x24 reporting through 0x088BF94C and 0x088BF978, while a clear one raises byte +0x52 to 0x78, sets bit 1 of the
// halfword and drives the same words. Then 0x088BEC3C(self) (its f0 kept in f21) clears bit 0 of word(self+0x2C) when the state
// byte +0x12 is set, 0x088BC21C(self) runs, four lane records (0x0894268C(word(self+4), lane, byte at lane record +0xE9)) are
// summed, and the ratio (float at word(self+8)+0x720 over a quarter of that sum, absolute) goes through 0x088BFE70 /
// 0x088C0738 / 0x088C10B0 / 0x088BE248 / 0x088BC000 / 0x088C12B0. v0 is the last call's, or 4 / 0 on the two early exits.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x0891ABC4 (a0 = self, a1 = other): the steering update. The float holder at self+0x1F8 answers three values (0x0891A698,
// 0x0891A874, 0x0891A864). With the kind word at self+0x1FC equal to 1, 3 or 4 the controller from 0x089260F4(self) is asked for
// four floats (its virtual word 0x30 with 0 / 4 / 8) and seven indices (word 0x28 with 0x19, 0x1A, 1, 2, 3, 4, 8): the flags fold
// into the two accumulators (+-1 and the byte at holder+0x29 picking -2.0 / 0.7 or -0.2 / 0.2), indices 0x19 / 0x1A pass the
// accumulator through 0x08B66684, the last index / 1.0 is written at holder+0x29, the third float scaled by 0x3C8EFA35 (degrees to
// radians) joins f23, and the sum goes to float(self+0x200). Kind 2 skips all that. Any other kind takes the other branch: the
// holder's float at self+0x200 is fed to 0x088D2BC8(word(other+0x61C)) and stored back, 0x088DA460(other) is compared with the
// first accumulator and the accumulator moves by 0x088DA460's answer (subtracting under a half of it), then relaxes toward the
// stored value by the factor at self+0x204. Both branches end with 0x0891A6A0(holder, f21, f22) / 0x0891A87C(holder, f23) and
// 0x0891A884(holder, f12, f13, f14) with the constants kind 3 and 4 pick, the other object stored at holder+8, 0x0892632C(holder,
// self+0x10), and 0x08B4A774(self+0x1E4, self+0x10, sp) with a {0, 0, 0, 1.0} record; v0 is that last call's.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08935640 (a0 = self, a1, a2, a3): builds the record a later lookup is keyed by. Two calls of 0x08936584 fill stack records -
// the first at sp with the incoming a2 / a3, the second at sp+0x18 with the pair at sp+8 / sp+0xC - after 0x089363FC() (a global
// holder) and 0x089368DC(holder, a1, pair) answer; a zero answer, or a pair of -1, returns 0. 0x089322CC(self) runs, the second
// record is copied into self field by field (0x00 / 0x04, 0x08 / 0x0C from a2 / a3, 0x18-0x6C straight, 0x70-0x94 from the record's
// 0xF8-0x11C, then the reordered 0x98-0xE4 block), and the last two words 0xE8 / 0xEC take -1. Each pair of the fields 0x20-0x64
// that is all -1 returns 0; then 0x08934238(self), byte +0x10 cleared, 0x089354CC(self) unless the pair at sp+0x10 is all -1, and
// 0x0892ED70(self) answer: v0 = 1 when it is not zero, else 0.
void func_08935640(Runtime &rt, AllegrexContext &ctx) {
    static constexpr u32 kCopy[][2] = {
        {0x00u, 0x00u}, {0x04u, 0x04u},   {0x08u, 0xF0u},   {0x0Cu, 0xF4u},   {0x18u, 0x18u},   {0x1Cu, 0x1Cu},
        {0x20u, 0x20u}, {0x24u, 0x24u},   {0x28u, 0x28u},   {0x2Cu, 0x2Cu},   {0x30u, 0x30u},   {0x34u, 0x34u},
        {0x38u, 0x38u}, {0x3Cu, 0x3Cu},   {0x40u, 0x40u},   {0x44u, 0x44u},   {0x48u, 0x48u},   {0x4Cu, 0x4Cu},
        {0x50u, 0x50u}, {0x54u, 0x54u},   {0x58u, 0x58u},   {0x5Cu, 0x5Cu},   {0x60u, 0x60u},   {0x64u, 0x64u},
        {0x68u, 0x68u}, {0x6Cu, 0x6Cu},   {0x70u, 0x50u},   {0x74u, 0x54u},   {0x78u, 0x100u},  {0x7Cu, 0x104u},
        {0x80u, 0x108u}, {0x84u, 0x10Cu}, {0x88u, 0x110u},  {0x8Cu, 0x114u},  {0x90u, 0x118u},  {0x94u, 0x11Cu},
        {0x98u, 0xA0u}, {0x9Cu, 0xA4u},   {0xA0u, 0x78u},   {0xA4u, 0x7Cu},   {0xA8u, 0x80u},   {0xACu, 0x84u},
        {0xB0u, 0xC0u}, {0xB4u, 0xC4u},   {0xB8u, 0xB8u},   {0xBCu, 0xBCu},   {0xC0u, 0xC8u},   {0xC4u, 0xCCu},
        {0xC8u, 0xB0u}, {0xCCu, 0xB4u},   {0xD0u, 0xA8u},   {0xD4u, 0xACu},   {0xD8u, 0x30u},   {0xDCu, 0x34u},
        {0xE0u, 0x38u}, {0xE4u, 0x3Cu},
    };
    const u32 self = ctx.gpr[4];  // fp
    const u32 key_hi = ctx.gpr[6];
    const u32 key_lo = ctx.gpr[7];
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x150u);
    put(rt, frame.at(0xF0u), key_hi);
    put(rt, frame.at(0xF4u), key_lo);
    const u32 holder = call(rt, ctx, 0x089363FCu, {self});
    if (call(rt, ctx, 0x089368DCu, {holder, ctx.gpr[5], key_hi, key_lo}) == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        return;
    }
    // The second 0x089363FC() is handed the a0 the previous callee left: the callee reads none of its arguments.
    const u32 again = call(rt, ctx, 0x089363FCu, {ctx.gpr[4]});
    (void)call_keep(rt, ctx, 0x08936584u, {again, ctx.gpr[5], key_hi, key_lo, frame.at(0u)}, {});
    const u32 id_hi = word(rt, frame.at(8u));
    const u32 id_lo = word(rt, frame.at(0xCu));
    if (id_hi == 0xFFFFFFFFu && id_lo == 0xFFFFFFFFu) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        return;
    }
    const u32 third = call(rt, ctx, 0x089363FCu, {ctx.gpr[4]});
    if (call(rt, ctx, 0x089368DCu, {third, ctx.gpr[5], id_hi, id_lo}) == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        return;
    }
    const u32 holder2 = call(rt, ctx, 0x089363FCu, {ctx.gpr[4]});
    (void)call_keep(rt, ctx, 0x08936584u, {holder2, ctx.gpr[5], id_hi, id_lo, frame.at(0x18u)}, {});
    (void)call(rt, ctx, 0x089322CCu, {self});
    for (const auto &pair : kCopy) put(rt, self + pair[0], word(rt, frame.at(pair[1])));
    put(rt, self + 0xE8u, 0xFFFFFFFFu);
    put(rt, self + 0xECu, 0xFFFFFFFFu);
    for (u32 offset = 0x20u; offset <= 0x60u; offset += 8u) {
        if (word(rt, self + offset) == 0xFFFFFFFFu && word(rt, self + offset + 4u) == 0xFFFFFFFFu) {
            ctx.gpr[2] = 0u;
            ctx.gpr[3] = 0u;
            return;
        }
    }
    (void)call(rt, ctx, 0x08934238u, {self});
    const u32 tail_hi = word(rt, frame.at(0x10u));
    const u32 tail_lo = word(rt, frame.at(0x14u));
    put8(rt, self + 0x10u, 0u);
    if (tail_hi != 0xFFFFFFFFu || tail_lo != 0xFFFFFFFFu) (void)call(rt, ctx, 0x089354CCu, {self});
    const u32 answer = call(rt, ctx, 0x0892ED70u, {self});
    ctx.gpr[3] = answer != 0u ? 1u : 0u;
    ctx.gpr[2] = ctx.gpr[3];
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x0894A600 (a0 = self): the agent's speed / progress state machine. The float at word(self+8)+0x590 is the accumulator kept
// across frames and the byte at word(self+8)+0x5F9 selects the branch: 0 the plain accumulation through 0x0895549C(self),
// 1 one step of 0x0894A0DC (its answer compared against 0x0895BBC4(word(self+0xC)+0x270) and scaled by word(self+8)+0x5F0),
// 2 / 3 the throttle block (the selector byte at word(self+8)+4, or 0 when byte +0xE is set; the flag at +0x5EC; then the lane
// scale 0x0895BBB4 and the two lanes 0x08955300(self, 2i) / 2i+1), anything else keeps the accumulator. The route length at
// word(self+0xC)+0x23E scaled by 0x3DD67750 is the limit the accumulation is clamped to. The closing block moves the candidate
// against 0 and against the distance at word(self+0xC)+0x23C plus ten units, then stores the lap's frames into the halfword at
// word(self+8)+0x58E: the accumulator over 240 / 6.2831855 rounded toward zero, raised to the lap length - 1 when
// byte(word(self+0xC)+0x281) is not 4. v0 = that halfword's value.
void func_0894A600(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];               // s2
    const u32 state = word(rt, self + 8u);     // s1
    const u32 lanes = word(rt, self + 0xCu);   // s3
    SavedRegs saved(ctx);
    SavedFloat f20(ctx, 20), f21(ctx, 21);
    GuestFrame frame(ctx, 0x40u);
    const float f21v = call_keep(rt, ctx, 0x0895549Cu, {self}, {}).f0;  // f21
    float f20v = 0.0f;                                                  // f20
    u32 code = byte_u(rt, state + 0x5F9u);
    float candidate = 0.0f;  // f4
    if (code == 0u) {
        f20v = static_cast<float>(static_cast<s32>(half_u(rt, lanes + 0x23Eu))) * f32(0x3DD67750u);
        float factor = 0.0f;  // f2
        if (byte_u(rt, lanes + 0x270u) == 7u) {
            factor = call_keep(rt, ctx, 0x08947CF0u, {self}, {}).f0;
        } else {
            factor = call_keep(rt, ctx, 0x089491A0u, {self}, {float_at(rt, state + 0x534u)}).f0;
        }
        const float added = float_at(rt, state + 0x590u) + f21v * (factor * float_at(rt, lanes + 0x270u + 0xE4u));
        candidate = added;
        put_float(rt, state + 0x590u, added);
        if (added < f20v) {
            candidate = f20v;
            put_float(rt, state + 0x590u, f20v);
        }
        code = byte_u(rt, state + 0x5F9u);
    } else {
        f20v = call_keep(rt, ctx, 0x0894A0DCu, {self}, {}).f0;
        code = byte_u(rt, state + 0x5F9u);
        if (code == 1u) {
            const u32 answer = call(rt, ctx, 0x0895BBC4u, {lanes + 0x270u});
            candidate = answer == 0u ? f20v : f20v * float_at(rt, state + 0x5F0u);
            put_float(rt, state + 0x590u, candidate);
            code = byte_u(rt, state + 0x5F9u);
        } else if (code == 2u || code == 3u) {
            const u32 selector = byte_u(rt, state + 0xEu) != 0u ? byte_u(rt, state + 4u) : 0u;
            if ((selector == 3u || selector == 6u) && float_at(rt, state + 0x5ECu) == 1.0f) {
                const float base = float_at(rt, state + 0x590u);
                const u32 index = byte_u(rt, state + 0x5F8u);
                const u32 answer = call(rt, ctx, 0x0895BBB4u, {lanes + 0x270u});
                const u32 lane = answer != 0u ? (index <= 1u ? index : 1u) : index;
                float step = base / float_at(rt, lanes + 0x270u + (lane << 2) + 0x58u);
                put_float(rt, state + 0x5FCu, step);
                for (u32 i = 0; i < 2u; ++i) {
                    const float scale = float_at(rt, lanes + 0x270u + 4u * i + 0xA8u);
                    const u32 even = i << 1;
                    const u32 odd = even ^ 1u;
                    const u32 first = call(rt, ctx, 0x08955300u, {self, even});
                    step *= scale;
                    put_float(rt, first + 0x34u, step);
                    if (odd != 0x7Fu) {
                        const u32 second = call(rt, ctx, 0x08955300u, {self, odd});
                        put_float(rt, second + 0x34u, step);
                    }
                    put_float(rt, state + 0x5FCu + 4u * (i + 1u), step);
                }
                candidate = base;
                code = 1u;
                put8(rt, state + 0x5F9u, 1u);
            } else {
                const float upper = float_at(rt, state + 0x5E4u);   // f0
                const float current = float_at(rt, state + 0x5E8u); // f1
                candidate = float_at(rt, state + 0x590u);
                bool pulled = false;
                if (!(current < upper) && f20v <= candidate) {
                    candidate = f20v;
                    pulled = true;
                }
                if (!pulled && !(upper < current)) {
                    if (candidate <= f20v) {
                        candidate = f20v;
                        pulled = true;
                    }
                }
                if (pulled) {
                    code = 1u;
                    put8(rt, state + 0x512u, 0x0Cu);
                    put8(rt, state + 0x5F9u, 1u);
                    put_float(rt, state + 0x590u, f20v);
                }
            }
        } else {
            candidate = float_at(rt, state + 0x590u);
        }
    }
    // 0x0894A880 / 0x0894A8C8: the code word decides between the lap-length test and the vehicle block
    if (code == 3u) {
        const float bound = static_cast<float>(static_cast<s32>(half_u(rt, lanes + 0x23Eu))) * f32(0x3DD67750u);
        if (float_at(rt, state + 0x5E8u) <= bound) {
            put8(rt, state + 0x5F9u, 0u);
            code = 0u;
        }
    } else if (code == 2u) {
        const u32 other = word(rt, self + 8u);
        const u32 vehicle = word(rt, self + 0xCu);
        bool reached = byte_u(rt, other + 0xDu) != 0u;
        if (!reached) {
            if (f32(0x3F400000u) < float_at(rt, other + 0x538u)) {
                reached = true;
            } else if (byte_u(rt, vehicle + 0x272u) == 1u && f32(0x3E800000u) < float_at(rt, other + 0x53Cu)) {
                reached = true;
            }
        }
        if (reached) {
            const u32 units = static_cast<u32>(static_cast<s32>(half_u(rt, lanes + 0x23Eu)) + 0x1F4u);
            const float bound = static_cast<float>(static_cast<s32>(units)) * f32(0x3DD67750u);
            if (candidate < bound) {
                code = 0u;
                put8(rt, state + 0x5F9u, 0u);
            } else {
                const float limit = float_at(rt, state + 0x6C4u);
                if (!(f32(0x4031C71Du) <= limit) && f32(0xC031C71Du) < limit) {
                    code = 0u;
                    put8(rt, state + 0x5F9u, 0u);
                }
            }
        }
    }
    // 0x0894A998: a negative candidate stores zero; one past the distance stores the distance
    if (candidate < 0.0f) {
        put_float(rt, state + 0x590u, 0.0f);
    } else {
        const float bound = static_cast<float>(static_cast<s32>(half_u(rt, lanes + 0x23Cu)) + 0xAu) * f32(0x3DD67750u);
        if (bound <= candidate) put_float(rt, state + 0x590u, bound);
    }
    if ((code & 0xFFu) != 3u) put8(rt, state + 0x74Fu, 0u);
    const u32 counter = word(rt, self + 8u);
    const u32 vehicle = word(rt, self + 0xCu);
    const float frames = (float_at(rt, counter + 0x590u) * f32(0x42700000u)) / f32(0x40C90FDBu);
    const float truncated = std::trunc(frames);  // trunc.w.s
    const s32 rounded = static_cast<s32>(truncated > 2147483647.0f ? 2147483647.0f
                                         : truncated < -2147483648.0f ? -2147483648.0f
                                                                      : truncated);
    const u32 flag = byte_u(rt, vehicle + 0x281u) ^ 4u;
    const s32 lap = static_cast<s32>(half_u(rt, vehicle + 0x23Eu)) - 1;
    const u32 chosen = flag != 0u ? static_cast<u32>(std::max(rounded, lap)) : static_cast<u32>(rounded);
    put16(rt, counter + 0x58Eu, chosen);
    ctx.gpr[2] = chosen;
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088FD508 (a0 = self, a1 = source, a2 = other): installs a new counted pair. 0x089624FC(a1, a1, a2) decides first (zero returns
// 0, a null a2 returns its answer); the object at a2 answers virtual word 0x38, and two -1 words return 0. A 0x548-byte object is
// allocated (0x08B62344), constructed with 0x088FCA30(alloc, a1, a2, 0) and given the vtable 0x08CC51A8; a 0x10-byte holder takes
// 0x08CC51B8 and the counts 1 / 1 with the object at +0xC, and 0x08B8E380(sp+4, alloc, alloc, 1) runs on it. The pair at self+0x10 /
// 0x14 is replaced (the previous holder released through its counted vtable words 8 / 0xC), the global pair 0x08C8A4E8 is loaded
// with 0x08BBC08C, the new object's owner word at +4 is locked (0x08B34888) and its 0x2C counter read: not zero inserts a
// 0x14-byte node 0x08CC51D0 into the owner's 0x34 / 0x38 list (a guarded static 0x08CC51E8 at 0x08D76498 built under the byte
// 0x08D761B8) and releases the source holder; zero invokes the delegate pair (bit 0 of the second word selects a virtual slot) with
// the object and the second word's half as the this adjustment. The owner is unlocked (0x08B34B24) and the holder released twice
// more. v0 = 1 at the end, 0 on the early exits.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08C21D48 (a0 = self, a1 = argument): splices two counted-pointer sequences. 0x08A6158C(argument, argument) answers the source
// holder and 0x08A6159C(argument, word(self+8)) the destination list, kept at sp+0x20; 0x08A76ABC(sp+0x14, holder) fills a first
// temporary whose virtual word 0x78 decides whether 0x08A98C1C(sp+0x18, 1) then 0x08A5BAD8 / 0x08A5B9EC rebuild sp+0x14. The
// element at sp+0x14 answers virtual word 0x7C = the count the destination list at word(self+8) already holds: equal counts run
// over every element with the temporary at sp+0x10 (word 0x84 asked for the element's index, word 0xC4 takes the temporary), more
// reached than held runs the elements past the held count through sp+0xC and then the rest through sp+8 (0x08A76AA0), fewer
// reached than held runs the held ones through sp+4 and then 0x08A98C1C(sp+0x18, reached - held + 1) over the range and the last
// element through word 0xC4. Every element that is null reports through 0x08A5BB7C, and each temporary is released with
// 0x08A5B9EC. 0x08A61584(argument, sp+0x14) closes it; v0 is the last call's answer.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088EE1EC (a0 = self): the queue step of the record at self+0x11C. A null record returns -1. When the id word at self+0x548 is
// not -1 and equals the one at self+0x54C, and the record's read index byte +0x18 differs from its write one +0x19, the virtual
// word self+0xB0 (a0 = word(self+0xBC)) answers an index that is stored into the record (halfword at record + 2*index takes the
// halfword at record + 2*read + 0x10, the read index advances modulo 4) around 0x08B344E0 / 0x08B3457C(self+0xC4). Then, unless
// the index words at self+0x4EC / self+0x4F0 are equal, the word at self+0x4E8 is taken as a try-lock (a busy one releases
// nothing and returns 1 as well) and the current 40-byte entry at self+index*40 is used: when its word +0x28 is not zero and
// 0x08B3453C(0x08D473E8) succeeds, a two-word value is asked of the virtual word 0x70 of word(self+0x538) for the entry's words
// +8 and +0xC. A null entry word at +4 goes on with the id at self+0x54C searched in the record's four halfwords (and the flag
// byte at self+0x4B8 + i), reporting the index into the entry and setting that byte; an entry with data either goes through
// 0x088EEE98 / 0x088EEC88 / 0x088EED18 / 0x088EEDD8 and clears its 0x90-byte area at self + index*144 + 0x210 (a -1 halfword
// first, then 0x80 bytes zeroed), or, when the next index is not the one at self+0x4F0, moves the entry (its 0x20 bytes and the
// 8 at +0x20 through the stack, with 0x3C at stack +0x20) and counts down its +0x20 word. The index at self+0x4EC becomes the
// next one on the paths that finish through the entry, and the lock word is cleared. v0 = 1.
void func_088EE1EC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];  // s3
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x70u);
    const u32 record = word(rt, self + 0x11Cu);
    if (record == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 current_id = word(rt, self + 0x548u);
    if (current_id != 0xFFFFFFFFu && current_id == word(rt, self + 0x54Cu) &&
        byte_u(rt, record + 0x19u) != byte_u(rt, record + 0x18u)) {
        const u32 slot = self + 0xC4u;  // s1
        const u32 index = call_indirect(rt, ctx, word(rt, self + 0xB0u), {word(rt, self + 0xBCu)});
        (void)call(rt, ctx, 0x08B344E0u, {slot});
        const u32 read = byte_u(rt, record + 0x18u);
        put16(rt, record + 2u * index, half_u(rt, record + 2u * read + 0x10u));
        put8(rt, record + 0x18u, (read + 1u) & 3u);
        (void)call(rt, ctx, 0x08B3457Cu, {slot});
    }
    const u32 lock = self + 0x4E8u;  // s4
    if (word(rt, self + 0x4ECu) != word(rt, self + 0x4F0u) && word(rt, lock) == 0u) {
        put(rt, lock, 1u);
        const u32 entry_base = self + word(rt, self + 0x4ECu) * 40u;
        if (word(rt, entry_base + 0x28u) != 0u && call(rt, ctx, 0x08B3453Cu, {0x08D473E8u}) != 0u) {
            u32 first = 0u;    // fp, from the virtual word 0x70 for the entry's +0xC
            u32 stored = 0u;   // stack +0x30, from the virtual word 0x70 for the entry's +8
            const u32 base = self + word(rt, self + 0x4ECu) * 40u;
            const u32 entry = base + 4u;  // s5
            const u32 source = word(rt, base + 4u);  // s1
            if (source != 0u) {
                const u32 owner = word(rt, self + 0x538u);
                stored = call_indirect(rt, ctx, word(rt, word(rt, owner) + 0x70u), {owner, word(rt, source + 8u)});
                put(rt, frame.at(0x30u), stored);
                first = call_indirect(rt, ctx, word(rt, word(rt, owner) + 0x70u), {owner, word(rt, source + 0xCu)});
            }
            (void)call(rt, ctx, 0x08B3457Cu, {0x08D473E8u});
            const u32 second = word(rt, entry + 4u);            // s2
            const u32 next = (word(rt, self + 0x4ECu) + 1u) & 3u;  // s7
            u32 found = 0u;  // a0
            if (second == 0u) {
                const u32 wanted = word(rt, self + 0x54Cu);
                const u32 flags = word(rt, self + 0x11Cu);
                for (u32 i = 0; i < 4u; ++i) {
                    if (half_u(rt, flags + 2u * i) == wanted && signed_byte(rt, self + 0x4B8u + i) == 0) {
                        if (source != 0u) put8(rt, source, i);
                        put8(rt, self + 0x4B8u + i, 1u);
                        found = 1u;
                        break;
                    }
                }
            } else if (source != 0u && first != 0u && stored != 0u) {
                found = signed_byte(rt, source) != -1 ? 2u : 0u;
            }
            if (source == 0u) {
                put(rt, entry + 0x24u, 0u);
                put(rt, self + 0x4ECu, next);
            } else if (found == 0u) {
                // 0x088EE4E4: the entry is either moved to the next index or its +0x20 word counted down
                if (next != word(rt, self + 0x4F0u)) {
                    const u32 counter = word(rt, entry + 0x20u);
                    if (counter != 0u) {
                        put(rt, entry + 0x20u, counter - 1u);
                    } else {
                        const u32 destination = self + next * 40u + 4u;
                        for (u32 i = 0; i < 0x20u; i += 4u) put(rt, frame.at(i), word(rt, destination + i));
                        for (u32 i = 0; i < 0x20u; i += 4u) put(rt, destination + i, word(rt, entry + i));
                        for (u32 i = 0; i < 8u; i += 4u) put(rt, destination + 0x20u + i, word(rt, entry + 0x20u + i));
                        put(rt, frame.at(0x20u), 0x3Cu);
                        const u32 live = self + word(rt, self + 0x4ECu) * 40u + 4u;
                        for (u32 i = 0; i < 0x20u; i += 4u) put(rt, live + i, word(rt, frame.at(i)));
                        for (u32 i = 0; i < 8u; i += 4u) put(rt, live + 0x20u + i, word(rt, frame.at(0x20u + i)));
                    }
                }
            } else {
                // 0x088EE430: the entry's data
                const u32 table = word(rt, self + 0x118u);
                const u32 flag = second != 0u ? 1u : 0u;
                const u32 handle = call(rt, ctx, 0x088EEE98u, {table, static_cast<u32>(signed_byte(rt, word(rt, entry)))});
                const u32 node = call(rt, ctx, 0x088EEC88u, {handle, source, flag});
                (void)call(rt, ctx, 0x088EED18u, {handle});
                (void)call(rt, ctx, 0x088EEDD8u, {handle});
                const u32 area = self + static_cast<u32>(signed_byte(rt, node)) * 144u + 0x210u;
                const u32 data = area + 8u;
                (void)call(rt, ctx, 0x08B36018u, {data, 0u, 0x80u});
                put16(rt, area + 8u, 0xFFFFFFFFu);
                put(rt, data + 0x84u, 0u);
                put8(rt, data + 0x88u, 0u);
                put8(rt, data + 0x89u, 0u);
                put(rt, data + 0x80u, 0u);
                if (flag == 0u) put8(rt, node + 0x37u, 1u);
                put(rt, entry + 0xCu, first);
                (void)call_indirect(rt, ctx, word(rt, self + 0xB4u), {node, entry + 4u, word(rt, self + 0xBCu)});
                put(rt, entry + 0x24u, 0u);
                put(rt, self + 0x4ECu, next);
            }
        }
    }
    if (word(rt, lock) == 1u) put(rt, lock, 0u);
    ctx.gpr[2] = 1u;
}

} // namespace
} // namespace new_batch_main_42
} // namespace gt::game

#define NEW42(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_42::name, gt::ReturnKind::kind)
NEW42(0x08940BF8u, func_08940BF8, Word);
NEW42(0x08935640u, func_08935640, Word);
NEW42(0x088EE1ECu, func_088EE1EC, Word);
NEW42(0x0894A600u, func_0894A600, Word);
