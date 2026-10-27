// Hot batch 93: reached functions of 121-170 instructions (a new probe, `tools.decomp_gate probe b93p`: the five routes with --decomp off over the 557 not-yet-done functions of that size; 192 reached) plus one candidate batch 90 left (func_08B0ACDC, the parent of two functions written then). (main includes batch 92, 6,326 functions.) Before any function was read its address was searched for in src/ (main and the open PR branches).
//
// Why a new probe: after batch 92 the batch-89 probe (71-120 instructions) had 80 reached functions left without an active registration; the ones not yet written are VFPU, call the import stubs from 0x08C70000 or jump through a register, are float-heavy (more than 12 FPU instructions), write no memory of their own or are tangled (the head comments of batches 89-92 give the reasons function by function). Only func_08B0ACDC was left to write. The 121-170 pool is bigger and, clustered with the new `tools.shapes --skeleton` (instruction kinds only: registers, immediates and branch
// offsets ignored, so one function compiled into two register allocations is one cluster), it holds families the masked shape and `tools.family_members` keep apart: the same 154-instruction parser four times (scan_block), a flag handler in two shapes (flag_dispatch_a / _b, 2 + 2 members) with a variant that notifies first (notify_then_gate_b) and one that builds a three-element array (object_call_c), a counted holder assignment twice (assign_holder), a script native twice (args_to_vector, not registered)
// and a vector fill-insert twelve times: a vector of 4- or 16-byte elements with the moves done by callees (vector_fill_insert, 5 members), of 2-byte elements with an inlined byte copy (fill_insert_bytes, 2), and of scalars with a memmove (fill_insert_scalar: halfword, word, float and byte, 5); their reallocating half is one helper (fill_insert_grow) with the size shift, the length limit and the allocator as its row. 26 of the 44 registered functions are instances of nine templates,
// each member's own constants in its row (the wrapper `func_X` carries them, so `tools.literal_check` reads them against that function's own words); the other 18 were read one by one with `tools.disasm`, branch TARGETS and the delay slot of every branch and jal included. The role words in the comments (holder, string, vector, object, virtual slot, key, record) describe the shape of the accesses; none is a name from the game.
//
// Registered (44): see the HOT93 lines at the end; written but not registered (4, each commented HOT93 line says why): func_089B2014 and func_089B2324 (every case stops on `kernel isolation (sleep would block)` / the import stub sceDisplay::0x46F186C3: not comparable, compared 0), func_088EFA1C (its one compared case changed no memory) and func_08A17818 (its 8 compared cases changed no memory: the singleton always existed).
// Left (read, not written): func_0883C988 / func_0883CCC8 (a formatter that passes an `a1` the previous call leaves undefined), the 16- and 32-byte fill-inserts func_08C0E3B8 (an extra word per element), func_08C4BB10 and func_08BF1768 (word elements copied unaligned), and the tangled or float-heavy ones (func_08B22694 has mtlo / madd, func_08AF9010 and func_088DEA1C are likely-branch mazes, func_08B13B84 a table state machine).
//
// Gates (fast gate, five routes uncapped): every registered function compared at least once with 0 mismatches and at least one compared case that changed memory; `tools.decomp_gate sabotage` pins 44 of 44 (the first round pinned 37; its race route was killed by the timeout while another agent's gates held the machine, so the 7 functions only race reaches were sabotaged again alone and all pinned); `tools.contract_audit` finds nothing new (the two registrations of batch 90 whose callers read v1);
// `tools.family_members --check` reads no row of this file; `tools.literal_check` finds 0 literals missing in the 48 functions with 33 `--allow` entries, every one a displacement written as base + step in the asm (`4($v1)` with v1 = this + 0x18 is this + 0x1C here: func_089B0EAC has 26, func_08920E78 4, func_08B0ACDC 2) or a bit the asm tests with bgez / ins (0x80000000 in func_0896EFAC); each one was checked against the asm by hand and all of them are on paths every compared case takes (the three displacements of func_08983F74's `visible` branch, which no case
// takes, are written as base + step so the tool reads them). The helper and template bodies, which the tool skips, were checked by hand: fill_insert_grow (limit 0xFFFFFFFF >> shift = 0x3FFFFFFF / 0x7FFFFFFF / 0x0FFFFFFF, the overflow size -(1 << shift), allocators 08A5BC20(bytes, 4) / 08B35DA0(bytes) / 08B35840(16, bytes), message of each row), release_cell, release_counted, assign_counted_pointer (offsets 4, 8, 0xC), scan_block (0x7D, 0x08D74FDC, the two literals of each member), flag_dispatch_* (the masks 0x04000000 / 0x08000000 / 0x40000000 / 0x80000000 / 0x10000000 / 0x20000000, 0x08CA2878),
// object_call_c (0x08CA2878), copy_0x92 (0x90 and 0x92 bytes in blocks of 16 / 8) and the words of the display list of func_08B544B0 (0xEB000000, 0xEC000000, 0xEE000000, 0xEA000000, 0x10000000, 0x08000000, 0x0B000000 with their ins fields 0/10, 10/10, 0/10, 0/24, 16/8).
// Findings of the first gate: the four members of scan_block faulted because the virtual slot after the loop reads `a1` (the stream) that the original sets in the delay slot of a `bne`; func_08896780 because func_0892B968 / func_0892BB10 take `a0` = this[0x6C] set 30 instructions earlier; func_08BFB68C returned the destination of the previous call where the original has `sll v0, s3, 2` (the void fill callee hands v0 on). A scratch audit of each literal call's argument count against the callee's reads found one more on a path no route runs: func_08BF9B6C takes the end as `a2` (FINDINGS).
// Branches the compared cases do not take are listed in each function's comment (from `tools.decomp_gate coverage`, then read against the asm by hand). The weakest proofs: the loop of func_08896780 and its table walk (race only), the overlap-copy loop of fill_insert_bytes (neither member ever moved elements), the room-left halves of three of the five vector_fill_insert members (the reallocating half ran in two), the second literal and the end-of-block loop exits of scan_block, the `visible != 0` half of func_08983F74, and every path of func_08A6F4D8 with no separator.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value, so a callee that reads a register this function never passes sees what an earlier call_set left (the lesson of the first gate). The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs; $f20 is saved and restored by SavedFloat where it is used. The delay slot of a `jal` / `jalr` runs BEFORE the call,
// a `bnel` / `beql` / `bgtzl` / `blezl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths; every "the delay slot of ..." comment below names the instruction right after the branch it says. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the early returns and the `sll v0` temporaries between two calls; the dead stack (spills the original makes and nothing reads) is not reproduced, the harness ignores it; the frames that callees read are.
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_93 {
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

inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// %hi of an address as the lui / addiu pair builds it (the addiu has the signed %lo).
inline std::uint32_t high_half(std::uint32_t address) { return (address + ((address & 0x8000u) != 0u ? 0x10000u : 0u)) & 0xFFFF0000u; }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (call_indirect's rule).
inline std::uint32_t call_set_v0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = address;
    return call_set(rt, ctx, address, arguments);
}
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
// lwl / lwr pair at `address` + 3 / + 0: the little-endian word that starts at an unaligned address; swl / swr at + 3 / + 0 store one.
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return static_cast<std::uint32_t>(rt.memory().load8(address)) | static_cast<std::uint32_t>(rt.memory().load8(address + 1u)) << 8 |
           static_cast<std::uint32_t>(rt.memory().load8(address + 2u)) << 16 | static_cast<std::uint32_t>(rt.memory().load8(address + 3u)) << 24;
}
inline void put_unaligned(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) rt.memory().store8(address + i, static_cast<std::uint8_t>(value >> (8u * i)));
}

// The ll / sc counters (one thread: a plain load and store). A decrement leaves the old value in a0, v0 = 1 (the sc's success flag) and v1 = the address; an increment leaves v0 = 1 and v1 = the address.
inline std::uint32_t atomic_decrement(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// A virtual call: the slot at `offset` of the object's vtable (the word at object[0]) with the object as a0 and the remaining arguments after it.
inline std::uint32_t slot_of(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t offset) { return word(rt, word(rt, object) + offset); }

// The release of a string cell whose data pointer is `data`: nothing happens when data - 0xC is D_08D74FD0 (the shared empty cell), otherwise func_08B60804(data - 4, -1) and, when its result is not positive, func_08B8542C(data - 0xC, flag). Leaves a0 / a1 like the original (the callee-saved register that holds data - 0xC is not touched: the callee-saved registers are never set by this batch, the original restores them before it returns).
inline void release_cell(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data, std::uint32_t flag) {
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;  // the delay slot of the beq
    if (data - 0xCu == 0x08D74FD0u) return;
    const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = data - 0xCu;
    ctx.gpr[5] = flag;  // the delay slot of the bgtz
    if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, flag});
}

// mtc1 / cvt.s.w / div.s / mul.s on the context's float registers: mtc1 moves the raw bits, cvt.s.w converts the integer in the register's bits.
inline void mtc1(psprecomp::AllegrexContext &ctx, unsigned fd, std::uint32_t bits) { ctx.fpr[fd] = std::bit_cast<float>(bits); }
inline void cvt_s_w(psprecomp::AllegrexContext &ctx, unsigned fd, unsigned fs) { ctx.fpr[fd] = static_cast<float>(std::bit_cast<std::int32_t>(ctx.fpr[fs])); }
inline std::uint32_t ins_bits(std::uint32_t target, std::uint32_t source, unsigned pos, unsigned size) {
    const std::uint32_t mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((source << pos) & mask);
}

// The release of a counted object: one reference at +4 goes; when that was the last its virtual slot 8 runs, one reference at +8 goes and, when that was the last too, its virtual slot 0xC runs. The counters are ll / sc loops (one thread: a load and a store).
inline void release_counted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    ctx.gpr[3] = object + 4u;
    if (atomic_decrement(rt, ctx, object + 4u) != 1u) return;
    ctx.gpr[2] = 1u;
    (void)call_set(rt, ctx, slot_of(rt, object, 8u), {object});
    ctx.gpr[3] = object + 8u;
    if (atomic_decrement(rt, ctx, object + 8u) != 1u) return;
    ctx.gpr[2] = 1u;
    (void)call_set(rt, ctx, slot_of(rt, object, 0xCu), {object});
}

// A counted pointer slot replaced by `incoming` (the ll / sc counters read as plain load and store): the new object gets one more reference at +4 first (v0 = 1 and v1 = its counter's address, the sc's flag), the old one loses its reference at +4 and, when that was the last, its virtual slot 8 runs, it loses the reference
// at +8 and, when that was the last too, its virtual slot 0xC runs; the slot is stored at the end on every path.
inline void assign_counted_pointer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t incoming) {
    if (incoming != 0u) {
        put(rt, incoming + 4u, word(rt, incoming + 4u) + 1u);
        ctx.gpr[3] = incoming + 4u;
        ctx.gpr[2] = 1u;
    }
    const std::uint32_t old = word(rt, slot);
    ctx.gpr[3] = old + 4u;
    if (old != 0u && atomic_decrement(rt, ctx, old + 4u) == 1u) {
        ctx.gpr[2] = 1u;
        (void)call_set(rt, ctx, slot_of(rt, old, 8u), {old});
        ctx.gpr[3] = old + 8u;
        if (atomic_decrement(rt, ctx, old + 8u) == 1u) {
            ctx.gpr[2] = 1u;
            (void)call_set(rt, ctx, slot_of(rt, old, 0xCu), {old});
        }
    }
    put(rt, slot, incoming);
}

// The ll / sc increment of a reference counter (one thread: a plain load and store): v0 = 1 (the sc's flag) and v1 = the counter's address stay behind.
inline void atomic_increment(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// scan_block<First, Second>(this, stream): a copy t of the string at this + 4 is compared with the literal First (the common length, then the length difference when the bytes agree); when it is not equal the same copy is compared with the literal Second;
// each literal string and the copy are released (release_cell). When one of the two compares was equal, the cell sp[0x20] is set to the shared empty cell + 0xC (D_08D74FDC) and func_08987A74(stream, &cell) is called twice (the result of the first call is
// dropped); then, while the next func_08987A74(stream, &cell) is neither -1 nor 0x7D, the virtual slot 8 of this[0xC] decides: with a zero answer func_0898682C(stream, 0) runs. The cell is released at the end. Four members (func_08975314, func_089A16CC, func_089F8B2C, func_08A05D3C) differ only in the two literals.
// v0 is the last call's answer on every path (the call results stay in v0 like the original's; the delay slots that load a0 or s1 have no effect on it).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (answer != 0u) difference = answer;`; `if (difference != 0u)`; `if (answer2 != 0u) difference2 = answer2;`; `if (!equal) return;`; `if (item == 0xFFFFFFFFu || item == 0x7Du) break;`; `if (decided == 0u) (void)call_set(rt, ctx, 0x0898682Cu, {stream, 0u});`
template <std::uint32_t First, std::uint32_t Second>
void scan_block(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], stream = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t copy_a = frame.at(0x14u), copy_b = frame.at(0x10u), cell = frame.at(0x20u);
    (void)call_set(rt, ctx, 0x08B88F44u, {copy_a, self + 4u});
    (void)call_set(rt, ctx, 0x08B85EB8u, {frame.at(0xCu), First, frame.at(6u)});
    const std::uint32_t text = word(rt, copy_a), literal = word(rt, frame.at(0xCu));
    const std::uint32_t text_length = word(rt, text - 0xCu), literal_length = word(rt, literal - 0xCu);
    std::uint32_t difference = text_length - literal_length;  // s0
    const std::uint32_t answer = call_set(rt, ctx, 0x08B36170u, {text, literal, std::min(literal_length, text_length)});
    if (answer != 0u) difference = answer;  // the movn in the delay slot of the beq: both paths
    release_cell(rt, ctx, literal, frame.at(5u));
    bool equal = true;  // s1 = 1 in the delay slot of the beqz
    if (difference != 0u) {
        (void)call_set(rt, ctx, 0x08B88F44u, {copy_b, self + 4u});
        (void)call_set(rt, ctx, 0x08B85EB8u, {frame.at(8u), Second, frame.at(4u)});
        const std::uint32_t text2 = word(rt, copy_b), literal2 = word(rt, frame.at(8u));
        const std::uint32_t text2_length = word(rt, text2 - 0xCu), literal2_length = word(rt, literal2 - 0xCu);
        std::uint32_t difference2 = text2_length - literal2_length;  // s1
        const std::uint32_t answer2 = call_set(rt, ctx, 0x08B36170u, {text2, literal2, std::min(literal2_length, text2_length)});
        if (answer2 != 0u) difference2 = answer2;
        release_cell(rt, ctx, literal2, frame.at(3u));
        equal = difference2 == 0u;  // sltiu s1, s1, 1 in the delay slot of the beq
        release_cell(rt, ctx, word(rt, copy_b), frame.at(2u));
    }
    release_cell(rt, ctx, word(rt, copy_a), frame.at(1u));
    if (!equal) return;  // the beqz s1: v0 stays the last call's
    ctx.gpr[2] = 0x08D74FDCu;  // the empty cell + 0xC
    put(rt, cell, 0x08D74FDCu);
    (void)call_set(rt, ctx, 0x08987A74u, {stream, cell});
    for (;;) {
        const std::uint32_t item = call_set(rt, ctx, 0x08987A74u, {stream, cell});
        if (item == 0xFFFFFFFFu || item == 0x7Du) break;
        const std::uint32_t owner = word(rt, self + 0xCu);
        ctx.gpr[5] = stream;  // the delay slot of the bne
        ctx.gpr[6] = cell;
        const std::uint32_t decided = call_set(rt, ctx, slot_of(rt, owner, 8u), {owner, stream, cell});
        ctx.gpr[4] = stream;
        ctx.gpr[5] = 0u;  // the delay slot of the bnez
        if (decided == 0u) (void)call_set(rt, ctx, 0x0898682Cu, {stream, 0u});
    }
    release_cell(rt, ctx, word(rt, cell), frame.at(0u));
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// The shared middle of func_0896ED94, func_0896EFAC, func_0896F5AC and func_0896F7C4 (a handler that builds a small object `first` = sp[0x14] from `self` and a literal, then, when it is non-empty, a second object sp[0x18] and a 2-element array sp[0x1C..0x24)): returns false when
// `first` came back empty (nothing else was built). Otherwise two func_08A76AAC pairs record `item` and `value`, the virtual slot 0xCC of `first` runs with (second, 2, array), and when sp[0x18] is non-zero its virtual slot 0x3C gives `result` (otherwise `result` = `empty_result`); the array, `second` and
// `first` are destroyed (func_08A5B9EC, the array from its end) on the way out. Every call passes the argument registers the original sets before the jal; the delay slot stores before the jalr / jal run first (sp[8] = item, sp[0] = value).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `std::uint32_t &result)`; `if (made == 0u) return false;`; `if (array != frame.at(0xCu)) (void)call_set(rt, ctx, 0x08A5BAD8u, {array, word(rt, frame.at(0xCu))});`; `if (frame.at(0x20u) != frame.at(4u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x20u), word(rt, frame.at(`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2878u});`; `if (child == 0u)`; `if (cursor == array) break;`; `if (cursor == array) break;`
inline bool flag_object_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t self, std::uint32_t item, std::uint32_t value, std::uint32_t literal, std::uint32_t empty_result,
                             std::uint32_t &result) {
    const std::uint32_t first = frame.at(0x14u), second = frame.at(0x18u), array = frame.at(0x1Cu), array_end = frame.at(0x24u);
    (void)call_set(rt, ctx, 0x08A76AA0u, {first});
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0x10u), literal});
    (void)call_set(rt, ctx, 0x08A7BFD0u, {self, first, frame.at(0x10u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0x10u)});
    const std::uint32_t made = word(rt, first);
    ctx.gpr[2] = made;
    if (made == 0u) return false;
    (void)call_set(rt, ctx, 0x08A76AA0u, {second});
    (void)call_set(rt, ctx, 0x08A76AA0u, {array});
    (void)call_set(rt, ctx, 0x08A76AA0u, {array + 4u});
    put(rt, frame.at(8u), item);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(0xCu), frame.at(8u)});
    if (array != frame.at(0xCu)) (void)call_set(rt, ctx, 0x08A5BAD8u, {array, word(rt, frame.at(0xCu))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    put(rt, frame.at(0u), value);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(4u), frame.at(0u)});
    if (frame.at(0x20u) != frame.at(4u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x20u), word(rt, frame.at(4u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    const std::uint32_t object = word(rt, first);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2878u});
    (void)call_set(rt, ctx, slot_of(rt, object, 0xCCu), {object, second, 2u, array});
    const std::uint32_t child = word(rt, second);
    ctx.gpr[4] = child;
    if (child == 0u) {
        result = empty_result;  // the delay slot of the beql (s0 = array_end) runs, then the loop below
        for (std::uint32_t cursor = array_end - 4u;; cursor -= 4u) {
            (void)call_set(rt, ctx, 0x08A5B9ECu, {cursor});
            if (cursor == array) break;
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {second});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {first});
        ctx.gpr[2] = result;
        return true;
    }
    result = call_set(rt, ctx, slot_of(rt, child, 0x3Cu), {child});
    for (std::uint32_t cursor = array_end - 4u;; cursor -= 4u) {
        (void)call_set(rt, ctx, 0x08A5B9ECu, {cursor});
        if (cursor == array) break;
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {second});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {first});
    ctx.gpr[2] = result;
    return true;
}

// flag_dispatch_a<Mask, Literal>(this, item, value): func_0896ED94 (Mask 0x40000000) and func_0896EFAC (Mask 0x80000000). When this[0x60] has bit 2 and this[0xE0] has Mask, flag_object_call(this, item, value, Literal) runs and its result is returned (0 when `first` was empty
// after it is destroyed); otherwise this[0xE0] is stored back with Mask cleared and the result is 0. v0 = the result on every path.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if ((word(rt, self + 0x60u) & 4u) == 0u || (word(rt, self + 0xE0u) & Mask) == 0u)`; `if (!flag_object_call(rt, ctx, frame, self, item, value, Literal, 0u, result))`
template <std::uint32_t Mask, std::uint32_t Literal>
void flag_dispatch_a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], item = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    if ((word(rt, self + 0x60u) & 4u) == 0u || (word(rt, self + 0xE0u) & Mask) == 0u) {
        put(rt, self + 0xE0u, word(rt, self + 0xE0u) & ~Mask);
        ctx.gpr[2] = 0u;
        return;
    }
    std::uint32_t result = 0u;
    if (!flag_object_call(rt, ctx, frame, self, item, value, Literal, 0u, result)) {
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
        result = 0u;
    }
    ctx.gpr[2] = result;
}

// flag_gate_b(this, item, value, Mask, Literal, Fail): the handler body of func_0896F5AC and func_0896F7C4 (Fail 1) and of func_0897006C and func_089702C8 after their first steps (Fail 0): without bit 2 of this[0x60] or without Mask in this[0xE0] the result is Fail and nothing is stored; otherwise flag_object_call; when `first` came back
// empty this[0xE0] is stored back with Mask cleared, `first` is destroyed and the result is Fail. v0 = the result.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if ((word(rt, self + 0x60u) & 4u) == 0u || (word(rt, self + 0xE0u) & mask) == 0u)`; `if (!flag_object_call(rt, ctx, frame, self, item, value, literal, fail, result))`
inline void flag_gate_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self, std::uint32_t item, std::uint32_t value, std::uint32_t mask, std::uint32_t literal, std::uint32_t fail) {
    GuestFrame frame(ctx, 0x50u);
    if ((word(rt, self + 0x60u) & 4u) == 0u || (word(rt, self + 0xE0u) & mask) == 0u) {
        ctx.gpr[2] = fail;
        return;
    }
    std::uint32_t result = fail;
    if (!flag_object_call(rt, ctx, frame, self, item, value, literal, fail, result)) {
        put(rt, self + 0xE0u, word(rt, self + 0xE0u) & ~mask);
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
        result = fail;
    }
    ctx.gpr[2] = result;
}

// flag_dispatch_b<Mask, Literal>(this, item, value): func_0896F5AC (Mask 0x04000000) and func_0896F7C4 (Mask 0x08000000), result 1 where flag_dispatch_a has 0.
template <std::uint32_t Mask, std::uint32_t Literal>
void flag_dispatch_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    flag_gate_b(rt, ctx, ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], Mask, Literal, 1u);
}

// notify_then_gate_b<Mask, Literal, Notify>(this, item, owner): func_0897006C (Mask 0x10000000, with the extra steps `Notify` = 0) and func_089702C8 (Mask 0x20000000, Notify = 1): the virtual slot 0x118 of `owner` (a2) is called; when it answers `this` and item is non-zero, item is handled first:
// func_08968B9C(item, this) (Notify 0, after func_089AAC8C(item) and, when that is non-zero, func_089A4CBC(its answer)) or func_08968B00(item, this) (Notify 1). Then flag_gate_b(this, item, owner, Mask, Literal, 0) decides the result (its `value` is the a2 of the entry).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (answer == self && item != 0u)`; `if (Notify == 0u)`; `if (found != 0u) (void)call_set(rt, ctx, 0x089A4CBCu, {found});`; `} else`
template <std::uint32_t Mask, std::uint32_t Literal, unsigned Notify>
void notify_then_gate_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], item = ctx.gpr[5], owner = ctx.gpr[6];
    const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, owner, 0x118u), {owner});
    if (answer == self && item != 0u) {
        if (Notify == 0u) {
            ctx.gpr[4] = item;  // the delay slot of the beql chain
            const std::uint32_t found = call_set(rt, ctx, 0x089AAC8Cu, {item});
            ctx.gpr[4] = item;  // the delay slot of the beqz
            if (found != 0u) (void)call_set(rt, ctx, 0x089A4CBCu, {found});
            (void)call_set(rt, ctx, 0x08968B9Cu, {item, self});
        } else {
            ctx.gpr[4] = item;  // the delay slot of the beqz
            (void)call_set(rt, ctx, 0x08968B00u, {item, self});
        }
    }
    flag_gate_b(rt, ctx, self, item, owner, Mask, Literal, 0u);
}

// object_call_c<Literal>(self, item, value): func_0896F9DC and func_0896FE3C (they differ in the literal only). Without bit 2 of self[0x60] the result is 0. Otherwise, as in flag_object_call, an object `first` = sp[0x1C] is built from `self` and Literal (empty: destroyed, result 0), then a second object sp[0x20] and a 3-element array sp[0x24..0x30)
// whose elements are filled by three func_08A76AAC pairs (item, value, self); the virtual slot 0xCC of `first` runs with (second, 3, array); with sp[0x20] non-zero its virtual slot 0x3C is the result (otherwise 0); the array (from its end), the second and the first object are destroyed (func_08A5B9EC).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if ((word(rt, self + 0x60u) & 4u) == 0u)`; `if (made == 0u)`; `if (array != frame.at(0x14u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {array, word(rt, frame.at(0x14u))});`; `if (frame.at(0x28u) != frame.at(0xCu)) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x28u), word(rt, frame.a`; `if (frame.at(0x2Cu) != frame.at(4u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x2Cu), word(rt, frame.at(`; `if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2878u});`; `if (child != 0u) result = call_set(rt, ctx, slot_of(rt, child, 0x3Cu), {child});`; `if (cursor == array) break;`
template <std::uint32_t Literal>
void object_call_c(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], item = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    if ((word(rt, self + 0x60u) & 4u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t first = frame.at(0x1Cu), second = frame.at(0x20u), array = frame.at(0x24u), array_end = frame.at(0x30u);
    (void)call_set(rt, ctx, 0x08A76AA0u, {first});
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0x18u), Literal});
    (void)call_set(rt, ctx, 0x08A7BFD0u, {self, first, frame.at(0x18u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0x18u)});
    const std::uint32_t made = word(rt, first);
    ctx.gpr[2] = made;
    if (made == 0u) {
        (void)call_set(rt, ctx, 0x08A5B9ECu, {first});
        ctx.gpr[2] = 0u;
        return;
    }
    (void)call_set(rt, ctx, 0x08A76AA0u, {second});
    (void)call_set(rt, ctx, 0x08A76AA0u, {array});
    (void)call_set(rt, ctx, 0x08A76AA0u, {array + 4u});
    (void)call_set(rt, ctx, 0x08A76AA0u, {array + 8u});
    put(rt, frame.at(0x10u), item);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(0x14u), frame.at(0x10u)});
    if (array != frame.at(0x14u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {array, word(rt, frame.at(0x14u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
    put(rt, frame.at(8u), value);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(0xCu), frame.at(8u)});
    if (frame.at(0x28u) != frame.at(0xCu)) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x28u), word(rt, frame.at(0xCu))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    put(rt, frame.at(0u), self);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(4u), frame.at(0u)});
    if (frame.at(0x2Cu) != frame.at(4u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x2Cu), word(rt, frame.at(4u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    const std::uint32_t object = word(rt, first);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2878u});
    (void)call_set(rt, ctx, slot_of(rt, object, 0xCCu), {object, second, 3u, array});
    const std::uint32_t child = word(rt, second);
    ctx.gpr[4] = child;
    std::uint32_t result = 0u;
    if (child != 0u) result = call_set(rt, ctx, slot_of(rt, child, 0x3Cu), {child});
    for (std::uint32_t cursor = array_end - 4u;; cursor -= 4u) {
        (void)call_set(rt, ctx, 0x08A5B9ECu, {cursor});
        if (cursor == array) break;
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {second});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {first});
    ctx.gpr[2] = result;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// assign_holder<Offset, ReturnsThis>(this, src): func_088D8F8C (the holder at this + 0x65C, v0 untouched) and func_08B9A958 (the holder is `this`, v0 = this). With d = the holder: nothing happens when d == src. Otherwise d's vector {begin, end, capacity} (d[0], d[4], d[8]) is cleared and handed to func_08B8FE44(begin, end, sp) with the three words at sp[4], sp[8], sp[0xC];
// a non-zero sp[4] is freed (func_08B35254); func_08B90024(d, src) copies src's vector; d[0xC] = src[0xC]; the counted pointer d[0x10] becomes src[0x10] when they differ (assign_counted_pointer); d[0x14] = src[0x14]; the counted pointer d[0x18] becomes src[0x18] when they differ.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (holder == src)`; `if (ReturnsThis) ctx.gpr[2] = self;`; `if (buffer != 0u) (void)call_set(rt, ctx, 0x08B35254u, {buffer});`; `if (first != ctx.gpr[2]) assign_counted_pointer(rt, ctx, holder + 0x10u, first);`; `if (second != current) assign_counted_pointer(rt, ctx, holder + 0x18u, second);`; `if (ReturnsThis) ctx.gpr[2] = self;`
template <std::uint32_t Offset, bool ReturnsThis>
void assign_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], src = ctx.gpr[5];
    const std::uint32_t holder = self + Offset;
    GuestFrame frame(ctx, 0x30u);
    if (holder == src) {
        if (ReturnsThis) ctx.gpr[2] = self;
        return;
    }
    const std::uint32_t begin = word(rt, holder), end = word(rt, holder + 4u), capacity = word(rt, holder + 8u);
    put(rt, holder, 0u);
    put(rt, holder + 4u, 0u);
    put(rt, holder + 8u, 0u);
    put(rt, frame.at(0xCu), capacity);
    put(rt, frame.at(4u), begin);
    put(rt, frame.at(8u), end);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B8FE44u, {begin, end, frame.at(0u), capacity});
    const std::uint32_t buffer = word(rt, frame.at(4u));
    ctx.gpr[4] = holder;  // the delay slot of the beql
    if (buffer != 0u) (void)call_set(rt, ctx, 0x08B35254u, {buffer});
    (void)call_set(rt, ctx, 0x08B90024u, {holder, src});
    const std::uint32_t word_c = word(rt, src + 0xCu);
    ctx.gpr[2] = word(rt, holder + 0x10u);
    put(rt, holder + 0xCu, word_c);
    const std::uint32_t first = word(rt, src + 0x10u);
    if (first != ctx.gpr[2]) assign_counted_pointer(rt, ctx, holder + 0x10u, first);
    const std::uint32_t word_14 = word(rt, src + 0x14u);
    ctx.gpr[2] = word_14;
    const std::uint32_t current = word(rt, holder + 0x18u);
    ctx.gpr[3] = current;
    put(rt, holder + 0x14u, word_14);
    const std::uint32_t second = word(rt, src + 0x18u);
    if (second != current) assign_counted_pointer(rt, ctx, holder + 0x18u, second);
    if (ReturnsThis) ctx.gpr[2] = self;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// The reallocating half shared by the fill-insert functions (func_08B85F30, func_08C02A4C, func_08C254A8, func_08B8AB38, func_08BEF324, func_08BFB68C, func_08BDDD68, func_08BDE864, func_08C4B8C0): the vector {begin, end, capacity} of elements of (1 << shift) bytes receives `count` copies of *value before `position`.
// size = (end - begin) >> shift; a size over `limit` - count calls func_08B5ADDC(message) first; bytes = (size + max(size, count)) << shift (-(1 << shift) on overflow of the sum); the block comes from func_08A5BC20(bytes, 4) (the old block is freed with func_08A5BC54(begin, ((capacity - begin) >> shift) << shift, 4)) or, with
// `plain`, from func_08B35DA0(bytes) (nothing is allocated when the sum is 0; the old block is freed with func_08B35254(begin)). Then e = copy_block(begin, position, block, sp + 3), fill_block(e, count, value, sp + 2), last = copy_block(position, end', e + (count << shift), sp + 1), destroy_range(begin, end', sp), the old block is freed and the vector becomes {block, last, block + bytes}; v0 = block + bytes.
struct GrowRow {
    std::uint32_t copy_block, fill_block, destroy_range, message, limit;
    unsigned shift;
    bool plain;
    std::uint32_t align = 0u;  // non-zero: the block comes from func_08B35840(align, bytes) (no zero check) and is freed like the plain one
};
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (g.limit - size < count) (void)call_set(rt, ctx, 0x08B5ADDCu, {g.message});`; `const std::uint32_t larger = size < count ? count : size;`; `if (g.align != 0u)`; `if (!overflow) bytes = sum << g.shift;`; `} else if (!g.plain)`; `if (!overflow) bytes = sum << g.shift;`; `if (!overflow)`; `if (sum == 0u)`; `if (allocate)`; `if (!g.plain && g.align == 0u)`; `if (old_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_begin, static_cast<std::uint32_t>(signed_of(cap`; `if (old_begin != 0u) (void)call_set(rt, ctx, 0x08B35254u, {old_begin});`
inline void fill_insert_grow(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t vector, std::uint32_t position, std::uint32_t count, std::uint32_t value, const GrowRow &g) {
    const std::uint32_t end = word(rt, vector + 4u);
    std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> g.shift);
    put(rt, frame.at(0xCu), size);
    if (g.limit - size < count) (void)call_set(rt, ctx, 0x08B5ADDCu, {g.message});
    const std::uint32_t larger = size < count ? count : size;
    const std::uint32_t sum = size + larger;
    const bool overflow = sum < size;
    std::uint32_t bytes = 0u - (1u << g.shift);
    std::uint32_t block = 0u;
    if (g.align != 0u) {
        if (!overflow) bytes = sum << g.shift;
        block = call_set(rt, ctx, 0x08B35840u, {g.align, bytes});
        begin = word(rt, vector);
    } else if (!g.plain) {
        if (!overflow) bytes = sum << g.shift;
        block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    } else {
        bool allocate = true;
        if (!overflow) {
            bytes = 0u;
            if (sum == 0u) {
                allocate = false;
            } else {
                bytes = sum << g.shift;
            }
        }
        if (allocate) {
            block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
            begin = word(rt, vector);
        }
    }
    const std::uint32_t first = call_set(rt, ctx, g.copy_block, {begin, position, block, frame.at(3u)});
    (void)call_set(rt, ctx, g.fill_block, {first, count, value, frame.at(2u)});
    const std::uint32_t last = call_set(rt, ctx, g.copy_block, {position, word(rt, vector + 4u), (count << g.shift) + first, frame.at(1u)});
    (void)call_set(rt, ctx, g.destroy_range, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_begin = word(rt, vector), capacity = word(rt, vector + 8u);
    if (!g.plain && g.align == 0u) {
        ctx.gpr[4] = old_begin;
        ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(capacity - old_begin) >> g.shift);
        if (old_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_begin, static_cast<std::uint32_t>(signed_of(capacity - old_begin) >> g.shift) << g.shift, 4u});
    } else {
        ctx.gpr[2] = block + bytes;  // the delay slot of the beqz
        if (old_begin != 0u) (void)call_set(rt, ctx, 0x08B35254u, {old_begin});
    }
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, last);
    put(rt, vector, block);
}

// The callees of vector_fill_insert (the row of each member): `temp_construct(sp + 8, value)`, `move_tail(end - 4n, end, end, sp + 6)`, `fill(first, last, temp)`, `fill_uninit(end, count, temp, sp + 5)`, `copy_uninit(position, end, dest, sp + 4)`, and for the reallocating half `copy_to_block(first, last, dest, sp + 3 / sp + 1)`,
// `fill_block(e, n, value, sp + 2)`, `destroy_range(begin, end, sp)` and the message of the length error (the argument of func_08B5ADDC).
struct FillInsertRow {
    std::uint32_t temp_construct, move_tail, fill, fill_uninit, copy_uninit, copy_to_block, fill_block, destroy_range, message;
    std::uint32_t temp_destroy;  // func_08A5B9EC, or the member's own
    std::uint32_t assign;        // 0: the element move is func_08A5BAD8(dest, *source) (skipped when dest == source); otherwise assign(dest, source) always
    unsigned shift;              // log2 of the element size
    std::uint32_t align;         // 0: func_08A5BC20(bytes, 4) allocates; otherwise func_08B35840(align, bytes)
};

// vector_fill_insert<Row>(vector, position, count, value): insert `count` copies of *value before `position` into a vector of 4-byte elements {begin, end, capacity} (func_08B85F30, func_08C02A4C). Nothing happens when count == 0 (v0 untouched). With room (count <= (capacity - end) >> 2) a temporary copy of the value
// is made at sp + 8; with n = (end - position) >> 2 elements behind the position: n > count moves the last `count` elements up (move_tail, then one func_08A5BAD8(dest, *source) per element of the overlap, from the back), moves the end by count and fills [position, position + count); otherwise the tail is filled
// past the end (count - n copies), the end moves, the old tail is copied after it, the end moves by n and [position, old end) is filled; the temporary is destroyed (func_08A5B9EC, v0 = its result). Without room: size = (end - begin) >> 2, a size over 0x3FFFFFFF - count calls func_08B5ADDC(message) first, bytes = 4 * (size + max(size, count)) (-4 on overflow),
// block = func_08A5BC20(bytes, 4), e = copy_to_block(begin, position, block), e' = fill_block(e, count, value), copy_to_block(position, end, e + 4 * count), destroy_range(begin, end), the old block is freed with func_08A5BC54(begin, ((capacity - begin) >> 2) * 4, 4) and the vector becomes {block, new end, block + bytes}; v0 = block + bytes.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (count == 0u) return;`; `if (room < count)`; `const std::uint32_t temp = frame.at(8u);`; `if (count < tail)`; `if (signed_of(tail) > 0)`; `for (std::uint32_t done = 0u; done != tail; ++done)`; `if (Row.assign != 0u)`; `} else if (to != from)`; `if (Row.temp_destroy != 0u) (void)call_set(rt, ctx, Row.temp_destroy, {temp});`
template <FillInsertRow Row>
void vector_fill_insert(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr unsigned shift = Row.shift;
    constexpr std::uint32_t size = 1u << shift;
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], count = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x10u), count);  // the delay slot of the beqz
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> shift);
    if (room < count) {
        fill_insert_grow(rt, ctx, frame, vector, position, count, value, GrowRow{Row.copy_to_block, Row.fill_block, Row.destroy_range, Row.message, 0xFFFFFFFFu >> shift, shift, false, Row.align});
        return;
    }
    const std::uint32_t temp = frame.at(8u);
    (void)call_set(rt, ctx, Row.temp_construct, {temp, value});
    std::uint32_t tail = static_cast<std::uint32_t>(signed_of(end - position) >> shift);  // s3
    if (count < tail) {
        (void)call_set(rt, ctx, Row.move_tail, {end - (count << shift), end, end, frame.at(6u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (count << shift));
        ctx.gpr[2] = word(rt, vector + 4u);  // addu v0, v0, v1
        std::uint32_t from = end - (count << shift);  // s0
        tail = static_cast<std::uint32_t>(signed_of(from - position) >> shift);
        if (signed_of(tail) > 0) {
            std::uint32_t to = end;  // s2
            for (std::uint32_t done = 0u; done != tail; ++done) {
                to -= size;
                from -= size;
                if (Row.assign != 0u) {
                    ctx.gpr[5] = from;  // the delay slot of the jal
                    (void)call_set(rt, ctx, Row.assign, {to, from});
                } else if (to != from) {
                    (void)call_set(rt, ctx, 0x08A5BAD8u, {to, word(rt, from)});
                }
            }
        }
        (void)call_set(rt, ctx, Row.fill, {position, position + (count << shift), temp});
    } else {
        ctx.gpr[2] = end - position;
        (void)call_set(rt, ctx, Row.fill_uninit, {end, count - tail, temp, frame.at(5u)});
        const std::uint32_t new_end = word(rt, vector + 4u) + ((count - tail) << shift);
        put(rt, vector + 4u, new_end);
        ctx.gpr[2] = (count - tail) << shift;
        (void)call_set(rt, ctx, Row.copy_uninit, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (tail << shift));
        ctx.gpr[2] = tail << shift;  // sll v0, s3, shift
        (void)call_set(rt, ctx, Row.fill, {position, end, temp});
    }
    if (Row.temp_destroy != 0u) (void)call_set(rt, ctx, Row.temp_destroy, {temp});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// The callees of fill_insert_bytes (the row of each member): move_tail(end - 2n, end, end, sp + 6), fill(first, last, temp), fill_uninit(end, count, temp, sp + 5), copy_uninit(position, end, dest, sp + 4), fill_block(e, n, value, sp + 2), copy_block(first, last, dest, sp + 3 / sp + 1), destroy_range(begin, end, sp)
// and the message of the length error (the argument of func_08B5ADDC).
struct FillInsertBytesRow {
    std::uint32_t move_tail, fill, fill_uninit, copy_uninit, fill_block, copy_block, destroy_range, message;
};

// fill_insert_bytes<Row>(vector, position, count, value): vector_fill_insert for a vector of 2-byte elements {begin, end, capacity} (func_08BDDD68, func_08BDE864): the element is two bytes that the function copies itself (the temporary at sp + 7 / sp + 8, the overlap of the move is a byte-pair loop), the room is
// counted in halfwords, the reallocating half is fill_insert_grow (plain allocator, limit 0x7FFFFFFF). Nothing happens when count == 0 (v0 untouched); v0 = the last call's result, or block + bytes after a reallocation.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (count == 0u) return;`; `if (room < count)`; `const std::uint32_t low = byte_u(rt, value), high = byte_u(rt, value + 1u);`; `if (count < tail)`; `if (signed_of(moved) > 0)`; `for (std::uint32_t done = 0u; done != moved; ++done)`
template <FillInsertBytesRow Row>
void fill_insert_bytes(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], count = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x10u), count);  // the delay slot of the beqz
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 1);
    if (room < count) {
        fill_insert_grow(rt, ctx, frame, vector, position, count, value, GrowRow{Row.copy_block, Row.fill_block, Row.destroy_range, Row.message, 0x7FFFFFFFu, 1u, true});
        return;
    }
    const std::uint32_t low = byte_u(rt, value), high = byte_u(rt, value + 1u);
    std::uint32_t tail = static_cast<std::uint32_t>(signed_of(end - position) >> 1);
    put8(rt, frame.at(7u), low);
    put8(rt, frame.at(8u), high);
    const std::uint32_t temp = frame.at(7u);
    if (count < tail) {
        (void)call_set(rt, ctx, Row.move_tail, {end - (count << 1), end, end, frame.at(6u)});
        std::uint32_t from = end - (count << 1);
        const std::uint32_t moved = static_cast<std::uint32_t>(signed_of(from - position) >> 1);
        put(rt, vector + 4u, word(rt, vector + 4u) + (count << 1));
        ctx.gpr[2] = word(rt, vector + 4u);  // addu v0, v0, v1
        std::uint32_t to = end;
        if (signed_of(moved) > 0) {
            for (std::uint32_t done = 0u; done != moved; ++done) {
                from -= 2u;
                to -= 2u;
                put8(rt, to, byte_u(rt, from));
                ctx.gpr[2] = byte_u(rt, from + 1u);  // lbu v0, 1(a1)
                put8(rt, to + 1u, ctx.gpr[2]);
            }
        }
        (void)call_set(rt, ctx, Row.fill, {position, position + (count << 1), temp});
    } else {
        ctx.gpr[2] = end - position;
        (void)call_set(rt, ctx, Row.fill_uninit, {end, count - tail, temp, frame.at(5u)});
        const std::uint32_t new_end = word(rt, vector + 4u) + ((count - tail) << 1);
        put(rt, vector + 4u, new_end);
        ctx.gpr[2] = (count - tail) << 1;
        (void)call_set(rt, ctx, Row.copy_uninit, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (tail << 1));
        ctx.gpr[2] = tail << 1;  // sll v0, s3, 1
        (void)call_set(rt, ctx, Row.fill, {position, end, temp});
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// The callees of fill_insert_scalar (the row of each member).
struct FillInsertScalarRow {
    std::uint32_t move_tail, fill, fill_uninit, copy_uninit, copy_block, fill_block, destroy_range, message;
    unsigned kind;  // 0: a halfword element (lhu / sh), 1: a word (lw / sw), 2: a float (lwc1 / swc1: the value also stays in $f0), 3: a byte (lbu / sb)
    bool plain;     // the allocator of fill_insert_grow
};

// fill_insert_scalar<Row>(vector, position, count, value): vector_fill_insert for a vector of scalars {begin, end, capacity}: func_08B8AB38 (halfwords), func_08BFB68C (words), func_08BEF324 (floats) and func_08C4B8C0 (halfwords, the plain allocator). Nothing happens when count == 0 (v0 untouched). With room, the element is copied to sp + 8; with
// n = (end - position) >> shift: n > count moves the last `count` elements up with move_tail(end - count*size, end, end, sp + 6), then func_08B79BAC(end - bytes, position, bytes) moves the overlap (bytes = (end - count*size - position) with its low `shift` bits cleared), the end grows by count*size and fill(position, position + count*size, temp) runs; otherwise
// fill_uninit(end, count - n, temp, sp + 5), the end grows, copy_uninit(position, end, new end, sp + 4), the end grows by n*size and fill(position, end, temp) runs. v0 = the last call's result. Without room fill_insert_grow does it.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `constexpr unsigned shift = Row.kind == 0u ? 1u : (Row.kind == 3u ? 0u : 2u);`; `if (count == 0u) return;`; `if (room < count)`; `if (Row.kind == 0u)`; `} else if (Row.kind == 3u)`; `ctx.gpr[2] = count < tail ? 1u : 0u;`; `if (Row.kind == 2u) ctx.fpr[0] = float_at(rt, value);`; `if (count < tail)`; `if (shift != 0u) ctx.gpr[2] = tail << shift;`
template <FillInsertScalarRow Row>
void fill_insert_scalar(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr unsigned shift = Row.kind == 0u ? 1u : (Row.kind == 3u ? 0u : 2u);
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], count = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x10u), count);  // the delay slot of the beqz
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> shift);
    if (room < count) {
        const GrowRow grow{Row.copy_block, Row.fill_block, Row.destroy_range, Row.message, 0xFFFFFFFFu >> shift, shift, Row.plain};
        fill_insert_grow(rt, ctx, frame, vector, position, count, value, grow);
        return;
    }
    const std::uint32_t temp = frame.at(8u);
    std::uint32_t tail = static_cast<std::uint32_t>(signed_of(end - position) >> shift);
    if (Row.kind == 0u) {
        put16(rt, temp, half_u(rt, value));
        ctx.gpr[2] = half_u(rt, value);
    } else if (Row.kind == 3u) {
        put8(rt, temp, byte_u(rt, value));
        ctx.gpr[2] = count < tail ? 1u : 0u;  // the sltu that picks the branch
    } else {
        put(rt, temp, word(rt, value));
        if (Row.kind == 2u) ctx.fpr[0] = float_at(rt, value);
        else ctx.gpr[2] = word(rt, value);
    }
    if (count < tail) {
        (void)call_set(rt, ctx, Row.move_tail, {end - (count << shift), end, end, frame.at(6u)});
        const std::uint32_t step = count << shift;
        std::uint32_t bytes = end - step - position;
        bytes &= ~((1u << shift) - 1u);
        put(rt, vector + 4u, word(rt, vector + 4u) + step);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08B79BACu, {end - bytes, position, bytes});
        (void)call_set(rt, ctx, Row.fill, {position, position + step, temp});
    } else {
        (void)call_set(rt, ctx, Row.fill_uninit, {end, count - tail, temp, frame.at(5u)});
        const std::uint32_t new_end = word(rt, vector + 4u) + ((count - tail) << shift);
        put(rt, vector + 4u, new_end);
        ctx.gpr[2] = (count - tail) << shift;
        (void)call_set(rt, ctx, Row.copy_uninit, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (tail << shift));
        if (shift != 0u) ctx.gpr[2] = tail << shift;  // sll v0, s3, shift: the fill callee leaves v0 as it finds it
        (void)call_set(rt, ctx, Row.fill, {position, end, temp});
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// args_to_vector<Callee>(a0, handle, count, holder): func_089B2014 (Callee func_089B1FCC) and func_089B2324 (Callee func_089B22A4), a script native that gathers `count` arguments into a vector of words {begin, end, capacity} at sp + 0x1C. With count <= 0 nothing happens (v0 untouched). Otherwise
// the object o = *holder (a null one calls func_08A5BB7C(D_08CA50BC) first, every time it is read) answers its virtual slot 0x78: a non-zero answer makes n = the virtual slot 0x7C of o the count, the vector is resized to n words (func_08BF9B6C(vec, begin + 4n) when it is longer, else func_08BFB68C(vec, end, n - size, &0) fills with zero)
// and each word i is the answer of func_089A354C(&tmp, tmp2) after the virtual slot 0x84 of o with (tmp2, o, i) (the temporaries are destroyed with func_08A5B9EC); a zero answer resizes to `count` words the same way and each word i is func_089A354C(&tmp, holder + 4i). Then func_089AB370(&cell, handle), the object cell[0] (null: func_08A5BB7C(D_08CA50C4)) gets Callee(cell[0], &vec), the cell is destroyed, func_08BF8B48(begin, end, sp)
// destroys the elements and the block is freed with func_08A5BC54(begin, ((capacity - begin) >> 2) << 2, 4). v0 = the last call's result, or the capacity in words when there is no block.
// Branches the routes do not take: not registered, see its HOT93 lines (read against the asm only; the callee func_08BF9B6C takes the end of the range as a2, found by reading the callee).
template <std::uint32_t Callee>
void args_to_vector(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[5], count = ctx.gpr[6], holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    if (signed_of(count) <= 0) return;
    const std::uint32_t vector = frame.at(0x1Cu), vector_end = frame.at(0x20u), vector_capacity = frame.at(0x24u);
    put(rt, vector, 0u);
    put(rt, vector_end, 0u);
    put(rt, vector_capacity, 0u);  // the delay slot of the bnez
    const auto object = [&]() {
        const std::uint32_t o = word(rt, holder);
        if (o == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
        return o;
    };
    const auto resize = [&](std::uint32_t wanted, std::uint32_t zero) {
        const std::uint32_t end = word(rt, vector_end), begin = word(rt, vector);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
        put(rt, zero, 0u);
        if (wanted < size) {
            (void)call_set(rt, ctx, 0x08BF9B6Cu, {vector, begin + (wanted << 2), end});
        } else {
            (void)call_set(rt, ctx, 0x08BFB68Cu, {vector, end, wanted - size, zero});
        }
    };
    const std::uint32_t first = object();
    const std::uint32_t kind = call_set(rt, ctx, slot_of(rt, first, 0x78u), {first});
    std::uint32_t cell = frame.at(0xCu);
    if (kind != 0u) {
        const std::uint32_t second = object();
        const std::uint32_t wanted = call_set(rt, ctx, slot_of(rt, second, 0x7Cu), {second});
        resize(wanted, frame.at(8u));
        for (std::uint32_t i = 0u; i != wanted; ++i) {
            const std::uint32_t begin = word(rt, vector);
            const std::uint32_t o = object();
            (void)call_set(rt, ctx, slot_of(rt, o, 0x84u), {frame.at(0x14u), o, i});
            (void)call_set(rt, ctx, 0x089A354Cu, {frame.at(0x18u), frame.at(0x14u)});
            put(rt, begin + (i << 2), word(rt, frame.at(0x18u)));  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x18u)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
        }
    } else {
        resize(count, frame.at(4u));
        std::uint32_t source = holder;
        for (std::uint32_t i = 0u; i != count; ++i) {
            const std::uint32_t slot = word(rt, vector) + (i << 2);
            (void)call_set(rt, ctx, 0x089A354Cu, {frame.at(0x10u), source});
            put(rt, slot, word(rt, frame.at(0x10u)));  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
            source += 4u;
        }
    }
    (void)call_set(rt, ctx, 0x089AB370u, {cell, handle});
    const std::uint32_t target = word(rt, cell);
    if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
    (void)call_set(rt, ctx, Callee, {target, vector});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {cell});
    const std::uint32_t begin = word(rt, vector);
    (void)call_set(rt, ctx, 0x08BF8B48u, {begin, word(rt, vector_end), frame.at(0u)});
    const std::uint32_t block = word(rt, vector);
    const std::uint32_t words = static_cast<std::uint32_t>(signed_of(word(rt, vector_capacity) - block) >> 2);
    ctx.gpr[2] = words;
    if (block != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {block, words << 2, 4u});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// copy_0x92(source, dest): the unrolled copy of 0x92 bytes the original makes (0x90 bytes in blocks of 8 halfword-wise when either address is not word aligned, in blocks of 16 word-wise otherwise, then one more halfword). Each block is read whole before it is written.
inline void copy_0x92(psprecomp::Runtime &rt, std::uint32_t source, std::uint32_t dest) {
    const std::uint32_t end = source + 0x90u;
    if (((source | dest) & 3u) != 0u) {
        while (source != end) {
            const std::uint32_t h0 = half_u(rt, source), h1 = half_u(rt, source + 2u), h2 = half_u(rt, source + 4u), h3 = half_u(rt, source + 6u);
            put16(rt, dest, h0);
            put16(rt, dest + 2u, h1);
            put16(rt, dest + 4u, h2);
            put16(rt, dest + 6u, h3);
            source += 8u;
            dest += 8u;
        }
    } else {
        while (source != end) {
            const std::uint32_t w0 = word(rt, source), w1 = word(rt, source + 4u), w2 = word(rt, source + 8u), w3 = word(rt, source + 0xCu);
            put(rt, dest, w0);
            put(rt, dest + 4u, w1);
            put(rt, dest + 8u, w2);
            put(rt, dest + 0xCu, w3);
            source += 0x10u;
            dest += 0x10u;
        }
    }
    put16(rt, dest, half_u(rt, source));
}

// func_088EFA1C(this, a1, source): this[0xC8] = 8, this[0xC9..0xCB] = 0; the 0x92 bytes at `source` (the table at D_08CDD650 when it is null) are copied to this + 0x24; func_088EF8B0(this + 0xB6); a non-null this[0x20] also calls func_088EF814(this + 0xB6); this[0xC9] = this[0xB7]. With the pair p = this[0xCC] = {begin, count} two
// function objects are built on the stack: {D_088F12E0, 0} at sp + 0x10 and func_08BB8784(begin, begin + 84 * count, sp + 0x10) gives r; with q = this[0xCC] again ({.., .., b, n}) the object at sp is {D_088F0F08, 0, ., ., ., ., r, 12 * count, r, 12 * count} (r and 12 * count at sp + 0x18, 0x1C, 0x20, 0x24) and func_08BB8838(b, b + 8 * n, sp) runs; v0 = its result.
// Branches the routes do not take: not registered, see its HOT93 line (read against the asm only).
[[maybe_unused]] void func_088EFA1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    put8(rt, self + 0xC8u, 8u);
    put8(rt, self + 0xC9u, 0u);
    put8(rt, self + 0xCAu, 0u);
    put8(rt, self + 0xCBu, 0u);
    copy_0x92(rt, source != 0u ? source : 0x08CDD650u, self + 0x24u);
    const std::uint32_t inner = self + 0xB6u;
    (void)call_set(rt, ctx, 0x088EF8B0u, {inner});
    const std::uint32_t flag = word(rt, self + 0x20u);
    ctx.gpr[5] = flag;
    if (flag != 0u) (void)call_set(rt, ctx, 0x088EF814u, {inner});
    put8(rt, self + 0xC9u, byte_u(rt, self + 0xB7u));
    const std::uint32_t pair = word(rt, self + 0xCCu);
    const std::uint32_t count = word(rt, pair + 4u), begin = word(rt, pair);
    const std::uint32_t scaled = (count << 4) - (count << 2);  // 12 * count
    put(rt, frame.at(0x10u), 0x088F12E0u);
    put(rt, frame.at(0x14u), 0u);  // the delay slot of the jal
    const std::uint32_t r = call_set(rt, ctx, 0x08BB8784u, {begin, begin + ((scaled << 3) - scaled), frame.at(0x10u)});
    ctx.gpr[3] = scaled;
    put(rt, frame.at(0x18u), r);
    put(rt, frame.at(0x20u), r);
    const std::uint32_t pair2 = word(rt, self + 0xCCu);
    put(rt, frame.at(0x1Cu), scaled);
    put(rt, frame.at(0x24u), scaled);
    put(rt, frame.at(0u), 0x088F0F08u);
    const std::uint32_t first = word(rt, pair2 + 8u), n = word(rt, pair2 + 0xCu);
    put(rt, frame.at(4u), 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BB8838u, {first, first + (n << 3), frame.at(0u)});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08C30AD0(vector, position, value): insert a copy of the string `value` before `position` into a vector of string pointers {begin, end, capacity} (4-byte elements, the element copy is func_08B88F44(dest, source), the element assignment func_08B855E0(dest, source)). With room (end != capacity): a non-null end gets
// func_08B88F44(end, end - 4), the end grows by 4, a temporary at sp + 4 is made from `value` (func_08B88F44), n = (end' - 8 - position) >> 2 elements are moved up one slot from the last (func_08B855E0(dest, source)), position takes the temporary (func_08B855E0(position, sp + 4)) and the temporary is released (release_cell with a1 = sp).
// Without room: size = (end - begin) >> 2 (0x3FFFFFFF calls func_08B5ADDC(D_08CB2DA0)), bytes = 4 for an empty vector, size * 8 otherwise (-4 when the doubling overflowed), block = func_08A5BC20(bytes, 4), e = func_08C30A6C(begin, position, block, sp + 3), a non-null e gets func_08B88F44(e, value), last = func_08C30A6C(position, end, e + 4, sp + 2),
// func_08C309E8(begin, end, sp + 1) destroys the old elements, the old block is freed with func_08A5BC54(begin, ((capacity - begin) >> 2) * 4, 4) and the vector becomes {block, last, block + bytes}; v0 = block + bytes.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (end != word(rt, vector + 8u))`; `if (end != 0u) (void)call_set(rt, ctx, 0x08B88F44u, {end, end - 4u});`; `if (signed_of(moved) > 0)`; `for (std::uint32_t done = 0u; done != moved; ++done)`; `if (size == 0x3FFFFFFFu) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CB2DA0u});`; `bytes = doubled < size ? 0xFFFFFFFCu : doubled << 2;`; `if (e != 0u) (void)call_set(rt, ctx, 0x08B88F44u, {e, value});`
void func_08C30AD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t end = word(rt, vector + 4u);
    if (end != word(rt, vector + 8u)) {
        if (end != 0u) (void)call_set(rt, ctx, 0x08B88F44u, {end, end - 4u});
        const std::uint32_t grown = word(rt, vector + 4u) + 4u;
        put(rt, vector + 4u, grown);
        const std::uint32_t temp = frame.at(4u);
        (void)call_set(rt, ctx, 0x08B88F44u, {temp, value});
        const std::uint32_t top = word(rt, vector + 4u);
        std::uint32_t source = top - 8u;
        const std::uint32_t moved = static_cast<std::uint32_t>(signed_of(source - position) >> 2);
        if (signed_of(moved) > 0) {
            std::uint32_t dest = top - 4u;
            for (std::uint32_t done = 0u; done != moved; ++done) {
                dest -= 4u;
                source -= 4u;
                (void)call_set(rt, ctx, 0x08B855E0u, {dest, source});
            }
        }
        (void)call_set(rt, ctx, 0x08B855E0u, {position, temp});
        release_cell(rt, ctx, word(rt, temp), frame.at(0u));
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
    if (size == 0x3FFFFFFFu) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CB2DA0u});
    std::uint32_t bytes = 4u;
    if (size != 0u) {
        const std::uint32_t doubled = size << 1;
        bytes = doubled < size ? 0xFFFFFFFCu : doubled << 2;
    }
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    const std::uint32_t e = call_set(rt, ctx, 0x08C30A6Cu, {word(rt, vector), position, block, frame.at(3u)});
    if (e != 0u) (void)call_set(rt, ctx, 0x08B88F44u, {e, value});
    const std::uint32_t last = call_set(rt, ctx, 0x08C30A6Cu, {position, word(rt, vector + 4u), e + 4u, frame.at(2u)});
    (void)call_set(rt, ctx, 0x08C309E8u, {word(rt, vector), word(rt, vector + 4u), frame.at(1u)});
    const std::uint32_t old_begin = word(rt, vector);
    const std::uint32_t words = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - old_begin) >> 2);
    ctx.gpr[2] = words;
    if (old_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_begin, words << 2, 4u});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, last);
    put(rt, vector, block);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08843F8C(this): this registers a script class: a string made from D_08C9C7CC (func_08B85EB8) is handed to the virtual slot 0x108 of this and released; the object func_08A76A94() gives (a non-null one is passed to func_08A5BA08) replaces this[0x38] (a non-null old one is passed to func_08A5B930); func_08B8850C(this, D_08CF6F14, 0x08844338),
// func_08B8A004(this, D_08CF6F24, 0x0884340C), four func_08B87108(this, name, handler) and five func_08B878E8(this, name, getter, setter) register the entries of its tables. v0 = the last call's result.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (made != 0u) (void)call_set(rt, ctx, 0x08A5BA08u, {made});`; `if (old != 0u) (void)call_set(rt, ctx, 0x08A5B930u, {old});`
void func_08843F8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t text = frame.at(4u);
    const std::uint32_t slot = word(rt, word(rt, self) + 0x108u);
    (void)call_set(rt, ctx, 0x08B85EB8u, {text, 0x08C9C7CCu, frame.at(1u)});
    ctx.gpr[4] = self;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {self, text});
    release_cell(rt, ctx, word(rt, text), frame.at(0u));
    const std::uint32_t made = call_set(rt, ctx, 0x08A76A94u, {});
    if (made != 0u) (void)call_set(rt, ctx, 0x08A5BA08u, {made});
    const std::uint32_t old = word(rt, self + 0x38u);
    ctx.gpr[4] = old;
    if (old != 0u) (void)call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, self + 0x38u, made);
    (void)call_set(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08844338u});
    (void)call_set(rt, ctx, 0x08B8A004u, {self, 0x08CF6F24u, 0x0884340Cu});
    (void)call_set(rt, ctx, 0x08B87108u, {self, 0x08C9C820u, 0x08843978u});
    (void)call_set(rt, ctx, 0x08B87108u, {self, 0x08C9C82Cu, 0x08843924u});
    (void)call_set(rt, ctx, 0x08B87108u, {self, 0x08C9C838u, 0x088438D0u});
    (void)call_set(rt, ctx, 0x08B87108u, {self, 0x08C9C850u, 0x08843BD0u});
    (void)call_set(rt, ctx, 0x08B878E8u, {self, 0x08C9C864u, 0x08843D64u, 0x08843D64u});
    (void)call_set(rt, ctx, 0x08B878E8u, {self, 0x08C9C878u, 0x08843DF4u, 0x08843838u});
    (void)call_set(rt, ctx, 0x08B878E8u, {self, 0x08C9C890u, 0x08843E7Cu, 0x088437A0u});
    (void)call_set(rt, ctx, 0x08B878E8u, {self, 0x08C9C8A0u, 0x08843F04u, 0x08843708u});
    (void)call_set(rt, ctx, 0x08B878E8u, {self, 0x08C9C8B4u, 0x08843680u, 0x088435E8u});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// take_vector(vector, destroy_at): the vector {begin, end, capacity} at `vector` is cleared (the three words zero) and its old contents handed on: destroy(begin, end, sp-slot), then a non-null block is freed with func_08A5BC54(begin, ((capacity - begin) >> 2) * 4, 4). The old words are kept at frame[`copy`..+8] like the original does (the destroy call reads the slot address).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (block != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {block, words << 2, 4u});`
inline void clear_vector_words(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t vector, std::uint32_t copy, std::uint32_t slot) {
    const std::uint32_t begin = word(rt, vector), end = word(rt, vector + 4u), capacity = word(rt, vector + 8u);
    put(rt, vector, 0u);
    put(rt, vector + 4u, 0u);
    put(rt, vector + 8u, 0u);
    put(rt, frame.at(copy), begin);
    put(rt, frame.at(copy + 8u), capacity);
    put(rt, frame.at(copy + 4u), end);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08C02614u, {begin, end, slot});
    const std::uint32_t block = word(rt, frame.at(copy));
    const std::uint32_t words = static_cast<std::uint32_t>(signed_of(word(rt, frame.at(copy + 8u)) - block) >> 2);
    ctx.gpr[2] = words;
    if (block != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {block, words << 2, 4u});
}

// func_089D1720(this, a1): func_08994B6C(this); this[0x1A4] = func_089CFCCC(this, this[0x198]); func_089CFE68(this, a1, 0); the vectors at this + 0x160 and this + 0x16C are cleared and released (func_08C02614 destroys their elements, func_08A5BC54 frees the blocks); two temporaries made by func_08969188(&t, D_08CF6ED8)
// go to func_089CDA4C(this, a1, &t) and func_089CDDF8(this, a1, &t) and are destroyed (func_08A5B9EC); then the string slots this[0x17C], this[0x180] and this[0x184] each take a fresh func_08A76AAC(&t, D_08CF6ED8) value (func_08A5BAD8(slot, *t)) and the temporary is destroyed. v0 = the last call's result.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (self + slot != frame.at(temp)) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + slot, word(rt, frame.at(temp))`
void func_089D1720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], extra = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    (void)call_set(rt, ctx, 0x08994B6Cu, {self});
    const std::uint32_t made = call_set(rt, ctx, 0x089CFCCCu, {self, word(rt, self + 0x198u)});
    put(rt, self + 0x1A4u, made);
    (void)call_set(rt, ctx, 0x089CFE68u, {self, extra, 0u});
    clear_vector_words(rt, ctx, frame, self + 0x160u, 0x24u, frame.at(1u));
    clear_vector_words(rt, ctx, frame, self + 0x16Cu, 0x18u, frame.at(0u));
    const std::uint32_t first = frame.at(0x14u), second = frame.at(0x10u);
    (void)call_set(rt, ctx, 0x08969188u, {first, 0x08CF6ED8u});
    (void)call_set(rt, ctx, 0x089CDA4Cu, {self, extra, first});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {first});
    (void)call_set(rt, ctx, 0x08969188u, {second, 0x08CF6ED8u});
    (void)call_set(rt, ctx, 0x089CDDF8u, {self, extra, second});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {second});
    for (const auto [slot, temp] : {std::pair<std::uint32_t, std::uint32_t>{0x17Cu, 0xCu}, {0x180u, 8u}, {0x184u, 4u}}) {
        (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(temp), 0x08CF6ED8u});
        if (self + slot != frame.at(temp)) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + slot, word(rt, frame.at(temp))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(temp)});
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08AD8094(this): n = max(this[0x2C], 200) is looked up in the `count` = this[0x28] records of 16 bytes at this[0x24] (halfwords r[2] and r[4] are a range): the first record whose range does not lie below n sets this[9] = its index and this[0x19] = -1, or, when the previous record's upper bound is below n and n is below this record's lower
// bound, this[9] = index - 1 and this[0x19] = index (the index counts the records passed). Then the pair this[9] / this[0x19] is exchanged with this[0x18] / this[8] (bytes) as the original does, and for the two 16-byte entries at this + 4 (byte e[4] = old, e[5] = new): e[4] = new; a new value >= 0 asks the virtual slot 0x18 (this, old, new): a non-zero answer
// runs func_08AD800C(this, i, n, 1) and the virtual slot 0x20 (this, i); otherwise the virtual slot 0x28 (this), func_08AD800C(this, i, n, 0), the slot 0x1C (this, e, new) and the slot 0x24 (this, i) run; a new value < 0 with an old one >= 0 runs the slot 0x28 (this, i). v0 = the last call's result, or the old byte of the last entry.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(limit) < 0xC8) limit = 0xC8u;`; `if (signed_of(count) > 0)`; `} else if (!(signed_of(sext16(half_u(rt, previous + 4u))) < signed_of(limit)))`; `if (index == count) break;`; `} else if (sext8(spare) == sext8(byte_u(rt, self + 8u)))`
void func_08AD8094(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t limit = word(rt, self + 0x2Cu);
    const std::uint32_t count = word(rt, self + 0x28u);
    std::uint32_t record = word(rt, self + 0x24u);
    if (signed_of(limit) < 0xC8) limit = 0xC8u;  // the movn in the delay slot of the blez
    if (signed_of(count) > 0) {
        std::uint32_t previous = 0u, index = 0u;
        for (;;) {
            const std::uint32_t below = signed_of(limit) < signed_of(sext16(half_u(rt, record + 2u))) ? 1u : 0u;  // a1
            if (below == 0u && !(signed_of(sext16(half_u(rt, record + 4u))) < signed_of(limit))) {
                put8(rt, self + 9u, index);
                put8(rt, self + 0x19u, 0xFFFFFFFFu);
                break;
            }
            if (previous == 0u) {
                ++index;
            } else if (!(signed_of(sext16(half_u(rt, previous + 4u))) < signed_of(limit))) {
                ++index;
            } else if (below != 0u) {
                index = sext8(index);
                put8(rt, self + 9u, index - 1u);
                put8(rt, self + 0x19u, index);
                break;
            } else {
                ++index;
            }
            previous = record;
            record += 0x10u;
            if (index == count) break;
        }
    }
    const std::uint32_t current = sext8(byte_u(rt, self + 9u));
    const std::uint32_t spare = byte_u(rt, self + 0x19u);  // a0, the delay slot of the beq
    if (sext8(byte_u(rt, self + 0x18u)) == current) {
        put8(rt, self + 9u, spare);
        put8(rt, self + 0x19u, current);
    } else if (sext8(spare) == sext8(byte_u(rt, self + 8u))) {
        put8(rt, self + 9u, spare);
        put8(rt, self + 0x19u, current);
    }
    std::uint32_t i = 0u;
    std::uint32_t entry = self + 4u;
    for (;;) {
        const std::uint32_t fresh = sext8(byte_u(rt, entry + 5u));
        const std::uint32_t old = sext8(byte_u(rt, entry + 4u));
        ctx.gpr[2] = old;
        put8(rt, entry + 4u, fresh);
        if (signed_of(fresh) >= 0) {
            const std::uint32_t known = call_set(rt, ctx, slot_of(rt, self, 0x18u), {self, old, fresh});
            if (known != 0u) {
                (void)call_set(rt, ctx, 0x08AD800Cu, {self, i, limit, 1u});
                (void)call_set(rt, ctx, slot_of(rt, self, 0x20u), {self, i});
            } else {
                (void)call_set(rt, ctx, slot_of(rt, self, 0x28u), {self, i, limit, 1u});
                (void)call_set(rt, ctx, 0x08AD800Cu, {self, i, limit, 0u});
                (void)call_set(rt, ctx, slot_of(rt, self, 0x1Cu), {self, entry, fresh});
                (void)call_set(rt, ctx, slot_of(rt, self, 0x24u), {self, i});
            }
        } else if (signed_of(old) >= 0) {
            ctx.gpr[5] = i;  // the delay slot of the bltz
            (void)call_set(rt, ctx, slot_of(rt, self, 0x28u), {self, i});
        }
        ++i;
        entry += 0x10u;
        if (i == 2u) break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08B8F2B4(out, a1, a2, a3, t0): out = {object, counter block}. func_08B8F080(a2) == 0 stores {0, 0}. Otherwise the object o = func_08B62344(0x48) is built by func_0889F1DC(o, a2, a3, t0, 0, 0) and its counter block c = func_08B62344(0x10) is filled ({D_08CBF458, 1, 1, o}); func_08B8E380(&c', o, o) with c' = c at sp + 0xC;
// a copy of the pair at sp is made (one more reference at c + 4), func_0889E258(o, copy) runs and the copy is released (release_counted); out = {o, c'} with one more reference at c' + 4, and c' is released once. v0 = out on every path.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (accepted == 0u)`; `if (counted != 0u) atomic_increment(rt, ctx, counted + 4u);`; `if (copy != 0u) release_counted(rt, ctx, copy);`; `if (kept != 0u)`; `if (again != 0u) release_counted(rt, ctx, again);`
void func_08B8F2B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], arg2 = ctx.gpr[6], arg3 = ctx.gpr[7], arg4 = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t accepted = call_set(rt, ctx, 0x08B8F080u, {arg2});
    if (accepted == 0u) {
        put(rt, out, 0u);  // the delay slot of the beql
        put(rt, out + 4u, 0u);
        ctx.gpr[2] = out;
        return;
    }
    const std::uint32_t raw = call_set(rt, ctx, 0x08B62344u, {0x48u});
    (void)call_set(rt, ctx, 0x0889F1DCu, {raw, arg2, arg3, arg4, 0u, 0u});
    put(rt, frame.at(8u), raw);  // the delay slot of the next jal
    const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, block + 4u, 1u);
    put(rt, block, 0x08CBF458u);
    put(rt, block + 8u, 1u);
    put(rt, block + 0xCu, raw);
    put(rt, frame.at(0xCu), block);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B8E380u, {frame.at(0xCu), raw, raw});
    const std::uint32_t object = word(rt, frame.at(8u)), counted = word(rt, frame.at(0xCu));
    put(rt, frame.at(0u), object);
    put(rt, frame.at(4u), counted);  // the delay slot of the beqz
    if (counted != 0u) atomic_increment(rt, ctx, counted + 4u);
    (void)call_set(rt, ctx, 0x0889E258u, {object, frame.at(0u)});
    const std::uint32_t copy = word(rt, frame.at(4u));
    if (copy != 0u) release_counted(rt, ctx, copy);
    const std::uint32_t kept = word(rt, frame.at(0xCu));
    put(rt, out + 4u, kept);
    put(rt, out, word(rt, frame.at(8u)));  // the delay slot of the beqz
    if (kept != 0u) {
        atomic_increment(rt, ctx, kept + 4u);
        const std::uint32_t again = word(rt, frame.at(0xCu));
        if (again != 0u) release_counted(rt, ctx, again);
    }
    ctx.gpr[2] = out;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_0886C12C(this, index, object): with d = this[4]: d[0x10 + 4 * index] = object; an old child d[0x1C + 4 * index] gets its virtual slot 0x2C called and is cleared. A non-null object with index 0 or 1 becomes the child d[0x1C] / d[0x20] (the answer of its virtual slot 0x18), which is then set up: slot 8 of the child
// with func_0887BD94(2 or 1), slot 0x14 (child, this), slot 0x18 (child, D_08B93AA0 / D_08B93AA8) and slot 0x28 (child). Then every listener of the list d[4] (each node {listener, ., next}) gets its virtual slot 8 called with (listener, index, object) - not for a null object or an index above 1, whose list is walked
// from d[4] straight away. v0 = the last call's answer, or d + 4 * index / 1 where no call ran after those loads.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (child != 0u)`; `} else`; `} else if (index == 1u)`
void func_0886C12C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5], object = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    put(rt, word(rt, self + 4u) + (index << 2) + 0x10u, object);
    std::uint32_t holder = word(rt, self + 4u);
    ctx.gpr[2] = holder + (index << 2);
    const std::uint32_t old = word(rt, holder + (index << 2) + 0x1Cu);
    if (old != 0u) {
        (void)call_set(rt, ctx, slot_of(rt, old, 0x2Cu), {old});
        ctx.gpr[2] = word(rt, self + 4u) + (index << 2);
        put(rt, ctx.gpr[2] + 0x1Cu, 0u);
        holder = word(rt, self + 4u);
    }
    std::uint32_t head = 0u;  // s0 when the listener loop starts
    const auto bind = [&](std::uint32_t field, std::uint32_t kind, std::uint32_t constant) {
        const std::uint32_t made = call_set(rt, ctx, slot_of(rt, object, 0x18u), {object});
        put(rt, holder + field, made);
        const std::uint32_t current = word(rt, self + 4u);
        const std::uint32_t child = word(rt, current + field);
        if (child != 0u) {
            const std::uint32_t method = word(rt, word(rt, child) + 8u);  // s0, the delay slot of the jal
            const std::uint32_t code = call_set(rt, ctx, 0x0887BD94u, {kind});
            (void)call_set(rt, ctx, method, {child, code});
            const std::uint32_t second = word(rt, word(rt, self + 4u) + field);
            (void)call_set(rt, ctx, slot_of(rt, second, 0x14u), {second, self});
            const std::uint32_t third = word(rt, word(rt, self + 4u) + field);
            (void)call_set(rt, ctx, slot_of(rt, third, 0x18u), {third, constant});
            const std::uint32_t fourth = word(rt, word(rt, self + 4u) + field);
            (void)call_set(rt, ctx, slot_of(rt, fourth, 0x28u), {fourth});
            head = word(rt, word(rt, self + 4u) + 4u);
        } else {
            head = word(rt, current + 4u);
        }
    };
    if (object == 0u) {
        head = word(rt, holder + 4u);
    } else if (index == 0u) {
        bind(0x1Cu, 2u, 0x08B93AA0u);
    } else if (index == 1u) {
        bind(0x20u, 1u, 0x08B93AA8u);
    } else {
        ctx.gpr[2] = 1u;
        head = word(rt, holder + 4u);
    }
    while (head != 0u) {
        const std::uint32_t listener = word(rt, head);
        (void)call_set(rt, ctx, slot_of(rt, listener, 8u), {listener, index, object});
        head = word(rt, head + 8u);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08896780(this, state, extra): with this[0x6C] non-zero and this[0xA8] set, a set byte this[0xA9] is cleared when func_08895544(this + 0x70) answers 0 (mode 0); with this[0xA8] clear, this[0xC] clear picks func_0892BB10() -> func_0892B4B0(state, ..), func_0892B440(state) and state[0xE] = 1 (mode 0); this[0xC] set and this[0xB] set the same with state[0xA] = 1
// (mode 0); this[0xC] set and this[0xB] clear only func_0892B4B0(state, func_0892BB10()) (mode 1); then a set this[0xA9] with func_08895A88(this + 0x70, state) == 0 clears it, func_08894E8C(this + 0x70, this[0xAC]) runs and this[0xB0] = this[0xAC]. func_0889608C(this, state) follows; this[0xFC] is -1 unless the mode is 1: then func_08896228(this) == 0 sets
// it to 0 and otherwise increments it. With this[0x60] and this[0x6C] non-zero and this[0xB] clear, the 8 entries of func_0892B968() (when its [4] is non-zero) each ask func_0892B428(o, i) and call the virtual slot 0xC4 of this[0x60] with (extra, i) for those answered. v0 = 1.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (word(rt, self + 0x6Cu) != 0u)`; `if (byte_u(rt, self + 0xA8u) != 0u)`; `if (byte_u(rt, self + 0xA9u) != 0u)`; `if (call_set(rt, ctx, 0x08895544u, {sink}) == 0u) put8(rt, self + 0xA9u, 0u);`; `if (byte_u(rt, self + 0xCu) == 0u || byte_u(rt, self + 0xBu) != 0u)`; `put16(rt, state + (byte_u(rt, self + 0xCu) == 0u ? 0xEu : 0xAu), 1u);`; `if (byte_u(rt, self + 0xA9u) != 0u && call_set(rt, ctx, 0x08895A88u, {sink, state}) == 0u)`; `if (mode != 1u)`; `} else if (call_set(rt, ctx, 0x08896228u, {self}) == 0u)`; `if (word(rt, self + 0x60u) != 0u && word(rt, self + 0x6Cu) != 0u && byte_u(rt, self + 0xBu) == 0u)`; `if (word(rt, table + 4u) != 0u)`; `for (std::uint32_t i = 0u; i != 8u;)`; `if (asked != 0u)`
void func_08896780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], state = ctx.gpr[5], extra = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t mode = 0u;  // s3
    const std::uint32_t sink = self + 0x70u;
    if (word(rt, self + 0x6Cu) != 0u) {
        if (byte_u(rt, self + 0xA8u) != 0u) {
            if (byte_u(rt, self + 0xA9u) != 0u) {
                if (call_set(rt, ctx, 0x08895544u, {sink}) == 0u) put8(rt, self + 0xA9u, 0u);
            }
        } else {
            if (byte_u(rt, self + 0xCu) == 0u || byte_u(rt, self + 0xBu) != 0u) {
                const std::uint32_t level = call_set(rt, ctx, 0x0892BB10u, {word(rt, self + 0x6Cu)});
                (void)call_set(rt, ctx, 0x0892B4B0u, {state, level});
                (void)call_set(rt, ctx, 0x0892B440u, {state});
                put16(rt, state + (byte_u(rt, self + 0xCu) == 0u ? 0xEu : 0xAu), 1u);
            } else {
                mode = 1u;
                const std::uint32_t level = call_set(rt, ctx, 0x0892BB10u, {word(rt, self + 0x6Cu)});
                (void)call_set(rt, ctx, 0x0892B4B0u, {state, level});
            }
            if (byte_u(rt, self + 0xA9u) != 0u && call_set(rt, ctx, 0x08895A88u, {sink, state}) == 0u) {
                put8(rt, self + 0xA9u, 0u);
                (void)call_set(rt, ctx, 0x08894E8Cu, {sink, word(rt, self + 0xACu)});
                put(rt, self + 0xB0u, word(rt, self + 0xACu));  // the delay slot of the j
            }
        }
    }
    (void)call_set(rt, ctx, 0x0889608Cu, {self, state});
    if (mode != 1u) {
        put(rt, self + 0xFCu, 0xFFFFFFFFu);
    } else if (call_set(rt, ctx, 0x08896228u, {self}) == 0u) {
        put(rt, self + 0xFCu, 0u);  // the delay slot of the beql
    } else {
        put(rt, self + 0xFCu, word(rt, self + 0xFCu) + 1u);
    }
    if (word(rt, self + 0x60u) != 0u && word(rt, self + 0x6Cu) != 0u && byte_u(rt, self + 0xBu) == 0u) {
        const std::uint32_t table = call_set(rt, ctx, 0x0892B968u, {word(rt, self + 0x6Cu)});
        if (word(rt, table + 4u) != 0u) {
            for (std::uint32_t i = 0u; i != 8u;) {
                const std::uint32_t asked = call_set(rt, ctx, 0x0892B428u, {table, i});
                const std::uint32_t entry = i;
                ++i;  // the delay slot of the beqz
                if (asked != 0u) {
                    const std::uint32_t owner = word(rt, self + 0x60u);
                    (void)call_set_v0(rt, ctx, slot_of(rt, owner, 0xC4u), {owner, extra, entry});
                }
            }
        }
    }
    ctx.gpr[2] = 1u;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08C1DD98(this): a string-like object t at sp + 0x10 is made by func_08A98C1C(t, 2); with the owner o = this[0x14] and the parts p = this[8] and q = o[0x14] (a null one calls func_08A5BB7C with its own message, then the original reads through it) the position s6 = p[0x1C] + q[0x10] (or p[0x18][0xC] + q[0x10] when
// p[0x1C] is zero) is computed; a new counted node n (func_08A5BC20(0x18, 4), func_08A76ACC, n[0] = D_08CB6B88, func_08A6972C(n + 0x14, o + 0x10)) is stored through func_08A76AB4 into the slot s2 = t[0x14] with func_08A5BAD8 (when s2 differs from the temporary at sp + 0xC), the temporary is destroyed, func_08A57B54(sp + 4, o[0x14] + 0xC, this[8] + 0x18, s6)
// and func_08A56884(sp + 8, [sp + 4]) follow and s2 = t[0x14] + 4 takes sp[8]; the temporaries are destroyed; this + 4 takes t unless they are the same cell; this[0x14] = func_08B5F16C(this[0x14]); t is destroyed. v0 = the result of the last func_08A5B9EC.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {message});`; `if (slot != temp_a) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp_a)});`; `if (second_slot != temp_c) (void)call_set(rt, ctx, 0x08A5BAD8u, {second_slot, word(rt, temp_c)});`; `if (self + 4u != text) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + 4u, word(rt, text)});`
void func_08C1DD98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t text = frame.at(0x10u);
    (void)call_set(rt, ctx, 0x08A98C1Cu, {text, 2u});
    const std::uint32_t owner = word(rt, self + 0x14u);
    std::uint32_t part = word(rt, self + 8u);  // s1
    const std::uint32_t owner_field = owner + 0x14u;  // s5
    const auto check = [&](std::uint32_t value, std::uint32_t message) {
        if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {message});
    };
    check(part, 0x08CAF5E0u);
    std::uint32_t inner = word(rt, owner + 0x14u);  // s0
    check(inner, 0x08CAF594u);
    inner = word(rt, inner + 0x10u);
    std::uint32_t base = word(rt, part + 0x1Cu);  // v0
    std::uint32_t position = base + inner;        // s6
    if (base == 0u) {
        part = word(rt, part + 0x18u);
        check(part, 0x08CAF540u);
        base = word(rt, part + 0xCu);
        position = base + inner;
    }
    const std::uint32_t slot_owner = word(rt, self + 0x14u);  // s1
    const std::uint32_t slot = word(rt, word(rt, text) + 0x14u);  // s2, the delay slot of the jal
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    (void)call_set(rt, ctx, 0x08A76ACCu, {block});
    put(rt, block, 0x08CB6B88u);  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A6972Cu, {block + 0x14u, slot_owner + 0x10u});
    const std::uint32_t temp_a = frame.at(0xCu);
    put(rt, frame.at(0u), block);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A76AB4u, {temp_a, frame.at(0u)});
    if (slot != temp_a) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp_a)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {temp_a});
    const std::uint32_t current = word(rt, text);  // v0
    const std::uint32_t sink_owner = word(rt, owner_field);  // s1
    const std::uint32_t second_slot = word(rt, current + 0x14u) + 4u;  // s2
    check(sink_owner, 0x08CAF594u);
    const std::uint32_t head = word(rt, self + 8u);  // s0
    check(head, 0x08CAF5E0u);
    const std::uint32_t temp_b = frame.at(4u), temp_c = frame.at(8u);
    (void)call_set(rt, ctx, 0x08A57B54u, {temp_b, sink_owner + 0xCu, head + 0x18u, position});
    const std::uint32_t result = word(rt, temp_b);
    check(result, 0x08CAF5E0u);
    (void)call_set(rt, ctx, 0x08A56884u, {temp_c, result});
    if (second_slot != temp_c) (void)call_set(rt, ctx, 0x08A5BAD8u, {second_slot, word(rt, temp_c)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {temp_c});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {temp_b});
    if (self + 4u != text) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + 4u, word(rt, text)});
    const std::uint32_t renewed = call_set(rt, ctx, 0x08B5F16Cu, {word(rt, self + 0x14u)});
    put(rt, self + 0x14u, renewed);
    (void)call_set(rt, ctx, 0x08A5B9ECu, {text});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_088F1DA0(this, a1, source): without a source whose virtual slots 0x68 and 0x6C both answer non-zero, func_088F1C98(this) is the result. Otherwise func_088F1C40(this) runs and, under the lock at this[8] + 4 (func_08B344E0 / func_08B3457C), i = 1 - this[8][0x2C] picks the buffer b = this[8] + 0x408 * i + 0x30: when the virtual slot 0x2C of the source answers non-zero,
// the object r it gives (slot 0x2C of the source again) has a length n (its slot 8); with the word count b[0x404] + ((n + 3) >> 2) not above 0x100 its slot 0xC gives the bytes that func_08B79A88(b + 4 + 4 * count, bytes, n) copies to the buffer and the count grows. Unless the source's virtual slot 0x7C answers non-zero, two more words {0x00010002, 0x3C888889} are appended at b[0x404] when the count + 2 is not above 0x100.
// v0 = the unlock's result (the result of func_088F1C98 on the first path).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (source != 0u)`; `if (usable) usable = call_set(rt, ctx, slot_of(rt, source, 0x6Cu), {source}) != 0u;`; `if (!usable)`; `if (answered != 0u)`; `if (grown < 0x101u)`; `if (more == 0u)`; `if (used + 2u < 0x101u)`
void func_088F1DA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    bool usable = false;
    if (source != 0u) {
        usable = call_set_v0(rt, ctx, slot_of(rt, source, 0x68u), {source}) != 0u;
        if (usable) usable = call_set_v0(rt, ctx, slot_of(rt, source, 0x6Cu), {source}) != 0u;
    }
    if (!usable) {
        (void)call_set(rt, ctx, 0x088F1C98u, {self});
        return;
    }
    (void)call_set(rt, ctx, 0x088F1C40u, {self});
    const std::uint32_t lock = word(rt, self + 8u) + 4u;  // s6
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    const std::uint32_t index = 1u - word(rt, word(rt, self + 8u) + 0x2Cu);  // s2, the delay slot of the jalr
    const std::uint32_t buffer_offset = (index << 3) + (index << 10) + 0x30u;
    const std::uint32_t answered = call_set_v0(rt, ctx, slot_of(rt, source, 0x2Cu), {source});
    if (answered != 0u) {
        const std::uint32_t buffer = word(rt, self + 8u) + buffer_offset;  // s3, the delay slot of the jalr
        const std::uint32_t object = call_set_v0(rt, ctx, slot_of(rt, source, 0x2Cu), {source});
        const std::uint32_t length = call_set_v0(rt, ctx, slot_of(rt, object, 8u), {object});
        const std::uint32_t count = word(rt, buffer + 0x404u);
        const std::uint32_t grown = count + ((length + 3u) >> 2);
        if (grown < 0x101u) {
            const std::uint32_t bytes = call_set_v0(rt, ctx, slot_of(rt, object, 0xCu), {object});
            (void)call_set(rt, ctx, 0x08B79A88u, {buffer + (word(rt, buffer + 0x404u) << 2) + 4u, bytes, length});
            put(rt, buffer + 0x404u, grown);
        }
    }
    const std::uint32_t more = call_set_v0(rt, ctx, slot_of(rt, source, 0x7Cu), {source});
    if (more == 0u) {
        const std::uint32_t b = word(rt, self + 8u) + buffer_offset;
        ctx.fpr[0] = std::bit_cast<float>(0x3C888889u);
        const std::uint32_t used = word(rt, b + 0x404u);
        if (used + 2u < 0x101u) {
            put(rt, b + (used << 2) + 4u, 0x00010002u);
            put(rt, b + (used << 2) + 8u, 0x3C888889u);
            put(rt, b + 0x404u, used + 2u);
        }
    }
    (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_0891A088(this): a constructor. func_08926D60(this); this[0] = D_08CC5AE8; this[0x1F8] = this[0x1FC] = this[0x200] = 0; four sub-objects at this + 0x20C, 0x238, 0x268 and 0x294 are built by func_089263EC and take their vtable words (D_08CC6198 / D_08CC6150) in the delay slots; func_08926398 builds the six at this + 0x2C4, 0x2F0, 0x31C, 0x348, 0x374 and 0x3A0 and func_089263EC the one at
// this + 0x3CC (this[0x3CC] = D_08CC61E0); func_08926514(sub, D_08CDDB38, 1) for the first two, func_08926488(sub, name, 1) for the other seven (names D_08CDDB68, D_08CDDB50, D_08CDDB98 twice, D_08CDDB80 twice and D_08CDDB20); the byte at D_08CF212C is cleared in the delay slot of the first func_08926514; this[0x3F8] = 1.0f, this[0x3FC] = 1, this[0x400] = this[0x404] = 0,
// this[0x408] = 0, this[0x40C] = 0, this[0x410] = the word at D_08C8A87C; o = func_0891C35C(func_08B62344(0xF0)) is stored in this[0x204], this[0x208] = 0, this[0x414] = 0.1f. v0 = func_0891C35C's result.
void func_0891A088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    (void)call_set(rt, ctx, 0x08926D60u, {self});
    put(rt, self, 0x08CC5AE8u);
    put(rt, self + 0x1F8u, 0u);
    put(rt, self + 0x1FCu, 0u);
    put(rt, self + 0x200u, 0u);  // the delay slot of the first func_089263EC
    (void)call_set(rt, ctx, 0x089263ECu, {self + 0x20Cu});
    put(rt, self + 0x20Cu, 0x08CC6198u);
    (void)call_set(rt, ctx, 0x089263ECu, {self + 0x238u});
    put(rt, self + 0x238u, 0x08CC6150u);
    (void)call_set(rt, ctx, 0x089263ECu, {self + 0x268u});
    put(rt, self + 0x268u, 0x08CC6198u);
    (void)call_set(rt, ctx, 0x089263ECu, {self + 0x294u});
    put(rt, self + 0x294u, 0x08CC6150u);
    for (const std::uint32_t offset : {0x2C4u, 0x2F0u, 0x31Cu, 0x348u, 0x374u, 0x3A0u}) (void)call_set(rt, ctx, 0x08926398u, {self + offset});
    (void)call_set(rt, ctx, 0x089263ECu, {self + 0x3CCu});
    put(rt, self + 0x3CCu, 0x08CC61E0u);
    put8(rt, 0x08CF212Cu, 0u);  // the delay slot of the first func_08926514
    (void)call_set(rt, ctx, 0x08926514u, {self + 0x20Cu, 0x08CDDB38u, 1u});
    (void)call_set(rt, ctx, 0x08926514u, {self + 0x268u, 0x08CDDB38u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x2C4u, 0x08CDDB68u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x2F0u, 0x08CDDB50u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x31Cu, 0x08CDDB98u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x348u, 0x08CDDB98u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x374u, 0x08CDDB80u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x3A0u, 0x08CDDB80u, 1u});
    (void)call_set(rt, ctx, 0x08926488u, {self + 0x3CCu, 0x08CDDB20u, 1u});
    ctx.fpr[0] = std::bit_cast<float>(0x3F800000u);
    put(rt, self + 0x3F8u, 0x3F800000u);
    put(rt, self + 0x410u, word(rt, 0x08C8A87Cu));
    put8(rt, self + 0x3FCu, 1u);
    put(rt, self + 0x400u, 0u);
    put(rt, self + 0x404u, 0u);
    put8(rt, self + 0x408u, 0u);
    put(rt, self + 0x40Cu, 0u);  // the delay slot of the jal
    const std::uint32_t raw = call_set(rt, ctx, 0x08B62344u, {0xF0u});
    (void)call_set(rt, ctx, 0x0891C35Cu, {raw});
    ctx.fpr[0] = std::bit_cast<float>(0x3DCCCCCDu);
    put(rt, self + 0x414u, 0x3DCCCCCDu);
    put(rt, self + 0x204u, raw);
    put(rt, self + 0x208u, 0u);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08984824(this): a constructor. this[0] = D_08CCE158; three color sub-objects at this + 0x10, 0x50 and 0x80 are built from temporaries (func_08964224(t, color) then func_0896418C(sub, t) with the colors 0xFFD4D4D4, 0xFF000000 and 0xFF000000; sub[0x10] = -1 after each copy); func_08B85EB8(this + 0x30, D_08CA38CC, sp + 2) makes a string; the fields this[0x34..0x39],
// this[0x3C], this[0x40], this[0x44], this[0x48], this[0x70], this[0xA0..0xC8] and this[0xCC..0xE0] take the constants the code stores (1.0f at 0x40, 0x44, 0xBC and 0xC0, 4 at 0x3C, -1 at 0xC8 and 0xE0); two blocks of 0x20 bytes (func_08A5BC20(0x20, 4)) are filled with func_08BEF0EC(block, 8, &0, sp + 1 / sp) and become {block, block + 0x20, block + 0x20} at this + 0xCC / 0xE4.
// $f20 is restored. v0 = this[0xE4] + 0x20.
void func_08984824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    SavedFloat saved_f20(ctx, 20u);
    const float one = std::bit_cast<float>(0x3F800000u);
    ctx.fpr[20] = one;
    const std::uint32_t first_sub = self + 0x10u, second_sub = self + 0x50u, third_sub = self + 0x80u, left = self + 0xCCu, right = self + 0xE4u;
    put(rt, self, 0x08CCE158u);
    (void)call_set(rt, ctx, 0x08964224u, {frame.at(0x30u), 0xFFD4D4D4u});
    (void)call_set(rt, ctx, 0x0896418Cu, {first_sub, frame.at(0x30u)});
    put(rt, first_sub + 0x10u, 0xFFFFFFFFu);
    (void)call_set(rt, ctx, 0x08B85EB8u, {self + 0x30u, 0x08CA38CCu, frame.at(2u)});
    put(rt, self + 0x3Cu, 4u);
    put8(rt, self + 0x34u, 1u);
    put8(rt, self + 0x35u, 1u);
    put8(rt, self + 0x38u, 1u);
    put_float(rt, self + 0x40u, one);
    put_float(rt, self + 0x44u, one);
    put8(rt, self + 0x36u, 0u);
    put8(rt, self + 0x37u, 0u);
    put8(rt, self + 0x39u, 0u);
    put(rt, self + 0x48u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08964224u, {frame.at(0x20u), 0xFF000000u});
    (void)call_set(rt, ctx, 0x0896418Cu, {second_sub, frame.at(0x20u)});
    put(rt, second_sub + 0x10u, 0xFFFFFFFFu);
    put(rt, self + 0x70u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08964224u, {frame.at(0x10u), 0xFF000000u});
    (void)call_set(rt, ctx, 0x0896418Cu, {third_sub, frame.at(0x10u)});
    put(rt, third_sub + 0x10u, 0xFFFFFFFFu);
    put8(rt, self + 0xA9u, 1u);
    put_float(rt, self + 0xC0u, one);
    put8(rt, self + 0xA8u, 1u);
    put_float(rt, self + 0xBCu, one);
    put(rt, self + 0xC8u, 0xFFFFFFFFu);
    for (const std::uint32_t offset : {0xA0u, 0xA4u, 0xACu, 0xB0u, 0xB4u, 0xB8u}) put(rt, self + offset, 0u);
    put8(rt, self + 0xC4u, 0u);
    put(rt, left, 0u);
    put(rt, left + 4u, 0u);
    put(rt, left + 8u, 0u);
    put(rt, frame.at(8u), 0u);  // the delay slot of the jal
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    put(rt, left + 8u, block + 0x20u);
    put(rt, left, block);
    put(rt, left + 4u, block);  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08BEF0ECu, {block, 8u, frame.at(8u), frame.at(1u)});
    put(rt, right, 0u);
    put(rt, left + 4u, word(rt, left) + 0x20u);
    put(rt, self + 0xDCu, 0u);
    put(rt, self + 0xE0u, 0xFFFFFFFFu);
    put(rt, self + 0xD8u, 0u);
    put(rt, right + 4u, 0u);
    put(rt, right + 8u, 0u);
    put(rt, frame.at(4u), 0u);  // the delay slot of the jal
    const std::uint32_t second = call_set(rt, ctx, 0x08A5BC20u, {0x20u, 4u});
    put(rt, right, second);
    put(rt, right + 4u, second);
    put(rt, right + 8u, second + 0x20u);  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08BEF0ECu, {second, 8u, frame.at(4u), frame.at(0u)});
    ctx.gpr[2] = word(rt, right) + 0x20u;
    put(rt, right + 4u, ctx.gpr[2]);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_089B0EAC(this, a1): a constructor. func_08A76ACC(this); this[0] = D_08CBB598; this[0x14] = a1; the fields take the zeros, -1 (this[0x5C], this[0xB4]), 1 (the bytes this[0x60], 0x6E, 0xF8, 0xF9, 0x120, 0x138), 5 (this[0x70]), 2 (this[0x124]), floats (this[0xA8] / this[0xAC] = D_08CDE764 / D_08CDE760, this[0xD8] = this[0xDC] = 0.5f, this[0xE0] = -22.5f, this[0xE4] / this[0xE8] = those globals times 0.5f, this[0x110] = 0.0166667f,
// this[0x160] and the three words after it = 1.0f); this[0x17C] = this + 0x178 (and this[0x178] too); func_089A7644(this + 0x68, 0) and func_089B4108(this + 0xB8, ..) run in between, this[0x58] and this[0xB0] take two blocks from func_08B35840(0x10, 0x400) / func_08B35840(0x10, 0x1000). $f0-$f3 end as the original leaves them. v0 = the second block.
void func_089B0EAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CBB598u);
    put(rt, self + 0x14u, argument);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x5Cu, 0xFFFFFFFFu);
    put8(rt, self + 0x60u, 1u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x30u, 0u);
    put8(rt, self + 0x3Cu, 0u);
    for (const std::uint32_t offset : {0x40u, 0x44u, 0x48u, 0x4Cu, 0x50u, 0x54u, 0x58u}) put(rt, self + offset, 0u);
    put(rt, self + 0x64u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x089A7644u, {self + 0x68u, 0u});
    put(rt, self + 0x70u, 5u);
    put8(rt, self + 0x6Eu, 1u);
    put8(rt, self + 0x6Cu, 0u);
    put8(rt, self + 0x6Du, 0u);
    put(rt, self + 0x74u, 0u);
    put(rt, self + 0x7Cu, 0u);
    put(rt, self + 0x78u, 0u);
    put(rt, self + 0x88u, 0u);
    put(rt, self + 0x84u, 0u);
    put(rt, self + 0x94u, 0u);
    put(rt, self + 0x90u, 0u);
    put(rt, self + 0x80u, 0u);
    put(rt, self + 0x8Cu, 0u);
    const float first_constant = float_at(rt, 0x08CDE764u), second_constant = float_at(rt, 0x08CDE760u);
    put(rt, self + 0xA4u, 0u);
    put_float(rt, self + 0xACu, second_constant);
    put_float(rt, self + 0xA8u, first_constant);
    put(rt, self + 0xA0u, 0u);
    put(rt, self + 0xB4u, 0xFFFFFFFFu);
    put(rt, self + 0xB0u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x089B4108u, {self + 0xB8u, self + 0x74u, self + 0x80u, self + 0x8Cu});
    const float half = std::bit_cast<float>(0x3F000000u), one = std::bit_cast<float>(0x3F800000u);
    put(rt, self + 0xBCu, 0u);
    put(rt, self + 0xC0u, 0u);
    put(rt, self + 0xC4u, 0u);
    put(rt, self + 0xD4u, 0u);
    put(rt, self + 0xCCu, 0u);
    put(rt, self + 0xD0u, 0u);
    put_float(rt, self + 0xD8u, half);
    put_float(rt, self + 0xDCu, half);
    put_float(rt, self + 0xE0u, std::bit_cast<float>(0xC1B40000u));
    put(rt, self + 0xC8u, 0u);
    put_float(rt, self + 0xE4u, psp_mul(float_at(rt, 0x08CDE764u), half));
    put(rt, self + 0x110u, 0x3C888889u);
    put_float(rt, self + 0xE8u, psp_mul(float_at(rt, 0x08CDE760u), half));
    put(rt, self + 0x108u, 0u);
    put(rt, self + 0x10Cu, 0u);
    put(rt, self + 0x124u, 2u);
    put8(rt, self + 0xF8u, 1u);
    put8(rt, self + 0xF9u, 1u);
    put8(rt, self + 0x120u, 1u);
    put(rt, self + 0xECu, 0u);
    put(rt, self + 0xF0u, 0u);
    put(rt, self + 0xF4u, 0u);
    put8(rt, self + 0xFAu, 0u);
    put8(rt, self + 0xFBu, 0u);
    put8(rt, self + 0xFCu, 0u);
    put8(rt, self + 0xFDu, 0u);
    put(rt, self + 0x114u, 0u);
    put8(rt, self + 0x118u, 0u);
    put(rt, self + 0x11Cu, 0u);
    put(rt, self + 0x134u, 0u);
    put(rt, self + 0x130u, 0u);
    put(rt, self + 0x12Cu, 0u);
    put8(rt, self + 0x138u, 1u);
    put(rt, self + 0x13Cu, 0u);
    put(rt, self + 0x140u, 0u);
    put(rt, self + 0x14Cu, 0u);
    put(rt, self + 0x144u, 0u);
    put(rt, self + 0x148u, 0u);
    put8(rt, self + 0x150u, 0u);
    put8(rt, self + 0x151u, 0u);
    put8(rt, self + 0x152u, 0u);
    put_float(rt, self + 0x164u, one);
    put_float(rt, self + 0x16Cu, one);
    put_float(rt, self + 0x168u, one);
    put_float(rt, self + 0x160u, one);
    put(rt, self + 0x170u, 0u);
    put(rt, self + 0x174u, 0u);
    put(rt, self + 0x178u, self + 0x178u);
    put(rt, self + 0x17Cu, self + 0x178u);  // the delay slot of the jal
    const std::uint32_t first_block = call_set(rt, ctx, 0x08B35840u, {0x10u, 0x400u});
    put(rt, self + 0x58u, first_block);
    const std::uint32_t second_block = call_set(rt, ctx, 0x08B35840u, {0x10u, 0x1000u});
    put(rt, self + 0xB0u, second_block);
    ctx.fpr[0] = std::bit_cast<float>(0x3C888889u);
    ctx.fpr[1] = psp_mul(second_constant, half);
    ctx.fpr[2] = half;
    ctx.fpr[3] = one;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08B544B0(a0, a1, bytes, count): writes a command run into the graphics library's display list (pointer D_08D6CE5C): n = (bytes + 3) & ~3 rounded to words / 4; D_08D6CDE4 = the list pointer + 8; each of the `count` entries stores the words {0xEB000000 | (offset & 0x3FF), 0xEC000000 | ((i & 0x3FF) << 10), 0xEE000000 | ((n - 1) & 0x3FF), 0xEA000000} at the list pointer + 8 + 16 * i
// (the offset grows by n); a jump word {0x10000000 | (end >> 24) << 16, 0x08000000 | (end & 0xFFFFFF)} opens the run and 0x0B000000 closes it; D_08D6CE5C = end. Then func_08B55AF8(0, a0, 4, 0x40), func_08B55860(a1, 3, 0x10), func_08B55924(0, 0x20, 0x20), func_08B557E0(0.0f, 0.0f), func_08B55820(n0 * D_08CB6198, count * D_08CB6198) with the float of `bytes` rounded and of `count`, ten func_08B55AAC(state, value) calls and a tail call to func_08B59838.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `const std::uint32_t words = static_cast<std::uint32_t>(signed_of(signed_of(aligned) < 0 ? aligned + 3u : align`; `if (signed_of(count) > 0)`
void func_08B544B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = ctx.gpr[4], second = ctx.gpr[5], count = ctx.gpr[7];
    const std::uint32_t aligned = (ctx.gpr[6] + 3u) & ~3u;  // s0
    const std::uint32_t list = word(rt, kGuListPointer);   // t3
    const std::uint32_t words = static_cast<std::uint32_t>(signed_of(signed_of(aligned) < 0 ? aligned + 3u : aligned) >> 2);  // a2
    put(rt, 0x08D6CDE4u, list + 8u);
    std::uint32_t offset = 3u;       // t0
    std::uint32_t closing = 2u;      // v0
    if (signed_of(count) > 0) {
        std::uint32_t at = list;       // a1
        std::uint32_t position = 0u;   // t0
        for (std::uint32_t i = 0u; i != count; ++i, at += 0x10u) {
            put(rt, at + 8u, ins_bits(0xEB000000u, position, 0u, 10u));
            put(rt, at + 0xCu, ins_bits(0xEC000000u, i, 10u, 10u));
            position += words;
            put(rt, at + 0x10u, ins_bits(0xEE000000u, words - 1u, 0u, 10u));
            put(rt, at + 0x14u, 0xEA000000u);
        }
        offset = (count << 2) + 3u;
        closing = (count << 2) + 2u;
    }
    const std::uint32_t end = list + (offset << 2);  // t0
    put(rt, list + (closing << 2), 0x0B000000u);
    put(rt, list + 4u, ins_bits(0x08000000u, end, 0u, 24u));
    put(rt, list, ins_bits(0x10000000u, end >> 24, 16u, 8u));
    put(rt, kGuListPointer, end);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B55AF8u, {0u, target, 4u, 0x40u});
    (void)call_set(rt, ctx, 0x08B55860u, {second, 3u, 0x10u});
    (void)call_set(rt, ctx, 0x08B55924u, {0u, 0x20u, 0x20u});
    (void)call_keep(rt, ctx, 0x08B557E0u, {}, {0.0f, 0.0f});
    const float scale = float_at(rt, 0x08CB6198u);
    const float f12 = static_cast<float>(signed_of(aligned)), f13 = static_cast<float>(signed_of(count));
    (void)call_keep(rt, ctx, 0x08B55820u, {}, {psp_mul(f12, scale), psp_mul(f13, scale)});
    for (const auto [state, value] : {std::pair<std::uint32_t, std::uint32_t>{9u, 0u}, {0xDu, 0u}, {8u, 0u}, {0x10u, 0u}, {0xEu, 0u}, {0xFu, 0xFFu}, {5u, 1u}, {6u, 1u}, {4u, 1u}, {3u, 1u}})
        (void)call_set(rt, ctx, 0x08B55AACu, {state, value});
    tail_call(ctx, 0x08B59838u, {3u, 1u});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08983F74(this, object): copies the settings of `this` into the record r = D_08CF68E8 (the word at that address) using the answers of `object`'s accessors: s = func_089A8BF4(object) (a float) goes to func_08A4D57C(r, this[0x30]) and, as f12, to the two func_08964254 calls (this + 0x50, this + 0x80) whose results are stored in r[0x40] / r[0x44]; r[0x20] / r[0x24] / r[0x5C] /
// r[0x60] / r[0x64] / r[0x68] copy the floats at this + 0xBC, 0xC0, 0x48, 0x70, 0xA4, 0xA0; the flag word r[0x58] takes this[0x34] at bit 10, this[0x35] at 11, this[0x39] at 14, this[0x36] at 9 (a third func_08964254(this + 0x10, s) gives r[0x3C]), this[0xA8] at 12; then four more accessors (func_089A8B38 / B50 / B68 / B80) give the floats a, b, c, d and r[0x48] = a, r[0x4C] = b, r[0x50] = a + c, r[0x54] = b + d.
// With this[0x38] set func_08A4CCE8(r, this[0x3C]) is called (its low byte is stored at r[0x58], bit 8 is set); otherwise func_08A4D738(r, this[0xB4], this[0xB8]). When func_089A8CE0(object) is non-zero, bit 13 is set, the virtual slot 0x138 of the object runs with (object, 1) and its four floats at object + 0x140 are copied to r + 0x10; otherwise bit 13 is cleared. Bit 16 takes this[0xA9]. v0 = r[0x58].
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (visible != 0u)`
void func_08983F74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t record = word(rt, 0x08CF68E8u);  // s0, the delay slot of the first jal
    const float scale = call_keep(rt, ctx, 0x089A8BF4u, {object}, {}).f0;  // f20
    (void)call_set(rt, ctx, 0x08A4D57Cu, {record, word(rt, self + 0x30u)});
    put_float(rt, record + 0x20u, float_at(rt, self + 0xBCu));
    put_float(rt, record + 0x24u, float_at(rt, self + 0xC0u));
    put_float(rt, record + 0x5Cu, float_at(rt, self + 0x48u));  // the delay slot of the jal
    const std::uint32_t first = call_keep(rt, ctx, 0x08964254u, {self + 0x50u}, {scale}).v0;
    put(rt, record + 0x40u, first);
    put_float(rt, record + 0x60u, float_at(rt, self + 0x70u));  // the delay slot of the jal
    const std::uint32_t second = call_keep(rt, ctx, 0x08964254u, {self + 0x80u}, {scale}).v0;
    put(rt, record + 0x44u, second);
    put_float(rt, record + 0x68u, float_at(rt, self + 0xA0u));
    put_float(rt, record + 0x64u, float_at(rt, self + 0xA4u));
    std::uint32_t flags = word(rt, record + 0x58u);
    flags = ins_bits(flags, byte_u(rt, self + 0x34u), 10u, 1u);
    flags = ins_bits(flags, byte_u(rt, self + 0x35u), 11u, 1u);
    flags = ins_bits(flags, byte_u(rt, self + 0x39u), 14u, 1u);
    flags = ins_bits(flags, byte_u(rt, self + 0x36u), 9u, 1u);
    put(rt, record + 0x58u, flags);  // the delay slot of the jal
    const std::uint32_t third = call_keep(rt, ctx, 0x08964254u, {self + 0x10u}, {scale}).v0;
    put(rt, record + 0x3Cu, third);
    flags = ins_bits(word(rt, record + 0x58u), byte_u(rt, self + 0xA8u), 12u, 1u);
    put(rt, record + 0x58u, flags);  // the delay slot of the jal
    const float a = call_keep(rt, ctx, 0x089A8B38u, {object}, {}).f0;
    const float b = call_keep(rt, ctx, 0x089A8B50u, {object}, {}).f0;
    const float c = call_keep(rt, ctx, 0x089A8B68u, {object}, {}).f0;
    const float d = call_keep(rt, ctx, 0x089A8B80u, {object}, {}).f0;
    put_float(rt, record + 0x48u, a);
    put_float(rt, record + 0x50u, a + c);
    put_float(rt, record + 0x54u, b + d);
    put_float(rt, record + 0x4Cu, b);
    if (byte_u(rt, self + 0x38u) != 0u) {
        const std::uint32_t low = call_set(rt, ctx, 0x08A4CCE8u, {record, word(rt, self + 0x3Cu)});
        put8(rt, record + 0x58u, low);
        flags = word(rt, record + 0x58u) | 0x100u;
        put(rt, record + 0x58u, flags);
    } else {
        (void)call_keep(rt, ctx, 0x08A4D738u, {record}, {float_at(rt, self + 0xB4u), float_at(rt, self + 0xB8u)});
    }
    const std::uint32_t visible = call_set(rt, ctx, 0x089A8CE0u, {object});
    if (visible != 0u) {
        flags = word(rt, record + 0x58u) | 0x2000u;
        put(rt, record + 0x58u, flags);
        (void)call_set(rt, ctx, slot_of(rt, object, 0x138u), {object, 1u});
        const std::uint32_t floats = object + 0x140u;  // addiu v0, s2, 0x140
        const float x0 = float_at(rt, object + 0x140u), x1 = float_at(rt, floats + 4u);
        flags = word(rt, record + 0x58u);
        put_float(rt, record + 0x10u, x0);
        put_float(rt, record + 0x14u, x1);
        put_float(rt, record + 0x18u, float_at(rt, floats + 8u));
        put_float(rt, record + 0x1Cu, float_at(rt, floats + 0xCu));
    } else {
        flags = ins_bits(word(rt, record + 0x58u), 0u, 13u, 1u);
        put(rt, record + 0x58u, flags);
    }
    flags = ins_bits(flags, byte_u(rt, self + 0xA9u), 16u, 1u);
    put(rt, record + 0x58u, flags);
    ctx.gpr[2] = flags;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08920E78(this, a1, a2, a3): a constructor. func_08925FE4(this); this[0] = D_08CC5EE0; func_08926398 builds the nine sub-objects of 0x2C bytes at this + 0x40, 0x6C, 0x98, 0xC4, 0xF0, 0x11C, 0x148, 0x174 and 0x1A0; this[8] = a3, this[0xC] = a2, the words this[0x10..0x3C) are zero; the virtual slot 8 of each sub-object runs with (sub, name, 1): D_08CDDCC0 for the first three,
// D_08CDDCA8 for the next three, D_08CDDC90 for the next two and D_08CDDC78 for the last; this[0x1CC] = 5. v0 = 5.
void func_08920E78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], third = ctx.gpr[6], fourth = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    (void)call_set(rt, ctx, 0x08925FE4u, {self});
    put(rt, self, 0x08CC5EE0u);
    for (const std::uint32_t offset : {0x40u, 0x6Cu, 0x98u, 0xC4u, 0xF0u, 0x11Cu, 0x148u, 0x174u, 0x1A0u}) (void)call_set(rt, ctx, 0x08926398u, {self + offset});
    put(rt, self + 8u, fourth);
    put(rt, self + 0xCu, third);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x18u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x20u, 0u);
    put(rt, self + 0x2Cu, 0u);
    put(rt, self + 0x24u, 0u);
    put(rt, self + 0x28u, 0u);
    put(rt, self + 0x30u, 0u);
    put(rt, self + 0x3Cu, 0u);
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    const auto announce = [&](std::uint32_t offset, std::uint32_t name) {
        const std::uint32_t sub = self + offset;
        (void)call_set(rt, ctx, slot_of(rt, sub, 8u), {sub, name, 1u});
    };
    announce(0x40u, 0x08CDDCC0u);
    announce(0x6Cu, 0x08CDDCC0u);
    announce(0x98u, 0x08CDDCC0u);
    announce(0xC4u, 0x08CDDCA8u);
    announce(0xF0u, 0x08CDDCA8u);
    announce(0x11Cu, 0x08CDDCA8u);
    announce(0x148u, 0x08CDDC90u);
    announce(0x174u, 0x08CDDC90u);
    announce(0x1A0u, 0x08CDDC78u);
    ctx.gpr[2] = 5u;
    put(rt, self + 0x1CCu, 5u);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_088B4AE8(this, config): this[0x38] = func_088B4384(config[8]); the 0xB4 bytes at this + 0x3C are cleared (memset); the vectors / lists at this + 0x1B8, 0x1BC, 0x1CC, 0x1E0, 0x1EC, 0x1F8, 0x204, 0x210 and 0x21C are zeroed (three words each, this + 0x1C8, 0x1D8 and 0x1DC too) and the halfwords this[0x228..0x22C] are 0xFFFF; the record {config[0], config[4], func_088B199C(owner), config[8]}
// is kept at sp (owner = this[4]); this[0x3FC] = config[0xC]. When the record's first three words are non-zero, the virtual slot 0x5C of the owner (config[0xC]) gives p, and a non-zero p calls func_088BB44C(this + 0x3C, p, sp, func_088B19E8(owner), config[0xC]); func_088B55EC(this + 0x1B8, sp) runs in that case and when p is zero. Then func_088B6664(this + 0x260); the record
// goes to this[8], this[0xC], this[0x10] and this[0x14]; this[0x18] = func_088B19E8(owner), this[0x1C] = this + 0x3C, this[0x28] = this + 0xF0, this[0x2C] = this + 0x260, this[0x30] = this + 0x1B8 and func_088B4894(this) is the result.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first != 0u && second != 0u && third != 0u)`; `if (found != 0u)`
void func_088B4AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], config = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t lists = self + 0x3Cu, tail = self + 0x1B8u;
    const std::uint32_t owner = word(rt, self + 4u);  // s6, the delay slot of the jal
    const std::uint32_t resolved = call_set(rt, ctx, 0x088B4384u, {word(rt, config + 8u)});
    put(rt, self + 0x38u, resolved);
    (void)call_set(rt, ctx, 0x08B36018u, {lists, 0u, 0xB4u});
    put(rt, self + 0x1B8u, 0u);
    for (const std::uint32_t base : {0x1BCu, 0x1CCu, 0x1E0u, 0x1ECu, 0x1F8u, 0x204u, 0x210u, 0x21Cu}) {
        put(rt, self + base + 4u, 0u);
        put(rt, self + base + 8u, 0u);
        put(rt, self + base, 0u);
        if (base == 0x1BCu) put(rt, tail + 0x10u, 0u);
        if (base == 0x1CCu) {
            put(rt, tail + 0x20u, 0u);
            put(rt, tail + 0x24u, 0u);
        }
    }
    put16(rt, tail + 0x74u, 0xFFFFFFFFu);
    put16(rt, tail + 0x70u, 0xFFFFFFFFu);
    put16(rt, tail + 0x72u, 0xFFFFFFFFu);
    const std::uint32_t first = word(rt, config);
    const std::uint32_t second = word(rt, config + 4u);  // s5, the delay slot of the jal
    const std::uint32_t third = call_set(rt, ctx, 0x088B199Cu, {owner});
    put(rt, frame.at(0u), first);
    put(rt, frame.at(0xCu), word(rt, config + 8u));
    put8(rt, self + 0x3FCu, word(rt, config + 0xCu));
    put(rt, frame.at(4u), second);
    put(rt, frame.at(8u), third);  // the delay slot of the beqz
    if (first != 0u && second != 0u && third != 0u) {
        const std::uint32_t found = call_set_v0(rt, ctx, slot_of(rt, owner, 0x5Cu), {owner, word(rt, config + 0xCu)});
        if (found != 0u) {
            const std::uint32_t counted = call_set(rt, ctx, 0x088B19E8u, {owner});
            (void)call_set(rt, ctx, 0x088BB44Cu, {lists, found, frame.at(0u), counted, word(rt, config + 0xCu)});
        }
        (void)call_set(rt, ctx, 0x088B55ECu, {tail, frame.at(0u)});
    }
    const std::uint32_t sink = self + 0x260u;
    (void)call_set(rt, ctx, 0x088B6664u, {sink});
    put(rt, self + 0x14u, word(rt, frame.at(0xCu)));
    put(rt, self + 0xCu, word(rt, frame.at(4u)));
    put(rt, self + 8u, word(rt, frame.at(0u)));
    const std::uint32_t counted = call_set(rt, ctx, 0x088B19E8u, {owner});
    put(rt, self + 0x10u, word(rt, frame.at(8u)));  // the delay slot of the jal
    put(rt, self + 0x30u, tail);
    put(rt, self + 0x1Cu, lists);
    put(rt, self + 0x2Cu, sink);
    put(rt, self + 0x18u, counted);
    put(rt, self + 0x28u, self + 0xF0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088B4894u, {self});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08A17818(): under the lock D_08CF6250 (func_08B344E0 / func_08B3457C), when the list head h = func_08A17698() has a first node other than itself and the slot D_08CF6240 is empty: the node n = h[0] hands its string n[8] and its pointer n[0xC] to the slots D_08CF6244 / D_08CF6248 (func_08B855E0 assigns the string unless the same cell; a different n[0xC] frees the old one
// with func_08A5BA08 / func_08A5B930 and is stored in D_08CF6244[4]); the first node of func_08A17698() is unlinked (func_08B5C094), its pointer is freed (func_08A5B930), its string released (release_cell) and the node freed with func_08A5BC54(node, 0x10, 4). A new object o = func_08B0DBF8(func_08B62344(0x130)) with o[0] = D_08CD05C0 is initialised with func_08B0EA48(o, the string): on success o[0x30] = D_08A17A0C,
// o[0x34] = 0, o[0x68] = 1, o[0x10C] = 1, o[0x120] = o[0x124] = 0, o[0x128] = 0, D_08CF6240 = o, o[0x7C] = 0, o[0x74] = D_08A17428, o[0x78] = 0 and its virtual slot 8 runs; otherwise its virtual slot 4 runs. v0 = the unlock's result.
// Branches the routes do not take: not registered, see its HOT93 line (read against the asm only).
[[maybe_unused]] void func_08A17818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    constexpr std::uint32_t lock = 0x08CF6250u, singleton = 0x08CF6240u, string_slot = 0x08CF6244u, pointer_slot = 0x08CF6248u;
    (void)call_set(rt, ctx, 0x08B344E0u, {lock});
    const std::uint32_t head = call_set(rt, ctx, 0x08A17698u, {});
    if (word(rt, head) != head && word(rt, singleton) == 0u) {
        const std::uint32_t first = word(rt, call_set(rt, ctx, 0x08A17698u, {}));  // s0
        if (first + 8u != string_slot) {
            (void)call_set(rt, ctx, 0x08B855E0u, {string_slot, first + 8u});
            if (first + 0xCu != pointer_slot) {
                const std::uint32_t old = word(rt, first + 0xCu);
                if (old != 0u) (void)call_set(rt, ctx, 0x08A5BA08u, {old});
                const std::uint32_t kept = word(rt, string_slot + 4u);
                if (kept != 0u) (void)call_set(rt, ctx, 0x08A5B930u, {kept});
                put(rt, string_slot + 4u, word(rt, first + 0xCu));
            }
        }
        const std::uint32_t node = word(rt, call_set(rt, ctx, 0x08A17698u, {}));  // s0
        (void)call_set(rt, ctx, 0x08B5C094u, {node});
        const std::uint32_t pointer = word(rt, node + 0xCu);
        if (pointer != 0u) (void)call_set(rt, ctx, 0x08A5B930u, {pointer});
        release_cell(rt, ctx, word(rt, node + 8u), frame.at(0u));
        ctx.gpr[5] = 0x10u;
        (void)call_set(rt, ctx, 0x08A5BC54u, {node, 0x10u, 4u});
        const std::uint32_t name = word(rt, string_slot);  // s0, the delay slot of the jal
        const std::uint32_t raw = call_set(rt, ctx, 0x08B62344u, {0x130u});
        (void)call_set(rt, ctx, 0x08B0DBF8u, {raw});
        put(rt, raw, 0x08CD05C0u);
        const std::uint32_t ready = call_set(rt, ctx, 0x08B0EA48u, {raw, name});
        if (ready != 0u) {
            put(rt, raw + 0x30u, 0x08A17A0Cu);
            put(rt, raw + 0x34u, 0u);
            put8(rt, raw + 0x68u, 1u);
            put(rt, singleton, raw);
            put8(rt, raw + 0xF8u + 0x14u, 1u);
            put8(rt, raw + 0xF8u + 0x30u, 0u);
            put(rt, raw + 0xF8u + 0x2Cu, 0u);
            put(rt, raw + 0xF8u + 0x28u, 0u);
            put(rt, word(rt, singleton) + 0x7Cu, 0u);
            put(rt, word(rt, singleton) + 0x74u, 0x08A17428u);
            put(rt, word(rt, singleton) + 0x78u, 0u);
            const std::uint32_t owner = word(rt, singleton);
            (void)call_set_v0(rt, ctx, slot_of(rt, owner, 8u), {owner});
        } else {
            (void)call_set_v0(rt, ctx, slot_of(rt, raw, 4u), {raw});
        }
    }
    (void)call_set(rt, ctx, 0x08B3457Cu, {lock});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08B0ACDC(holder): a manager object m = func_08B62344(0x2A450) is built: m[0] = 0, m[0xB4..0xC0] = 0, func_08B09F98(m + 0xD4), func_08B08B1C(m + 0x100), the words m[0x104..0x118] and the bytes m[0x11C], m[0x11D] = 0, func_08B2F31C(m + 0x120), func_08B451A0(m + 0x9770); the bytes m[4..0x17] are cleared, func_08B36018(m + 0x18, 0, 0x18) and func_08B36018(m + 0x30, 0, 0x80);
// m[0xCC] = 0x1E, m[0xC8] = m[0xC4] = 0x3C, m[0xD0] = 0x5A, m[0x110] = m + 0x9770, the bytes m[0xB0..0xB2] = 0; holder[0] = m. The guarded static at D_08D77340 (func_08B6441C / func_08B64494; first time: D_08D77348 = D_08CD7B20) is read into holder[4] = D_08D77348; D_08D473BC takes `holder` when it was zero. v0 = the new D_08D473BC.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (byte_u(rt, guard) == 0u)`; `if (call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u)`; `if (current == 0u) current = holder;`
void func_08B0ACDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t manager = call_set(rt, ctx, 0x08B62344u, {0x2A450u});
    const std::uint32_t tail = manager + 0x9770u;
    put(rt, manager, 0u);
    put(rt, manager + 0xB4u, 0u);
    put(rt, manager + 0xB8u, 0u);
    put(rt, manager + 0xBCu, 0u);
    put(rt, manager + 0xC0u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B09F98u, {manager + 0xD4u});
    (void)call_set(rt, ctx, 0x08B08B1Cu, {manager + 0x100u});
    put(rt, manager + 0x104u, 0u);
    put(rt, manager + 0x108u, 0u);
    put(rt, manager + 0x10Cu, 0u);
    put(rt, manager + 0x114u, 0u);
    put(rt, manager + 0x118u, 0u);
    put8(rt, manager + 0x11Cu, 0u);
    put8(rt, manager + 0x11Du, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B2F31Cu, {manager + 0x120u});
    (void)call_set(rt, ctx, 0x08B451A0u, {tail});
    put8(rt, manager + 4u, 0u);
    put8(rt, manager + 0xEu, 0u);
    for (unsigned i = 1u; i <= 9u; ++i) put8(rt, manager + 0xEu + i, 0u);
    for (unsigned i = 1u; i <= 9u; ++i) put8(rt, manager + 4u + i, 0u);  // the last one in the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B36018u, {manager + 0x18u, 0u, 0x18u});
    (void)call_set(rt, ctx, 0x08B36018u, {manager + 0x30u, 0u, 0x80u});
    put(rt, manager + 0xCCu, 0x1Eu);
    put8(rt, manager + 0xB0u, 0u);
    put(rt, manager + 0xC8u, 0x3Cu);
    put(rt, manager + 0xD0u, 0x5Au);
    put(rt, manager + 0x110u, tail);
    put8(rt, manager + 0xB2u, 0u);
    put8(rt, manager + 0xB1u, 0u);
    put(rt, manager + 0xC4u, 0x3Cu);
    put(rt, holder, manager);
    constexpr std::uint32_t guard = 0x08D77340u, instance = 0x08D77348u;
    if (byte_u(rt, guard) == 0u) {
        if (call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            put(rt, instance, 0x08CD7B20u);
            (void)call_set(rt, ctx, 0x08B64494u, {guard});
        }
    }
    put(rt, holder + 4u, instance);
    std::uint32_t current = word(rt, 0x08D473BCu);
    if (current == 0u) current = holder;  // movz v0, s2, v0
    ctx.gpr[2] = current;
    put(rt, 0x08D473BCu, current);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08A6F4D8(this, source, separator): func_08A98A70(this); the text t = func_08A6BFFC(source) is split at every occurrence of the string *separator (func_08B7CA4C(text, separator) finds the next one): each piece [start, found) is made into a string (func_08C2486C(&piece, start, found, sp + 3)), copied (func_08A6D2D4) and appended to the vector of strings at container + 0x14 with container = this[0]
// (a null container calls func_08A5BB7C(D_08CB0774) first; a full vector goes through func_08B8A03C(vector, end, value), otherwise func_08A76ABC(end, value) constructs it in place), and the piece is released (release_cell); the rest after the last separator is appended the same way (func_08B85EB8 makes it). v0 = this on every path.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (container == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0774u});`; `if (end != 0u) (void)call_set(rt, ctx, 0x08A76ABCu, {end, copy_cell});`; `if (found != 0u)`
void func_08A6F4D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5], separator_slot = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    (void)call_set(rt, ctx, 0x08A98A70u, {self});
    const std::uint32_t separator = word(rt, separator_slot);  // s5, the delay slot of the jal
    const std::uint32_t text = call_set(rt, ctx, 0x08A6BFFCu, {source});
    std::uint32_t start = text;  // s2
    std::uint32_t found = call_set(rt, ctx, 0x08B7CA4Cu, {text, separator});  // s3
    const auto append = [&](std::uint32_t copy_cell, std::uint32_t source_cell) {
        const std::uint32_t container = word(rt, self);  // s0 / s1
        if (container == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0774u});
        const std::uint32_t vector = container + 0x14u;
        (void)call_set(rt, ctx, 0x08A6D2D4u, {copy_cell, source_cell});
        const std::uint32_t end = word(rt, vector + 4u), capacity = word(rt, vector + 8u);
        if (end == capacity) {
            (void)call_set(rt, ctx, 0x08B8A03Cu, {vector, word(rt, container + 0x18u), copy_cell});
        } else {
            ctx.gpr[2] = end + 4u;  // the delay slot of the beqz
            if (end != 0u) (void)call_set(rt, ctx, 0x08A76ABCu, {end, copy_cell});
            ctx.gpr[2] = word(rt, vector + 4u) + 4u;
            put(rt, vector + 4u, ctx.gpr[2]);
        }
    };
    if (found != 0u) {
        const std::uint32_t separator_length = word(rt, separator - 0xCu);  // s7
        const std::uint32_t piece = frame.at(0x10u), copy = frame.at(8u);
        for (;;) {
            (void)call_set(rt, ctx, 0x08C2486Cu, {piece, start, found, frame.at(3u)});
            append(copy, piece);
            start = found + separator_length;
            (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
            found = call_set(rt, ctx, 0x08B7CA4Cu, {start, separator});
            release_cell(rt, ctx, word(rt, piece), frame.at(1u));
            if (found == 0u) break;
        }
    }
    const std::uint32_t rest = frame.at(0xCu), copy = frame.at(4u);
    (void)call_set(rt, ctx, 0x08B85EB8u, {rest, start, frame.at(2u)});
    append(copy, rest);
    (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
    release_cell(rt, ctx, word(rt, rest), frame.at(0u));
    ctx.gpr[2] = self;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// The registered instances (one function per address, the row of each member in its call).

void func_08975314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scan_block<0x08CA2CE0u, 0x08CA2D58u>(rt, ctx); }
void func_089A16CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scan_block<0x08CA4A7Cu, 0x08CA4AF4u>(rt, ctx); }
void func_089F8B2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scan_block<0x08CA84ECu, 0x08CA856Cu>(rt, ctx); }
void func_08A05D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scan_block<0x08CA8E4Cu, 0x08CA8EC4u>(rt, ctx); }
void func_0896ED94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_dispatch_a<0x40000000u, 0x08CA2AE4u>(rt, ctx); }
void func_0896EFAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_dispatch_a<0x80000000u, 0x08CA2AF0u>(rt, ctx); }
void func_0896F5AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_dispatch_b<0x04000000u, 0x08CA2B20u>(rt, ctx); }
void func_0896F7C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { flag_dispatch_b<0x08000000u, 0x08CA2B28u>(rt, ctx); }
void func_0897006C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify_then_gate_b<0x10000000u, 0x08CA2B54u, 0u>(rt, ctx); }
void func_089702C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify_then_gate_b<0x20000000u, 0x08CA2B64u, 1u>(rt, ctx); }
void func_0896F9DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object_call_c<0x08CA2B30u>(rt, ctx); }
void func_0896FE3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object_call_c<0x08CA2B48u>(rt, ctx); }
void func_088D8F8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_holder<0x65Cu, false>(rt, ctx); }
void func_08B9A958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_holder<0u, true>(rt, ctx); }
void func_08B85F30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_fill_insert<FillInsertRow{0x08A76ABCu, 0x08B856F4u, 0x08B85B5Cu, 0x08B85AF4u, 0x08B85A90u, 0x08B859C4u, 0x08B85A28u, 0x08B856B0u, 0x08C97BD8u, 0x08A5B9ECu, 0u, 2u, 0u}>(rt, ctx);
}
void func_08C02A4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_fill_insert<FillInsertRow{0x08A1A718u, 0x08C02658u, 0x08C02894u, 0x08C0282Cu, 0x08C027C8u, 0x08C026FCu, 0x08C02760u, 0x08C02614u, 0x08CA69C4u, 0x08A5B9ECu, 0u, 2u, 0u}>(rt, ctx);
}
void func_08C254A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_fill_insert<FillInsertRow{0x08A6972Cu, 0x08C25238u, 0x08C24F48u, 0x08C251D0u, 0x08C2516Cu, 0x08C25298u, 0x08C25104u, 0x08C24F04u, 0x08CB0DD4u, 0x08A69D68u, 0x08A6A23Cu, 2u, 0u}>(rt, ctx);
}
void func_08C0729C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_fill_insert<FillInsertRow{0x0896418Cu, 0x08C06694u, 0x08C06634u, 0x08C068C0u, 0x08C0685Cu, 0x08C06790u, 0x08C067F4u, 0x08C06684u, 0x08CA75D0u, 0u, 0x089641C4u, 4u, 0x10u}>(rt, ctx);
}
void func_08C12BDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_fill_insert<FillInsertRow{0x089FA280u, 0x08C124BCu, 0x08C1222Cu, 0x08C1251Cu, 0x08C12458u, 0x08C1238Cu, 0x08C123F0u, 0x08C12060u, 0x08CA9108u, 0x089F9B74u, 0x089FB570u, 4u, 0x10u}>(rt, ctx);
}
void func_08BDDD68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    fill_insert_bytes<FillInsertBytesRow{0x08BDD978u, 0x08BDDA10u, 0x08BDD948u, 0x08BDDA3Cu, 0x08BDDB70u, 0x08B8CFA8u, 0x08BDD7B8u, 0x08CA1ABCu}>(rt, ctx);
}
void func_08BDE864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    fill_insert_bytes<FillInsertBytesRow{0x08BDD9DCu, 0x08BDDA74u, 0x08BDD9ACu, 0x08BDDAA0u, 0x08BDDC38u, 0x08B8CFF0u, 0x08BDD7C8u, 0x08CA1ABCu}>(rt, ctx);
}
void func_08B8AB38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    fill_insert_scalar<FillInsertScalarRow{0x08B8A6ACu, 0x08B8A6D8u, 0x08B8A684u, 0x08B8A6FCu, 0x08B8A7F8u, 0x08B8A7D0u, 0x08B8A644u, 0x08C99680u, 0u, false}>(rt, ctx);
}
void func_08BFB68C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    fill_insert_scalar<FillInsertScalarRow{0x08BF8DBCu, 0x08BF8E18u, 0x08BF8D94u, 0x08BF8E3Cu, 0x08BF91E8u, 0x08BF9218u, 0x08BF8B48u, 0x08CA5158u, 1u, false}>(rt, ctx);
}
void func_08BEF324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    fill_insert_scalar<FillInsertScalarRow{0x08BEF114u, 0x08BEF068u, 0x08BEF0ECu, 0x08BEF0BCu, 0x08BEF178u, 0x08BEF150u, 0x08BEEA84u, 0x08CA3870u, 2u, false}>(rt, ctx);
}
void func_08C01FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    fill_insert_scalar<FillInsertScalarRow{0x08C01F20u, 0x08C01EDCu, 0x08C01F54u, 0x08C01F80u, 0x08C01EA4u, 0x08C01E78u, 0x08C01F00u, 0x08CA6734u, 3u, false}>(rt, ctx);
}
void func_08C4B8C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    fill_insert_scalar<FillInsertScalarRow{0x08C499C0u, 0x08C4AF08u, 0x08C4AE20u, 0x08C4AF2Cu, 0x08C46FA4u, 0x08C4B13Cu, 0x08C499B0u, 0x08CB4AFCu, 0u, true}>(rt, ctx);
}
[[maybe_unused]] void func_089B2014(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { args_to_vector<0x089B1FCCu>(rt, ctx); }
[[maybe_unused]] void func_089B2324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { args_to_vector<0x089B22A4u>(rt, ctx); }

}  // namespace
}  // namespace hot_batch_93
}  // namespace gt::game

#define HOT93(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_93::name, gt::ReturnKind::kind)
#define HOT93N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_93::name, gt::ReturnKind::kind)
// Not registered (its only compared case, equal, changed no memory): HOT93(0x088EFA1Cu, func_088EFA1C, Word);
HOT93(0x08C30AD0u, func_08C30AD0, Word);
HOT93(0x08843F8Cu, func_08843F8C, Word);
HOT93(0x089D1720u, func_089D1720, Word);
HOT93(0x08AD8094u, func_08AD8094, Word);
HOT93(0x08B8F2B4u, func_08B8F2B4, Word);
HOT93(0x0886C12Cu, func_0886C12C, Word);
HOT93(0x08896780u, func_08896780, Word);
HOT93(0x08C1DD98u, func_08C1DD98, Word);
HOT93(0x088F1DA0u, func_088F1DA0, Word);
HOT93(0x0891A088u, func_0891A088, Word);
HOT93(0x08984824u, func_08984824, Word);
HOT93(0x089B0EACu, func_089B0EAC, Word);
HOT93(0x08B544B0u, func_08B544B0, Word);
HOT93(0x08983F74u, func_08983F74, Word);
HOT93(0x08920E78u, func_08920E78, Word);
HOT93(0x088B4AE8u, func_088B4AE8, Word);
// Not registered (its 8 compared cases, equal, changed no memory (the singleton already existed in all of them)): HOT93(0x08A17818u, func_08A17818, Word);
HOT93(0x08B0ACDCu, func_08B0ACDC, Word);
HOT93(0x08A6F4D8u, func_08A6F4D8, Word);
HOT93(0x08975314u, func_08975314, Word);
HOT93(0x089A16CCu, func_089A16CC, Word);
HOT93(0x089F8B2Cu, func_089F8B2C, Word);
HOT93(0x08A05D3Cu, func_08A05D3C, Word);
HOT93(0x0896ED94u, func_0896ED94, Word);
HOT93(0x0896EFACu, func_0896EFAC, Word);
HOT93(0x0896F5ACu, func_0896F5AC, Word);
HOT93(0x0896F7C4u, func_0896F7C4, Word);
HOT93(0x0897006Cu, func_0897006C, Word);
HOT93(0x089702C8u, func_089702C8, Word);
HOT93(0x0896F9DCu, func_0896F9DC, Word);
HOT93(0x0896FE3Cu, func_0896FE3C, Word);
HOT93(0x088D8F8Cu, func_088D8F8C, Word);
HOT93(0x08B9A958u, func_08B9A958, Word);
HOT93(0x08B85F30u, func_08B85F30, Word);
HOT93(0x08C02A4Cu, func_08C02A4C, Word);
HOT93(0x08C254A8u, func_08C254A8, Word);
HOT93(0x08C0729Cu, func_08C0729C, Word);
HOT93(0x08C12BDCu, func_08C12BDC, Word);
HOT93(0x08BDDD68u, func_08BDDD68, Word);
HOT93(0x08BDE864u, func_08BDE864, Word);
HOT93(0x08B8AB38u, func_08B8AB38, Word);
HOT93(0x08BFB68Cu, func_08BFB68C, Word);
HOT93(0x08BEF324u, func_08BEF324, Word);
HOT93(0x08C01FB8u, func_08C01FB8, Word);
HOT93(0x08C4B8C0u, func_08C4B8C0, Word);
// Not registered (every case stops on 'kernel isolation (sleep would block)' (not comparable), compared 0): HOT93(0x089B2014u, func_089B2014, Word);
// Not registered (every case stops on the import stub sceDisplay::0x46F186C3 (not comparable), compared 0): HOT93(0x089B2324u, func_089B2324, Word);
