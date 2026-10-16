// Hot batch 95: reached functions of 121-170 instructions left by batch 94 (the probe `tools.decomp_gate probe b93p`; 111 of its 192 reached functions had no `func_X` in src/) and, because that pool is nearly dry, a new probe over 171-230 instructions (`tools.decomp_gate probe b95p`: 348 not-done functions of that size, 104 reached). (main includes batch 94, 6,402 functions.) Before any function was read its address was searched for in src/ (main and the open PR branches: none is open).
//
// How the functions were found: `tools.shapes --skeleton` over the 111 left of the 121-170 pool finds no family (every cluster is one function). Over the 104 reached of the 171-230 pool it finds one: a 182-instruction hinted insertion into a string-keyed red-black tree (hinted_insert: func_08BF70B4 and four members that differ only in the two callees of their own tree: the search for the unique position and the node insertion; `tools.family_members --check` reads the five rows and finds them equal to their words), whose (word, string)-keyed sibling func_08C34C58 (228 instructions, 45,104 calls) was read from the asm separately.
// The rest were read one by one with `tools.disasm`, `tools.sp_loads` and `tools.arg_reads` (new, see below): three vector writers of 4-, 16- and 40-byte elements (func_08BEFA18, func_08C13A68, func_08BF56E4: n elements through the stream's virtual slot 8 after fitting the vector), two number-to-text natives that share number_text_tail (func_08A26744, func_08A26940), two array builders (func_0899BEC0, func_0899CB44), a tree-like registry lookup with virtual comparisons (func_08C29370), string natives that build text through holders and cells (func_08A6E974, func_08858090,
// func_08819170, func_08A43724, func_089C9F24, func_089CA474, func_08A31244, func_08858298), counted-pointer code (func_089170E0, func_08888988: shared_ptr copies with the ll/sc counts of release_counted), the reference-map copy func_08A7A3D4, a classifier (func_08B29070), a destructor (func_089F4140) and similar. The role words in the comments (holder, string, vector, record, cell, queue, slot, counted block) describe the shape of the accesses; none is a name from the game.
//
// Registered (33): see the HOT95 lines at the end (27 of the 121-170 pool and the 6 of the 171-230 pool). Written but not registered (4, each commented HOT95 line says why): func_089C9F24 (every case stops on the import stub IoFileMgrForUser::sceIoRead), func_089CA474 (its reached call stops on `kernel isolation (LwMutex contention)`), func_08A31244 (its reached call stops on the import stub sceWlanDrv::0x0C622081): none of the three was compared on any of the five routes; func_08A7A3D4 (compared 93 cases in the fast gate and 3000 in a dense race run of 15,907 calls, and not one changed memory: every call returns early, so the copy never ran).
// Left (read, not written): jump-table functions (func_0895E3F4, func_08ADC4E0), recursive sorts and tree rebalancing (func_08B8BF44, func_08C496A4, func_08B5F444, the recursive pool release func_08BB2CFC), a unit made of several tail-called blocks (func_08B3177C), map functions with shared pointers (func_08A535A4, func_08AACAF8), a wait/signal ring buffer (func_08AF0B78, import stubs), the 64-bit tail-calling func_088A66A4, the nested initialiser func_0891179C, the collision pass func_0895F7C0 (sqrt, nested loops),
// the string-escape routine func_08A6A40C and the natives with several modes (func_08A77940, func_08C23158).
//
// Gates (fast gate, five routes uncapped): every registered function compared at least once with 0 mismatches and at least one compared case that changed memory (func_08B29070, a pure classifier, writes nothing; its 49 cases are all equal); `tools.decomp_gate sabotage` pins 33 of 33 in the first round; `tools.contract_audit` finds nothing new (the two registrations of batch 90 whose callers read v1); `tools.family_members --check` reads 5 rows, all equal to their words; `tools.literal_check` finds 0 literals missing in 37 functions with 3 `--allow` entries
// (0x08D70000 of func_08A6E974 is the high half the asm leaves in v0 after `lui v0, 0xA`, the recompiled code loads `2263u << 16u`; 0x20 / 0x24 of func_08BF56E4 are the offsets of the default element's last two words from its base, sp + 0x5C and sp + 0x60 over sp + 0x3C, sp + 0x34 and sp + 0x38 over sp + 0x14); `tools.sp_loads` finds only loads the C++ models (a holder or object re-read after a callee stored it at sp + off).
// `tools.arg_reads` (new, 5 tests) lists six calls that pass fewer arguments than their callee reads, all harmless (see docs/agents/decomp-workflow.md).
// Branches the compared cases do not take are listed in each function's comment (from `tools.decomp_gate coverage`, then read against the asm by hand). The weakest proofs: func_08B29070 (only the lo < 100 classes ran: every other class and the whole 64-bit tree are read from the asm), func_08858090 (only the delegating call to func_08855990 ran, with count below 2), func_08819170 (count was always 3), func_089170E0 (count was always 1, no counted block was released), func_08C34C58 and the hinted_insert rows
// (the word-equality and string-compare arms and most of the neighbour arms), func_089936C4 (every optional object was present), func_08888988 (the early exits, the null pair and the "not ready" arms) and func_08A82864 (the arm that finds an existing node and does not insert).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value, so a callee that reads a register this function never passes sees what an earlier call_set left. The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs; $f20 / $f21 are saved and restored by SavedFloat where they are used. The delay slot of a `jal` / `jalr` runs BEFORE the call,
// a `bnel` / `beql` / `bgtzl` / `blezl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths; every "the delay slot of ..." comment below names the instruction right after the branch it says. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the constant a string release that finds the shared empty cell leaves in v0 (FINDINGS batch 95); the dead stack (spills the original makes and nothing reads, the intermediate copies through sp, the pair {node, inserted} spilled by the unique-insert calls) is not reproduced, the harness ignores it; the frames that callees read are.
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_95 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}

inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A virtual call: the slot at `offset` of the object's vtable (the word at object[0]).
inline std::uint32_t slot_of(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t offset) { return word(rt, word(rt, object) + offset); }

// The release of a string cell whose data pointer is `data`: nothing happens when data - 0xC is D_08D74FD0 (the shared empty cell), otherwise func_08B60804(data - 4, -1) and, when its result is not positive, func_08B8542C(data - 0xC, flag). Leaves a0 / a1 like the original.
inline void release_cell(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data, std::uint32_t flag) {
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;  // the delay slot of the beq
    if (data - 0xCu == 0x08D74FD0u) return;
    const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = data - 0xCu;
    ctx.gpr[5] = flag;  // the delay slot of the bgtz
    if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, flag});
}

// A floating-point register as it was at entry (the original saves the callee-saved $f20 / $f21 in its frame and restores them before it returns).
class SavedFloat {
public:
    SavedFloat(psprecomp::AllegrexContext &ctx, unsigned index) : ctx_(ctx), index_(index), saved_(ctx.fpr[index]) {}
    ~SavedFloat() { ctx_.fpr[index_] = saved_; }
    SavedFloat(const SavedFloat &) = delete;
    SavedFloat &operator=(const SavedFloat &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    unsigned index_;
    float saved_;
};

// The release of a counted block (the asm's ll / sc pairs, one thread: a plain load and store; v0 = 1 and a0 = the old count after each): the strong count at c[4] is decremented and, when it was 1, the virtual slot 8 of c runs; then the weak count at c[8] is decremented and, when that was 1, the slot 0xC.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): the early returns after each decrement (`if (old != 1u) return;` was never false, so the virtual slots 8 and 0xC never ran from here).
inline void release_counted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t counted) {
    std::uint32_t old = word(rt, counted + 4u);
    put(rt, counted + 4u, old - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = old;
    if (old != 1u) return;
    (void)call_set(rt, ctx, slot_of(rt, counted, 8u), {counted});
    old = word(rt, counted + 8u);
    put(rt, counted + 8u, old - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = old;
    if (old != 1u) return;
    (void)call_set(rt, ctx, slot_of(rt, counted, 0xCu), {counted});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08819170(a0, a1, count, list): nothing happens (v0 = 1, the slti's answer) when count < 3 (signed). Otherwise the holder sp[0x1C] = func_08817FCC(sp + 0x1C, a1) (null: func_08A5BB7C(D_08C98D6C (0x08C98D6C)) first) gives fp = func_08817A54(it) and is destroyed (func_08A5B9EC); the objects list[0..4] (null: func_08A5BB7C(D_08C98D34 (0x08C98D34)) first) are asked:
// slot 0xC of list[0] fills the string sp + 0x18, v = func_08B20A54(D_08CE2AA4 (0x08CE2AA4), sp[0x18], 0) is kept at sp[0x30] and the string is released (flag sp + 0x12); the 64-bit answer of slot 0x50 of list[1] is (s4, s5) = (v0, v1), slot 0x4C of list[2] is s7 and, unless count is 3, slot 0x4C of list[3] (when non-null) is s6 (else 0). With count >= 5 the slot 0xC of list[4]
// fills sp + 0x14, func_08B855E0(sp + 0x20, sp + 0x14) appends it to the cell sp[0x20] (initially the shared empty cell + 0xC) and sp + 0x14 is released (flag sp + 0x11). func_088183D4(fp, fp[0], sp[0x30], _, s4, s5, s7 & 0xFF, s6) runs with sp[0] = sp + 0x20 as the ninth argument; the cell sp + 0x20 is released (flag sp + 0x10). v0 = the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(count) < 3) return;`; `if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98D6Cu});`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});`; `if (count != 3u) {`; `if (object != 0u) extra = call_set(rt, ctx, slot_of(rt, object, 0x4Cu), {object});`; `if (signed_of(count) >= 5) {`
void func_08819170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t second = ctx.gpr[5], count = ctx.gpr[6], list = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    ctx.gpr[2] = signed_of(count) < 3 ? 1u : 0u;  // the slti at 0x08819174 writes v0
    if (signed_of(count) < 3) return;
    const std::uint32_t holder = frame.at(0x1Cu);
    (void)call_set(rt, ctx, 0x08817FCCu, {holder, second});
    const std::uint32_t source = word(rt, frame.at(0x1Cu));
    if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98D6Cu});
    const std::uint32_t target = call_set(rt, ctx, 0x08817A54u, {source});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});  // fp = target is the delay slot's
    const auto checked = [&](std::uint32_t index) {
        const std::uint32_t object = word(rt, list + 4u * index);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});
        return object;
    };
    std::uint32_t object = checked(0u);
    (void)call_set(rt, ctx, slot_of(rt, object, 0xCu), {frame.at(0x18u), object});
    const std::uint32_t named = call_set(rt, ctx, 0x08B20A54u, {0x08CE2AA4u, word(rt, frame.at(0x18u)), 0u});
    put(rt, frame.at(0x30u), named);
    release_cell(rt, ctx, word(rt, frame.at(0x18u)), frame.at(0x12u));
    object = checked(1u);
    const GuestResult pair = call_keep(rt, ctx, slot_of(rt, object, 0x50u), {object}, {});
    const std::uint32_t low = pair.v0, high = pair.v1;
    object = checked(2u);
    const std::uint32_t flags = call_set(rt, ctx, slot_of(rt, object, 0x4Cu), {object});
    std::uint32_t extra = 0u;  // s6
    if (count != 3u) {
        object = word(rt, list + 0xCu);
        if (object != 0u) extra = call_set(rt, ctx, slot_of(rt, object, 0x4Cu), {object});
    }
    const std::uint32_t cell = frame.at(0x20u);
    put(rt, cell, 0x08D74FD0u + 0xCu);
    if (signed_of(count) >= 5) {
        object = word(rt, list + 0x10u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});
        const std::uint32_t text = frame.at(0x14u);
        (void)call_set(rt, ctx, slot_of(rt, object, 0xCu), {text, object});
        (void)call_set(rt, ctx, 0x08B855E0u, {cell, text});
        release_cell(rt, ctx, word(rt, text), frame.at(0x11u));
    }
    put(rt, frame.at(0u), cell);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088183D4u, {target, word(rt, target), word(rt, frame.at(0x30u)), ctx.gpr[7] /* a3: the callee does not read it */, low, high, flags & 0xFFu, extra});
    release_cell(rt, ctx, word(rt, cell), frame.at(0x10u));
}

// func_08858090(out, key, count, list): s5 = 0, or with count >= 2 the virtual slot 0x30 of list[1] (null: func_08A5BB7C(D_08C9E14C (0x08C9E14C)) first) answers it. The holder sp[0xC] = func_088547C4(sp + 0xC, key) (null: func_08A5BB7C(D_08C9E188 (0x08C9E188)) first) is given to func_08857FC4(sp[0xC], s5) and destroyed. With count <= 0 that is all (v0 = the destroy call's).
// Otherwise o = list[0]: null gives *out = func_08A97A6C(sp + 8, 0) (assigned with func_08A5BAD8 unless it is out, destroyed); else when func_08A76F20(o, func_0884C458()) answers zero func_08855990(out, key, count, list) ends, and otherwise t = func_0884C868(sp + 0x10, list), s1 = func_08854678(func_088547C4(sp + 4, key)'s object (null: D_08C9E188 first)) with sp + 4 destroyed,
// func_088A58A4(s1, _, sp[0x10][0x18], sp[0x10][0x1C]) (null: func_08A5BB7C(D_08C9E548 (0x08C9E548)) first) and func_088A599C(s1, halfword sp[0x10][0x20]), then *out = func_08A97A6C(sp, 1) (assigned unless it is out) and sp and sp + 0x10 are destroyed. v0 = the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (!(signed_of(count) < 2)) {`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E14Cu});`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E188u});`; `if (first == 0u) {`; `if (out != made) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, made)});`; `if (call_set(rt, ctx, 0x08A76F20u, {first, scene}) == 0u) {`; `const std::uint32_t record = frame.at(0x10u), temporary = frame.at(4u);`; `if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E548u});`
void func_08858090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], key = ctx.gpr[5], count = ctx.gpr[6], list = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    std::uint32_t named = 0u;  // s5
    if (!(signed_of(count) < 2)) {
        const std::uint32_t object = word(rt, list + 4u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E14Cu});
        named = call_set(rt, ctx, slot_of(rt, object, 0x30u), {object});
    }
    const std::uint32_t holder = frame.at(0xCu);
    (void)call_set(rt, ctx, 0x088547C4u, {holder, key});
    std::uint32_t object = word(rt, frame.at(0xCu));
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E188u});
    (void)call_set(rt, ctx, 0x08857FC4u, {object, named});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
    if (signed_of(count) <= 0) return;
    const std::uint32_t first = word(rt, list);
    if (first == 0u) {
        const std::uint32_t made = frame.at(8u);
        (void)call_set(rt, ctx, 0x08A97A6Cu, {made, 0u});
        if (out != made) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, made)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {made});
        return;
    }
    const std::uint32_t scene = call_set(rt, ctx, 0x0884C458u, {});
    if (call_set(rt, ctx, 0x08A76F20u, {first, scene}) == 0u) {
        (void)call_set(rt, ctx, 0x08855990u, {out, key, count, list});
        return;
    }
    const std::uint32_t record = frame.at(0x10u), temporary = frame.at(4u);
    (void)call_set(rt, ctx, 0x0884C868u, {record, list});
    (void)call_set(rt, ctx, 0x088547C4u, {temporary, key});
    object = word(rt, temporary);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E188u});
    const std::uint32_t target = call_set(rt, ctx, 0x08854678u, {object});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});  // s1 = target is the delay slot's
    std::uint32_t source = word(rt, record);
    if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E548u});
    (void)call_set(rt, ctx, 0x088A58A4u, {target, 0u, word(rt, source + 0x18u), word(rt, source + 0x1Cu)});
    source = word(rt, record);
    if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E548u});
    (void)call_set(rt, ctx, 0x088A599Cu, {target, sext16(half_u(rt, source + 0x20u))});
    const std::uint32_t made = frame.at(0u);
    (void)call_set(rt, ctx, 0x08A97A6Cu, {made, 1u});
    if (out != made) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, made)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {made});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {record});
}

// func_08858298(out, key): s5 = func_088546F0(key), the holder sp[0x1C] = func_08A813F4(sp + 0x1C) (its object o: null: func_08A5BB7C(D_08C9E554 (0x08C9E554))) and two callbacks are run on o by their virtual slot 0xA0 (read before the call that builds their argument): first func_08A69B3C(sp + 8, D_08C9E55C (0x08C9E55C)) names a new 0x18-byte block (func_08A5BC20(0x18, 4), func_08A76ACC(block), block[0] = D_08CB6B88 (0x08CB6B88),
// func_08A6972C(block + 0x14, sp + 8), func_08A76AB4(sp + 0x18, sp + 0xC) with sp[0xC] = block) and slot 0xA0(o, sp + 0x18, sp + 0x14) runs with sp + 0x14 = func_08A85B1C(sp + 0x14, func_088A747C(s5)); then sp + 0x20 = func_08A950D0(sp + 0x20, 0, func_088A6C78(), 0x10) and func_088A6F74(s5, sp[0x20][0x14]) follow, and the same again with D_08C9E564 (0x08C9E564) on the holder re-read (null: the same
// handler): the slot 0xA0(o, sp + 0x10, sp + 0x20). Then func_08A76ABC(out, sp + 0x1C) and the holders sp + 0x10, sp + 0x20, sp + 0x1C are destroyed (func_08A5B9EC); v0 = out.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E554u});`
void func_08858298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t kBlockVtable = 0x08CB6B88u;
    const std::uint32_t scene = call_set(rt, ctx, 0x088546F0u, {key});
    const std::uint32_t holder = frame.at(0x1Cu);
    (void)call_set(rt, ctx, 0x08A813F4u, {holder});
    std::uint32_t target = word(rt, frame.at(0x1Cu));
    if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E554u});
    std::uint32_t callback = slot_of(rt, target, 0xA0u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(8u), 0x08C9E55Cu});
    std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call_set(rt, ctx, 0x08A76ACCu, {block});
    put(rt, block, kBlockVtable);
    (void)call_set(rt, ctx, 0x08A6972Cu, {block + 0x14u, frame.at(8u)});
    put(rt, frame.at(0xCu), block);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x18u), frame.at(0xCu)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(8u)});
    const std::uint32_t found = call_set(rt, ctx, 0x088A747Cu, {scene});
    (void)call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0x14u), found});
    (void)call_set(rt, ctx, callback, {target, frame.at(0x18u), frame.at(0x14u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
    const std::uint32_t length = call_set(rt, ctx, 0x088A6C78u, {});
    (void)call_set(rt, ctx, 0x08A950D0u, {frame.at(0x20u), 0u, length, 0x10u});
    (void)call_set(rt, ctx, 0x088A6F74u, {scene, word(rt, word(rt, frame.at(0x20u)) + 0x14u)});
    target = word(rt, frame.at(0x1Cu));
    if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E554u});
    callback = slot_of(rt, target, 0xA0u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08C9E564u});
    block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call_set(rt, ctx, 0x08A76ACCu, {block});
    put(rt, block, kBlockVtable);
    (void)call_set(rt, ctx, 0x08A6972Cu, {block + 0x14u, frame.at(0u)});
    put(rt, frame.at(4u), block);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AB4u, {frame.at(0x10u), frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    (void)call_set(rt, ctx, callback, {target, frame.at(0x10u), frame.at(0x20u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
    (void)call_set(rt, ctx, 0x08A76ABCu, {out, holder});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x20u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[2] = out;
}

// func_08888988(this, fp, a, b): nothing happens (v0 as it was) when a or b is null. n = the virtual slot 0x24 of this answers a count (nothing when <= 0); for each index i below n: the slot 0x28 of this (sp + 8, this, i) fills the counted pair {sp[8] object, sp[0xC] counted block}; with a non-null object o, c = the slot 0x18 of a (a, i) must answer true for its slots 0xC and 0x10 and the slot 0x1C of a (a, i)
// give r (zero: the pair is just released); then q = the slot 0x64 of b (b) and the slot 0x10 of o runs with (o, fp, r, b, i == q); the global object D_08CFFAAC's word (0x08CFFAACu) g gets its slot 0x1C with (g, sp) where {sp[0], sp[4]} is a copy of the pair (the counted block's strong count is incremented first and the copy is released afterwards). The pair's counted block sp[0xC] is released at the end of every round (release_counted).
// v0 = the last value the asm leaves (the release_counted note).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first == 0u || second == 0u) return;`; `if (signed_of(count) <= 0) return;`; `if (word(rt, pair) != 0u) {`; `if (ready) ready = call_set(rt, ctx, slot_of(rt, item, 0x10u), {item}) != 0u;`; `if (ready) {`; `if (answer != 0u) {`; `if (pair_block != 0u) {`; `if (kept != 0u) release_counted(rt, ctx, kept);`; `if (block != 0u) release_counted(rt, ctx, block);`
void func_08888988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], fp = ctx.gpr[5], first = ctx.gpr[6], second = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    if (first == 0u || second == 0u) return;
    const std::uint32_t count = call_set(rt, ctx, slot_of(rt, self, 0x24u), {self});
    if (signed_of(count) <= 0) return;
    const std::uint32_t pair = frame.at(8u), copy = frame.at(0u);
    for (std::uint32_t index = 0u; index != count; ++index) {
        (void)call_set(rt, ctx, slot_of(rt, self, 0x28u), {pair, self, index});
        std::uint32_t block = word(rt, pair + 4u);
        if (word(rt, pair) != 0u) {
            const std::uint32_t item = call_set(rt, ctx, slot_of(rt, first, 0x18u), {first, index});
            bool ready = call_set(rt, ctx, slot_of(rt, item, 0xCu), {item}) != 0u;
            if (ready) ready = call_set(rt, ctx, slot_of(rt, item, 0x10u), {item}) != 0u;
            if (ready) {
                const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, first, 0x1Cu), {first, index});
                if (answer != 0u) {
                    const std::uint32_t chosen = call_set(rt, ctx, slot_of(rt, second, 0x64u), {second});
                    const std::uint32_t object = word(rt, pair);
                    const std::uint32_t same = (index == chosen) ? 1u : 0u;
                    (void)call_set(rt, ctx, slot_of(rt, object, 0x10u), {object, fp, answer, second, same});
                    const std::uint32_t global = word(rt, 0x08CFFAACu);
                    const std::uint32_t entry = word(rt, word(rt, global) + 0x1Cu);
                    const std::uint32_t pair_object = word(rt, pair), pair_block = word(rt, pair + 4u);
                    put(rt, copy, pair_object);
                    put(rt, copy + 4u, pair_block);
                    if (pair_block != 0u) {
                        put(rt, pair_block + 4u, word(rt, pair_block + 4u) + 1u);
                        ctx.gpr[2] = 1u;
                    }
                    (void)call_set(rt, ctx, entry, {global, copy});
                    const std::uint32_t kept = word(rt, copy + 4u);
                    if (kept != 0u) release_counted(rt, ctx, kept);
                }
            }
            block = word(rt, pair + 4u);
        }
        if (block != 0u) release_counted(rt, ctx, block);
    }
}

// func_089170E0(this, count): a vector of counted pointers, this = {begin, end, capacity, +0x18 byte 0, +0x19 byte 1}: filled by func_08BDC338(this, 0, count, sp + 8) with the null pointer {sp[8], sp[0xC]} = {0, 0} (the counted block of the value, sp[0xC], is released once afterwards), then every element i (offset i * 8) gets a new object: block = func_08B62344(0x18), func_08916A50(block),
// c = func_08B62344(0x10) = {D_08CC5890 (0x08CC5890), 1, 1, block}, func_08B8E380(sp + 4, block, block) (sp[0] = block, sp[4] = c), the pair {sp[0], sp[4]} is swapped with the element's {[0], [4]} and the old counted block (now sp[4]) is released. After each element the NEW object's first word (element[0][0]) is set to s7, the previous element's object (0 for the first);
// afterwards this[0x14], this[0xC] and this[0x10] = s7 = the last element's object (0 when count <= 0). v0 = the last element's address in the loop case, otherwise the register the asm leaves (see release_counted).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (value != 0u) release_counted(rt, ctx, value);`; `if (signed_of(count) > 0) {`; `if (old_control != 0u) release_counted(rt, ctx, old_control);`; `if (index == count) break;`; `offset += 8u;`
void func_089170E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    put8(rt, self + 0x19u, 1u);
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put8(rt, self + 0x18u, 0u);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0xCu), 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BDC338u, {self, 0u, count, frame.at(8u)});
    const std::uint32_t value = word(rt, frame.at(0xCu));
    if (value != 0u) release_counted(rt, ctx, value);
    std::uint32_t last = 0u;  // s7
    if (signed_of(count) > 0) {
        std::uint32_t offset = 0u;
        for (std::uint32_t index = 0u;;) {
            const std::uint32_t element = word(rt, self) + offset;
            const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0x18u});
            (void)call_set(rt, ctx, 0x08916A50u, {block});
            put(rt, frame.at(0u), block);  // the delay slot of the next jal
            const std::uint32_t control = call_set(rt, ctx, 0x08B62344u, {0x10u});
            put(rt, control, 0x08CC5890u);
            put(rt, control + 4u, 1u);
            put(rt, control + 0xCu, block);
            put(rt, control + 8u, 1u);
            put(rt, frame.at(4u), control);  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08B8E380u, {frame.at(4u), block, block});
            const std::uint32_t old_first = word(rt, element);
            const std::uint32_t new_first = word(rt, frame.at(0u));
            put(rt, frame.at(0u), old_first);
            put(rt, element, new_first);
            const std::uint32_t new_second = word(rt, frame.at(4u));
            put(rt, frame.at(4u), word(rt, element + 4u));
            put(rt, element + 4u, new_second);
            const std::uint32_t old_control = word(rt, frame.at(4u));
            if (old_control != 0u) release_counted(rt, ctx, old_control);
            ++index;
            put(rt, word(rt, word(rt, self) + offset), last);
            const std::uint32_t after = word(rt, self) + offset;
            last = word(rt, after);  // the delay slot of the bne
            ctx.gpr[2] = after;
            if (index == count) break;
            offset += 8u;
        }
    }
    put(rt, self + 0x14u, last);
    put(rt, self + 0xCu, last);
    put(rt, self + 0x10u, last);
}

// func_089936C4(this, arg): when func_089673F4(this) answers non-zero a transform is built (func_08A17C48(sp + 0x18, this + 0x120), func_08A16EA4(sp + 8), func_08A180CC(sp + 0x18, this), func_08A16EB8(sp + 8, this)) and every child c of this[0xF4] (next = func_08968A38(c)) for which func_08A00448(this, c) answers non-zero is handled by the kind of c: this[0x138] == c: func_08A18494(sp + 0x18, c) and
// func_08A18010(sp + 0x18, c, this + 0x148, 0, this + 0x14C, 0); c == this[0x13C] or this[0x140]: func_08A18494(sp + 0x18, c); anything else: func_08A17104(sp + 8, c). Then k = func_08993668(this) (nothing more when 0): func_089687D8(this[0x140], k[0x1D]) (when the object is non-null) and func_089687D8(this[0x13C], k[0x1C]); f20 = k[0x18], f21 = k[0x14] (callee-saved floats, restored); s2 = 1 when this[0x134] is 0 or
// when it is 1 and f20 < 1.0f. With this[0x138] non-zero func_08A17C48(sp + 0x18, this + 0x120) is rebuilt and func_08A18270(sp + 0x18, this[0x138], sp, 0, sp + 4, 0) runs with sp[0] = this[0x148] + f21 * this[0x14C] and sp[4] = f20 * this[0x14C]; a non-zero s2 ends with func_08A01C30(this, arg). v0 = the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (child == word(rt, self + 0x138u)) {`; `} else if (child == word(rt, self + 0x13Cu) || child == word(rt, self + 0x140u)) {`; `} else {`; `if (kind == 0u) return;`; `if (word(rt, self + 0x140u) != 0u) (void)call_set(rt, ctx, 0x089687D8u, {word(rt, self + 0x140u), byte_u(rt...`; `if (word(rt, self + 0x13Cu) != 0u) (void)call_set(rt, ctx, 0x089687D8u, {word(rt, self + 0x13Cu), byte_u(rt...`; `if (word(rt, self + 0x138u) != 0u) {`
void func_089936C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], arg = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const SavedFloat keep20(ctx, 20u), keep21(ctx, 21u);
    const std::uint32_t transform = frame.at(0x18u), local = frame.at(8u);
    if (call_set(rt, ctx, 0x089673F4u, {self}) != 0u) {
        (void)call_set(rt, ctx, 0x08A17C48u, {transform, self + 0x120u});
        (void)call_set(rt, ctx, 0x08A16EA4u, {local});
        (void)call_set(rt, ctx, 0x08A180CCu, {transform, self});
        (void)call_set(rt, ctx, 0x08A16EB8u, {local, self});
        std::uint32_t child = word(rt, self + 0xF4u);
        while (child != 0u) {
            if (call_set(rt, ctx, 0x08A00448u, {self, child}) != 0u) {
                if (child == word(rt, self + 0x138u)) {
                    (void)call_set(rt, ctx, 0x08A18494u, {transform, child});
                    (void)call_set(rt, ctx, 0x08A18010u, {transform, word(rt, self + 0x138u), self + 0x148u, 0u, self + 0x14Cu, 0u});
                } else if (child == word(rt, self + 0x13Cu) || child == word(rt, self + 0x140u)) {
                    (void)call_set(rt, ctx, 0x08A18494u, {transform, child});
                } else {
                    (void)call_set(rt, ctx, 0x08A17104u, {local, child});
                }
            }
            child = call_set(rt, ctx, 0x08968A38u, {child});
        }
    }
    const std::uint32_t kind = call_set(rt, ctx, 0x08993668u, {self});
    if (kind == 0u) return;
    if (word(rt, self + 0x140u) != 0u) (void)call_set(rt, ctx, 0x089687D8u, {word(rt, self + 0x140u), byte_u(rt, kind + 0x1Du)});
    if (word(rt, self + 0x13Cu) != 0u) (void)call_set(rt, ctx, 0x089687D8u, {word(rt, self + 0x13Cu), byte_u(rt, kind + 0x1Cu)});
    const float f20 = float_at(rt, kind + 0x18u), f21 = float_at(rt, kind + 0x14u);
    ctx.fpr[20] = f20;
    ctx.fpr[21] = f21;
    const std::uint32_t mode = word(rt, self + 0x134u);
    bool finish = mode == 0u;
    if (mode == 1u) finish = f20 < 1.0f;
    if (word(rt, self + 0x138u) != 0u) {
        (void)call_set(rt, ctx, 0x08A17C48u, {transform, self + 0x120u});
        const float scale = float_at(rt, self + 0x14Cu), base = float_at(rt, self + 0x148u);
        put_float(rt, frame.at(0u), base + psp_mul(f21, scale));
        put_float(rt, frame.at(4u), psp_mul(f20, scale));  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A18270u, {transform, word(rt, self + 0x138u), frame.at(0u), 0u, frame.at(4u), 0u});
    }
    if (finish) (void)call_set(rt, ctx, 0x08A01C30u, {self, arg});
}

// func_0899BEC0(this): builds an array of this[0x2C] vector's count + 1 words from a holder made by func_08A76ABC(sp + 0x10, this + 0x38) (nothing happens when that holder is empty); the array {sp[0x18] begin, sp[0x1C] end, sp[0x20] capacity} is allocated with func_08A5BC20(bytes, 4), filled by func_08B85AF4(begin, count, sp + 4, sp + 1), its first word
// is set from this[0x28] through the func_089AB244(sp + 0xC, sp + 8) holder and the words 1.. are copied (func_08A5BAD8(dest, *source) when dest != source) from the vector at this + 0x2C ({begin, end}); the holder's object o (sp[0x10]; null: func_08A5BB7C(D_08CA46E0) first) then runs its virtual slot 0xCC with (sp + 0x14, count, array begin), the array is destroyed
// (func_08A5B9EC, func_08B856B0(begin, end, sp), the block freed with func_08A5BC54(begin, bytes, 4)) and the holder at sp + 0x10 is destroyed. v0 = 0.
// The delay slots of the jals run before the call (sp[0x1C] = begin before func_08A76AA0, sp[8] = this[0x28] before func_089AB244).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (word(rt, frame.at(0x10u)) != 0u) {`; `if (begin != frame.at(0xCu)) (void)call_set(rt, ctx, 0x08A5BAD8u, {begin, word(rt, frame.at(0xCu))});`; `if (!(index < total)) break;`; `const std::uint32_t to = word(rt, frame.at(0x18u)) + cursor;`; `if (to != from) {`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA46E0u});`; `if (free_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {free_begin, bytes, 4u});`
void func_0899BEC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t outer = frame.at(0x10u);
    (void)call_set(rt, ctx, 0x08A76ABCu, {outer, self + 0x38u});
    if (word(rt, frame.at(0x10u)) != 0u) {
        const std::uint32_t source = self + 0x2Cu;
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, source + 4u) - word(rt, self + 0x2Cu)) >> 2) + 1u;
        std::uint32_t bytes = count << 2;
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        put(rt, frame.at(0x20u), 0u);
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        put(rt, frame.at(0x20u), block + bytes);
        put(rt, frame.at(0x18u), block);
        put(rt, frame.at(0x1Cu), block);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(4u)});
        (void)call_set(rt, ctx, 0x08B85AF4u, {block, count, frame.at(4u), frame.at(1u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        const std::uint32_t begin = word(rt, frame.at(0x18u));
        put(rt, frame.at(0x1Cu), bytes + begin);
        put(rt, frame.at(8u), word(rt, self + 0x28u));  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x089AB244u, {frame.at(0xCu), frame.at(8u)});
        if (begin != frame.at(0xCu)) (void)call_set(rt, ctx, 0x08A5BAD8u, {begin, word(rt, frame.at(0xCu))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
        std::uint32_t a1 = word(rt, self + 0x2Cu), a2 = word(rt, source + 4u);
        std::uint32_t index = 0u, cursor = 4u;
        for (;;) {
            const std::uint32_t total = static_cast<std::uint32_t>(signed_of(a2 - a1) >> 2);
            const std::uint32_t from = cursor - 4u + a1;
            if (!(index < total)) break;
            const std::uint32_t to = word(rt, frame.at(0x18u)) + cursor;
            ++index;
            cursor += 4u;
            if (to != from) {
                (void)call_set(rt, ctx, 0x08A5BAD8u, {to, word(rt, from)});
                a2 = word(rt, source + 4u);
                a1 = word(rt, source);
            }
        }
        const std::uint32_t holder = frame.at(0x14u);
        (void)call_set(rt, ctx, 0x08A76AA0u, {holder});
        const std::uint32_t object = word(rt, frame.at(0x10u));
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA46E0u});
        const std::uint32_t begin_now = word(rt, frame.at(0x18u)), end_now = word(rt, frame.at(0x1Cu));
        (void)call_set(rt, ctx, slot_of(rt, object, 0xCCu), {object, holder, static_cast<std::uint32_t>(signed_of(end_now - begin_now) >> 2), begin_now});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
        const std::uint32_t array = word(rt, frame.at(0x18u));
        (void)call_set(rt, ctx, 0x08B856B0u, {array, word(rt, frame.at(0x1Cu)), frame.at(0u)});
        const std::uint32_t free_begin = word(rt, frame.at(0x18u));
        bytes = static_cast<std::uint32_t>(signed_of(word(rt, frame.at(0x20u)) - free_begin) >> 2) << 2;
        if (free_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {free_begin, bytes, 4u});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {outer});
    ctx.gpr[2] = 0u;
}

// func_0899CB44(this): the same build as func_0899BEC0 for the vector object this[0x28] (its {begin, end} at +0x14 / +0x18; null: func_08A5BB7C(D_08CA483C) first), handed to this[0x24]'s virtual slot 0xCC (null: func_08A5BB7C(D_08CA47D4) first). Runs only when the virtual slot 0x10C of this returns non-zero
// and this[0x24] is non-zero; the array {sp[0x14], sp[0x18], sp[0x1C]} of count + 1 words has its first word from this[0x1C] (func_08A76AAC(sp + 0xC, sp + 8)) and the words 1.. copied from the vector's elements. v0 = 1 on every path.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (call_set(rt, ctx, slot_of(rt, self, 0x10Cu), {self}) != 0u && word(rt, self + 0x24u) != 0u) {`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA483Cu});`; `if (begin != frame.at(0xCu)) (void)call_set(rt, ctx, 0x08A5BAD8u, {begin, word(rt, frame.at(0xCu))});`; `if (elements != 0u) {`; `if (to != from) (void)call_set(rt, ctx, 0x08A5BAD8u, {to, word(rt, from)});`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA47D4u});`; `if (free_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {free_begin, bytes, 4u});`
void func_0899CB44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    if (call_set(rt, ctx, slot_of(rt, self, 0x10Cu), {self}) != 0u && word(rt, self + 0x24u) != 0u) {
        const std::uint32_t holder_owner = self + 0x28u;
        std::uint32_t object = word(rt, self + 0x28u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA483Cu});
        const std::uint32_t elements = static_cast<std::uint32_t>(signed_of(word(rt, object + 0x18u) - word(rt, object + 0x14u)) >> 2);
        const std::uint32_t count = elements + 1u;
        std::uint32_t bytes = count << 2;
        put(rt, frame.at(0x14u), 0u);
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        put(rt, frame.at(0x1Cu), block + bytes);
        put(rt, frame.at(0x14u), block);
        put(rt, frame.at(0x18u), block);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(4u)});
        (void)call_set(rt, ctx, 0x08B85AF4u, {block, count, frame.at(4u), frame.at(1u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        const std::uint32_t begin = word(rt, frame.at(0x14u));
        put(rt, frame.at(0x18u), bytes + begin);
        put(rt, frame.at(8u), word(rt, self + 0x1Cu));  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(0xCu), frame.at(8u)});
        if (begin != frame.at(0xCu)) (void)call_set(rt, ctx, 0x08A5BAD8u, {begin, word(rt, frame.at(0xCu))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
        if (elements != 0u) {
            std::uint32_t index = 0u, cursor = 4u;
            do {
                const std::uint32_t vector = word(rt, holder_owner);
                const std::uint32_t to = word(rt, frame.at(0x14u)) + cursor;
                const std::uint32_t from = cursor - 4u + word(rt, vector + 0x14u);
                ++index;
                cursor += 4u;
                if (to != from) (void)call_set(rt, ctx, 0x08A5BAD8u, {to, word(rt, from)});
            } while (index < elements);
        }
        const std::uint32_t holder = frame.at(0x10u);
        (void)call_set(rt, ctx, 0x08A76AA0u, {holder});
        object = word(rt, self + 0x24u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA47D4u});
        const std::uint32_t begin_now = word(rt, frame.at(0x14u)), end_now = word(rt, frame.at(0x18u));
        (void)call_set(rt, ctx, slot_of(rt, object, 0xCCu), {object, holder, static_cast<std::uint32_t>(signed_of(end_now - begin_now) >> 2), begin_now});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
        (void)call_set(rt, ctx, 0x08B856B0u, {word(rt, frame.at(0x14u)), word(rt, frame.at(0x18u)), frame.at(0u)});
        const std::uint32_t free_begin = word(rt, frame.at(0x14u));
        bytes = static_cast<std::uint32_t>(signed_of(word(rt, frame.at(0x1Cu)) - free_begin) >> 2) << 2;
        if (free_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {free_begin, bytes, 4u});
    }
    ctx.gpr[2] = 1u;
}

// func_089B02F0(this, f12): stores f12 at this[0x110]; then, when func_08B3453C(D_08CF4A7C) (0x08CF4A7C) returns non-zero (a lock whose release func_08B3457C(D_08CF4A7C) comes before the checks below, except on the path that skips the block), and the byte this[0x3C] is non-zero: that byte is cleared, k = func_089AAC8C(this),
// func_08B344E0(D_08CF4A54) (0x08CF4A54), the range this[0x24] .. this[0x18] handed to func_08BFBB38 when the two addresses (this + 0x24, this + 0x18) differ, func_08B3457C(D_08CF4A54), and when func_089AAC8C(this) answers something else than k: o = this[0x14], func_0897BB18(o, func_089D5B30()), o[0x1B0] = 0, the two bits of this[0x70] clear
// func_08977390(o, 0) / func_08977390(o, 1); with the new answer m non-zero the bytes this[0xF8] / this[0xF9] get bit 7 of m[0x138] inverted and m[0x120] (1: func_089AF5E8(this, 0); 2: func_089AF5E8(this, 1)) picks the call, then func_089AFF3C(this, 0), func_089B0284(this, 0), func_089B00BC(this). Then (the byte this[0x6D] clear) func_089A8978(this, element) for every word
// of the range this[0x24] .. this[0x28]; r = func_089AB0C8(this) and, when non-zero, func_089BD4DC(r) compared with func_089AAC8C(this): when they differ func_08A0BE48(sp, this, r), func_0897783C(this[0x14], sp), func_08A5B9EC(sp), func_089AB114(this, 0). v0 = func_089AB274(this)'s.
// The delay slots: s0 = k and s3 = the second answer are set before the next calls; a0 = this at the jal of func_089AFF3C, a2 = r at the beq after func_089BD4DC (both paths).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (call_set(rt, ctx, 0x08B3453Cu, {lock}) != 0u) {`; `if (byte_u(rt, self + 0x3Cu) != 0u) {`; `if (range != other) (void)call_set(rt, ctx, 0x08BFBB38u, {range, other});`; `if (before != after) {`; `if ((bits & 1u) != 0u) (void)call_set(rt, ctx, 0x08977390u, {owner, 0u});`; `if ((bits & 2u) != 0u) (void)call_set(rt, ctx, 0x08977390u, {owner, 1u});`; `if (after != 0u) {`; `if (mode == 1u) (void)call_set(rt, ctx, 0x089AF5E8u, {self, 0u});`; `else if (mode == 2u) (void)call_set(rt, ctx, 0x089AF5E8u, {self, 1u});`; `if (flag == 0u) {`; `if (found != 0u) {`; `if (expected != current) {`
void func_089B02F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    put_float(rt, self + 0x110u, ctx.fpr[12]);
    const std::uint32_t lock = 0x08CF4A7Cu, queue_lock = 0x08CF4A54u;
    std::uint32_t flag;
    if (call_set(rt, ctx, 0x08B3453Cu, {lock}) != 0u) {
        if (byte_u(rt, self + 0x3Cu) != 0u) {
            put8(rt, self + 0x3Cu, 0u);
            const std::uint32_t before = call_set(rt, ctx, 0x089AAC8Cu, {self});
            (void)call_set(rt, ctx, 0x08B344E0u, {queue_lock});
            const std::uint32_t range = self + 0x24u, other = self + 0x18u;
            if (range != other) (void)call_set(rt, ctx, 0x08BFBB38u, {range, other});
            (void)call_set(rt, ctx, 0x08B3457Cu, {queue_lock});
            const std::uint32_t after = call_set(rt, ctx, 0x089AAC8Cu, {self});
            if (before != after) {
                const std::uint32_t owner = word(rt, self + 0x14u);  // the delay slot of the jal
                const std::uint32_t shared = call_set(rt, ctx, 0x089D5B30u, {});
                (void)call_set(rt, ctx, 0x0897BB18u, {owner, shared});
                put(rt, owner + 0x1B0u, 0u);
                const std::uint32_t bits = word(rt, self + 0x70u);
                if ((bits & 1u) != 0u) (void)call_set(rt, ctx, 0x08977390u, {owner, 0u});
                if ((bits & 2u) != 0u) (void)call_set(rt, ctx, 0x08977390u, {owner, 1u});
                if (after != 0u) {
                    const std::uint32_t inverted = ((word(rt, after + 0x138u) >> 7) & 1u) ^ 1u;
                    put8(rt, self + 0xF8u, inverted);
                    put8(rt, self + 0xF9u, inverted);
                    const std::uint32_t mode = word(rt, after + 0x120u);
                    if (mode == 1u) (void)call_set(rt, ctx, 0x089AF5E8u, {self, 0u});
                    else if (mode == 2u) (void)call_set(rt, ctx, 0x089AF5E8u, {self, 1u});
                }
                (void)call_set(rt, ctx, 0x089AFF3Cu, {self, 0u});
                (void)call_set(rt, ctx, 0x089B0284u, {self, 0u});
                (void)call_set(rt, ctx, 0x089B00BCu, {self});
            }
        }
        (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
    }
    flag = byte_u(rt, self + 0x6Du);
    if (flag == 0u) {
        const std::uint32_t end = word(rt, self + 0x28u);
        std::uint32_t cursor = word(rt, self + 0x24u);
        if (cursor != end) {
            do {
                const std::uint32_t element = word(rt, cursor);
                cursor += 4u;
                (void)call_set(rt, ctx, 0x089A8978u, {self, element});
            } while (end != cursor);
        }
    }
    const std::uint32_t found = call_set(rt, ctx, 0x089AB0C8u, {self});
    if (found != 0u) {
        const std::uint32_t expected = call_set(rt, ctx, 0x089BD4DCu, {found});
        const std::uint32_t current = call_set(rt, ctx, 0x089AAC8Cu, {self});
        ctx.gpr[6] = found;  // the delay slot of the beq
        if (expected != current) {
            const std::uint32_t owner = word(rt, self + 0x14u);
            (void)call_set(rt, ctx, 0x08A0BE48u, {frame.at(0u), self, found});
            (void)call_set(rt, ctx, 0x0897783Cu, {owner, frame.at(0u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
            (void)call_set(rt, ctx, 0x089AB114u, {self, 0u});
        }
    }
    (void)call_set(rt, ctx, 0x089AB274u, {self});
}

// func_089B90B4(this, object): func_089B8BA0(this[0xC]); with this[8] negative a string holder at sp + 4 is made (func_089BAC14), the record at sp + 8 ({0x08CCED90, sp[0xC] = D_08D74FD0 + 0xC (the shared empty cell), sp[0x10] = -1, sp[0x14] = sp + 4}) goes to func_08A189C0(sp + 8, object) and func_089B8C6C(this[0xC], sp[4], s2) with s2 = this[0xC][0xC]
// (read before the first call), the cell sp[0xC] is released (release_cell, flag sp + 1) and the holder at sp + 4 destroyed. With this[8] non-negative the same record is built per round (sp[0xC] = D_08D74FD0 + 0xC) and the virtual slot 8 of `object` runs with (object, sp + 8); the cell is released (flag sp), then, until object[0x34] is 0x103, a non-empty sp[4]
// gives func_089B8C6C(this[0xC], sp[4], s2) and the holder is destroyed, the next round starting at func_089BAC14; when it is 0x103 the holder is destroyed and func_089EC958(object, 0x7D) ends it. v0 = the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(word(rt, self + 8u)) < 0) {`; `if (pending != 0u) (void)call_set(rt, ctx, 0x089B8C6Cu, {word(rt, self + 0xCu), pending, s2});`
void func_089B90B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t holder = frame.at(4u), record = frame.at(8u);
    (void)call_set(rt, ctx, 0x089B8BA0u, {word(rt, self + 0xCu)});  // the delay slot loads a0 = this[0xC]
    if (signed_of(word(rt, self + 8u)) < 0) {
        const std::uint32_t s2 = word(rt, word(rt, self + 0xCu) + 0xCu);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x089BAC14u, {holder});
        put(rt, frame.at(0xCu), 0x08D74FD0u + 0xCu);
        put(rt, frame.at(0x10u), 0xFFFFFFFFu);
        put(rt, record, 0x08CCED90u);
        put(rt, frame.at(0x14u), holder);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A189C0u, {record, object});
        (void)call_set(rt, ctx, 0x089B8C6Cu, {word(rt, self + 0xCu), word(rt, holder), s2});
        put(rt, record, 0x08CCD990u);
        release_cell(rt, ctx, word(rt, frame.at(0xCu)), frame.at(1u));
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    const std::uint32_t s2 = word(rt, word(rt, self + 0xCu) + 0xCu);  // the delay slot of the bgezl (taken)
    for (;;) {
        (void)call_set(rt, ctx, 0x089BAC14u, {holder});
        put(rt, frame.at(0xCu), 0x08D74FD0u + 0xCu);
        put(rt, frame.at(0x10u), 0xFFFFFFFFu);
        put(rt, record, 0x08CCED90u);
        put(rt, frame.at(0x14u), holder);
        (void)call_set(rt, ctx, slot_of(rt, object, 8u), {object, record});
        const std::uint32_t data = word(rt, frame.at(0xCu));
        put(rt, record, 0x08CCD990u);  // the delay slot of the beq: runs on both paths
        release_cell(rt, ctx, data, frame.at(0u));
        if (word(rt, object + 0x34u) == 0x103u) break;
        const std::uint32_t pending = word(rt, holder);
        if (pending != 0u) (void)call_set(rt, ctx, 0x089B8C6Cu, {word(rt, self + 0xCu), pending, s2});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
    (void)call_set(rt, ctx, 0x089EC958u, {object, 0x7Du});
}

// func_089BACAC(this, name): a lookup of `name` (a string cell: its length is the word at data - 0xC) in the registry s4 = this[0x4C]. With an empty name or a null registry: sp = func_08A69654(sp, name), func_08A7BCC8(this, sp), func_08A69D68(sp) and done. Otherwise p = func_08A7BCC0(this, name), the pair of holders sp + 0x1C = func_08A6972C(_, p) and
// sp + 0x20 = func_08A69654(_, name), func_08A5D438(sp + 0x24) and func_08A7D5FC(s4, sp + 0x24, sp + 0x1C) find the entry e = sp[0x24]. With an entry: func_08A76AA0(sp + 0x28), its virtual slot 0x1C (e, sp + 0x28, sp + 0x18 (func_08A76AA0 first)) fills sp + 0x28 and sp + 0x18 is destroyed; when sp[0x28] is this itself and the two holders differ (sp[0x1C] != sp[0x20]) func_08A5BA28(this), func_08A7DE18(s4, sp + 0x1C),
// func_08A7F5FC(sp + 0x14, s4, sp + 0x20), func_08A7BCC8(this, sp + 0x14) (destroyed with func_08A69D68), func_08A18F14(sp + 0x10, this), func_08A7C5C8(s4, sp + 0x10) (destroyed) and func_08A5B970(this) run; when sp[0x28] is something else the same without func_08A5BA28 / func_08A7DE18 (func_08A7F5FC(sp + 0xC, s4, sp + 0x20), func_08A7BCC8(this, sp + 0xC), func_08A69D68(sp + 0xC),
// func_08A18F14(sp + 8, this), func_08A7C5C8(s4, sp + 8), destroy); sp + 0x28 is destroyed. Without an entry: func_08A7BCC8(this, sp + 0x20) when the holders differ, then func_08A18F14(sp + 4, this), func_08A7C5C8(s4, sp + 4), destroy. At the end sp + 0x24 is destroyed and the holders sp + 0x20 / sp + 0x1C get func_08A69D68. v0 = the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (word(rt, word(rt, name) - 0xCu) == 0u || registry == 0u) {`; `if (word(rt, found) != 0u) {`; `if (entry == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5CA0u});`; `if (word(rt, result) != self) {`; `} else if (word(rt, first) != word(rt, second)) {`; `if (word(rt, first) != word(rt, second)) (void)call_set(rt, ctx, 0x08A7BCC8u, {self, second});`
void func_089BACAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], name = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t registry = word(rt, self + 0x4Cu);
    if (word(rt, word(rt, name) - 0xCu) == 0u || registry == 0u) {
        (void)call_set(rt, ctx, 0x08A69654u, {frame.at(0u), name});
        (void)call_set(rt, ctx, 0x08A7BCC8u, {self, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
        return;
    }
    const std::uint32_t first = frame.at(0x1Cu), second = frame.at(0x20u), found = frame.at(0x24u);
    const std::uint32_t path = call_set(rt, ctx, 0x08A7BCC0u, {self, name});
    (void)call_set(rt, ctx, 0x08A6972Cu, {first, path});
    (void)call_set(rt, ctx, 0x08A69654u, {second, name});
    (void)call_set(rt, ctx, 0x08A5D438u, {found});
    (void)call_set(rt, ctx, 0x08A7D5FCu, {registry, found, first});
    if (word(rt, found) != 0u) {
        const std::uint32_t result = frame.at(0x28u);
        (void)call_set(rt, ctx, 0x08A76AA0u, {result});
        const std::uint32_t entry = word(rt, found);
        if (entry == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5CA0u});
        const std::uint32_t slot = slot_of(rt, entry, 0x1Cu);  // read in the delay slot of the jal
        const std::uint32_t scratch = frame.at(0x18u);
        (void)call_set(rt, ctx, 0x08A76AA0u, {scratch});
        (void)call_set(rt, ctx, slot, {entry, result, scratch});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {scratch});
        if (word(rt, result) != self) {
            const std::uint32_t local = frame.at(0xCu);
            (void)call_set(rt, ctx, 0x08A7F5FCu, {local, registry, second});
            (void)call_set(rt, ctx, 0x08A7BCC8u, {self, local});
            (void)call_set(rt, ctx, 0x08A69D68u, {local});
            (void)call_set(rt, ctx, 0x08A18F14u, {frame.at(8u), self});
            (void)call_set(rt, ctx, 0x08A7C5C8u, {registry, frame.at(8u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        } else if (word(rt, first) != word(rt, second)) {
            (void)call_set(rt, ctx, 0x08A5BA28u, {self});
            const std::uint32_t local = frame.at(0x14u);
            (void)call_set(rt, ctx, 0x08A7DE18u, {registry, first});
            (void)call_set(rt, ctx, 0x08A7F5FCu, {local, registry, second});
            (void)call_set(rt, ctx, 0x08A7BCC8u, {self, local});
            (void)call_set(rt, ctx, 0x08A69D68u, {local});
            (void)call_set(rt, ctx, 0x08A18F14u, {frame.at(0x10u), self});
            (void)call_set(rt, ctx, 0x08A7C5C8u, {registry, frame.at(0x10u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
            (void)call_set(rt, ctx, 0x08A5B970u, {self});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {result});
    } else {
        if (word(rt, first) != word(rt, second)) (void)call_set(rt, ctx, 0x08A7BCC8u, {self, second});
        (void)call_set(rt, ctx, 0x08A18F14u, {frame.at(4u), self});
        (void)call_set(rt, ctx, 0x08A7C5C8u, {registry, frame.at(4u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {found});
    (void)call_set(rt, ctx, 0x08A69D68u, {second});
    (void)call_set(rt, ctx, 0x08A69D68u, {first});
}

// func_089C9F24(this, out, name, flag): a string holder sp[4] = func_08B85EB8(sp + 4, D_08CA6680 (0x08CA6680), sp + 2) and func_089C8818(sp + 8, name, sp + 4) build a record at sp + 8, the first holder is released (release_cell, flag sp + 1); func_08B2E05C() and func_089C843C(this) bracket the work: when func_089C9CE4(sp + 8, out) answers non-zero
// this[0x3C] (null: func_08A5BB7C(D_08CA6384) first) gets func_08A7CE50(this[0x3C], out), then out[0] = o (null: func_08A5BB7C(D_08CA6378) first at each use) is given func_089B47EC(o, sp + 8), its virtual slots 0x12C and 0x130 run (a0 = o) and, when flag & 0xFF is non-zero, slot 0x150 with (o, this); the result flag s1 is 1 after
// slot 0x130 runs. func_089C83A4(this), func_08B2E05C(), the record at sp + 8 is released (flag sp) and v0 = s1 (0 or 1) on every path.
[[maybe_unused]] void func_089C9F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5], name = ctx.gpr[6], flag = ctx.gpr[7] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = frame.at(4u), record = frame.at(8u);
    (void)call_set(rt, ctx, 0x08B85EB8u, {first, 0x08CA6680u, frame.at(2u)});
    (void)call_set(rt, ctx, 0x089C8818u, {record, name, first});
    release_cell(rt, ctx, word(rt, first), frame.at(1u));
    std::uint32_t done = 0u;  // s1
    (void)call_set(rt, ctx, 0x08B2E05Cu, {});
    (void)call_set(rt, ctx, 0x089C843Cu, {self});
    if (call_set(rt, ctx, 0x089C9CE4u, {record, out}) != 0u) {
        std::uint32_t object = word(rt, self + 0x3Cu);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6384u});
        (void)call_set(rt, ctx, 0x08A7CE50u, {object, out});
        object = word(rt, out);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6378u});
        (void)call_set(rt, ctx, 0x089B47ECu, {object, record});
        object = word(rt, out);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6378u});
        (void)call_set(rt, ctx, slot_of(rt, object, 0x12Cu), {object});
        object = word(rt, out);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6378u});
        done = 1u;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot_of(rt, object, 0x130u), {object});
        if (flag != 0u) {
            object = word(rt, out);
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6378u});
            done = 1u;
            (void)call_set(rt, ctx, slot_of(rt, object, 0x150u), {object, self});
        }
    }
    (void)call_set(rt, ctx, 0x089C83A4u, {self});
    (void)call_set(rt, ctx, 0x08B2E05Cu, {});
    release_cell(rt, ctx, word(rt, record), frame.at(0u));
    ctx.gpr[2] = done;
}

// func_089CA474(out, name, kind, source): a string cell sp[0xC] starts as the shared empty cell (D_08D74FD0 + 0xC); kind 1 fills it with func_08B8A4C0(sp + 0xC, D_08CA668C (0x08CA668C), 8); kind 2 asks s = source[4] (null: func_08A5BB7C(D_08CA6304) first) for a string by its virtual slot 0xC (s, sp + 8), appends it with
// func_08B855E0(sp + 0xC, sp + 8) and releases it (release_cell, flag sp + 2). When the cell's length (the word at data - 0xC, which is v0) is non-zero: func_089C5850(sp + 0x10, name), func_089BAC14(sp + 0x14), o = source[0] (null: func_08A5BB7C(D_08CA6304); the first word of sp + 0x10 null: D_08CA63A0), o's slot 0xC fills sp + 4, func_089CA288(sp[0x10], sp + 0x14, sp + 4, sp + 0xC)
// runs, sp + 4 is released (flag sp + 1), the array at sp + 0x14 is assigned to *out (func_08A5BAD8(out, sp[0x14]) unless it is out) and both holders are destroyed. The cell sp + 0xC is released (flag sp) on every path. v0 = the last callee's (or the length when nothing is called after it).
[[maybe_unused]] void func_089CA474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], name = ctx.gpr[5], kind = ctx.gpr[6], source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t cell = frame.at(0xCu);
    put(rt, cell, 0x08D74FD0u + 0xCu);  // the delay slot of the bne
    if (kind == 1u) {
        (void)call_set(rt, ctx, 0x08B8A4C0u, {cell, 0x08CA668Cu, 8u});
    } else if (kind == 2u) {
        const std::uint32_t provider = word(rt, source + 4u);
        if (provider == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
        (void)call_set(rt, ctx, slot_of(rt, provider, 0xCu), {frame.at(8u), provider});
        (void)call_set(rt, ctx, 0x08B855E0u, {cell, frame.at(8u)});
        release_cell(rt, ctx, word(rt, frame.at(8u)), frame.at(2u));
    }
    std::uint32_t data = word(rt, cell);
    const std::uint32_t length = word(rt, data - 0xCu);
    ctx.gpr[2] = length;
    if (length != 0u) {
        const std::uint32_t names = frame.at(0x10u), array = frame.at(0x14u);
        (void)call_set(rt, ctx, 0x089C5850u, {names, name});
        (void)call_set(rt, ctx, 0x089BAC14u, {array});
        const std::uint32_t first = word(rt, frame.at(0x10u));
        if (first == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA63A0u});
        const std::uint32_t object = word(rt, source);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
        (void)call_set(rt, ctx, slot_of(rt, object, 0xCu), {frame.at(4u), object});
        (void)call_set(rt, ctx, 0x089CA288u, {first, array, frame.at(4u), cell});
        release_cell(rt, ctx, word(rt, frame.at(4u)), frame.at(1u));
        if (out != array) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x14u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {array});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {names});
        data = word(rt, cell);
    }
    release_cell(rt, ctx, data, frame.at(0u));
}

// func_089D2C68(this, item, packet): the result of func_0896ED94(this, item, packet) is returned when it is non-zero. Otherwise with k = packet[0x28] and "known" = func_089CD228(this, k) || func_089CC960(this, k): when known and packet[0x3C] is 0, this[0x1A8] = this[0x198]; the step s2 = this[0x12C] when |this[0x198] - this[0x1A8]| < 10 (signed) and this[0x130] otherwise;
// func_08B344E0(D_08CF5134) (0x08CF5134) takes the lock, then s0 = func_089CF9D4(this, s2) when func_089CD228(this, k) is non-zero, otherwise func_089CFAB8(this, s2) when func_089CC960(this, k) is non-zero (0 when neither), and func_08B3457C(D_08CF5134) releases it. With s0 == 0: s0 = 0 and this[0x1A8] = this[0x198].
// With s0 != 0: func_089D291C(this, item), this[0x1C0] = 1, a string sp[8] = func_08B88F44(sp + 8, this + 0x150), and when it is empty (the length word at data - 0xC) func_089BD4DC(this) = o (non-zero: func_08B855E0(sp + 8, o + 0x128) appends); a non-empty one is handed to func_08A36C3C(sp + 4) = h (null: func_08A5BB7C(D_08CA6A5C) first) and func_08A34DA8(h, sp[8]), then func_08A5B9EC(sp + 4);
// the string is released (release_cell, flag sp) and s0 = 1. Last, known (the two calls again, k) with packet[0x3C] non-zero gives s0 = 1; v0 = s0 on every path.
// The delay slots: v1 = this[0x198] before the max in every path; s2 and the lock calls follow the compare.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (result != 0u) {`; `if (!known) known = call_set(rt, ctx, 0x089CC960u, {self, key}) != 0u;`; `if (known && byte_u(rt, packet + 0x3Cu) == 0u) put(rt, self + 0x1A8u, word(rt, self + 0x198u));`; `const std::uint32_t step = std::max(distance, signed_of(0u - static_cast<std::uint32_t>(distance))) < 10 ? ...`; `if (call_set(rt, ctx, 0x089CD228u, {self, key}) != 0u) {`; `if (word(rt, word(rt, text) - 0xCu) == 0u) {`; `if (source != 0u) (void)call_set(rt, ctx, 0x08B855E0u, {text, source + 0x128u});`; `if (word(rt, word(rt, text) - 0xCu) != 0u) {`; `if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6A5Cu});`; `if (!again) again = call_set(rt, ctx, 0x089CC960u, {self, key}) != 0u;`; `if (again && byte_u(rt, packet + 0x3Cu) != 0u) result = 1u;`
void func_089D2C68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], item = ctx.gpr[5], packet = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t result = call_set(rt, ctx, 0x0896ED94u, {self, item, packet});
    if (result != 0u) {
        ctx.gpr[2] = result;
        return;
    }
    const std::uint32_t key = word(rt, packet + 0x28u);
    const std::uint32_t lock = 0x08CF5134u;
    bool known = call_set(rt, ctx, 0x089CD228u, {self, key}) != 0u;
    if (!known) known = call_set(rt, ctx, 0x089CC960u, {self, key}) != 0u;
    if (known && byte_u(rt, packet + 0x3Cu) == 0u) put(rt, self + 0x1A8u, word(rt, self + 0x198u));
    const std::int32_t distance = signed_of(word(rt, self + 0x198u) - word(rt, self + 0x1A8u));
    const std::uint32_t step = std::max(distance, signed_of(0u - static_cast<std::uint32_t>(distance))) < 10 ? word(rt, self + 0x12Cu) : word(rt, self + 0x130u);
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    if (call_set(rt, ctx, 0x089CD228u, {self, key}) != 0u) {
        result = call_set(rt, ctx, 0x089CF9D4u, {self, step});
    } else if (call_set(rt, ctx, 0x089CC960u, {self, key}) != 0u) {
        result = call_set(rt, ctx, 0x089CFAB8u, {self, step});
    } else {
        result = 0u;
    }
    (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
    if (result == 0u) {
        put(rt, self + 0x1A8u, word(rt, self + 0x198u));
    } else {
        (void)call_set(rt, ctx, 0x089D291Cu, {self, item});
        put(rt, self + 0x1C0u, 1u);
        const std::uint32_t text = frame.at(8u);
        (void)call_set(rt, ctx, 0x08B88F44u, {text, self + 0x150u});
        if (word(rt, word(rt, text) - 0xCu) == 0u) {
            const std::uint32_t source = call_set(rt, ctx, 0x089BD4DCu, {self});
            if (source != 0u) (void)call_set(rt, ctx, 0x08B855E0u, {text, source + 0x128u});
        }
        if (word(rt, word(rt, text) - 0xCu) != 0u) {
            const std::uint32_t holder = frame.at(4u);
            (void)call_set(rt, ctx, 0x08A36C3Cu, {holder});
            const std::uint32_t target = word(rt, holder);
            if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6A5Cu});
            (void)call_set(rt, ctx, 0x08A34DA8u, {target, word(rt, text)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
        }
        release_cell(rt, ctx, word(rt, text), frame.at(0u));
        result = 1u;
    }
    bool again = call_set(rt, ctx, 0x089CD228u, {self, key}) != 0u;
    if (!again) again = call_set(rt, ctx, 0x089CC960u, {self, key}) != 0u;
    if (again && byte_u(rt, packet + 0x3Cu) != 0u) result = 1u;
    ctx.gpr[2] = result;
}

// func_089F4140(this): a destructor. this[0] = D_08CC8288 (0x08CC8288), this[0x38] = D_08CC84C4 (0x08CC84C4); func_08A5B9EC on the members this + 0x268, 0x264, 0x260, 0x25C, 0x258, 0x254, 0x250, 0x24C (in that order); the string cells this[0x248], [0x244], [0x240], [0x23C], [0x238], [0x234], [0x230], [0x22C] are released (release_cell, the flag
// operand sp + 7, 6, 5, 4, 3, 2, 1, 0 in that order); func_089E2430(this) and func_08A5BC54(this, 0x280, 4) free the object (its v0 is the result).
void func_089F4140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CC8288u);
    put(rt, self + 0x38u, 0x08CC84C4u);
    for (std::uint32_t offset = 0x268u; offset >= 0x24Cu; offset -= 4u) (void)call_set(rt, ctx, 0x08A5B9ECu, {self + offset});
    for (std::uint32_t index = 0u; index < 8u; ++index) release_cell(rt, ctx, word(rt, self + 0x248u - 4u * index), frame.at(7u - index));
    (void)call_set(rt, ctx, 0x089E2430u, {self});
    (void)call_set(rt, ctx, 0x08A5BC54u, {self, 0x280u, 4u});
}

// func_08A233B0(a0, a1, count, list): nothing happens (v0 is left as it was) when count < 3. Otherwise the two 0x44-byte records at sp + 8 are zeroed, s2 = func_08ABC2B8(D_08CAAC44 (0x08CAAC44)) (0: v0 = 0, done) and func_08ABC608(s2, sp + 8) (0: done); then the list's objects (list[0], [4], [8], [0xC], [0x10], [0x14], each null: func_08A5BB7C(D_08CAAA48) first) are asked:
// the virtual slot 0x3C of [0], [4], [8] gives the index i, a value p and a value q; the slot 0xC of [0xC] fills the string sp[4] (a0 = sp + 4, a1 = the object), r = func_08B20A54(D_08CF64C4 (0x08CF64C4), sp[4], -1); the slot 0x3C of [0x10] gives s and the slot 0x54 of [0x14] a float f. The record i (sp + 8 + i * 0x44) gets [0] = p + (q << 10), [4] = r, [8] = s, [0x10] = f;
// func_08ABBE30(s2, sp + 8) follows and the string sp[4] is released (flag sp); on the early-out paths v0 = 0 (func_08ABC2B8 / func_08ABC608 answered 0), after release_cell's skip path v0 = D_08D74FD0 (0x08D74FD0) as the asm leaves it, otherwise the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(count) < 3) return;`; `if (handle == 0u) return;`; `if (call_set(rt, ctx, 0x08ABC608u, {handle, records}) == 0u) return;`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAA48u});`
void func_08A233B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[6], list = ctx.gpr[7];
    GuestFrame frame(ctx, 0xC0u);
    if (signed_of(count) < 3) return;
    const std::uint32_t records = frame.at(8u);
    for (std::uint32_t record = 0u; record != 2u; ++record) {
        put(rt, records + record * 0x44u, 0u);
        for (std::uint32_t offset = 4u; offset != 0x44u; offset += 4u) put(rt, records + record * 0x44u + offset, 0u);
    }
    const std::uint32_t handle = call_set(rt, ctx, 0x08ABC2B8u, {0x08CAAC44u});
    if (handle == 0u) return;
    if (call_set(rt, ctx, 0x08ABC608u, {handle, records}) == 0u) return;
    std::uint32_t values[3];
    for (std::uint32_t index = 0u; index < 3u; ++index) {
        const std::uint32_t object = word(rt, list + 4u * index);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAA48u});
        values[index] = call_set(rt, ctx, slot_of(rt, object, 0x3Cu), {object});
    }
    std::uint32_t object = word(rt, list + 0xCu);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAA48u});
    (void)call_set(rt, ctx, slot_of(rt, object, 0xCu), {frame.at(4u), object});
    const std::uint32_t named = call_set(rt, ctx, 0x08B20A54u, {0x08CF64C4u, word(rt, frame.at(4u)), 0xFFFFFFFFu});
    object = word(rt, list + 0x10u);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAA48u});
    const std::uint32_t third = call_set(rt, ctx, slot_of(rt, object, 0x3Cu), {object});
    object = word(rt, list + 0x14u);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAA48u});
    const float scale = call_keep(rt, ctx, slot_of(rt, object, 0x54u), {object}, {}).f0;
    const std::uint32_t record = frame.at(8u) + values[0] * 0x44u;
    put_float(rt, record + 0x10u, scale);
    put(rt, record, values[1] + (values[2] << 10));
    put(rt, record + 4u, named);
    put(rt, record + 8u, third);
    (void)call_set(rt, ctx, 0x08ABBE30u, {handle, records});
    ctx.gpr[2] = 0x08D74FD0u;
    release_cell(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
}

// The shared tail of func_08A26744 and func_08A26940 (a number text in the buffer sp + 0x14 becomes a string *out): with count == 1, list[1] == 0 or list[1]'s virtual slot 0x3C answering 0, the buffer itself becomes the string (func_08B85EB8(sp + 4, buffer, sp + 2), a copy func_08A6D2D4(sp + 8, sp + 4) assigned to *out unless it is out, destroyed, sp + 4 released with flag sp);
// otherwise the unit text u = unit(D_08D6950C (0x08D6950C)) is appended: func_08B37640(sp + 0x54, D_08CAB28C (0x08CAB28C, "%s%s"), buffer, u), then the same through sp + 0xC / sp + 0x10 with the release of sp + 0xC (flag sp + 1). v0 = D_08D74FD0 (0x08D74FD0) after a release that finds the shared empty cell, else the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (count != 1u) {`; `if (target != 0u) with_unit = call_set(rt, ctx, slot_of(rt, target, 0x3Cu), {target}) != 0u;`; `if (with_unit) {`; `if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, copy)});`
inline void number_text_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t out, std::uint32_t count, std::uint32_t list, std::uint32_t unit_callee) {
    const std::uint32_t buffer = frame.at(0x14u);
    bool with_unit = false;
    if (count != 1u) {
        const std::uint32_t target = word(rt, list + 4u);
        if (target != 0u) with_unit = call_set(rt, ctx, slot_of(rt, target, 0x3Cu), {target}) != 0u;
    }
    if (with_unit) {
        const std::uint32_t unit = call_set(rt, ctx, unit_callee, {0x08D6950Cu});
        (void)call_set(rt, ctx, 0x08B37640u, {frame.at(0x54u), 0x08CAB28Cu, buffer, unit});
        const std::uint32_t made = frame.at(0xCu), copy = frame.at(0x10u);
        (void)call_set(rt, ctx, 0x08B85EB8u, {made, frame.at(0x54u), frame.at(3u)});
        (void)call_set(rt, ctx, 0x08A6D2D4u, {copy, made});
        if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, copy)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
        ctx.gpr[2] = 0x08D74FD0u;
        release_cell(rt, ctx, word(rt, made), frame.at(1u));
    } else {
        const std::uint32_t made = frame.at(4u), copy = frame.at(8u);
        (void)call_set(rt, ctx, 0x08B85EB8u, {made, buffer, frame.at(2u)});
        (void)call_set(rt, ctx, 0x08A6D2D4u, {copy, made});
        if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, copy)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
        ctx.gpr[2] = 0x08D74FD0u;
        release_cell(rt, ctx, word(rt, made), frame.at(0u));
    }
}

// func_08A26744(out, count, list): nothing happens when count <= 0 (v0 as it was). Otherwise x = the virtual slot 0x54 of list[0] (null: func_08A5BB7C(D_08CAAFBC (0x08CAAFBC)) first) answers a float and f = func_08B491A8(D_08D6950C (0x08D6950C), f12 = x) (a unit conversion, f0); the number text
// func_08ABE514(trunc(f * 10 + 0.5), 0, 1, sp + 0x14, 0x40) (one decimal) goes to number_text_tail with the unit text func_08B48E44. trunc is Allegrex trunc.w.s (the recompiler's fpu_float_to_word_ct<1>), the multiply the PSP's (inf * 0 gives 0x7FC00000).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(count) <= 0) return;`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAFBCu});`
void func_08A26744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5], list = ctx.gpr[6];
    GuestFrame frame(ctx, 0x100u);
    if (signed_of(count) <= 0) return;
    const std::uint32_t object = word(rt, list);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAFBCu});
    const float x = call_keep(rt, ctx, slot_of(rt, object, 0x54u), {object}, {}).f0;
    ctx.fpr[12] = x;
    const float converted = call_keep(rt, ctx, 0x08B491A8u, {0x08D6950Cu}, {x}).f0;
    const float scaled = psp_mul(converted, 10.0f) + 0.5f;
    (void)call_set(rt, ctx, 0x08ABE514u, {ctx.fpu_float_to_word_ct<1u>(scaled), 0u, 1u, frame.at(0x14u), 0x40u});
    number_text_tail(rt, ctx, frame, out, count, list, 0x08B48E44u);
}

// func_08A26940(out, count, list): the same with a second scale: x as above, and when the word at D_08D6950C is 1 the number text is func_08ABE514(trunc((float)func_08B491D0(D_08D6950C, trunc(x), 0) * 0.1f + 0.5f), 0, 2, sp + 0x14, 0x40) (two decimals), otherwise func_08ABE514(trunc(x * 10 + 0.5), 0, 1, sp + 0x14, 0x40);
// the tail is number_text_tail with the unit text func_08B48E78. (The recompiled 0.1f constant is 0x3DCCCCCD.)
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(count) <= 0) return;`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAFBCu});`; `if (word(rt, 0x08D6950Cu) == 1u) {`; `} else {`
void func_08A26940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5], list = ctx.gpr[6];
    GuestFrame frame(ctx, 0x100u);
    if (signed_of(count) <= 0) return;
    const std::uint32_t object = word(rt, list);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAFBCu});
    const float x = call_keep(rt, ctx, slot_of(rt, object, 0x54u), {object}, {}).f0;
    if (word(rt, 0x08D6950Cu) == 1u) {
        const std::uint32_t whole = ctx.fpu_float_to_word_ct<1u>(x);
        const std::uint32_t converted = call_set(rt, ctx, 0x08B491D0u, {0x08D6950Cu, whole, 0u});
        const float scaled = psp_mul(static_cast<float>(signed_of(converted)), std::bit_cast<float>(0x3DCCCCCDu)) + 0.5f;
        (void)call_set(rt, ctx, 0x08ABE514u, {ctx.fpu_float_to_word_ct<1u>(scaled), 0u, 2u, frame.at(0x14u), 0x40u});
    } else {
        const float scaled = psp_mul(x, 10.0f) + 0.5f;
        (void)call_set(rt, ctx, 0x08ABE514u, {ctx.fpu_float_to_word_ct<1u>(scaled), 0u, 1u, frame.at(0x14u), 0x40u});
    }
    number_text_tail(rt, ctx, frame, out, count, list, 0x08B48E78u);
}

// func_08A31244(out, count, list): a text for an id into the string *out. With count > 0, s0 = the virtual slot 0x30 of *list (null: func_08A5BB7C(D_08CAC844 (0x08CAC844)) first), else 0. Six zero bytes at sp + 0x2C are filled by func_08B31C14(D_08D69170 (0x08D69170), sp + 0x2C, 6).
// Answer non-zero and s0 non-zero: the six bytes (big-endian, ORed into the low word and shifted left by 8 each step, so the 64-bit value is the 48-bit number << 8) go to func_08A5DEE8(sp + 0x28, _, lo, hi); *out = that string (func_08A5BAD8(out, sp[0x28]) unless it is out) and it is destroyed. Answer non-zero and s0 zero: func_08B36018(sp + 0x32, 0, 0x12) clears the
// buffer, func_08B3767C(sp + 0x32, 0x12, D_08CAC968 (0x08CAC968), b0, b1, b2, b3, b4, [sp] = b5) formats the bytes, func_08B85EB8(sp + 0x20, sp + 0x32, sp + 0x13) makes a string and func_08A6D2D4(sp + 0x24, sp + 0x20) a copy that is assigned to *out (and destroyed); sp + 0x20 is released (flag sp + 0x11). Answer zero with s0 non-zero: func_08A5DEE8(sp + 0x1C, _, 0, 0)
// assigned to *out; with s0 zero: func_08B85EB8(sp + 0x14, D_08C8E67C (0x08C8E67C), sp + 0x12), func_08A6D2D4(sp + 0x18, sp + 0x14) assigned to *out, sp + 0x14 released (flag sp + 0x10). When the release of the last two paths finds the shared empty cell v0 = D_08D74FD0 (0x08D74FD0), the constant the asm still holds; otherwise the last callee's.
[[maybe_unused]] void func_08A31244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5], list = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    std::uint32_t s0 = 0u;
    if (signed_of(count) > 0) {
        const std::uint32_t object = word(rt, list);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAC844u});
        s0 = call_set(rt, ctx, slot_of(rt, object, 0x30u), {object});
    }
    const std::uint32_t bytes = frame.at(0x2Cu);
    for (std::uint32_t i = 0u; i < 6u; ++i) put8(rt, bytes + i, 0u);
    const std::uint32_t answer = call_set(rt, ctx, 0x08B31C14u, {0x08D69170u, bytes, 6u});
    if (answer != 0u) {
        if (s0 != 0u) {
            std::uint32_t low = 0u, high = 0u;
            for (std::uint32_t i = 0u; i < 6u; ++i) {
                const std::uint32_t mixed = low | byte_u(rt, bytes + i);
                high = (high << 8) | (mixed >> 24);
                low = mixed << 8;
            }
            const std::uint32_t holder = frame.at(0x28u);
            (void)call_set(rt, ctx, 0x08A5DEE8u, {holder, bytes, low, high});
            if (out != holder) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x28u))});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
            return;
        }
        const std::uint32_t text = frame.at(0x32u);
        (void)call_set(rt, ctx, 0x08B36018u, {text, 0u, 0x12u});
        put(rt, frame.at(0u), byte_u(rt, frame.at(0x31u)));
        (void)call_set(rt, ctx, 0x08B3767Cu, {text, 0x12u, 0x08CAC968u, byte_u(rt, frame.at(0x2Cu)), byte_u(rt, frame.at(0x2Du)), byte_u(rt, frame.at(0x2Eu)), byte_u(rt, frame.at(0x2Fu)), byte_u(rt, frame.at(0x30u))});
        const std::uint32_t made = frame.at(0x20u), copy = frame.at(0x24u);
        (void)call_set(rt, ctx, 0x08B85EB8u, {made, text, frame.at(0x13u)});
        (void)call_set(rt, ctx, 0x08A6D2D4u, {copy, made});
        if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x24u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
        ctx.gpr[2] = 0x08D74FD0u;
        release_cell(rt, ctx, word(rt, frame.at(0x20u)), frame.at(0x11u));
        return;
    }
    if (s0 != 0u) {
        const std::uint32_t holder = frame.at(0x1Cu);
        (void)call_set(rt, ctx, 0x08A5DEE8u, {holder, bytes, 0u, 0u});
        if (out != holder) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x1Cu))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    const std::uint32_t made = frame.at(0x14u), copy = frame.at(0x18u);
    (void)call_set(rt, ctx, 0x08B85EB8u, {made, 0x08C8E67Cu, frame.at(0x12u)});
    (void)call_set(rt, ctx, 0x08A6D2D4u, {copy, made});
    if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x18u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
    ctx.gpr[2] = 0x08D74FD0u;
    release_cell(rt, ctx, word(rt, frame.at(0x14u)), frame.at(0x10u));
}

// func_08A43724(a0, name, count, list): nothing happens when count < 2 (signed; v0 is left as it was). Otherwise o = list[1] (null: func_08A5BB7C(D_08CADB98 (0x08CADB98)) first) and k = func_08A76F20(o, func_08A40AD0()). Non-zero: an object sp[0x18] = func_08A40E90(sp + 0x18, name) (its first word s2 null: func_08A5BB7C(D_08CADBFC (0x08CADBFC))), the
// virtual slot 0xC of list[0] (null: D_08CADB98) fills the string sp + 0x14, func_08A40E90(sp + 0x10, list + 4), func_08A42F54(s2, sp + 0x14, sp + 0x10); sp + 0x10 is destroyed, sp + 0x14 released (flag sp + 2) and sp + 0x18 destroyed. Zero: func_08A40E90(sp + 0xC, name), s2 = sp[0xC] (null: D_08CADBFC), the slot 0xC of list[0] fills sp + 8 and of list[1] sp + 4,
// func_08A436A0(s2, sp + 8, sp + 4); sp + 4 is released (flag sp + 1), sp + 8 (flag sp) and sp + 0xC destroyed. v0 = the last callee's (func_08A5B9EC).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(count) < 2) return;`; `if (second == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADB98u});`; `if (known != 0u) {`; `if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADBFCu});`; `if (first == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADB98u});`; `} else {`; `if (next == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADB98u});`
void func_08A43724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t name = ctx.gpr[5], count = ctx.gpr[6], list = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    if (signed_of(count) < 2) return;
    const std::uint32_t second = word(rt, list + 4u);
    if (second == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADB98u});
    const std::uint32_t scene = call_set(rt, ctx, 0x08A40AD0u, {});
    const std::uint32_t known = call_set(rt, ctx, 0x08A76F20u, {second, scene});
    std::uint32_t destroyed;
    if (known != 0u) {
        destroyed = frame.at(0x18u);
        (void)call_set(rt, ctx, 0x08A40E90u, {destroyed, name});
        const std::uint32_t target = word(rt, frame.at(0x18u));
        if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADBFCu});
        const std::uint32_t first = word(rt, list);
        if (first == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADB98u});
        const std::uint32_t text = frame.at(0x14u), extra = frame.at(0x10u);
        (void)call_set(rt, ctx, slot_of(rt, first, 0xCu), {text, first});
        (void)call_set(rt, ctx, 0x08A40E90u, {extra, list + 4u});
        (void)call_set(rt, ctx, 0x08A42F54u, {target, text, extra});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {extra});
        release_cell(rt, ctx, word(rt, text), frame.at(2u));
    } else {
        destroyed = frame.at(0xCu);
        (void)call_set(rt, ctx, 0x08A40E90u, {destroyed, name});
        const std::uint32_t target = word(rt, frame.at(0xCu));
        if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADBFCu});
        const std::uint32_t first = word(rt, list);
        if (first == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADB98u});
        (void)call_set(rt, ctx, slot_of(rt, first, 0xCu), {frame.at(8u), first});
        const std::uint32_t next = word(rt, list + 4u);
        if (next == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADB98u});
        (void)call_set(rt, ctx, slot_of(rt, next, 0xCu), {frame.at(4u), next});
        (void)call_set(rt, ctx, 0x08A436A0u, {target, frame.at(8u), frame.at(4u)});
        release_cell(rt, ctx, word(rt, frame.at(4u)), frame.at(1u));
        release_cell(rt, ctx, word(rt, frame.at(8u)), frame.at(0u));
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {destroyed});
}

// func_08A6E974(out, source): a string *out from the text sp[0x10] that the virtual slot 0xC of *source (null: func_08A5BB7C(D_08CB06B8 (0x08CB06B8)) first) fills. With p = func_08C24530(sp + 0x10, D_08CB07C0 (0x08CB07C0), 0, 1) == -1 (no match): the literal D_08C8EDDC (0x08C8EDDC) becomes a string (func_08B85EB8(sp + 8, _, sp + 3)) and a copy
// (func_08A6D2D4(sp + 0xC, sp + 8)) is assigned to *out (func_08A5BAD8 unless it is out) and destroyed, sp + 8 released (flag sp + 2). Otherwise q = func_08C246E4(sp + 0x10, D_08CB07C0, -1, 1), n = q - p + 1; when the string's length (the word at data - 0xC) is below p func_08B5AEDC(D_08CB07A8 (0x08CB07A8)) is called first (an out-of-range report); the
// substring func_08B8D96C(sp + 0x14, sp + 0x10, p, n) is copied (func_08A6D2D4(sp + 4, sp + 0x14)), assigned to *out and destroyed and sp + 0x14 is released (flag sp + 1). The string sp + 0x10 is released (flag sp) on both paths. v0 = D_08D74FD0's high half (0x08D70000, the `lui v0` the asm leaves) when the releases find the shared empty cell, otherwise the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB06B8u});`; `if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0xCu))});`; `if (word(rt, word(rt, frame.at(0x10u)) - 0xCu) < first) (void)call_set(rt, ctx, 0x08B5AEDCu, {0x08CB07A8u});`; `if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(4u))});`
void func_08A6E974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t object = word(rt, source);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB06B8u});
    const std::uint32_t text = frame.at(0x10u);
    (void)call_set(rt, ctx, slot_of(rt, object, 0xCu), {text, object});
    const std::uint32_t needle = 0x08CB07C0u;
    const std::uint32_t first = call_set(rt, ctx, 0x08C24530u, {text, needle, 0u, 1u});
    if (first == 0xFFFFFFFFu) {
        const std::uint32_t made = frame.at(8u), copy = frame.at(0xCu);
        (void)call_set(rt, ctx, 0x08B85EB8u, {made, 0x08C8EDDCu, frame.at(3u)});
        (void)call_set(rt, ctx, 0x08A6D2D4u, {copy, made});
        if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0xCu))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
        ctx.gpr[2] = 0x08D70000u;
        release_cell(rt, ctx, word(rt, made), frame.at(2u));
    } else {
        const std::uint32_t last = call_set(rt, ctx, 0x08C246E4u, {text, needle, 0xFFFFFFFFu, 1u});
        const std::uint32_t length = last - first + 1u;
        if (word(rt, word(rt, frame.at(0x10u)) - 0xCu) < first) (void)call_set(rt, ctx, 0x08B5AEDCu, {0x08CB07A8u});
        ctx.gpr[7] = length;
        const std::uint32_t part = frame.at(0x14u), copy = frame.at(4u);
        (void)call_set(rt, ctx, 0x08B8D96Cu, {part, text, first, length});
        (void)call_set(rt, ctx, 0x08A6D2D4u, {copy, part});
        if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(4u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
        ctx.gpr[2] = 0x08D70000u;
        release_cell(rt, ctx, word(rt, part), frame.at(1u));
    }
    release_cell(rt, ctx, word(rt, frame.at(0x10u)), frame.at(0u));
}

// func_08A7A3D4(dest, source): copies the map `source` (its tree object source[0x10]: a header whose end node is header + 4, whose first node is header[0xC]; a node: [0x10] key, [0x14] value cell, the key's word at +0x14) into a new tree object func_08A5BC20(0x240, 4) = dest[0x10] ({[4], [8] = 0, [0xC] = [0x10] = header + 4, [0x14] = 0}). Nothing happens (v0 as it was) when source is
// null or source[0x10] is 0. For each source node n in order (func_08B5F100 steps): k = n + 0x10; h = func_08C25AF4(dest tree, k) is the insertion hint; the node is inserted with func_08C25E64(tree, h, key holder) when h is the end node or n[0x10] < h[0x10] (unsigned), the key holder being sp + 0x18 (func_08A6972C(sp + 0x18, k), a pair func_08A76AA0(sp + 4) / func_08A76ABC(sp + 0x1C, sp + 4) around it) when k[4] is non-zero
// and sp + 0x10 (pair sp + 0 / sp + 0x14) when it is zero; the value cell of the node found or inserted (+0x14) is assigned n[0x14] (a copy func_08A77444(sp + 0xC, n[0x14]), null: func_08A5BB7C(D_08CB0E38 (0x08CB0E38)) first) in the first case and an empty holder (func_08A76AA0(sp + 8)) in the second, with func_08A5BAD8 unless it is the source itself. v0 = source header + 4 (the loop test's) when it ends.
[[maybe_unused]] void func_08A7A3D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    if (source == 0u) return;
    ctx.gpr[2] = word(rt, source + 0x10u);
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t tree = call_set(rt, ctx, 0x08A5BC20u, {0x240u, 4u});
    put(rt, tree + 0xCu, tree + 4u);
    put(rt, tree + 0x10u, tree + 4u);
    put(rt, tree + 0x14u, 0u);
    put(rt, tree + 4u, 0u);
    put(rt, tree + 8u, 0u);
    put(rt, dest + 0x10u, tree);
    const std::uint32_t header = word(rt, source + 0x10u);
    std::uint32_t node = word(rt, header + 0xCu);
    ctx.gpr[2] = header + 4u;
    if (node == header + 4u) return;
    for (;;) {
        const std::uint32_t key = node + 0x10u;
        const std::uint32_t destination_tree = word(rt, dest + 0x10u);
        const bool has_value = word(rt, key + 4u) != 0u;
        const std::uint32_t hint = call_set(rt, ctx, 0x08C25AF4u, {destination_tree, key});
        std::uint32_t found = hint;  // s0
        // The rows of the two halves: the default holder, the key copy, the pair, then the value tail.
        const std::uint32_t plain = has_value ? frame.at(4u) : frame.at(0u), copy = has_value ? frame.at(0x18u) : frame.at(0x10u), pair = has_value ? frame.at(0x1Cu) : frame.at(0x14u);
        if (hint == destination_tree + 4u || word(rt, node + 0x10u) < word(rt, hint + 0x10u)) {
            (void)call_set(rt, ctx, 0x08A76AA0u, {plain});
            (void)call_set(rt, ctx, 0x08A6972Cu, {copy, key});
            (void)call_set(rt, ctx, 0x08A76ABCu, {pair, plain});
            found = call_set(rt, ctx, 0x08C25E64u, {destination_tree, hint, copy});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {pair});
            (void)call_set(rt, ctx, 0x08A69D68u, {copy});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {plain});
        }
        found += 0x14u;
        if (has_value) {
            const std::uint32_t value = word(rt, node + 0x14u);
            if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
            const std::uint32_t temporary = frame.at(0xCu);
            (void)call_set(rt, ctx, 0x08A77444u, {temporary, value});
            if (found != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {found, word(rt, temporary)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        } else {
            const std::uint32_t empty = frame.at(8u);
            (void)call_set(rt, ctx, 0x08A76AA0u, {empty});
            if (found != empty) (void)call_set(rt, ctx, 0x08A5BAD8u, {found, word(rt, empty)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {empty});
        }
        node = call_set(rt, ctx, 0x08B5F100u, {node});
        ctx.gpr[2] = word(rt, source + 0x10u) + 4u;
        if (node == ctx.gpr[2]) break;
    }
}

// func_08A82864(this, key, out): a lookup in the map at this + 0x14 (a tree whose end node is this + 0x18). func_08A818A0(sp + 4, this, key) makes a key holder (nothing else happens when sp[4] is 0); n = func_08C287AC(this + 0x14, sp + 4) is the node found. When it is not the end node an array of two holders (sp + 0x14, sp + 0x18) is made (func_08A76AA0 each, sp[0x14] = sp[4] and sp[0x18] = n[0x10]
// assigned with func_08A5BAD8 unless the destination is the source), o = this[0x14][0x2C] (null: func_08A5BB7C(D_08CB15A0 (0x08CB15A0)) first) runs its virtual slot 0xCC with (sp + 8, 2, the array) and p = sp[8] (null: D_08CB15A0) its virtual slot 0x30; the array is destroyed from its end and sp + 8 too. A non-zero answer (or the end node found) inserts: holders sp + 0xC (a copy of sp + 4) and
// sp + 0x10 (a copy of sp[0]) and n' = func_08C28D70(sp[0x20] (this + 0x14), n, sp + 0xC), all three destroyed. Then *out (func_08A5BAD8(n + 0x14, *out) unless it is out) is assigned and sp + 4 destroyed. v0 = the last callee's.
// The delay slot of the jal after the insertion call sets s1 (the destination of the final assignment) to its result.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (word(rt, frame.at(4u)) != 0u) {`; `if (pair != holder) (void)call_set(rt, ctx, 0x08A5BAD8u, {pair, word(rt, frame.at(4u))});`; `if (frame.at(0x18u) != node + 0x10u) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x18u), word(rt, node +...`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});`; `if (insert) {`; `if (target != out) (void)call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, out)});`
void func_08A82864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5], out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t holder = frame.at(4u);
    put(rt, frame.at(0x24u), out);
    (void)call_set(rt, ctx, 0x08A818A0u, {holder, self, key});
    if (word(rt, frame.at(4u)) != 0u) {
        put(rt, frame.at(0x20u), self + 0x14u);
        const std::uint32_t node = call_set(rt, ctx, 0x08C287ACu, {self + 0x14u, holder});
        std::uint32_t destination = node;  // s1
        bool insert = true;
        if (self + 0x18u != node) {
            const std::uint32_t pair = frame.at(0x14u), results = frame.at(8u), first = word(rt, self + 0x14u);
            (void)call_set(rt, ctx, 0x08A76AA0u, {results});
            for (std::uint32_t cell = pair; cell != frame.at(0x1Cu); cell += 4u) (void)call_set(rt, ctx, 0x08A76AA0u, {cell});
            if (pair != holder) (void)call_set(rt, ctx, 0x08A5BAD8u, {pair, word(rt, frame.at(4u))});
            if (frame.at(0x18u) != node + 0x10u) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x18u), word(rt, node + 0x10u)});
            std::uint32_t object = word(rt, first + 0x2Cu);
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
            (void)call_set(rt, ctx, slot_of(rt, object, 0xCCu), {object, results, 2u, pair});
            object = word(rt, frame.at(8u));
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
            const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, object, 0x30u), {object});
            std::uint32_t cell = frame.at(0x1Cu);
            do {
                cell -= 4u;
                (void)call_set(rt, ctx, 0x08A5B9ECu, {cell});
            } while (cell != pair);
            (void)call_set(rt, ctx, 0x08A5B9ECu, {results});
            insert = answer != 0u;
        }
        if (insert) {
            const std::uint32_t copy = frame.at(0xCu), second = frame.at(0x10u);
            (void)call_set(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
            (void)call_set(rt, ctx, 0x08A76ABCu, {copy, holder});
            (void)call_set(rt, ctx, 0x08A76ABCu, {second, frame.at(0u)});
            const std::uint32_t made = call_set(rt, ctx, 0x08C28D70u, {word(rt, frame.at(0x20u)), node, copy});
            destination = made;  // the delay slot of the next jal
            (void)call_set(rt, ctx, 0x08A5B9ECu, {second});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
        const std::uint32_t target = destination + 0x14u;
        if (target != out) (void)call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, out)});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08AF9010(this): nothing else happens (v0 = the answer) when func_08AF7FEC(this) answers non-zero. Otherwise func_08AFC9F4(this[0x4C]) when that is non-null and func_08AF7B0C(this[0x48], D_08CE0A00's word (0x08CE0A00)) when this[0x48] is non-null; then four loops over tables: this[0x2C] (halfword) entries of 0x28 bytes at this[0x88]: func_08AFCE8C(entry, this[0x4C]);
// this[0x1C] entries of 0x18 bytes at this[0x44]: func_08AFCE54(entry, this[0x48], this[0x4C], D_08CE0A00's word); this[0x18] entries of 0x30 bytes at this[0x40]: func_08AFBD90(entry, this[0x4C]); with this[0x38] non-null, this[0x14] entries of 0x30 bytes at this[0x38]: func_08AF89B4(entry, this[0x40] + entry[2] * 0x30, this[0x94]). Then, when this[0xA8] is non-null, this[0x14]
// entries of 32 bytes: func_08B537B4(this[0xA8] + i * 32); func_08AF8EB4(this, 0) when this[0x84] is non-zero; with this[0x9C] non-zero: func_08B4E648(sp + 0x40), s1 = this[0x8C] (halfword), func_08B4E734(sp, sp + 0x40), func_08AF5BF0(this, s1, sp). The halfword this[0xE] gets bit 15 set and v0 = that halfword | 0xFFFF8000.
// Each loop runs its body at least once when the count (a halfword, so never negative) is non-zero (blezl / blez on the first test); the counts are re-read after every round.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (early != 0u) return;`; `if (word(rt, self + 0x4Cu) != 0u) (void)call_set(rt, ctx, 0x08AFC9F4u, {word(rt, self + 0x4Cu)});`; `if (word(rt, self + 0x48u) != 0u) (void)call_set(rt, ctx, 0x08AF7B0Cu, {word(rt, self + 0x48u), word(rt, se...`; `if (half_u(rt, self + 0x2Cu) != 0u) {`; `do {`; `} while (signed_of(index) < signed_of(half_u(rt, self + 0x2Cu)));`; `if (half_u(rt, self + 0x1Cu) != 0u) {`; `} while (signed_of(index) < signed_of(half_u(rt, self + 0x1Cu)));`; `if (half_u(rt, self + 0x18u) != 0u) {`; `} while (signed_of(index) < signed_of(half_u(rt, self + 0x18u)));`; `if (word(rt, self + 0x38u) != 0u && half_u(rt, self + 0x14u) != 0u) {`; `} while (signed_of(index) < signed_of(half_u(rt, self + 0x14u)));`; `if (word(rt, self + 0xA8u) != 0u && half_u(rt, self + 0x14u) != 0u) {`; `if (word(rt, self + 0x84u) != 0u) (void)call_set(rt, ctx, 0x08AF8EB4u, {self, 0u});`; `if (word(rt, self + 0x9Cu) != 0u) {`
void func_08AF9010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0xA0u);
    const std::uint32_t seed = 0x08CE0A00u;
    const std::uint32_t early = call_set(rt, ctx, 0x08AF7FECu, {self});
    if (early != 0u) return;
    if (word(rt, self + 0x4Cu) != 0u) (void)call_set(rt, ctx, 0x08AFC9F4u, {word(rt, self + 0x4Cu)});
    if (word(rt, self + 0x48u) != 0u) (void)call_set(rt, ctx, 0x08AF7B0Cu, {word(rt, self + 0x48u), word(rt, seed)});
    if (half_u(rt, self + 0x2Cu) != 0u) {
        std::uint32_t index = 0u, offset = 0u;
        do {
            const std::uint32_t entry = word(rt, self + 0x88u) + offset;
            ++index;
            (void)call_set(rt, ctx, 0x08AFCE8Cu, {entry, word(rt, self + 0x4Cu)});
            offset += 0x28u;
        } while (signed_of(index) < signed_of(half_u(rt, self + 0x2Cu)));
    }
    if (half_u(rt, self + 0x1Cu) != 0u) {
        std::uint32_t index = 0u, offset = 0u;
        do {
            const std::uint32_t entry = word(rt, self + 0x44u) + offset;
            ++index;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08AFCE54u, {entry, word(rt, self + 0x48u), word(rt, self + 0x4Cu), word(rt, seed)});
            offset += 0x18u;
        } while (signed_of(index) < signed_of(half_u(rt, self + 0x1Cu)));
    }
    if (half_u(rt, self + 0x18u) != 0u) {
        std::uint32_t index = 0u, offset = 0u;
        do {
            const std::uint32_t entry = word(rt, self + 0x40u) + offset;
            ++index;
            (void)call_set(rt, ctx, 0x08AFBD90u, {entry, word(rt, self + 0x4Cu)});
            offset += 0x30u;
        } while (signed_of(index) < signed_of(half_u(rt, self + 0x18u)));
    }
    if (word(rt, self + 0x38u) != 0u && half_u(rt, self + 0x14u) != 0u) {
        std::uint32_t index = 0u, offset = 0u;
        do {
            const std::uint32_t entry = word(rt, self + 0x38u) + offset;
            const std::uint32_t source = half_u(rt, entry + 2u) * 0x30u;
            (void)call_set(rt, ctx, 0x08AF89B4u, {entry, word(rt, self + 0x40u) + source, word(rt, self + 0x94u)});
            ++index;
            offset += 0x30u;
        } while (signed_of(index) < signed_of(half_u(rt, self + 0x14u)));
    }
    if (word(rt, self + 0xA8u) != 0u && half_u(rt, self + 0x14u) != 0u) {
        std::uint32_t index = 0u;
        do {
            (void)call_set(rt, ctx, 0x08B537B4u, {word(rt, self + 0xA8u) + (index << 5)});
            ++index;
        } while (signed_of(index) < signed_of(half_u(rt, self + 0x14u)));
    }
    if (word(rt, self + 0x84u) != 0u) (void)call_set(rt, ctx, 0x08AF8EB4u, {self, 0u});
    if (word(rt, self + 0x9Cu) != 0u) {
        const std::uint32_t scratch = frame.at(0x40u);
        (void)call_set(rt, ctx, 0x08B4E648u, {scratch});
        const std::uint32_t kind = half_u(rt, self + 0x8Cu);
        (void)call_set(rt, ctx, 0x08B4E734u, {frame.at(0u), scratch});
        (void)call_set(rt, ctx, 0x08AF5BF0u, {self, kind, frame.at(0u)});
    }
    const std::uint32_t flags = half_u(rt, self + 0xEu) | 0xFFFF8000u;
    rt.memory().store16(self + 0xEu, static_cast<std::uint16_t>(flags));
    ctx.gpr[2] = flags;
}

// func_08B29070(lo, hi): a class number 0..0x12 for the 64-bit value hi:lo (a0 = lo, a1 = hi) against two tables of thresholds. With hi == 0: lo below the word at D_08C93A00 + 0x14 is 0 / 1 for lo below / above 10 when lo < 100, then 2 (< 1000), 3 (< 10000) or 4;
// otherwise (lo at least that word) the thresholds at +0x1C, +0x18, +0x24, +0x20 decide 5 / 6 (lo below the word at +0x1C: below +0x18 gives 5, else 6) or 7 / 8 (below +0x24: below +0x20 gives 7, else 8) or 9. With hi != 0 a tree of 64-bit comparisons "(hi:lo) < (T[off + 4]:T[off])" over the pairs of D_08C93940 (T = 0x08C93940; the offsets are the low
// words: 0x70, 0x60, 0x58, 0x50, 0x68, 0x90, 0x80, 0x78, 0x88, each high word 4 above) picks the class: 0x74 -> (0x64 -> (0x5C -> (0x54 -> 9 / 0xA) / 0xB) / (0x6C -> 0xC / 0xD)) / (0x94 -> (0x84 -> (0x7C -> 0xE / 0xF) / (0x8C -> 0x10 / 0x11)) / 0x12).
// v0 = a2 = the class. Every compare is unsigned; hi is compared first, lo only on equal high words (the asm's sltu / bne pairs).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `bool below_pair(psprecomp::Runtime &rt, std::uint32_t hi, std::uint32_t lo, std::uint32_t offset) {`; `if (hi < limit_hi) return true;`; `if (hi != limit_hi) return false;`; `return lo < word(rt, 0x08C93940u + offset);`
bool below_pair(psprecomp::Runtime &rt, std::uint32_t hi, std::uint32_t lo, std::uint32_t offset) {
    const std::uint32_t limit_hi = word(rt, 0x08C93940u + offset + 4u);
    if (hi < limit_hi) return true;
    if (hi != limit_hi) return false;
    return lo < word(rt, 0x08C93940u + offset);
}
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (hi == 0u) {`; `if (lo < word(rt, table + 0x14u)) {`; `else if (lo >= 10000u) result = 4u;`; `} else if (lo < word(rt, table + 0x1Cu)) {`; `result = lo >= word(rt, table + 0x18u) ? 6u : 5u;`; `} else if (lo >= word(rt, table + 0x24u)) {`; `} else {`; `result = lo >= word(rt, table + 0x20u) ? 8u : 7u;`; `} else if (below_pair(rt, hi, lo, 0x70u)) {`; `if (below_pair(rt, hi, lo, 0x60u)) {`; `if (below_pair(rt, hi, lo, 0x58u)) result = below_pair(rt, hi, lo, 0x50u) ? 9u : 0xAu;`; `else result = 0xBu;`; `result = below_pair(rt, hi, lo, 0x68u) ? 0xCu : 0xDu;`; `} else if (below_pair(rt, hi, lo, 0x90u)) {`; `if (below_pair(rt, hi, lo, 0x80u)) result = below_pair(rt, hi, lo, 0x78u) ? 0xEu : 0xFu;`; `else result = below_pair(rt, hi, lo, 0x88u) ? 0x10u : 0x11u;`
void func_08B29070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t lo = ctx.gpr[4], hi = ctx.gpr[5];
    std::uint32_t result;
    const std::uint32_t table = 0x08C93A00u;
    if (hi == 0u) {
        if (lo < word(rt, table + 0x14u)) {
            if (lo < 100u) result = lo >= 10u ? 1u : 0u;
            else if (lo >= 10000u) result = 4u;
            else result = lo >= 1000u ? 3u : 2u;
        } else if (lo < word(rt, table + 0x1Cu)) {
            result = lo >= word(rt, table + 0x18u) ? 6u : 5u;
        } else if (lo >= word(rt, table + 0x24u)) {
            result = 9u;
        } else {
            result = lo >= word(rt, table + 0x20u) ? 8u : 7u;
        }
    } else if (below_pair(rt, hi, lo, 0x70u)) {
        if (below_pair(rt, hi, lo, 0x60u)) {
            if (below_pair(rt, hi, lo, 0x58u)) result = below_pair(rt, hi, lo, 0x50u) ? 9u : 0xAu;
            else result = 0xBu;
        } else {
            result = below_pair(rt, hi, lo, 0x68u) ? 0xCu : 0xDu;
        }
    } else if (below_pair(rt, hi, lo, 0x90u)) {
        if (below_pair(rt, hi, lo, 0x80u)) result = below_pair(rt, hi, lo, 0x78u) ? 0xEu : 0xFu;
        else result = below_pair(rt, hi, lo, 0x88u) ? 0x10u : 0x11u;
    } else {
        result = 0x12u;
    }
    ctx.gpr[2] = result;
    ctx.gpr[6] = result;
}

// func_08BD4270(this, arg): s2 = this + 0xC; func_08909B00(s2, arg); s6 = func_08BD2DC4(arg, (signed byte) this[0x24]). With this[0x20] == 1: p = func_08AF296C(arg) (s3), func_08BD3210(p, p[0], p[1]), fp = func_08BD2E5C(sp + 0x10, arg, 0); k = func_089099A0(s2) (0: skip to the end); k == 1: x = func_08906304(fp, this[4][0][0x30] + func_089099FC(s2, 0) * 48 + 4) and
// func_08AF10C4(p, this) with f12 = x; otherwise func_08909A28(s2) / func_08909A38(s2) give (b, c) and func_08BD344C(sp + 0x50, b, c, sp + 0x20) with the record {sp[0x20] = p, this, s6, this[4][0], fp} (sp[0x24..0x30]); its five result words are copied to sp + 0x34..0x44. The end: func_08AF1124(p), func_08AF29C4(arg) and func_08AF1068(p, arg, (result ^ 1) & 0xFF).
// With this[0x20] != 1: p = this[4]; func_089099A0(s2) == 0 ends; 1: func_08AFA170(p[0], func_089099FC(s2, 0), p, s6); otherwise (func_08909A28(s2), func_08909A38(s2)) = (b, c), sp[0] = p, sp[4] = s6, func_08AF8904(p[0], p), func_08BD2F8C(sp + 8, b, c, sp) and func_08AF8434(sp[8][0]), func_08AF8434(sp[0][0]). v0 = the last callee's.
// The delay slots: s3 (= the answer of func_08AF296C) before func_08BD3210's result is dropped, s0 = this[4] before the call of func_089099A0 (k), s7 = s0[0] on both paths of the k == 1 test, s1 = func_08909A28's answer before func_08909A38.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (word(rt, self + 0x20u) == 1u) {`; `if (kind != 0u) {`; `if (kind == 1u) {`; `} else {`; `for (std::uint32_t i = 0u; i < 5u; ++i) put(rt, frame.at(0x34u + 4u * i), word(rt, frame.at(0x50u + 4u * i)));`; `if (kind == 0u) return;`; `if (call_set(rt, ctx, 0x089099A0u, {sub}) == 1u) {`
void func_08BD4270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], arg = ctx.gpr[5];
    GuestFrame frame(ctx, 0xA0u);
    const std::uint32_t sub = self + 0xCu;
    (void)call_set(rt, ctx, 0x08909B00u, {sub, arg});
    const std::uint32_t chosen = call_set(rt, ctx, 0x08BD2DC4u, {arg, sext8(byte_u(rt, self + 0x24u))});
    if (word(rt, self + 0x20u) == 1u) {
        const std::uint32_t pointer = call_set(rt, ctx, 0x08AF296Cu, {arg});
        (void)call_set(rt, ctx, 0x08BD3210u, {pointer, word(rt, pointer), word(rt, pointer + 4u)});
        const std::uint32_t built = call_set(rt, ctx, 0x08BD2E5Cu, {frame.at(0x10u), arg, 0u});
        const std::uint32_t inner = word(rt, self + 4u);
        const std::uint32_t kind = call_set(rt, ctx, 0x089099A0u, {sub});
        if (kind != 0u) {
            const std::uint32_t first = word(rt, inner);  // s7: loaded in the delay slot of the bne
            if (kind == 1u) {
                const std::uint32_t index = call_set(rt, ctx, 0x089099FCu, {sub, 0u});
                const std::uint32_t table = word(rt, first + 0x30u) + index * 48u + 4u;
                const GuestResult result = call_keep(rt, ctx, 0x08906304u, {built, table}, {});
                ctx.fpr[12] = result.f0;
                (void)call_set(rt, ctx, 0x08AF10C4u, {pointer, self});
            } else {
                const std::uint32_t b = call_set(rt, ctx, 0x08909A28u, {sub});
                const std::uint32_t c = call_set(rt, ctx, 0x08909A38u, {sub});
                put(rt, frame.at(0x24u), self);
                put(rt, frame.at(0x28u), chosen);
                put(rt, frame.at(0x2Cu), first);
                put(rt, frame.at(0x30u), built);
                put(rt, frame.at(0x20u), pointer);  // the delay slot of the jal
                (void)call_set(rt, ctx, 0x08BD344Cu, {frame.at(0x50u), b, c, frame.at(0x20u)});
                for (std::uint32_t i = 0u; i < 5u; ++i) put(rt, frame.at(0x34u + 4u * i), word(rt, frame.at(0x50u + 4u * i)));
            }
        }
        (void)call_set(rt, ctx, 0x08AF1124u, {pointer});
        const std::uint32_t flag = call_set(rt, ctx, 0x08AF29C4u, {arg});
        (void)call_set(rt, ctx, 0x08AF1068u, {pointer, arg, (flag ^ 1u) & 0xFFu});
        return;
    }
    const std::uint32_t inner = word(rt, self + 4u);
    const std::uint32_t kind = call_set(rt, ctx, 0x089099A0u, {sub});
    if (kind == 0u) return;
    if (call_set(rt, ctx, 0x089099A0u, {sub}) == 1u) {
        const std::uint32_t first = word(rt, inner);
        const std::uint32_t index = call_set(rt, ctx, 0x089099FCu, {sub, 0u});
        (void)call_set(rt, ctx, 0x08AFA170u, {first, index, inner, chosen});
        return;
    }
    const std::uint32_t b = call_set(rt, ctx, 0x08909A28u, {sub});
    const std::uint32_t c = call_set(rt, ctx, 0x08909A38u, {sub});
    put(rt, frame.at(4u), chosen);
    put(rt, frame.at(0u), inner);
    (void)call_set(rt, ctx, 0x08AF8904u, {word(rt, inner), inner});
    (void)call_set(rt, ctx, 0x08BD2F8Cu, {frame.at(8u), b, c, frame.at(0u)});
    (void)call_set(rt, ctx, 0x08AF8434u, {word(rt, word(rt, frame.at(8u)))});
    (void)call_set(rt, ctx, 0x08AF8434u, {word(rt, word(rt, frame.at(0u)))});
}

// func_08BEFA18(this, stream): writes the vector of words this[0xC] ({begin, end}, 4-byte elements) through the stream's virtual slot 8, after fitting it to the count n = this[8]. With n > 0 the vector is cut to n elements (func_08BEF1A8(V, begin + n * 4, end) when it holds more than n) or grown with zeros (func_08BEF324(V, end, n - size, sp + 8) with sp[8] = 0);
// then for each of the n elements the record at sp + 0xC ({vtable D_08CCD9F0 (0x08CCD9F0), sp[0x10] = D_08D74FD0 + 0xC (the shared empty cell), sp[0x14] = -1, sp[0x18] = the element's address}) goes to slot 8 (stream, sp + 0xC), the record's vtable word is set to D_08CCD990 (0x08CCD990) and its cell released (release_cell, flag sp + 1).
// With n <= 0 the vector is fitted to one element (func_08BEF1A8(V, begin + 4, end) when it holds at least two, else func_08BEF324(V, end, 1 - size, sp + 4) with sp[4] = 0) and func_08988BB8(sp + 0xC, stream) writes its first element in the same record shape; the cell is released (flag sp). v0 = the last callee's; when a release finds the shared empty cell it is D_08D74FD0 (loop case, 0x08D74FD0) or D_08CCD990 (single case, 0x08CCD990), the constants the asm holds in v0.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(count) > 0) {`; `if (count < size) {`; `put(rt, frame.at(4u), 0u);`; `if (size >= 2u) (void)call_set(rt, ctx, 0x08BEF1A8u, {vector, begin + 4u, end});`; `else (void)call_set(rt, ctx, 0x08BEF324u, {vector, end, 1u - size, frame.at(4u)});`
void func_08BEFA18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], stream = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t count = word(rt, self + 8u), record = frame.at(0xCu);
    const std::uint32_t vector = word(rt, self + 0xCu);
    if (signed_of(count) > 0) {
        put(rt, frame.at(8u), 0u);
        const std::uint32_t begin = word(rt, vector), end = word(rt, vector + 4u);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
        if (count < size) {
            (void)call_set(rt, ctx, 0x08BEF1A8u, {vector, begin + (count << 2), end});
        } else {
            (void)call_set(rt, ctx, 0x08BEF324u, {vector, end, count - size, frame.at(8u)});
        }
        std::uint32_t index = 0u;
        do {
            const std::uint32_t elements = word(rt, word(rt, self + 0xCu));
            put(rt, frame.at(0x10u), 0x08D74FD0u + 0xCu);
            put(rt, frame.at(0x18u), elements + (index << 2));
            put(rt, frame.at(0x14u), 0xFFFFFFFFu);
            put(rt, record, 0x08CCD9F0u);
            const std::uint32_t slot = slot_of(rt, stream, 8u);
            ++index;  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot, {stream, record});
            const std::uint32_t data = word(rt, frame.at(0x10u));
            put(rt, record, 0x08CCD990u);
            ctx.gpr[2] = 0x08D74FD0u;
            release_cell(rt, ctx, data, frame.at(1u));
        } while (index != count);
        return;
    }
    put(rt, frame.at(4u), 0u);
    const std::uint32_t begin = word(rt, vector), end = word(rt, vector + 4u);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
    if (size >= 2u) (void)call_set(rt, ctx, 0x08BEF1A8u, {vector, begin + 4u, end});
    else (void)call_set(rt, ctx, 0x08BEF324u, {vector, end, 1u - size, frame.at(4u)});
    const std::uint32_t first = word(rt, word(rt, self + 0xCu));
    put(rt, frame.at(0x10u), 0x08D74FD0u + 0xCu);
    put(rt, record, 0x08CCD9F0u);
    put(rt, frame.at(0x14u), 0xFFFFFFFFu);
    put(rt, frame.at(0x18u), first);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08988BB8u, {record, stream});
    const std::uint32_t data = word(rt, frame.at(0x10u));
    put(rt, record, 0x08CCD990u);  // the delay slot of the beq
    ctx.gpr[2] = 0x08CCD990u;
    release_cell(rt, ctx, data, frame.at(0u));
}

// func_08C13A68(this, stream): the same writer for 16-byte elements (this[0xC] = {begin, end} of the vector V, a default element built by func_089FA09C at sp + 0x14 (n > 0) or sp + 4 and destroyed by func_089F9B74 once the vector is fitted): fitting uses func_08C122C8(V, first, last) to cut and func_08C12BDC(V, end, count, default) to grow; the record at sp + 0x24
// has the vtable D_08CCFCB0 (0x08CCFCB0), sp[0x28] = the empty cell + 0xC, sp[0x2C] = -1, sp[0x30] = the element's address (index * 16 + begin), is written through the stream's slot 8 per element (n > 0) or, for one element, by func_089FB5B4(sp + 0x24, stream), its vtable then set to D_08CCD990 (0x08CCD990) and the cell released (flag sp + 1 in the loop, sp for the single case).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(count) > 0) {`; `if (count < size) (void)call_set(rt, ctx, 0x08C122C8u, {vector, begin + (count << 4), end});`; `const std::uint32_t holder = frame.at(4u);`; `if (size >= 2u) (void)call_set(rt, ctx, 0x08C122C8u, {vector, begin + 0x10u, end});`; `else (void)call_set(rt, ctx, 0x08C12BDCu, {vector, end, 1u - size, holder});`
void func_08C13A68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], stream = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    const std::uint32_t count = word(rt, self + 8u), record = frame.at(0x24u);
    std::uint32_t vector = word(rt, self + 0xCu);
    if (signed_of(count) > 0) {
        const std::uint32_t holder = frame.at(0x14u);
        (void)call_set(rt, ctx, 0x089FA09Cu, {holder});
        const std::uint32_t begin = word(rt, vector), end = word(rt, vector + 4u);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 4);
        if (count < size) (void)call_set(rt, ctx, 0x08C122C8u, {vector, begin + (count << 4), end});
        else (void)call_set(rt, ctx, 0x08C12BDCu, {vector, end, count - size, holder});
        (void)call_set(rt, ctx, 0x089F9B74u, {holder});
        std::uint32_t index = 0u;
        do {
            const std::uint32_t elements = word(rt, word(rt, self + 0xCu));
            put(rt, frame.at(0x28u), 0x08D74FD0u + 0xCu);
            put(rt, frame.at(0x30u), elements + (index << 4));
            put(rt, frame.at(0x2Cu), 0xFFFFFFFFu);
            put(rt, record, 0x08CCFCB0u);
            const std::uint32_t slot = slot_of(rt, stream, 8u);
            ++index;  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot, {stream, record});
            const std::uint32_t data = word(rt, frame.at(0x28u));
            put(rt, record, 0x08CCD990u);
            ctx.gpr[2] = 0x08D74FD0u;
            release_cell(rt, ctx, data, frame.at(1u));
        } while (index != count);
        return;
    }
    const std::uint32_t holder = frame.at(4u);
    (void)call_set(rt, ctx, 0x089FA09Cu, {holder});
    const std::uint32_t begin = word(rt, vector), end = word(rt, vector + 4u);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 4);
    if (size >= 2u) (void)call_set(rt, ctx, 0x08C122C8u, {vector, begin + 0x10u, end});
    else (void)call_set(rt, ctx, 0x08C12BDCu, {vector, end, 1u - size, holder});
    (void)call_set(rt, ctx, 0x089F9B74u, {holder});
    vector = word(rt, self + 0xCu);
    const std::uint32_t first = word(rt, vector);
    put(rt, frame.at(0x28u), 0x08D74FD0u + 0xCu);
    put(rt, record, 0x08CCFCB0u);
    put(rt, frame.at(0x2Cu), 0xFFFFFFFFu);
    put(rt, frame.at(0x30u), first);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x089FB5B4u, {record, stream});
    const std::uint32_t data = word(rt, frame.at(0x28u));
    put(rt, record, 0x08CCD990u);  // the delay slot of the beq
    ctx.gpr[2] = 0x08CCD990u;
    release_cell(rt, ctx, data, frame.at(0u));
}

// func_08BF56E4(this, stream): the writer func_08BEFA18 / func_08C13A68 for 40-byte elements (this[0xC] = {begin, end} of the vector V; the size is ((end - begin) >> 3) * 0xCCCCCCCD, the inverse of 5): the default element is built at sp + 0x3C (n > 0) or sp + 0x14 by func_089D76A8 with the vtable word D_08CCE6E0 (0x08CCE6E0) and two zero words at +0x20 / +0x24, and
// destroyed by func_089D6684 (the vtable word stored again in its delay slot) once the vector is fitted: func_08BE5DC0(V, first, last) cuts, func_08BE6DB8(V, end, count, default) grows. n = this[8] > 0: per element the record at sp + 4 ({vtable D_08CCE718 (0x08CCE718), sp[8] = the empty cell + 0xC, sp[0xC] = -1, sp[0x10] = begin + i * 0x28}) goes to the stream's slot 8, then its vtable becomes D_08CCD990
// (0x08CCD990) and its cell is released (flag sp + 1). n <= 0: the vector is fitted to one element and func_089A16CC(sp + 4, stream) writes the first (release flag sp). v0 = the last callee's, D_08D74FD0 (0x08D74FD0) after a skipped release in the loop and D_08CCD990 after one in the single case.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `const std::uint32_t holder = signed_of(count) > 0 ? frame.at(0x3Cu) : frame.at(0x14u);`; `if (signed_of(count) > 0) {`; `if (count < size) (void)call_set(rt, ctx, 0x08BE5DC0u, {vector, begin + (count << 3) + (count << 5), end});`; `if (size >= 2u) (void)call_set(rt, ctx, 0x08BE5DC0u, {vector, begin + 0x28u, end});`; `else (void)call_set(rt, ctx, 0x08BE6DB8u, {vector, end, 1u - size, holder});`
void func_08BF56E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], stream = ctx.gpr[5];
    GuestFrame frame(ctx, 0xA0u);
    const std::uint32_t count = word(rt, self + 8u), record = frame.at(4u);
    const std::uint32_t vector = word(rt, self + 0xCu);
    const std::uint32_t kDefaultVtable = 0x08CCE6E0u;
    const std::uint32_t holder = signed_of(count) > 0 ? frame.at(0x3Cu) : frame.at(0x14u);
    (void)call_set(rt, ctx, 0x089D76A8u, {holder});
    put(rt, holder, kDefaultVtable);
    put(rt, holder + 0x20u, 0u);
    put(rt, holder + 0x24u, 0u);
    const std::uint32_t begin = word(rt, vector), end = word(rt, vector + 4u);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 3) * 0xCCCCCCCDu;
    if (signed_of(count) > 0) {
        if (count < size) (void)call_set(rt, ctx, 0x08BE5DC0u, {vector, begin + (count << 3) + (count << 5), end});
        else (void)call_set(rt, ctx, 0x08BE6DB8u, {vector, end, count - size, holder});
        put(rt, holder, kDefaultVtable);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x089D6684u, {holder});
        std::uint32_t index = 0u, offset = 0u;
        do {
            const std::uint32_t elements = word(rt, word(rt, self + 0xCu));
            put(rt, frame.at(8u), 0x08D74FD0u + 0xCu);
            put(rt, frame.at(0x10u), elements + offset);
            put(rt, frame.at(0xCu), 0xFFFFFFFFu);
            put(rt, record, 0x08CCE718u);
            const std::uint32_t slot = slot_of(rt, stream, 8u);
            ++index;
            offset += 0x28u;  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot, {stream, record});
            const std::uint32_t data = word(rt, frame.at(8u));
            put(rt, record, 0x08CCD990u);
            ctx.gpr[2] = 0x08D74FD0u;
            release_cell(rt, ctx, data, frame.at(1u));
        } while (index != count);
        return;
    }
    if (size >= 2u) (void)call_set(rt, ctx, 0x08BE5DC0u, {vector, begin + 0x28u, end});
    else (void)call_set(rt, ctx, 0x08BE6DB8u, {vector, end, 1u - size, holder});
    put(rt, holder, kDefaultVtable);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x089D6684u, {holder});
    const std::uint32_t first = word(rt, word(rt, self + 0xCu));
    put(rt, frame.at(8u), 0x08D74FD0u + 0xCu);
    put(rt, record, 0x08CCE718u);
    put(rt, frame.at(0xCu), 0xFFFFFFFFu);
    put(rt, frame.at(0x10u), first);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x089A16CCu, {record, stream});
    const std::uint32_t data = word(rt, frame.at(8u));
    put(rt, record, 0x08CCD990u);  // the delay slot of the beq
    ctx.gpr[2] = 0x08CCD990u;
    release_cell(rt, ctx, data, frame.at(0u));
}

// hinted_insert(tree, hint, value): the insertion of a string-keyed node with a position hint (a red-black tree: header = tree + 4 is the end node, tree[0xC] the leftmost, tree[0x10] the rightmost, tree[0x14] the size; a node: [0xC] right child, [0x10] its key's string data (the length is the word at data - 0xC); `value` points at the key's data pointer). Returns the node.
// Keys are ordered by func_08B36170(a, b, min length), whose answer, when zero, is replaced by the difference of the lengths. `find_position(tree, value)` is the member's own unique-insert search (the columns of the table rows: find_position, insert_node) and `insert_node(tree, left, parent, value)` its node insertion;
// func_08B5F174 / func_08B5F100 are the previous / next node. hint == end: an empty tree or a key not above the rightmost node searches (find_position), otherwise it is appended (insert_node(tree, 0, rightmost, value)). Otherwise key < hint key: the leftmost hint inserts before it (left = parent = hint); else with p = previous node: key not above p searches, p having a right child inserts before the hint (left = parent = hint) and otherwise below p (insert_node(tree, 0, p, value));
// key >= hint key: equal keys return the hint; with the hint the rightmost node the key is appended below it; else with n = next node: key not below n searches, a hint with a right child inserts before n (left = parent = n), otherwise below the hint (insert_node(tree, 0, hint, value)). The pair {node, inserted} that find_position returns is spilled to dead stack (the harness ignores it); v0 = the node.
inline std::int32_t compare_keys(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second) {
    const std::uint32_t first_length = word(rt, first - 0xCu), second_length = word(rt, second - 0xCu);
    const std::uint32_t shorter = first_length < second_length ? first_length : second_length;
    const std::uint32_t answer = call_set(rt, ctx, 0x08B36170u, {first, second, shorter});
    return signed_of(answer != 0u ? answer : first_length - second_length);
}
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `result = compare_keys(rt, ctx, word(rt, rightmost + 0x10u), key) < 0 ? insert(0u, rightmost) : search();`; `if (compare_keys(rt, ctx, key, word(rt, hint + 0x10u)) < 0) {`; `if (compare_keys(rt, ctx, word(rt, previous + 0x10u), key) >= 0) result = search();`; `} else if (compare_keys(rt, ctx, word(rt, hint + 0x10u), key) >= 0) {`; `} else if (word(rt, tree + 0x10u) == hint) {`; `} else {`; `if (compare_keys(rt, ctx, key, word(rt, next + 0x10u)) >= 0) result = search();`; `else if (word(rt, hint + 0xCu) != 0u) result = insert(next, next);`; `else result = insert(0u, hint);`
inline void hinted_insert(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t find_position, std::uint32_t insert_node) {
    const std::uint32_t tree = ctx.gpr[4], hint = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    const auto search = [&]() { return call_set(rt, ctx, find_position, {tree, value}); };
    const auto insert = [&](std::uint32_t left, std::uint32_t parent) { return call_set(rt, ctx, insert_node, {tree, left, parent, value}); };
    std::uint32_t result;
    if (hint == tree + 4u) {
        if (word(rt, tree + 0x14u) == 0u) {
            result = search();
        } else {
            const std::uint32_t rightmost = word(rt, tree + 0x10u), key = word(rt, value);
            result = compare_keys(rt, ctx, word(rt, rightmost + 0x10u), key) < 0 ? insert(0u, rightmost) : search();
        }
    } else {
        const std::uint32_t key = word(rt, value);
        if (compare_keys(rt, ctx, key, word(rt, hint + 0x10u)) < 0) {
            if (hint == word(rt, tree + 0xCu)) {
                result = insert(hint, hint);
            } else {
                const std::uint32_t previous = call_set(rt, ctx, 0x08B5F174u, {hint});
                if (compare_keys(rt, ctx, word(rt, previous + 0x10u), key) >= 0) result = search();
                else if (word(rt, previous + 0xCu) != 0u) result = insert(hint, hint);
                else result = insert(0u, previous);
            }
        } else if (compare_keys(rt, ctx, word(rt, hint + 0x10u), key) >= 0) {
            result = hint;
        } else if (word(rt, tree + 0x10u) == hint) {
            result = insert(0u, hint);
        } else {
            const std::uint32_t next = call_set(rt, ctx, 0x08B5F100u, {hint});
            if (compare_keys(rt, ctx, key, word(rt, next + 0x10u)) >= 0) result = search();
            else if (word(rt, hint + 0xCu) != 0u) result = insert(next, next);
            else result = insert(0u, hint);
        }
    }
    ctx.gpr[2] = result;
}
void func_08BF70B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hinted_insert(rt, ctx, 0x08BF6F58u, 0x08BF6E48u); }
void func_08BF76A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hinted_insert(rt, ctx, 0x08BF754Cu, 0x08BF7448u); }
void func_08C0195C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hinted_insert(rt, ctx, 0x08C01800u, 0x08C016FCu); }
void func_08C2FED8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hinted_insert(rt, ctx, 0x08C2FD7Cu, 0x08C2FC7Cu); }
void func_08C33C44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hinted_insert(rt, ctx, 0x08C33AE8u, 0x08C339E8u); }

// func_08C23660(this, host): s1 = this[0xC] (null: func_08A5BB7C(D_08CB02F0 (0x08CB02F0)) first); a new 0x34-byte block func_08A5BC20(0x34, 4) is made (func_08A90D6C(block, s1)) and handed to func_08A90470(sp + 4, sp) (sp[0] = block). With o = host[0x34][0x30] (null: func_08A5BB7C(D_08CB02A4 (0x08CB02A4))) and its word o[0x34]: at least 8 copies the vector of 3-word entries sp[4][0x24] ({begin, end}) into
// the array func_08A6159C(host, count) returns (count = (end - begin) / 12 as ((end - begin) >> 2) * 0xAAAAAAAB), entry i's word at begin + 12 * i + 8 getting func_08A5BAD8(dest, *source) unless it is the source (the source advances 4 per entry); at least 7 does the same for the vector at sp[4][0x18]. Then s1 = func_08A61624(host), func_08A90BF4(sp[4] (null: D_08CB02F0 first), s1),
// func_08A7C868(s1, this + 8, sp + 4) and func_08A5B9EC(sp + 4). v0 = the last callee's.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02F0u});`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02A4u});`; `if (signed_of(word(rt, object + 0x34u)) < signed_of(pass.threshold)) continue;`; `if (owner == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02F0u});`; `if (cursor != dest) {`
void func_08C23660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], host = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t holder = frame.at(4u);
    const std::uint32_t source = word(rt, self + 0xCu);
    if (source == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02F0u});
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x34u, 4u});
    (void)call_set(rt, ctx, 0x08A90D6Cu, {block, source});
    put(rt, frame.at(0u), block);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A90470u, {holder, frame.at(0u)});
    const struct {
        std::uint32_t threshold, vector;
    } passes[2] = {{8u, 0x24u}, {7u, 0x18u}};
    for (const auto &pass : passes) {
        std::uint32_t object = word(rt, word(rt, host + 0x34u) + 0x30u);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02A4u});
        if (signed_of(word(rt, object + 0x34u)) < signed_of(pass.threshold)) continue;
        const std::uint32_t owner = word(rt, holder);
        if (owner == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02F0u});
        const std::uint32_t vector = owner + pass.vector;
        const std::uint32_t count = static_cast<std::uint32_t>(signed_of(word(rt, vector + 4u) - word(rt, owner + pass.vector)) >> 2) * 0xAAAAAAABu;
        std::uint32_t cursor = call_set(rt, ctx, 0x08A6159Cu, {host, count});
        std::uint32_t a1 = word(rt, owner + pass.vector), a2 = word(rt, vector + 4u);
        std::uint32_t index = 0u, offset = 0u;
        for (;;) {
            const std::uint32_t entries = static_cast<std::uint32_t>(signed_of(a2 - a1) >> 2) * 0xAAAAAAABu;
            const std::uint32_t dest = offset + a1 + 8u;
            offset += 0xCu;
            const bool more = index < entries;
            ++index;
            if (!more) break;
            if (cursor != dest) {
                (void)call_set(rt, ctx, 0x08A5BAD8u, {dest, word(rt, cursor)});
                a2 = word(rt, vector + 4u);
                a1 = word(rt, vector);
            }
            cursor += 4u;
        }
    }
    const std::uint32_t result = call_set(rt, ctx, 0x08A61624u, {host});
    const std::uint32_t owner = word(rt, holder);
    if (owner == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB02F0u});
    (void)call_set(rt, ctx, 0x08A90BF4u, {owner, result});
    (void)call_set(rt, ctx, 0x08A7C868u, {result, self + 8u, holder});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08C29370(this, key): a lower-bound search in the tree whose end node is this + 4 and whose root is this[8] (a node: [8] left, [0xC] right, [0x10] a value), ordered by a predicate object p = this[0][0x2C] (null: func_08A5BB7C(D_08CB15A0 (0x08CB15A0)) first): for every compared pair two holders are made at sp + 4 / sp + 8 (func_08A76AA0 each, after one at sp), the pair's two values are assigned
// into them (func_08A5BAD8, skipped when the destination is the source's own address), p's virtual slot 0xCC runs with (sp, 2, sp + 4) and the holder sp[0]'s virtual slot 0x30 (null: D_08CB15A0) answers; the three holders are destroyed (func_08A5B9EC, sp + 8, sp + 4, sp). Descent: the pair (node value, key); a non-zero answer goes right (node[0xC]), zero remembers the node and goes left (node[8]).
// Afterwards, unless the remembered node is the end node, the pair (key, remembered value) is asked once more: a non-zero answer returns the end node and zero the remembered node. v0 = the node returned.
// Branches the routes do not take must be read against the asm: the swapped pair of the second ask (holder 1 = key, holder 2 = the remembered node's value).
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first != a) (void)call_set(rt, ctx, 0x08A5BAD8u, {first, word(rt, a)});`; `if (second != b) (void)call_set(rt, ctx, 0x08A5BAD8u, {second, word(rt, b)});`; `if (predicate == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});`; `if (ask(key, best + 0x10u) != 0u) result = word(rt, frame.at(0x10u));`
void func_08C29370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t end = self + 4u, outer = frame.at(0u), first = frame.at(4u), second = frame.at(8u), after = frame.at(0xCu);
    // One ask of the predicate: `a` is assigned to the first holder and `b` to the second (both are addresses of values).
    const auto ask = [&](std::uint32_t a, std::uint32_t b) {
        const std::uint32_t owner = word(rt, self);
        (void)call_set(rt, ctx, 0x08A76AA0u, {outer});
        for (std::uint32_t cell = first; cell != after; cell += 4u) (void)call_set(rt, ctx, 0x08A76AA0u, {cell});
        if (first != a) (void)call_set(rt, ctx, 0x08A5BAD8u, {first, word(rt, a)});
        if (second != b) (void)call_set(rt, ctx, 0x08A5BAD8u, {second, word(rt, b)});
        std::uint32_t predicate = word(rt, owner + 0x2Cu);
        if (predicate == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
        (void)call_set(rt, ctx, slot_of(rt, predicate, 0xCCu), {predicate, outer, 2u, first});
        predicate = word(rt, outer);
        if (predicate == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
        const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, predicate, 0x30u), {predicate});
        std::uint32_t cell = after;
        do {
            cell -= 4u;
            (void)call_set(rt, ctx, 0x08A5B9ECu, {cell});
        } while (cell != first);
        (void)call_set(rt, ctx, 0x08A5B9ECu, {outer});
        return answer;
    };
    std::uint32_t best = end, node = word(rt, self + 8u);
    put(rt, frame.at(0x10u), end);
    while (node != 0u) {
        std::uint32_t next;
        if (ask(node + 0x10u, key) != 0u) {
            next = word(rt, node + 0xCu);
        } else {
            next = word(rt, node + 8u);
            best = node;
        }
        node = next;
    }
    std::uint32_t result = best;
    if (best != end) {
        if (ask(key, best + 0x10u) != 0u) result = word(rt, frame.at(0x10u));
    } else {
        result = word(rt, frame.at(0x10u));
    }
    ctx.gpr[2] = result;
}

// func_08C34C58(tree, hint, value): the hinted insertion of hinted_insert for nodes ordered by a word first (node[0x14], compared unsigned with value[1]) and, on equal words, by the string (node[0x10] against value[0], func_08B36170 as compare_keys). The search is func_08C34ABC(tree, value), the insertion func_08C34990(tree, left, parent, value),
// the neighbours func_08B5F174 (previous) and func_08B5F100 (next). hint == end: an empty tree or a key not above the rightmost node searches, otherwise it is appended below the rightmost node. Otherwise key < hint: the leftmost hint inserts before itself (left = parent = hint); else with p = previous node: key not above p searches, p with a right child inserts before the hint, else below p; key >= hint (words equal: compare_keys(key, hint) >= 0 and then compare_keys(hint, key) >= 0
// returns the hint): the rightmost hint appends below itself; else with n = next node: key not below n searches, a hint with a right child inserts before n (left = parent = n), otherwise below the hint. The word compare and the string compare keep the asm's operand order at every site. v0 = the node.
// Branches the compared cases do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (node_word < key_word) append = true;`; `else if (key_word < node_word) append = false;`; `else append = compare_keys(rt, ctx, word(rt, rightmost + 0x10u), word(rt, value)) < 0;`; `result = append ? insert(0u, word(rt, tree + 0x10u)) : search();`; `if (key_word < hint_word) {`; `} else if (hint_word < key_word) {`; `} else if (compare_keys(rt, ctx, key, word(rt, hint + 0x10u)) < 0) {`; `} else {`; `if (equal) {`; `} else if (below) {`; `if (previous_word < probe) search_it = false;`; `else if (probe < previous_word) search_it = true;`; `else search_it = compare_keys(rt, ctx, word(rt, previous + 0x10u), key) >= 0;`; `if (search_it) result = search();`; `} else if (word(rt, tree + 0x10u) == hint) {`; `if (probe < next_word) search_it = false;`; `else if (next_word < probe) search_it = true;`; `else search_it = compare_keys(rt, ctx, key, word(rt, next + 0x10u)) >= 0;`; `else if (word(rt, hint + 0xCu) != 0u) result = insert(next, next);`; `else result = insert(0u, hint);`
void func_08C34C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t tree = ctx.gpr[4], hint = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    const auto search = [&]() { return call_set(rt, ctx, 0x08C34ABCu, {tree, value}); };
    const auto insert = [&](std::uint32_t left, std::uint32_t parent) { return call_set(rt, ctx, 0x08C34990u, {tree, left, parent, value}); };
    std::uint32_t result;
    if (hint == tree + 4u) {
        if (word(rt, tree + 0x14u) == 0u) {
            result = search();
        } else {
            const std::uint32_t rightmost = word(rt, tree + 0x10u);
            const std::uint32_t node_word = word(rt, rightmost + 0x14u), key_word = word(rt, value + 4u);
            bool append;
            if (node_word < key_word) append = true;
            else if (key_word < node_word) append = false;
            else append = compare_keys(rt, ctx, word(rt, rightmost + 0x10u), word(rt, value)) < 0;
            result = append ? insert(0u, word(rt, tree + 0x10u)) : search();
        }
    } else {
        const std::uint32_t key_word = word(rt, value + 4u), hint_word = word(rt, hint + 0x14u), key = word(rt, value);
        bool below = false, equal = false;
        if (key_word < hint_word) {
            below = true;
        } else if (hint_word < key_word) {
            below = false;
        } else if (compare_keys(rt, ctx, key, word(rt, hint + 0x10u)) < 0) {
            below = true;
        } else {
            equal = compare_keys(rt, ctx, word(rt, hint + 0x10u), key) >= 0;
        }
        if (equal) {
            result = hint;
        } else if (below) {
            if (hint == word(rt, tree + 0xCu)) {
                result = insert(hint, hint);
            } else {
                const std::uint32_t previous = call_set(rt, ctx, 0x08B5F174u, {hint});
                const std::uint32_t previous_word = word(rt, previous + 0x14u), probe = word(rt, value + 4u);
                bool search_it = false;
                if (previous_word < probe) search_it = false;
                else if (probe < previous_word) search_it = true;
                else search_it = compare_keys(rt, ctx, word(rt, previous + 0x10u), key) >= 0;
                if (search_it) result = search();
                else if (word(rt, previous + 0xCu) != 0u) result = insert(hint, hint);
                else result = insert(0u, previous);
            }
        } else if (word(rt, tree + 0x10u) == hint) {
            result = insert(0u, hint);
        } else {
            const std::uint32_t next = call_set(rt, ctx, 0x08B5F100u, {hint});
            const std::uint32_t next_word = word(rt, next + 0x14u), probe = word(rt, value + 4u);
            bool search_it = false;
            if (probe < next_word) search_it = false;
            else if (next_word < probe) search_it = true;
            else search_it = compare_keys(rt, ctx, key, word(rt, next + 0x10u)) >= 0;
            if (search_it) result = search();
            else if (word(rt, hint + 0xCu) != 0u) result = insert(next, next);
            else result = insert(0u, hint);
        }
    }
    ctx.gpr[2] = result;
}

}  // namespace
}  // namespace hot_batch_95
}  // namespace gt::game

#define HOT95(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_95::name, gt::ReturnKind::kind)
#define HOT95N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_95::name, gt::ReturnKind::kind)
HOT95(0x08819170u, func_08819170, Word);
HOT95(0x08858090u, func_08858090, Word);
HOT95(0x08858298u, func_08858298, Word);
HOT95(0x08888988u, func_08888988, Word);
HOT95(0x089170E0u, func_089170E0, Word);
HOT95(0x089936C4u, func_089936C4, Word);
HOT95(0x0899BEC0u, func_0899BEC0, Word);
HOT95(0x0899CB44u, func_0899CB44, Word);
HOT95(0x089B02F0u, func_089B02F0, Word);
HOT95(0x089B90B4u, func_089B90B4, Word);
HOT95(0x089BACACu, func_089BACAC, Word);
HOT95(0x089D2C68u, func_089D2C68, Word);
HOT95(0x089F4140u, func_089F4140, Word);
HOT95(0x08A233B0u, func_08A233B0, Word);
HOT95(0x08A26744u, func_08A26744, Word);
HOT95(0x08A26940u, func_08A26940, Word);
HOT95(0x08A43724u, func_08A43724, Word);
HOT95(0x08A6E974u, func_08A6E974, Word);
HOT95(0x08A82864u, func_08A82864, Word);
HOT95(0x08AF9010u, func_08AF9010, Word);
HOT95(0x08B29070u, func_08B29070, Word);
HOT95(0x08BD4270u, func_08BD4270, Word);
HOT95(0x08BEFA18u, func_08BEFA18, Word);
HOT95(0x08C13A68u, func_08C13A68, Word);
HOT95(0x08BF56E4u, func_08BF56E4, Word);
HOT95(0x08BF70B4u, func_08BF70B4, Word);
HOT95(0x08BF76A8u, func_08BF76A8, Word);
HOT95(0x08C0195Cu, func_08C0195C, Word);
HOT95(0x08C2FED8u, func_08C2FED8, Word);
HOT95(0x08C33C44u, func_08C33C44, Word);
HOT95(0x08C23660u, func_08C23660, Word);
HOT95(0x08C29370u, func_08C29370, Word);
HOT95(0x08C34C58u, func_08C34C58, Word);
