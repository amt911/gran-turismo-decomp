// Hot batch 87: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet decompiled, run on the five routes with --decomp off;
// batches 85 and 86 are in main, so no address of theirs is taken). Before reading any function its address was searched for in src/ (main and the open PR branches: none). Candidates without VFPU, without imports that the
// harness cannot replay (calls into the import stubs from 0x08C70000 on were left out, so were the sibling initialiser that tail-calls one, func_08AC45F4, and the variadic wrapper func_08C46D74), without endless loops
// and multi-entry blobs (the jump-table switches func_08AEFB40 and func_088A0DD4 among them); each was read by hand word by word with `tools/disasm.py` (branch targets and relocations resolved by the tool, every delay slot
// part of its branch's path; the words of the one jump table, D_08C8B3E8, were read from the ELF).
//
//  - destructors and teardowns (func_0887ED48, func_0886DE00, func_088AF1D0, func_088A11F8, func_08BD67B8, func_08BE46A8, func_089EF8A8, func_08ABF590) with the shared-pointer release (func_0889A4F0, func_08BAEF98, func_08BE45C4, func_08BC0808,
//    func_08BAC9EC, func_08BE0724) and the array destruction (`destroy_elements`);
//  - holder / string-temporary accessors of the script classes (func_08A83474, func_08A83AC0, func_08A52F9C, func_08A20EC0, func_089ADE38, func_089C0360, func_0880C1DC, func_088797F8, func_0887894C, func_08A57D74, func_08A1503C, func_08A2A724, ...);
//  - constructors with guarded statics (func_0889DA58, func_0892BE4C, func_08809650) and plain constructors (func_08ACFA08, func_08922CF8, func_089E6520, func_0894CD28), vector growth (func_08C4A4AC), a strtok-style split (func_08ABD5EC) and small
//    table or state readers (func_0894D934, func_088D2E78, func_08915DEC, func_08898D30).
//
// Written but not registered (the commented HOT87 lines and each function's comment say why): the two parents func_0880D564 and func_0880D478, whose single call per menu route destroys a member object with its whole
// decompiled descendant tree (their cases swallowed the compared calls of nine of this batch's destructors; FINDINGS, batch 86), eight that every case stops at a lock, a sleep, a thread start or an import the harness does not replay
// (func_0883B600, func_0897BFEC, func_089C1368, func_089C151C, func_08ABAFCC, func_08AC2BD8, func_08BCFEF8, func_08BD787C), three without a compared case that changed memory (func_0888CFCC, func_08ABECA0, func_08B8ECAC) and func_08B9355C, compared only with a null node.
// Five functions that only run inside the cases of their decompiled parents (the destructors of the members those parents destroy) are registered NESTED (return contract audited: `tools.contract_audit` 0), so the gate
// verifies them through the parents' equal cases: func_0889A4F0, func_088A11F8, func_088A23EC, func_088AF1D0, func_08BD67B8.
//
// Compared on their main path but with a side path read from the asm only (each comment lists what the cases did not take; the helpers `release_body` / `release_string` never met the shared empty body, `destroy_elements` never an empty
// array, `drop_holder` never a null holder): func_08A57D74 (mode 0), func_08A20EC0 and func_089C0360 (their second half), func_089ADE38 (the setter half), func_08A52F9C (n > 0), the counter releases of func_08BC0808, func_08BAC9EC,
// func_08BD67B8, func_08BE45C4, func_08BAEF98 (the old pointer's release) and func_0889A4F0 (its early returns), func_088A23EC (the tail call), func_08ABD5EC (all tokens found), func_0881D450 (a pair other than -1:-1),
// func_08C4A4AC (a vector that already holds enough, a non-null old block), func_08BE0724 (the virtual pointer-to-member) and func_0894D934 (which jump-table entries the cases used is not told by any branch).
// Measured: the gate ran with every route time doubled (the host was loaded: FINDINGS).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest copies from ctx.
// Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's), and the callee-saved registers (s0-s7, fp) hold the original's values around the calls (`SavedRegs`
// restores the entry values on the way out, like the original's epilogue): a callee may store the caller's s0 (FINDINGS, batch 84). The delay slot of a `jal` runs BEFORE the call: a move from v0 there captures the
// PREVIOUS call's result (func_0880D478 took the allocation for it and faulted in the first gate; FINDINGS).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_87 {
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

inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline std::int16_t half_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// The object o at `holder_slot` of a destructor, when set, has its vtable slot 0 called with (o, word at `value_address`) (the word the original reads in the delay slot of the call).
inline void drop_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder_slot, std::uint32_t value_address) {
    const std::uint32_t object = word(rt, holder_slot);
    ctx.gpr[3] = object;
    ctx.gpr[4] = object;
    if (object == 0u) return;
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    const std::uint32_t argument = word(rt, value_address);
    ctx.gpr[5] = argument;
    (void)call_keep(rt, ctx, function, {object, argument});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// The release of a reference-counted body whose data pointer is `data` (body = data - 0xC, `shared` the shared empty body): unless it is the shared one, func_08B60804(data - 4, -1) takes a reference away and an answer <= 0 frees it with
// `free_function`(body, free_argument).
inline std::uint32_t release_body(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data, std::uint32_t shared, std::uint32_t free_function, std::uint32_t free_argument,
                                  unsigned body_register) {
    const std::uint32_t body = data - 0xCu;
    ctx.gpr[body_register] = body;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;
    if (body != shared) {
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = free_argument;
        if (signed_of(references) <= 0) (void)call_keep(rt, ctx, free_function, {body, free_argument});
    }
    return body;
}

// The array destruction of an `operator delete[]`-style teardown: `array` is the first element, the element count sits at array[-1] (4 bytes before), each element of `stride` bytes has its vtable slot 0 called with (element) from the last to the first
// (the loop ends when the word at `slot` equals the element just destroyed, which is the first one), then func_08B61FC0(first - 4) frees the block. s0 = the element, s1 = the cursor, as in the original.
inline void destroy_elements(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t array, std::uint32_t stride) {
    const std::uint32_t count = word(rt, array - 4u);
    ctx.gpr[2] = count;
    const std::uint32_t end = array + count * stride;
    std::uint32_t cursor = end - stride;
    ctx.gpr[16] = end;
    ctx.gpr[3] = cursor;
    ctx.gpr[17] = cursor;
    std::uint32_t element = end;
    if (array != end) {
        for (;;) {
            element = cursor;
            const std::uint32_t vtable = word(rt, element);
            const std::uint32_t function = word(rt, vtable);
            ctx.gpr[16] = element;
            ctx.gpr[4] = element;
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            cursor -= stride;
            ctx.gpr[17] = cursor;
            (void)call_keep(rt, ctx, function, {element});
            ctx.gpr[2] = word(rt, slot);
            ctx.gpr[3] = element - stride;
            if (word(rt, slot) == element) break;
        }
    }
    ctx.gpr[4] = element - 4u;
    (void)call_keep(rt, ctx, 0x08B61FC0u, {element - 4u});
}

// The release of a reference-counted string body that the temporary at `holder` points to (its data pointer p; body = p - 0xC; D_08D74FD0 is the shared empty body): unless it is the empty one,
// func_08B60804(p - 4, -1) takes a reference away and when that answers <= 0 func_08B8542C(body, sp) frees it. s0 holds the body and v0 the shared body for the compare (the function leaves it there when the body is shared).
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t holder, std::uint32_t frame_sp) {
    const std::uint32_t data = word(rt, holder);
    const std::uint32_t body = data - 0xCu;
    ctx.gpr[3] = data;
    ctx.gpr[16] = body;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;
    if (body != 0x08D74FD0u) {
        const std::uint32_t references = call_keep(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = body;
        ctx.gpr[5] = frame_sp;
        if (signed_of(references) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {body, frame_sp});
    }
}

// The guarded static D_08D750F8 (guard byte, func_08B6441C / func_08B64494) holds D_08D75100 (set to D_08CBCD90 on the first call); then slot = D_08D75100. s1 = the guard, s2 = the holder, v0 = the lui of the high half (0x08D70000) when the guard was set or
// func_08B6441C answered 0, v0 = the last call's otherwise.
inline void guarded_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t guard = 0x08D750F8u, holder = 0x08D75100u;
    ctx.gpr[3] = 0x08D70000u;
    ctx.gpr[2] = static_cast<std::uint32_t>(byte_at(rt, guard));
    ctx.gpr[17] = guard;
    bool set_holder = false;
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[4] = guard;
        const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
        if (acquired != 0u) set_holder = true;
        else ctx.gpr[2] = 0x08D70000u;
    } else {
        ctx.gpr[2] = 0x08D70000u;
    }
    ctx.gpr[18] = holder;
    if (set_holder) {
        ctx.gpr[2] = 0x08CBCD90u;
        put(rt, holder, 0x08CBCD90u);
        ctx.gpr[4] = guard;
        (void)call_keep(rt, ctx, 0x08B64494u, {guard});
    }
    put(rt, slot, holder);
}
// func_0894D934(): o = func_0894CF58(); a state byte s = o[0x270]; v0 = the class of the state: 0 -> 0, 1 -> 1, 2 -> 3, 3 -> 2, 4..7 -> 4, 8 -> 2, 9 -> 3, 10 -> 2, 11 -> 3, anything above 11 -> 2 (a jump table at 0x08C8B3E8 for s < 12; read from the ELF's words).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a state of 12 or more (the `beqz v0` at 0x0894D94C: v0 = 2) never ran; which of the twelve jump-table entries the cases used is not told by any branch, so the table (read from the ELF's words at D_08C8B3E8) is checked against the asm only.
void func_0894D934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t object = call_keep(rt, ctx, 0x0894CF58u, {});
    const std::uint32_t state = byte_u(rt, object + 0x270u);
    ctx.gpr[3] = state;
    static constexpr std::uint32_t kClass[12] = {0u, 1u, 3u, 2u, 4u, 4u, 4u, 4u, 2u, 3u, 2u, 3u};
    if (state < 12u) {  // the jump through the table leaves v1 = the table and a0 = the entry
        ctx.gpr[3] = 0x08C8B3E8u;
        ctx.gpr[4] = word(rt, 0x08C8B3E8u + state * 4u);
    }
    ctx.gpr[2] = state < 12u ? kClass[state] : 2u;
}

// func_08AC2BD8(): a callback pointer at D_08CFFCA8 (its first word f, its argument = the second word of the same record): when both are set f(record[1]) runs; then the object at D_08CFFCA4, when set, is handed to func_08AC2BAC as a tail call (v0 = 0x08D00000, the lui
// the original leaves there); else the object o at D_08CFFCA0, when set, has its vtable slot 0x58 tail-called with (o) (v0 = the vtable); with neither v0 = 0.
// Not registered: every case stops at a sleep that would block (not comparable).
void func_08AC2BD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t record = word(rt, 0x08CFFCA8u);
        ctx.gpr[2] = record;
        if (record != 0u) {
            const std::uint32_t function = word(rt, record);
            ctx.gpr[3] = function;
            if (function != 0u) {
                ctx.gpr[4] = word(rt, record + 4u);
                (void)call_keep(rt, ctx, function, {word(rt, record + 4u)});
            }
        }
        ctx.gpr[2] = 0x08D00000u;
        const std::uint32_t object = word(rt, 0x08CFFCA4u);
        ctx.gpr[3] = object;
        ctx.gpr[4] = object;
        if (object != 0u) {
            ctx.gpr[2] = 0x08D00000u;
            tail_call(ctx, 0x08AC2BACu, {object});
            return;
        }
        const std::uint32_t other = word(rt, 0x08CFFCA0u);
        ctx.gpr[2] = other;
        ctx.gpr[4] = other;
        if (other == 0u) return;
        const std::uint32_t vtable = word(rt, other);
        ctx.gpr[2] = vtable;
        ctx.gpr[25] = word(rt, vtable + 0x58u);
        tail_call(ctx, ctx.gpr[25], {other});
        return;
    }
}

// func_08BE0724(this): this[0] = D_08CC68C0; the pointer-to-member call: adjusted = this[8] + (this[0x10] >> 1) (signed), the function is this[0xC] itself or, with bit 0 of this[0x10] set, the vtable word at (vtable of adjusted) + this[0xC]; function(adjusted);
// then this[0] = D_08CC52E0 and a tail call to func_08B64654(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): bit 0 of this[0x10] is never set (the virtual form of the pointer-to-member, 0x08BE0750-0x08BE0768: the function is the vtable word at (vtable of adjusted) + this[0xC]) -> read from the asm only.
void func_08BE0724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[16] = self;
        put(rt, self, 0x08CC68C0u);
        const std::uint32_t flags = word(rt, self + 0x10u);
        const std::uint32_t offset = static_cast<std::uint32_t>(signed_of(flags) >> 1);
        ctx.gpr[5] = offset;
        ctx.gpr[3] = flags & 1u;
        const std::uint32_t adjusted = word(rt, self + 8u) + offset;
        ctx.gpr[4] = adjusted;
        std::uint32_t function;
        if ((flags & 1u) != 0u) {
            function = word(rt, word(rt, adjusted) + word(rt, self + 0xCu));
            ctx.gpr[3] = word(rt, self + 0xCu);
        } else {
            function = word(rt, self + 0xCu);
        }
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {adjusted});
        ctx.gpr[2] = 0x08CC52E0u;
        put(rt, self, 0x08CC52E0u);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_08AB3774(this): i = this[0x2C]; o = this[0x28][i] (a table of pointers); v0:v1 = 0 unless o is set and i < this[0x24] (signed): then v0:v1 = the vtable slot 0x58 of o called with (o); this[0x2C] = i + 1 (read again after the call); the result is v0:v1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the entry o is never null and the index is always below this[0x24] (the `beqz v1` at 0x08AB379C and the `beqz v0` at 0x08AB37AC would skip the call and leave v0:v1 = 0, the final increment of this[0x2C] still runs).
void func_08AB3774(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    std::uint32_t index = word(rt, self + 0x2Cu);
    const std::uint32_t object = word(rt, word(rt, self + 0x28u) + index * 4u);
    std::uint32_t lo = 0u, hi = 0u;
    ctx.gpr[5] = index;
    ctx.gpr[3] = object;
    if (object != 0u) {
        ctx.gpr[2] = signed_of(index) < signed_of(word(rt, self + 0x24u)) ? 1u : 0u;
        ctx.gpr[4] = object;
        if (signed_of(index) < signed_of(word(rt, self + 0x24u))) {
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t function = word(rt, vtable + 0x58u);
            ctx.gpr[2] = vtable;
            ctx.gpr[5] = function;
            const GuestResult result = call_keep(rt, ctx, function, {object}, {});
            lo = result.v0;
            hi = result.v1;
            index = word(rt, self + 0x2Cu);
            ctx.gpr[5] = index;
        }
    }
    ctx.gpr[6] = lo;
    ctx.gpr[7] = hi;
    ctx.gpr[4] = index + 1u;
    put(rt, self + 0x2Cu, index + 1u);
    ctx.gpr[2] = lo;
    ctx.gpr[3] = hi;
}

// func_0889A4F0(this): s = this[4]; a null s leaves v0 alone (v1 = 4). The strong counter at s + 4 is decremented (ll / sc, one thread: a plain load and store), v0 = 1; when it was 1 the virtual slot +8 of s runs on s, then the weak counter at s + 8 is decremented the same
// way (v0 = 1) and, when it was 1, the virtual slot +0xC runs on s as a tail call (v0 = the vtable, v1 = s + 8).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null s (the `beqz s0` at 0x0889A500: return with v1 = 4), a strong counter above 1 (`bne a0, v0` at 0x0889A51C) and a weak counter above 1 (0x0889A54C) never ran: every compared case released both counters and took the final tail call (0x0889A554-0x0889A568); the three early returns are read from the asm only.
void func_0889A4F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t target = 0u, shared = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        shared = word(rt, self + 4u);
        ctx.gpr[16] = shared;
        ctx.gpr[3] = shared + 4u;
        if (shared == 0u) return;
        const std::uint32_t strong = word(rt, shared + 4u);
        put(rt, shared + 4u, strong - 1u);
        ctx.gpr[2] = 1u;
        ctx.gpr[4] = strong;
        if (strong != 1u) return;
        const std::uint32_t vtable = word(rt, shared);
        const std::uint32_t function = word(rt, vtable + 8u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {shared});
        ctx.gpr[3] = shared + 8u;
        const std::uint32_t weak = word(rt, shared + 8u);
        put(rt, shared + 8u, weak - 1u);
        ctx.gpr[2] = 1u;
        ctx.gpr[4] = weak;
        if (weak != 1u) return;
        const std::uint32_t final_vtable = word(rt, shared);
        ctx.gpr[2] = final_vtable;
        ctx.gpr[4] = shared;
        target = word(rt, final_vtable + 0xCu);
    }
    tail_call(ctx, target, {shared});
}

// func_08BD67B8(this): this[0] = D_08CC55E0; s = this[0x14]; when set: the strong counter at s + 4 is decremented (ll / sc), and when it was 1 the virtual slot +8 of s runs on s, then the weak counter at s + 8 is decremented and, when it was 1, the slot +0xC runs on s;
// then func_08BAEF88(this) and a tail call to func_08B64654(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the shared pointer s is never null (the `beqz s0` at 0x08BD67D8 skips the counters) and its strong counter is never 1 (the virtual slot +8, the weak counter and the slot +0xC, 0x08BD6804-0x08BD683C, never ran) -> read from the asm only.
void func_08BD67B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        const std::uint32_t shared = word(rt, self + 0x14u);
        ctx.gpr[16] = shared;
        ctx.gpr[2] = 0x08CC55E0u;
        put(rt, self, 0x08CC55E0u);
        if (shared != 0u) {
            ctx.gpr[3] = shared + 4u;
            const std::uint32_t strong = word(rt, shared + 4u);
            put(rt, shared + 4u, strong - 1u);
            ctx.gpr[2] = 1u;
            ctx.gpr[4] = strong;
            if (strong == 1u) {
                const std::uint32_t vtable = word(rt, shared);
                const std::uint32_t function = word(rt, vtable + 8u);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                (void)call_keep(rt, ctx, function, {shared});
                ctx.gpr[3] = shared + 8u;
                const std::uint32_t weak = word(rt, shared + 8u);
                put(rt, shared + 8u, weak - 1u);
                ctx.gpr[2] = 1u;
                ctx.gpr[4] = weak;
                if (weak == 1u) {
                    const std::uint32_t final_vtable = word(rt, shared);
                    const std::uint32_t final_function = word(rt, final_vtable + 0xCu);
                    ctx.gpr[3] = final_vtable;
                    ctx.gpr[2] = final_function;
                    (void)call_keep(rt, ctx, final_function, {shared});
                }
            }
        }
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x08BAEF88u, {self});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_08B8ECAC(first, last): for each 8-byte element from first until last (a do-while on `!=`, nothing happens when first == last): s = element[1]; when set, the strong counter at s + 4 is decremented (ll / sc, v0 = 1) and, when it was 1, the virtual slot +8 of s runs on s, then the
// weak counter at s + 8 is decremented and, when it was 1, the slot +0xC runs on s. v0 is the last operation's.
// Not registered: no compared case changed memory (5 compared, all equal).
void func_08B8ECAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = last;
    ctx.gpr[17] = first;
    if (first == last) return;
    ctx.gpr[19] = 1u;
    std::uint32_t at = first;
    do {
        ctx.gpr[17] = at;
        const std::uint32_t shared = word(rt, at + 4u);
        ctx.gpr[16] = shared;
        if (shared != 0u) {
            ctx.gpr[3] = shared + 4u;
            const std::uint32_t strong = word(rt, shared + 4u);
            put(rt, shared + 4u, strong - 1u);
            ctx.gpr[2] = 1u;
            ctx.gpr[4] = strong;
            if (strong == 1u) {
                const std::uint32_t vtable = word(rt, shared);
                const std::uint32_t function = word(rt, vtable + 8u);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                (void)call_keep(rt, ctx, function, {shared});
                ctx.gpr[3] = shared + 8u;
                const std::uint32_t weak = word(rt, shared + 8u);
                put(rt, shared + 8u, weak - 1u);
                ctx.gpr[2] = 1u;
                ctx.gpr[4] = weak;
                if (weak == 1u) {
                    const std::uint32_t final_vtable = word(rt, shared);
                    const std::uint32_t final_function = word(rt, final_vtable + 0xCu);
                    ctx.gpr[3] = final_vtable;
                    ctx.gpr[2] = final_function;
                    ctx.gpr[4] = shared;
                    (void)call_keep(rt, ctx, final_function, {shared});
                }
            }
        }
        at += 8u;
        ctx.gpr[17] = at;
    } while (at != last);
}

// func_0887ED48(this): this[0] = D_08CC1130, this[0x70] = D_08CC116C, func_0886AA0C(this, 5), (this, 2), (this, 4), (this, 3); the object o = this[0x78], when set, has its vtable slot 0 called with (o, this[0x74]) and this[0x78] = this[0x74] = 0;
// this[0x70] = D_08CC1180, func_0886A858(this); the tail call is func_08B64654(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object this[0x78] is never null (the `beqz v1` at 0x0887EDA8 would skip its release).
void func_0887ED48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[2] = 0x08CC116Cu;
        ctx.gpr[17] = self + 0x74u;
        ctx.gpr[16] = self;
        put(rt, self, 0x08CC1130u);
        put(rt, self + 0x70u, 0x08CC116Cu);
        ctx.gpr[5] = 5u;
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 5u});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 2u});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 4u});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 3u});
        const std::uint32_t object = word(rt, self + 0x78u);
        ctx.gpr[3] = object;
        ctx.gpr[4] = object;
        if (object != 0u) {
            drop_holder(rt, ctx, self + 0x78u, self + 0x74u);
            put(rt, self + 0x78u, 0u);
            put(rt, self + 0x74u, 0u);
        }
        ctx.gpr[2] = 0x08CC1180u;
        ctx.gpr[4] = self;
        put(rt, self + 0x70u, 0x08CC1180u);
        (void)call_keep(rt, ctx, 0x0886A858u, {self});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_0886DE00(this): this[0] = D_08CBF770; func_0886A8B8(this, i, 0) for i = 0, 1, 2; func_0886AA0C(this, 2), (this, 5), (this, 6); the object o = this[0x74], when set, has its vtable slot 0 called with (o, this[0x70]) and this[0x74] = this[0x70] = 0;
// func_0886A858(this); the tail call is func_08B64654(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object this[0x74] is never null (the `beqz a0` at 0x0886DE74 would skip its release).
void func_0886DE00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[2] = 0x08CBF770u;
        ctx.gpr[18] = 3u;
        ctx.gpr[17] = self;
        ctx.gpr[16] = 0u;
        put(rt, self, 0x08CBF770u);
        for (std::uint32_t index = 0u; index < 3u; ++index) {
            ctx.gpr[5] = index;
            ctx.gpr[4] = self;
            ctx.gpr[16] = index + 1u;
            ctx.gpr[6] = 0u;
            (void)call_keep(rt, ctx, 0x0886A8B8u, {self, index, 0u});
        }
        ctx.gpr[5] = 2u;
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 2u});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 5u});
        ctx.gpr[16] = self + 0x70u;
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886AA0Cu, {self, 6u});
        const std::uint32_t object = word(rt, self + 0x74u);
        ctx.gpr[4] = object;
        if (object != 0u) {
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t function = word(rt, vtable);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            ctx.gpr[5] = word(rt, self + 0x70u);
            (void)call_keep(rt, ctx, function, {object, word(rt, self + 0x70u)});
            put(rt, self + 0x74u, 0u);
            put(rt, self + 0x70u, 0u);
        }
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x0886A858u, {self});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_08A83474(this, out, a2, ptr): o = *ptr (null: func_08A5BB7C(D_08CB168C) runs first); lo:hi = o's vtable slot 0x40 called with (o); the 64-bit difference (this[0x18]:this[0x1C]) - lo:hi goes to func_08A83160(sp, -, difference low, difference high);
// unless `out` is sp func_08A5BAD8(out, [sp]); func_08A5B9EC(sp). v0 is the last call's. Sibling of func_08A83E98 (batch 86, AND instead of the subtraction).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object *ptr is never null (the func_08A5BB7C assertion call skipped by the `bnez s2` at 0x08A834A0), the 64-bit subtraction never borrows (`sltu a0, s0, a2` at 0x08A834C4 is always 0, so a3 = s1 - v1) and `out` is never the holder at sp (the copy func_08A5BAD8 always runs; `beq s3, sp` at 0x08A834D8).
void func_08A83474(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5], ptr = ctx.gpr[7], self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = out;
    const std::uint32_t object = word(rt, ptr);
    ctx.gpr[18] = object;
    const std::uint32_t low = word(rt, self + 0x18u), high = word(rt, self + 0x1Cu);
    ctx.gpr[16] = low;
    ctx.gpr[17] = high;
    ctx.gpr[4] = 0x08CB168Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB168Cu});
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0x40u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = object;
    const GuestResult wide = call_keep(rt, ctx, function, {object}, {});
    const std::uint32_t diff_lo = low - wide.v0;
    const std::uint32_t borrow = low < diff_lo ? 1u : 0u;
    const std::uint32_t diff_hi = high - wide.v1 - borrow;
    ctx.gpr[6] = diff_lo;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[7] = diff_hi;
    (void)call_keep(rt, ctx, 0x08A83160u, {frame.at(0u), ctx.gpr[5], diff_lo, diff_hi});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08A83AC0(this, out, a2, ptr): o = *ptr (null: func_08A5BB7C(D_08CB168C) runs first); lo:hi = o's vtable slot 0x40 called with (o); flag = (this[0x1C]:this[0x18]) < lo:hi as a signed 64-bit compare; func_08A97A6C(sp, flag), unless `out` is sp func_08A5BAD8(out, [sp]);
// func_08A5B9EC(sp). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object *ptr is never null (the `bnez s0` at 0x08A83AEC), the flag is always 0: the high words are equal (`slt` at 0x08A83B10 never 1, the `bne s3, v1` at 0x08A83B20 never taken) and the low word of this is never below the answer's (`beqz v0` at 0x08A83B28 always taken: a1 = 1 at 0x08A83B30 never ran), and `out` is never the holder at sp (`beq s1, sp` at 0x08A83B3C).
void func_08A83AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5], ptr = ctx.gpr[7], self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t object = word(rt, ptr);
    ctx.gpr[16] = object;
    ctx.gpr[17] = out;
    const std::uint32_t low = word(rt, self + 0x18u), high = word(rt, self + 0x1Cu);
    ctx.gpr[18] = low;
    ctx.gpr[19] = high;
    ctx.gpr[4] = 0x08CB168Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB168Cu});
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0x40u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = object;
    const GuestResult wide = call_keep(rt, ctx, function, {object}, {});
    ctx.gpr[6] = wide.v0;
    std::uint32_t flag = 0u;
    if (signed_of(high) < signed_of(wide.v1)) flag = 1u;
    else if (high == wide.v1 && low < wide.v0) flag = 1u;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = flag;
    (void)call_keep(rt, ctx, 0x08A97A6Cu, {frame.at(0u), flag});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08915DEC(this): first = func_08915D8C(this), end = func_08915D9C(this); delta = this - this[1]; this[1] = this; for i = first while i < end (signed): the word at this + 0x14 + 4 i += delta and func_08915DB4(that word, delta). v0 is the last call's (the old this[1] without iterations).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): first >= end (the `beqz v1` at 0x08915E2C returns at once with v0 = the old this[4]) never ran: every compared case entered the loop.
void func_08915DEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    (void)call_keep(rt, ctx, 0x08915D8Cu, {self});
    ctx.gpr[4] = self;
    ctx.gpr[17] = ctx.gpr[2];
    std::uint32_t index = ctx.gpr[2];
    const std::uint32_t end = call_keep(rt, ctx, 0x08915D9Cu, {self});
    ctx.gpr[4] = end;
    const std::uint32_t previous = word(rt, self + 4u);
    ctx.gpr[2] = previous;
    put(rt, self + 4u, self);
    const std::uint32_t delta = self - previous;
    ctx.gpr[19] = delta;
    ctx.gpr[3] = signed_of(index) < signed_of(end) ? 1u : 0u;
    if (!(signed_of(index) < signed_of(end))) return;
    std::uint32_t slot = self + index * 4u + 0x10u;
    std::uint32_t source = self + index * 4u + 0x14u;
    ctx.gpr[20] = end;
    ctx.gpr[18] = source;
    ctx.gpr[16] = slot;
    for (;;) {
        ctx.gpr[2] = word(rt, slot + 4u);
        ++index;
        ctx.gpr[17] = index;
        ctx.gpr[5] = delta;
        put(rt, slot + 4u, word(rt, slot + 4u) + delta);
        slot += 4u;
        ctx.gpr[16] = slot;
        const std::uint32_t argument = word(rt, source);
        ctx.gpr[4] = argument;
        ctx.gpr[18] = source + 4u;
        (void)call_keep(rt, ctx, 0x08915DB4u, {argument, delta});
        source += 4u;
        if (index == end) break;
    }
}

// func_08898D30(this, buf, x): the four words of buf are set to 0xDEADBEEF; n = func_088F93EC(this); the object o = this[0x40] has its vtable slot 0x40 called with (o, n); v0 = func_08898874(x) + buf.
// Branches: none that the compared cases skip (every branch took both ways).
void func_08898D30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], buffer = ctx.gpr[5], x = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = x;
    ctx.gpr[17] = self;
    ctx.gpr[16] = buffer;
    ctx.gpr[2] = 0xDEADBEEFu;
    for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, buffer + i * 4u, 0xDEADBEEFu);
    ctx.gpr[3] = buffer + 0x10u;
    ctx.gpr[5] = buffer + 0x10u;
    const std::uint32_t count = call_keep(rt, ctx, 0x088F93ECu, {self});
    const std::uint32_t object = word(rt, self + 0x40u);
    ctx.gpr[4] = object;
    ctx.gpr[5] = count;
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0x40u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    (void)call_keep(rt, ctx, function, {object, count});
    ctx.gpr[4] = x;
    const std::uint32_t result = call_keep(rt, ctx, 0x08898874u, {x});
    ctx.gpr[2] = result + buffer;
}

// func_088AF1D0(this): the buffers this[0xC] and this[8], when set, are freed with func_08B61FC0 (this[4] = 0 first, this[0xC] = 0 / this[8] = 0 after each); the array this[0x10], when set, is destroyed from its last element to the first (0x440-byte elements: the vtable
// slot 0 called with the element, the count at array[-1]) and its block freed with func_08B61FC0(first - 4); this[0x10] = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the blocks this[0xC] and this[8] and the array this[0x10] are never null (the `beqz a0` at 0x088AF1EC, the `beql a0` at 0x088AF204 and the `beqz a1` at 0x088AF21C skip them) and the array is never empty (the count 0: `beq a1, s0` at 0x088AF23C goes straight to the free).
void func_088AF1D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    const std::uint32_t first_block = word(rt, self + 0xCu);
    ctx.gpr[4] = first_block;
    put(rt, self + 4u, 0u);
    if (first_block != 0u) {
        (void)call_keep(rt, ctx, 0x08B61FC0u, {first_block});
        put(rt, self + 0xCu, 0u);
    }
    const std::uint32_t second_block = word(rt, self + 8u);
    ctx.gpr[4] = second_block;
    if (second_block != 0u) {
        (void)call_keep(rt, ctx, 0x08B61FC0u, {second_block});
        put(rt, self + 8u, 0u);
    }
    const std::uint32_t array = word(rt, self + 0x10u);
    ctx.gpr[5] = array;
    if (array == 0u) return;
    destroy_elements(rt, ctx, self + 0x10u, array, 0x440u);
    put(rt, self + 0x10u, 0u);
}

// func_088A11F8(this): this[0] = D_08CC27E8; the block this[0xC], when set, is freed with func_08B61FC0; the object o = this[8], when set, has its vtable slot 4 called with (o); the array this[4], when set, is destroyed from its last element to the first (0x7C-byte elements: the
// vtable slot 0 called with the element, the count at array[-1]) and its block freed with func_08B61FC0(first - 4); this[0] = D_08CC2990; the tail call is func_08B64654(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the block this[0xC], the object this[8] and the array this[4] are never null (the `beqz a0` at 0x088A121C, the `beql a0` at 0x088A1230 and the `beqz a1` at 0x088A124C skip them) and the array is never empty (`beq a1, s0` at 0x088A126C).
void func_088A11F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = self;
        ctx.gpr[2] = 0x08CC27E8u;
        const std::uint32_t block = word(rt, self + 0xCu);
        ctx.gpr[4] = block;
        put(rt, self, 0x08CC27E8u);
        if (block != 0u) (void)call_keep(rt, ctx, 0x08B61FC0u, {block});
        const std::uint32_t object = word(rt, self + 8u);
        ctx.gpr[4] = object;
        if (object != 0u) {
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t function = word(rt, vtable + 4u);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            (void)call_keep(rt, ctx, function, {object});
        }
        const std::uint32_t array = word(rt, self + 4u);
        ctx.gpr[5] = array;
        ctx.gpr[2] = 0x08CC0000u;
        if (array != 0u) destroy_elements(rt, ctx, self + 4u, array, 0x7Cu);
        ctx.gpr[2] = 0x08CC2990u;
        put(rt, self, 0x08CC2990u);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_08BE46A8(this): s = this[0xC]; a null s leaves v0 alone; else func_08BE427C(s + 0x98), func_08B28BE0(s + 0x6C), func_08B2C3BC(s + 8); the shared object p = s[1], when set: its strong counter at p + 4 is decremented (ll / sc, one thread: a plain load and store) and, when it was 1,
// the virtual slot +8 of p runs on p, then the weak counter at p + 8 is decremented the same way and, when it was 1, the slot +0xC runs on p; the tail call is func_08B64654(s).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null this[0xC] (the `beqz s0` at 0x08BE46BC returns with v0 untouched), a null shared pointer (`beqz s1` at 0x08BE46E0) and a strong or weak counter above 1 (`bne` at 0x08BE46FC and 0x08BE472C) never ran: every compared case released both counters and ran the slots +8 and +0xC.
void func_08BE46A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t record;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        record = word(rt, self + 0xCu);
        ctx.gpr[16] = record;
        if (record == 0u) return;
        ctx.gpr[4] = record + 0x98u;
        (void)call_keep(rt, ctx, 0x08BE427Cu, {record + 0x98u});
        ctx.gpr[4] = record + 0x6Cu;
        (void)call_keep(rt, ctx, 0x08B28BE0u, {record + 0x6Cu});
        ctx.gpr[4] = record + 8u;
        (void)call_keep(rt, ctx, 0x08B2C3BCu, {record + 8u});
        const std::uint32_t shared = word(rt, record + 4u);
        ctx.gpr[17] = shared;
        if (shared != 0u) {
            ctx.gpr[3] = shared + 4u;
            const std::uint32_t strong = word(rt, shared + 4u);
            put(rt, shared + 4u, strong - 1u);
            ctx.gpr[2] = 1u;
            ctx.gpr[4] = record;
            if (strong == 1u) {
                const std::uint32_t vtable = word(rt, shared);
                const std::uint32_t function = word(rt, vtable + 8u);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                ctx.gpr[4] = shared;
                (void)call_keep(rt, ctx, function, {shared});
                ctx.gpr[3] = shared + 8u;
                const std::uint32_t weak = word(rt, shared + 8u);
                put(rt, shared + 8u, weak - 1u);
                ctx.gpr[2] = 1u;
                ctx.gpr[4] = record;
                if (weak == 1u) {
                    const std::uint32_t final_vtable = word(rt, shared);
                    const std::uint32_t final_function = word(rt, final_vtable + 0xCu);
                    ctx.gpr[3] = final_vtable;
                    ctx.gpr[2] = final_function;
                    ctx.gpr[4] = shared;
                    (void)call_keep(rt, ctx, final_function, {shared});
                }
            }
        }
        ctx.gpr[4] = record;
    }
    tail_call(ctx, 0x08B64654u, {record});
}

// func_0880D564(this, a1): func_0896D6A4(this, a1), func_0880C020(this[0xE8], a1); with this[0xE8] set: the object o = this[0xEC], when set, has its vtable slot 0 called with (o) and this[0xEC] = 0; this[0xE8] = 0 (also when o is null). Then the guarded static D_08D750F8
// (func_08B6441C / func_08B64494) holds D_08D75100 (set to D_08CBCD90 on the first call) and this[0xEC] = D_08D75100. v0 is the last call's, D_08D70000's lui when the guard was set.
// Not registered: its single call per menu route destroys a member object with its whole decompiled descendant tree: its case swallows the compared calls of its descendants (nine of this batch's destructors showed `calls 0`; FINDINGS, batch 86). Compared 4, equal 4 in the first gate.
void func_0880D564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    ctx.gpr[16] = argument;
    (void)call_keep(rt, ctx, 0x0896D6A4u, {self, argument});
    ctx.gpr[5] = argument;
    const std::uint32_t current = word(rt, self + 0xE8u);
    ctx.gpr[4] = current;
    ctx.gpr[16] = self + 0xE8u;
    (void)call_keep(rt, ctx, 0x0880C020u, {current, argument});
    const std::uint32_t pending = word(rt, self + 0xE8u);
    ctx.gpr[5] = pending;
    if (pending == 0u) return;
    const std::uint32_t object = word(rt, self + 0xECu);
    ctx.gpr[4] = object;
    if (object == 0u) {
        put(rt, self + 0xE8u, 0u);
    } else {
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {object});
        put(rt, self + 0xECu, 0u);
        put(rt, self + 0xE8u, 0u);
    }
    guarded_holder(rt, ctx, self + 0xECu);
}

// func_0887894C(this): a string temporary t at sp + 8 (func_08AB85F0), func_08875658(this, sp + 4, t, 1); n = func_08AB856C(t); with the word at sp + 0x10 above 0: p = func_088756F0(this, D_08CA0088, n), and a non-null p: q = func_0886A9A0(p, 2), and a non-null q: func_08881C38(q, [sp + 4]).
// Then the string release of t (the shared empty body D_08D75298: func_08B60804(p - 4, -1), an answer <= 0 frees it with func_08B93554(body, sp)). v0 is the last call's (the shared body's address when nothing was released).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the word at sp + 0x10 is always above 0 and both lookups answer non-null (the `blez v0` at 0x08878998, the `beqz v0` at 0x088789AC and 0x088789BC would skip the rest); the shared empty body of the string is never met (`beq s0, v0` at 0x088789E0) -> read from the asm only.
void func_0887894C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(8u);
    ctx.gpr[16] = holder;
    ctx.gpr[17] = self;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08AB85F0u, {holder});
    ctx.gpr[6] = holder;
    ctx.gpr[5] = frame.at(4u);
    ctx.gpr[4] = self;
    ctx.gpr[7] = 1u;
    (void)call_keep(rt, ctx, 0x08875658u, {self, frame.at(4u), holder, 1u});
    ctx.gpr[4] = holder;
    const std::uint32_t name = call_keep(rt, ctx, 0x08AB856Cu, {holder});
    ctx.gpr[6] = name;
    const std::uint32_t count = word(rt, frame.at(0x10u));
    ctx.gpr[2] = count;
    ctx.gpr[5] = 0x08CA0088u;
    ctx.gpr[4] = self;
    if (signed_of(count) > 0) {
        const std::uint32_t found = call_keep(rt, ctx, 0x088756F0u, {self, 0x08CA0088u, name});
        ctx.gpr[4] = found;
        ctx.gpr[5] = 2u;
        if (found != 0u) {
            const std::uint32_t converted = call_keep(rt, ctx, 0x0886A9A0u, {found, 2u});
            ctx.gpr[4] = converted;
            if (converted != 0u) {
                ctx.gpr[5] = word(rt, frame.at(4u));
                (void)call_keep(rt, ctx, 0x08881C38u, {converted, word(rt, frame.at(4u))});
            }
        }
    }
    const std::uint32_t data = word(rt, frame.at(8u));
    ctx.gpr[3] = data;
    ctx.gpr[2] = 0x08D75298u;
    release_body(rt, ctx, data, 0x08D75298u, 0x08B93554u, frame.at(0u), 16u);
}

// func_08BAEF98(this, ref): a shared-pointer assignment: n = *ref, o = *this; with n == o nothing happens. Else the strong counter at n + 4 is incremented when n is set (ll / sc); o = *this is read again and, when set, its strong counter at o + 4 is decremented and, when it
// was 1, the virtual slot +8 of o runs on o, then the weak counter at o + 8 is decremented and, when it was 1, the slot +0xC runs on o; *this = n is stored last (after those calls). v0 = this.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): n == o (the `beq s1, s0` at 0x08BAEFB4 returns at once), a null n (the `beqz s1` at 0x08BAEFBC skips the increment) and a non-null old pointer (the `beqz s0` at 0x08BAEFE0: its strong decrement, the slot +8, the weak decrement and the slot +0xC, 0x08BAEFE8-0x08BAF040) never ran: the compared cases all assigned into an empty pointer -> the release is read from the asm only.
void func_08BAEF98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], ref = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    const std::uint32_t incoming = word(rt, ref);
    ctx.gpr[17] = incoming;
    std::uint32_t current = word(rt, self);
    ctx.gpr[16] = current;
    if (incoming != current) {
        if (incoming != 0u) {
            ctx.gpr[3] = incoming + 4u;
            put(rt, incoming + 4u, word(rt, incoming + 4u) + 1u);
            ctx.gpr[2] = word(rt, incoming + 4u);
        }
        current = word(rt, self);
        ctx.gpr[16] = current;
        if (current != 0u) {
            ctx.gpr[3] = current + 4u;
            const std::uint32_t strong = word(rt, current + 4u);
            put(rt, current + 4u, strong - 1u);
            ctx.gpr[2] = 1u;
            ctx.gpr[4] = strong;
            if (strong == 1u) {
                const std::uint32_t vtable = word(rt, current);
                const std::uint32_t function = word(rt, vtable + 8u);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                (void)call_keep(rt, ctx, function, {current});
                ctx.gpr[3] = current + 8u;
                const std::uint32_t weak = word(rt, current + 8u);
                put(rt, current + 8u, weak - 1u);
                ctx.gpr[2] = 1u;
                ctx.gpr[4] = weak;
                if (weak == 1u) {
                    const std::uint32_t final_vtable = word(rt, current);
                    const std::uint32_t final_function = word(rt, final_vtable + 0xCu);
                    ctx.gpr[3] = final_vtable;
                    ctx.gpr[2] = final_function;
                    (void)call_keep(rt, ctx, final_function, {current});
                }
            }
        }
        put(rt, self, incoming);
    }
    ctx.gpr[2] = self;
}

// func_08A6BE64(this, a1): o = *this (null: func_08A5BB7C(D_08CB05D8) runs first); a string temporary t at sp + 4 = func_08B85EB8(sp + 4, a1, sp + 1); a holder h at sp + 8 = func_08A6D2D4(sp + 8, sp + 4); r = (o's vtable slot 0x98)(o, sp + 8); func_08A5B9EC(sp + 8) and the string release of t
// (func_08B60804 / func_08B8542C(body, sp)). v0 = r.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the object *this is never null (the func_08A5BB7C assertion call skipped by the `bnez s1` at 0x08A6BE90); the shared empty body of the string (`beq s0, v0` at 0x08A6BEF0) is never met.
void func_08A6BE64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t temp = frame.at(4u), holder = frame.at(8u);
    ctx.gpr[19] = temp;
    ctx.gpr[18] = holder;
    ctx.gpr[16] = argument;
    const std::uint32_t object = word(rt, self);
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08CB05D8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB05D8u});
    const std::uint32_t vtable = word(rt, object);
    ctx.gpr[2] = vtable;
    ctx.gpr[6] = frame.at(1u);
    ctx.gpr[5] = argument;
    ctx.gpr[4] = temp;
    const std::uint32_t function = word(rt, vtable + 0x98u);
    ctx.gpr[16] = function;
    (void)call_keep(rt, ctx, 0x08B85EB8u, {temp, argument, frame.at(1u)});
    ctx.gpr[5] = temp;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A6D2D4u, {holder, temp});
    ctx.gpr[4] = object;
    ctx.gpr[5] = holder;
    const std::uint32_t result = call_keep(rt, ctx, function, {object, holder});
    ctx.gpr[4] = holder;
    ctx.gpr[17] = result;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    release_string(rt, ctx, temp, frame.at(0u));
    ctx.gpr[2] = result;
}

// func_08ABD5EC(array, count): func_08C1A0C4(D_08CFFAC4) is copied into a 0x80-byte buffer at sp (func_08B7C830(sp, text, 0x80)); then, for i = 0 while i < count (signed), token = func_08B7DCA4(i == 0 ? sp : 0, D_08CB43B8) (a strtok-like split): a token
// stores func_08B497A0(token) in array[i] and the next token is asked with a null buffer; the first missing token fills array[i..count) with -1. v0 = the number of tokens read.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): count <= 0 (the `blez s1` at 0x08ABD630 returns 0) and a split that finds all `count` tokens (the `bne s1, s2` at 0x08ABD664 falling through to the return of s2, 0x08ABD66C) never ran: every compared case ended on a missing token and filled the rest with -1.
void func_08ABD5EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t array = ctx.gpr[4], count = ctx.gpr[5];
    GuestFrame frame(ctx, 0xA0u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = array;
    ctx.gpr[18] = 0u;
    ctx.gpr[17] = count;
    ctx.gpr[4] = 0x08CFFAC4u;
    const std::uint32_t text = call_keep(rt, ctx, 0x08C1A0C4u, {0x08CFFAC4u});
    ctx.gpr[5] = text;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[6] = 0x80u;
    (void)call_keep(rt, ctx, 0x08B7C830u, {frame.at(0u), text, 0x80u});
    std::uint32_t index = 0u;
    ctx.gpr[2] = 0u;
    if (signed_of(count) <= 0) return;
    ctx.gpr[16] = array;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[19] = 0x08CB0000u;
    std::uint32_t cursor = array;
    for (;;) {
        ctx.gpr[5] = 0x08CB43B8u;
        const std::uint32_t token = call_keep(rt, ctx, 0x08B7DCA4u, {ctx.gpr[4], 0x08CB43B8u});
        if (token == 0u) {
            std::uint32_t fill = array + index * 4u;
            ctx.gpr[2] = index * 4u;
            ctx.gpr[5] = 0xFFFFFFFFu;
            std::uint32_t v = index;
            do {
                ++v;
                put(rt, fill, 0xFFFFFFFFu);
                fill += 4u;
            } while (signed_of(v) < signed_of(count));
            break;
        }
        ctx.gpr[4] = token;
        ++index;
        ctx.gpr[18] = index;
        const std::uint32_t value = call_keep(rt, ctx, 0x08B497A0u, {token});
        put(rt, cursor, value);
        ctx.gpr[4] = 0u;
        cursor += 4u;
        ctx.gpr[16] = cursor;
        if (count == index) break;
    }
    ctx.gpr[2] = index;
}

// func_0883B600(out, n, ref): a string temporary h at sp + 8 starts as the shared empty string (D_08D74FD0 + 0xC); with n > 0 and a non-null object o = *ref: o's vtable slot 0xC called with (sp + 4, o) fills a temporary t at sp + 4, func_08B855E0(sp + 8, sp + 4) copies it
// into h and t is released (func_08B60804(p - 4, -1), an answer <= 0 frees the body with func_08B8542C(body, sp + 1)); c = func_0892EE54(h's data pointer, or 0 when n <= 0 or o is null); then h is released (the same, with sp as the free argument). v0 is the last call's.
// Not registered: every case stops at a sleep that would block (not comparable).
void func_0883B600(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[5], ref = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t shared = 0x08D74FD0u;
    ctx.gpr[17] = shared;
    ctx.gpr[3] = shared + 0xCu;
    put(rt, frame.at(8u), shared + 0xCu);
    std::uint32_t text = 0u;
    if (signed_of(count) > 0) {
        const std::uint32_t object = word(rt, ref);
        ctx.gpr[5] = object;
        ctx.gpr[16] = frame.at(4u);
        if (object != 0u) {
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t function = word(rt, vtable + 0xCu);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            ctx.gpr[4] = frame.at(4u);
            (void)call_keep(rt, ctx, function, {frame.at(4u), object});
            ctx.gpr[5] = frame.at(4u);
            ctx.gpr[4] = frame.at(8u);
            (void)call_keep(rt, ctx, 0x08B855E0u, {frame.at(8u), frame.at(4u)});
            const std::uint32_t data = word(rt, frame.at(4u));
            ctx.gpr[4] = data;
            release_body(rt, ctx, data, shared, 0x08B8542Cu, frame.at(1u), 16u);
            text = word(rt, frame.at(8u));
        }
    }
    ctx.gpr[4] = text;
    (void)call_keep(rt, ctx, 0x0892EE54u, {text});
    const std::uint32_t data = word(rt, frame.at(8u));
    ctx.gpr[4] = data;
    release_body(rt, ctx, data, shared, 0x08B8542Cu, frame.at(0u), 16u);
}

// func_0889DA58(out, count): p = func_08B62344(0x34); func_08B28DCC(p); p[0x2C] = count, p[0x28] = 0, p[0x30] = 0, func_08B344E0(p); with count != 0 p[0x28] = func_08B6461C(count * 8); func_08B3457C(p); out[0] = p;
// the guarded static D_08D75EF8 (guard byte, func_08B6441C / func_08B64494) holds D_08D75F00 (set to D_08CC23F0 on the first call); out[1] = D_08D75F00. v0 is the last call's (the lui of the guard's high half when it was set).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): count = 0 (the `beqz v0` at 0x0889DAA0 skips func_08B6461C), the guard already set (`bnez a1` at 0x0889DAD4) and func_08B6441C answering 0 (`beqz v0` at 0x0889DAEC) never ran: the first-use path of the guarded static (D_08D75EF8) ran in every case.
void func_0889DA58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5];
    const std::uint32_t guard = 0x08D75EF8u, holder = 0x08D75F00u;
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = out;
    ctx.gpr[16] = count;
    ctx.gpr[4] = 0x34u;
    const std::uint32_t p = call_keep(rt, ctx, 0x08B62344u, {0x34u});
    ctx.gpr[17] = p;
    ctx.gpr[4] = p;
    (void)call_keep(rt, ctx, 0x08B28DCCu, {p});
    ctx.gpr[4] = p;
    put(rt, p + 0x2Cu, count);
    put(rt, p + 0x28u, 0u);
    put(rt, p + 0x30u, 0u);
    (void)call_keep(rt, ctx, 0x08B344E0u, {p});
    const std::uint32_t slots = word(rt, p + 0x2Cu);
    ctx.gpr[2] = slots;
    ctx.gpr[4] = slots << 3;
    if (slots != 0u) {
        const std::uint32_t table = call_keep(rt, ctx, 0x08B6461Cu, {slots << 3});
        put(rt, p + 0x28u, table);
    }
    ctx.gpr[4] = p;
    (void)call_keep(rt, ctx, 0x08B3457Cu, {p});
    put(rt, out, p);
    ctx.gpr[2] = 0x08D70000u;
    ctx.gpr[3] = 0x08D70000u;
    const std::uint32_t flag = static_cast<std::uint32_t>(byte_at(rt, guard));
    ctx.gpr[5] = flag;
    ctx.gpr[16] = guard;
    ctx.gpr[17] = holder;
    ctx.gpr[4] = guard;
    if (flag == 0u) {
        const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
        ctx.gpr[5] = 0x08D70000u;
        ctx.gpr[17] = holder;
        ctx.gpr[4] = guard;
        if (acquired != 0u) {
            ctx.gpr[2] = 0x08CC23F0u;
            put(rt, holder, 0x08CC23F0u);
            (void)call_keep(rt, ctx, 0x08B64494u, {guard});
        }
    }
    put(rt, out + 4u, holder);
}

// func_088797F8(this): a string temporary t at sp + 0x10 (func_08AB85F0); func_088755E4(this, sp + 4, t, 3); n = func_08AB856C(t); with the word at sp + 0x18 at least 3: p = func_088756F0(this, 0, n), a non-null p: q = func_08B93734(func_0886A9A0(p, 4)),
// a non-null q: func_08870448(q, f12 = [sp + 4], f13 = [sp + 8], f14 = [sp + 0xC]); then the release of t (shared empty body D_08D75298, func_08B60804 / func_08B93554(body, sp)). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the word at sp + 0x18 is always at least 3 and both lookups answer non-null (the `bnez v0` at 0x08879840, the `beqz v0` at 0x08879854 and 0x0887986C would skip the rest); the shared empty body of the string is never met (`beq s0, v0` at 0x08879894).
void func_088797F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(0x10u);
    ctx.gpr[16] = holder;
    ctx.gpr[17] = self;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08AB85F0u, {holder});
    ctx.gpr[6] = holder;
    ctx.gpr[4] = self;
    ctx.gpr[5] = frame.at(4u);
    ctx.gpr[7] = 3u;
    (void)call_keep(rt, ctx, 0x088755E4u, {self, frame.at(4u), holder, 3u});
    ctx.gpr[4] = holder;
    const std::uint32_t name = call_keep(rt, ctx, 0x08AB856Cu, {holder});
    ctx.gpr[6] = name;
    const std::uint32_t level = word(rt, frame.at(0x18u));
    ctx.gpr[2] = signed_of(level) < 3 ? 1u : 0u;
    if (signed_of(level) >= 3) {
        ctx.gpr[4] = self;
        ctx.gpr[5] = 0u;
        const std::uint32_t found = call_keep(rt, ctx, 0x088756F0u, {self, 0u, name});
        ctx.gpr[4] = found;
        if (found != 0u) {
            ctx.gpr[5] = 4u;
            const std::uint32_t converted = call_keep(rt, ctx, 0x0886A9A0u, {found, 4u});
            ctx.gpr[4] = converted;
            const std::uint32_t target = call_keep(rt, ctx, 0x08B93734u, {converted});
            const float x = float_at(rt, frame.at(4u)), y = float_at(rt, frame.at(8u)), z = float_at(rt, frame.at(0xCu));
            ctx.fpr[12] = x;
            ctx.fpr[13] = y;
            ctx.fpr[14] = z;
            if (target != 0u) {
                ctx.gpr[4] = target;
                (void)call_keep(rt, ctx, 0x08870448u, {target}, {x, y, z});
            }
        }
    }
    const std::uint32_t data = word(rt, frame.at(0x10u));
    ctx.gpr[4] = data;
    ctx.gpr[2] = 0x08D75298u;
    release_body(rt, ctx, data, 0x08D75298u, 0x08B93554u, frame.at(0u), 16u);
}

// func_088E537C(this, a1): inner = this[0]; with inner[0xA4] set nothing happens (v0 untouched). Else o = (a1's vtable slot 0x18 called with (a1)) is stored in inner[0xA4] and (this[0]'s inner[0xA4]) =: m gets its vtable slot 8 called with (m, 0x1010), slot 0x14 with (m, this),
// slot 0x18 with (m, D_08BB86BC); this[0]'s words at +0x50C and +0x508 are set to 0 (the old inner[0xA4]); the tail call is slot 0x28 of this[0]'s inner[0xA4] with (it).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): inner[0xA4] already set (the `bnez s2` at 0x088E539C returns at once with v0 untouched) never ran: every compared case took the creation path.
void func_088E537C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    std::uint32_t last = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[16] = self;
        const std::uint32_t inner = word(rt, self);
        ctx.gpr[17] = inner;
        const std::uint32_t existing = word(rt, inner + 0xA4u);
        ctx.gpr[18] = existing;
        ctx.gpr[4] = argument;
        if (existing != 0u) return;
        std::uint32_t vtable = word(rt, argument);
        std::uint32_t function = word(rt, vtable + 0x18u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        const std::uint32_t created = call_keep(rt, ctx, function, {argument});
        put(rt, inner + 0xA4u, created);
        std::uint32_t member = word(rt, word(rt, self) + 0xA4u);
        ctx.gpr[4] = member;
        vtable = word(rt, member);
        function = word(rt, vtable + 8u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[5] = 0x1010u;
        (void)call_keep(rt, ctx, function, {member, 0x1010u});
        member = word(rt, word(rt, self) + 0xA4u);
        ctx.gpr[4] = member;
        vtable = word(rt, member);
        function = word(rt, vtable + 0x14u);
        ctx.gpr[6] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[5] = self;
        (void)call_keep(rt, ctx, function, {member, self});
        ctx.gpr[6] = word(rt, self);
        member = word(rt, ctx.gpr[6] + 0xA4u);
        ctx.gpr[4] = member;
        ctx.gpr[5] = 0x08BB86BCu;
        vtable = word(rt, member);
        function = word(rt, vtable + 0x18u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        (void)call_keep(rt, ctx, function, {member, 0x08BB86BCu});
        put(rt, word(rt, self) + 0x50Cu, existing);
        put(rt, word(rt, self) + 0x508u, existing);
        last = word(rt, word(rt, self) + 0xA4u);
        ctx.gpr[4] = last;
        vtable = word(rt, last);
        ctx.gpr[2] = vtable;
        ctx.gpr[25] = word(rt, vtable + 0x28u);
    }
    tail_call(ctx, ctx.gpr[25], {last, 0x08BB86BCu});
}

// func_089C1368(out, a1, a2, ref): a holder h at sp + 8 = func_089BEE4C(sp + 8) (its object o is *h, null: func_08A5BB7C(D_08CA5EB4) first); p = *ref (null: func_08A5BB7C(D_08CA5E6C) first); a string temporary t at sp + 4 is filled by p's vtable slot 0xC called with (sp + 4, p);
// func_089C12F4(o, sp + 4); t is released (func_08B60804 / func_08B8542C(body, sp)); func_08A5B9EC(sp + 8). v0 is the last call's.
// Not registered: every case stops on a held LwMutex (not comparable).
void func_089C1368(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(8u), temp = frame.at(4u);
    ctx.gpr[19] = holder;
    ctx.gpr[4] = holder;
    ctx.gpr[18] = temp;
    ctx.gpr[16] = ref;
    (void)call_keep(rt, ctx, 0x089BEE4Cu, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08CA5EB4u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA5EB4u});
    const std::uint32_t source = word(rt, ref);
    ctx.gpr[16] = source;
    ctx.gpr[4] = 0x08CA5E6Cu;
    if (source == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA5E6Cu});
    const std::uint32_t vtable = word(rt, source);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[5] = source;
    ctx.gpr[2] = function;
    ctx.gpr[4] = temp;
    (void)call_keep(rt, ctx, function, {temp, source});
    ctx.gpr[4] = object;
    ctx.gpr[5] = temp;
    (void)call_keep(rt, ctx, 0x089C12F4u, {object, temp});
    release_string(rt, ctx, temp, frame.at(0u));
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_089C151C(out, a1, level, ref): with level < 2 (signed) nothing happens (v0 untouched). Else the holders h0 at sp = func_089BEE4C(sp), h4 at sp + 4 = func_089AB370(sp + 4, ref), h8 at sp + 8 = func_08A6C410(sp + 8, ref + 4); o = *h0 and p = *h8 (null: func_08A5BB7C(D_08CA5EB4) / (D_08CA60DC) first);
// n = func_08A6BFFC(p); func_089C1440(o, *h4, n); then func_08A5B9EC on h8, h4 and h0. v0 is the last call's.
// Not registered: every case would start a thread (not comparable).
void func_089C151C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t level = ctx.gpr[6], ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t h0 = frame.at(0u), h4 = frame.at(4u), h8 = frame.at(8u);
    ctx.gpr[6] = signed_of(level) < 2 ? 1u : 0u;
    ctx.gpr[4] = h0;
    ctx.gpr[20] = h4;
    ctx.gpr[19] = h8;
    ctx.gpr[16] = ref;
    if (signed_of(level) < 2) return;
    (void)call_keep(rt, ctx, 0x089BEE4Cu, {h0});
    ctx.gpr[4] = h4;
    ctx.gpr[5] = ref;
    (void)call_keep(rt, ctx, 0x089AB370u, {h4, ref});
    ctx.gpr[4] = h8;
    ctx.gpr[5] = ref + 4u;
    (void)call_keep(rt, ctx, 0x08A6C410u, {h8, ref + 4u});
    const std::uint32_t first = word(rt, h0);
    ctx.gpr[18] = first;
    ctx.gpr[4] = 0x08CA5EB4u;
    if (first == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA5EB4u});
    const std::uint32_t second = word(rt, h8);
    ctx.gpr[16] = second;
    const std::uint32_t middle = word(rt, h4);
    ctx.gpr[17] = middle;
    ctx.gpr[4] = 0x08CA60DCu;
    if (second == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA60DCu});
    ctx.gpr[4] = second;
    const std::uint32_t number = call_keep(rt, ctx, 0x08A6BFFCu, {second});
    ctx.gpr[6] = number;
    ctx.gpr[5] = middle;
    ctx.gpr[4] = first;
    (void)call_keep(rt, ctx, 0x089C1440u, {first, middle, number});
    ctx.gpr[4] = h8;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {h8});
    ctx.gpr[4] = h4;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {h4});
    ctx.gpr[4] = h0;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {h0});
}

// func_089EF8A8(this): this[0] = D_08CBF080, this[0x38] = D_08CBF2BC; func_08A5B9EC(this + 0x244), func_08A5B9EC(this + 0x240); the string bodies of this[0x23C] and this[0x238] are released (shared empty body D_08D74FD0: func_08B60804(p - 4, -1), an answer <= 0 frees it with
// func_08B8542C(body, sp + 1) for the first and (body, sp) for the second); func_089E2430(this); the tail-position call func_08A5BC54(this, 0xE70, 4). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the shared empty body of either string (the `beq s1, s2` skips at 0x089EF8FC and 0x089EF92C) is never met -> only the release of non-shared bodies ran.
void func_089EF8A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    put(rt, self, 0x08CBF080u);
    put(rt, self + 0x38u, 0x08CBF2BCu);
    ctx.gpr[4] = self + 0x244u;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {self + 0x244u});
    ctx.gpr[4] = self + 0x240u;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {self + 0x240u});
    ctx.gpr[18] = 0x08D74FD0u;
    ctx.gpr[3] = word(rt, self + 0x23Cu);
    release_body(rt, ctx, word(rt, self + 0x23Cu), 0x08D74FD0u, 0x08B8542Cu, frame.at(1u), 17u);
    ctx.gpr[2] = word(rt, self + 0x238u);
    release_body(rt, ctx, word(rt, self + 0x238u), 0x08D74FD0u, 0x08B8542Cu, frame.at(0u), 17u);
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x089E2430u, {self});
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0xE70u;
    ctx.gpr[6] = 4u;
    (void)call_keep(rt, ctx, 0x08A5BC54u, {self, 0xE70u, 4u});
}

// func_08A52F9C(out, a1, n, ref): with n > 0 (signed) o = *ref (null: func_08A5BB7C(D_08CAF4B8) first) and m = o's vtable slot 0x3C called with (o), else m = -1; the holder h at sp = func_08A52A18(sp, a1) (its object p = *h, null: func_08A5BB7C(D_08CAF4A8) first);
// the temporary at sp + 4 = func_08A52D3C(sp + 4, p, m, 0, 0); unless `out` is sp + 4 func_08A5BAD8(out, [sp + 4]); func_08A5B9EC(sp + 4) and func_08A5B9EC(sp). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): n > 0 (the object *ref, its vtable slot 0x3C, 0x08A52FCC-0x08A52FF4: the null object of the assertion call skipped by the `bnez s0` at 0x08A52FD4) never ran: every compared case had n <= 0 and used -1; the second object is never null (`bnez s0` at 0x08A5300C) and `out` is never the holder at sp + 4 (`beq s4, s2` at 0x08A53034).
void func_08A52F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argument = ctx.gpr[5], count = ctx.gpr[6], ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t result_slot = frame.at(4u), holder = frame.at(0u);
    ctx.gpr[20] = out;
    ctx.gpr[19] = 0xFFFFFFFFu;
    ctx.gpr[18] = result_slot;
    ctx.gpr[17] = argument;
    std::uint32_t mode = 0xFFFFFFFFu;
    if (signed_of(count) > 0) {
        const std::uint32_t object = word(rt, ref);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08CAF4B8u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAF4B8u});
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t function = word(rt, vtable + 0x3Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = object;
        mode = call_keep(rt, ctx, function, {object});
        ctx.gpr[19] = mode;
    }
    ctx.gpr[4] = holder;
    ctx.gpr[5] = argument;
    (void)call_keep(rt, ctx, 0x08A52A18u, {holder, argument});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08CAF4A8u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAF4A8u});
    ctx.gpr[4] = result_slot;
    ctx.gpr[5] = object;
    ctx.gpr[6] = mode;
    ctx.gpr[7] = 0u;
    ctx.gpr[8] = 0u;
    (void)call_keep(rt, ctx, 0x08A52D3Cu, {result_slot, object, mode, 0u, 0u});
    ctx.gpr[4] = out;
    if (out != result_slot) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result_slot)});
    ctx.gpr[4] = result_slot;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {result_slot});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_0888CFCC(this, a1, flag, ptr, t0): a record at sp: [0] = 2, [8] = a1, byte [0xC] = (flag != 0), [0x10]:[0x14] = func_088A5EE0(ptr + 8, a1)'s v0:v1, and at sp + 0x18 a 0x80-byte string: with t0 non-zero func_08B7C830(sp + 0x18, func_08896FB8(t0 + 0x20), 0x80), else its first
// byte 0; func_088936A4(this[0x18], sp); o = func_088DA588(this[4], a1); a non-null o has its vtable slot 0x10 called with (o, 0). v0 is the last call's (0 when o is null).
// Not registered: no compared case changed memory (4 compared, all equal).
void func_0888CFCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5], flag = ctx.gpr[6], ptr = ctx.gpr[7], extra = ctx.gpr[8];
    GuestFrame frame(ctx, 0xB0u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    ctx.gpr[18] = argument;
    ctx.gpr[16] = extra;
    ctx.gpr[2] = 2u;
    put(rt, frame.at(0u), 2u);
    ctx.gpr[6] = flag != 0u ? 1u : 0u;
    rt.memory().store8(frame.at(0xCu), static_cast<std::uint8_t>(ctx.gpr[6]));
    put(rt, frame.at(8u), argument);
    ctx.gpr[4] = ptr + 8u;
    const GuestResult wide = call_keep(rt, ctx, 0x088A5EE0u, {ptr + 8u, argument}, {});
    ctx.gpr[4] = extra + 0x20u;
    put(rt, frame.at(0x10u), wide.v0);
    put(rt, frame.at(0x14u), wide.v1);
    if (extra != 0u) {
        const std::uint32_t text = call_keep(rt, ctx, 0x08896FB8u, {extra + 0x20u});
        ctx.gpr[5] = text;
        ctx.gpr[4] = frame.at(0x18u);
        ctx.gpr[6] = 0x80u;
        (void)call_keep(rt, ctx, 0x08B7C830u, {frame.at(0x18u), text, 0x80u});
    } else {
        rt.memory().store8(frame.at(0x18u), 0u);
    }
    const std::uint32_t head = word(rt, self + 0x18u);
    ctx.gpr[4] = head;
    ctx.gpr[5] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x088936A4u, {head, frame.at(0u)});
    const std::uint32_t owner = word(rt, self + 4u);
    ctx.gpr[4] = owner;
    ctx.gpr[5] = argument;
    const std::uint32_t object = call_keep(rt, ctx, 0x088DA588u, {owner, argument});
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;
    if (object == 0u) return;
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0x10u);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = function;
    (void)call_keep(rt, ctx, function, {object, 0u});
}

// func_08A57D74(out, a1, mode, extra): mode 0: a holder h at sp + 4 = func_08A57854(sp + 4), its object o = *h (null: func_08A5BB7C(D_08CAF7EC) first); a result holder r at sp + 8 = func_08A56884(sp + 8, o); unless `out` is sp + 8 func_08A5BAD8(out, [sp + 8]); func_08A5B9EC(sp + 8) and func_08A5B9EC(sp + 4).
// mode 1: a holder at sp = func_08A57854(sp), its object o = *h (null: func_08A5BB7C first), func_08A56824(o, extra) and func_08A5B9EC(sp). Any other mode returns at once with v0 = 1. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): mode 0 (the whole first half, 0x08A57D98-0x08A57DEC: both holders, the assertion call, the copy and the two releases) and the modes above 1 (the `bne a2, v0` at 0x08A57DF8 returns with v0 = 1) never ran: every compared case was mode 1, whose object is never null (`bnel s0` at 0x08A57E0C) -> mode 0 is read from the asm only.
void func_08A57D74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], extra = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = out;
    ctx.gpr[17] = extra;
    if (mode == 0u) {
        const std::uint32_t first = frame.at(4u), second = frame.at(8u);
        ctx.gpr[19] = first;
        ctx.gpr[4] = first;
        (void)call_keep(rt, ctx, 0x08A57854u, {first});
        const std::uint32_t object = word(rt, first);
        ctx.gpr[17] = object;
        ctx.gpr[16] = second;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAF7ECu});
        ctx.gpr[5] = object;
        ctx.gpr[4] = second;
        (void)call_keep(rt, ctx, 0x08A56884u, {second, object});
        ctx.gpr[5] = word(rt, second);
        ctx.gpr[4] = out;
        if (out != second) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, second)});
        ctx.gpr[4] = second;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
        ctx.gpr[4] = first;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {first});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A57854u, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08CAF7ECu;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAF7ECu});
    }
    ctx.gpr[4] = object;
    ctx.gpr[5] = extra;
    (void)call_keep(rt, ctx, 0x08A56824u, {object, extra});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08BC0808(a0, a1, a2, a3, t0, t1): a copy c of the shared pointer pair at t1 ([0], [4]) is made at sp (the counter at c[4] + 4 incremented when c[4] is non-null, ll / sc); func_08961928(a1, a2, D_088FC2FC, a0, a3, t0, sp, a0) runs;
// then the pair's release: s = c[4], when set: the strong counter at s + 4 is decremented (ll / sc) and, when it was 1, the virtual slot +8 of s runs on s, then the weak counter at s + 8 is decremented and, when it was 1, the slot +0xC runs on s. v0 is the last operation's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-null pair (the `beqz v1` at 0x08BC0834 skips the counter increment; the release of the copy, 0x08BC086C-0x08BC08D0: the strong decrement, the slot +8, the weak decrement, the slot +0xC) never ran: every compared case copied an empty pair -> the increment and the release are read from the asm only.
void func_08BC0808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8], t1 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t pair_low = word(rt, t1), pair_high = word(rt, t1 + 4u);
    put(rt, frame.at(4u), pair_high);
    put(rt, frame.at(0u), pair_low);
    if (pair_high != 0u) {
        put(rt, pair_high + 4u, word(rt, pair_high + 4u) + 1u);
        ctx.gpr[2] = 1u;
    }
    ctx.gpr[4] = a1;
    ctx.gpr[5] = a2;
    ctx.gpr[6] = 0x088FC2FCu;
    ctx.gpr[7] = a0;
    ctx.gpr[8] = a3;
    ctx.gpr[9] = t0;
    ctx.gpr[10] = frame.at(0u);
    ctx.gpr[11] = a0;
    (void)call_keep(rt, ctx, 0x08961928u, {a1, a2, 0x088FC2FCu, a0, a3, t0, frame.at(0u), a0});
    const std::uint32_t shared = word(rt, frame.at(4u));
    ctx.gpr[16] = shared;
    if (shared == 0u) return;
    ctx.gpr[3] = shared + 4u;
    const std::uint32_t strong = word(rt, shared + 4u);
    put(rt, shared + 4u, strong - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = strong;
    if (strong != 1u) return;
    const std::uint32_t vtable = word(rt, shared);
    const std::uint32_t function = word(rt, vtable + 8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    (void)call_keep(rt, ctx, function, {shared});
    ctx.gpr[3] = shared + 8u;
    const std::uint32_t weak = word(rt, shared + 8u);
    put(rt, shared + 8u, weak - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = weak;
    if (weak != 1u) return;
    const std::uint32_t final_vtable = word(rt, shared);
    const std::uint32_t final_function = word(rt, final_vtable + 0xCu);
    ctx.gpr[3] = final_vtable;
    ctx.gpr[2] = final_function;
    ctx.gpr[4] = shared;
    (void)call_keep(rt, ctx, final_function, {shared});
}

// func_08809650(this): the guarded static D_08CE2910 (guard byte, func_08B6441C / func_08B64494) fills the table at D_08CE2918 on its first use: [0] = func_0880DB48(), [1] = func_08809AEC(), [2] = func_0880B5CC() (each stored before the next call);
// then, when [0] is non-zero, for each table entry e (a zero word ends the table): o = this[0] (null: func_08A5BB7C(D_08C98110) first) and func_08A7CC30(o, e); finally func_08809738(this), func_08809730(this) and the tail call func_08809980(this).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard set on entry (the `bnez v1` at 0x0880967C), func_08B6441C answering 0 (0x0880968C), an empty table head (`beqz v0` at 0x088096BC) and a null this[0] (the assertion call skipped by the `bnez s0` at 0x088096D0) never ran.
void func_08809650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t guard = 0x08CE2910u, table = 0x08CE2918u;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[19] = self;
        ctx.gpr[18] = guard;
        ctx.gpr[17] = 0x08CE0000u;
        const std::uint32_t flag = static_cast<std::uint32_t>(byte_at(rt, guard));
        ctx.gpr[3] = flag;
        if (flag == 0u) {
            ctx.gpr[4] = guard;
            const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
            ctx.gpr[2] = word(rt, table);
            if (acquired != 0u) {
                ctx.gpr[16] = table;
                put(rt, table, call_keep(rt, ctx, 0x0880DB48u, {}));
                put(rt, table + 4u, call_keep(rt, ctx, 0x08809AECu, {}));
                put(rt, table + 8u, call_keep(rt, ctx, 0x0880B5CCu, {}));
                ctx.gpr[4] = guard;
                (void)call_keep(rt, ctx, 0x08B64494u, {guard});
            }
        }
        const std::uint32_t head = word(rt, table);
        ctx.gpr[2] = head;
        ctx.gpr[17] = table;
        if (head != 0u) {
            ctx.gpr[20] = 0x08CA0000u;
            std::uint32_t cursor = table;
            std::uint32_t object = word(rt, self);
            for (;;) {
                ctx.gpr[16] = object;
                ctx.gpr[18] = cursor;
                ctx.gpr[4] = 0x08C98110u;
                if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C98110u});
                const std::uint32_t entry = word(rt, cursor);
                ctx.gpr[5] = entry;
                ctx.gpr[4] = object;
                ctx.gpr[17] = cursor + 4u;
                (void)call_keep(rt, ctx, 0x08A7CC30u, {object, entry});
                const std::uint32_t next = word(rt, cursor + 4u);
                ctx.gpr[2] = next;
                if (next == 0u) break;
                cursor += 4u;
                object = word(rt, self);
            }
        }
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x08809738u, {self});
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x08809730u, {self});
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08809980u, {self});
}

// func_08A1503C(out, a1, level, ref): with level <= 0 (signed) nothing happens (v0 untouched). Else a holder h at sp + 8 = func_08A14610(sp + 8) (its object o = *h, null: func_08A5BB7C(D_08CA9A6C) first); p = *ref (null: func_08A5BB7C(D_08CA9A08) first);
// a string temporary t at sp + 4 is filled by p's vtable slot 0xC called with (sp + 4, p); func_08A142B8(o, sp + 4); t is released (func_08B60804 / func_08B8542C(body, sp)); func_08A5B9EC(sp + 8). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): level <= 0 (the `blez a2` at 0x08A15060 returns with v0 untouched) and the two null objects (the assertion calls skipped by the `bnez` at 0x08A15078 and 0x08A15090); the shared empty body of the string is never met (`beq s0, v0` at 0x08A150D4).
void func_08A1503C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t level = ctx.gpr[6], ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(8u), temp = frame.at(4u);
    ctx.gpr[19] = holder;
    ctx.gpr[4] = holder;
    ctx.gpr[18] = temp;
    ctx.gpr[16] = ref;
    if (signed_of(level) <= 0) return;
    (void)call_keep(rt, ctx, 0x08A14610u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08CA9A6Cu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA9A6Cu});
    const std::uint32_t source = word(rt, ref);
    ctx.gpr[16] = source;
    ctx.gpr[4] = 0x08CA9A08u;
    if (source == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA9A08u});
    const std::uint32_t vtable = word(rt, source);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[5] = source;
    ctx.gpr[2] = function;
    ctx.gpr[4] = temp;
    (void)call_keep(rt, ctx, function, {temp, source});
    ctx.gpr[4] = object;
    ctx.gpr[5] = temp;
    (void)call_keep(rt, ctx, 0x08A142B8u, {object, temp});
    release_string(rt, ctx, temp, frame.at(0u));
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08B9355C(this, node): destroys a tree of nodes (nothing for a null node), left to right: for each node n (walking n[8] = the next/left node in a loop): func_08B9355C(this, n[0xC]) on n[0xC] (recursive), the shared pointer p = n[0x18], when set: its weak-style counter at p + 8 is decremented
// (ll / sc) and, when it was 1, the virtual slot +0xC runs on p; the string at n[0x10] is released (shared empty body D_08D75298: func_08B60804(data - 4, -1), an answer <= 0 frees it with func_08B93554(body, sp)); func_08B35254(n). v0 is the last call's.
// Not registered: compared only with a null node (the early return: the tree walk, the shared-pointer release and the string release never ran) and no compared case changed memory.
void func_08B9355C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = self;
    ctx.gpr[16] = node;
    if (node == 0u) return;
    ctx.gpr[20] = 0x08D75298u;
    ctx.gpr[21] = 1u;
    for (;;) {
        const std::uint32_t right = word(rt, node + 0xCu);
        ctx.gpr[5] = right;
        ctx.gpr[4] = self;
        ctx.gpr[17] = node + 0x10u;
        (void)call_keep(rt, ctx, 0x08B9355Cu, {self, right});
        const std::uint32_t shared = word(rt, node + 0x18u);
        ctx.gpr[4] = shared;
        const std::uint32_t next = word(rt, node + 8u);
        ctx.gpr[18] = next;
        if (shared != 0u) {
            ctx.gpr[3] = shared + 8u;
            const std::uint32_t counter = word(rt, shared + 8u);
            put(rt, shared + 8u, counter - 1u);
            ctx.gpr[2] = 1u;
            ctx.gpr[5] = counter;
            if (counter == 1u) {
                const std::uint32_t vtable = word(rt, shared);
                const std::uint32_t function = word(rt, vtable + 0xCu);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                (void)call_keep(rt, ctx, function, {shared});
            }
        }
        const std::uint32_t data = word(rt, node + 0x10u);
        ctx.gpr[4] = data;
        release_body(rt, ctx, data, 0x08D75298u, 0x08B93554u, frame.at(0u), 17u);
        ctx.gpr[4] = node;
        (void)call_keep(rt, ctx, 0x08B35254u, {node});
        if (next == 0u) break;
        node = next;
        ctx.gpr[16] = node;
    }
}

// func_088A23EC(this, index): s = the signed half-word at this[0x10] + 2 index; with s < 0 nothing else happens (v0 = s * 0x430). Else func_088A3978(this[8] + s * 0x430), that half-word is set to -1, the byte this[0xC] + s = 0, this[0x1C] -= 1 and func_0892C034(this[0x60] + s * 20)
// (the decrement is stored before the call); with the byte this[0x18] set, a tail call func_088A2364(this[0x3C], index); else v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): s < 0 (the `bltz s0` at 0x088A2430 returns with v0 = s * 0x430) and the byte this[0x18] set (the tail call func_088A2364, 0x088A248C-0x088A24AC) never ran: every compared case ended with that byte clear (v0 = 0) -> the tail call is read from the asm only.
void func_088A23EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[20] = index;
        const std::uint32_t scaled = index << 1;
        ctx.gpr[19] = scaled;
        ctx.gpr[17] = self;
        const std::uint32_t entry = word(rt, self + 0x10u) + scaled;
        const std::uint32_t slot = static_cast<std::uint32_t>(static_cast<std::int32_t>(half_at(rt, entry)));
        ctx.gpr[16] = slot;
        const std::uint32_t times4 = slot << 2;
        ctx.gpr[18] = times4;
        const std::uint32_t offset = ((times4 + (slot << 6)) - slot) << 4;
        ctx.gpr[2] = offset;
        if (signed_of(slot) < 0) return;
        const std::uint32_t record = word(rt, self + 8u) + offset;
        ctx.gpr[4] = record;
        (void)call_keep(rt, ctx, 0x088A3978u, {record});
        rt.memory().store16(word(rt, self + 0x10u) + scaled, 0xFFFFu);
        const std::uint32_t flags = word(rt, self + 0xCu) + slot;
        rt.memory().store8(flags, 0u);
        const std::uint32_t used = word(rt, self + 0x1Cu) - 1u;
        const std::uint32_t area = word(rt, self + 0x60u) + times4 + (slot << 4);
        ctx.gpr[4] = area;
        put(rt, self + 0x1Cu, used);
        (void)call_keep(rt, ctx, 0x0892C034u, {area});
        const std::uint32_t flag = byte_u(rt, self + 0x18u);
        ctx.gpr[2] = flag;
        ctx.gpr[5] = index;
        if (flag == 0u) return;
        ctx.gpr[4] = word(rt, self + 0x3Cu);
        tail = true;
    }
    if (tail) tail_call(ctx, 0x088A2364u, {ctx.gpr[4], index});
}

// func_08993FCC(this): a string temporary t at sp + 4 = func_08B85EB8(sp + 4, D_08CA4160, sp + 1); this's vtable slot 0x108 called with (this, sp + 4); t is released (func_08B60804 / func_08B8542C(body, sp)); r = func_08A0CF80(), func_08A5BA08(r) when r is set; the old object this[0x38],
// when set, goes to func_08A5B930; this[0x38] = r; func_08B878E8(this, D_08CA416C, D_08993CAC, D_08993CAC). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null r (the `beqz v0` at 0x08994054 skips func_08A5BA08) and a non-empty old object this[0x38] (the `beqz v0` at 0x08994068: func_08A5B930 never ran); the shared empty body of the string (`beq s0, v0` at 0x08994024) is never met.
void func_08993FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t temp = frame.at(4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = temp;
    const std::uint32_t vtable = word(rt, self);
    const std::uint32_t function = word(rt, vtable + 0x108u);
    ctx.gpr[17] = function;
    ctx.gpr[6] = frame.at(1u);
    ctx.gpr[5] = 0x08CA4160u;
    ctx.gpr[4] = temp;
    (void)call_keep(rt, ctx, 0x08B85EB8u, {temp, 0x08CA4160u, frame.at(1u)});
    ctx.gpr[5] = temp;
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, function, {self, temp});
    release_string(rt, ctx, temp, frame.at(0u));
    const std::uint32_t created = call_keep(rt, ctx, 0x08A0CF80u, {});
    ctx.gpr[16] = created;
    ctx.gpr[4] = created;
    if (created != 0u) (void)call_keep(rt, ctx, 0x08A5BA08u, {created});
    const std::uint32_t old = word(rt, self + 0x38u);
    ctx.gpr[2] = old;
    ctx.gpr[4] = old;
    if (old != 0u) (void)call_keep(rt, ctx, 0x08A5B930u, {old});
    ctx.gpr[6] = 0x08993CACu;
    ctx.gpr[5] = 0x08CA416Cu;
    put(rt, self + 0x38u, created);
    ctx.gpr[4] = self;
    ctx.gpr[7] = 0x08993CACu;
    (void)call_keep(rt, ctx, 0x08B878E8u, {self, 0x08CA416Cu, 0x08993CACu, 0x08993CACu});
}

// func_08ABF590(this): func_08B344E0(this + 0x11C); for each of the 16 slots s = this + 0x1C + 0x10 i: s[4] = 0; with s[0xC] set and s[8] (an array of 32-byte elements, the count at array[-1]) non-null the elements are destroyed from the last to the first (vtable slot 0 called
// with the element) and the block freed with func_08B61FC0(first - 4), then the block s[0xC], when set, with func_08B61FC0, and s[8] = s[0xC] = 0; the tail call is func_08B3457C(this + 0x11C).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null array s[8] (the `beql a1` at 0x08ABF5DC skips the slot) and an empty array (`beq a1, s0` at 0x08ABF5F4) never ran, and the block s[0xC] is never null after the destruction (the `beql a0` at 0x08ABF62C would skip the free); slots with s[0xC] = 0 (the `beqz v0` at 0x08ABF5D0) do occur.
void func_08ABF590(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        const std::uint32_t lock = self + 0x11Cu;
        ctx.gpr[21] = lock;
        ctx.gpr[16] = self + 0x1Cu;
        ctx.gpr[4] = lock;
        ctx.gpr[20] = 0x10u;
        ctx.gpr[19] = 0u;
        ctx.gpr[18] = self + 0x1Cu;
        (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
        std::uint32_t slot = self + 0x1Cu;
        for (std::uint32_t index = 0u; index < 0x10u; ++index, slot += 0x10u) {
            ctx.gpr[18] = slot;
            ctx.gpr[19] = index;
            const std::uint32_t block = word(rt, slot + 0xCu);
            ctx.gpr[2] = block;
            put(rt, slot + 4u, 0u);
            if (block != 0u) {
                const std::uint32_t array = word(rt, slot + 8u);
                ctx.gpr[5] = array;
                if (array == 0u) {
                    ctx.gpr[19] = index + 1u;
                    continue;
                }
                destroy_elements(rt, ctx, slot + 8u, array, 0x20u);
                const std::uint32_t buffer = word(rt, slot + 0xCu);
                ctx.gpr[4] = buffer;
                if (buffer != 0u) (void)call_keep(rt, ctx, 0x08B61FC0u, {buffer});
                put(rt, slot + 8u, 0u);
                put(rt, slot + 0xCu, 0u);
            }
            ctx.gpr[19] = index + 1u;
        }
        ctx.gpr[18] = slot;
        ctx.gpr[19] = 0x10u;
        ctx.gpr[4] = lock;
    }
    tail_call(ctx, 0x08B3457Cu, {self + 0x11Cu});
}

// func_08ABECA0(this): func_08B344E0(this + 0x11C); for each of the 16 slots s = this + 0x1C + 0x10 i: s[4] = 0; with the pointer table s[0xC] set and the block s[8] non-null, the table entries t[4 j] = s[8] + 32 j are filled for j < s[0] (unsigned, at least once when s[0] is non-zero);
// func_08B3457C(this + 0x11C); func_08ABF6E0(this + 0x144, 0); the words at this + 0xC .. this + 0x18 = 0. v0 = 4.
// Not registered: no compared case changed memory (10 compared, all equal).
void func_08ABECA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const std::uint32_t lock = self + 0x11Cu;
    ctx.gpr[18] = lock;
    ctx.gpr[17] = self;
    ctx.gpr[16] = self + 0x1Cu;
    ctx.gpr[4] = lock;
    (void)call_keep(rt, ctx, 0x08B344E0u, {lock});
    ctx.gpr[7] = 0u;
    ctx.gpr[8] = 0x10u;
    std::uint32_t slot = self + 0x1Cu;
    for (std::uint32_t index = 0u; index < 0x10u; ++index, slot += 0x10u) {
        ctx.gpr[16] = slot;
        ctx.gpr[7] = index;
        const std::uint32_t table = word(rt, slot + 0xCu);
        ctx.gpr[6] = table;
        put(rt, slot + 4u, 0u);
        if (table == 0u) continue;
        if (word(rt, slot + 8u) == 0u) continue;
        const std::uint32_t count = word(rt, slot);
        ctx.gpr[2] = count;
        ctx.gpr[4] = 0u;
        if (count == 0u) continue;
        for (std::uint32_t entry = 0u; entry < count; ++entry) {
            const std::uint32_t block = word(rt, slot + 8u);
            put(rt, word(rt, slot + 0xCu) + entry * 4u, block + entry * 0x20u);
            ctx.gpr[4] = entry + 1u;
        }
    }
    ctx.gpr[16] = slot;
    ctx.gpr[7] = 0x10u;
    ctx.gpr[4] = lock;
    (void)call_keep(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[4] = self + 0x144u;
    ctx.gpr[5] = 0u;
    (void)call_keep(rt, ctx, 0x08ABF6E0u, {self + 0x144u, 0u});
    for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, self + 0xCu + i * 4u, 0u);
    ctx.gpr[4] = self + 0x10u;
    ctx.gpr[3] = 4u;
    ctx.gpr[2] = 4u;
}

// func_08BE45C4(this, a1, a2): func_08BE43B0(sp), func_08BE43B0(this, sp), func_08B2C958(this + 8, this[0], a2), this[0x68] = func_08B2D1A8(this + 8), func_08B28D34(this + 0x6C); the shared pointer p = [sp + 4], when set: its strong counter at p + 4 is decremented (ll / sc) and, when it was 1, the
// virtual slot +8 of p runs on p, then the weak counter at p + 8 is decremented and, when it was 1, the slot +0xC runs on p; this[0x98] = this + 0x98 and (this + 0x98)[4] = this + 0x98. v0 = this + 0x98.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null shared pointer ([sp + 4], the `beqz s0` at 0x08BE4620) and a strong counter of 1 (the slot +8, the weak decrement and the slot +0xC, 0x08BE4640-0x08BE4680) never ran: the counter was always above 1 -> the release is read from the asm only.
void func_08BE45C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], third = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    ctx.gpr[17] = third;
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[16] = ctx.gpr[16];
    (void)call_keep(rt, ctx, 0x08BE43B0u, {frame.at(0u)});
    ctx.gpr[5] = frame.at(0u);
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08BE43B0u, {self, frame.at(0u)});
    const std::uint32_t head = word(rt, self);
    ctx.gpr[5] = head;
    ctx.gpr[16] = self + 8u;
    ctx.gpr[6] = third;
    ctx.gpr[4] = self + 8u;
    (void)call_keep(rt, ctx, 0x08B2C958u, {self + 8u, head, third});
    ctx.gpr[4] = self + 8u;
    const std::uint32_t answer = call_keep(rt, ctx, 0x08B2D1A8u, {self + 8u});
    put(rt, self + 0x68u, answer);
    ctx.gpr[4] = self + 0x6Cu;
    (void)call_keep(rt, ctx, 0x08B28D34u, {self + 0x6Cu});
    const std::uint32_t shared = word(rt, frame.at(4u));
    ctx.gpr[16] = shared;
    std::uint32_t result = self + 0x98u;
    if (shared != 0u) {
        ctx.gpr[3] = shared + 4u;
        const std::uint32_t strong = word(rt, shared + 4u);
        put(rt, shared + 4u, strong - 1u);
        ctx.gpr[2] = 1u;
        ctx.gpr[4] = strong;
        if (strong == 1u) {
            const std::uint32_t vtable = word(rt, shared);
            const std::uint32_t function = word(rt, vtable + 8u);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            ctx.gpr[4] = shared;
            (void)call_keep(rt, ctx, function, {shared});
            ctx.gpr[3] = shared + 8u;
            const std::uint32_t weak = word(rt, shared + 8u);
            put(rt, shared + 8u, weak - 1u);
            ctx.gpr[2] = 1u;
            ctx.gpr[4] = weak;
            if (weak == 1u) {
                const std::uint32_t final_vtable = word(rt, shared);
                const std::uint32_t final_function = word(rt, final_vtable + 0xCu);
                ctx.gpr[3] = final_vtable;
                ctx.gpr[2] = final_function;
                ctx.gpr[4] = shared;
                (void)call_keep(rt, ctx, final_function, {shared});
            }
        }
    }
    ctx.gpr[2] = result;
    put(rt, self + 0x98u, result);
    put(rt, result + 4u, result);
}

// func_0881D450(out): a holder h at sp + 4 = func_0881BD98(sp + 4) (its object o = *h, null: func_08A5BB7C(D_08C98F48) first); r = func_0881B068(o); func_08A5B9EC(sp + 4); a record at sp + 8 = func_088A79AC(sp + 8, 0); with r[0x10] and r[0x14] not both -1 (the pair r[0x10]:r[0x14]):
// func_088A58A4(sp + 8, -1, r[0x10], r[0x14]) and func_088A599C(sp + 8, r[0xC]); func_08857EE4(sp, sp + 8, 0); unless `out` is sp func_08A5BAD8(out, [sp]); func_08A5B9EC(sp) and func_088A7548(sp + 8). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null object (the assertion call skipped by `bnez s0` at 0x0881D484), a pair other than -1:-1 (the `bne v0, a1` at 0x0881D4C8 / `beql v1` at 0x0881D4D0 would run func_088A58A4 and func_088A599C, 0x0881D4D8-0x0881D4E8: read from the asm only) and `out` being sp (the copy skipped by `beq s3, sp`).
void func_0881D450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(4u), record = frame.at(8u);
    ctx.gpr[19] = out;
    ctx.gpr[18] = holder;
    ctx.gpr[4] = holder;
    ctx.gpr[17] = record;
    (void)call_keep(rt, ctx, 0x0881BD98u, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.gpr[4] = 0x08C98F48u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    ctx.gpr[4] = object;
    const std::uint32_t found = call_keep(rt, ctx, 0x0881B068u, {object});
    ctx.gpr[4] = holder;
    ctx.gpr[16] = found;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
    ctx.gpr[5] = 0u;
    ctx.gpr[4] = record;
    (void)call_keep(rt, ctx, 0x088A79ACu, {record, 0u});
    const std::uint32_t low = word(rt, found + 0x10u), high = word(rt, found + 0x14u);
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = record;
    ctx.gpr[6] = low;
    ctx.gpr[7] = high;
    if (low != 0xFFFFFFFFu || high != 0xFFFFFFFFu) {
        (void)call_keep(rt, ctx, 0x088A58A4u, {record, 0xFFFFFFFFu, low, high});
        const std::uint32_t color = word(rt, found + 0xCu);
        ctx.gpr[5] = color;
        ctx.gpr[4] = record;
        (void)call_keep(rt, ctx, 0x088A599Cu, {record, color});
    }
    ctx.gpr[4] = frame.at(0u);
    ctx.gpr[5] = record;
    ctx.gpr[6] = 0u;
    (void)call_keep(rt, ctx, 0x08857EE4u, {frame.at(0u), record, 0u});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[4] = record;
    (void)call_keep(rt, ctx, 0x088A7548u, {record});
}

// func_08A20EC0(out, a1, level, ref): with level > 0 (signed): the holder h at sp + 8 = func_08A20CC8(sp + 8), its object o = *h (null: func_08A5BB7C(D_08CAA7F4) first), p = *ref (null: func_08A5BB7C(D_08CAA7EC) first), o[0x1C] (a float) = p's vtable slot 0x54 called with (p) (its f0);
// func_08A5B9EC(sp + 8). Else: the holder h at sp = func_08A20CC8(sp), its object o (null: func_08A5BB7C first), t at sp + 4 = func_08A87EEC(sp + 4, o[0x1C] as f12); unless `out` is sp + 4 func_08A5BAD8(out, [sp + 4]); func_08A5B9EC(sp + 4) and func_08A5B9EC(sp). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): level <= 0 (the whole second half, 0x08A20F40-0x08A20F8C: the holder at sp, its object o, o[0x1C] as f12 into func_08A87EEC, the copy and the two releases) and the null objects (the assertion calls skipped by the `bnel` at 0x08A20EF0 and 0x08A20F08) never ran -> the second half is read from the asm only.
void func_08A20EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], level = ctx.gpr[6], ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = ref;
    if (signed_of(level) > 0) {
        const std::uint32_t holder = frame.at(8u);
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A20CC8u, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[17] = object;
        ctx.gpr[4] = 0x08CAA7F4u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAA7F4u});
        const std::uint32_t source = word(rt, ref);
        ctx.gpr[16] = source;
        ctx.gpr[4] = 0x08CAA7ECu;
        if (source == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAA7ECu});
        const std::uint32_t vtable = word(rt, source);
        const std::uint32_t function = word(rt, vtable + 0x54u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = source;
        const GuestResult value = call_keep(rt, ctx, function, {source}, {});
        put_float(rt, object + 0x1Cu, value.f0);
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A20CC8u, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08CAA7F4u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAA7F4u});
    }
    const float value = float_at(rt, object + 0x1Cu);
    ctx.fpr[12] = value;
    ctx.gpr[16] = frame.at(4u);
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A87EECu, {frame.at(4u)}, {value});
    ctx.gpr[5] = word(rt, frame.at(4u));
    ctx.gpr[4] = out;
    if (out != frame.at(4u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(4u))});
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08ACFA08(this): func_08ABCEB4(this); this[0] = D_08CD6B38; the ten words at +0x8C, +0x90, +0x94, +0x98, +0x9C and +0xA4, +0xA8, +0xAC, +0xB0, +0xB4 (two groups of five, 0x18 bytes apart) = 0; func_08B1AFB8(this + 0xB8);
// func_08AFB4B4 on this + 0xFC + 0xD4 i (i = 0, 1), on this + 0x1A8 + 0xFC + 0xD4 i (i = 0, 1) and on this + 0xFC + 0x350; func_08B1BC98(this + 0x528) and func_08B1BC98(this + 0x5F8); this[0xC] = 0x8000, this[0xF0] = 0x3E80, this[0xF4] = this[0xF8] = 0. v0 = 0x3E80.
// Branches: none that the compared cases skip (every branch took both ways).
void func_08ACFA08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    (void)call_keep(rt, ctx, 0x08ABCEB4u, {self});
    ctx.gpr[2] = 0x08CD6B38u;
    put(rt, self, 0x08CD6B38u);
    for (std::uint32_t group = 0u; group < 2u; ++group) {
        const std::uint32_t base = self + 0x9Cu + group * 0x18u;
        ctx.gpr[3] = 1u - group;
        ctx.gpr[2] = base;
        put(rt, base - 0x10u, 0u);
        put(rt, base - 0xCu, 0u);
        put(rt, base - 8u, 0u);
        put(rt, base - 4u, 0u);
        put(rt, base, 0u);
    }
    const std::uint32_t area = self + 0xFCu;
    ctx.gpr[19] = area;
    ctx.gpr[4] = self + 0xB8u;
    ctx.gpr[3] = 0u;
    (void)call_keep(rt, ctx, 0x08B1AFB8u, {self + 0xB8u});
    for (std::uint32_t i = 0u; i < 2u; ++i) {
        ctx.gpr[16] = area + i * 0xD4u;
        ctx.gpr[17] = 1u - i;
        ctx.gpr[4] = area + i * 0xD4u;
        (void)call_keep(rt, ctx, 0x08AFB4B4u, {area + i * 0xD4u});
    }
    for (std::uint32_t i = 0u; i < 2u; ++i) {
        ctx.gpr[16] = area + 0x1A8u + i * 0xD4u;
        ctx.gpr[17] = 1u - i;
        ctx.gpr[4] = area + 0x1A8u + i * 0xD4u;
        (void)call_keep(rt, ctx, 0x08AFB4B4u, {area + 0x1A8u + i * 0xD4u});
    }
    ctx.gpr[4] = area + 0x350u;
    (void)call_keep(rt, ctx, 0x08AFB4B4u, {area + 0x350u});
    ctx.gpr[4] = self + 0x528u;
    (void)call_keep(rt, ctx, 0x08B1BC98u, {self + 0x528u});
    ctx.gpr[4] = self + 0x5F8u;
    (void)call_keep(rt, ctx, 0x08B1BC98u, {self + 0x5F8u});
    ctx.gpr[4] = 0x8000u;
    put(rt, self + 0xCu, 0x8000u);
    ctx.gpr[2] = 0x3E80u;
    put(rt, self + 0xF0u, 0x3E80u);
    ctx.gpr[3] = 0u;
    put(rt, self + 0xF4u, 0u);
    put(rt, self + 0xF8u, 0u);
}

// func_08BAC9EC(a0, a1, a2, a3, t0, t1, t2, t3): a copy c of the shared pointer pair at t3 ([0], [1]) is made at sp + 0x10 (the counter at c[4] + 4 incremented when c[4] is non-null, ll / sc) and sp[0] = sp + 0x10; func_08961EF0(a1, a2, a3, t0, D_0889B6B0, a0, t1, t2) runs;
// then the pair's release: s = c[4], when set: the strong counter at s + 4 is decremented (ll / sc) and, when it was 1, the virtual slot +8 of s runs on s, then the weak counter at s + 8 is decremented and, when it was 1, the slot +0xC runs on s. v0 is the last operation's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-null pair (the `beqz v1` at 0x08BACA20 skips the counter increment; the release of the copy, 0x08BACA5C-0x08BACAC4) never ran: every compared case copied an empty pair -> the increment and the release are read from the asm only.
void func_08BAC9EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8], t1 = ctx.gpr[9], t2 = ctx.gpr[10], t3 = ctx.gpr[11];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t pair_low = word(rt, t3), pair_high = word(rt, t3 + 4u);
    put(rt, frame.at(0x10u), pair_low);
    put(rt, frame.at(0x14u), pair_high);
    if (pair_high != 0u) {
        put(rt, pair_high + 4u, word(rt, pair_high + 4u) + 1u);
        ctx.gpr[2] = 1u;
    }
    ctx.gpr[2] = frame.at(0x10u);
    ctx.gpr[4] = a1;
    ctx.gpr[5] = a2;
    ctx.gpr[6] = a3;
    ctx.gpr[7] = t0;
    ctx.gpr[8] = 0x0889B6B0u;
    ctx.gpr[9] = a0;
    ctx.gpr[10] = t1;
    ctx.gpr[11] = t2;
    put(rt, frame.at(0u), frame.at(0x10u));
    (void)call_keep(rt, ctx, 0x08961EF0u, {a1, a2, a3, t0, 0x0889B6B0u, a0, t1, t2});
    const std::uint32_t shared = word(rt, frame.at(0x14u));
    ctx.gpr[16] = shared;
    if (shared == 0u) return;
    ctx.gpr[3] = shared + 4u;
    const std::uint32_t strong = word(rt, shared + 4u);
    put(rt, shared + 4u, strong - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = strong;
    if (strong != 1u) return;
    const std::uint32_t vtable = word(rt, shared);
    const std::uint32_t function = word(rt, vtable + 8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    (void)call_keep(rt, ctx, function, {shared});
    ctx.gpr[3] = shared + 8u;
    const std::uint32_t weak = word(rt, shared + 8u);
    put(rt, shared + 8u, weak - 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[4] = weak;
    if (weak != 1u) return;
    const std::uint32_t final_vtable = word(rt, shared);
    const std::uint32_t final_function = word(rt, final_vtable + 0xCu);
    ctx.gpr[3] = final_vtable;
    ctx.gpr[2] = final_function;
    ctx.gpr[4] = shared;
    (void)call_keep(rt, ctx, final_function, {shared});
}

// func_08C4A4AC(this, n): a vector of 16-bit elements (this[0] = begin, this[4] = end, this[8] = end of the capacity) grown to hold n: a negative n only calls func_08B5ADDC(D_08CB4AEC) (the length error; the original goes on with a0 clobbered, read from the asm only).
// With the capacity (this[2] - this[0]) >> 1 below n (unsigned) a new block is allocated (func_08B35DA0(2 n), nothing for n = 0), the elements are moved into it (func_08C499C0(begin, end, block, sp)), the old range is destroyed (func_08C499B0(this[0], this[1], sp + 1))
// and the old block freed (func_08B35254(this[0]), when set); this[0] = block, this[4] = block + 2 size, this[8] = block + 2 n. v0 = block + 2 size (0 without growth).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): n < 0 (the `bgez a1` at 0x08C4A4CC calls func_08B5ADDC with the original going on with a0 clobbered), n = 0 (`beqz a1` at 0x08C4A508), a vector that already holds enough (the `beqz v0` at 0x08C4A4F4 returns 0) and a non-null old block (the free func_08B35254, `beqz a0` at 0x08C4A548) never ran: every compared case grew an empty vector.
void func_08C4A4AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    if (signed_of(count) < 0) {
        ctx.gpr[4] = 0x08CB4AECu;
        (void)call_keep(rt, ctx, 0x08B5ADDCu, {0x08CB4AECu});
        return;
    }
    const std::uint32_t begin = word(rt, self), end_of_storage = word(rt, self + 8u);
    ctx.gpr[17] = begin;
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(end_of_storage - begin) >> 1);
    const std::uint32_t grow = capacity < count ? 1u : 0u;
    ctx.gpr[2] = grow;
    ctx.gpr[19] = 0u;
    if (grow == 0u) return;
    const std::uint32_t end = word(rt, self + 4u);
    ctx.gpr[18] = end;
    ctx.gpr[20] = 0u;
    ctx.gpr[2] = end - begin;
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 1);
    std::uint32_t block = 0u, bytes = 0u;
    ctx.gpr[21] = size;
    if (count != 0u) {
        bytes = count << 1;
        ctx.gpr[20] = bytes;
        ctx.gpr[4] = bytes;
        block = call_keep(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[19] = block;
    }
    ctx.gpr[4] = begin;
    ctx.gpr[5] = end;
    ctx.gpr[6] = block;
    ctx.gpr[7] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08C499C0u, {begin, end, block, frame.at(0u)});
    const std::uint32_t old_begin = word(rt, self), old_end = word(rt, self + 4u);
    ctx.gpr[4] = old_begin;
    ctx.gpr[5] = old_end;
    ctx.gpr[6] = frame.at(1u);
    (void)call_keep(rt, ctx, 0x08C499B0u, {old_begin, old_end, frame.at(1u)});
    const std::uint32_t old_block = word(rt, self);
    ctx.gpr[4] = old_block;
    ctx.gpr[2] = size << 1;
    if (old_block != 0u) {
        (void)call_keep(rt, ctx, 0x08B35254u, {old_block});
        ctx.gpr[2] = size << 1;
    }
    ctx.gpr[3] = block + bytes;
    put(rt, self + 8u, block + bytes);
    ctx.gpr[2] = block + (size << 1);
    put(rt, self + 4u, block + (size << 1));
    put(rt, self, block);
}

// func_0880D478(this, a1): func_0896B0E4(this, a1); r = func_089BD4DC(this), p = func_08B62344(0x210), func_0880BF70(p, r); with p different from the word at offset 0xE8: the object o = the word at 0xEC, when set, has its vtable slot 0 called with (o) and both words are cleared; the word at 0xE8 = p;
// the guarded static D_08D750F8 (func_08B6441C / func_08B64494) holds D_08D75100 (set to D_08CBCD90 on the first call) and the word at 0xEC = D_08D75100; the tail call is func_0880B5E8(the word at 0xE8, a1).
// Not registered: same as func_0880D564 (its slot-0 call destroys the same member object). Its first version took the allocation for the result of func_089BD4DC (a move in the delay slot of the allocating `jal` runs before the call) and faulted; fixed and compared equal (1 case per menu route).
void func_0880D478(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = self;
        ctx.gpr[19] = argument;
        (void)call_keep(rt, ctx, 0x0896B0E4u, {self, argument});
        ctx.gpr[4] = self;
        const std::uint32_t source = call_keep(rt, ctx, 0x089BD4DCu, {self});
        ctx.gpr[4] = 0x210u;
        ctx.gpr[16] = source;
        const std::uint32_t block = call_keep(rt, ctx, 0x08B62344u, {0x210u});
        ctx.gpr[4] = block;
        ctx.gpr[5] = source;
        ctx.gpr[17] = block;
        (void)call_keep(rt, ctx, 0x0880BF70u, {block, source});
        const std::uint32_t current = word(rt, self + 0xE8u);
        ctx.gpr[5] = current;
        ctx.gpr[16] = self + 0xE8u;
        if (block != current) {
            const std::uint32_t object = word(rt, self + 0xECu);
            ctx.gpr[4] = object;
            if (object == 0u) {
                put(rt, self + 0xE8u, block);
            } else {
                const std::uint32_t vtable = word(rt, object);
                const std::uint32_t function = word(rt, vtable);
                ctx.gpr[3] = vtable;
                ctx.gpr[2] = function;
                (void)call_keep(rt, ctx, function, {object});
                put(rt, self + 0xECu, 0u);
                put(rt, self + 0xE8u, 0u);
                put(rt, self + 0xE8u, block);
            }
            guarded_holder(rt, ctx, self + 0xECu);
        }
        ctx.gpr[4] = word(rt, self + 0xE8u);
        ctx.gpr[5] = argument;
    }
    tail_call(ctx, 0x0880B5E8u, {word(rt, self + 0xE8u), argument});
}

// func_08ABAFCC(a0, a1, a2, index, flag, f12, f13): o = func_08AD3C5C(a2) (null: v0 = 0); n = o[0x10] (0: v0 = 0); the index is a3 (signed), or for a negative one func_08B1ECC8() modulo n (unsigned); with the index below n (unsigned) the tail call
// func_08ABADE0(a0, func_08AD3B5C(*(o + 4 index + 0x14)), flag & 0xFF, f12, f13), else v0 = 0.
// Not registered: every case stops on a held LwMutex (not comparable).
void func_08ABAFCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], third = ctx.gpr[6], index_argument = ctx.gpr[7], flag = ctx.gpr[8] & 0xFFu;
    const float entry_f12 = ctx.fpr[12], entry_f13 = ctx.fpr[13];
    std::uint32_t table = 0u, index = index_argument;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegs saved(ctx);
        ctx.gpr[19] = first;
        ctx.gpr[20] = flag;
        ctx.gpr[18] = index_argument;
        ctx.gpr[4] = third;
        const std::uint32_t object = call_keep(rt, ctx, 0x08AD3C5Cu, {third});
        ctx.gpr[16] = object;
        ctx.gpr[2] = 0u;
        if (object == 0u) return;
        const std::uint32_t count = word(rt, object + 0x10u);
        ctx.gpr[17] = count;
        if (count == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (signed_of(index) < 0) {
            const std::uint32_t random = call_keep(rt, ctx, 0x08B1ECC8u, {});
            index = random % count;
            ctx.gpr[18] = index;
        }
        ctx.gpr[2] = (index << 2) + object;
        if (!(index < count)) {
            ctx.gpr[2] = 0u;
            return;
        }
        ctx.gpr[4] = word(rt, ctx.gpr[2] + 0x14u);
        table = call_keep(rt, ctx, 0x08AD3B5Cu, {word(rt, ctx.gpr[2] + 0x14u)});
        ctx.fpr[12] = entry_f12;
        ctx.fpr[13] = entry_f13;
    }
    tail_call(ctx, 0x08ABADE0u, {first, table, flag});
}

// func_089ADE38(out, a1, mode, ref): mode 0: a holder h at sp + 4 = func_089AB370(sp + 4, a1), its object o = *h (null: func_08A5BB7C(D_08CA50C4) first); t at sp = func_08A87EEC(sp, o[0x14][0x170] as f12); unless `out` is sp func_08A5BAD8(out, [sp]); func_08A5B9EC(sp) and func_08A5B9EC(sp + 4).
// mode > 0: the holder at sp + 4 = func_089AB370(sp + 4, a1), o = *h (null: assertion D_08CA50C4), p = *ref (null: func_08A5BB7C(D_08CA50BC)), o[0x14][0x170] = p's vtable slot 0x54 called with (p) (its f0); func_08A5B9EC(sp + 4). Mode < 0 returns at once (v0 untouched). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): mode != 0 (the setter half: the holder at sp + 4, o and p, the float stored in inner[0x170], 0x089ADEB4-0x089ADF08) never ran, and the null object of the getter (the assertion call skipped by `bnel s0` at 0x089ADE68) and `out` being sp (`beq s1, sp` at 0x089ADE8C): the setter is read from the asm only.
void func_089ADE38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argument = ctx.gpr[5], mode = ctx.gpr[6], ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = ref;
    const std::uint32_t holder = frame.at(4u);
    if (mode == 0u) {
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x089AB370u, {holder, argument});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08CA50C4u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
        const std::uint32_t inner = word(rt, object + 0x14u);
        ctx.gpr[2] = inner;
        const float value = float_at(rt, inner + 0x170u);
        ctx.fpr[12] = value;
        ctx.gpr[4] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {value});
        ctx.gpr[5] = word(rt, frame.at(0u));
        ctx.gpr[4] = out;
        if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
        ctx.gpr[4] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    if (signed_of(mode) <= 0) return;
    ctx.gpr[18] = holder;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x089AB370u, {holder, argument});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08CA50C4u;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
    const std::uint32_t source = word(rt, ref);
    ctx.gpr[16] = source;
    const std::uint32_t inner = word(rt, object + 0x14u);
    ctx.gpr[17] = inner;
    ctx.gpr[4] = 0x08CA50BCu;
    if (source == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
    const std::uint32_t vtable = word(rt, source);
    const std::uint32_t function = word(rt, vtable + 0x54u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = source;
    const GuestResult value = call_keep(rt, ctx, function, {source}, {});
    put_float(rt, inner + 0x170u, value.f0);
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_0897BFEC(out, a1, level, extra): a holder h8 at sp + 8 = func_08978000(sp + 8); with level > 0 (signed): a holder h4 at sp + 4 = func_08A5D2D4(sp + 4, extra), o = *h4 (null: func_08A5BB7C(D_08CA32CC) first), p = *h8 (null: func_08A5BB7C(D_08CA2F94) first),
// n = func_089788A8(p), a temporary t at sp = func_08A85B1C(sp, n), o's vtable slot 0xC4 called with (o, sp), func_08A5B9EC(sp) and func_08A5B9EC(sp + 4). In every case q = *h8 (null: the same assertion call), func_0897BC08(q) and func_08A5B9EC(sp + 8). v0 is the last call's.
// Not registered: every case reaches an import stub (not comparable: sceDisplay).
void func_0897BFEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t level = ctx.gpr[6], extra = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t first = frame.at(8u), second = frame.at(4u);
    ctx.gpr[20] = first;
    ctx.gpr[19] = second;
    ctx.gpr[17] = level;
    ctx.gpr[16] = extra;
    ctx.gpr[4] = first;
    (void)call_keep(rt, ctx, 0x08978000u, {first});
    ctx.gpr[5] = extra;
    ctx.gpr[4] = second;
    if (signed_of(level) > 0) {
        (void)call_keep(rt, ctx, 0x08A5D2D4u, {second, extra});
        const std::uint32_t object = word(rt, second);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08CA32CCu;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA32CCu});
        const std::uint32_t vtable = word(rt, object);
        ctx.gpr[2] = vtable;
        const std::uint32_t holder = word(rt, first);
        ctx.gpr[17] = holder;
        ctx.gpr[4] = 0x08CA2F94u;
        const std::uint32_t function = word(rt, vtable + 0xC4u);
        ctx.gpr[18] = function;
        if (holder == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA2F94u});
        ctx.gpr[4] = holder;
        const std::uint32_t name = call_keep(rt, ctx, 0x089788A8u, {holder});
        ctx.gpr[5] = name;
        ctx.gpr[4] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08A85B1Cu, {frame.at(0u), name});
        ctx.gpr[5] = frame.at(0u);
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, function, {object, frame.at(0u)});
        ctx.gpr[4] = frame.at(0u);
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        ctx.gpr[4] = second;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {second});
    }
    const std::uint32_t last = word(rt, first);
    ctx.gpr[16] = last;
    ctx.gpr[4] = 0x08CA2F94u;
    if (last == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA2F94u});
    ctx.gpr[4] = last;
    (void)call_keep(rt, ctx, 0x0897BC08u, {last});
    ctx.gpr[4] = first;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {first});
}

// The common core of the two task wrappers (func_08BD787C, func_08BCFEF8): the shared pointer p = this[0x2C] gets its strong counter incremented (ll / sc) when set; this[0xE] (byte) = 0, this[0x1C] = 0; a pointer-to-member call: base = this[0x28], flags = this[0x34], adjusted = base + (flags >> 1) (signed),
// the function is this[0x30] itself or, with bit 0 of flags, the vtable word at (vtable of adjusted) + this[0x30]; function(adjusted); then v0 = s0 = this[0x1C] and the pair's release: the strong counter at p + 4 is decremented (ll / sc) and, when it was 1, the virtual slot +8 of p runs on p, then the weak
// counter at p + 8 is decremented and, when it was 1, the slot +0xC runs on p.
inline void run_task(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self, std::uint32_t shared, std::uint32_t after_call_clear) {
    const std::uint32_t flags = word(rt, self + 0x34u);
    ctx.gpr[5] = flags;
    ctx.gpr[2] = flags & 1u;
    const std::uint32_t base = word(rt, self + 0x28u);
    ctx.gpr[6] = base;
    const std::uint32_t adjusted = base + static_cast<std::uint32_t>(signed_of(flags) >> 1);
    std::uint32_t function;
    if ((flags & 1u) != 0u) function = word(rt, word(rt, adjusted) + word(rt, self + 0x30u));
    else function = word(rt, self + 0x30u);
    ctx.gpr[3] = function;
    ctx.gpr[4] = adjusted;
    (void)call_keep(rt, ctx, function, {adjusted});
    if (after_call_clear != 0u) put(rt, self + 0x18u, 0u);
    const std::uint32_t result = word(rt, self + 0x1Cu);
    ctx.gpr[16] = result;
    ctx.gpr[2] = result;
    if (shared == 0u) return;
    ctx.gpr[3] = shared + 4u;
    const std::uint32_t strong = word(rt, shared + 4u);
    put(rt, shared + 4u, strong - 1u);
    ctx.gpr[4] = strong;
    if (strong != 1u) return;
    const std::uint32_t vtable = word(rt, shared);
    const std::uint32_t slot = word(rt, vtable + 8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    (void)call_keep(rt, ctx, slot, {shared});
    ctx.gpr[3] = shared + 8u;
    const std::uint32_t weak = word(rt, shared + 8u);
    put(rt, shared + 8u, weak - 1u);
    ctx.gpr[4] = weak;
    ctx.gpr[2] = result;
    if (weak != 1u) return;
    const std::uint32_t final_vtable = word(rt, shared);
    const std::uint32_t final_slot = word(rt, final_vtable + 0xCu);
    ctx.gpr[3] = final_vtable;
    ctx.gpr[2] = final_slot;
    ctx.gpr[4] = shared;
    (void)call_keep(rt, ctx, final_slot, {shared});
    ctx.gpr[2] = result;
}

// func_08BD787C(this): see run_task: the counter increment, this[0xE] = 0 (byte), this[0x1C] = 0, the pointer-to-member call and the release; v0 = this[0x1C] as the call left it.
// Not registered: every case stops on a held LwMutex (not comparable).
void func_08BD787C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const std::uint32_t shared = word(rt, self + 0x2Cu);
    ctx.gpr[17] = shared;
    ctx.gpr[16] = self;
    if (shared != 0u) {
        put(rt, shared + 4u, word(rt, shared + 4u) + 1u);
        ctx.gpr[2] = 1u;
    }
    rt.memory().store8(self + 0xEu, 0u);
    put(rt, self + 0x1Cu, 0u);
    run_task(rt, ctx, self, shared, 0u);
}

// func_08BCFEF8(this, a1): the same as func_08BD787C with this[0x18] = a1 stored before the call and this[0x18] = 0 after it (a1 itself is overwritten by this[10]).
// Not registered: every case stops on a held LwMutex (not comparable).
void func_08BCFEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    const std::uint32_t shared = word(rt, self + 0x2Cu);
    ctx.gpr[17] = shared;
    ctx.gpr[16] = self;
    if (shared != 0u) {
        put(rt, shared + 4u, word(rt, shared + 4u) + 1u);
        ctx.gpr[2] = 1u;
    }
    put(rt, self + 0x18u, argument);
    rt.memory().store8(self + 0xEu, 0u);
    put(rt, self + 0x1Cu, 0u);
    run_task(rt, ctx, self, shared, 1u);
}

// func_0894CD28(this, n): p = func_08B6461C(n * 0x2100 + 8), p[4] = n; for each of the n elements e (0x2100 bytes, from p + 8): e[0] = D_08CC6828, e[8] = 0, func_08958324(e + 0x19B0), func_089545BC(e + 0x1E70 + 0x38 j) for j = 0..3, func_08949AEC(e + 0x2000);
// this[4] = p + 8, this[8] = n, and for n > 0 the words e[4] and e[8] of every element are cleared through this[4]. v0 is the last operation's (p when n is 0).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): n = 0 (the `beqz s3` at 0x0894CD70 skips the construction and the `blez s3` at 0x0894CDCC returns; v0 = the allocation) never ran.
void func_0894CD28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], count = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = self;
    ctx.gpr[19] = count;
    const std::uint32_t bytes = count * 0x2100u;
    ctx.gpr[2] = bytes;
    ctx.gpr[4] = bytes + 8u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08B6461Cu, {bytes + 8u});
    const std::uint32_t first = block + 8u;
    ctx.gpr[23] = first;
    put(rt, block + 4u, count);
    if (count != 0u) {
        ctx.gpr[22] = 0x08CC6828u;
        std::uint32_t element = first;
        std::uint32_t index = 0u;
        do {
            ctx.gpr[18] = element;
            ctx.gpr[20] = index;
            put(rt, element, 0x08CC6828u);
            ctx.gpr[4] = element + 0x19B0u;
            ctx.gpr[16] = element + 0x1E70u;
            put(rt, element + 8u, 0u);
            (void)call_keep(rt, ctx, 0x08958324u, {element + 0x19B0u});
            std::uint32_t part = element + 0x1E70u;
            for (std::uint32_t remaining = 4u; remaining != 0u; --remaining) {
                ctx.gpr[16] = part;
                ctx.gpr[17] = remaining - 1u;
                ctx.gpr[4] = part;
                (void)call_keep(rt, ctx, 0x089545BCu, {part});
                part += 0x38u;
                ctx.gpr[16] = part;
            }
            ctx.gpr[4] = element + 0x2000u;
            ctx.gpr[20] = index + 1u;
            (void)call_keep(rt, ctx, 0x08949AECu, {element + 0x2000u});
            ++index;
            element += 0x2100u;
            ctx.gpr[18] = element;
        } while (index != count);
    }
    put(rt, self + 4u, first);
    put(rt, self + 8u, count);
    if (signed_of(count) <= 0) return;
    std::uint32_t offset = 0u, counter = 0u;
    for (;;) {
        ctx.gpr[3] = word(rt, self + 4u) + offset;
        ++counter;
        put(rt, word(rt, self + 4u) + offset + 4u, 0u);
        ctx.gpr[2] = word(rt, self + 4u) + offset;
        put(rt, ctx.gpr[2] + 8u, 0u);
        ctx.gpr[5] = counter;
        ctx.gpr[4] = offset + 0x2100u;
        if (count == counter) break;
        offset += 0x2100u;
    }
}

// func_089E6520(this): func_08A02218(this); the words at byte offsets 0x1D4, 0x19C, 0x1A0, 0x1AC, 0x1B0, 0x1BC, 0x1C4 and 0x1CC = -1 and at 0x1B8, 0x1C8, 0x1C0 = 0 (0x1D0 = 0 in the delay slot of the next call); m = func_089BD4DC(this), with m set func_08B855E0(this + 0x1E0, m + 0x128);
// then three string lookups: t = func_08A69B3C(sp + 8, D_08CA7A1C), func_08A7BFD0(this, this + 0x184, t), func_08A69D68(t); the same with sp + 4 / D_08CA7A2C / this + 0x188 and sp / D_08CA7A3C / this + 0x18C. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null m = func_089BD4DC(this) (the `beqz v0` at 0x089E6584 skips func_08B855E0) never ran.
void func_089E6520(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = frame.at(4u);
    ctx.gpr[17] = frame.at(8u);
    ctx.gpr[16] = self;
    (void)call_keep(rt, ctx, 0x08A02218u, {self});
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[4] = self;
    put(rt, self + 0x1D4u, 0xFFFFFFFFu);
    put(rt, self + 0x19Cu, 0xFFFFFFFFu);
    put(rt, self + 0x1A0u, 0xFFFFFFFFu);
    put(rt, self + 0x1ACu, 0xFFFFFFFFu);
    put(rt, self + 0x1B0u, 0xFFFFFFFFu);
    put(rt, self + 0x1B8u, 0u);
    put(rt, self + 0x1BCu, 0xFFFFFFFFu);
    put(rt, self + 0x1C8u, 0u);
    put(rt, self + 0x1CCu, 0xFFFFFFFFu);
    put(rt, self + 0x1C0u, 0u);
    put(rt, self + 0x1C4u, 0xFFFFFFFFu);
    put(rt, self + 0x1D0u, 0u);
    const std::uint32_t source = call_keep(rt, ctx, 0x089BD4DCu, {self});
    ctx.gpr[5] = source + 0x128u;
    ctx.gpr[4] = self + 0x1E0u;
    if (source != 0u) (void)call_keep(rt, ctx, 0x08B855E0u, {self + 0x1E0u, source + 0x128u});
    static constexpr std::uint32_t kNames[3] = {0x08CA7A1Cu, 0x08CA7A2Cu, 0x08CA7A3Cu};
    static constexpr std::uint32_t kFields[3] = {0x184u, 0x188u, 0x18Cu};
    const std::uint32_t temps[3] = {frame.at(8u), frame.at(4u), frame.at(0u)};
    for (unsigned i = 0u; i < 3u; ++i) {
        ctx.gpr[4] = temps[i];
        ctx.gpr[5] = kNames[i];
        (void)call_keep(rt, ctx, 0x08A69B3Cu, {temps[i], kNames[i]});
        ctx.gpr[6] = temps[i];
        ctx.gpr[5] = self + kFields[i];
        ctx.gpr[4] = self;
        (void)call_keep(rt, ctx, 0x08A7BFD0u, {self, self + kFields[i], temps[i]});
        ctx.gpr[4] = temps[i];
        (void)call_keep(rt, ctx, 0x08A69D68u, {temps[i]});
    }
}

// func_089C0360(out, a1, mode, ref): mode > 0 (signed): a holder h at sp + 8 = func_089BEE4C(sp + 8), o = *h (null: func_08A5BB7C(D_08CA5EB4) first), p = *ref (null: func_08A5BB7C(D_08CA5E6C) first), func_089BF568(o, f12 = p's vtable slot 0x54 called with (p), its f0); func_08A5B9EC(sp + 8).
// mode <= 0: the holder at sp = func_089BEE4C(sp), o = *h (null: the same assertion), t at sp + 4 = func_08A87EEC(sp + 4, o[0x244] (a float) as f12); unless `out` is sp + 4 func_08A5BAD8(out, [sp + 4]); func_08A5B9EC(sp + 4) and func_08A5B9EC(sp). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): mode > 0 is the only mode the cases used: the getter half (mode <= 0, 0x089C03E8-0x089C0448: the holder at sp, o[0x244] as f12 into func_08A87EEC, the copy and the releases) and the null objects (the assertion calls skipped by the `bnel` at 0x089C0390, 0x089C03A8 and 0x089C03F4) never ran -> the getter is read from the asm only.
void func_089C0360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = ref;
    if (signed_of(mode) > 0) {
        const std::uint32_t holder = frame.at(8u);
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x089BEE4Cu, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[17] = object;
        ctx.gpr[4] = 0x08CA5EB4u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA5EB4u});
        const std::uint32_t source = word(rt, ref);
        ctx.gpr[16] = source;
        ctx.gpr[4] = 0x08CA5E6Cu;
        if (source == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA5E6Cu});
        const std::uint32_t vtable = word(rt, source);
        const std::uint32_t function = word(rt, vtable + 0x54u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = source;
        const GuestResult value = call_keep(rt, ctx, function, {source}, {});
        ctx.fpr[12] = value.f0;
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, 0x089BF568u, {object}, {value.f0});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x089BEE4Cu, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    ctx.gpr[16] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08CA5EB4u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA5EB4u});
    }
    const float value = float_at(rt, object + 0x244u);
    ctx.fpr[12] = value;
    ctx.gpr[16] = frame.at(4u);
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A87EECu, {frame.at(4u)}, {value});
    ctx.gpr[5] = word(rt, frame.at(4u));
    ctx.gpr[4] = out;
    if (out != frame.at(4u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(4u))});
    ctx.gpr[4] = frame.at(4u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_0880C1DC(out, a1, level, ref): with level > 0 (signed): a holder h at sp + 4 = func_0880B8F8(sp + 4); p = *ref (null: func_08A5BB7C(D_08C98418) first) has its vtable slot 0xD8 called with (p); p = *ref again (null: the same assertion) and q = its vtable slot 0xDC called with (p, 0);
// o = *h (null: func_08A5BB7C(D_08C9848C)); func_0880BA98(o[0xE8], q); func_08A5B9EC(sp + 4); flag = 1 (else flag = 0). Then t at sp = func_08A85B1C(sp, flag); unless `out` is sp func_08A5BAD8(out, [sp]); func_08A5B9EC(sp). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): level <= 0 (the `blez a2` at 0x0880C1FC skips the whole middle, flag 0), the null objects (the assertion calls skipped by the `bnel`/`bnez` at 0x0880C214, 0x0880C23C and 0x0880C268) and `out` being sp (the copy skipped by `beq s2, sp`) never ran.
void func_0880C1DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], level = ctx.gpr[6], ref = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[2] = 0u;
    ctx.gpr[18] = out;
    ctx.gpr[17] = ref;
    std::uint32_t flag = 0u;
    if (signed_of(level) > 0) {
        const std::uint32_t holder = frame.at(4u);
        ctx.gpr[19] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x0880B8F8u, {holder});
        std::uint32_t object = word(rt, ref);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08C98418u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C98418u});
        std::uint32_t vtable = word(rt, object);
        std::uint32_t function = word(rt, vtable + 0xD8u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, function, {object});
        object = word(rt, ref);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08C98418u;
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C98418u});
        vtable = word(rt, object);
        function = word(rt, vtable + 0xDCu);
        ctx.gpr[4] = object;
        ctx.gpr[5] = 0u;
        ctx.gpr[2] = function;
        const std::uint32_t answer = call_keep(rt, ctx, function, {object, 0u});
        ctx.gpr[17] = answer;
        const std::uint32_t target = word(rt, holder);
        ctx.gpr[16] = target;
        ctx.gpr[4] = 0x08C9848Cu;
        if (target == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08C9848Cu});
        const std::uint32_t member = word(rt, target + 0xE8u);
        ctx.gpr[4] = member;
        ctx.gpr[5] = answer;
        (void)call_keep(rt, ctx, 0x0880BA98u, {member, answer});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        flag = 1u;
        ctx.gpr[2] = 1u;
    }
    ctx.gpr[5] = flag;
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {frame.at(0u), flag});
    ctx.gpr[4] = out;
    if (out != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    ctx.gpr[4] = frame.at(0u);
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_088D2E78(this, a1, kind): o = func_088DA588(this[0], a1); with o set and the table D_08CDD244 + 24 * kind (its field at +8: f, at +0x10: g): f = 0 or 2 runs o's vtable slot 0x10 with (o, 0) and, when func_088D62FC(this[0] + 4) is not 3, slot 0x1C with (o, 0);
// then, with g = 0, this[0]'s word at +0x618 is a pointer whose byte is set to 1; o[0x10] = 0 in every case. v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null o (the `beqz s0` at 0x088D2EC4 returns at once), a field f other than 0 (the `bne v1, v0` at 0x088D2EE0: f = 2 runs the slots too, any other value skips them), func_088D62FC answering 3 (the `beq v0, v1` at 0x088D2F08 skips slot 0x1C) and a non-zero g (`bnel a0` at 0x088D2F30: the byte store skipped) never ran.
void func_088D2E78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], kind = ctx.gpr[6];
    const std::uint32_t table = 0x08CDD244u;
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = 0x08CE0000u;
    ctx.gpr[20] = kind << 5;
    ctx.gpr[19] = kind << 3;
    ctx.gpr[17] = self;
    const std::uint32_t head = word(rt, self);
    ctx.gpr[4] = head;
    const std::uint32_t object = call_keep(rt, ctx, 0x088DA588u, {head, ctx.gpr[5]});
    ctx.gpr[16] = object;
    const std::uint32_t head_again = word(rt, self);
    ctx.gpr[3] = head_again;
    ctx.gpr[4] = table;
    const std::uint32_t entry = (kind << 5) - (kind << 3) + table;
    ctx.gpr[2] = entry;
    ctx.gpr[18] = head_again + 4u;
    if (object == 0u) return;
    const std::uint32_t mode = word(rt, entry + 8u);
    ctx.gpr[3] = mode;
    ctx.gpr[5] = 0u;
    ctx.gpr[4] = object;
    if (mode == 0u || mode == 2u) {
        std::uint32_t vtable = word(rt, object);
        std::uint32_t function = word(rt, vtable + 0x10u);
        ctx.gpr[2] = vtable;
        ctx.gpr[3] = function;
        (void)call_keep(rt, ctx, function, {object, 0u});
        ctx.gpr[4] = head_again + 4u;
        const std::uint32_t state = call_keep(rt, ctx, 0x088D62FCu, {head_again + 4u});
        ctx.gpr[3] = 3u;
        ctx.gpr[4] = object;
        ctx.gpr[5] = 0u;
        if (state != 3u) {
            vtable = word(rt, object);
            function = word(rt, vtable + 0x1C);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {object, 0u});
        }
    }
    const std::uint32_t slot = (kind << 5) - (kind << 3) + table;
    ctx.gpr[2] = slot;
    ctx.gpr[3] = table;
    const std::uint32_t pointer = word(rt, slot + 0x10u);
    ctx.gpr[4] = pointer;
    if (pointer != 0u) {
        put(rt, object + 0x10u, 0u);
        return;
    }
    const std::uint32_t current = word(rt, self);
    ctx.gpr[2] = current;
    ctx.gpr[3] = 1u;
    const std::uint32_t target = word(rt, current + 0x618u);
    ctx.gpr[4] = target;
    rt.memory().store8(target, 1u);
    put(rt, object + 0x10u, 0u);
}

// func_08A0BF5C(this): a string temporary t at sp + 4 = func_08B85EB8(sp + 4, D_08CA92DC, sp + 1); this's vtable slot 0x108 called with (this, sp + 4); t is released (func_08B60804 / func_08B8542C(body, sp)); r = func_08965418(), func_08A5BA08(r) when r is set; the old object this[0x38],
// when set, goes to func_08A5B930; this[0x38] = r; func_08B8850C(this, D_08CF6F14, D_08A0BEB8) and func_08B8A004(this, D_08CF6F24, D_08A0BC44). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null r (the `beqz v0` at 0x08A0BFE4 skips func_08A5BA08) and a non-empty old object this[0x38] (the `beqz v0` at 0x08A0BFF8: func_08A5B930 never ran); the shared empty body of the string (`beq s0, v0` at 0x08A0BFB4) is never met.
void func_08A0BF5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t temp = frame.at(4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = temp;
    const std::uint32_t vtable = word(rt, self);
    const std::uint32_t function = word(rt, vtable + 0x108u);
    ctx.gpr[17] = function;
    ctx.gpr[6] = frame.at(1u);
    ctx.gpr[5] = 0x08CA92DCu;
    ctx.gpr[4] = temp;
    (void)call_keep(rt, ctx, 0x08B85EB8u, {temp, 0x08CA92DCu, frame.at(1u)});
    ctx.gpr[5] = temp;
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, function, {self, temp});
    release_string(rt, ctx, temp, frame.at(0u));
    const std::uint32_t created = call_keep(rt, ctx, 0x08965418u, {});
    ctx.gpr[16] = created;
    ctx.gpr[4] = created;
    if (created != 0u) (void)call_keep(rt, ctx, 0x08A5BA08u, {created});
    const std::uint32_t old = word(rt, self + 0x38u);
    ctx.gpr[2] = old;
    ctx.gpr[4] = old;
    if (old != 0u) (void)call_keep(rt, ctx, 0x08A5B930u, {old});
    ctx.gpr[6] = 0x08A0BEB8u;
    ctx.gpr[5] = 0x08CF6F14u;
    put(rt, self + 0x38u, created);
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A0BEB8u});
    ctx.gpr[5] = 0x08CF6F24u;
    ctx.gpr[6] = 0x08A0BC44u;
    ctx.gpr[4] = self;
    (void)call_keep(rt, ctx, 0x08B8A004u, {self, 0x08CF6F24u, 0x08A0BC44u});
}

// func_08922CF8(this, a1, a2): func_08925FE4(this); this[0] = D_08CC5EC0; func_08926398 on the five members this + 0x1C, + 0x48, + 0x74, + 0xA0 and + 0xCC (in this order); this[8] = a2; then func_08926488(member, name, 1) with D_08CDDD08 for + 0x1C, D_08CDDCD8 for + 0x48 and D_08CDDCF0 for
// + 0x74, + 0xA0 and + 0xCC; the last one is the tail call.
// Branches: none that the compared cases skip (every branch took both ways).
void func_08922CF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], extra = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[22] = extra;
        ctx.gpr[16] = self;
        (void)call_keep(rt, ctx, 0x08925FE4u, {self});
        ctx.gpr[2] = 0x08CC5EC0u;
        ctx.gpr[18] = self + 0x1Cu;
        put(rt, self, 0x08CC5EC0u);
        ctx.gpr[17] = self + 0x48u;
        ctx.gpr[4] = self + 0x1Cu;
        (void)call_keep(rt, ctx, 0x08926398u, {self + 0x1Cu});
        ctx.gpr[19] = self + 0x74u;
        ctx.gpr[4] = self + 0x48u;
        (void)call_keep(rt, ctx, 0x08926398u, {self + 0x48u});
        ctx.gpr[20] = self + 0xA0u;
        ctx.gpr[4] = self + 0x74u;
        (void)call_keep(rt, ctx, 0x08926398u, {self + 0x74u});
        ctx.gpr[21] = self + 0xCCu;
        ctx.gpr[4] = self + 0xA0u;
        (void)call_keep(rt, ctx, 0x08926398u, {self + 0xA0u});
        ctx.gpr[4] = self + 0xCCu;
        (void)call_keep(rt, ctx, 0x08926398u, {self + 0xCCu});
        put(rt, self + 8u, extra);
        ctx.gpr[4] = self + 0x1Cu;
        ctx.gpr[5] = 0x08CDDD08u;
        ctx.gpr[6] = 1u;
        (void)call_keep(rt, ctx, 0x08926488u, {self + 0x1Cu, 0x08CDDD08u, 1u});
        ctx.gpr[16] = 0x08CE0000u;
        ctx.gpr[4] = self + 0x48u;
        ctx.gpr[5] = 0x08CDDCD8u;
        ctx.gpr[6] = 1u;
        (void)call_keep(rt, ctx, 0x08926488u, {self + 0x48u, 0x08CDDCD8u, 1u});
        ctx.gpr[4] = self + 0x74u;
        ctx.gpr[5] = 0x08CDDCF0u;
        ctx.gpr[6] = 1u;
        (void)call_keep(rt, ctx, 0x08926488u, {self + 0x74u, 0x08CDDCF0u, 1u});
        ctx.gpr[4] = self + 0xA0u;
        ctx.gpr[5] = 0x08CDDCF0u;
        ctx.gpr[6] = 1u;
        (void)call_keep(rt, ctx, 0x08926488u, {self + 0xA0u, 0x08CDDCF0u, 1u});
        ctx.gpr[4] = self + 0xCCu;
        ctx.gpr[5] = 0x08CDDCF0u;
    }
    tail_call(ctx, 0x08926488u, {self + 0xCCu, 0x08CDDCF0u, 1u});
}

// func_0892BE4C(this): func_08B225FC(this); this[0] = D_08CC6740; p = func_08B62344(0x9C), func_0892991C(p + 4), func_0892B758(p + 0x78), p[0x90] = p[0x94] = 0, this[0x260] = p; the guarded static D_08D76A18 (func_08B6441C / func_08B64494) holds D_08D76A20 (set to D_08CC6768 on the first call);
// this[0x264] = D_08D76A20, p[0] = 0, func_08929980(p + 4), func_08B20BA0(p + 0x40), func_08B20BA0(p + 0x5C), p[0x98] = -1. v0 = -1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard set on entry (the `bnez v0` at 0x0892BEB8) and func_08B6441C answering 0 (`beqz v0` at 0x0892BED0) never ran; the holder address is never null (static).
void func_0892BE4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t guard = 0x08D76A18u, holder = 0x08D76A20u;
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    ctx.gpr[18] = self + 0x260u;
    (void)call_keep(rt, ctx, 0x08B225FCu, {self});
    ctx.gpr[2] = 0x08CC6740u;
    ctx.gpr[4] = 0x9Cu;
    put(rt, self, 0x08CC6740u);
    const std::uint32_t p = call_keep(rt, ctx, 0x08B62344u, {0x9Cu});
    ctx.gpr[16] = p;
    ctx.gpr[4] = p + 4u;
    (void)call_keep(rt, ctx, 0x0892991Cu, {p + 4u});
    ctx.gpr[4] = p + 0x78u;
    (void)call_keep(rt, ctx, 0x0892B758u, {p + 0x78u});
    put(rt, p + 0x90u, 0u);
    ctx.gpr[3] = 0x08D70000u;
    put(rt, p + 0x94u, 0u);
    ctx.gpr[19] = holder;
    put(rt, self + 0x260u, p);
    ctx.gpr[16] = guard;
    ctx.gpr[2] = static_cast<std::uint32_t>(byte_at(rt, guard));
    ctx.gpr[4] = guard;
    if (ctx.gpr[2] == 0u) {
        const std::uint32_t acquired = call_keep(rt, ctx, 0x08B6441Cu, {guard});
        ctx.gpr[5] = 0x08D70000u;
        ctx.gpr[19] = holder;
        ctx.gpr[4] = guard;
        if (acquired != 0u) {
            ctx.gpr[2] = 0x08CC6768u;
            put(rt, holder, 0x08CC6768u);
            (void)call_keep(rt, ctx, 0x08B64494u, {guard});
        }
    }
    ctx.gpr[2] = word(rt, self + 0x260u);
    put(rt, self + 0x264u, holder);
    put(rt, word(rt, self + 0x260u), 0u);
    const std::uint32_t object = word(rt, self + 0x260u);
    ctx.gpr[4] = object + 4u;
    (void)call_keep(rt, ctx, 0x08929980u, {object + 4u});
    const std::uint32_t second = word(rt, self + 0x260u);
    ctx.gpr[4] = second + 0x40u;
    (void)call_keep(rt, ctx, 0x08B20BA0u, {second + 0x40u});
    const std::uint32_t third = word(rt, self + 0x260u);
    ctx.gpr[4] = third + 0x5Cu;
    (void)call_keep(rt, ctx, 0x08B20BA0u, {third + 0x5Cu});
    const std::uint32_t last = word(rt, self + 0x260u);
    ctx.gpr[3] = last;
    ctx.gpr[2] = 0xFFFFFFFFu;
    put(rt, last + 0x98u, 0xFFFFFFFFu);
}

// func_08A2A724(ref): a string temporary t at sp = func_08A69B3C(sp, D_08CABD04); q = func_08A5BC20(0x38, 4), func_08A7E914(q, sp), a holder h at sp + 0xC = func_08A76AB4(sp + 0xC, sp + 4) with [sp + 4] = q (stored in the delay slot of that call), func_08A69D68(sp);
// o = *h (null: func_08A5BB7C(D_08CABCEC) first); a record r at sp + 8 = func_08A85B1C(sp + 8, D_08CDEC48's word); func_08B86E98(o, D_08CDEC44's word, sp + 8); func_08A5B9EC(sp + 8); p = *ref (null: the same assertion), func_08A7CE50(p, sp + 0xC) and func_08A5B9EC(sp + 0xC). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the two null objects (the assertion calls skipped by the `bnez` at 0x08A2A798 and 0x08A2A7E0) never ran.
void func_08A2A724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t ref = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t text = frame.at(0u), pending = frame.at(4u), record = frame.at(8u), holder = frame.at(0xCu);
    ctx.gpr[20] = ref;
    ctx.gpr[5] = 0x08CABD04u;
    ctx.gpr[4] = text;
    ctx.gpr[19] = holder;
    ctx.gpr[18] = record;
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {text, 0x08CABD04u});
    ctx.gpr[5] = 4u;
    ctx.gpr[4] = 0x38u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[4] = block;
    ctx.gpr[5] = text;
    ctx.gpr[16] = block;
    (void)call_keep(rt, ctx, 0x08A7E914u, {block, text});
    ctx.gpr[5] = pending;
    ctx.gpr[4] = holder;
    put(rt, pending, block);
    (void)call_keep(rt, ctx, 0x08A76AB4u, {holder, pending});
    ctx.gpr[4] = text;
    (void)call_keep(rt, ctx, 0x08A69D68u, {text});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[17] = object;
    ctx.gpr[4] = 0x08CABCECu;
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CABCECu});
    const std::uint32_t first = word(rt, 0x08CDEC44u), second = word(rt, 0x08CDEC48u);
    ctx.gpr[2] = 0x08CDEC44u;
    ctx.gpr[16] = first;
    ctx.gpr[5] = second;
    ctx.gpr[4] = record;
    (void)call_keep(rt, ctx, 0x08A85B1Cu, {record, second});
    ctx.gpr[5] = first;
    ctx.gpr[4] = object;
    ctx.gpr[6] = record;
    (void)call_keep(rt, ctx, 0x08B86E98u, {object, first, record});
    ctx.gpr[4] = record;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {record});
    const std::uint32_t target = word(rt, ref);
    ctx.gpr[16] = target;
    ctx.gpr[4] = 0x08CABCECu;
    if (target == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CABCECu});
    ctx.gpr[4] = target;
    ctx.gpr[5] = holder;
    (void)call_keep(rt, ctx, 0x08A7CE50u, {target, holder});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

}  // namespace
}  // namespace hot_batch_87
}  // namespace gt::game

#define HOT87(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_87::name, gt::ReturnKind::kind)
#define HOT87N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_87::name, gt::ReturnKind::kind)
HOT87(0x0894D934u, func_0894D934, Word);
// Not registered (every case stops at a sleep that would block (not comparable)): HOT87(0x08AC2BD8u, func_08AC2BD8, Word);
HOT87(0x08BE0724u, func_08BE0724, Word);
HOT87(0x08AB3774u, func_08AB3774, DoubleWord);
HOT87N(0x0889A4F0u, func_0889A4F0, Word);
HOT87N(0x08BD67B8u, func_08BD67B8, Word);
// Not registered (no compared case changed memory): HOT87(0x08B8ECACu, func_08B8ECAC, Word);
HOT87(0x0887ED48u, func_0887ED48, Word);
HOT87(0x0886DE00u, func_0886DE00, Word);
HOT87(0x08A83474u, func_08A83474, Word);
HOT87(0x08A83AC0u, func_08A83AC0, Word);
HOT87(0x08915DECu, func_08915DEC, Word);
HOT87(0x08898D30u, func_08898D30, Word);
HOT87N(0x088AF1D0u, func_088AF1D0, Word);
HOT87N(0x088A11F8u, func_088A11F8, Word);
HOT87(0x08BE46A8u, func_08BE46A8, Word);
// Not registered (its case swallows the compared calls of its decompiled descendants: the destructor chain of this[0xEC]; compared 4, equal 4): HOT87(0x0880D564u, func_0880D564, Word);
HOT87(0x0887894Cu, func_0887894C, Word);
HOT87(0x08BAEF98u, func_08BAEF98, Word);
HOT87(0x08A6BE64u, func_08A6BE64, Word);
HOT87(0x08ABD5ECu, func_08ABD5EC, Word);
// Not registered (every case stops at a sleep that would block (not comparable)): HOT87(0x0883B600u, func_0883B600, Word);
HOT87(0x0889DA58u, func_0889DA58, Word);
HOT87(0x088797F8u, func_088797F8, Word);
HOT87(0x088E537Cu, func_088E537C, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT87(0x089C1368u, func_089C1368, Word);
// Not registered (every case would start a thread (not comparable)): HOT87(0x089C151Cu, func_089C151C, Word);
HOT87(0x089EF8A8u, func_089EF8A8, Word);
HOT87(0x08A52F9Cu, func_08A52F9C, Word);
// Not registered (no compared case changed memory): HOT87(0x0888CFCCu, func_0888CFCC, Word);
HOT87(0x08A57D74u, func_08A57D74, Word);
HOT87(0x08BC0808u, func_08BC0808, Word);
HOT87(0x08809650u, func_08809650, Word);
HOT87(0x08A1503Cu, func_08A1503C, Word);
// Not registered (compared only with a null node (the early return: the tree walk, the shared-pointer release and the string release never ran) and no compared case changed memory): HOT87(0x08B9355Cu, func_08B9355C, Word);
HOT87N(0x088A23ECu, func_088A23EC, Word);
HOT87(0x08993FCCu, func_08993FCC, Word);
HOT87(0x08ABF590u, func_08ABF590, Word);
// Not registered (no compared case changed memory): HOT87(0x08ABECA0u, func_08ABECA0, Word);
HOT87(0x08BE45C4u, func_08BE45C4, Word);
HOT87(0x0881D450u, func_0881D450, Word);
HOT87(0x08A20EC0u, func_08A20EC0, Word);
HOT87(0x08ACFA08u, func_08ACFA08, Word);
HOT87(0x08BAC9ECu, func_08BAC9EC, Word);
HOT87(0x08C4A4ACu, func_08C4A4AC, Word);
// Not registered (its case swallows the compared calls of its decompiled descendants: the destructor chain of this[0xEC]): HOT87(0x0880D478u, func_0880D478, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT87(0x08ABAFCCu, func_08ABAFCC, Word);
HOT87(0x089ADE38u, func_089ADE38, Word);
// Not registered (every case reaches an import stub (not comparable: sceDisplay)): HOT87(0x0897BFECu, func_0897BFEC, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT87(0x08BD787Cu, func_08BD787C, Word);
// Not registered (every case stops on a held LwMutex (not comparable)): HOT87(0x08BCFEF8u, func_08BCFEF8, Word);
HOT87(0x0894CD28u, func_0894CD28, Word);
HOT87(0x089E6520u, func_089E6520, Word);
HOT87(0x089C0360u, func_089C0360, Word);
HOT87(0x0880C1DCu, func_0880C1DC, Word);
HOT87(0x088D2E78u, func_088D2E78, Word);
HOT87(0x08A0BF5Cu, func_08A0BF5C, Word);
HOT87(0x08922CF8u, func_08922CF8, Word);
HOT87(0x0892BE4Cu, func_0892BE4C, Word);
HOT87(0x08A2A724u, func_08A2A724, Word);
