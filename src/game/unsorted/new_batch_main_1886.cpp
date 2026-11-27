// Main-session batch 1624: 14 functions retried from earlier drafts (and new ones the extra seeds made emittable), written by
// hand from `tools.disasm`. Every body is structured C: branches and loops as they read, named typed locals, guest memory
// through the batch helpers, and a call only at the original's `jal` (the argument registers the asm sets, a delay slot's
// work done before the call). Callee-saved registers are set where the original sets them (a callee may read them) and put
// back at the end; `v0` ends as every exit path of the asm leaves it. No names: none has evidence.
// Skipped: 0x08A63C5C (jump table over 0x08CFF278, function body longer than the disassembler's boundary), 0x08B754C4 and
// 0x08B75B04 (epilogue adds a callee's answer to $sp: `jr $ra; addu $sp, $sp, $v1`).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1886 {
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
// The same, with `v1` and `f0` too (a callee that never writes `v1` leaves it).
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A call whose arguments are floats ($f12...) and whose answer may be in $f0.
GuestResult call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
// The FPU's division: 0/0 and inf/inf give the PSP's default NaN (0x7FC00000), not the host's.
float psp_div(float a, float b) {
    if ((a == 0.0f && b == 0.0f) || (std::isinf(a) && std::isinf(b))) return std::bit_cast<float>(0x7FC00000u);
    return a / b;
}
// The FPU's square root: a negative operand gives the default NaN.
float psp_sqrt(float a) {
    if (a < 0.0f) return std::bit_cast<float>(0x7FC00000u);
    return std::sqrt(a);
}

// s0-s7 and fp are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[9];
};

// $f20-$f31 are callee-saved the same way.
class SavedFloats {
public:
    explicit SavedFloats(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 12u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFloats() {
        for (unsigned i = 0u; i < 12u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[12];
};

// 0x0881BAC0 (a0 = this, a1..a3 and t0 = a key plus a destination index): the 30 records of 8 bytes from this + 0x354 are
// scanned; 0x088A5EE0(record) answers two words and 0x088A6164(record) a third. A record whose pair is (-1, -1) ends the
// search at index s3 (0x1D through 0x0881ACC0(this, 1, 1) when that index is negative), one whose words are (a2, a3) and
// whose third is the saved t0 answers 0, and an exhausted scan reports through 0x0881ACC0 too and uses index 0x1D. Then
// 0x088A79AC(sp, 0), 0x088A58A4(sp, a2, a3), 0x088A599C(sp, t0), 0x088A6C18(this + 0x354 + index * 8, sp) and
// 0x088A7548(sp) run and v0 = 1.
void func_0881BAC0(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    ctx.gpr[30] = self;
    ctx.gpr[23] = 0x1Eu;
    ctx.gpr[22] = 0xFFFFFFFFu;
    ctx.gpr[21] = third;
    ctx.gpr[20] = second;
    ctx.gpr[19] = 0u;
    ctx.gpr[18] = self + 0x354u;
    put(rt, frame.at(0x10u), fourth);
    u32 index = 0u;
    for (;;) {
        const GuestResult pair = call_result(rt, ctx, 0x088A5EE0u, {ctx.gpr[18], first, second, third});
        const u32 left = pair.v0;
        const u32 right = pair.v1;
        ctx.gpr[16] = left;
        ctx.gpr[17] = right;
        const u32 probe = call(rt, ctx, 0x088A6164u, {ctx.gpr[18], first, second, third});
        ctx.gpr[18] += 8u;
        if (left == 0xFFFFFFFFu && right == 0xFFFFFFFFu) {
            if (static_cast<s32>(ctx.gpr[19]) >= 0) {
                index = ctx.gpr[19];
            } else {
                call(rt, ctx, 0x0881ACC0u, {self, 1u, 1u});
                index = 0x1Du;
            }
            break;
        }
        const u32 scanned = ctx.gpr[19] + 1u;
        ctx.gpr[19] = scanned;
        if (left == second && right == third && word(rt, frame.at(0x10u)) == probe) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (scanned != 0x1Eu) continue;
        call(rt, ctx, 0x0881ACC0u, {self, 1u, 1u});
        index = 0x1Du;
        break;
    }
    call(rt, ctx, 0x088A79ACu, {frame.at(0u), 0u});
    call(rt, ctx, 0x088A58A4u, {frame.at(0u), second, third});
    call(rt, ctx, 0x088A599Cu, {frame.at(0u), word(rt, frame.at(0x10u))});
    call(rt, ctx, 0x088A6C18u, {(index << 3) + self + 0x354u, frame.at(0u)});
    call(rt, ctx, 0x088A7548u, {frame.at(0u)});
    ctx.gpr[2] = 1u;
}

// 0x08B66D54 (f12 = a value): 0x08B6A1AC(f12) answers f21. With the mode word at 0x08C96698 not -1 and 0x08B6ECB0(f12)
// answering 0, 0x08B663A4(f12) is compared with the float at 0x08CB62B4: above it the 0x28-byte request at sp is built (1
// at +0, 0x08CB5E3C at +4, 0 at +0x20, then 0x08B70F28(f12)'s answer at +8 and +0x10, 1 at +0xC and +0x14, 0 at +0x18 and
// +0x1C) and 0x08B6E524(sp) decides, a mode of 2 skipping that call: the 0x21 case (mode 2 or a zero answer) puts 0x21 in
// the slot 0x08B77D40 answers, a non-zero word(sp + 0x20) overwrites it with that word, and 0x08B71EC8(word(sp + 0x18),
// word(sp + 0x1C)) finishes. f0 = f21 on the early exits, else its f0.
void func_08B66D54(Runtime &rt, AllegrexContext &ctx) {
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x50u);
    const float f20 = ctx.fpr[12];
    const u32 argument0 = ctx.gpr[4];
    const u32 argument1 = ctx.gpr[5];
    const u32 argument2 = ctx.gpr[6];
    const u32 argument3 = ctx.gpr[7];
    const float f21 = call_float(rt, ctx, 0x08B6A1ACu, {argument0, argument1, argument2, argument3}, {f20}).f0;
    const u32 mode = word(rt, 0x08C96698u);
    if (mode == 0xFFFFFFFFu) {
        ctx.fpr[0] = f21;
        return;
    }
    if (call(rt, ctx, 0x08B6ECB0u, {argument0, argument1, argument2, argument3}) != 0u) {
        ctx.fpr[0] = f21;
        return;
    }
    const float probe = call_float(rt, ctx, 0x08B663A4u, {argument0, argument1, argument2, argument3}, {f20}).f0;
    if (!(float_at(rt, 0x08CB62B4u) < probe)) {
        ctx.fpr[0] = f21;
        return;
    }
    put(rt, frame.at(0u), 1u);
    put(rt, frame.at(4u), 0x08CB5E3Cu);
    put(rt, frame.at(0x20u), 0u);
    const u32 request = call_float(rt, ctx, 0x08B70F28u, {argument0, argument1, argument2, argument3}, {f20}).v0;
    put(rt, frame.at(8u), request);
    put(rt, frame.at(0x10u), request);
    put(rt, frame.at(0xCu), 1u);
    put(rt, frame.at(0x14u), 1u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    bool skipped = mode == 2u;
    if (!skipped) skipped = call(rt, ctx, 0x08B6E524u, {frame.at(0u)}) == 0u;
    if (skipped) {
        const u32 slot = call(rt, ctx, 0x08B77D40u, {});
        put(rt, slot, 0x21u);
    }
    if (word(rt, frame.at(0x20u)) != 0u) {
        const u32 slot = call(rt, ctx, 0x08B77D40u, {});
        put(rt, slot, word(rt, frame.at(0x20u)));
    }
    ctx.fpr[0] = call_float(rt, ctx, 0x08B71EC8u, {word(rt, frame.at(0x18u)), word(rt, frame.at(0x1Cu))}, {}).f0;
}

// 0x08B73D9C (a0 = a scene, a1 = a holder): with word(a1 + 0x74) zero, a byte 4 at 0x08D72BB4 + 0x1D makes a1's +0x74
// point at a scratch word holding word(a1 + 0x4CC) (else 0x08B77B04 aborts with that word). The 306 bytes of 0x08D72BB4 are
// then walked four words at a time against the words at a0 and word(a1 + 0x74); a pair of non-zero, different words goes to
// 0x08B79A88(left, right, byte). When word(a0 + 0x74) is zero the answer is
// (word(word(a1 + 0x74)) - word(a0 + 0x4CC)) + word(a1 + 0x4E4) (0x08B77B04 again when that table byte is not 4); when it
// is not, v0 = 0.
void func_08B73D9C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 scene = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    ctx.gpr[21] = scene;
    ctx.gpr[20] = holder;
    ctx.gpr[22] = 0x08D72BB4u;
    if (word(rt, holder + 0x74u) == 0u) {
        if (rt.memory().load8(0x08D72BB4u + 0x1Du) != 4u) {
            call(rt, ctx, 0x08B77B04u, {word(rt, holder + 0x4CCu)});
            return;
        }
        put(rt, frame.at(0u), word(rt, holder + 0x4CCu));
        put(rt, holder + 0x74u, frame.at(0u));
    }
    u32 left = scene;
    u32 right = holder;
    u32 table = 0x08D72BB4u;
    do {
        const u32 argument1 = word(rt, right);
        const u32 argument0 = word(rt, left);
        if (argument1 != 0u && argument0 != 0u && argument0 != argument1)
            call(rt, ctx, 0x08B79A88u, {argument0, argument1, rt.memory().load8(table)});
        table += 1u;
        left += 4u;
        right += 4u;
    } while (table != 0x08D72CE6u);
    if (word(rt, scene + 0x74u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (rt.memory().load8(0x08D72BB4u + 0x1Du) != 4u) {
        call(rt, ctx, 0x08B77B04u, {word(rt, holder + 0x74u)});
        return;
    }
    const u32 answer = (word(rt, word(rt, holder + 0x74u)) - word(rt, scene + 0x4CCu)) + word(rt, holder + 0x4E4u);
    ctx.gpr[2] = answer;
}

// 0x08996CBC (a0 = this): word(this + 0x20) is cleared and 0x08996A18(sp + 0x18, this, f12 = 0) runs. Then 7 times
// (f21 = 0.125, 0.25, ..., 1.0, s2 counting down from 7) 0x08996A18(sp + 0x20, this, f21) gives a vector whose first two
// floats go to sp + 0x10 and 0x14; 0x08B4AC30(sp + 0x10, sp + 0x10, sp + 0x18) and 0x08B4A708(sp + 0x10) reduce it to a
// length, the vector's first two floats go on to sp + 0x18 and 0x1C, and the length's square root accumulates into
// this + 0x20. A last 0x08996A18(sp, this, 1.0) vector goes the same way through sp + 8 and is added. f0 = the total.
void func_08996CBC(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x60u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[17] = self;
    ctx.gpr[19] = frame.at(0x18u);
    ctx.gpr[20] = frame.at(0x20u);
    ctx.gpr[16] = frame.at(0x10u);
    ctx.gpr[18] = 7u;
    float f21 = 0.0f;
    const float step = std::bit_cast<float>(0x3E000000u);
    put_float(rt, self + 0x20u, 0.0f);
    call_float(rt, ctx, 0x08996A18u, {frame.at(0x18u), self}, {0.0f});
    for (;;) {
        f21 += step;
        call_float(rt, ctx, 0x08996A18u, {frame.at(0x20u), self}, {f21});
        put_float(rt, frame.at(0x10u), float_at(rt, frame.at(0x20u)));
        put_float(rt, frame.at(0x14u), float_at(rt, frame.at(0x24u)));
        const float accumulated = float_at(rt, self + 0x20u);
        call_float(rt, ctx, 0x08B4AC30u, {frame.at(0x10u), frame.at(0x10u), frame.at(0x18u)}, {});
        const float length = call_float(rt, ctx, 0x08B4A708u, {frame.at(0x10u)}, {}).f0;
        put_float(rt, frame.at(0x18u), float_at(rt, frame.at(0x20u)));
        put_float(rt, frame.at(0x1Cu), float_at(rt, frame.at(0x24u)));
        const float total = accumulated + psp_sqrt(length);
        --ctx.gpr[18];
        put_float(rt, self + 0x20u, total);
        if (ctx.gpr[18] == 0u) break;
    }
    call_float(rt, ctx, 0x08996A18u, {frame.at(0u), self}, {1.0f});
    put_float(rt, frame.at(8u), float_at(rt, frame.at(0u)));
    put_float(rt, frame.at(0xCu), float_at(rt, frame.at(4u)));
    call_float(rt, ctx, 0x08B4AC30u, {frame.at(8u), frame.at(8u), frame.at(0x18u)}, {});
    const float last = call_float(rt, ctx, 0x08B4A708u, {frame.at(8u)}, {}).f0;
    const float answer = float_at(rt, self + 0x20u) + psp_sqrt(last);
    put_float(rt, self + 0x20u, answer);
    ctx.fpr[0] = answer;
}

// 0x08ACEBC4 (a0 = this): the constructor. Vtable 0x08CD6B28 at +0, 0x08B2EE10(this + 4, 2, 0x300) and
// 0x08B2EE10(this + 0x18, 2, 0x303), 0x08B2EDD4(this + 0x2C, 0x08CB49D4), 0x08B0CC04(this + 0x40), two zero bytes at +0x160
// and +0x162, 0x08B28DCC(this + 0x164), this + 0x240 stored at +0x244, zeros at +0x300, +0x30C, +0x314, +0x31C, +0x328,
// +0x32C and +0x330, 0x08B28D34 on +0x334, +0x360, +0x38C, +0x448 and +0x474, 0x08B28DCC on +0x3B8, +0x420 and +0x4A0,
// 4 at +0x510, -1 at +0x514, 0 at +0x518, 0x08B2EEB8 on +4, +0x18 and +0x2C, 0x08C7AD70(), and 0 at +0x240. v0 = its answer.

// 0x089CB2B4 (a0 = this): the vtables 0x08CC9FD8 and 0x08CCA230 (at +0x38) are stored and the four containers at +0x150,
// +0x144, +0x138 and +0x12C are released: 0x08C01F00 (the first two) or 0x08BEEA84 (the last two) on the block's begin and
// end, then 0x08A5BC54 on the block when its begin is set (the byte distance for the first two, the word count times four
// for the last two). 0x08A0DD40(this) finishes.
void func_089CB2B4(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[17] = self;
    ctx.gpr[20] = self + 0x12Cu;
    ctx.gpr[19] = self + 0x138u;
    ctx.gpr[18] = self + 0x144u;
    ctx.gpr[16] = self + 0x150u;
    put(rt, self, 0x08CC9FD8u);
    put(rt, self + 0x38u, 0x08CCA230u);
    call(rt, ctx, 0x08C01F00u, {word(rt, self + 0x150u), word(rt, self + 0x154u), frame.at(3u)});
    const u32 first = word(rt, self + 0x150u);
    if (first != 0u)
        call(rt, ctx, 0x08A5BC54u, {first, word(rt, self + 0x158u) - first, 4u});
    call(rt, ctx, 0x08C01F00u, {word(rt, self + 0x144u), word(rt, self + 0x148u), frame.at(2u)});
    const u32 second = word(rt, self + 0x144u);
    if (second != 0u)
        call(rt, ctx, 0x08A5BC54u, {second, word(rt, self + 0x14Cu) - second, 4u});
    call(rt, ctx, 0x08BEEA84u, {word(rt, self + 0x138u), word(rt, self + 0x13Cu), frame.at(1u)});
    const u32 third = word(rt, self + 0x138u);
    if (third != 0u) {
        const u32 count = static_cast<u32>(static_cast<s32>(word(rt, self + 0x140u) - third) >> 2) << 2;
        call(rt, ctx, 0x08A5BC54u, {third, count, 4u});
    }
    call(rt, ctx, 0x08BEEA84u, {word(rt, self + 0x12Cu), word(rt, self + 0x130u), frame.at(0u)});
    const u32 fourth = word(rt, self + 0x12Cu);
    if (fourth != 0u) {
        const u32 count = static_cast<u32>(static_cast<s32>(word(rt, self + 0x134u) - fourth) >> 2) << 2;
        call(rt, ctx, 0x08A5BC54u, {fourth, count, 4u});
    }
    call(rt, ctx, 0x08A0DD40u, {self});
}

// 0x08918498 (a0 = an array, a1 = an index, a2 = three floats, a3 = two floats out): word(a0 + index * 4 + 8) is the
// object. 0x08927A88(sp + 0x60, object) and 0x08B4A774(sp + 0x10, sp + 0x60, {x, y, z, 1}) build a matrix,
// 0x08928D74(sp + 0x20, object) a second one that 0x08B4A774(sp, sp + 0x20, the first) combines. With the combined w
// negative the answer is (0, 0) and v0 = 0; otherwise out[0] = x/z * 0.5 + 0.5, out[1] = 0.5 - y/z * 0.5 and v0 = 1.

// 0x08A72364 (a0 = this): 0x08B85EB8(sp + 4, 0x08CB0AC8, sp + 1) formats the class name and the virtual +0x108(this, sp + 4)
// runs; the string at sp + 4 is released (0x08B60804 + maybe 0x08B8542C(sp)). 0x08A76A94() answers the new object (released
// with 0x08A5BA08 when nonzero) that replaces word(this + 0x38) (0x08A5B930 on the old one). Then the native registrations
// through 0x08B87108 (0x08CB0AD0 -> 0x08A71F70, 0x08CB0AD8 -> 0x08A7210C, 0x08CB0AE0 -> 0x08A71EC8, 0x08CB0AE8 ->
// 0x08A71F1C) and 0x08B8D51C(0x08CB0AF0, 0x08A724B4, 0).
void func_08A72364(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[18] = self;
    ctx.gpr[16] = frame.at(4u);
    ctx.gpr[17] = word(rt, word(rt, self) + 0x108u);
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CB0AC8u, frame.at(1u)});
    call_indirect(rt, ctx, ctx.gpr[17], {self, frame.at(4u)});
    const u32 text = word(rt, frame.at(4u));
    if (text - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {text - 0xCu, frame.at(0u)});
    }
    const u32 created = call(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = created;
    if (created != 0u) call(rt, ctx, 0x08A5BA08u, {created});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, created);
    call(rt, ctx, 0x08B87108u, {self, 0x08CB0AD0u, 0x08A71F70u});
    call(rt, ctx, 0x08B87108u, {self, 0x08CB0AD8u, 0x08A7210Cu});
    call(rt, ctx, 0x08B87108u, {self, 0x08CB0AE0u, 0x08A71EC8u});
    call(rt, ctx, 0x08B87108u, {self, 0x08CB0AE8u, 0x08A71F1Cu});
    call(rt, ctx, 0x08B8D51Cu, {self, 0x08CB0AF0u, 0x08A724B4u, 0u});
}

// 0x089BB55C (a0 = this, a1 = another): 0x089BA708(this, another) first. While word(this + 0x44) is set, the lists at its
// +0xC and at word(another + 0x44) + 0xC are walked in lockstep by +0x54 and each pair goes through this function again,
// then both holders advance by +0x18. The two 0x08A76AA0(sp + 8) / 0x08A5D438(sp + 0xC) locals are filled, the second
// hands (this + 0x1C...) to the virtual +0x10C with 0x08CF6F30, and word(sp + 0xC) goes through 0x089BAC3C(sp + 4, sp) and
// its virtual +0x1C(sp + 8, sp + 4). Then word(sp + 8)'s virtual +0xCC(sp + 0x10, 0, 0) runs (0x08A5BB7C when it is
// missing) and the three locals are released with 0x08A5B9EC. v0 = the last release's answer.

// 0x089E02C4 (a0 = a state block, a1 = an object, a2 = a vector, a3 = an argument): after two timing spins (0x089E0300 and
// 0x089E030C, no memory touched) 0x089A8B68(object) and 0x089A8B80(object) answer two floats that scale the block's +0x1E8
// / +0x1EC (the vector's +0x18) and +0x1F8 / +0x1FC (the vector's +0x1C) into the eight words at sp: 0, 0, the two answers,
// the four products. The virtual +0x110(object, a3, a2, a3) runs, the byte at +0x118 is set, bits 7, 8 and 9 of
// word(block + 0x1B0) go to +0x150, +0x151 and +0x152, 0x089641C4(object + 0x160, block + 0x1C0), +0x170 / +0x174 take
// the block's +0x1E0 / +0x1E4, +0x11C takes a2, 0x089A932C(object, sp, sp + 0x10), 0x089A8C24(object) and the virtual
// +0x114(object). v0 = the last virtual's answer.

// 0x08A4F8B4 (a0 = a table key, a1 = an object, a2 = a record or zero): with a2 set, 0x08C7AC90(a1, a2, 0, sp) answers a
// result that stands unless word(sp) reports an error (then 0). Without a2 the 0xA8-byte request at sp + 4 is cleared, its
// float at +0 is 20.0 and its u16s at +0x14, +0x16 and +0x1A are 0, 0 and 2; when word(0x08CDF65C) is set the 0x14-byte
// entries of 0x08CDF670 are walked (0x08C1CDCC(a0) asking whether to go on) and the first entry it accepts fills that
// request from 0x08CDF64C + index * 0x14 (float +4, u16s from +0, +8 and +0xC). 0x08C7AC70(a1, sp + 4, sp) then
// 0x08C7ACA0(a1, its answer, 0, sp) run; word(sp) reporting an error answers 0. v0 = the answer.

// 0x08862150 (a0 = an owner, a1 = an index): the record is word(word(word(a0) + 0x3F50)) + index * 392 and
// 0x08866434(word(a0) + 0x401C, index, word(word(a0) + 0x3F50), its first word) answers s3. 0x0885D5B0(record) answers s2.
// The 0x24-byte request at sp gets the index at +0, s2's low byte at +0xA, "record's byte +0x15 is set" at +4,
// 0x08866654(sp, record + 4), 0x088666B4(sp, record), record's +0x28 low byte at +0xB, its +0x38 / +0x3C at +0x10 / +0x14,
// its +0x18 at +0x18 and -1 bytes at +0x20..+0x22; +8 / +9 are 0 or 1 by that byte at +0x15. 0x0885D54C(record) answers s0,
// +0x4C is 2, and 0x088661B8(s3, 0x08866174(), sp, s0, s2, record + 0x58) runs. 0x08866174() then goes with
// word(word(a0) + 0x5AC) and s3 to 0x08DEA1C; a zero answer sets +0x4C to 3. v0 = its answer.

// 0x089795AC (a0 = an object): 0x08978000(sp + 0x14) fills a first smart pointer (0x08A5BB7C when it came out empty), and
// 0x089772F4(it, sp + 0x18, sp + 0x1C, sp + 0x20, sp + 0x24) and 0x08A98C1C(sp + 0x28, 4) fill the four floats and the list.
// Each of the four (temp sp + 0x10, +0xC, +8, +4; float sp + 0x18, +0x1C, +0x20, +0x24; slot word(word(sp + 0x28) + 0x14)
// + 0, +4, +8, +0xC) goes through 0x08A87EEC(temp, float), 0x08A5BAD8(slot, word(temp)) when they differ, and
// 0x08A5B9EC(temp). 0x08A983E8(sp, sp + 0x28) then moves into the object (0x08A5BAD8 when they differ) and the three locals
// are released. v0 = the last release's answer.

// 0x08BD48FC switches on the word at +0x10: 3 hands the float at +0x18 with (a1, this) to 0x08AF2978 and returns; anything
// but 2 goes to 0x08AF213C(this). For 2 it reads the table held at word(this + 4) (word +0x30 entries, u16 +0x10 of
// 0x30-byte records), asks 0x089063A8 whether the key matches one record (the signed byte at +0x14 when the word at +0x0C is
// negative, or its value as an index) and then formats that record's float at +4 through 0x08906304 into 0x08AF2978(a1,
// record). The split when the word at +0x0C is negative is "every record" (0x08BD2EB4 supplies the index each time) against
// one record. v0 = the last call's answer (the table span when the loop is empty).
void func_08BD48FC(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 kind = word(rt, self + 0x10u);
    if (kind == 3u) {
        call_float(rt, ctx, 0x08AF2978u, {argument, self}, {float_at(rt, self + 0x18u)});
        return;
    }
    if (kind != 2u) {
        call(rt, ctx, 0x08AF213Cu, {self});
        return;
    }
    const u32 formatter = call(rt, ctx, 0x08BD2E04u, {frame.at(4u), argument, 0u});
    const u32 key = call(rt, ctx, 0x08BD2DDCu, {argument, static_cast<u32>(signed_byte_at(rt, self + 0x14u))});
    const u32 index = word(rt, self + 0xCu);
    const u32 object = word(rt, word(rt, self + 4u));
    if (static_cast<s32>(index) >= 0) {
        if (call(rt, ctx, 0x089063A8u, {object, key, index}) == 0u) return;
        const u32 record = word(rt, object + 0x30u) + index * 48u;
        const GuestResult formatted = call_float(rt, ctx, 0x08906304u, {formatter, record + 4u}, {});
        call_float(rt, ctx, 0x08AF2978u, {argument, self}, {formatted.f0});
        return;
    }
    const u32 entries = word(rt, object + 0x30u);
    const u32 span = static_cast<u32>(rt.memory().load16(object + 0x10u)) * 48u;
    u32 record = entries;
    const u32 end = entries + span;
    u32 entry_index = 0u;
    ctx.gpr[2] = span;
    if (record == end) return;
    do {
        if (call(rt, ctx, 0x089063A8u, {object, key, entry_index}) != 0u) {
            put(rt, frame.at(0u), entry_index);
            const u32 value = call(rt, ctx, 0x08BD2EB4u, {self, frame.at(0u), entry_index});
            const GuestResult formatted = call_float(rt, ctx, 0x08906304u, {formatter, record + 4u}, {});
            call_float(rt, ctx, 0x08AF2978u, {argument, value}, {formatted.f0});
        }
        record += 0x30u;
        ++entry_index;
    } while (record != end);
}

} // namespace
} // namespace new_batch_main_1886
} // namespace gt::game

#define NEW1624(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1886::name, gt::ReturnKind::kind)
NEW1624(0x0881BAC0u, func_0881BAC0, Word);
NEW1624(0x08B66D54u, func_08B66D54, Float);
NEW1624(0x08B73D9Cu, func_08B73D9C, Word);
NEW1624(0x08996CBCu, func_08996CBC, Float);
NEW1624(0x089CB2B4u, func_089CB2B4, Word);
NEW1624(0x08A72364u, func_08A72364, Word);
NEW1624(0x08BD48FCu, func_08BD48FC, Word);
