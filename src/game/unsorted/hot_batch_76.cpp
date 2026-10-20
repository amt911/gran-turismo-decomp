// Hot batch 76: picked by reach, not by shape. The call probe is the one of batches 70-75 (every function of up to 70 instructions not yet decompiled,
// run on the five routes with --decomp off; batches 74 and 75 excluded with --exclude-ref), but instead of taking the families a template already
// covers, the reached functions were ordered by calls and by decompiled callers, and each one was read by hand against the asm (Ghidra / rabbitizer
// output and the generated code were reading aids only). Their classes aren't known yet, so they keep their addresses as names.
//
// Read one by one:
//  - display-list writers over the state at 0x08D6CE00 (write pointer +0x5C, flags +0x6C): func_08B54694, func_08B55AAC, func_08B58C9C, func_08B55AF8
//    and func_08B57780;
//  - float helpers: func_08B4AD64 (a pair through func_08B4AD10), func_08B57D6C / func_08B52E00 (a float handed to the VFPU loader func_08B57E40),
//    func_08B67C24 (func_08B66524 / func_08B662C4 of an angle), func_08B49F3C (pi/2 minus func_08B49D98), func_08B4BA68, func_08AF432C and its caller
//    func_08872484;
//  - func_08B71EC8 and func_08B70F28 (unpack a value through a helper and hand the words, with sticky bits folded into a3 / t0 / t1, to the next one);
//  - func_08B34C1C (moves a node between two lists between the interrupt-suspend and -resume stubs), func_08B89124, func_089CBFE4, func_08A7BFD0
//    (virtual calls through a temporary) and func_089444F8 (table lookups).
//
// Families (one member read in full by hand, the others compared with it word by word: `tools.family_members` prints no `!!` line; each member's
// constants were decoded from its own relocated words and `--check` re-reads every row of this file against them):
//  - "init handle with an index" (18 instructions, the 16-instruction family of batches 66 / 73 plus a3): 10 functions, all with the initialiser func_08B9DE60;
//  - "pair call D" (19 instructions): the static pair, a2's pair, a worker call; 9 functions;
//  - "relocation loop" (36 instructions): three of its members are called by hot_leaves' func_08AF771C; 9 functions;
//  - "handle wrapper" (21 instructions): a variadic maker returns the payload stored in a handle; 6 functions (func_08B9DE60 is the maker the init handles use);
//  - "guarded static object, constructor without a size" (35 instructions; batch 70's shape): 8 functions.
//
// Written but not registered (the gate could not compare them: the original reaches an import the harness does not replay, or blocks): func_08AD96A0,
// func_08AEB908, func_08AEB930, func_08B0C650, func_08B33A24, func_08B52D58, func_08B58E28, func_08B58E84, func_08B590D0, func_08B83C84 and func_08B84270.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx. Where the original ends with a tail call to an import stub, v0 is set as the original leaves it
// (the HLE of the interrupt-resume stub does not write v0).

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_76 {

using namespace gt::game::hot;

// ---- the graphics library's display-list writers ---------------------------------------------------------------------------------------
// (kGuState = 0x08D6CE00: write pointer +0x5C, flag word +0x6C; each command word is the opcode in the top byte.)

// func_08B54694(a0, a1): appends seven command words to the list: 0xB2/0xB3 and 0xB4/0xB5 pairs carrying a0 and a1 (24 low bits in the first of each
// pair, the top byte of the argument in bits 16-23 of the second, 0x100 and 0x10 in the low bits), 0x10 / 0x0A words built from the word at 0x08D6CDE4
// (top byte, low 24 bits) and the terminating 0xCC000000. v0 = 0xCC000000, v1 = the new write pointer.
// Branches the routes do not take (checked against the asm by hand): none, the function is straight-line.
void func_08B54694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5];
    const std::uint32_t t7 = word(rt, 0x08D6CDE4u);
    const std::uint32_t list = word(rt, kGuListPointer);
    const std::uint32_t second = 0xB3000000u | ((a0 >> 24) << 16) | 0x100u;
    const std::uint32_t fourth = 0xB5000000u | ((a1 >> 24) << 16) | 0x10u;
    const std::uint32_t fifth = 0x10000000u | ((t7 >> 24) << 16);
    const std::uint32_t sixth = 0x0A000000u | (t7 & 0xFFFFFFu);
    const std::uint32_t first = 0xB2000000u | (a0 & 0xFFFFFFu);
    const std::uint32_t third = 0xB4000000u | (a1 & 0xFFFFFFu);
    put(rt, list, first);
    put(rt, list + 4u, second);
    put(rt, list + 8u, third);
    put(rt, list + 12u, fourth);
    put(rt, list + 16u, fifth);
    put(rt, list + 20u, sixth);
    put(rt, list + 24u, 0xCC000000u);
    put(rt, kGuListPointer, list + 0x1Cu);
    ctx.gpr[2] = 0xCC000000u;
    ctx.gpr[3] = list + 0x1Cu;
}

// func_08B55AAC(a0, a1): func_08B5596C(a0, a1, 0x08D6D048) returns a mask; it is or-ed into the words +0x27C and +0x278 of the state, and flag 0x80 is
// set in +0x6C. v0 = mask | the old +0x278, v1 = the state.
void func_08B55AAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mask = call_guest(rt, ctx, 0x08B5596Cu, {ctx.gpr[4], ctx.gpr[5], 0x08D6D048u});
    const std::uint32_t high = word(rt, kGuState + 0x27Cu);
    const std::uint32_t low = word(rt, kGuState + 0x278u);
    const std::uint32_t flags = word(rt, kGuFlags);
    put(rt, kGuFlags, flags | 0x80u);
    put(rt, kGuState + 0x27Cu, mask | high);
    put(rt, kGuState + 0x278u, mask | low);
    ctx.gpr[2] = mask | low;
    ctx.gpr[3] = kGuState;
}

// func_08B58C9C(a0, a1, a2, a3): records a draw's parameters in the state (+0xB8 a0, +0xBC a1, +0xC8 the float at 0x08CB61FC, +0xCC 0, halfwords +0xD0
// a2, +0xD2 a3, +0xD4 (a2 + 63) & ~63), appends two commands (0x9E000000 | a0, 0x9F000000 | a0's top byte << 16 | the low 11 bits of the rounded a2) and
// tail calls func_08B58B5C(state, the new write pointer, a2, a3, the second command, the rounded a2, the first command, the old write pointer).
void func_08B58C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    const std::uint32_t list = word(rt, kGuListPointer);
    const float value = float_at(rt, 0x08CB61FCu);
    const std::uint32_t rounded = (a2 + 0x3Fu) & ~0x3Fu;
    const std::uint32_t second = 0x9F000000u | ((a0 >> 24) << 16) | (rounded & 0x7FFu);
    const std::uint32_t first = 0x9E000000u | (a0 & 0xFFFFFFu);
    put(rt, kGuState + 0xBCu, a1);
    put_float(rt, kGuState + 0xC8u, value);
    rt.memory().store16(kGuState + 0xD0u, static_cast<std::uint16_t>(a2));
    rt.memory().store16(kGuState + 0xD2u, static_cast<std::uint16_t>(a3));
    put(rt, kGuState + 0xB8u, a0);
    put(rt, kGuState + 0xCCu, 0u);
    rt.memory().store16(kGuState + 0xD4u, static_cast<std::uint16_t>(rounded));
    put(rt, list, first);
    put(rt, list + 4u, second);
    put(rt, kGuListPointer, list + 8u);
    ctx.gpr[2] = 0x08CB0000u;
    ctx.gpr[3] = kGuState;
    ctx.fpr[0] = value;
    tail_call(ctx, 0x08B58B5Cu, {kGuState, list + 8u, a2, a3, second, rounded, first, list});
}

// func_08B4AD64(a0, a1, a2): copies the two floats at a2 to the stack and calls func_08B4AD10(a0, a1, the copy). v0 = its v0.
void func_08B4AD64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t second = word(rt, ctx.gpr[6] + 4u);
    const std::uint32_t first = word(rt, ctx.gpr[6]);
    put(rt, frame.at(4u), second);
    put(rt, frame.at(0u), first);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B4AD10u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u)});
}

// func_08B57D6C(a0..): f15 = the float at 0x08CB61C8 for func_08B57E40, whose result is passed to func_08B57D40 as its only argument (a tail call).
void func_08B57D6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0x08CB0000u;
    ctx.fpr[15] = float_at(rt, 0x08CB61C8u);
    const std::uint32_t result = call_guest(rt, ctx, 0x08B57E40u, {});
    tail_call(ctx, 0x08B57D40u, {result});
}

// func_08B67C24(f12 = angle, a0 = &sin, a1 = &cos): *a0 = func_08B66524(angle), *a1 = func_08B662C4(angle). f0 = the second result.
void func_08B67C24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float angle = ctx.fpr[12];
    const std::uint32_t sine_out = ctx.gpr[4], cosine_out = ctx.gpr[5];
    const GuestResult sine = call_guest(rt, ctx, 0x08B66524u, {}, {angle});
    put_float(rt, sine_out, sine.f0);
    const GuestResult cosine = call_guest(rt, ctx, 0x08B662C4u, {}, {angle});
    put_float(rt, cosine_out, cosine.f0);
    ctx.fpr[0] = cosine.f0;
    ctx.gpr[2] = cosine.v0;
    ctx.gpr[3] = cosine.v1;
}

// func_08B34C1C(this): between func_08B2E6DC and the import stub func_08C7A888 (whose result is handed to the stub func_08C7A8A8 by the final tail call):
// when this[8] is non-zero and func_08B2E6DC's result equals this[6], the node n = this[2] (if any) is unlinked from the first list (this[2] = n[0],
// this[3] = 0 when that is null), n[0] = 0, and linked at the end of the second one: the old tail this[5] (this[4] when it is null) points to n and
// this[5] = n. v0 at the tail call is the last word the original loaded (the stub for func_08C7A8A8 does not set it), so it is reproduced per path.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): this[8] == 0 (the beqz lands on the epilogue, v0 = 0), the
// thread id differing from this[6] (the bnel lands on the epilogue with v0 = this[6]), this[2] == 0 (the beql lands there too, v0 = this[6]), the node
// being the last one (the delay slot of the bnez stores n[0] = 0 into this[2] either way, then this[3] = 0), and this[5] == 0 (the beql's delay slot
// stores the node in this[4]; v0 = this[5] = 0). The routes only take non-zero this[8], equal ids, a node with a successor and a non-empty tail.
void func_08B34C1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t thread = call_guest(rt, ctx, 0x08B2E6DCu, {});
    const std::uint32_t flags = call_guest(rt, ctx, 0x08C7A888u, {});
    // v0 at the tail call is whatever the last load left (the import stub does not set it): the word +0x20 (0 on that path), then +0x18, then the tail.
    std::uint32_t v0 = word(rt, self + 0x20u);
    if (v0 != 0u) {
        v0 = word(rt, self + 0x18u);
        if (thread == v0) {
            const std::uint32_t node = word(rt, self + 8u);
            if (node != 0u) {
                const std::uint32_t next = word(rt, node);
                put(rt, self + 8u, next);
                if (next == 0u) put(rt, self + 12u, 0u);
                put(rt, node, 0u);
                v0 = word(rt, self + 0x14u);
                if (v0 == 0u) put(rt, self + 0x10u, node);
                else put(rt, v0, node);
                put(rt, self + 0x14u, node);
            }
        }
    }
    ctx.gpr[2] = v0;
    tail_call(ctx, 0x08C7A8A8u, {flags});
}

// func_08B71EC8(a0, a1): func_08B72314(&{a0, a1}, &out) fills out[0..7]; func_08B72004(out[0], out[1], out[2], a3, t0, 0) is called with
// a3 = out[7] << 2 | out[6] >> 30 | t0 and t0 = whether the low 30 bits of out[6] are non-zero. v0 / v1 / f0 = its own: the float comes
// back in $f0 (func_08B72564 ends every path with `mtc1 $a2, $f0`), so callers (func_08B71EC8(func_08B77BF4(text)) in hot_batch_94) read f0, not v0.
void func_08B71EC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(0u), ctx.gpr[4]);
    put(rt, frame.at(4u), ctx.gpr[5]);
    (void)call_guest(rt, ctx, 0x08B72314u, {frame.at(0u), frame.at(8u)});
    const std::uint32_t low = word(rt, frame.at(0x18u));
    const std::uint32_t high = word(rt, frame.at(0x1Cu));
    const std::uint32_t a0 = word(rt, frame.at(8u));
    const std::uint32_t a1 = word(rt, frame.at(0xCu));
    const std::uint32_t a2 = word(rt, frame.at(0x10u));
    const std::uint32_t sticky = (low & 0x3FFFFFFFu) != 0u ? 1u : 0u;
    const std::uint32_t a3 = sticky | ((high << 2) | (low >> 30));
    const GuestResult packed = call_guest(rt, ctx, 0x08B72004u, {a0, a1, a2, a3, sticky, 0u}, {});
    ctx.gpr[2] = packed.v0;
    ctx.gpr[3] = packed.v1;
    ctx.fpr[0] = packed.f0;
}

// func_08B70F28(f12): func_08B71F2C(&x, &out) fills out[0..4] from the float x (kept on the stack); func_08B71E98(out[0], out[1], out[2], a3,
// out[3] << 30, out[3] >> 2) follows. v0 / v1 = its 64-bit result (callers read v1).
void func_08B70F28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(0u), std::bit_cast<std::uint32_t>(ctx.fpr[12]));
    (void)call_guest(rt, ctx, 0x08B71F2Cu, {frame.at(0u), frame.at(4u)});
    const std::uint32_t t0 = word(rt, frame.at(0x10u));
    const std::uint32_t a0 = word(rt, frame.at(4u));
    const std::uint32_t a1 = word(rt, frame.at(8u));
    const std::uint32_t a2 = word(rt, frame.at(0xCu));
    const GuestResult packed = call_guest(rt, ctx, 0x08B71E98u, {a0, a1, a2, ctx.gpr[7], t0 << 30, t0 >> 2}, {});
    ctx.gpr[2] = packed.v0;
    ctx.gpr[3] = packed.v1;
}

// func_08AF432C(a0, a1): func_08B509EC with the eight floats a1[5..12] (f12-f19) and a1[13] on the stack. v0 = a0.
void func_08AF432C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[0] = float_at(rt, source + 0x34u);
    put(rt, frame.at(0u), word(rt, source + 0x34u));
    (void)call_guest(rt, ctx, 0x08B509ECu, {},
                     {float_at(rt, source + 0x14u), float_at(rt, source + 0x18u), float_at(rt, source + 0x1Cu), float_at(rt, source + 0x20u),
                      float_at(rt, source + 0x24u), float_at(rt, source + 0x28u), float_at(rt, source + 0x2Cu), float_at(rt, source + 0x30u)});
    ctx.gpr[2] = self;
}

// func_08872484(out, obj): out = the matrix func_08AF432C builds from obj + 12; when bits 0 and 1 of the word at obj[23]'s +0x100 are both set it is
// multiplied (func_08B4E994) by the matrix at obj[23] + 0x130 and copied (func_08B4E734) to out. v0 = out.
// Branches the routes do not take (checked against the asm by hand): the multiplying path (both bits set): the matrix is built in the frame (+0), the
// product goes to +0x40, a2 = obj[23] + 0x130 is loaded after the first call and added in the delay slot of the second, the copy gets (out, frame + 0x40) and
// the function leaves through the `j` with v0 = out. The routes only reach the single call with the destination `out`.
void func_08872484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], obj = ctx.gpr[5];
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t flags = word(rt, word(rt, obj + 0x5Cu) + 0x100u);
    if ((flags & 1u) != 0u && ((flags >> 1) & 1u) != 0u) {
        (void)call_guest(rt, ctx, 0x08AF432Cu, {frame.at(0u), obj + 0xCu});
        const std::uint32_t basis = word(rt, obj + 0x5Cu) + 0x130u;
        (void)call_guest(rt, ctx, 0x08B4E994u, {frame.at(0x40u), frame.at(0u), basis});
        (void)call_guest(rt, ctx, 0x08B4E734u, {out, frame.at(0x40u)});
    } else {
        (void)call_guest(rt, ctx, 0x08AF432Cu, {out, obj + 0xCu});
    }
    ctx.gpr[2] = out;
}

// func_08B57780(a0, a1): func_08B57714(state, func_08B57E68(a1)) (a tail call).
void func_08B57780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t value = call_guest(rt, ctx, 0x08B57E68u, {ctx.gpr[5]});
    tail_call(ctx, 0x08B57714u, {kGuState, value});
}

// func_08AEB930(a0): a1 = 1; when a0 is non-null, func_08C7AA08(*a0, 1) (an import). v0 = its v0, or unchanged.
void func_08AEB930(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    ctx.gpr[5] = 1u;
    if (handle != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08C7AA08u, {word(rt, handle), 1u});
}

// func_08AEB908(a0): the same with the import func_08C7AA20(*a0, 1, 0).
void func_08AEB908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    ctx.gpr[5] = 1u;
    ctx.gpr[6] = 0u;
    if (handle != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08C7AA20u, {word(rt, handle), 1u, 0u});
}

// func_08B4BA68(a0, a1): func_08B4B86C(a0, &tmp) fills three floats, copied to a1[0..2]. v0 = its v0.
void func_08B4BA68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B4B86Cu, {ctx.gpr[4], frame.at(0u)});
    const std::uint32_t y = word(rt, frame.at(4u)), z = word(rt, frame.at(8u)), x = word(rt, frame.at(0u));
    put(rt, out + 4u, y);
    put(rt, out, x);
    put(rt, out + 8u, z);
}

// func_08B52E00(f15): f15 * the float at 0x08CB6184 goes to func_08B57E40; its result is stored at 0x08CE0DC8. v0 = that result.
void func_08B52E00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.fpr[0] = float_at(rt, 0x08CB6184u);
    ctx.fpr[15] = psp_mul(ctx.fpr[15], ctx.fpr[0]);
    const std::uint32_t result = call_guest(rt, ctx, 0x08B57E40u, {});
    put(rt, 0x08CE0DC8u, result);
    ctx.gpr[2] = result;
    ctx.gpr[3] = 0x08CE0000u;
}

// func_08B49F3C(f12): f0 = 0x3FC90FDB (pi / 2 as a float) - func_08B49D98(...). v0 = that function's v0.
void func_08B49F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const GuestResult inner = call_guest(rt, ctx, 0x08B49D98u, {}, {});
    ctx.fpr[0] = std::bit_cast<float>(0x3FC90FDBu) - inner.f0;
    ctx.gpr[2] = inner.v0;
}

// func_08B84270(): 0 while the word at 0x08D740F8 is 0, else func_08C7B068(0x08D74100, that word) (an import).
void func_08B84270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = word(rt, 0x08D740F8u);
    if (handle == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B068u, {0x08D74100u, handle});
}

// func_08B83C84(a0, a1, a2): 0x80420100 while the word at 0x08D740F8 is 0, 0x80420005 when a0 is 0 or not 64-aligned, else the import
// func_08C7B058(0x08D74100, a0, a1, a2).
void func_08B83C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    ctx.gpr[7] = a2;
    if (word(rt, 0x08D740F8u) == 0u) {
        ctx.gpr[2] = 0x80420100u;
    } else if (a0 == 0u || (a0 & 0x3Fu) != 0u) {
        ctx.gpr[2] = 0x80420005u;
    } else {
        ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B058u, {0x08D74100u, a0, a1, a2});
    }
}

// func_08B590D0(): func_08C7AC48(0) (an import); then, when the state's word +0x34 is non-zero, func_08C7AC20(it, 0); the word is cleared.
void func_08B590D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)call_guest(rt, ctx, 0x08C7AC48u, {0u});
    const std::uint32_t handle = word(rt, kGuState + 0x34u);
    ctx.gpr[2] = handle;
    if (handle != 0u) ctx.gpr[2] = call_guest(rt, ctx, 0x08C7AC20u, {handle, 0u});
    put(rt, kGuState + 0x34u, 0u);
}

// func_08B58E84(a0): s = a0, or the state's word +0x2D4 when a0 is 0. func_08C7AAD8() and func_08C7AC48(0) run (imports), then the state's word +0x34 =
// func_08C7AC40(s[2], 0, state[0x30], 0). v0 = that.
void func_08B58E84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t object = ctx.gpr[4];
    if (object == 0u) object = word(rt, kGuState + 0x2D4u);
    (void)call_guest(rt, ctx, 0x08C7AAD8u, {});
    (void)call_guest(rt, ctx, 0x08C7AC48u, {0u});
    const std::uint32_t result = call_guest(rt, ctx, 0x08C7AC40u, {word(rt, object + 8u), 0u, word(rt, kGuState + 0x30u), 0u});
    put(rt, kGuState + 0x34u, result);
    ctx.gpr[2] = result;
}

// func_08B58E28(a0, a1, a2, a3): records a0 / a1 / (a2 + 63) & ~63 / a3 in the state (+0x80 / +0x84 / halfwords +0x98, +0x94 = a2, +0x96 = a3) and tail
// calls the import func_08C7AC08(a0, rounded, a1, changed) where changed is 0 only when a1 and the rounded a2 equal the previous +0x84 / +0x98.
void func_08B58E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    const std::uint32_t rounded = (a2 + 0x3Fu) & ~0x3Fu;
    std::uint32_t changed = 1u;
    if (word(rt, kGuState + 0x84u) == a1 && rounded == rt.memory().load16(kGuState + 0x98u)) changed = 0u;
    rt.memory().store16(kGuState + 0x94u, static_cast<std::uint16_t>(a2));
    rt.memory().store16(kGuState + 0x96u, static_cast<std::uint16_t>(a3));
    rt.memory().store16(kGuState + 0x98u, static_cast<std::uint16_t>(rounded));
    put(rt, kGuState + 0x80u, a0);
    put(rt, kGuState + 0x84u, a1);
    tail_call(ctx, 0x08C7AC08u, {a0, rounded, a1, changed, rounded, a1, kGuState, a0});
}

// func_08B52D58(a0 = &{w0, w1, w2, w3}): func_08B58E28(w0, w1, w2, w3) (a tail call). v0 = a0.
void func_08B52D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[4];
    const std::uint32_t w3 = word(rt, source + 12u);
    const std::uint32_t w1 = word(rt, source + 4u);
    const std::uint32_t w0 = word(rt, source);
    const std::uint32_t w2 = word(rt, source + 8u);
    ctx.gpr[2] = source;
    tail_call(ctx, 0x08B58E28u, {w0, w1, w2, w3});
}

// func_08AD96A0(): func_08AD95EC(0). v0 = its v0.
void func_08AD96A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = call_guest(rt, ctx, 0x08AD95ECu, {0u});
}

// func_08A7BFD0(this, a1, a2): a scratch object tmp (func_08A5D438 builds it, func_08A5B9EC releases it); this->vtable[0x10C / 4](this, &tmp, a2) and
// this->vtable[0x114 / 4](this, a1, &tmp), then v0 = (tmp[0] != 0).
void func_08A7BFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A5D438u, {frame.at(0u)});
    (void)call_guest(rt, ctx, word(rt, word(rt, self) + 0x10Cu), {self, frame.at(0u), a2});
    (void)call_guest(rt, ctx, word(rt, word(rt, self) + 0x114u), {self, a1, frame.at(0u)});
    const std::uint32_t first = word(rt, frame.at(0u));
    (void)call_guest(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = first != 0u ? 1u : 0u;
}

// func_08B0C650(a0, a1): n = func_08B0C43C(a0, a1); -1 when n is 0, else a tail call to n->vtable[0x84 / 4](n, a0, a1, 1).
void func_08B0C650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5];
    const std::uint32_t object = call_guest(rt, ctx, 0x08B0C43Cu, {a0, a1});
    if (object == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const std::uint32_t target = word(rt, word(rt, object) + 0x84u);
    ctx.gpr[2] = target;
    tail_call(ctx, target, {object, a0, a1, 1u});
}

// func_08B89124(a0, a1, a2): func_08B88F44(a0, a1); func_08B89054(a0, a2). v0 = a0.
void func_08B89124(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], third = ctx.gpr[6];
    (void)call_guest(rt, ctx, 0x08B88F44u, {});
    (void)call_guest(rt, ctx, 0x08B89054u, {self, third});
    ctx.gpr[2] = self;
}

// func_08B33A24(a0): func_08B33950(a0), then func_08C7A840(a0) (an import) until it returns 0 or 0x80020323, calling func_08B2E064() after each other result.
void func_08B33A24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    (void)call_guest(rt, ctx, 0x08B33950u, {});
    std::uint32_t result;
    for (;;) {
        result = call_guest(rt, ctx, 0x08C7A840u, {handle});
        if (result == 0u || result == 0x80020323u) break;
        (void)call_guest(rt, ctx, 0x08B2E064u, {});
    }
    ctx.gpr[2] = result;
}

// func_08B55AF8(a0, a1, a2, a3): appends two commands (((a0 + 0xA0) << 24 | a1's low 24 bits, ((a0 + 0xA8) << 24 | a1's top byte << 16 | a3's low
// 11 bits) and sets the pending flags: when flag 8 of state[0x278] is clear or the byte at +0x254 differs from a2, state[0x278] gets 8, [0x27C] gets 8
// and the byte is set to a2; then flags 0x800 are set in +0x278 / +0x27C and 0x80 in +0x6C.
void func_08B55AF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    const std::uint32_t pending = word(rt, kGuState + 0x278u);
    const std::uint32_t second = ((a0 + 0xA8u) << 24) | ((a1 >> 24) << 16) | (a3 & 0x7FFu);
    const std::uint32_t first = ((a0 + 0xA0u) << 24) | (a1 & 0xFFFFFFu);
    if ((pending & 8u) == 0u || a2 != rt.memory().load8(kGuState + 0x254u)) {
        const std::uint32_t other = word(rt, kGuState + 0x27Cu);
        put(rt, kGuState + 0x278u, pending | 8u);
        rt.memory().store8(kGuState + 0x254u, static_cast<std::uint8_t>(a2));
        put(rt, kGuState + 0x27Cu, other | 8u);
    }
    const std::uint32_t list = word(rt, kGuListPointer);
    put(rt, list, first);
    put(rt, list + 4u, second);
    const std::uint32_t other = word(rt, kGuState + 0x27Cu);
    const std::uint32_t low = word(rt, kGuState + 0x278u);
    const std::uint32_t flags = word(rt, kGuFlags);
    put(rt, kGuFlags, flags | 0x80u);
    put(rt, kGuListPointer, list + 8u);
    put(rt, kGuState + 0x27Cu, other | 0x800u);
    put(rt, kGuState + 0x278u, low | 0x800u);
    ctx.gpr[2] = flags | 0x80u;
    ctx.gpr[3] = other | 0x800u;
}

// func_089CBFE4(a0, a1): func_08B85EB8(a0, a1[5], &tmp) between func_08B344E0 and func_08B3457C of the object at 0x08CF4F40. v0 = a0.
void func_089CBFE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08B344E0u, {0x08CF4F40u});
    (void)call_guest(rt, ctx, 0x08B85EB8u, {out, word(rt, source + 0x14u), frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08B3457Cu, {0x08CF4F40u});
    ctx.gpr[2] = out;
}

// func_089444F8(a0, a1, a2, a3): -1 unless 1 <= a1 <= 6. With i = a1 - 1: *a2 (when a2 is non-null) and *a3 (when a3 is non-null) are filled, from the
// bytes +0x16 / +0x17 of the record at a0[3] + 0x270 when i is 1, else from the signed byte tables at 0x08CF2AF0 + i / 0x08CF2AE8 + i. The result is the
// signed byte at a0[2] + i + 0x540.
// Branches the routes do not take (checked against the asm by hand, reading the targets): a1 outside 1..6 (blez / the slti fall to the `jr ra` with v0 = -1) and
// i == 1 (the bne to the table path falls through: a0 + 0x270 is formed in the delay slot of the beqz a2, the bytes +0x16 / +0x17 are zero-extended
// (lbu) and stored; the beql a3 skips to the result with the delay slot v0 = i + a0[2]). The routes only take the table path (i != 1) with a1 in range.
void func_089444F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    const std::uint32_t base = word(rt, self + 8u);
    const std::int32_t a1 = static_cast<std::int32_t>(ctx.gpr[5]);
    if (a1 <= 0 || a1 >= 7) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const std::uint32_t i = static_cast<std::uint32_t>(a1 - 1);
    if (i == 1u) {
        const std::uint32_t record = word(rt, self + 12u) + 0x270u;
        if (a2 != 0u) put(rt, a2, rt.memory().load8(record + 0x16u));
        if (a3 != 0u) put(rt, a3, rt.memory().load8(record + 0x17u));
    } else {
        if (a2 != 0u) put(rt, a2, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(0x08CF2AF0u + i)))));
        if (a3 != 0u) put(rt, a3, static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(0x08CF2AE8u + i)))));
    }
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(i + base + 0x540u))));
}

// ---- shape families (templates read in full by hand; the members' constants decoded with tools.family_members) ----------------------------------

// Init handle with an index (18 instructions; the 16-instruction "init handle" of batches 66 / 73 plus a3): this[0] = this[1] = 0, byte this[8] = 0
// (that one before the call), then initialiser(this, a2 + a1[3] + a3 * 4, 0) with a3 already shifted (the callee receives a3 << 2, and t0-t3 as they
// came). v0 = this.
// Branches: none, the function is straight-line.
void init_handle_indexed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t initialiser) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t shifted = ctx.gpr[7] << 2;
    const std::uint32_t offset = word(rt, ctx.gpr[5] + 12u);
    put(rt, self, 0u);
    const std::uint32_t where = ctx.gpr[6] + offset + shifted;
    put(rt, self + 4u, 0u);
    rt.memory().store8(self + 8u, 0u);
    (void)call_guest(rt, ctx, initialiser, {self, where, 0u, shifted});
    ctx.gpr[2] = self;
}

// Pair call D (19 instructions): frame+0 = the static pair (words at `pairs`, `pairs` + 4), frame+8 = a2's pair; calls `callee`(a0, a1, &frame[0],
// &frame[8], t0 = a3). v0 = its result.
// Branches: none, the function is straight-line.
void pair_call_d(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pairs, std::uint32_t callee) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t second = ctx.gpr[6];
    const std::uint32_t second_hi = word(rt, second + 4u);
    const std::uint32_t second_lo = word(rt, second);
    const std::uint32_t static_hi = word(rt, pairs + 4u);
    const std::uint32_t static_lo = word(rt, pairs);
    put(rt, frame.at(0xCu), second_hi);
    put(rt, frame.at(8u), second_lo);
    put(rt, frame.at(0u), static_lo);
    put(rt, frame.at(4u), static_hi);
    ctx.gpr[2] = call_guest(rt, ctx, callee, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), frame.at(8u), ctx.gpr[7]});
}

// Relocation loop (36 instructions, hot_leaves' func_08AF771C calls six members): a0 = the delta block, a1 = the address of a pointer field, a2 = how
// many elements it points to. `relocate`(a0, a1, a2) fixes the pointer itself; when the pointer is then non-null and a2 is non-zero, `element`(
// pointer + i * stride, a0) runs for i = 0 .. a2 - 1 (the pointer is read again before each call but the first). v0 = the last callee's result.
// Branches the routes do not take (checked against the asm by hand, reading the targets): a2 = 0 with a non-null pointer (the bnez at 0x08C40438 falls to
// the epilogue with v0 = the first callee's; its delay slot only sets s1) and a2 negative (the loop would run until the counter wraps; kept as `!=`). A
// null pointer (the first bnez falls to the epilogue) does occur in the routes.
void relocation_loop(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t relocate, std::uint32_t element, std::uint32_t stride) {
    const std::uint32_t delta = ctx.gpr[4], slot = ctx.gpr[5], count = ctx.gpr[6];
    std::uint32_t result = call_guest(rt, ctx, relocate, {});
    const std::uint32_t first = word(rt, slot);
    if (first != 0u && count != 0u) {
        std::uint32_t offset = 0u, index = 0u;
        do {
            const std::uint32_t base = index == 0u ? first : word(rt, slot);
            ++index;
            result = call_guest(rt, ctx, element, {base + offset, delta});
            offset += stride;
        } while (count != index);
    }
    ctx.gpr[2] = result;
}

// Handle wrapper (21 instructions): the wrapper is variadic (a2-t3 are spilled to its frame, dead), `maker`(a0, a1, a2, ... as they came) returns the
// payload, then this[0] = payload, this[1] = a1, byte this[8] = 0. v0 = the stores' last value is left as the callee returned it.
// Branches: none, the function is straight-line.
void handle_wrapper(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t maker) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t payload = call_guest(rt, ctx, maker, {});
    put(rt, self, payload);
    put(rt, self + 4u, a1);
    rt.memory().store8(self + 8u, 0u);
    ctx.gpr[2] = payload;
}

// Guarded static object, constructor without a size (35 instructions; batch 70's shape minus the size): `static T x(source, 0);` inside a function.
// If the guard byte is 0 and func_08B6441C(guard) lets this caller build it, object != 0: constructor(object, source, 0), object[0] = vtable (stored
// before the next call runs), registrar() (the accessor of another function-local static; it reads no argument); then the tail call
// func_08B64494(guard).
// v0 is hi(guard) when the guard byte is already set, 0 when the acquire fails, else whatever the release leaves.
// Branches the routes do not take (checked against the asm by hand, reading the targets): func_08B6441C answering 0 (the beqz at 0x08B9FB6C lands on the
// epilogue with v0 = 0; its delay slot only sets v1) and the null object (the beqz at 0x08B9FB80 lands on `move a0, s0` and the same tail call, skipping the
// construction; the object is a constant above the load base, so it cannot happen).
void guarded_static_plain(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object, std::uint32_t source,
                          std::uint32_t constructor, std::uint32_t vtable, std::uint32_t registrar) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) != 0) {
            ctx.gpr[2] = (guard + 0x8000u) & 0xFFFF0000u;
            return;
        }
        if (call_guest(rt, ctx, 0x08B6441Cu, {guard}) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (object != 0u) {
            (void)call_guest(rt, ctx, constructor, {object, source, 0u});
            put(rt, object, vtable);
            (void)call_guest(rt, ctx, registrar, {});
        }
    }
    tail_call(ctx, 0x08B64494u, {guard});
}

// func_XXXXXXXX (init handle with an index): see init_handle_indexed (initialiser).
void func_08BA2534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BA26BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BA2704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BA274C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BA2794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BA27DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BA2824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BA292C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BCE06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }
void func_08BCE334(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle_indexed(rt, ctx, 0x08B9DE60u); }

// func_XXXXXXXX (pair call D): see pair_call_d (pairs, callee).
void func_08BEA7C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8B8ACu, 0x08BEA6B8u); }
void func_08BF7F14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8C6CCu, 0x08BF7E0Cu); }
void func_08BFE438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8C9B0u, 0x08BFE330u); }
void func_08BFFA28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8CAA0u, 0x08BFF920u); }
void func_08C0333C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8CDD0u, 0x08C03234u); }
void func_08C108DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8DC98u, 0x08C107D4u); }
void func_08C1433C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8DF5Cu, 0x08C14234u); }
void func_08C14490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8DF64u, 0x08C14388u); }
void func_08C145E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_d(rt, ctx, 0x08C8DF6Cu, 0x08C144DCu); }

// func_XXXXXXXX (relocation loop): see relocation_loop (relocate, element, stride).
void func_08C403CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C40364u, 0x08AF7460u, 152u); }
void func_08C404C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C4022Cu, 0x08AF7618u, 12u); }
void func_08C40724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C4015Cu, 0x08AF76F8u, 28u); }
void func_08C41144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C410DCu, 0x08AF8840u, 20u); }
void func_08C41514(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C414ACu, 0x08AFCE4Cu, 24u); }
void func_08C416DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C41674u, 0x08AFCE84u, 40u); }
void func_08C418A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C4160Cu, 0x08AF9744u, 48u); }
void func_08C41A6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C415A4u, 0x08AF9818u, 48u); }
void func_08C41D00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { relocation_loop(rt, ctx, 0x08C41374u, 0x08AF98E0u, 72u); }

// func_XXXXXXXX (handle wrapper): see handle_wrapper (maker).
void func_08B9BD70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_wrapper(rt, ctx, 0x08B9BCB8u); }
void func_08B9CF6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_wrapper(rt, ctx, 0x08B9CEBCu); }
void func_08B9DE60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_wrapper(rt, ctx, 0x08B9DDB0u); }
void func_08BB9718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_wrapper(rt, ctx, 0x08BB964Cu); }
void func_08BB992C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_wrapper(rt, ctx, 0x08BB9860u); }
void func_08BBD0E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { handle_wrapper(rt, ctx, 0x08BBD034u); }

// func_XXXXXXXX (guarded static object, constructor without a size): see guarded_static_plain (guard, object, source, constructor, vtable, registrar).
void func_08B9FB40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_plain(rt, ctx, 0x08D754B0u, 0x08D75BACu, 0x08CA018Cu, 0x08B9FAFCu, 0x08CC0DA0u, 0x08B9D1F0u); }
void func_08BA0140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_plain(rt, ctx, 0x08D75438u, 0x08D75BE4u, 0x08CA01A8u, 0x08BA00FCu, 0x08CC00E8u, 0x08B9E5DCu); }
void func_08BA0210(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_plain(rt, ctx, 0x08D75408u, 0x08D75BF4u, 0x08CA01B0u, 0x08BA01CCu, 0x08CBFFB8u, 0x08B9E808u); }
void func_08BA1C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_plain(rt, ctx, 0x08D754A0u, 0x08D75C78u, 0x08CA01ECu, 0x08BA0098u, 0x08CC01A8u, 0x08B9E328u); }
void func_08BA2360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_plain(rt, ctx, 0x08D75538u, 0x08D75CE8u, 0x08CA0240u, 0x08BA0034u, 0x08CC0268u, 0x08B9DCE4u); }
void func_08BBA1DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_plain(rt, ctx, 0x08D76058u, 0x08D76114u, 0x08CA0BC0u, 0x08BB9980u, 0x08CC3C70u, 0x08BB99C4u); }
void func_08BC52E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_plain(rt, ctx, 0x08D761E8u, 0x08D76710u, 0x08CA0DFCu, 0x08BC520Cu, 0x08CC4CA8u, 0x08BC5250u); }
void func_08BCBD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { guarded_static_plain(rt, ctx, 0x08D76258u, 0x08D768C4u, 0x08CA1094u, 0x08BC5F08u, 0x08CC4538u, 0x08BC5F4Cu); }

} // namespace hot_batch_76
} // namespace gt::game

#define HOT76(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_76::name, gt::ReturnKind::kind)
HOT76(0x08B54694u, func_08B54694, Word);
HOT76(0x08B55AACu, func_08B55AAC, Word);
HOT76(0x08B58C9Cu, func_08B58C9C, Word);
HOT76(0x08B4AD64u, func_08B4AD64, Word);
HOT76(0x08B57D6Cu, func_08B57D6C, Word);
HOT76(0x08B67C24u, func_08B67C24, Float);
HOT76(0x08B34C1Cu, func_08B34C1C, Word);
HOT76(0x08B71EC8u, func_08B71EC8, Float);
HOT76(0x08B70F28u, func_08B70F28, DoubleWord);
HOT76(0x08AF432Cu, func_08AF432C, Word);
HOT76(0x08872484u, func_08872484, Word);
HOT76(0x08B57780u, func_08B57780, Word);
HOT76(0x08AEB930u, func_08AEB930, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
HOT76(0x08AEB908u, func_08AEB908, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
HOT76(0x08B4BA68u, func_08B4BA68, Word);
HOT76(0x08B52E00u, func_08B52E00, Word);
HOT76(0x08B49F3Cu, func_08B49F3C, Float);
HOT76(0x08B84270u, func_08B84270, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
HOT76(0x08B83C84u, func_08B83C84, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
// HOT76(0x08B590D0u, func_08B590D0, Word);  // not comparable: import stub 0x08C7AC48 (sceGe_user::0xB287BD61) (99 cases)
// HOT76(0x08B58E84u, func_08B58E84, Word);  // not comparable: import stub 0x08C7AC48 (sceGe_user::0xB287BD61) (99 cases)
// HOT76(0x08B58E28u, func_08B58E28, Word);  // not comparable: import stub 0x08C7AC08 (sceDisplay::0x289D82FE) (99 cases)
// HOT76(0x08B52D58u, func_08B52D58, Word);  // not comparable: its tail target reaches the import stub 0x08C7AC08 (sceDisplay::0x289D82FE) (99 cases)
HOT76(0x08AD96A0u, func_08AD96A0, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
HOT76(0x08A7BFD0u, func_08A7BFD0, Word);
// HOT76(0x08B0C650u, func_08B0C650, Word);  // not comparable: kernel isolation (LwMutex contention) in func_08B0C43C (69 cases)
HOT76(0x08B89124u, func_08B89124, Word);
HOT76(0x08B33A24u, func_08B33A24, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
HOT76(0x08B55AF8u, func_08B55AF8, Word);
HOT76(0x089CBFE4u, func_089CBFE4, Word);
HOT76(0x089444F8u, func_089444F8, Word);
HOT76(0x08BA2534u, func_08BA2534, Word);
HOT76(0x08BA26BCu, func_08BA26BC, Word);
HOT76(0x08BA2704u, func_08BA2704, Word);
HOT76(0x08BA274Cu, func_08BA274C, Word);
HOT76(0x08BA2794u, func_08BA2794, Word);
HOT76(0x08BA27DCu, func_08BA27DC, Word);
HOT76(0x08BA2824u, func_08BA2824, Word);
HOT76(0x08BA292Cu, func_08BA292C, Word);
HOT76(0x08BCE06Cu, func_08BCE06C, Word);
HOT76(0x08BCE334u, func_08BCE334, Word);
HOT76(0x08BEA7C0u, func_08BEA7C0, Word);
HOT76(0x08BF7F14u, func_08BF7F14, Word);
HOT76(0x08BFE438u, func_08BFE438, Word);
HOT76(0x08BFFA28u, func_08BFFA28, Word);
HOT76(0x08C0333Cu, func_08C0333C, Word);
HOT76(0x08C108DCu, func_08C108DC, Word);
HOT76(0x08C1433Cu, func_08C1433C, Word);
HOT76(0x08C14490u, func_08C14490, Word);
HOT76(0x08C145E4u, func_08C145E4, Word);
HOT76(0x08C403CCu, func_08C403CC, Word);
HOT76(0x08C404C4u, func_08C404C4, Word);
HOT76(0x08C40724u, func_08C40724, Word);
HOT76(0x08C41144u, func_08C41144, Word);
HOT76(0x08C41514u, func_08C41514, Word);
HOT76(0x08C416DCu, func_08C416DC, Word);
HOT76(0x08C418A4u, func_08C418A4, Word);
HOT76(0x08C41A6Cu, func_08C41A6C, Word);
HOT76(0x08C41D00u, func_08C41D00, Word);
HOT76(0x08B9BD70u, func_08B9BD70, Word);
HOT76(0x08B9CF6Cu, func_08B9CF6C, Word);
HOT76(0x08B9DE60u, func_08B9DE60, Word);
HOT76(0x08BB9718u, func_08BB9718, Word);
HOT76(0x08BB992Cu, func_08BB992C, Word);
HOT76(0x08BBD0E4u, func_08BBD0E4, Word);
HOT76(0x08B9FB40u, func_08B9FB40, Word);
HOT76(0x08BA0140u, func_08BA0140, Word);
HOT76(0x08BA0210u, func_08BA0210, Word);
HOT76(0x08BA1C34u, func_08BA1C34, Word);
HOT76(0x08BA2360u, func_08BA2360, Word);
HOT76(0x08BBA1DCu, func_08BBA1DC, Word);
HOT76(0x08BC52E0u, func_08BC52E0, Word);
HOT76(0x08BCBD68u, func_08BCBD68, Word);
