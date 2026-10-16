// Hot batch 90: reached functions that no earlier batch has written, from the batch-89 probe (`tools.decomp_gate probe b89p`: the five routes with --decomp off over the 1,403 not-yet-done functions of 71-120 instructions; 281 reached, 264 without VFPU,
// import-stub calls or register jumps). (main includes batch 89, 6,181 functions.) Before reading any function its address was searched for in src/ (main and the open PR branches: `gh pr list` shows only the draft spec PR #214, no code).
//
// Of the 281 reached, 56 were batch 89's `register_table` family and 225 were still absent from src/. They were clustered with `tools.shapes` (masked-shape signature) and `tools.family_members` decoded each member's own constants: the clusters left in the 71-120 pool
// are small (the biggest has 7 members), so the batch is six cluster templates, the three reserves and 21 hand-read singles. Every function was read with `tools.disasm`, branch TARGETS included. The role words in the comments (node, link, key, vector, begin / end,
// holder, counter, ring, record) describe the shape of the accesses; none is a name from the game, and the classes are not named.
//
//  - clusters (one template each, the members' own constants in their rows, or no row when their words are identical): `hint_insert` (4: func_08BF99BC, func_08C0154C, func_08C1D530, func_08C25E64; an insert with a position hint into a linked structure),
//    `assign_counted` (7 identical copies: func_08BC0DFC, func_08BD0B30, func_08BD3C90, func_08BD80D4, func_08BD8228, func_08BE157C, func_08BE17C8; a reference-counted pointer replaced by another, the ll / sc counters read as plain load and store),
//    `call_member_and_release` (3 identical copies: func_08BD3924, func_08BE0AC0, func_08BE10C4), `owner_release_a` (5: func_08BC98FC, func_08BD24AC, func_08BD523C, func_08BE2E44, func_08BE3A00) and `owner_release_b` (2: func_08BD6140, func_08BD63A0), each member with the vtable word
//    it stores first, `string_tree_insert` (2: func_08C011A0, func_08C0DEF8) and `refresh_then` (2: func_0898A5F8, func_0898A7A8; the second ends with another call). `tools.family_members --check` re-reads the 15 rows with arguments of this file against the words: 15 match, 0 bad,
//    0 unparsed. The rows without arguments (the 6 registered `assign_counted` members and the 3 of `call_member_and_release`) cannot be checked that way, so `tools.family_members TEMPLATE` was run for their two templates (func_08BC0DFC, func_08BD3924): no `!!` line, i.e. no member
//    has words the template lacks; the one unregistered member, func_08BC0DFC, is the template of `assign_counted`;
//  - `vector_reserve_scaled` (func_08C4A594 with 6-byte elements, func_08C4A6B0 with 240-byte ones, func_08C4A7CC with 84-byte ones): three shapes of the reserve of batch 88's `vector_reserve`, for element sizes that are not powers of two (the count comes from a shift and a 32-bit multiply
//    by the modular inverse); they differ in constants and, for func_08C4A7CC, in the registers they end with, so they are not a cluster and have no rows (template arguments are not matched by `--check`; their constants were read by hand);
//  - constructors and initialisers (field-by-field stores around a few calls): func_088E8184, func_08AF1FC0 (a guarded static), func_08B2F31C, func_088B4938, func_08B451A0, func_08B7A13C, func_088E90F0, func_08979B08, func_0888A9B8, func_0889F1DC, func_08BE449C, func_08AE2D38;
//  - the other singles: func_08BB6BFC (a free-list insertion with merging, guarded static), func_088D53FC (a 0x82-byte copy), func_089C6370, func_0881CDC4 and func_08A6D10C (holders and string temporaries, as the script natives of batches 88-89), func_089B139C (three vectors emptied),
//    func_08B03A48 (four tables reset when full), func_08C48004 (a handle record), func_08B5240C (bit-field packing of word pairs).
//
// Written but not registered (the commented HOT90 lines say why): func_08BC0DFC (its only compared case wrote nothing; the six identical copies did), func_08BA9498 and func_08C070FC (a vector erase and a vector assignment: dense runs of 3,000 cases per route compared every call
// (12,231 and 66 calls) and none changed memory), func_08B48A74 (every case stops on `kernel isolation (sleep would block)`).
// Read and left for a later batch: func_08B0ACDC (the parent of two functions written here, func_08B2F31C and func_08B451A0: registered it would swallow their compared calls, FINDINGS batch 86), func_0883C988 / func_0883CCC8 (a formatter that passes `a1` from a
// call that leaves it undefined), func_08B7C598 (a case-insensitive compare: no memory), func_089D5690 (a field-wise compare: no memory), func_08AF07C8, func_08B2E434, func_08B79F58, func_08AEFE28, func_08B82420, func_08AEC030, func_08ACECFC (calls to the import stubs from 0x08C70000),
// func_08BB2660, func_08BB2340, func_08BB24B0, func_08AE1D6C, func_08B020EC, func_089064D8, func_08B14864, func_08BB13D4, func_08C1E634, func_0888AD94, func_08B23A70 (tangled control flow), and batch 89's 13 deferred candidates.
//
// Compared on their main path but with side paths read from the asm only: each function's comment lists, from `tools.decomp_gate coverage`, what the compared cases did not take and what was checked against the asm by hand. Every registered function was compared at least once.
// The two registrations that callers read `v1` from (func_08C011A0, func_08C0DEF8: `tools.contract_audit` lists them, 0 of them GT_DECOMP_NESTED) leave the stack word with the flag byte in it; those callers store it and read one byte back.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest copies from ctx. Every call goes through `call_keep` (the callee's v0,
// v1 and f0 stay in the registers like the original's), and the callee-saved registers (s0-s7, fp) hold the original's values around the calls (`SavedRegs` restores the entry values on the way out, like the original's epilogue): a callee may store the caller's s0 (FINDINGS,
// batch 84). The delay slot of a `jal` / `jalr` runs BEFORE the call, and a `bnel` delay slot only when the branch is taken. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the early returns; the other caller-saved
// registers are set to the original's value where it is cheap and a callee could read them, not otherwise (the gate compares v0, s0-s7, sp, ra and RAM). The dead stack (spills the original makes and nothing reads) is not reproduced, the harness ignores it; the frames that
// callees read (holders, records passed by address) are.
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_90 {
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

// The callee-saved registers s0-s7 and fp as they were at entry: the original saves them in its frame and restores them before it returns or tail-calls.
class SavedRegs {
public:
    explicit SavedRegs(psprecomp::AllegrexContext &ctx) : ctx_(ctx) {
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
    psprecomp::AllegrexContext &ctx_;
    std::uint32_t saved_[8];
    std::uint32_t saved_fp_;
};

// A floating-point register as it was at entry (the original saves the callee-saved $f20 in its frame and restores it before it returns or tail-calls).
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

inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }

inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }

inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }

// The ll / sc counters (one thread: a plain load and store). A decrement leaves the old value in a0, v0 = 1 (the sc's success flag) and v1 = the address; an increment leaves v0 = 1 and v1 = the address (the old value is in v0 until
// the addiu, then the sc flag replaces it).
inline std::uint32_t atomic_decrement(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}
inline void atomic_increment(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}
inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// %hi of an address as the lui / addiu pair builds it (the addiu has the signed %lo).
inline std::uint32_t high_half(std::uint32_t address) { return (address + ((address & 0x8000u) != 0u ? 0x10000u : 0u)) & 0xFFFF0000u; }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// hint_insert(find, insert): func_08BF99BC, func_08C0154C, func_08C1D530, func_08C25E64 (108 instructions, one function four times; the members differ in the two callees of their own, the row holds them).
//
// func_X(this, position, value): a structure of linked nodes whose header node is this + 4 (this[0xC] and this[0x10] are node pointers that position is compared with, this[0x14] a count); each node holds a word key at +0x10 and a word at +0xC (called `link` below), and `value` points at the key to insert. With find(this, value) = `find` (v0 the node, v1 the word whose low byte is a flag) and insert(this, hint, parent, value) = `insert`:
//   - position == this + 4: a non-empty tree whose last node (this[0x10]) has a key below *value gives insert(this, 0, last, value); otherwise find(this, value);
//   - *value < position's key: position == this[0xC] gives insert(this, position, position, value); otherwise before = func_08B5F174(position): its key below *value gives insert(this, 0, before, value) when before's link is null
//     and insert(this, position, position, value) when it is set; a key that is not below gives find(this, value);
//   - position's key < *value: position == this[0x10] gives insert(this, 0, position, value); otherwise after = func_08B5F100(position): *value below its key gives insert(this, 0, position, value) when position's link is null and
//     insert(this, after, after, value) when it is set; otherwise find(this, value);
//   - equal keys: v0 = position (v1 = position's key).
// v0 is the insert's answer, the node of find (v1 too, the original copies it there) or position. The dead stores to the frame (the original spills find's pair there) are not reproduced; the harness ignores the dead stack.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS; coverage of the four members together, addresses of func_08BF99BC): the end position with a last node whose key is below (0x08BF99F0-0x08BF9A04, target 0x08BF9B08) never occurred, only find; for any other position only `*value < position's key` happened, and within it position == this[0xC] (0x08BF9A44-0x08BF9A58) and a predecessor whose key is not below (find at 0x08BF9A94-0x08BF9AB8) never did, so the whole `position's key < *value` part and the equal-keys return (0x08BF9ABC-0x08BF9B50 and 0x08BF9B54) are read from the asm only.
inline void hint_insert(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t find, std::uint32_t insert) {
    const std::uint32_t self = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = value;
    ctx.gpr[17] = self;
    ctx.gpr[16] = position;
    ctx.gpr[2] = self + 4u;
    const auto do_find = [&]() {
        ctx.gpr[4] = self;
        ctx.gpr[5] = value;
        const std::uint32_t node = call_keep(rt, ctx, find, {self, value});
        ctx.gpr[16] = node;
        ctx.gpr[3] = node;
        ctx.gpr[2] = node;
    };
    const auto do_insert = [&](std::uint32_t hint, std::uint32_t parent) {
        ctx.gpr[4] = self;
        ctx.gpr[5] = hint;
        ctx.gpr[6] = parent;
        ctx.gpr[7] = value;
        const std::uint32_t node = call_keep(rt, ctx, insert, {self, hint, parent, value});
        ctx.gpr[16] = node;
        ctx.gpr[2] = node;
    };
    if (self + 4u == position) {
        if (word(rt, self + 0x14u) != 0u) {
            const std::uint32_t last = word(rt, self + 0x10u), key = word(rt, value);
            ctx.gpr[6] = last;
            ctx.gpr[3] = key;
            if (word(rt, last + 0x10u) < key) {
                do_insert(0u, last);
                return;
            }
        }
        do_find();
        return;
    }
    const std::uint32_t key = word(rt, value), position_key = word(rt, position + 0x10u);
    ctx.gpr[6] = key;
    ctx.gpr[3] = position_key;
    if (key < position_key) {
        if (position == word(rt, self + 0xCu)) {
            do_insert(position, position);
            return;
        }
        ctx.gpr[4] = position;
        const std::uint32_t before = call_keep(rt, ctx, 0x08B5F174u, {position});
        ctx.gpr[6] = before;
        const std::uint32_t current_key = word(rt, value);
        ctx.gpr[3] = current_key;
        if (word(rt, before + 0x10u) < current_key) {
            ctx.gpr[2] = word(rt, before + 0xCu);
            if (word(rt, before + 0xCu) != 0u) do_insert(position, position);
            else do_insert(0u, before);
            return;
        }
        do_find();
        return;
    }
    if (position_key < key) {
        ctx.gpr[2] = word(rt, self + 0x10u);
        if (word(rt, self + 0x10u) == position) {
            do_insert(0u, position);
            return;
        }
        ctx.gpr[4] = position;
        const std::uint32_t after = call_keep(rt, ctx, 0x08B5F100u, {position});
        ctx.gpr[5] = after;
        const std::uint32_t after_key = word(rt, after + 0x10u), current_key = word(rt, value);
        ctx.gpr[3] = after_key;
        ctx.gpr[2] = current_key;
        if (current_key < after_key) {
            if (word(rt, position + 0xCu) != 0u) do_insert(after, after);
            else do_insert(0u, position);
            return;
        }
        do_find();
        return;
    }
    ctx.gpr[2] = position;
}
void func_08BF99BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hint_insert(rt, ctx, 0x08BF98E4u, 0x08BF981Cu); }
void func_08C0154C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hint_insert(rt, ctx, 0x08C01474u, 0x08C013ACu); }
void func_08C1D530(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hint_insert(rt, ctx, 0x08C1D458u, 0x08C1D2DCu); }
void func_08C25E64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { hint_insert(rt, ctx, 0x08C25D8Cu, 0x08C25B2Cu); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// assign_counted: func_08BC0DFC, func_08BD0B30, func_08BD3C90, func_08BD80D4, func_08BD8228, func_08BE157C, func_08BE17C8 (85 instructions, one function seven times; every member has the same words, the shape has no row).
//
// func_X(this, other), with this[0x2C] a pointer to a reference-counted object (strong counter at +4, weak counter at +8, the vtable words at offsets 8 and 0xC run when the counters reach zero):
//   - other == 0: this[0x28] = 0 and this[0x2C] = 0; the old object (a null one returns) has its strong counter decremented: when it was 1 the vtable word at offset 8 is called with it, then its weak counter is decremented and when that was 1 the function
//     ends with a tail call to the vtable word at offset 0xC of the object (a0 = the object); a counter that was not 1 returns with v0 = 1;
//   - other != 0: this[0x28] = other[0]; n = other[4]; n == this[0x2C] returns (v0 = other[0]); otherwise a non-null n has its strong counter incremented, the old object is let go the same way (a null one just gets the store;
//     the calls through the vtable offsets 8 and 0xC are plain calls here, the last one followed by the store), and this[0x2C] = n is stored at every exit after the increment.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS; coverage of the seven members together, addresses of func_08BC0DFC): with other == 0 the old object's strong counter was never 1 (0x08BC0E3C always branched to 0x08BC0F3C), so the virtual call (0x08BC0E40-0x08BC0E4C), the weak step (0x08BC0E54-0x08BC0E6C) and the tail call (0x08BC0E70-0x08BC0E90) never ran; with other != 0 the case n == this[0x2C] (0x08BC0EA4) never held, n was never null (0x08BC0EAC) and the old object was always null (0x08BC0ED0 always branched to 0x08BC0F34), so the release of an old object (0x08BC0ED8-0x08BC0F30) is read from the asm only.
inline void assign_counted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    std::uint32_t tail_target = 0u, tail_object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        if (other == 0u) {
            const std::uint32_t old = word(rt, self + 0x2Cu);
            ctx.gpr[16] = old;
            put(rt, self + 0x28u, 0u);
            put(rt, self + 0x2Cu, 0u);
            if (old == 0u) return;
            if (atomic_decrement(rt, ctx, old + 4u) != 1u) return;
            const std::uint32_t vtable = word(rt, old), function = word(rt, vtable + 8u);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            ctx.gpr[4] = old;
            (void)call_keep(rt, ctx, function, {old});
            if (atomic_decrement(rt, ctx, old + 8u) != 1u) return;
            const std::uint32_t weak_vtable = word(rt, old);
            ctx.gpr[2] = weak_vtable;
            ctx.gpr[4] = old;
            tail_target = word(rt, weak_vtable + 0xCu);
            tail_object = old;
            ctx.gpr[25] = tail_target;
        } else {
            const std::uint32_t first = word(rt, other), current = word(rt, self + 0x2Cu);
            ctx.gpr[2] = first;
            ctx.gpr[3] = current;
            put(rt, self + 0x28u, first);
            const std::uint32_t incoming = word(rt, other + 4u);
            ctx.gpr[17] = incoming;
            ctx.gpr[18] = self + 0x2Cu;
            if (incoming == current) return;
            if (incoming != 0u) atomic_increment(rt, ctx, incoming + 4u);
            const std::uint32_t old = word(rt, self + 0x2Cu);
            ctx.gpr[16] = old;
            ctx.gpr[3] = old + 4u;  // the delay slot of the test: it runs on both paths
            if (old != 0u) {
                if (atomic_decrement(rt, ctx, old + 4u) != 1u) {
                    put(rt, self + 0x2Cu, incoming);  // the delay slot of the `bnel`: it runs when the branch is taken
                    return;
                }
                const std::uint32_t vtable = word(rt, old), function = word(rt, vtable + 8u);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                ctx.gpr[4] = old;
                (void)call_keep(rt, ctx, function, {old});
                if (atomic_decrement(rt, ctx, old + 8u) != 1u) {
                    put(rt, self + 0x2Cu, incoming);
                    return;
                }
                const std::uint32_t weak_vtable = word(rt, old), weak_function = word(rt, weak_vtable + 0xCu);
                ctx.gpr[3] = weak_vtable;
                ctx.gpr[2] = weak_function;
                ctx.gpr[4] = old;
                (void)call_keep(rt, ctx, weak_function, {old});
            }
            put(rt, self + 0x2Cu, incoming);
            return;
        }
    }
    tail_call(ctx, tail_target, {tail_object});
}
[[maybe_unused]] void func_08BC0DFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_counted(rt, ctx); }
void func_08BD0B30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_counted(rt, ctx); }
void func_08BD3C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_counted(rt, ctx); }
void func_08BD80D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_counted(rt, ctx); }
void func_08BD8228(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_counted(rt, ctx); }
void func_08BE157C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_counted(rt, ctx); }
void func_08BE17C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_counted(rt, ctx); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// call_member_and_release: func_08BD3924, func_08BE0AC0, func_08BE10C4 (85 instructions, one function three times; the words are the same in every member).
//
// func_X(this, other): c = this[0x2C] (a reference-counted object as in assign_counted: a non-null c has its strong counter at +4 incremented); this[0xC] = 1 and this[0xE] = 0 (bytes); then the member function that this[0x28], this[0x30] and
// this[0x34] hold is called: p = this[0x28] + (this[0x34] >> 1, arithmetic) is the object, and when this[0x34] is odd the function is read from the vtable of p at the offset this[0x30] (word at *p + this[0x30]), otherwise it is this[0x30]
// itself; a0 = p, a1 = this[0x34]. After it, unless this[0xE] != 0 or (other[0xA8] == 0 and this[0xD] != 0), the object o = this[8] has its vtable word at offset 0xC called with (o, 0), func_08AC05F4(this) runs and this[8] = 0.
// Then c, when non-null, has its strong counter decremented: when it was 1 the vtable word at offset 8 is called with it, then its weak counter is decremented and when that was 1 the function ends with a tail call to the vtable word at offset 0xC of c.
// v0 is 1 after a counter step that did not lead to a call, the call's v0 after one that did, and (a null c) the last byte or call result the code before it left.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS; coverage of the three members together, addresses of func_08BD3924): a null this[0x2C] (0x08BD3940 and 0x08BD39EC), an odd this[0x34] (the lookup through the vtable, 0x08BD397C-0x08BD3994), a non-zero other[0xA8] (0x08BD39AC, target 0x08BD39C4), the this[0xE] test after a zero this[0xD] (0x08BD39C0) and the cleanup (0x08BD39CC-0x08BD39E8: this[0xD] was non-zero in every case) never occurred, and the strong counter of c was never 1 at the end, so the release (0x08BD3A10-0x08BD3A5C, the tail call included) never ran.
inline void call_member_and_release(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    std::uint32_t tail_target = 0u, tail_object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = other;
        const std::uint32_t counted = word(rt, self + 0x2Cu);
        ctx.gpr[17] = counted;
        ctx.gpr[16] = self;
        if (counted != 0u) atomic_increment(rt, ctx, counted + 4u);
        const std::uint32_t pointer = word(rt, self + 0x34u);
        ctx.gpr[5] = pointer;
        ctx.gpr[2] = 1u;
        rt.memory().store8(self + 0xCu, 1u);
        ctx.gpr[3] = pointer & 1u;
        rt.memory().store8(self + 0xEu, 0u);
        const std::uint32_t base = word(rt, self + 0x28u);
        ctx.gpr[6] = base;
        const std::uint32_t adjusted = base + static_cast<std::uint32_t>(signed_of(pointer) >> 1);
        std::uint32_t function = 0u;
        if ((pointer & 1u) != 0u) {
            const std::uint32_t vtable = word(rt, adjusted);
            ctx.gpr[2] = adjusted;
            const std::uint32_t offset = word(rt, self + 0x30u);
            ctx.gpr[4] = offset;
            function = word(rt, vtable + offset);
            ctx.gpr[3] = function;
        } else {
            function = word(rt, self + 0x30u);
            ctx.gpr[3] = function;
        }
        ctx.gpr[4] = adjusted;
        (void)call_keep(rt, ctx, function, {adjusted, pointer});
        const std::uint32_t flag = word(rt, other + 0xA8u);
        ctx.gpr[2] = flag;
        bool skip = false;
        if (flag != 0u) {
            ctx.gpr[2] = byte_u(rt, self + 0xEu);
            skip = ctx.gpr[2] != 0u;
        } else {
            ctx.gpr[2] = byte_u(rt, self + 0xDu);
            skip = ctx.gpr[2] != 0u;
            if (!skip) {
                ctx.gpr[2] = byte_u(rt, self + 0xEu);
                skip = ctx.gpr[2] != 0u;
            }
        }
        if (!skip) {
            const std::uint32_t object = word(rt, self + 8u);
            ctx.gpr[4] = object;
            const std::uint32_t vtable = word(rt, object), slot = word(rt, vtable + 0xCu);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = slot;
            ctx.gpr[5] = 0u;
            (void)call_keep(rt, ctx, slot, {object, 0u});
            ctx.gpr[4] = self;
            (void)call_keep(rt, ctx, 0x08AC05F4u, {self});
            put(rt, self + 8u, 0u);
        }
        ctx.gpr[3] = counted + 4u;  // the delay slot of the test: it runs on both paths
        if (counted == 0u) return;
        if (atomic_decrement(rt, ctx, counted + 4u) != 1u) return;
        const std::uint32_t vtable = word(rt, counted), slot = word(rt, vtable + 8u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = slot;
        ctx.gpr[4] = counted;
        (void)call_keep(rt, ctx, slot, {counted});
        if (atomic_decrement(rt, ctx, counted + 8u) != 1u) return;
        const std::uint32_t weak_vtable = word(rt, counted);
        ctx.gpr[2] = weak_vtable;
        ctx.gpr[4] = counted;
        tail_target = word(rt, weak_vtable + 0xCu);
        tail_object = counted;
        ctx.gpr[25] = tail_target;
    }
    tail_call(ctx, tail_target, {tail_object});
}
void func_08BD3924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_member_and_release(rt, ctx); }
void func_08BE0AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_member_and_release(rt, ctx); }
void func_08BE10C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_member_and_release(rt, ctx); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// owner_release_a(vtable): func_08BC98FC, func_08BD24AC, func_08BD523C, func_08BE2E44, func_08BE3A00 (78 instructions) and owner_release_b(vtable): func_08BD6140, func_08BD63A0 (77 instructions, the `FinalCall` shape).
// The members differ in the vtable word they store at the start; the two shapes differ at the end only.
//
// func_X(this): o = this[0x10]; this[0] = `vtable`; func_08B34888(o). Then, when o[0x2C] is 0: func_08B34B24(o); a non-null p = o[0x38] has the vtable word at offset 0 called with (p, o[0x34]), and o[0x38] = 0, o[0x34] = 0 follow;
// func_08B28BE0(o) and func_08B64654(o) run. Otherwise o[0x2C] is decremented and when it reached 0: a non-null q = o[0x34] is taken out together with r = o[0x38] (both cleared) and, when the byte o[0x30] is non-zero, func_08B34C1C(o) runs;
// then func_08B34B24(o), the vtable word at offset 8 of q called with (q) when q is non-null and the one at offset 0 of r called with (r, q) when r is non-null. The end:
//   shape a: this[0] = D_08CBF488 (v0 holds it) and a tail call to func_08B64654(this);
//   shape b: func_08BAEF88(this), then the same tail call to func_08B64654(this).
// Note that the first path calls func_08B64654(o) and also, through the end, func_08B64654(this); the original does both.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS; coverage per shape, addresses of func_08BC98FC for shape a and func_08BD6140 for shape b): o[0x2C] was never 0 (0x08BC9934 / 0x08BD6178 always branched to the other path), so the first path (0x08BC993C-0x08BC9978 / 0x08BD6180-0x08BD61BC: func_08B34B24, the slot-0 call, the two clearing stores, func_08B28BE0 and the first func_08B64654) is read from the asm only; the byte o[0x30] was always 0 (func_08B34C1C at 0x08BC99C8 / 0x08BD620C never called) and, in shape b, o[0x34] was never null (0x08BD61E8 never branched).
template <bool FinalCall>
inline void owner_release_impl(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        const std::uint32_t object = word(rt, self + 0x10u);
        ctx.gpr[16] = object;
        ctx.gpr[2] = vtable;
        put(rt, self, vtable);
        (void)call_keep(rt, ctx, 0x08B34888u, {object});
        std::uint32_t state = word(rt, object + 0x2Cu);
        ctx.gpr[2] = state;
        ctx.gpr[4] = object;
        std::uint32_t first = 0u, second = 0u;
        if (state == 0u) {
            ctx.gpr[18] = object + 0x34u;
            (void)call_keep(rt, ctx, 0x08B34B24u, {object});
            const std::uint32_t target = word(rt, object + 0x38u);
            ctx.gpr[4] = target;
            if (target != 0u) {
                const std::uint32_t target_vtable = word(rt, target), function = word(rt, target_vtable);
                ctx.gpr[3] = target_vtable;
                ctx.gpr[2] = function;
                const std::uint32_t argument = word(rt, object + 0x34u);
                ctx.gpr[5] = argument;
                (void)call_keep(rt, ctx, function, {target, argument});
                put(rt, object + 0x38u, 0u);
                put(rt, object + 0x34u, 0u);
            }
            ctx.gpr[4] = object;
            (void)call_keep(rt, ctx, 0x08B28BE0u, {object});
            ctx.gpr[4] = object;
            (void)call_keep(rt, ctx, 0x08B64654u, {object});
        } else {
            ctx.gpr[19] = 0u;
            ctx.gpr[18] = 0u;
            state -= 1u;
            ctx.gpr[2] = state;
            put(rt, object + 0x2Cu, state);
            const std::uint32_t reread = word(rt, object + 0x2Cu);
            ctx.gpr[3] = reread;
            if (reread == 0u) {
                const std::uint32_t taken = word(rt, object + 0x34u);
                ctx.gpr[2] = taken;
                if (taken != 0u) {
                    second = word(rt, object + 0x38u);
                    first = taken;
                    ctx.gpr[19] = second;
                    ctx.gpr[18] = first;
                    put(rt, object + 0x34u, 0u);
                    put(rt, object + 0x38u, 0u);
                }
                const std::uint32_t flag = byte_u(rt, object + 0x30u);
                ctx.gpr[2] = flag;
                if (flag != 0u) {
                    ctx.gpr[4] = object;
                    (void)call_keep(rt, ctx, 0x08B34C1Cu, {object});
                }
            }
            ctx.gpr[4] = object;
            (void)call_keep(rt, ctx, 0x08B34B24u, {object});
            if (first != 0u) {
                const std::uint32_t first_vtable = word(rt, first), function = word(rt, first_vtable + 8u);
                ctx.gpr[3] = first_vtable;
                ctx.gpr[2] = function;
                ctx.gpr[4] = first;
                (void)call_keep(rt, ctx, function, {first});
            }
            ctx.gpr[4] = second;  // the delay slot of the test: it runs on both paths
            if (second != 0u) {
                const std::uint32_t second_vtable = word(rt, second), function = word(rt, second_vtable);
                ctx.gpr[3] = second_vtable;
                ctx.gpr[2] = function;
                ctx.gpr[5] = first;
                (void)call_keep(rt, ctx, function, {second, first});
            }
        }
        ctx.gpr[4] = self;
        if constexpr (FinalCall) {
            (void)call_keep(rt, ctx, 0x08BAEF88u, {self});
        } else {
            ctx.gpr[2] = 0x08CBF488u;
            put(rt, self, 0x08CBF488u);
        }
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}
inline void owner_release_a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) { owner_release_impl<false>(rt, ctx, vtable); }
inline void owner_release_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) { owner_release_impl<true>(rt, ctx, vtable); }
void func_08BC98FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { owner_release_a(rt, ctx, 0x08CC5240u); }
void func_08BD24AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { owner_release_a(rt, ctx, 0x08CC53E0u); }
void func_08BD523C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { owner_release_a(rt, ctx, 0x08CC5570u); }
void func_08BE2E44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { owner_release_a(rt, ctx, 0x08CC6910u); }
void func_08BE3A00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { owner_release_a(rt, ctx, 0x08CC68A8u); }
void func_08BD6140(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { owner_release_b(rt, ctx, 0x08CC55C8u); }
void func_08BD63A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { owner_release_b(rt, ctx, 0x08CC5648u); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// string_tree_insert(insert): func_08C011A0, func_08C0DEF8 (87 instructions, one function twice; the members differ in the insert callee, the row holds it).
//
// func_X(this, key) with `key` a pointer to a string-data pointer t (t[-0xC] its length), a linked structure with the first node at this[8], the header node this + 4 and a node pointer this[0xC] (compared with the last node of the descent)
// and nodes that hold their string-data pointer at +0x10 and two links at +8 and +0xC. With compare(a, b, n) = func_08B36170 (called with the first n = min(lengths) characters; its non-zero answer wins over the length difference):
//   descent: from the first node while the node is non-null, d = compare(t, node text) or, when it is 0, length(t) - length(node text); d < 0 follows the link at +8, otherwise the one at +0xC; the last node visited is `last` (this + 4 for an empty tree);
//   a descent that ended on the link at +8 (and an empty tree) continues at the check with `last` = func_08B5F174(last) unless last == this[0xC], in which case insert(this, 0, last, key) is called at once;
//   the check: d = compare(last text, t) or the length difference (last's minus t's); d >= 0 means the key is already there (v0 = last, the flag 0), otherwise insert(this, 0, last, key) gives v0 and the flag is 1.
// The flag is stored as a byte at sp + 0xC and v1 is the whole word read back from there (the three upper bytes are whatever the stack held; the callers that read v1 after the call, at 0x089C7BB4, 0x089FB0F8 and 0x089FB508, store it and read back one byte with lbu); v0 is the node.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS; coverage of both members together, addresses of func_08C011A0): the key already present (0x08C01298 branching to 0x08C012BC with the flag 0) never occurred: every compared call inserted.
inline void string_tree_insert(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t insert) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = self;
    ctx.gpr[30] = key;
    std::uint32_t node = word(rt, self + 8u);
    ctx.gpr[18] = node;
    std::uint32_t last = self + 4u;
    ctx.gpr[20] = last;
    bool check = false;
    if (node != 0u) {
        const std::uint32_t text = word(rt, key);
        ctx.gpr[19] = text;
        for (;;) {
            const std::uint32_t length = word(rt, text - 0xCu);
            ctx.gpr[16] = length;
            const std::uint32_t node_text = word(rt, node + 0x10u), node_length = word(rt, node_text - 0xCu);
            ctx.gpr[17] = node_length;
            const std::uint32_t shortest = std::min(length, node_length);
            ctx.gpr[4] = text;
            ctx.gpr[5] = node_text;
            ctx.gpr[6] = shortest;
            std::uint32_t difference = length - node_length;
            const std::uint32_t compared = call_keep(rt, ctx, 0x08B36170u, {text, node_text, shortest});
            if (compared != 0u) difference = compared;
            const bool lower = (difference >> 31) != 0u;
            ctx.gpr[16] = difference >> 31;
            const std::uint32_t next = word(rt, node + (lower ? 8u : 0xCu));
            ctx.gpr[2] = next;
            if (next == 0u) {
                last = node;
                ctx.gpr[20] = last;
                check = !lower;
                break;
            }
            node = next;
            ctx.gpr[18] = node;
        }
    }
    std::uint32_t result = 0u;
    bool inserted = false;
    if (!check) {
        const std::uint32_t marked_node = word(rt, self + 0xCu);
        ctx.gpr[2] = marked_node;
        ctx.gpr[4] = self;
        if (last == marked_node) {
            ctx.gpr[5] = 0u;
            ctx.gpr[6] = last;
            ctx.gpr[7] = key;
            result = call_keep(rt, ctx, insert, {self, 0u, last, key});
            inserted = true;
        } else {
            ctx.gpr[4] = last;
            node = call_keep(rt, ctx, 0x08B5F174u, {last});
            ctx.gpr[18] = node;
        }
    } else {
        node = last;
        ctx.gpr[18] = node;
    }
    if (!inserted) {
        const std::uint32_t node_text = word(rt, node + 0x10u), text = word(rt, key);
        ctx.gpr[19] = text;
        const std::uint32_t node_length = word(rt, node_text - 0xCu), length = word(rt, text - 0xCu);
        ctx.gpr[16] = node_length;
        ctx.gpr[17] = length;
        const std::uint32_t shortest = std::min(length, node_length);
        std::uint32_t difference = node_length - length;
        ctx.gpr[4] = node_text;
        ctx.gpr[5] = text;
        ctx.gpr[6] = shortest;
        const std::uint32_t compared = call_keep(rt, ctx, 0x08B36170u, {node_text, text, shortest});
        if (compared != 0u) difference = compared;
        ctx.gpr[16] = difference;
        ctx.gpr[2] = 0u;  // the delay slot of the test: it runs on both paths
        if (signed_of(difference) >= 0) {
            result = node;
        } else {
            ctx.gpr[4] = self;
            ctx.gpr[5] = 0u;
            ctx.gpr[6] = last;
            ctx.gpr[7] = key;
            result = call_keep(rt, ctx, insert, {self, 0u, last, key});
            inserted = true;
        }
    }
    ctx.gpr[2] = inserted ? 1u : 0u;
    rt.memory().store8(frame.at(0xCu), static_cast<std::uint8_t>(ctx.gpr[2]));
    ctx.gpr[3] = word(rt, frame.at(0xCu));
    ctx.gpr[2] = result;
}
void func_08C011A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_tree_insert(rt, ctx, 0x08C01094u); }
void func_08C0DEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_tree_insert(rt, ctx, 0x08C0DDF8u); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// vector_reserve_scaled<Shift, SumInV1>(size, inverse, maximum, copy, destroy): func_08C4A594 (6-byte elements), func_08C4A6B0 (240-byte elements) and func_08C4A7CC (84-byte elements). Three shapes of one function (the third ends with the new end in v1 and
// 12 * used in v0, SumInV1): the element counts come from a byte difference
// shifted right (arithmetic) by `Shift` and multiplied by the modular inverse of the odd part of the element size (0xAAAAAAAB, 0xEEEEEEEF; a 32-bit mult, low word), the byte count of a request is n * size.
//
// func_X(this, n): n above `maximum` calls func_08B5ADDC(D_08CB4AEC) (the code after that call would read through a0, which still holds the string address, so the call is assumed not to return; the function returns after it, as func_08B904D8 of batch 88 does). Otherwise, when the
// capacity (this[8] - this[0]) in elements is below n: begin = this[0], end = this[4], used = elements in [begin, end); a block of n * size bytes comes from func_08B35DA0 (none for n == 0), `copy`(begin, end, block, sp) and
// `destroy`(this[0], this[4], sp + 1) run, the old block is freed with func_08B35254 when non-null, and this[8] = block + n * size, this[4] = block + used * size, this[0] = block. v0 = block + used * size, or the capacity compare (0) when
// nothing had to grow.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS; coverage of the three instances, addresses of func_08C4A594 / func_08C4A6B0 / func_08C4A7CC): n above the maximum (the func_08B5ADDC call, 0x08C4A5C0 / 0x08C4A6DC / 0x08C4A7F8), a request that fits the capacity (0x08C4A5F8 / 0x08C4A714 / 0x08C4A830), n == 0 (0x08C4A614 / 0x08C4A730 / 0x08C4A84C) and a non-null old block (the free, 0x08C4A65C / 0x08C4A778 / 0x08C4A89C) never occurred: every call grew an empty vector with n > 0.
template <unsigned Shift, bool SumInV1 = false>
inline void vector_reserve_scaled(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t size, std::uint32_t inverse, std::uint32_t maximum, std::uint32_t copy, std::uint32_t destroy) {
    const std::uint32_t self = ctx.gpr[4], count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    ctx.gpr[2] = maximum < count ? 1u : 0u;
    if (maximum < count) {
        ctx.gpr[4] = 0x08CB4AECu;
        (void)call_keep(rt, ctx, 0x08B5ADDCu, {0x08CB4AECu});
        return;
    }
    const std::uint32_t begin = word(rt, self);
    ctx.gpr[17] = begin;
    ctx.gpr[3] = inverse;
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - begin) >> Shift) * inverse;
    ctx.gpr[2] = capacity < count ? 1u : 0u;
    ctx.gpr[19] = 0u;
    if (!(capacity < count)) return;
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[18] = end;
    ctx.gpr[21] = 0u;
    const std::uint32_t used = static_cast<std::uint32_t>(signed_of(end - begin) >> Shift) * inverse;
    ctx.gpr[20] = used;
    std::uint32_t block = 0u, bytes = 0u;
    if (count != 0u) {
        bytes = count * size;
        ctx.gpr[21] = bytes;
        ctx.gpr[4] = bytes;
        block = call_keep(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[19] = block;
    }
    ctx.gpr[4] = begin;
    ctx.gpr[5] = end;
    ctx.gpr[6] = block;
    ctx.gpr[7] = frame.at(0u);
    (void)call_keep(rt, ctx, copy, {begin, end, block, frame.at(0u)});
    const std::uint32_t old_begin = word(rt, self), old_end = word(rt, self + 4u);
    ctx.gpr[4] = old_begin;
    ctx.gpr[5] = old_end;
    ctx.gpr[6] = frame.at(1u);
    (void)call_keep(rt, ctx, destroy, {old_begin, old_end, frame.at(1u)});
    const std::uint32_t old = word(rt, self);
    ctx.gpr[4] = old;
    if (old != 0u) {
        ctx.gpr[2] = used * size;
        (void)call_keep(rt, ctx, 0x08B35254u, {old});
    }
    const std::uint32_t used_bytes = used * size;
    put(rt, self + 8u, block + bytes);
    put(rt, self + 4u, block + used_bytes);
    put(rt, self, block);
    ctx.gpr[4] = block + bytes;
    if constexpr (SumInV1) {
        ctx.gpr[2] = used * 12u;
        ctx.gpr[3] = block + used_bytes;
    } else {
        ctx.gpr[3] = used << (size == 6u ? 1u : 4u);
        ctx.gpr[2] = block + used_bytes;
    }
}
void func_08C4A594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve_scaled<1u>(rt, ctx, 6u, 0xAAAAAAABu, 0x2AAAAAAAu, 0x08C49F24u, 0x08C49F14u); }
void func_08C4A6B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve_scaled<4u>(rt, ctx, 240u, 0xEEEEEEEFu, 0x01111111u, 0x08C49F74u, 0x08C49F64u); }
void func_08C4A7CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_reserve_scaled<2u, true>(rt, ctx, 84u, 0x3CF3CF3Du, 0x030C30C3u, 0x08C49E50u, 0x08C49E40u); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// func_088E8184(this): while the byte this[0x11] is non-zero func_08B2E2B8(0x3E8) is called (and the byte read again); h = this[0xC] was read before that loop. Then this is reset field by field (words 0 / -1 / 3 / 9, bytes 0 / 1 / 0xFF, the halfword
// this[0x20] = -1, the three words this[0x2A4..0x2AC] = 0) and func_088D1A90(this + 0x30) runs. A non-null h ends with a tail call to the vtable word at offset 0xD0 with a0 = h; a null h returns (v0 = the call's).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the wait loop (func_08B2E2B8 at 0x088E81A4, entered when the byte this[0x11] is non-zero) never ran, and a null this[0xC] (0x088E8270 branching to the plain return at 0x088E8298) never occurred: the tail call always ran.
void func_088E8184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t tail_target = 0u, tail_object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[16] = self;
        std::uint32_t flag = byte_u(rt, self + 0x11u);
        ctx.gpr[2] = flag;
        const std::uint32_t holder = word(rt, self + 0xCu);
        ctx.gpr[17] = holder;
        while (flag != 0u) {
            ctx.gpr[4] = 0x3E8u;
            (void)call_keep(rt, ctx, 0x08B2E2B8u, {0x3E8u});
            flag = byte_u(rt, self + 0x11u);
            ctx.gpr[2] = flag;
        }
        put(rt, self + 0x2D8u, 0xFFFFFFFFu);
        put(rt, self + 0x8u, 9u);
        put(rt, self + 0x2DCu, 0xFFFFFFFFu);
        put(rt, self + 0x2C4u, 3u);
        put(rt, self + 0x2BCu, 0xFFFFFFFFu);
        put8(rt, self + 0x2A0u, 0xFFu);
        put8(rt, self + 0x17u, 1u);
        put(rt, self + 0x0u, 0xFFFFFFFFu);
        put(rt, self + 0x2E0u, 0u);
        put8(rt, self + 0x13u, 0u);
        put8(rt, self + 0x14u, 0u);
        put8(rt, self + 0x15u, 1u);
        put(rt, self + 0x1FCu, 0xFFFFFFFFu);
        put(rt, self + 0xCu, 0u);
        put(rt, self + 0x4u, 3u);
        put8(rt, self + 0x10u, 0u);
        put8(rt, self + 0x12u, 0u);
        put(rt, self + 0x1Cu, 0u);
        put8(rt, self + 0x19u, 0u);
        put(rt, self + 0x2E4u, 0u);
        put(rt, self + 0x24u, 0u);
        put(rt, self + 0x28u, 0u);
        put16(rt, self + 0x20u, 0xFFFFFFFFu);
        put(rt, self + 0x2Cu, 0u);
        put8(rt, self + 0x2B7u, 0u);
        put(rt, self + 0x2B8u, 0xFFFFFFFFu);
        put(rt, self + 0x2C0u, 0u);
        put8(rt, self + 0x2A1u, 0u);
        put8(rt, self + 0x2A2u, 0u);
        put8(rt, self + 0x16u, 0u);
        for (std::uint32_t index = 0u; index < 3u; ++index) put(rt, self + 0x2A4u + 4u * index, 0u);
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[3] = self + 12u;
        ctx.gpr[4] = self + 0x30u;
        ctx.gpr[5] = 1u;
        ctx.gpr[6] = 3u;
        ctx.gpr[7] = 3u;
        ctx.gpr[8] = 3u;
        (void)call_keep(rt, ctx, 0x088D1A90u, {self + 0x30u});
        if (holder == 0u) return;
        const std::uint32_t vtable = word(rt, holder), function = word(rt, vtable + 0xD0u);
        ctx.gpr[4] = holder;
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[25] = function;
        tail_target = function;
        tail_object = holder;
    }
    tail_call(ctx, tail_target, {tail_object});
}

// func_08AF1FC0(this): this[0] = D_08CD7000 (the constant stored first), then a block b of 0x330 bytes from func_08B62344 is filled in: words 4, 8, 0xC, 0x10, 0x14 zero, 32 triples of zero words from b + 0x18 in steps of 0x14, -1 at +0x2AC, the bytes +0x310 = 0,
// +0x311 = +0x312 = +0x313 = 1, D_08CB4AA8 at +0x320 and zero at the words +0x298..0x2A8 (not 0x2AC), +0x2B0..0x2BC and +0x314, +0x318, +0x31C; this[4] = b. Then the guarded static D_08D77278 (guard byte D_08D77270, acquire func_08B6441C, release func_08B64494): the first
// use stores D_08CD7018 in it; this[8] = D_08D77278. v0 = D_08D77278's high half after the guard was already set (or its acquire refused), the release's v0 after the first use.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): none: coverage shows every branch outcome taken (guard already set, acquire granted and acquire refused included).
void func_08AF1FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[3] = self;
    ctx.gpr[16] = self + 4u;
    put(rt, self, 0x08CD7000u);
    ctx.gpr[2] = 0x08CD7000u;
    ctx.gpr[4] = 0x330u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x330u});
    ctx.gpr[5] = block;
    put(rt, block + 4u, 0u);
    put(rt, block + 8u, 0u);
    put(rt, block + 0xCu, 0u);
    put(rt, block + 0x10u, 0u);
    put(rt, block + 0x14u, 0u);
    for (std::uint32_t index = 0u; index < 0x20u; ++index) {
        put(rt, block + 0x18u + 0x14u * index, 0u);
        put(rt, block + 0x1Cu + 0x14u * index, 0u);
        put(rt, block + 0x20u + 0x14u * index, 0u);
    }
    put(rt, block + 0x2ACu, 0xFFFFFFFFu);
    put8(rt, block + 0x313u, 1u);
    put(rt, block + 0x320u, 0x08CB4AA8u);
    for (const std::uint32_t offset : {0x298u, 0x29Cu, 0x2A0u, 0x2A4u, 0x2A8u, 0x2B0u, 0x2B4u, 0x2B8u, 0x2BCu}) put(rt, block + offset, 0u);
    put8(rt, block + 0x310u, 0u);
    put8(rt, block + 0x311u, 1u);
    put8(rt, block + 0x312u, 1u);
    put(rt, block + 0x31Cu, 0u);
    put(rt, block + 0x318u, 0u);
    put(rt, block + 0x314u, 0u);
    put(rt, self + 4u, block);
    constexpr std::uint32_t guard = 0x08D77270u, object = 0x08D77278u;
    const std::uint32_t high = high_half(object);
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
    ctx.gpr[18] = guard;
    bool first_use = false;
    if (byte_at(rt, guard) == 0) {
        ctx.gpr[4] = guard;
        const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
        ctx.gpr[3] = high;
        first_use = acquired != 0u;
    }
    ctx.gpr[17] = object;
    if (first_use) {
        ctx.gpr[2] = 0x08CD7018u;
        put(rt, object, 0x08CD7018u);
        ctx.gpr[4] = guard;
        (void)call_keep(rt, ctx, 0x08B64494u, {guard});
    } else {
        ctx.gpr[2] = high;
    }
    put(rt, self + 8u, object);
}

// func_08B2F31C(this): this[0] = D_08CDA720; words this[0xA0], [0xA4], [0xA8], [0xAC], [0x10], [0x14], [0x18], [0x8F8] = 0, this[0xC] = -1, bytes this[4], [5], [7], [8] = 0 and this[6] = 1; func_08B28DCC(this + 0x8FC);
// then this[0x12B0], [0x12B4] = 0, this[0x12A8] = -1 and, with t = this + 0x10000, the words t[-0x69C0] = 0, t[-0x69BC] = 0, t[-0x69C4] = -1, t[-0x69C8] = -1 and the byte t[-0x69B8] = 0 (the last one in the delay slot of
// the next call, so before it); func_08B2F17C(0x4C00, 0) stored at t[-0x69B4]; func_08B36018 clears (value 0) this + 0x1C (0x80 bytes), + 0xB0 (0x848), + 0x928 (0x980), + 0x12B8 (0x8300) and + 0x95B8 (0x80); D_08D69160 = this.
// v0 is the last clear's answer.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight-line code).
void func_08B2F31C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const std::uint32_t table = self + 0x10000u;
    ctx.gpr[16] = self;
    ctx.gpr[17] = table;
    ctx.gpr[18] = 0xFFFFFFFFu;
    put(rt, self, 0x08CDA720u);
    put(rt, self + 0xA0u, 0u);
    put(rt, self + 0xA8u, 0u);
    put(rt, self + 0xA4u, 0u);
    put(rt, self + 0xACu, 0u);
    put8(rt, self + 6u, 1u);
    put(rt, self + 0xCu, 0xFFFFFFFFu);
    put8(rt, self + 4u, 0u);
    put8(rt, self + 5u, 0u);
    put8(rt, self + 7u, 0u);
    put8(rt, self + 8u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x8F8u, 0u);  // the delay slot of the jal: it runs before the call
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = 0u;
    ctx.gpr[4] = self + 0x8FCu;
    (void)call_keep(rt, ctx, 0x08B28DCCu, {self + 0x8FCu});
    put(rt, self + 0x12B0u, 0u);
    put(rt, self + 0x12B4u, 0u);
    put(rt, table - 0x69C0u, 0u);
    put(rt, table - 0x69BCu, 0u);
    put(rt, table - 0x69C4u, 0xFFFFFFFFu);
    put(rt, self + 0x12A8u, 0xFFFFFFFFu);
    put(rt, table - 0x69C8u, 0xFFFFFFFFu);
    put8(rt, table - 0x69B8u, 0u);  // the delay slot of the jal: it runs before the call
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[4] = 0x4C00u;
    ctx.gpr[5] = 0u;
    const std::uint32_t created = call_keep(rt, ctx, 0x08B2F17Cu, {0x4C00u, 0u});
    put(rt, table - 0x69B4u, created);
    const struct { std::uint32_t offset, size; } clears[] = {{0x1Cu, 0x80u}, {0xB0u, 0x848u}, {0x928u, 0x980u}, {0x12B8u, 0x8300u}, {0x95B8u, 0x80u}};
    for (const auto &clear : clears) {
        ctx.gpr[4] = self + clear.offset;
        ctx.gpr[5] = 0u;
        ctx.gpr[6] = clear.size;
        (void)call_keep(rt, ctx, 0x08B36018u, {self + clear.offset, 0u, clear.size});
    }
    ctx.gpr[3] = high_half(0x08D69160u);
    put(rt, 0x08D69160u, self);
}

// func_088B4938(this): v = func_088B4384(this[0x14]) (this[0x14] = the word at s3 + 0x10 with s3 = this + 4) is stored at this[0x38]; the 28 words this[0x1B8..0x224] are zeroed, and the halfwords this[0x228], [0x22A], [0x22C] = -1. Then o = this[4]: its vtable word at offset 0x5C is called with
// (o, byte this[0x3FC]); a non-zero answer r gives func_088BB44C(this + 0x3C, r, this + 8, this[0x18], byte this[0x3FC]). func_088B55EC(this + 0x1B8, this + 8), func_088B6664(this + 0x260) and a tail call to func_088B4894(this).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the answer of the virtual call was never 0 (0x088B4A24 never branched to 0x088B4A3C), so func_088BB44C always ran.
void func_088B4938(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[19] = self + 4u;
        ctx.gpr[16] = self;
        ctx.gpr[17] = self + 0x1B8u;
        ctx.gpr[18] = self + 8u;
        ctx.gpr[4] = word(rt, self + 0x14u);
        const std::uint32_t value = call_keep(rt, ctx, 0x088B4384u, {word(rt, self + 0x14u)});
        ctx.gpr[5] = 0xFFFFFFFFu;
        ctx.gpr[3] = self + 0x1BCu;
        ctx.gpr[4] = self + 0x1CCu;
        ctx.gpr[6] = self + 0x1E0u;
        ctx.gpr[7] = self + 0x1ECu;
        ctx.gpr[8] = self + 0x1F8u;
        ctx.gpr[9] = self + 0x204u;
        ctx.gpr[10] = self + 0x210u;
        ctx.gpr[11] = self + 0x21Cu;
        put(rt, self + 0x38u, value);
        for (std::uint32_t offset = 0x1B8u; offset <= 0x224u; offset += 4u) put(rt, self + offset, 0u);
        put16(rt, self + 0x228u, 0xFFFFu);
        put16(rt, self + 0x22Cu, 0xFFFFu);
        put16(rt, self + 0x22Au, 0xFFFFu);
        const std::uint32_t object = word(rt, self + 4u);
        ctx.gpr[4] = object;
        const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 0x5Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        const std::uint32_t flag = byte_u(rt, self + 0x3FCu);  // the delay slot of the jalr: it runs before the call
        ctx.gpr[5] = flag;
        const std::uint32_t answer = call_keep(rt, ctx, function, {object, flag});
        ctx.gpr[5] = answer;
        ctx.gpr[4] = self + 0x3Cu;  // the delay slot of the test: it runs on both paths
        if (answer != 0u) {
            const std::uint32_t third = word(rt, self + 0x18u), fifth = byte_u(rt, self + 0x3FCu);
            ctx.gpr[7] = third;
            ctx.gpr[8] = fifth;
            ctx.gpr[6] = self + 8u;
            (void)call_keep(rt, ctx, 0x088BB44Cu, {self + 0x3Cu, answer, self + 8u, third, fifth});
        }
        ctx.gpr[4] = self + 0x1B8u;
        ctx.gpr[5] = self + 8u;
        (void)call_keep(rt, ctx, 0x088B55ECu, {self + 0x1B8u, self + 8u});
        ctx.gpr[4] = self + 0x260u;
        (void)call_keep(rt, ctx, 0x088B6664u, {self + 0x260u});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x088B4894u, {self});
}

// func_08B451A0(this): func_08B08840(this) first; then this[0] = D_08CDA9A0, this[0xC0] = -1, the bytes this[0xAD], [0xB6..0xBA] = 0, this[0xB0] = 0, the halfword this[0xB4] = 0, the words this[0xBC], [0xC4], [0xC8] = 0 (the last in the delay slot of the next
// call, so before it); func_08B2C350(this + 0xCC), func_08B28DCC(this + 0x12C), func_08B3EB80(this + 0x154); func_08B36018 clears this + 0x454 and this + 0x654 (0x200 bytes each) and this + 0x854 (0x80); this[0x8D4] = 0x80 and this[0x8D8] = 0 (before the
// call) and func_08B3F75C(this + 0x8DC, this + 0xCC); the words this[0x910..0x92C] = 0, this[0x93C] = -1, this[0x930], [0x940], [0x944], [0x988] = 0, the halfwords this[0x934], [0x938] and the byte this[0x936] = 0; func_08B3FA98(this + 0xBB8) with
// this[0x98C] = 0 before it; D_08D69500 = 0 and D_08D694FC = 0, and a tail call to func_08B3FF98(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight-line code).
void func_08B451A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[19] = 0xFFFFFFFFu;
        ctx.gpr[16] = self;
        (void)call_keep(rt, ctx, 0x08B08840u, {self});
        put(rt, self, 0x08CDA9A0u);
        ctx.gpr[18] = self + 0xCCu;
        ctx.gpr[17] = self + 0x454u;
        put(rt, self + 0xC0u, 0xFFFFFFFFu);
        put8(rt, self + 0xADu, 0u);
        put(rt, self + 0xB0u, 0u);
        put16(rt, self + 0xB4u, 0u);
        for (const std::uint32_t offset : {0xB6u, 0xB7u, 0xB8u, 0xB9u, 0xBAu}) put8(rt, self + offset, 0u);
        put(rt, self + 0xBCu, 0u);
        put(rt, self + 0xC4u, 0u);
        put(rt, self + 0xC8u, 0u);  // the delay slot of the jal: it runs before the call
        ctx.gpr[4] = self + 0xCCu;
        (void)call_keep(rt, ctx, 0x08B2C350u, {self + 0xCCu});
        ctx.gpr[4] = self + 0x12Cu;
        (void)call_keep(rt, ctx, 0x08B28DCCu, {self + 0x12Cu});
        ctx.gpr[4] = self + 0x154u;
        (void)call_keep(rt, ctx, 0x08B3EB80u, {self + 0x154u});
        const struct { std::uint32_t offset, size; } clears[] = {{0x454u, 0x200u}, {0x654u, 0x200u}, {0x854u, 0x80u}};
        for (const auto &clear : clears) {
            ctx.gpr[4] = self + clear.offset;
            ctx.gpr[5] = 0u;
            ctx.gpr[6] = clear.size;
            (void)call_keep(rt, ctx, 0x08B36018u, {self + clear.offset, 0u, clear.size});
        }
        ctx.gpr[3] = 0x80u;
        put(rt, self + 0x8D4u, 0x80u);
        ctx.gpr[5] = self + 0xCCu;
        ctx.gpr[4] = self + 0x8DCu;
        put(rt, self + 0x8D8u, 0u);  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x08B3F75Cu, {self + 0x8DCu, self + 0xCCu});
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        put(rt, self + 0x910u, 0u);
        put(rt, self + 0x914u, 0u);
        ctx.gpr[4] = self + 0xBB8u;
        put(rt, self + 0x93Cu, 0xFFFFFFFFu);
        for (const std::uint32_t offset : {0x918u, 0x91Cu, 0x920u, 0x924u, 0x928u, 0x92Cu, 0x930u}) put(rt, self + offset, 0u);
        put16(rt, self + 0x934u, 0u);
        put8(rt, self + 0x936u, 0u);
        put16(rt, self + 0x938u, 0u);
        put(rt, self + 0x940u, 0u);
        put(rt, self + 0x944u, 0u);
        put(rt, self + 0x988u, 0u);
        put(rt, self + 0x98Cu, 0u);  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x08B3FA98u, {self + 0xBB8u});
        ctx.gpr[2] = high_half(0x08D69500u);
        ctx.gpr[3] = high_half(0x08D694FCu);
        put(rt, 0x08D69500u, 0u);
        put(rt, 0x08D694FCu, 0u);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B3FF98u, {self});
}

// func_08B7A13C(this): this[8] = this + 0x2C4, this[0xC] = this + 0x320, this[4] = this + 0x268, this[0], [0x10], [0x30], [0x38..0x5C] = 0, this[0x34] = D_08CB5EE0; the bytes this[0x14..0x2C] (25) and this[0x7C..0x9F] (36) and this[0x60], [0xD8], [0xE0] = 0;
// this[0xA8] = 1; the halfwords this[0xB0..0xBC] = 0x330E, 0xABCD, 0x1234, 0xE66D, 0xDEEC, 5, 0xB; the words this[0xA0], [0xAC], [0xC0..0xD4], [0xF8], [0xFC..0x120], [0x148..0x154], [0x1D4], [0x254..0x264] = 0 (this[0x264] in the delay slot of the call, so before it);
// func_08B36018(this + 0x268, 0, 0x114) clears 0x114 bytes there; then this[0x37C] = 0. v0 is the clear's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight-line code and two counted loops).
void func_08B7A13C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    put(rt, self + 8u, self + 0x2C4u);
    put(rt, self + 0xCu, self + 0x320u);
    put(rt, self + 0u, 0u);
    put(rt, self + 4u, self + 0x268u);
    put(rt, self + 0x10u, 0u);
    for (std::uint32_t index = 0u; index < 0x19u; ++index) put8(rt, self + 0x14u + index, 0u);
    put(rt, self + 0x34u, 0x08CB5EE0u);
    put(rt, self + 0x30u, 0u);
    for (std::uint32_t offset = 0x38u; offset <= 0x5Cu; offset += 4u) put(rt, self + offset, 0u);
    put8(rt, self + 0x60u, 0u);
    for (std::uint32_t index = 0u; index < 0x24u; ++index) put8(rt, self + 0x7Cu + index, 0u);
    put(rt, self + 0xA8u, 1u);
    put(rt, self + 0xACu, 0u);
    put16(rt, self + 0xB0u, 0x330Eu);
    put16(rt, self + 0xB2u, 0xABCDu);
    put16(rt, self + 0xB4u, 0x1234u);
    put16(rt, self + 0xB6u, 0xE66Du);
    put16(rt, self + 0xB8u, 0xDEECu);
    put16(rt, self + 0xBAu, 5u);
    put16(rt, self + 0xBCu, 0xBu);
    for (const std::uint32_t offset : {0xA0u, 0xC0u, 0xC4u, 0xC8u, 0xCCu, 0xD0u, 0xD4u}) put(rt, self + offset, 0u);
    for (std::uint32_t offset = 0xFCu; offset <= 0x120u; offset += 4u) put(rt, self + offset, 0u);
    put8(rt, self + 0xD8u, 0u);
    put8(rt, self + 0xE0u, 0u);
    put(rt, self + 0xF8u, 0u);
    for (const std::uint32_t offset : {0x148u, 0x14Cu, 0x150u, 0x154u, 0x254u, 0x1D4u, 0x258u, 0x25Cu, 0x260u, 0x264u}) put(rt, self + offset, 0u);
    ctx.gpr[2] = 5u;
    ctx.gpr[3] = 0xBu;
    ctx.gpr[4] = self + 0x268u;
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 0x114u;
    (void)call_keep(rt, ctx, 0x08B36018u, {self + 0x268u, 0u, 0x114u});
    put(rt, self + 0x37Cu, 0u);
}

// func_088E90F0(this): this[8] = 9, this[0] = -1, this[4] = 3, this[0xC] = 0, the bytes this[0x10..0x14], [0x16], [0x18], [0x19], [0x22] = 0, this[0x15] = this[0x17] = 1, this[0x1C], [0x24], [0x28], [0x2C] = 0, the halfword this[0x20] = -1;
// this[0x1FC] = -1 in the delay slot of func_088D0578(this + 0x200); this[0x2A0] = 0xFF, the bytes this[0x2A1], [0x2A2] = 0 (the last before the next call); b = func_08B62344(0x3C) and func_08B28D34(b) (s1 = b is set in the delay slot of that call,
// so before it); then b[0x2C], b[0x30] (byte), b[0x34], b[0x38] = 0; this[0x2D0], [0x2D4], [0x2E0], [0x2E4], [0x2C0], [0x2EC], [0x2F0], [0x2F4] = 0, the bytes this[0x2B5], [0x2B6], [0x2B7] = 0 and this[0x2B4] = 1, this[0x2B0] = b, the words this[0x2BC],
// [0x2B8], [0x2D8], [0x2DC] = -1, this[0x2C4], [0x2C8] = 3, this[0x2E8] = D_08CC3778 and the three words this[0x2A4..0x2AC] = 0. v0 = -1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight-line code and one counted loop).
void func_088E90F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = 3u;
    ctx.gpr[19] = 1u;
    ctx.gpr[18] = 0xFFFFFFFFu;
    ctx.gpr[16] = self;
    put(rt, self + 8u, 9u);
    put(rt, self + 0u, 0xFFFFFFFFu);
    put(rt, self + 4u, 3u);
    put(rt, self + 0xCu, 0u);
    for (const std::uint32_t offset : {0x10u, 0x11u, 0x12u, 0x13u, 0x14u, 0x16u, 0x18u, 0x19u}) put8(rt, self + offset, 0u);
    put8(rt, self + 0x15u, 1u);
    put8(rt, self + 0x17u, 1u);
    put(rt, self + 0x1Cu, 0u);
    put16(rt, self + 0x20u, 0xFFFFFFFFu);
    put8(rt, self + 0x22u, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x2Cu, 0u);
    ctx.gpr[2] = 9u;
    ctx.gpr[4] = self + 0x200u;
    put(rt, self + 0x1FCu, 0xFFFFFFFFu);  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x088D0578u, {self + 0x200u});
    put8(rt, self + 0x2A0u, 0xFFu);
    put8(rt, self + 0x2A1u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[4] = 0x3Cu;
    put8(rt, self + 0x2A2u, 0u);  // the delay slot of the jal: it runs before the call
    const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x3Cu});
    ctx.gpr[4] = block;
    ctx.gpr[17] = block;  // the delay slot of the next jal: it runs before the call
    (void)call_keep(rt, ctx, 0x08B28D34u, {block});
    ctx.gpr[2] = 0u;
    ctx.gpr[3] = 0u;
    put(rt, block + 0x2Cu, 0u);
    put8(rt, block + 0x30u, 0u);
    put(rt, block + 0x34u, 0u);
    put(rt, block + 0x38u, 0u);
    put(rt, self + 0x2D0u, 0u);
    put(rt, self + 0x2D4u, 0u);
    put8(rt, self + 0x2B4u, 1u);
    put(rt, self + 0x2B0u, block);
    put(rt, self + 0x2BCu, 0xFFFFFFFFu);
    put(rt, self + 0x2C8u, 3u);
    put(rt, self + 0x2D8u, 0xFFFFFFFFu);
    put(rt, self + 0x2DCu, 0xFFFFFFFFu);
    put8(rt, self + 0x2B5u, 0u);
    put8(rt, self + 0x2B6u, 0u);
    put8(rt, self + 0x2B7u, 0u);
    put(rt, self + 0x2B8u, 0xFFFFFFFFu);
    put(rt, self + 0x2C0u, 0u);
    put(rt, self + 0x2C4u, 3u);
    put(rt, self + 0x2E0u, 0u);
    put(rt, self + 0x2E4u, 0u);
    put(rt, self + 0x2E8u, 0x08CC3778u);
    put(rt, self + 0x2F4u, 0u);
    put(rt, self + 0x2ECu, 0u);
    put(rt, self + 0x2F0u, 0u);
    for (std::uint32_t index = 0u; index < 3u; ++index) put(rt, self + 0x2A4u + 4u * index, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = 0xFFFFFFFFu;
}

// func_08979B08(this): under the lock this + 0x20 (func_08B344E0 .. func_08B3457C) the list head this + 0x18 is made by func_08BEDD64(this + 0x18) and linked to itself (this[0x1C] = this[0x18] = this + 0x18, the second store in the delay slot of the unlock call);
// this[0x7C] = this[0x74], this[0x80] = this[0x78], this[0x90] = 0x01000000, this[0x88] (byte), this[0x8C] = 0, the 22 word pairs this[0x94 + 4i], this[0xEC + 4i] = 0; this[0x168] = 10, the halfword this[0x174] = 0x14, this[0x16C] = 0x12C, this[0x170] = 1.0f, the bytes this[0x149],
// [0x154], [0x155] = 0xFF, the halfword this[0x176] = 6, this[0x17C] = 0x00FFFFFF, and zero in this[0x144], [0x148], [0x14A], [0x14B], [0x14C], [0x150], the halfword [0x156], the bytes [0x158..0x15B], [0x160], this[0x15C], [0x164], [0x178] (the last in the delay slot of the next call);
// then o = func_08977260() and its vtable word at offset 0x194 with (o), func_0897851C(this), func_0897999C(this), this[0x198] = 1 (byte), this[0x1AC] = 0. v0 = 1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight-line code and one counted loop).
void func_08979B08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self + 0x20u;
    ctx.gpr[18] = self;
    ctx.gpr[16] = self + 0x18u;
    ctx.gpr[4] = self + 0x20u;
    (void)call_keep(rt, ctx, 0x08B344E0u, {self + 0x20u});
    ctx.gpr[4] = self + 0x18u;
    (void)call_keep(rt, ctx, 0x08BEDD64u, {self + 0x18u});
    ctx.gpr[4] = self + 0x20u;
    put(rt, self + 0x1Cu, self + 0x18u);
    put(rt, self + 0x18u, self + 0x18u);  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x08B3457Cu, {self + 0x20u});
    put(rt, self + 0x80u, word(rt, self + 0x78u));
    put(rt, self + 0x7Cu, word(rt, self + 0x74u));
    put(rt, self + 0x90u, 0x01000000u);
    put8(rt, self + 0x88u, 0u);
    put(rt, self + 0x8Cu, 0u);
    for (std::uint32_t index = 0u; index < 0x16u; ++index) {
        put(rt, self + 0x94u + 4u * index, 0u);
        put(rt, self + 0xECu + 4u * index, 0u);
    }
    put(rt, self + 0x168u, 10u);
    put16(rt, self + 0x174u, 0x14u);
    put(rt, self + 0x16Cu, 0x12Cu);
    put8(rt, self + 0x155u, 0xFFu);
    put_float(rt, self + 0x170u, 1.0f);
    put8(rt, self + 0x149u, 0xFFu);
    put8(rt, self + 0x154u, 0xFFu);
    put16(rt, self + 0x176u, 6u);
    put(rt, self + 0x17Cu, 0x00FFFFFFu);
    put(rt, self + 0x144u, 0u);
    put8(rt, self + 0x148u, 0u);
    put8(rt, self + 0x14Au, 0u);
    put8(rt, self + 0x14Bu, 0u);
    put(rt, self + 0x14Cu, 0u);
    put(rt, self + 0x150u, 0u);
    put16(rt, self + 0x156u, 0u);
    for (const std::uint32_t offset : {0x158u, 0x159u, 0x15Au, 0x15Bu}) put8(rt, self + offset, 0u);
    put(rt, self + 0x15Cu, 0u);
    put8(rt, self + 0x160u, 0u);
    put(rt, self + 0x164u, 0u);
    ctx.gpr[1] = 0x3F800000u;
    ctx.fpr[0] = 1.0f;
    ctx.gpr[2] = 0x00FFFFFFu;
    ctx.gpr[3] = 6u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = self;
    put(rt, self + 0x178u, 0u);  // the delay slot of the jal: it runs before the call
    const std::uint32_t object = call_keep(rt, ctx, 0x08977260u, {self});
    const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 0x194u);
    ctx.gpr[4] = object;
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    (void)call_keep(rt, ctx, function, {object});
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x0897851Cu, {self});
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x0897999Cu, {self});
    ctx.gpr[2] = 1u;
    put8(rt, self + 0x198u, 1u);
    put(rt, self + 0x1ACu, 0u);
}

// func_08BB6BFC(p): the guarded static D_08D75F5C (guard byte D_08D75F50, acquire func_08B6441C, release func_08B64494) is set up on first use: D_08D75F5C = 0 (before the release call), +4 = +8 = 0, +0xC = +0x14 = D_08D75F68, +0x10 = 0, +0x18 = 0x1001,
// +0x1C = +0x20 = +0x24 = 0, and func_08B77B1C(0x088B8234) runs after the release. A null `p` returns. Otherwise b = p - 8 is a block header (b[0] the next block, b[4] its size in 8-byte units) of a ring of blocks sorted by address that starts at
// r = D_08D75F5C[0x14]: r moves to its next block (r[0]) until r < b and b < r[0] (the block goes in the middle of the ring), or until r[0] <= r (the end of the ring) with r < b or b < r[0]. Then D_08D75F5C[0x20] -= b[4]; when b + 8 * b[4] is the block r[0] the two merge
// (b[4] += r[0][4], b[0] = r[0][0]), else b[0] = r[0]; when r + 8 * r[4] is b they merge (r[4] += b[4], r[0] = b[0]), else r[0] = b; D_08D75F5C[0x14] = r. v0 is D_08D75F5C (the guarded path leaves the callee's).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the first use of the guarded static (0x08BB6C18 and the acquire at 0x08BB6C28, up to 0x08BB6C78) never ran, nor did a null `p` (0x08BB6C7C branching to 0x08BB6D3C); the walk along the ring always stopped at its first node (b above r and below r[0]: the fall-through at 0x08BB6CC4), so the advance to the next node (0x08BB6CB0) and the wrap-around tests (0x08BB6C94-0x08BB6CA8) are read from the asm only.
void func_08BB6BFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pointer = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t guard = 0x08D75F50u, base = 0x08D75F5Cu, sentinel = 0x08D75F68u;
    ctx.gpr[17] = guard;
    ctx.gpr[2] = high_half(guard);
    ctx.gpr[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
    ctx.gpr[16] = pointer;
    if (byte_at(rt, guard) == 0) {
        ctx.gpr[4] = guard;
        if (call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            ctx.gpr[6] = high_half(base);
            ctx.gpr[5] = sentinel;
            ctx.gpr[2] = base;
            ctx.gpr[3] = 0x1001u;
            ctx.gpr[4] = guard;
            put(rt, base + 0x18u, 0x1001u);
            put(rt, base + 0xCu, sentinel);
            put(rt, base + 0x10u, 0u);
            put(rt, base + 0x4u, 0u);
            put(rt, base + 0x8u, 0u);
            put(rt, base + 0x14u, sentinel);
            put(rt, base + 0x1Cu, 0u);
            put(rt, base + 0x20u, 0u);
            put(rt, base + 0x24u, 0u);
            put(rt, base, 0u);  // the delay slot of the jal: it runs before the call
            (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            ctx.gpr[4] = 0x088B8234u;
            (void)call_keep(rt, ctx, 0x08B77B1Cu, {0x088B8234u});
        }
    }
    ctx.gpr[8] = high_half(base);  // the delay slot of the test: it runs on both paths
    if (pointer == 0u) return;
    ctx.gpr[2] = base;
    std::uint32_t ring = word(rt, base + 0x14u);
    ctx.gpr[5] = ring;
    const std::uint32_t block = pointer - 8u;
    ctx.gpr[7] = block;
    for (;;) {
        const bool before = ring < block;
        const std::uint32_t next = word(rt, ring);
        ctx.gpr[6] = before ? 1u : 0u;
        ctx.gpr[3] = next;
        if (before && block < next) {
            ctx.gpr[2] = 1u;
            break;
        }
        const bool ascending = ring < next, below = block < next;
        ctx.gpr[2] = ascending ? 1u : 0u;
        ctx.gpr[4] = below ? 1u : 0u;
        if (!ascending && (before || below)) break;
        ring = next;
        ctx.gpr[5] = ring;
    }
    ctx.gpr[4] = base;
    const std::uint32_t size = word(rt, block + 4u);
    ctx.gpr[2] = word(rt, base + 0x20u) - size;
    ctx.gpr[3] = size;
    put(rt, base + 0x20u, word(rt, base + 0x20u) - size);
    const std::uint32_t units = word(rt, block + 4u), following = word(rt, ring);
    ctx.gpr[4] = units;
    ctx.gpr[3] = following;
    ctx.gpr[2] = block + (units << 3);
    if (block + (units << 3) != following) {
        put(rt, block, following);
    } else {
        const std::uint32_t merged_size = word(rt, following + 4u), merged_next = word(rt, following);
        ctx.gpr[2] = units + merged_size;
        ctx.gpr[3] = merged_next;
        put(rt, block + 4u, units + merged_size);
        put(rt, block, merged_next);
    }
    const std::uint32_t ring_units = word(rt, ring + 4u);
    ctx.gpr[4] = ring_units;
    ctx.gpr[2] = (ring_units << 3) + ring;
    if (block != (ring_units << 3) + ring) {
        put(rt, ring, block);
    } else {
        const std::uint32_t block_size = word(rt, block + 4u), block_next = word(rt, block);
        ctx.gpr[2] = ring_units + block_size;
        ctx.gpr[3] = block_next;
        put(rt, ring + 4u, ring_units + block_size);
        put(rt, ring, block_next);
    }
    ctx.gpr[2] = base;
    put(rt, base + 0x14u, ring);
}

// func_088D53FC(this, destination): size = func_088D4800(this); a size above 0x82 clears `size` bytes at `destination` first (func_08B36018(destination, 0, size)). Then 0x82 bytes are copied from the address this[0] to `destination`: 0x60 bytes, the source word
// this[0] read again, then 0x20 bytes and two single bytes, in 16-byte steps that load four words and then store them (the original has an aligned form with lw / sw and an unaligned one with lwl / lwr / swl / swr; both copy the same bytes).
// v0 = destination + size and v1 is the last byte copied.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the size never exceeded 0x82 (0x088D5424 always branched to 0x088D5438), so the clearing func_08B36018 (0x088D5428-0x088D5434) never ran.
void func_088D53FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], destination = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    ctx.gpr[16] = destination;
    const std::uint32_t size = call_keep(rt, ctx, 0x088D4800u, {self});
    ctx.gpr[17] = size;
    ctx.gpr[2] = size - 0x82u;
    ctx.gpr[4] = destination;
    if (signed_of(size - 0x82u) > 0) {
        ctx.gpr[5] = 0u;
        ctx.gpr[6] = size;
        (void)call_keep(rt, ctx, 0x08B36018u, {destination, 0u, size});
    }
    const auto copy16 = [&](std::uint32_t to, std::uint32_t from) {
        std::uint8_t bytes[16];
        for (unsigned i = 0u; i < 16u; ++i) bytes[i] = rt.memory().load8(from + i);
        for (unsigned i = 0u; i < 16u; ++i) rt.memory().store8(to + i, bytes[i]);
    };
    std::uint32_t source = word(rt, self);
    for (std::uint32_t offset = 0u; offset < 0x60u; offset += 0x10u) copy16(destination + offset, source + offset);
    source = word(rt, self);
    for (std::uint32_t offset = 0x60u; offset < 0x80u; offset += 0x10u) copy16(destination + offset, source + offset);
    const std::uint32_t first = byte_u(rt, source + 0x80u);
    rt.memory().store8(destination + 0x80u, static_cast<std::uint8_t>(first));
    const std::uint32_t second = byte_u(rt, source + 0x81u);
    rt.memory().store8(destination + 0x81u, static_cast<std::uint8_t>(second));
    ctx.gpr[2] = destination + size;
    ctx.gpr[3] = second;
}

// func_08AE2D38(this): with g = D_08D40930: func_08B36018 clears 0x48 bytes at g; func_08AD8960(this, D_08C91484, g + 0x28, 0x20) and func_08AD8960(this, D_08C91494, g + 0x40, 0x20) fill g[0x28] and g[0x40]; b = func_08AD84B4(g[0x28] * 36, 1) is stored at g[0x24] (before the test
// of the next branch) and, when non-null, the list heads g + 4 and g + 0x14 are set (g[0x10] = 0, g[8] = g + 4, g[0xC] = -1, g[0x18] = g + 0x14, g[0x1C] = -1, g[0x20] = 0, g[4] = g + 4, g[0x14] = g + 0x14, the last in the delay slot of the call) and
// func_08AD8C18(g + 0x14, b, g[0x28], 0x24) runs; then c = func_08AD84B4(g[0x40] * 0x8C, 1) is stored at g[0x3C] and, when non-null, g[0x34] = -1, g[0x30] = g[0x2C] = g + 0x2C, g[0x38] = 0 and func_08AD8C18(g + 0x2C, c, g[0x40], 0x8C) runs; g[0] = 1 and v0 = v1 = 1.
// When a block is null: g[0x3C] (if non-zero) and g[0x24] (if non-zero) are freed with func_08AD85C8 and v0 = 0.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): neither allocation ever failed (0x08AE2DD0 and 0x08AE2E28 never branched to 0x08AE2E74), so the failure path (0x08AE2E74-0x08AE2EB0: the two frees and v0 = 0) is read from the asm only.
void func_08AE2D38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t g = 0x08D40930u;
    ctx.gpr[18] = high_half(g);
    ctx.gpr[17] = g;
    ctx.gpr[16] = self;
    ctx.gpr[4] = g;
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 0x48u;
    (void)call_keep(rt, ctx, 0x08B36018u, {g, 0u, 0x48u});
    ctx.gpr[6] = g + 0x28u;
    ctx.gpr[7] = 0x20u;
    ctx.gpr[5] = 0x08C91484u;
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08AD8960u, {self, 0x08C91484u, g + 0x28u, 0x20u});
    ctx.gpr[5] = 0x08C91494u;
    ctx.gpr[4] = self;
    ctx.gpr[6] = g + 0x40u;
    ctx.gpr[7] = 0x20u;
    (void)call_keep(rt, ctx, 0x08AD8960u, {self, 0x08C91494u, g + 0x40u, 0x20u});
    const std::uint32_t first_count = word(rt, g + 0x28u);
    ctx.gpr[19] = 0xFFFFFFFFu;
    ctx.gpr[3] = first_count;
    ctx.gpr[5] = 1u;
    ctx.gpr[4] = first_count * 36u;
    const std::uint32_t first = call_keep(rt, ctx, 0x08AD84B4u, {first_count * 36u, 1u});
    ctx.gpr[16] = g;
    ctx.gpr[8] = g + 0x14u;
    ctx.gpr[4] = g + 0x14u;
    ctx.gpr[5] = first;
    ctx.gpr[3] = g + 4u;
    ctx.gpr[7] = 0x24u;
    put(rt, g + 0x24u, first);  // the delay slot of the branch: it runs on both paths
    std::uint32_t second = 0u;
    bool failed = first == 0u;
    if (!failed) {
        const std::uint32_t count = word(rt, g + 0x28u);
        ctx.gpr[6] = count;
        put(rt, g + 0x10u, 0u);
        put(rt, g + 8u, g + 4u);
        put(rt, g + 0xCu, 0xFFFFFFFFu);
        put(rt, g + 0x18u, g + 0x14u);
        put(rt, g + 0x1Cu, 0xFFFFFFFFu);
        put(rt, g + 0x20u, 0u);
        put(rt, g + 4u, g + 4u);
        put(rt, g + 0x14u, g + 0x14u);  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x08AD8C18u, {g + 0x14u, first, count, 0x24u});
        const std::uint32_t second_count = word(rt, g + 0x40u);
        ctx.gpr[2] = 0x8Cu;
        ctx.gpr[4] = second_count * 0x8Cu;
        ctx.gpr[5] = 1u;
        second = call_keep(rt, ctx, 0x08AD84B4u, {second_count * 0x8Cu, 1u});
        ctx.gpr[3] = g + 0x2Cu;
        ctx.gpr[4] = g + 0x2Cu;
        ctx.gpr[5] = second;
        ctx.gpr[7] = 0x8Cu;
        put(rt, g + 0x3Cu, second);  // the delay slot of the branch: it runs on both paths
        failed = second == 0u;
        if (!failed) {
            const std::uint32_t count_two = word(rt, g + 0x40u);
            ctx.gpr[6] = count_two;
            put(rt, g + 0x34u, 0xFFFFFFFFu);
            put(rt, g + 0x30u, g + 0x2Cu);
            put(rt, g + 0x38u, 0u);
            put(rt, g + 0x2Cu, g + 0x2Cu);  // the delay slot of the jal: it runs before the call
            (void)call_keep(rt, ctx, 0x08AD8C18u, {g + 0x2Cu, second, count_two, 0x8Cu});
            ctx.gpr[2] = 1u;
            ctx.gpr[3] = 1u;
            put(rt, g, 1u);
            return;
        }
    }
    const std::uint32_t held = word(rt, g + 0x3Cu);
    ctx.gpr[2] = held;
    ctx.gpr[4] = held;
    if (held != 0u) {
        (void)call_keep(rt, ctx, 0x08AD85C8u, {held});
    }
    const std::uint32_t block = word(rt, g + 0x24u);
    ctx.gpr[2] = block;
    ctx.gpr[4] = block;
    if (block != 0u) (void)call_keep(rt, ctx, 0x08AD85C8u, {block});
    ctx.gpr[3] = 0u;
    ctx.gpr[2] = 0u;
}

// func_089C6370(this): a block of 0x4D0 bytes (func_08A5BC20(0x4D0, 4)) gets func_089C4180(block, this); a holder at sp + 8 is built from the block pointer stored at sp + 4 (func_08977AE4(sp + 8, sp + 4)) and copied to this + 0x38 with func_08A5BAD8
// (skipped when it is this + 0x38 itself), then destroyed (func_08A5B9EC). func_08A9DD14(sp) fills the holder at sp; its object o (null: func_08A5BB7C(D_08CA6398) first) gives func_08A9BC30(o), which func_08A7C0F0(sp + 0xC, answer) turns into the holder at
// sp + 0xC; sp is destroyed. The object p of sp + 0xC (null: func_08A5BB7C(D_08CA6384) first) is passed to func_08A7CBE4(p, D_08CA63AC, this + 0x3C), then, read again (null: the same assertion), to func_08A7CBE4(p, D_08CA63BC, this + 0x40); sp + 0xC is destroyed. v0 is that last call's.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the three null-object assertion calls (func_08A5BB7C at 0x089C63F8, 0x089C642C and 0x089C6458) never ran and the holder at sp + 8 was never this + 0x38 (0x089C63C8 never branched to 0x089C63D8).
void func_089C6370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder0 = frame.at(0u), value_slot = frame.at(4u), holder8 = frame.at(8u), holderC = frame.at(0xCu);
    ctx.gpr[19] = self;
    ctx.gpr[20] = holderC;
    ctx.gpr[18] = holder8;
    ctx.gpr[5] = 4u;
    ctx.gpr[4] = 0x4D0u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08A5BC20u, {0x4D0u, 4u});
    ctx.gpr[17] = block;
    ctx.gpr[4] = block;
    ctx.gpr[5] = self;
    ctx.gpr[16] = self + 0x38u;  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x089C4180u, {block, self});
    ctx.gpr[4] = holder8;
    ctx.gpr[5] = value_slot;
    put(rt, value_slot, block);  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x08977AE4u, {holder8, value_slot});
    ctx.gpr[4] = self + 0x38u;  // the delay slot of the test: it runs on both paths
    if (self + 0x38u != holder8) {
        const std::uint32_t source = word(rt, holder8);
        ctx.gpr[5] = source;
        (void)call_keep(rt, ctx, 0x08A5BAD8u, {self + 0x38u, source});
    }
    ctx.gpr[4] = holder8;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder8});
    ctx.gpr[4] = holder0;
    (void)call_keep(rt, ctx, 0x08A9DD14u, {holder0});
    std::uint32_t object = word(rt, holder0);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CA6398u;  // the delay slot of the test: it runs on both paths
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA6398u});
    ctx.gpr[4] = object;
    const std::uint32_t answer = call_keep(rt, ctx, 0x08A9BC30u, {object});
    ctx.gpr[5] = answer;
    ctx.gpr[4] = holderC;
    (void)call_keep(rt, ctx, 0x08A7C0F0u, {holderC, answer});
    ctx.gpr[4] = holder0;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder0});
    object = word(rt, holderC);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CA6384u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA6384u});
    ctx.gpr[5] = 0x08CA63ACu;
    ctx.gpr[4] = object;
    ctx.gpr[6] = self + 0x3Cu;
    (void)call_keep(rt, ctx, 0x08A7CBE4u, {object, 0x08CA63ACu, self + 0x3Cu});
    object = word(rt, holderC);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CA6384u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA6384u});
    ctx.gpr[5] = 0x08CA63BCu;
    ctx.gpr[4] = object;
    ctx.gpr[6] = self + 0x40u;
    (void)call_keep(rt, ctx, 0x08A7CBE4u, {object, 0x08CA63BCu, self + 0x40u});
    ctx.gpr[4] = holderC;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holderC});
}

// func_0881CDC4(out, a1, count, a3): a count of 0 or less returns (v0 untouched). Otherwise a holder at sp + 8 is filled by func_0881BD98, its object o (null: func_08A5BB7C(D_08C98F48) first) gives s = func_0881B068(o), and the holder is destroyed (func_08A5B9EC);
// func_0881BF5C(s, count, a3, sp + 0x10, sp + 0xC) fills the words sp + 0x10, sp + 0x14 and sp + 0xC. When the first two are both -1: func_08A97A6C(sp + 4, 0) builds a holder at sp + 4; otherwise r = func_0881B758(s, ., x, y, z) is taken, func_0881AC84(s, ., x, y, z) runs
// (x, y, z the three words read again) and func_08A97A6C(sp, r) builds the holder at sp. The holder (sp + 4 or sp) is copied to `out` with func_08A5BAD8 unless it is `out`, and destroyed with func_08A5B9EC; v0 is that call's.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): a count of 0 or less (0x0881CDEC), a null object (the assertion at 0x0881CE0C), the pair x = y = -1 (the blank path 0x0881CE58-0x0881CE84) and `out` being the holder at sp (0x0881CEB8) never occurred: only the main path from 0x0881CE88 was compared.
void func_0881CDC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], third = ctx.gpr[6], fourth = ctx.gpr[7];
    const std::int32_t count = signed_of(ctx.gpr[6]);
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = out;
    ctx.gpr[19] = fourth;
    ctx.gpr[18] = third;
    if (count <= 0) return;
    const std::uint32_t second = frame.at(8u), words = frame.at(0x10u), word_c = frame.at(0xCu);
    ctx.gpr[20] = second;
    ctx.gpr[4] = second;
    (void)call_keep(rt, ctx, 0x0881BD98u, {second});
    const std::uint32_t object = word(rt, second);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C98F48u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    ctx.gpr[4] = object;
    const std::uint32_t handle = call_keep(rt, ctx, 0x0881B068u, {object});
    ctx.gpr[17] = handle;
    ctx.gpr[4] = second;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
    ctx.gpr[6] = fourth;
    ctx.gpr[7] = words;
    ctx.gpr[4] = handle;
    ctx.gpr[5] = third;
    ctx.gpr[8] = word_c;  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x0881BF5Cu, {handle, third, fourth, words, word_c});
    const std::uint32_t x = word(rt, words), y = word(rt, words + 4u);
    ctx.gpr[6] = x;
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (x == 0xFFFFFFFFu && y == 0xFFFFFFFFu) {
        const std::uint32_t holder = frame.at(4u);
        ctx.gpr[16] = holder;
        ctx.gpr[4] = holder;
        ctx.gpr[5] = 0u;
        (void)call_keep(rt, ctx, 0x08A97A6Cu, {holder, 0u});
        if (out != holder) {
            const std::uint32_t source = word(rt, holder);
            ctx.gpr[5] = source;
            ctx.gpr[4] = out;
            (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, source});
        }
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[7] = y;
    ctx.gpr[8] = word(rt, word_c);
    ctx.gpr[4] = handle;
    const std::uint32_t result = call_keep(rt, ctx, 0x0881B758u, {handle, ctx.gpr[5], x, y, word(rt, word_c)});
    const std::uint32_t second_x = word(rt, words), second_y = word(rt, words + 4u), second_z = word(rt, word_c);
    ctx.gpr[6] = second_x;
    ctx.gpr[7] = second_y;
    ctx.gpr[8] = second_z;
    ctx.gpr[16] = result;
    ctx.gpr[4] = handle;
    (void)call_keep(rt, ctx, 0x0881AC84u, {handle, ctx.gpr[5], second_x, second_y, second_z});
    const std::uint32_t holder = frame.at(0u);
    ctx.gpr[4] = holder;
    ctx.gpr[5] = result;
    (void)call_keep(rt, ctx, 0x08A97A6Cu, {holder, result});
    if (out != holder) {
        const std::uint32_t source = word(rt, holder);
        ctx.gpr[5] = source;
        ctx.gpr[4] = out;
        (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, source});
    }
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08BA9498(vector, destination, first): n = (vector[4] - first) >> 3 elements of 8 bytes (a word and a pointer to a reference-counted object, as in assign_counted: strong counter at +4, weak counter at +8) are moved down from `first` to `destination`: for each
// element the word is copied; when the pointer differs from the destination's, the new object (non-null) has its strong counter incremented, the old one (non-null) has its strong counter decremented (when it was 1 the vtable word at offset 8 is called, then the weak counter is
// decremented and when it was 1 the one at offset 0xC is), and the pointer is stored. Then func_08BA8F6C(destination end, vector[4], sp) and vector[4] -= (first - destination) rounded down to a multiple of 8; v0 = destination.
// Branches the routes do not take: not measured (not registered): no compared case changed memory, so whether any element was moved is not known; the element loop (0x08BA94EC-0x08BA9598) and the counter releases in it are read from the asm only.
[[maybe_unused]] void func_08BA9498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], destination = ctx.gpr[5], first = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    ctx.gpr[30] = vector;
    ctx.gpr[23] = destination;
    ctx.gpr[17] = destination;
    const std::uint32_t end = word(rt, vector + 4u);
    ctx.gpr[3] = end;
    ctx.gpr[2] = end - first;
    const std::int32_t count = signed_of(end - first) >> 3;
    ctx.gpr[22] = static_cast<std::uint32_t>(count);
    put(rt, frame.at(0x10u), first);  // the delay slot of the branch: it runs on both paths
    std::uint32_t cursor = destination, source = first, done = 0u;
    if (count > 0) {
        ctx.gpr[19] = first;
        ctx.gpr[21] = 0u;
        do {
            const std::uint32_t item = word(rt, source), current = word(rt, cursor + 4u);
            ctx.gpr[2] = item;
            ctx.gpr[3] = current;
            put(rt, cursor, item);
            const std::uint32_t incoming = word(rt, source + 4u);
            ctx.gpr[18] = incoming;
            ctx.gpr[20] = cursor + 4u;
            if (incoming != current) {
                if (incoming != 0u) atomic_increment(rt, ctx, incoming + 4u);
                const std::uint32_t old = word(rt, cursor + 4u);
                ctx.gpr[16] = old;
                if (old != 0u) {
                    ctx.gpr[3] = old + 4u;
                    if (atomic_decrement(rt, ctx, old + 4u) == 1u) {
                        const std::uint32_t vtable = word(rt, old), function = word(rt, vtable + 8u);
                        ctx.gpr[3] = vtable;
                        ctx.gpr[2] = function;
                        ctx.gpr[4] = old;
                        (void)call_keep(rt, ctx, function, {old});
                        if (atomic_decrement(rt, ctx, old + 8u) == 1u) {
                            const std::uint32_t weak_vtable = word(rt, old), weak_function = word(rt, weak_vtable + 0xCu);
                            ctx.gpr[3] = weak_vtable;
                            ctx.gpr[2] = weak_function;
                            ctx.gpr[4] = old;
                            (void)call_keep(rt, ctx, weak_function, {old});
                        }
                    }
                }
                put(rt, cursor + 4u, incoming);
            }
            ++done;
            ctx.gpr[21] = done;
            source += 8u;
            ctx.gpr[19] = source;
            cursor += 8u;  // the delay slot of the loop branch: it runs on both paths
            ctx.gpr[17] = cursor;
        } while (static_cast<std::uint32_t>(count) != done);
        ctx.gpr[3] = word(rt, vector + 4u);
    }
    ctx.gpr[4] = cursor;
    ctx.gpr[5] = ctx.gpr[3];
    ctx.gpr[6] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08BA8F6Cu, {cursor, ctx.gpr[3], frame.at(0u)});
    const std::uint32_t saved_first = word(rt, frame.at(0x10u)), vector_end = word(rt, vector + 4u);
    ctx.gpr[4] = saved_first;
    std::uint32_t moved = saved_first - destination;
    moved &= ~7u;
    ctx.gpr[2] = moved;
    ctx.gpr[3] = vector_end - moved;
    put(rt, vector + 4u, vector_end - moved);
    ctx.gpr[2] = destination;
}

// func_0888A9B8(this, index, source, a3, t0, t1, flag): with e = this + 168 * index + 0xD78: func_08B7C830(e, source, 0x10) copies 16 bytes into e; e[0x18] = t0, e[0x1C] = t1, the halfword e[0x14] = 0xFFFF, the bytes e[0x11], [0x12], [0x13] = 0; with k = the byte this[0x1C35]
// (0: 0x481907FF, else -1) e[0x20] = k's value and e[0x24] = 0, and for 16 words e[0x28 + 4i] = the same value and e[0x68 + 4i] = 0. A zero `flag` (low byte) returns. Otherwise func_0888A878(this + 168 * index + 0x1018, e) and func_0888A878(this + 168 * index + 0x12B8, e) run,
// the halfwords at p = this + 432 * index + 0x1560 ([p], [p + 2], [p + 4]) are 0 and the 100 words p[0x1C + 4i] = the value (0x481907FF when this[0x1C35] is 0, else -1). v0 is that value (the last movz), v1 the last high-half-plus-0x7FF word.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the byte this[0x1C35] was always 0 (0x0888AA44 never branched, and the movz at 0x0888AA7C and 0x0888AB0C always chose 0x481907FF), so the words stored as -1 are read from the asm only; the flag was never 0 (0x0888AA94 never branched to the return).
void func_0888A9B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5], source = ctx.gpr[6], third = ctx.gpr[8], fourth = ctx.gpr[9], flag = ctx.gpr[10] & 0xFFu;
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t entry = self + 168u * index + 0xD78u;
    ctx.gpr[23] = index << 5;
    ctx.gpr[22] = index << 3;
    ctx.gpr[19] = entry;
    ctx.gpr[20] = index;
    ctx.gpr[18] = self;
    ctx.gpr[21] = flag;
    ctx.gpr[17] = fourth;
    ctx.gpr[16] = third;  // the delay slot of the jal: it runs before the call
    ctx.gpr[4] = entry;
    ctx.gpr[5] = source;
    ctx.gpr[6] = 0x10u;
    (void)call_keep(rt, ctx, 0x08B7C830u, {entry, source, 0x10u});
    put(rt, entry + 0x18u, third);
    put(rt, entry + 0x1Cu, fourth);
    put16(rt, entry + 0x14u, 0xFFFFu);
    put8(rt, entry + 0x11u, 0u);
    put8(rt, entry + 0x12u, 0u);
    put8(rt, entry + 0x13u, 0u);
    constexpr std::uint32_t special = 0x481907FFu;
    const auto value_of = [&]() { return byte_u(rt, self + 0x1C35u) == 0u ? special : 0xFFFFFFFFu; };
    ctx.gpr[4] = byte_u(rt, self + 0x1C35u) != 0u ? 0xFFFFFFFFu : special;
    put(rt, entry + 0x24u, 0u);
    put(rt, entry + 0x20u, ctx.gpr[4]);
    std::uint32_t value = 0u;
    for (std::uint32_t slot = 0u; slot < 0x10u; ++slot) {
        ctx.gpr[4] = byte_u(rt, self + 0x1C35u);
        value = value_of();
        put(rt, entry + 0x28u + 4u * slot, value);
        put(rt, entry + 0x68u + 4u * slot, 0u);
    }
    ctx.gpr[8] = 0x48190000u;
    ctx.gpr[3] = special;
    ctx.gpr[2] = value;
    if (flag == 0u) return;
    const std::uint32_t row = 168u * index + self;
    ctx.gpr[16] = row;
    ctx.gpr[4] = row + 0x1018u;
    ctx.gpr[5] = entry;
    (void)call_keep(rt, ctx, 0x0888A878u, {row + 0x1018u, entry});
    ctx.gpr[4] = row + 0x12B8u;
    ctx.gpr[5] = entry;
    (void)call_keep(rt, ctx, 0x0888A878u, {row + 0x12B8u, entry});
    const std::uint32_t block = self + 432u * index + 0x1560u;
    put16(rt, block, 0u);
    put16(rt, block + 2u, 0u);
    put16(rt, block + 4u, 0u);
    for (std::uint32_t slot = 0u; slot < 100u; ++slot) {
        ctx.gpr[4] = byte_u(rt, self + 0x1C35u);
        value = value_of();
        put(rt, block + 0x1Cu + 4u * slot, value);
    }
    ctx.gpr[9] = 0x48190000u;
    ctx.gpr[3] = special;
    ctx.gpr[2] = value;
}

// func_0889F1DC(this, a1, p, q, flag_a, flag_b): this[0] = D_08CC2420; the two four-word records p and q are copied to this + 4 and this + 0x14 (words 0 and 2 plain, words 1 and 3 are pointers to reference-counted objects as in assign_counted: stored, and a non-null
// pointer has its strong counter at +4 incremented; the pointer words are stored in the delay slot of the test, so also when null). Then b = func_08B62344(0x3C) with func_08B28D34(b) (the pointer s5 = this + 0x24 is set in the delay slot of that call), b[0x2C], b[0x34], b[0x38] = 0,
// the byte b[0x30] = 0, this[0x24] = b (before the next call) and func_0889C2C0(this + 0x28, a1, p, q, flag_a & 0xFF, this + 0x24), func_08912E08(this + 0x30). A non-zero flag_b (low byte) returns; otherwise the function ends with a tail call to
// func_08911D80(this + 0x30, this + 0x24, p, a1). v0 is the last call's.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): none: coverage shows every branch outcome taken.
void func_0889F1DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], p = ctx.gpr[6], q = ctx.gpr[7], flag_a = ctx.gpr[8] & 0xFFu, flag_b = ctx.gpr[9] & 0xFFu;
    bool returned = false;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegs saved(ctx);
        ctx.gpr[2] = 0x08CC2420u;
        ctx.gpr[30] = flag_b;
        ctx.gpr[23] = first;
        ctx.gpr[22] = flag_a;
        ctx.gpr[19] = p;
        ctx.gpr[18] = q;
        ctx.gpr[17] = self;
        put(rt, self, 0x08CC2420u);
        const auto copy_record = [&](std::uint32_t source, std::uint32_t target) {
            put(rt, target, word(rt, source));
            std::uint32_t pointer = word(rt, source + 4u);
            ctx.gpr[2] = pointer;
            put(rt, target + 4u, pointer);
            if (pointer != 0u) atomic_increment(rt, ctx, pointer + 4u);
            put(rt, target + 8u, word(rt, source + 8u));
            pointer = word(rt, source + 0xCu);
            ctx.gpr[2] = pointer;
            put(rt, target + 0xCu, pointer);
            if (pointer != 0u) atomic_increment(rt, ctx, pointer + 4u);
        };
        ctx.gpr[4] = self + 4u;
        copy_record(p, self + 4u);
        ctx.gpr[4] = self + 0x14u;
        copy_record(q, self + 0x14u);
        ctx.gpr[4] = 0x3Cu;
        const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x3Cu});
        ctx.gpr[16] = block;
        ctx.gpr[4] = block;
        ctx.gpr[21] = self + 0x24u;  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x08B28D34u, {block});
        ctx.gpr[20] = self + 0x30u;
        put(rt, block + 0x2Cu, 0u);
        put8(rt, block + 0x30u, 0u);
        put(rt, block + 0x34u, 0u);
        put(rt, block + 0x38u, 0u);
        ctx.gpr[4] = self + 0x28u;
        ctx.gpr[7] = q;
        ctx.gpr[8] = flag_a;
        ctx.gpr[5] = first;
        ctx.gpr[6] = p;
        ctx.gpr[9] = self + 0x24u;
        put(rt, self + 0x24u, block);  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x0889C2C0u, {self + 0x28u, first, p, q, flag_a, self + 0x24u});
        ctx.gpr[4] = self + 0x30u;
        (void)call_keep(rt, ctx, 0x08912E08u, {self + 0x30u});
        if (flag_b != 0u) {
            returned = true;
        } else {
            ctx.gpr[4] = self + 0x30u;
            ctx.gpr[5] = self + 0x24u;
            ctx.gpr[6] = p;
            ctx.gpr[7] = first;
        }
    }
    if (!returned) tail_call(ctx, 0x08911D80u, {self + 0x30u, self + 0x24u, p, first});
}

// func_08BE449C(out, a1, a2, a3): func_08BE43B0(sp) fills the record at sp; o = func_08B62344(0x6C) is built with func_08962918(o, sp, a2, a3) and out[0] = o (stored in the delay slot of the next call); a control block c = func_08B62344(0x10) gets c[4] = 1,
// c[0] = D_08CC6940, c[8] = 1, c[0xC] = o and out[4] = c (the last in the delay slot of the test, so also for a null c). A null c releases o first (when non-null: func_08962A20(o, o) and func_08B64654(o)). Then func_08B8E380(out + 4, o, o) runs and the record's pointer
// p = sp[4] (when non-null) has its strong counter at +4 decremented: when it was 1 the vtable word at offset 8 is called, then its weak counter at +8 is decremented and when that was 1 the one at offset 0xC is. v0 is 1 after a counter step that did not lead to a call, the call's v0 otherwise.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): neither allocation ever failed (the control block c was never null: 0x08BE4510 always branched to 0x08BE4530, so the release of o at 0x08BE4518-0x08BE452C never ran), the pointer sp[4] was never null (0x08BE4544) and its strong counter was never 1 (0x08BE4560 always branched to 0x08BE45AC), so the virtual calls and the weak step (0x08BE4568-0x08BE45A4) are read from the asm only.
void func_08BE449C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], third = ctx.gpr[6], fourth = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t record = frame.at(0u);
    ctx.gpr[19] = out;
    ctx.gpr[4] = record;
    ctx.gpr[17] = fourth;
    ctx.gpr[16] = third;  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x08BE43B0u, {record});
    ctx.gpr[4] = 0x6Cu;
    const std::uint32_t object = call_keep(rt, ctx, 0x08B62344u, {0x6Cu});
    ctx.gpr[18] = object;
    ctx.gpr[4] = object;
    ctx.gpr[5] = record;
    ctx.gpr[6] = third;
    ctx.gpr[7] = fourth;
    (void)call_keep(rt, ctx, 0x08962918u, {object, record, third, fourth});
    ctx.gpr[4] = 0x10u;
    put(rt, out, object);  // the delay slot of the jal: it runs before the call
    const std::uint32_t control = call_keep(rt, ctx, 0x08B62344u, {0x10u});
    ctx.gpr[3] = 0x08CC6940u;
    ctx.gpr[4] = 1u;
    put(rt, control + 4u, 1u);
    put(rt, control, 0x08CC6940u);
    put(rt, control + 8u, 1u);
    put(rt, control + 0xCu, object);
    put(rt, out + 4u, control);  // the delay slot of the test: it runs on both paths
    if (control == 0u) {
        if (object != 0u) {
            ctx.gpr[4] = object;
            ctx.gpr[5] = object;  // the delay slot of the jal: it runs before the call
            (void)call_keep(rt, ctx, 0x08962A20u, {object, object});
            ctx.gpr[4] = object;
            (void)call_keep(rt, ctx, 0x08B64654u, {object});
        }
    }
    ctx.gpr[5] = object;
    ctx.gpr[4] = out + 4u;
    ctx.gpr[6] = object;
    (void)call_keep(rt, ctx, 0x08B8E380u, {out + 4u, object, object});
    const std::uint32_t pointer = word(rt, record + 4u);
    ctx.gpr[16] = pointer;
    if (pointer == 0u) return;
    if (atomic_decrement(rt, ctx, pointer + 4u) != 1u) return;
    const std::uint32_t vtable = word(rt, pointer), function = word(rt, vtable + 8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = pointer;
    (void)call_keep(rt, ctx, function, {pointer});
    if (atomic_decrement(rt, ctx, pointer + 8u) != 1u) return;
    const std::uint32_t weak_vtable = word(rt, pointer), weak_function = word(rt, weak_vtable + 0xCu);
    ctx.gpr[3] = weak_vtable;
    ctx.gpr[2] = weak_function;
    ctx.gpr[4] = pointer;
    (void)call_keep(rt, ctx, weak_function, {pointer});
}

// func_089B139C(this): the object o = this[0xBC] (when non-null) has its vtable word at offset 0x164 called with (o); then the three vectors {begin, end, capacity} at this + 0x18, + 0x24 and + 0x30 are emptied in turn: the three words are read and cleared, spilled to
// the frame, func_08BF8B48(begin, end, sp + 2 / sp + 1 / sp) runs on the old range and a non-null old block is freed with func_08A5BC54(block, (capacity - block) rounded down to a multiple of 4, 4). v0 is the last free's (or the begin of the last vector when nothing is freed).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): this[0xBC] was never null (0x089B13B4 never branched) and no vector was without a block (0x089B1418, 0x089B1474 and 0x089B14D0 never branched past the free).
void func_089B139C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[5] = self;
    ctx.gpr[16] = self;
    const std::uint32_t object = word(rt, self + 0xBCu);
    ctx.gpr[2] = object;
    ctx.gpr[4] = object;  // the delay slot of the test: it runs on both paths
    if (object != 0u) {
        const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 0x164u);
        ctx.gpr[2] = vtable;
        ctx.gpr[3] = function;
        (void)call_keep(rt, ctx, function, {object, self});
    }
    const struct { std::uint32_t member, spill, scratch; } vectors[] = {{0x18u, 0x1Cu, 2u}, {0x24u, 0x10u, 1u}, {0x30u, 0x4u, 0u}};
    for (const auto &vector : vectors) {
        const std::uint32_t begin = word(rt, self + vector.member), end = word(rt, self + vector.member + 4u), capacity = word(rt, self + vector.member + 8u);
        put(rt, self + vector.member, 0u);
        put(rt, self + vector.member + 4u, 0u);
        put(rt, self + vector.member + 8u, 0u);
        ctx.gpr[2] = begin;
        ctx.gpr[3] = end;
        ctx.gpr[7] = capacity;
        put(rt, frame.at(vector.spill), begin);
        put(rt, frame.at(vector.spill + 8u), capacity);
        put(rt, frame.at(vector.spill + 4u), end);  // the delay slot of the jal: it runs before the call
        ctx.gpr[4] = begin;
        ctx.gpr[5] = end;
        ctx.gpr[6] = frame.at(vector.scratch);
        (void)call_keep(rt, ctx, 0x08BF8B48u, {begin, end, frame.at(vector.scratch)});
        const std::uint32_t block = word(rt, frame.at(vector.spill)), limit = word(rt, frame.at(vector.spill + 8u));
        ctx.gpr[6] = 4u;
        ctx.gpr[2] = block;
        ctx.gpr[4] = block;
        const std::uint32_t bytes = static_cast<std::uint32_t>(signed_of(limit - block) >> 2) << 2;
        ctx.gpr[5] = bytes;
        if (block != 0u) (void)call_keep(rt, ctx, 0x08A5BC54u, {block, bytes, 4u});
    }
}

// func_08C070FC(this, other): copy assignment of a vector of 16-byte elements {begin this[0], end this[4], capacity end this[8]}; other == this returns this. With n = the element count of `other`:
//   - n above this's capacity: a new block of 16 * n bytes comes from func_08B35840(0x10, bytes), func_08C0672C(other.begin, other.end, block, sp) copies the elements into it, func_08C06684(this[0], this[4], sp + 3) destroys the old ones, a non-null old block is freed
//     (func_08B35254), this[8] = block + 16 * n, this[0] = block;
//   - n not above this's size: the n elements are assigned one by one (func_089641C4(destination, source)) and func_08C06560(the end of them, this[4], sp + 2) destroys the rest;
//   - otherwise the first `size` elements are assigned one by one and func_08C0672C(other.begin + (size * 16), other.end, this[4], sp + 1) copy-constructs the rest.
// Then this[4] = this[0] + 16 * n; v0 = this.
// Branches the routes do not take: not measured (not registered): no compared case changed memory; the three paths (a new block, assigning n elements, assigning the old size and constructing the rest) are read from the asm only.
[[maybe_unused]] void func_08C070FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = other;
    ctx.gpr[19] = self;
    if (other == self) {
        ctx.gpr[2] = self;
        return;
    }
    const std::uint32_t capacity_end = word(rt, self + 8u);
    ctx.gpr[18] = word(rt, other + 4u);
    ctx.gpr[16] = word(rt, other);
    std::uint32_t source = word(rt, other), source_end = word(rt, other + 4u);
    std::uint32_t begin = word(rt, self);
    ctx.gpr[4] = begin;
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(source_end - source) >> 4);
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(capacity_end - begin) >> 4);
    ctx.gpr[20] = count;
    ctx.gpr[3] = source_end - source;
    ctx.gpr[2] = capacity < count ? 1u : 0u;
    std::uint32_t bytes = 0u;
    if (capacity < count) {
        bytes = count << 4;
        ctx.gpr[20] = bytes;
        ctx.gpr[4] = 0x10u;
        ctx.gpr[5] = bytes;
        const std::uint32_t block = call_keep(rt, ctx, 0x08B35840u, {0x10u, bytes});
        ctx.gpr[6] = block;
        ctx.gpr[4] = source;
        ctx.gpr[5] = source_end;
        ctx.gpr[7] = frame.at(0u);
        ctx.gpr[17] = block;  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x08C0672Cu, {source, source_end, block, frame.at(0u)});
        ctx.gpr[4] = word(rt, self);
        ctx.gpr[5] = word(rt, self + 4u);
        ctx.gpr[6] = frame.at(3u);
        (void)call_keep(rt, ctx, 0x08C06684u, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
        const std::uint32_t old = word(rt, self);
        ctx.gpr[4] = old;
        ctx.gpr[2] = block + bytes;  // the delay slot of the test: it runs on both paths
        if (old != 0u) {
            (void)call_keep(rt, ctx, 0x08B35254u, {old});
            ctx.gpr[2] = block + bytes;
        }
        ctx.gpr[5] = bytes;
        put(rt, self + 8u, block + bytes);
        put(rt, self, block);
    } else {
        std::uint32_t end = word(rt, self + 4u);
        ctx.gpr[6] = end;
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 4);
        ctx.gpr[2] = size;
        ctx.gpr[3] = size < count ? 1u : 0u;
        ctx.gpr[22] = size;
        std::uint32_t destination = begin, step = 0u;
        const auto assign_one = [&]() {
            ctx.gpr[4] = destination;
            ctx.gpr[5] = source;
            ctx.gpr[17] = destination;
            ctx.gpr[16] = source;
            ++step;
            ctx.gpr[18] = step;  // the delay slot of the jal: it runs before the call
            (void)call_keep(rt, ctx, 0x089641C4u, {destination, source});
            source += 0x10u;
            ctx.gpr[16] = source;
            destination += 0x10u;
            ctx.gpr[17] = destination;
        };
        if (!(size < count)) {
            ctx.gpr[17] = begin;
            if (signed_of(count) > 0) {
                ctx.gpr[18] = 0u;
                while (step != count) assign_one();
                end = word(rt, self + 4u);
            }
            ctx.gpr[5] = end;
            ctx.gpr[4] = destination;
            ctx.gpr[6] = frame.at(2u);
            (void)call_keep(rt, ctx, 0x08C06560u, {destination, end, frame.at(2u)});
            bytes = count << 4;
            ctx.gpr[5] = bytes;
        } else {
            ctx.gpr[17] = begin;
            if (signed_of(size) > 0) {
                ctx.gpr[18] = 0u;
                while (step != size) assign_one();
                source = word(rt, other);
                source_end = word(rt, other + 4u);
                ctx.gpr[16] = source;
                ctx.gpr[18] = source_end;
                begin = word(rt, self);
                end = word(rt, self + 4u);
            }
            const std::uint32_t skip = (end - begin) & ~0xFu;
            ctx.gpr[4] = source + skip;
            ctx.gpr[5] = source_end;
            ctx.gpr[6] = end;
            ctx.gpr[7] = frame.at(1u);
            (void)call_keep(rt, ctx, 0x08C0672Cu, {source + skip, source_end, end, frame.at(1u)});
            bytes = count << 4;
            ctx.gpr[5] = bytes;
        }
    }
    const std::uint32_t final_end = word(rt, self) + bytes;
    ctx.gpr[2] = final_end;
    put(rt, self + 4u, final_end);
    ctx.gpr[2] = self;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// refresh_then(final): func_0898A5F8 and func_0898A7A8 (108 instructions, one function twice; they differ only in the call they end with, the row holds it: func_089660A0 / func_089699D8).
//
// func_X(this, a1), with r = this + 0xF0 and h = the word at D_08CF68E8: when the byte r[0xC4] (this[0x1B4]) is non-zero: r[0xBC] = r[0x40], r[0xC0] = r[0x44] (floats), r[0xC4] = 0, func_08983B2C(r, h, this) runs, s = this[0x1E0] (null: func_08A5BB7C(D_08CA39B4) first)
// is turned into a holder at sp + 4 by func_089CC040, func_08B855E0(this + 0x1F4, holder) takes it over and the holder's text t (the header at t - 0xC; the shared empty header D_08D74FD0 excepted) is released: func_08B60804(t - 4, -1) and, when that does not answer a positive value,
// func_08B8542C(t - 0xC, sp); func_08A4D79C(h, this[0x1F4], 0, 0, 1) runs, this[0x1EC] = h[0x34] and this[0x1F0] = h[0x38] (floats, h read again each time); when the byte this[0x1E9] is non-zero func_0896A35C(this, 0, 0, sp + 8, sp + 0xC) fills two floats (x, y) and
// r[0xBC] = (x / this[0x1EC]) * r[0x40] when x < this[0x1EC], r[0xC0] = (y / this[0x1F0]) * r[0x44] when y < this[0x1F0]. Then func_08988F24(this, 1). In every case the function ends with final(this, a1).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS; coverage of both members together, addresses of func_0898A5F8): this[0x1E0] was never null (the assertion call at 0x0898A668), func_08B60804 never answered a non-positive value (func_08B8542C at 0x0898A6BC never ran) and the byte this[0x1E9] was always 0, so the float part (func_0896A35C, the two comparisons, divisions and products, 0x0898A700-0x0898A764) is read from the asm only.
inline void refresh_then(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t final_call) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t handle_slot = 0x08CF68E8u, empty_body = 0x08D74FD0u;
    const std::uint32_t region = self + 0xF0u;
    ctx.gpr[22] = argument;
    ctx.gpr[18] = region;
    const std::uint32_t pending = byte_u(rt, region + 0xC4u);
    ctx.gpr[2] = pending;
    ctx.gpr[17] = self;  // the delay slot of the test: it runs on both paths
    if (pending != 0u) {
        const float first = float_at(rt, region + 0x40u), second = float_at(rt, region + 0x44u);
        ctx.fpr[0] = first;
        ctx.fpr[1] = second;
        put8(rt, region + 0xC4u, 0u);
        ctx.gpr[21] = high_half(handle_slot);
        put_float(rt, region + 0xBCu, first);
        ctx.gpr[20] = self + 0x1F4u;
        put_float(rt, region + 0xC0u, second);
        ctx.gpr[4] = region;
        ctx.gpr[6] = self;
        ctx.gpr[5] = word(rt, handle_slot);  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x08983B2Cu, {region, word(rt, handle_slot), self});
        const std::uint32_t source = word(rt, self + 0x1E0u);
        ctx.gpr[19] = source;
        const std::uint32_t holder = frame.at(4u);
        ctx.gpr[16] = holder;  // the delay slot of the test: it runs on both paths
        if (source == 0u) {
            ctx.gpr[4] = 0x08CA39B4u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA39B4u});
        }
        ctx.gpr[4] = holder;
        ctx.gpr[5] = source;
        (void)call_keep(rt, ctx, 0x089CC040u, {holder, source});
        ctx.gpr[4] = self + 0x1F4u;
        ctx.gpr[5] = holder;
        (void)call_keep(rt, ctx, 0x08B855E0u, {self + 0x1F4u, holder});
        const std::uint32_t text = word(rt, holder), body = text - 0xCu;
        ctx.gpr[4] = text;
        ctx.gpr[2] = empty_body;
        ctx.gpr[16] = body;
        if (body != empty_body) {
            ctx.gpr[4] = text - 4u;
            ctx.gpr[5] = 0xFFFFFFFFu;
            const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
            if (!(signed_of(count) > 0)) {
                ctx.gpr[4] = body;
                ctx.gpr[5] = frame.at(0u);
                (void)call_keep(rt, ctx, 0x08B8542Cu, {body, frame.at(0u)});
            }
        }
        ctx.gpr[4] = word(rt, handle_slot);
        ctx.gpr[5] = word(rt, self + 0x1F4u);
        ctx.gpr[6] = 0u;
        ctx.gpr[7] = 0u;
        ctx.gpr[8] = 1u;
        (void)call_keep(rt, ctx, 0x08A4D79Cu, {word(rt, handle_slot), word(rt, self + 0x1F4u), 0u, 0u, 1u});
        const std::uint32_t flag = byte_u(rt, self + 0x1E9u);
        ctx.gpr[3] = flag;
        float limit = float_at(rt, word(rt, handle_slot) + 0x34u);
        ctx.fpr[0] = limit;
        put_float(rt, self + 0x1ECu, limit);
        limit = float_at(rt, word(rt, handle_slot) + 0x38u);
        ctx.fpr[0] = limit;
        put_float(rt, self + 0x1F0u, limit);  // the delay slot of the test: it runs on both paths
        if (flag != 0u) {
            ctx.gpr[4] = self;
            ctx.gpr[5] = 0u;
            ctx.gpr[6] = 0u;
            ctx.gpr[7] = frame.at(8u);
            ctx.gpr[8] = frame.at(0xCu);
            (void)call_keep(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, frame.at(8u), frame.at(0xCu)});
            float divisor = float_at(rt, self + 0x1ECu), value = float_at(rt, frame.at(8u));
            ctx.fpr[1] = divisor;
            ctx.fpr[0] = value;
            if (value < divisor) {
                value = value / divisor;
                divisor = float_at(rt, region + 0x40u);
                value = psp_mul(value, divisor);
                ctx.fpr[1] = divisor;
                ctx.fpr[0] = value;
                put_float(rt, region + 0xBCu, value);
            }
            divisor = float_at(rt, self + 0x1F0u);
            value = float_at(rt, frame.at(0xCu));
            ctx.fpr[1] = divisor;
            ctx.fpr[0] = value;
            ctx.gpr[4] = self;
            if (value < divisor) {
                value = value / divisor;
                divisor = float_at(rt, region + 0x44u);
                value = psp_mul(value, divisor);
                ctx.fpr[1] = divisor;
                ctx.fpr[0] = value;
                put_float(rt, region + 0xC0u, value);
            }
        }
        ctx.gpr[4] = self;
        ctx.gpr[5] = 1u;
        (void)call_keep(rt, ctx, 0x08988F24u, {self, 1u});
    }
    ctx.gpr[4] = self;
    ctx.gpr[5] = argument;
    (void)call_keep(rt, ctx, final_call, {self, argument});
}
void func_0898A5F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { refresh_then(rt, ctx, 0x089660A0u); }
void func_0898A7A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { refresh_then(rt, ctx, 0x089699D8u); }

// func_08B03A48(p): o = p[0] and four tables of o are reset when they are full, i.e. when the count word equals the number of elements between the table's begin and end: [o[0], o[4]) with 240-byte elements against o[0x10] (func_08C49A28(o, o[0], o[4])),
// [o[0x44], o[0x48]) with 8-byte elements against o[0x54] (func_08C49B00(o + 0x44, ...)), [o[0x60], o[0x64]) with 84-byte elements against o[0x70] (func_08C49C28(o + 0x60, ...)) and [o[0x7C], o[0x80]) with 6-byte elements against o[0x8C] (a tail call
// to func_08C49D08(o + 0x7C, ...)). The element counts are the arithmetic shift of the byte difference times the modular inverse of the odd part of the element size (shift 4 and 0xEEEEEEEF for 240 bytes, shift 3 alone for 8, shift 2 and 0x3CF3CF3D for 84, shift 1 and 0xAAAAAAAB for 6: a 32-bit mult, low word). A reset stores 0xFFFF in the halfwords
// at group + 2 and group + 0 and 0 in the count word group + 4 (group = o + 0xC, o + 0x50, o + 0x6C and o + 0x88 for the four tables), then runs the callee.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the fourth table was never full (0x08B03B5C always branched to 0x08B03B88), so its reset and the tail call to func_08C49D08 (0x08B03B60-0x08B03B84) never ran; the first three tables took both outcomes in the single compared case.
void func_08B03A48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pointer = ctx.gpr[4];
    bool tail = false;
    std::uint32_t tail_object = 0u, tail_begin = 0u, tail_end = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[7] = 0xFFFFFFFFu;
        const std::uint32_t object = word(rt, pointer);
        ctx.gpr[16] = object;
        const struct Stage {
            std::uint32_t begin, group, shift, inverse, callee;
        } stages[] = {{0x00u, 0x0Cu, 4u, 0xEEEEEEEFu, 0x08C49A28u}, {0x44u, 0x50u, 3u, 0u, 0x08C49B00u}, {0x60u, 0x6Cu, 2u, 0x3CF3CF3Du, 0x08C49C28u}, {0x7Cu, 0x88u, 1u, 0xAAAAAAABu, 0x08C49D08u}};
        for (unsigned index = 0u; index < 4u; ++index) {
            const Stage &stage = stages[index];
            const std::uint32_t begin = word(rt, object + stage.begin), end = word(rt, object + stage.begin + 4u), counter = word(rt, object + stage.group + 4u);
            std::uint32_t elements = static_cast<std::uint32_t>(signed_of(end - begin) >> stage.shift);
            if (stage.inverse != 0u) elements *= stage.inverse;
            ctx.gpr[2] = stage.inverse != 0u ? stage.inverse : elements;
            ctx.gpr[3] = elements;
            ctx.gpr[4] = object + (index == 0u ? 0u : stage.begin);  // the delay slot of the test (the first stage) or the first instruction of the stage
            if (counter != elements) continue;
            put16(rt, object + stage.group + 2u, 0xFFFFu);
            put(rt, object + stage.group + 4u, 0u);
            put16(rt, object + stage.group, 0xFFFFu);
            const std::uint32_t reset_begin = word(rt, object + stage.begin), reset_end = word(rt, object + stage.begin + 4u);
            ctx.gpr[5] = reset_begin;
            ctx.gpr[6] = reset_end;
            ctx.gpr[4] = object + stage.begin;
            if (index == 3u) {
                tail = true;
                tail_object = object + stage.begin;
                tail_begin = reset_begin;
                tail_end = reset_end;
                break;
            }
            (void)call_keep(rt, ctx, stage.callee, {object + stage.begin, reset_begin, reset_end});
        }
    }
    if (tail) tail_call(ctx, 0x08C49D08u, {tail_object, tail_begin, tail_end});
}

// func_08A6D10C(out, a1, count, items): a count of 0 or less returns (v0 untouched). Otherwise holders at sp + 4 (func_08A6C410) and sp + 8 (func_08A98A70) are built and, for each of the `count` words of `items`, the vector at o + 0x14 (o = the object of the holder sp + 8; null:
// func_08A5BB7C(D_08CB0774) first) gets the word appended: with b = v[4] and e = v[8], b == e calls func_08B8A03C(v, b, item); otherwise func_08A76ABC(b, item) runs for a non-null b and v[4] = b + 4 (a null b stores 4). Then the object p of the holder sp + 4
// (null: func_08A5BB7C(D_08CB0754) first) gives func_08AB398C(sp, p, sp + 8); the holder at sp is copied to `out` with func_08A5BAD8 unless it is `out`, and the three holders are destroyed (func_08A5B9EC: sp, sp + 8, sp + 4). v0 is the last call's.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): a count of 0 or less (0x08A6D13C), a null object in the loop (the assertion at 0x08A6D178), a null end pointer begin (0x08A6D19C: the store of 4), a null object p (the assertion at 0x08A6D1E0) and `out` being the holder at sp (0x08A6D1F8) never occurred.
void func_08A6D10C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[6], items = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t holder0 = frame.at(0u), holder4 = frame.at(4u), holder8 = frame.at(8u);
    ctx.gpr[22] = out;
    ctx.gpr[19] = count;
    ctx.gpr[16] = items;
    if (signed_of(count) <= 0) return;
    ctx.gpr[23] = holder4;
    ctx.gpr[4] = holder4;
    ctx.gpr[21] = holder8;  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x08A6C410u, {holder4});
    ctx.gpr[4] = holder8;
    (void)call_keep(rt, ctx, 0x08A98A70u, {holder8});
    std::uint32_t cursor = items, index = 0u;
    ctx.gpr[17] = cursor;
    ctx.gpr[18] = 0u;
    ctx.gpr[20] = high_half(0x08CB0774u);
    do {
        const std::uint32_t object = word(rt, holder8);
        ctx.gpr[2] = object;
        ctx.gpr[4] = 0x08CB0774u;
        const std::uint32_t vector = object + 0x14u;
        ctx.gpr[16] = vector;  // the delay slot of the test: it runs on both paths
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB0774u});
        const std::uint32_t begin = word(rt, vector + 4u), end = word(rt, vector + 8u);
        ctx.gpr[3] = begin;
        ctx.gpr[2] = end;
        ctx.gpr[4] = vector;
        ctx.gpr[5] = begin;
        ctx.gpr[6] = cursor;
        if (begin == end) {
            (void)call_keep(rt, ctx, 0x08B8A03Cu, {vector, begin, cursor});
        } else {
            ctx.gpr[4] = begin;
            ctx.gpr[5] = cursor;
            if (begin != 0u) (void)call_keep(rt, ctx, 0x08A76ABCu, {begin, cursor});
            const std::uint32_t reread = begin != 0u ? word(rt, vector + 4u) : begin;
            ctx.gpr[3] = reread + 4u;
            put(rt, vector + 4u, reread + 4u);
        }
        ++index;
        ctx.gpr[18] = index;
        cursor += 4u;  // the delay slot of the loop branch: it runs on both paths
        ctx.gpr[17] = cursor;
    } while (count != index);
    const std::uint32_t pointer = word(rt, holder4);
    ctx.gpr[16] = pointer;
    ctx.gpr[5] = pointer;  // the delay slot of the test: it runs on both paths
    if (pointer == 0u) {
        ctx.gpr[4] = 0x08CB0754u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB0754u});
        ctx.gpr[5] = pointer;
    }
    ctx.gpr[4] = holder0;
    ctx.gpr[6] = holder8;
    (void)call_keep(rt, ctx, 0x08AB398Cu, {holder0, pointer, holder8});
    if (out != holder0) {
        const std::uint32_t source = word(rt, holder0);
        ctx.gpr[5] = source;
        ctx.gpr[4] = out;
        (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, source});
    }
    for (const std::uint32_t holder : {holder0, holder8, holder4}) {
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    }
}

// func_08C48004(out, p, q): h = func_08C46D74(q[8], p + 0xC) (a wrapper that returns the signed halfword at q[8] + 0xEE) as a signed 16-bit value; p[4] -= 1. With r = q[0xC]:
//   - r != 0: func_08C46DAC(r, h, p + 0xC, p + 0xC) stores h in the halfword r + 0xEE; when h == -1 the halfword p[2] = ((r - *p[8]) >> 4) * 0xEEEEEEEF (the node index of r, a 32-bit mult, low word) and the index stays -1;
//   - r == 0: the halfword p[0] = h, and when h == -1 the halfword p[2] = h as well.
// Then the record `out` is filled: out[8] = (index >= 0 ? p[8][0] + 240 * index : 0), the byte out[0] = the byte q[0], out[4] = q[4], out[0xC] = r; v0 = out.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): r was always 0 (0x08C48064 always branched to 0x08C480A8), so the func_08C46DAC path (0x08C4806C-0x08C480A4) never ran, and h was always -1 (0x08C480B0 never branched, 0x08C480CC always did), so the computation of the slot (0x08C480D4-0x08C480E0) is read from the asm only.
void func_08C48004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], p = ctx.gpr[5], q = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = q;
    ctx.gpr[19] = out;
    ctx.gpr[18] = p;
    ctx.gpr[17] = 0u;
    ctx.gpr[16] = p + 0xCu;
    ctx.gpr[5] = p + 0xCu;
    const std::uint32_t ring = word(rt, q + 0xCu);
    ctx.gpr[21] = ring;  // the delay slot of the jal: it runs before the call
    const std::uint32_t half = call_keep(rt, ctx, 0x08C46D74u, {word(rt, q + 8u), p + 0xCu});
    ctx.gpr[3] = word(rt, p + 4u) - 1u;
    ctx.gpr[6] = p + 0xCu;
    ctx.gpr[7] = p + 0xCu;
    std::uint32_t index = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(half)));
    ctx.gpr[16] = index;
    ctx.gpr[4] = ring;
    put(rt, p + 4u, word(rt, p + 4u) - 1u);
    ctx.gpr[17] = index;
    ctx.gpr[5] = index;
    if (ring == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        put16(rt, p, index);
        if (index == 0xFFFFFFFFu) {
            put16(rt, p + 2u, index);
            ctx.gpr[17] = index;
        }
    } else {
        (void)call_keep(rt, ctx, 0x08C46DACu, {ring, index, p + 0xCu, p + 0xCu});
        ctx.gpr[2] = 0xFFFFFFFFu;
        if (index == 0xFFFFFFFFu) {
            const std::uint32_t first = word(rt, word(rt, p + 8u));
            ctx.gpr[3] = static_cast<std::uint32_t>(signed_of(ring - first) >> 4) * 0xEEEEEEEFu;
            ctx.gpr[2] = 0xEEEEEEEFu;
            put16(rt, p + 2u, ctx.gpr[3]);
        }
    }
    const std::uint32_t scaled = index << 8, small = index << 4;
    ctx.gpr[4] = scaled;
    ctx.gpr[5] = small;
    ctx.gpr[2] = out;
    std::uint32_t slot = 0u;
    ctx.gpr[16] = 0u;
    if (signed_of(index) >= 0) {
        const std::uint32_t base = word(rt, p + 8u);
        ctx.gpr[3] = base;
        ctx.gpr[5] = scaled - small;
        const std::uint32_t first = word(rt, base);
        ctx.gpr[4] = first;
        slot = first + (scaled - small);
        ctx.gpr[16] = slot;
    }
    const std::uint32_t kind = byte_u(rt, q), next = word(rt, q + 4u);
    ctx.gpr[3] = kind;
    ctx.gpr[4] = next;
    put(rt, out + 8u, slot);
    put8(rt, out, kind);
    put(rt, out + 4u, next);
    put(rt, out + 0xCu, ring);
    ctx.gpr[2] = out;
}

// func_08B5240C(this, cursor, value): w = this[0] (a pointer into a table of words; null returns, v0 untouched). A non-zero `value` is stored at this[0x44] (the delay slot of the test stores it either way) and written into the first pair of words: w[0] = (w[0] & 0xFF000000) | (value & 0x00FFFFFF),
// w[1] = (w[1] & 0xFF000000) | ((value & 0xFF000000) >> 8), and w advances by 8. Then k = this[0x24] & 0xF, f = func_08B52320(k), n = (the halfword this[0x22] & 7) + 1 and floor = 4 when k - 8 is below 3 (unsigned), else 1; for each of n entries (the group of two words at this + 0x40 + 8i)
// the cursor is stored at the group's +8, w[0] = (w[0] & 0xFF000000) | (cursor & 0x00FFFFFF), w[1] = (w[1] & ~0x00FF0000) | ((cursor & 0xFF000000) >> 8), w advances by 0xC, and the cursor advances by
// ((f * halfword group[0xC]) * max(floor, 1 << byte group[0xF])), made to round toward zero when divided by 8 (negative products get 7 added), arithmetic shift 3 (the mult is a 32-bit low word).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): a null w (0x08B5242C), a kind with k - 8 below 3 (the movz at 0x08B524A0 always chose 1), n below 1 (0x08B524A4) and a negative product (the movn at 0x08B52524 never took a0) never occurred.
void func_08B5240C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], cursor_in = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = cursor_in;
    std::uint32_t table = word(rt, self);
    ctx.gpr[17] = table;
    ctx.gpr[18] = self;
    if (table == 0u) return;
    put(rt, self + 0x44u, value);  // the delay slot of the test: it runs on both paths
    if (value != 0u) {
        std::uint32_t low = word(rt, table), high = word(rt, table + 4u);
        ctx.gpr[2] = low;
        ctx.gpr[3] = high;
        const std::uint32_t kept_low = value & 0x00FFFFFFu, kept_high = (value & 0xFF000000u) >> 8;
        low = (low & 0xFF000000u) | kept_low;
        high = (high & 0xFF000000u) | kept_high;
        put(rt, table, low);
        put(rt, table + 4u, high);
        table += 8u;
        ctx.gpr[17] = table;
    }
    const std::uint32_t kind = word(rt, self + 0x24u) & 0xFu;
    ctx.gpr[16] = kind;
    ctx.gpr[4] = kind;
    const std::uint32_t factor = call_keep(rt, ctx, 0x08B52320u, {kind});
    ctx.gpr[11] = factor;
    const std::uint32_t entries = (half_u(rt, self + 0x22u) & 7u) + 1u;
    ctx.gpr[10] = entries;
    const std::uint32_t floor_value = (kind - 8u) < 3u ? 4u : 1u;
    ctx.gpr[9] = floor_value;
    ctx.gpr[2] = 1u;
    if (signed_of(entries) <= 0) return;
    std::uint32_t cursor = cursor_in, group = self + 0x40u;
    for (std::uint32_t index = 0u; index < entries; ++index) {
        put(rt, group + 8u, cursor);
        const std::uint32_t first = word(rt, table), second = word(rt, table + 4u);
        put(rt, table, (first & 0xFF000000u) | (cursor & 0x00FFFFFFu));
        put(rt, table + 4u, (second & ~0x00FF0000u) | ((cursor & 0xFF000000u) >> 8));
        table += 0xCu;
        ctx.gpr[17] = table;
        const std::uint32_t width = half_u(rt, group + 0xCu), shift = byte_u(rt, group + 0xFu);
        group += 8u;
        const std::int32_t scale = signed_of(1u << (shift & 31u));
        const std::int32_t chosen = std::max(signed_of(floor_value), scale);
        const std::uint32_t product = (factor * width) * static_cast<std::uint32_t>(chosen);
        std::uint32_t advance = product;
        if (signed_of(product) < 0) advance = product + 7u;
        advance = static_cast<std::uint32_t>(signed_of(advance) >> 3);
        ctx.gpr[3] = static_cast<std::uint32_t>(chosen);
        ctx.gpr[2] = advance;
        ctx.gpr[4] = product + 7u;
        cursor += advance;
        ctx.gpr[7] = cursor;
    }
}

// func_08B48A74(object, a1, block, limit): a non-zero object[0] returns 1. Otherwise func_08B0EC60(sp + 0x80) fills a 0x3C-byte record that is copied to sp, with s = sp[0x10] (the record's fifth word): a null `block` is allocated with func_08B35DA0(s) and the byte object[4] = 1; a non-null one is
// accepted only when s is not above `limit` (signed; otherwise v0 = 0). sp[0] = block and sp[4] = s are stored in the copy, func_08B100D0(sp + 0xC0, a1, sp, 1) fills a second record that is copied to sp + 0x40; its first word non-zero gives v0 = 0, else
// func_08B48A68(object, block) runs and v0 = 1.
// Branches the routes do not take: not measured (not registered): every case stops on a sleep and nothing is compared; the whole body is read from the asm only.
[[maybe_unused]] void func_08B48A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], first = ctx.gpr[5], block_in = ctx.gpr[6], limit = ctx.gpr[7];
    GuestFrame frame(ctx, 0x120u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = first;
    ctx.gpr[3] = 1u;
    ctx.gpr[20] = limit;
    ctx.gpr[19] = block_in;
    ctx.gpr[17] = object;
    const std::uint32_t existing = word(rt, object);
    ctx.gpr[2] = existing;
    if (existing != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const auto copy_record = [&](std::uint32_t to, std::uint32_t from) {
        for (std::uint32_t offset = 0u; offset < 0x3Cu; offset += 4u) put(rt, to + offset, word(rt, from + offset));
    };
    ctx.gpr[16] = frame.at(0x80u);
    ctx.gpr[4] = frame.at(0x80u);
    (void)call_keep(rt, ctx, 0x08B0EC60u, {frame.at(0x80u)});
    copy_record(frame.at(0u), frame.at(0x80u));
    const std::uint32_t size = word(rt, frame.at(0x10u));
    ctx.gpr[18] = size;
    std::uint32_t block = block_in;
    if (block == 0u) {
        ctx.gpr[4] = size;
        block = call_keep(rt, ctx, 0x08B35DA0u, {size});
        ctx.gpr[19] = block;
        ctx.gpr[2] = 1u;
        put8(rt, object + 4u, 1u);  // the delay slot of the jump: it runs after the call
    } else {
        const bool refused = signed_of(limit) < signed_of(size);
        ctx.gpr[2] = refused ? 1u : 0u;
        ctx.gpr[3] = 0u;
        if (refused) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    ctx.gpr[16] = frame.at(0xC0u);
    ctx.gpr[6] = frame.at(0u);
    ctx.gpr[7] = 1u;
    ctx.gpr[5] = first;
    ctx.gpr[4] = frame.at(0xC0u);
    put(rt, frame.at(4u), size);
    put(rt, frame.at(0u), block);  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x08B100D0u, {frame.at(0xC0u), first, frame.at(0u), 1u});
    copy_record(frame.at(0x40u), frame.at(0xC0u));
    const std::uint32_t status = word(rt, frame.at(0x40u));
    ctx.gpr[2] = status;
    if (status != 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[4] = object;
    ctx.gpr[5] = block;
    (void)call_keep(rt, ctx, 0x08B48A68u, {object, block});
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 1u;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

}  // namespace
}  // namespace hot_batch_90
}  // namespace gt::game

#define HOT90(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_90::name, gt::ReturnKind::kind)
#define HOT90N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_90::name, gt::ReturnKind::kind)
HOT90(0x08BF99BCu, func_08BF99BC, Word);
HOT90(0x08C0154Cu, func_08C0154C, Word);
HOT90(0x08C1D530u, func_08C1D530, Word);
HOT90(0x08C25E64u, func_08C25E64, Word);
// Not registered (no compared case changed memory: its one call per route compared once and wrote nothing, the six other assign_counted members changed memory in 2-11 cases each): HOT90(0x08BC0DFCu, func_08BC0DFC, Word);
HOT90(0x08BD0B30u, func_08BD0B30, Word);
HOT90(0x08BD3C90u, func_08BD3C90, Word);
HOT90(0x08BD80D4u, func_08BD80D4, Word);
HOT90(0x08BD8228u, func_08BD8228, Word);
HOT90(0x08BE157Cu, func_08BE157C, Word);
HOT90(0x08BE17C8u, func_08BE17C8, Word);
HOT90(0x08BD3924u, func_08BD3924, Word);
HOT90(0x08BE0AC0u, func_08BE0AC0, Word);
HOT90(0x08BE10C4u, func_08BE10C4, Word);
HOT90(0x08BC98FCu, func_08BC98FC, Word);
HOT90(0x08BD24ACu, func_08BD24AC, Word);
HOT90(0x08BD523Cu, func_08BD523C, Word);
HOT90(0x08BE2E44u, func_08BE2E44, Word);
HOT90(0x08BE3A00u, func_08BE3A00, Word);
HOT90(0x08BD6140u, func_08BD6140, Word);
HOT90(0x08BD63A0u, func_08BD63A0, Word);
HOT90(0x08C011A0u, func_08C011A0, Word);
HOT90(0x08C0DEF8u, func_08C0DEF8, Word);
HOT90(0x08C4A594u, func_08C4A594, Word);
HOT90(0x08C4A6B0u, func_08C4A6B0, Word);
HOT90(0x088E8184u, func_088E8184, Word);
HOT90(0x08AF1FC0u, func_08AF1FC0, Word);
HOT90(0x08B2F31Cu, func_08B2F31C, Word);
HOT90(0x088B4938u, func_088B4938, Word);
HOT90(0x08B451A0u, func_08B451A0, Word);
HOT90(0x08C4A7CCu, func_08C4A7CC, Word);
HOT90(0x08B7A13Cu, func_08B7A13C, Word);
HOT90(0x088E90F0u, func_088E90F0, Word);
HOT90(0x08979B08u, func_08979B08, Word);
HOT90(0x08BB6BFCu, func_08BB6BFC, Word);
HOT90(0x088D53FCu, func_088D53FC, Word);
HOT90(0x08AE2D38u, func_08AE2D38, Word);
HOT90(0x089C6370u, func_089C6370, Word);
HOT90(0x0881CDC4u, func_0881CDC4, Word);
// Not registered (no compared case changed memory: 3000 dense cases on the race route, 12231 calls, none wrote): HOT90(0x08BA9498u, func_08BA9498, Word);
HOT90(0x0888A9B8u, func_0888A9B8, Word);
HOT90(0x0889F1DCu, func_0889F1DC, Word);
HOT90(0x08BE449Cu, func_08BE449C, Word);
HOT90(0x089B139Cu, func_089B139C, Word);
// Not registered (no compared case changed memory: dense runs compared every call (58 race, 4 dealer, 4 garage), none wrote): HOT90(0x08C070FCu, func_08C070FC, Word);
HOT90(0x0898A5F8u, func_0898A5F8, Word);
HOT90(0x0898A7A8u, func_0898A7A8, Word);
HOT90(0x08B03A48u, func_08B03A48, Word);
HOT90(0x08A6D10Cu, func_08A6D10C, Word);
HOT90(0x08C48004u, func_08C48004, Word);
HOT90(0x08B5240Cu, func_08B5240C, Word);
// Not registered (not comparable: all 21 cases of the fast gate stop on `kernel isolation (sleep would block)` (4 on the race route), nothing is compared): HOT90(0x08B48A74u, func_08B48A74, Word);
