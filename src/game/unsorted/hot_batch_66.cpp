// Hot batch 66: more small functions that the five pad routes reach (call probe over every function of up to 70 instructions
// not yet decompiled, run on the five routes with --decomp off, as in batches 57-65). As in batches 60-65, each function's comment
// lists the branches the compared cases of the fast gate do not take (found with `tools.decomp_gate coverage`); those were
// compared against the asm by hand, reading the branch TARGET (a value set in a delay slot is the result only if the target does not
// overwrite it). Their classes aren't known yet, so they keep their addresses as names. Written from the asm; the generated code was
// only a reading aid (it gives the relocated constants).
//
// Families (same instruction shape, immediates, static addresses and call targets differ per member, each decoded from its own words):
//  - "pair call A" (18 instructions): copies the 8 bytes at a static address and the 8 bytes a2 points to onto the stack and calls
//    a worker with pointers to both copies (and the loaded words in t0-t2);
//  - "pair call B" (23 instructions): the same with a third pair (a3's) and one more pointer argument;
//  - "init handle" (16 instructions): clears an object's first 9 bytes and calls an initialiser with a1 = a2 + *(a1 + 12);
//  - "call with a constant" (11 instructions): calls a shared worker with a2 = a constant and returns this;
//  - the "debug-draw gate" thunks (08B83DE8 ...): call a worker with a fixed block only when a flag is set;
//  - the "checked holder" tail calls (08AA9100 ...), record copy loops, a tree descent.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_66 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kAssertNull = 0x08A5BB7Cu; // (message): reports a null holder (called with the holder's message, then the code goes on)
constexpr std::uint32_t kHolderMessage = 0x08CB30B0u;

std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }

// Pair call A: copies the pair at `pairs` (static) to frame+0 and the pair a2 points to to frame+8, then calls `callee`
// (a0, a1, &frame[0], &frame[8], t0 = a2[0], t1 = a2[1], t2 = pairs[1], t3 as it was); v0 = its result.
void pair_call_a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pairs, std::uint32_t callee) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t source = ctx.gpr[6];
    const std::uint32_t arg_hi = word(rt, source + 4u), arg_lo = word(rt, source);
    const std::uint32_t static_hi = word(rt, pairs + 4u), static_lo = word(rt, pairs);
    put(rt, frame.at(12u), arg_hi);
    put(rt, frame.at(8u), arg_lo);
    put(rt, frame.at(0u), static_lo);
    put(rt, frame.at(4u), static_hi);
    ctx.gpr[2] = call_guest(rt, ctx, callee, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), frame.at(8u), arg_lo, arg_hi, static_hi, ctx.gpr[11]});
}

// Pair call B: frame+0 = the static pair, frame+8 = a3's pair, frame+16 = a2's pair; calls `callee`
// (a0, a1, &frame[0], &frame[16], t0 = &frame[8], t1 = a2[0], t2 = a2[1], t3 = pairs[0]); v0 = its result.
void pair_call_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pairs, std::uint32_t callee) {
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t second = ctx.gpr[6], third = ctx.gpr[7];
    const std::uint32_t third_lo = word(rt, third);
    const std::uint32_t second_hi = word(rt, second + 4u);
    const std::uint32_t second_lo = word(rt, second);
    const std::uint32_t static_hi = word(rt, pairs + 4u);
    const std::uint32_t static_lo = word(rt, pairs);
    const std::uint32_t third_hi = word(rt, third + 4u);
    put(rt, frame.at(0x14u), second_hi);
    put(rt, frame.at(0x10u), second_lo);
    put(rt, frame.at(0x0Cu), third_hi);
    put(rt, frame.at(8u), third_lo);
    put(rt, frame.at(0u), static_lo);
    put(rt, frame.at(4u), static_hi);
    ctx.gpr[2] = call_guest(rt, ctx, callee, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), frame.at(0x10u), frame.at(8u), second_lo, second_hi, static_lo});
}

// Init handle: this[0] = this[1] = 0, byte this[8] = 0 (that one before the call), then initialiser(this, a2 + a1[3], 0);
// a3 and t0-t3 reach the initialiser as they came. v0 = this.
void init_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t initialiser) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t offset = word(rt, ctx.gpr[5] + 12u);
    put(rt, self, 0u);
    const std::uint32_t where = ctx.gpr[6] + offset;
    put(rt, self + 4u, 0u);
    rt.memory().store8(self + 8u, 0u);
    (void)call_guest(rt, ctx, initialiser, {self, where, 0u});
    ctx.gpr[2] = self;
}

// worker(this, a1, constant); v0 = this.
void call_with_constant(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t constant) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = ctx.gpr[4];
    (void)call_guest(rt, ctx, 0x088BA424u, {self, ctx.gpr[5], constant});
    ctx.gpr[2] = self;
}

// The holder this[1] must be set: a null one calls func_08A5BB7C(message) and the code goes on regardless.
std::uint32_t checked_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    if (holder == 0u) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, kAssertNull, {kHolderMessage});
    }
    return holder;
}

// Calls `callee(kBlock, a0 [, a1, a2, a3])` only when the flag word 0x08D740F8 is set; v0 = 0x80420100 when it is not.
constexpr std::uint32_t kFlag = 0x08D740F8u;
constexpr std::uint32_t kBlock = 0x08D74100u;
void flagged_call_1(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t callee) {
    const std::uint32_t flag = word(rt, kFlag);
    ctx.gpr[2] = 0x80420100u;
    if (flag == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, callee, {kBlock, ctx.gpr[4], flag});
}

} // namespace

// func_XXXXXXXX (pair call A): the pair at STATIC and the pair a2 points to are copied to the stack and CALLEE gets pointers to both.
void func_08C379F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90164u, 0x08C37934u); }
void func_08C377C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C90010u, 0x08C37704u); }
void func_08C2D750(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F828u, 0x08C2D68Cu); }
void func_08C2A294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F38Cu, 0x08C2A1D0u); }
void func_08C29AD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F1F0u, 0x08C29A14u); }
void func_08C24E6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8EEBCu, 0x08C24DA8u); }
void func_08C1F4C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8EBE0u, 0x08C1F404u); }
void func_08C1F298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8EA8Cu, 0x08C1F1D4u); }
void func_08C150A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8E170u, 0x08C14F9Cu); }
void func_08C27114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_a(rt, ctx, 0x08C8F074u, 0x08C2700Cu); }

// func_XXXXXXXX (pair call B): the same with a3's pair as well.
void func_08BEA1E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8B88Cu, 0x08BEA0C8u); }
void func_08BEA978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8B8B4u, 0x08BEA858u); }
void func_08C0305C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8CDC0u, 0x08C02F3Cu); }
void func_08C103C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8DC90u, 0x08C102A8u); }
void func_08C09A84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D6A0u, 0x08C09964u); }
void func_08C031D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8CDC8u, 0x08C030B8u); }
void func_08BF1E38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C240u, 0x08BF1D18u); }
void func_08BF5E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C608u, 0x08BF5CF0u); }
void func_08BF3968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8C3B8u, 0x08BF3848u); }
void func_08C08434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pair_call_b(rt, ctx, 0x08C8D530u, 0x08C08314u); }

// func_XXXXXXXX (init handle): clears the first nine bytes of a0 and calls the initialiser CALLEE.
void func_08B9D608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9D5B4u); }
void func_08BA257C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA25BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA25FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA263C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08BA267C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }
void func_08B9DCA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DC50u); }
void func_08B9DEB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_handle(rt, ctx, 0x08B9DE60u); }

// func_XXXXXXXX (call with a constant): func_088BA424(this, a1, CONSTANT); v0 = this.
void func_08BB1610(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 1u); }
void func_08BB0E20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 1u); }
void func_08BB1814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 180u); }
void func_08BB1840(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 200u); }

// --- hand-written ---

// func_08B83DE8 / func_08B83E24(a0): the flag word at 0x08D740F8 set: worker(0x08D74100, a0, flag) (0x08C7B078 / 0x08C7B098); v0 = its result, else 0x80420100.
void func_08B83DE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_call_1(rt, ctx, 0x08C7B078u); }
void func_08B83E24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flagged_call_1(rt, ctx, 0x08C7B098u); }

// func_08B83FF4(a0, a1, a2, a3): with the flag word set: worker 0x08C7B088(0x08D74100, a0, a1, a2, t0 = a3, t1 = 0x08D70000, t2 = 0x80420000, t3 = 0x80420100); v0 = its result, else 0x80420100.
void func_08B83FF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t flag = word(rt, kFlag);
    ctx.gpr[2] = 0x80420100u;
    if (flag == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08C7B088u, {kBlock, ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], 0x08D70000u, 0x80420000u, 0x80420100u});
}

// func_08AA9510(this, b): the holder this[1] (a null one calls func_08A5BB7C(0x08CB30B0) first), then tail call func_08C31304(holder, (signed byte) b).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null holder (bnez skips the jal; the lui/addiu of the message sit in the delay slot, so r4 is the message on both paths, and the code goes on after the call with a0 = the null holder).
void func_08AA9510(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = checked_holder(rt, ctx, word(rt, ctx.gpr[4] + 4u));
    tail_call(ctx, 0x08C31304u, {holder, sext8(ctx.gpr[5])});
}
// func_08AA93F0(this, h): the same with a signed halfword, tail call func_08C3131C.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null holder, as above.
void func_08AA93F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = checked_holder(rt, ctx, word(rt, ctx.gpr[4] + 4u));
    tail_call(ctx, 0x08C3131Cu, {holder, sext16(ctx.gpr[5])});
}
// func_08AA9100(this, a1, a2, a3): the same, tail call func_08C31384(holder, a1, a2, a3).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null holder, as above (a2/a3 are set from the saved s-registers after the call).
void func_08AA9100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = checked_holder(rt, ctx, word(rt, ctx.gpr[4] + 4u));
    tail_call(ctx, 0x08C31384u, {holder, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
}

// func_08BAFF30(this): p = *this; unless p - 12 is the shared empty buffer 0x08D75298 (then v0 = that address): when the count p[-1] is
// positive, func_08B943A0(this, 0, 0, 0, t0 = p) runs and p is read again; then p[-1] = -1 (v0 = -1).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): p - 12 == 0x08D75298 (beq to the epilogue: v0 is the address left in v0 by the addiu before the branch, the delay slot only saves ra) and a positive count (the call; blez skips it, its delay slot a3 = 0 runs on both paths, and p is read again after the call).
void func_08BAFF30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kEmpty = 0x08D75298u;
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t p = word(rt, self);
    if (p - 12u == kEmpty) {
        ctx.gpr[2] = kEmpty;
        return;
    }
    if (static_cast<std::int32_t>(word(rt, p - 4u)) > 0) {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B943A0u, {self, 0u, 0u, 0u, p});
        p = word(rt, self);
    }
    put(rt, p - 4u, 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// func_08BE6D44(dest, count, source): count times: unless dest is null, copies the 40-byte record at source to dest (source stays; the loads
// of +0x20/+0x24 come after the first seven stores, and dest may overlap source) and stamps dest[0] = 0x08CCE6E0 (the record's class
// pointer); dest += 40. v0 = 0x08CD0000, or the word source[1] of the last copied record; v1 = source[2] likewise.
void func_08BE6D44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kRecordClass = 0x08CCE6E0u;
    std::uint32_t dest = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    ctx.gpr[2] = 0x08CD0000u;
    if (count == 0u) return;
    std::uint32_t done = 0u;
    do {
        ++done;
        if (dest != 0u) {
            const std::uint32_t w3 = word(rt, source + 0x0Cu), w4 = word(rt, source + 0x10u);
            const std::uint32_t w1 = word(rt, source + 4u), w2 = word(rt, source + 8u);
            const std::uint32_t w5 = word(rt, source + 0x14u), w6 = word(rt, source + 0x18u), w7 = word(rt, source + 0x1Cu);
            put(rt, dest + 4u, w1);
            put(rt, dest + 8u, w2);
            put(rt, dest + 0x0Cu, w3);
            put(rt, dest + 0x10u, w4);
            put(rt, dest + 0x14u, w5);
            put(rt, dest + 0x18u, w6);
            put(rt, dest + 0x1Cu, w7);
            const std::uint32_t w8 = word(rt, source + 0x20u), w9 = word(rt, source + 0x24u);
            put(rt, dest, kRecordClass);
            put(rt, dest + 0x20u, w8);
            put(rt, dest + 0x24u, w9);
            ctx.gpr[2] = w1;
            ctx.gpr[3] = w2;
        }
        dest += 0x28u;
    } while (count != done);
}

// func_08C4B3C0(first, last, dest) -> dest after: copies each 0x70-byte record of [first, last) to dest (in 16-byte chunks: four loads,
// then four stores), a null dest only advancing.
void func_08C4B3C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t source = ctx.gpr[4], dest = ctx.gpr[6];
    const std::uint32_t last = ctx.gpr[5];
    if (source != last) {
        do {
            if (dest != 0u) {
                for (std::uint32_t chunk = 0u; chunk < 0x70u; chunk += 16u) {
                    const std::uint32_t w0 = word(rt, source + chunk), w1 = word(rt, source + chunk + 4u);
                    const std::uint32_t w2 = word(rt, source + chunk + 8u), w3 = word(rt, source + chunk + 12u);
                    put(rt, dest + chunk, w0);
                    put(rt, dest + chunk + 4u, w1);
                    put(rt, dest + chunk + 8u, w2);
                    put(rt, dest + chunk + 12u, w3);
                    ctx.gpr[3] = w1;
                }
            }
            source += 0x70u;
            dest += 0x70u;
        } while (source != last);
    }
    ctx.gpr[2] = dest;
}

// func_08C1DA98(first, count, source): count times (a null element only advances): func_08B88F44(element, source), then
// func_08A52A98(element + 4, source + 4); element += 8. v0 = the last result of the second call (v0 as it came when nothing was called).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): count == 0 (beqz to the epilogue, v0 untouched) and a null element (beqz lands on the loop's bne, the delay slot `move a1, source` is dead there; r17 was already incremented and the element still advances 8 in the bne's delay slot).
void func_08C1DA98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t element = ctx.gpr[4];
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    if (count == 0u) return;
    std::uint32_t done = 0u;
    do {
        ++done;
        if (element != 0u) {
            GuestFrame frame(ctx, 0x20u);
            (void)call_guest(rt, ctx, 0x08B88F44u, {element, source});
            ctx.gpr[2] = call_guest(rt, ctx, 0x08A52A98u, {element + 4u, source + 4u});
        }
        element += 8u;
    } while (count != done);
}

// func_08BB4368(tree, key): finds where the key (a pointer to an object whose float at +8 orders the nodes) belongs in the tree {.., left +8,
// right +12, object +16, root tree[2]}: descends left when key < node (else right) to the last node visited (tree + 4 for an empty tree), then
// func_08BB404C(tree, 0, that node, key).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a non-empty tree: the descent was compared with the asm by hand (bc1fl: on !(key < node) the likely delay slot loads node[3], on key < node the fall-through loads node[2]; beqzl: a zero child ends with the node as parent (delay slot move r6, r3 runs only then), otherwise j with `move r3, r2`; NaN keys compare false and go right in both).
void func_08BB4368(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tree = ctx.gpr[4], key = ctx.gpr[5];
    std::uint32_t parent = tree + 4u;
    std::uint32_t node = word(rt, tree + 8u);
    if (node != 0u) {
        const float key_value = float_at(rt, word(rt, key) + 8u);
        for (;;) {
            const float node_value = float_at(rt, word(rt, node + 16u) + 8u);
            const std::uint32_t next = key_value < node_value ? word(rt, node + 8u) : word(rt, node + 12u);
            if (next == 0u) {
                parent = node;
                break;
            }
            node = next;
        }
    }
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08BB404Cu, {tree, 0u, parent, key});
}

// func_08C54A7C(this, a1): func_08B658FC(this, a1), func_08C548C4(this, this + 0x6C); bytes this[0x74], this[0x75] = 0, this[0x70] = this[4] = 0;
// this[0x78] = a1; this[5] = the word at 0x08C95F64 (a1 set) or 0x08C95F70 (a1 null). v0 = that word.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): a null a1 (beqz to the other constant: 0x08C95F70; the delay slot `sw a1, 0x78(this)` runs on both paths).
void func_08C54A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08B658FCu, {self, argument});
    (void)call_guest(rt, ctx, 0x08C548C4u, {self, self + 0x6Cu});
    rt.memory().store8(self + 0x74u, 0u);
    rt.memory().store8(self + 0x75u, 0u);
    put(rt, self + 0x70u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x78u, argument);
    const std::uint32_t value = word(rt, argument != 0u ? 0x08C95F64u : 0x08C95F70u);
    put(rt, self + 0x14u, value);
    ctx.gpr[2] = value;
}


} // namespace hot_batch_66
} // namespace gt::game

#define HOT66(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_66::name, gt::ReturnKind::kind)
HOT66(0x08C379F8u, func_08C379F8, Word);
HOT66(0x08C377C8u, func_08C377C8, Word);
HOT66(0x08C2D750u, func_08C2D750, Word);
HOT66(0x08C2A294u, func_08C2A294, Word);
HOT66(0x08C29AD8u, func_08C29AD8, Word);
HOT66(0x08C24E6Cu, func_08C24E6C, Word);
HOT66(0x08C1F4C8u, func_08C1F4C8, Word);
HOT66(0x08C1F298u, func_08C1F298, Word);
HOT66(0x08C150A4u, func_08C150A4, Word);
HOT66(0x08C27114u, func_08C27114, Word);
HOT66(0x08BEA1E8u, func_08BEA1E8, Word);
HOT66(0x08BEA978u, func_08BEA978, Word);
HOT66(0x08C0305Cu, func_08C0305C, Word);
HOT66(0x08C103C8u, func_08C103C8, Word);
HOT66(0x08C09A84u, func_08C09A84, Word);
HOT66(0x08C031D8u, func_08C031D8, Word);
HOT66(0x08BF1E38u, func_08BF1E38, Word);
HOT66(0x08BF5E10u, func_08BF5E10, Word);
HOT66(0x08BF3968u, func_08BF3968, Word);
HOT66(0x08C08434u, func_08C08434, Word);
HOT66(0x08B9D608u, func_08B9D608, Word);
HOT66(0x08BA257Cu, func_08BA257C, Word);
HOT66(0x08BA25BCu, func_08BA25BC, Word);
HOT66(0x08BA25FCu, func_08BA25FC, Word);
HOT66(0x08BA263Cu, func_08BA263C, Word);
HOT66(0x08BA267Cu, func_08BA267C, Word);
HOT66(0x08B9DCA4u, func_08B9DCA4, Word);
HOT66(0x08B9DEB4u, func_08B9DEB4, Word);
HOT66(0x08BB1610u, func_08BB1610, Word);
HOT66(0x08BB0E20u, func_08BB0E20, Word);
HOT66(0x08BB1814u, func_08BB1814, Word);
HOT66(0x08BB1840u, func_08BB1840, Word);
// Written but not registered: the worker is an import stub (sceSasCore) the harness cannot run inside a case (14 not comparable per route, compared 0).
// HOT66(0x08B83DE8u, func_08B83DE8, Word);
// HOT66(0x08B83E24u, func_08B83E24, Word);
// HOT66(0x08B83FF4u, func_08B83FF4, Word);
HOT66(0x08AA9510u, func_08AA9510, Word);
HOT66(0x08AA93F0u, func_08AA93F0, Word);
HOT66(0x08AA9100u, func_08AA9100, Word);
HOT66(0x08BAFF30u, func_08BAFF30, Word);
// Written but not registered: they write memory and no compared case changed any (compared 63 / 18, 0 cases that changed memory).
// HOT66(0x08BE6D44u, func_08BE6D44, Word);
// HOT66(0x08C4B3C0u, func_08C4B3C0, Word);
HOT66(0x08C1DA98u, func_08C1DA98, Word);
HOT66(0x08BB4368u, func_08BB4368, Word);
HOT66(0x08C54A7Cu, func_08C54A7C, Word);
