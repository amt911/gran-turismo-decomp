// Hot batch 92: the reached functions of 71-120 instructions that no earlier batch has registered, from the batch-89 probe (`tools.decomp_gate probe b89p`, the five routes with --decomp off). (main includes batch 91, 6,276 functions.) Before any function was read its address was searched for in src/ (main and the open PR branches).
//
// After batch 91 the probe's 281 reached functions left 130 without an active registration: 25 had been written or read and left by batches 89-91 (their head comments give the reasons), 2 have VFPU, 8 call the import stubs from 0x08C70000 and jump through a register, 21 are float-heavy (more than 12 FPU instructions), and 12 were read and left here
// (func_08849B8C and func_08B8B33C pass an `a1` that an earlier call leaves undefined or hold 24-byte records, func_08A92588 / func_08B00E90 / func_08BDABCC / func_08BDBB44 are tangled, func_088A6F74 is a block of several functions behind a jump, func_088BBA00, func_08BB1D9C, func_08C46E38 and func_08BD4724 write no memory of their own, func_08C07E24 is the vector assignment of batch 90's func_08C070FC again).
// So the batch is 62 functions read one by one with `tools.disasm`, branch TARGETS and the delay slot of every branch and jal included. `tools.family_members --list --any-template` over the probe finds no cluster; two members were found by reading: func_08BB2340 and func_08BB24B0 are one function twice (the words differ only in the operand order of two comparisons and in their own jump targets; both call release_range),
// and func_08BB2660 shares its release body (release_pooled) with them. Four more shared bodies are plain helpers (release_counted, assign_counted_pointer, format_ending, pool_push / pool_row). The role words in the comments (holder, string, vector, list, object, virtual slot, key) describe the shape of the accesses; none is a name from the game.
//
// Registered (50): func_08861FD0, func_08865B98, func_08898AB4, func_088A2074, func_088D31B0, func_08898918, func_088FC45C, func_089FAFD4, func_08ADE8C4, func_08A0F344, func_088721DC, func_08B2E434, func_088A5160, func_089BBF8C, func_089064D8 (GT_DECOMP_NESTED: it is reached only through func_088FC45C), func_08C34358, func_08A1F174, func_0883C7EC,
// func_0883CB34, func_0883D494, func_08BB24B0, func_08BB2660, func_08BB13D4, func_08C3EBD8, func_08ACCFF8, func_088A26E0, func_088738FC, func_088D7794, func_08877FB0, func_088F03E4, func_08905D18, func_0890B62C, func_08918170, func_089E4F44, func_08BD4118, func_08A12658, func_089CE3CC, func_089E5BF4, func_08B0E358, func_08990720, func_08998B70, func_089C80DC,
// func_08B5985C, func_08C3029C, func_0895B454 (GT_DECOMP_NESTED: its callers run it inside the compared cases of func_088A2074, which swallow its calls), func_08AF2ECC, func_08B14864, func_08C1E634, func_089EB064, func_08809E90.
// Written but not registered (12; each commented HOT92 line says why): func_088FEB9C, func_0898BDB4, func_088890AC, func_088F9F00, func_088F024C, func_088F05BC, func_088E6250, func_08ACECFC, func_0882F7A0, func_08BB2340, func_0888AD94, func_0897AAB4.
//
// Gates (fast gate, five routes uncapped): every registered function compared at least once with 0 mismatches and at least one compared case that changed memory; `tools.decomp_gate sabotage` pins 50 of 50 (3 rounds: func_088A2074 and func_088FC45C, the parents of the nested func_0895B454 / func_089064D8, were pinned in rounds 2 and 3); `tools.contract_audit` finds nothing new (the two registrations of batch 90 whose callers read v1);
// `tools.literal_check` finds 0 literals missing in the 62 functions (no `--allow` was needed; a few displacements are written as base + step to match the asm, e.g. this + 0x40 + 8); `tools.family_members --check` reads no row of this file. Two functions came back from the first gate with a v0 the original has and the C did not: func_08B2E434 and func_08A0F344 return an address the original leaves in v0 (the import stub / the jump table sets no v0), and func_088721DC returns the pair pointer it loaded when its second slot is empty.
// Branches the compared cases do not take are listed in each function's comment (from `tools.decomp_gate coverage`, then read against the asm). The weakest proofs are the functions whose only compared cases took the main path: see func_08BB13D4 and func_08C3EBD8 (the room-left halves never ran: their pointer arithmetic was re-derived from the asm), func_08918170 (its mask table was
// re-derived instruction by instruction) and the many null-check calls (func_08A5BB7C with the constant of each).
// Helper bodies were also checked by hand, since `tools.literal_check` skips them: release_counted (offsets 4, 8, 0xC), assign_counted_pointer, format_ending (08B85EB8 / 08A6D2D4 / 08A5BAD8 / 08932448), pool_row (registry offsets 0x34, 0x24, 0x28, row offset 0x10, comparator D_08BB1B40, sizes 4 and 0x14), pool_push (0x1C, 0xC) and release_cell (copied from batch 91).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value. Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's), and the callee-saved registers (s0-s7, fp) hold the original's values around the calls
// (`SavedRegs` restores the entry values on the way out; `SavedFloat` does the same for $f20). The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` / `beql` / `bgtzl` / `bgezl` / `blezl` / `bc1fl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths; every "the delay slot of ..." comment below names the instruction right after the branch it says.
// v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the early returns; the other caller-saved registers are set to the original's value where it is cheap and a callee could read them, not otherwise. The dead stack (spills the original makes and nothing reads) is not reproduced, the harness ignores it; the frames that callees read are.
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_92 {
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

// The release of a string cell whose data pointer is `data`: nothing happens when data - 0xC is D_08D74FD0 (the shared empty cell), otherwise func_08B60804(data - 4, -1) and, when its result is not positive, func_08B8542C(data - 0xC, flag). Leaves a0 and s0 (or the register `holder`) like the loops below do.
inline void release_cell(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data, std::uint32_t flag, unsigned holder) {
    ctx.gpr[holder] = data - 0xCu;
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


// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_088F05BC(this): the same checks as func_088F024C (this[4], [8], [0xC], [0x10], [0x14], [0x18] non-zero, the byte this[0x24] and this[0xE8] non-zero, func_088D9CCC(this[4]) == 0, func_088DAA84(this[4]) != 0), then for each halfword h of the list this[0xDC .. 0xE4) func_088F187C(this[0xCC][0] + 84 * h, this + 4, h) and, for each of them again,
// n = func_088F13A4(this[0xCC][0] + 84 * h, this + 4, h, this[0xD4][h] (signed byte)) and, for j below n, func_088EFD54(this, e[4], e[0], h) with e = this[0xCC][0] + 84 * h + 4 + 8 * j. v0 is the last word the original has loaded there.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_088F05BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t fields = self + 4u;  // s2
    ctx.gpr[18] = fields;
    const std::uint32_t item = word(rt, fields);
    ctx.gpr[16] = item;
    ctx.gpr[17] = self;  // the delay slot of the beqz
    if (item == 0u) return;
    for (const std::uint32_t offset : {4u, 8u, 0xCu, 0x10u, 0x14u}) {
        const std::uint32_t value = word(rt, fields + offset);
        ctx.gpr[2] = value;
        if (value == 0u) return;
    }
    const std::uint32_t byte = byte_u(rt, self + 0x24u);
    ctx.gpr[2] = byte;
    if (byte == 0u) return;
    const std::uint32_t last = word(rt, self + 0xE8u);
    ctx.gpr[2] = last;
    if (last == 0u) return;
    ctx.gpr[4] = item;  // the delay slot of the jal
    const std::uint32_t blocked = call_set(rt, ctx, 0x088D9CCCu, {item});
    if (blocked != 0u) return;
    ctx.gpr[4] = item;
    const std::uint32_t usable = call_set(rt, ctx, 0x088DAA84u, {item});
    if (usable == 0u) return;
    std::uint32_t cursor = word(rt, self + 0xDCu);
    ctx.gpr[16] = cursor;
    std::uint32_t end = word(rt, self + 0xE4u);
    ctx.gpr[2] = end;
    ctx.gpr[19] = self + 0xDCu;  // the delay slot of the beq
    if (cursor != end) {
        do {
            const std::uint32_t h = sext16(half_u(rt, cursor));
            ctx.gpr[6] = h;
            const std::uint32_t table = word(rt, self + 0xCCu);
            ctx.gpr[4] = table;
            ctx.gpr[5] = fields;
            const std::uint32_t first = word(rt, table);
            ctx.gpr[7] = first;
            ctx.gpr[3] = h * 4u;
            ctx.gpr[2] = h * 12u;
            ctx.gpr[4] = h * 84u;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x088F187Cu, {first + h * 84u, fields, h});
            end = word(rt, self + 0xE4u);
            ctx.gpr[2] = end;
            cursor += 2u;
            ctx.gpr[16] = cursor;  // the delay slot of the bnel
        } while (cursor != end);
    }
    std::uint32_t s6 = word(rt, self + 0xDCu);
    ctx.gpr[22] = s6;
    ctx.gpr[23] = self + 0xCCu;  // the delay slot of the beq
    if (s6 == end) return;
    do {
        const std::uint32_t h = sext16(half_u(rt, s6));
        ctx.gpr[19] = h;
        const std::uint32_t table = word(rt, self + 0xCCu);
        const std::uint32_t flags = word(rt, self + 0xD4u);
        const std::uint32_t offset = h * 84u;
        ctx.gpr[21] = offset;
        const std::uint32_t kind = sext8(byte_u(rt, flags + h));
        ctx.gpr[7] = kind;
        ctx.gpr[5] = fields;
        const std::uint32_t count = call_set(rt, ctx, 0x088F13A4u, {word(rt, table) + offset, fields, h, kind});
        ctx.gpr[20] = count;  // the delay slot of the beqz
        if (count != 0u) {
            for (std::uint32_t j = 0u; j < count; ++j) {
                ctx.gpr[16] = j;
                const std::uint32_t array = word(rt, self + 0xCCu);
                const std::uint32_t element = offset + word(rt, array) + 4u + (j << 3);
                ctx.gpr[16] = j + 1u;
                ctx.gpr[7] = h;
                ctx.gpr[6] = word(rt, element);
                (void)call_set(rt, ctx, 0x088EFD54u, {self, word(rt, element + 4u), word(rt, element), h});
            }
        }
        end = word(rt, self + 0xE4u);
        ctx.gpr[2] = end;
        s6 += 2u;
        ctx.gpr[22] = s6;
    } while (s6 != end);
}

// func_088FC45C(this, a, b, c): the virtual slot 0x38 of this (sp + 8, this) fills the pair {t, h} at sp + 8; with t non-zero and t[4][0x2C] zero, func_089064D8(t + 0x34, a, b, c, t + 0x6C, byte t[0x30]) decides: a non-zero answer passes the pair (t, h) at sp to the virtual slot 0x1C of the object D_08CFFAAC[0]
// (h gets one more reference at h + 4 first and the copy at sp[4] is released afterwards); the pair's holder h = sp[0xC] is released at the end (release_counted: the references at +4 and +8, the virtual slots 8 and 0xC).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (h == 0u) [the early return]`; `if (t == 0u)`; `if (guard != 0u)`; `if (decided == 0u)`; `if (h != 0u)`; `if (copy != 0u)`; `if (atomic_decrement(rt, ctx, copy + 4u) == 1u)`; `if (atomic_decrement(rt, ctx, copy + 8u) == 1u)`
void func_088FC45C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a = ctx.gpr[5], b = ctx.gpr[6], c = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[18] = a;
    ctx.gpr[17] = c;
    ctx.gpr[16] = b;
    ctx.gpr[5] = self;
    ctx.gpr[4] = p8;  // the delay slot of the jalr
    const std::uint32_t slot = slot_of(rt, self, 0x38u);
    ctx.gpr[3] = slot;
    (void)call_set(rt, ctx, slot, {p8, self});
    const std::uint32_t t = word(rt, p8);
    ctx.gpr[8] = t;
    const auto finish = [&]() {
        const std::uint32_t h = word(rt, pC);
        ctx.gpr[16] = h;  // the delay slot of the beql / beqz
        if (h == 0u) return;
        release_counted(rt, ctx, h);
    };
    if (t == 0u) {
        finish();
        return;
    }
    const std::uint32_t t4 = word(rt, t + 4u);
    const std::uint32_t guard = word(rt, t4 + 0x2Cu);
    ctx.gpr[3] = guard;
    if (guard != 0u) {
        finish();
        return;
    }
    const std::uint32_t kind = byte_u(rt, t + 0x30u);
    ctx.gpr[9] = kind;
    ctx.gpr[8] = t + 0x6Cu;  // the delay slot of the jal
    const std::uint32_t decided = call_set(rt, ctx, 0x089064D8u, {t + 0x34u, a, b, c, t + 0x6Cu, kind});
    if (decided == 0u) {
        finish();
        return;
    }
    constexpr std::uint32_t holder_slot = 0x08CFFAACu;
    const std::uint32_t object = word(rt, holder_slot);
    ctx.gpr[4] = object;
    const std::uint32_t h = word(rt, pC);
    ctx.gpr[5] = h;
    const std::uint32_t pair = word(rt, p8);
    ctx.gpr[3] = pair;
    const std::uint32_t vtable = word(rt, object);
    ctx.gpr[2] = vtable;
    const std::uint32_t target = word(rt, vtable + 0x1Cu);
    ctx.gpr[6] = target;
    put(rt, p0, pair);
    put(rt, p4, h);  // the delay slot of the beqz
    if (h != 0u) {
        put(rt, h + 4u, word(rt, h + 4u) + 1u);
        ctx.gpr[3] = h + 4u;
        ctx.gpr[2] = 1u;
    }
    ctx.gpr[5] = p0;  // the delay slot of the jalr
    (void)call_set(rt, ctx, target, {object, p0});
    const std::uint32_t copy = word(rt, p4);
    ctx.gpr[16] = copy;
    if (copy != 0u) {
        ctx.gpr[3] = copy + 4u;
        if (atomic_decrement(rt, ctx, copy + 4u) == 1u) {
            ctx.gpr[2] = 1u;
            (void)call_set(rt, ctx, slot_of(rt, copy, 8u), {copy});
            ctx.gpr[3] = copy + 8u;
            if (atomic_decrement(rt, ctx, copy + 8u) == 1u) {
                ctx.gpr[2] = 1u;
                (void)call_set(rt, ctx, slot_of(rt, copy, 0xCu), {copy});
            }
        }
    }
    finish();
}

// func_089FAFD4(k, name, rec): t = func_089F9BFC()[0] + 12 * k (a vector of 32-byte elements {begin, end, capacity}) is cut to 2 elements: with n = (t[1] - t[0]) / 32, func_08C0DBBC(t, t[0] + 0x20, t[1]) erases the tail when n >= 2, otherwise func_08C0E3B8(t, t[1], 1 - n, sp + 0x20) appends (the record at sp + 0x20 is four 1.0f).
// Then the holder e = func_089F9BFC()[0][12 * k] (read through the first word of the element) takes the record rec: func_0896418C(sp + 0x40, rec) builds a temporary, func_089641C4(e, sp + 0x40) assigns it (unless it is the same) and e[0x10] = rec[0x10]; the string func_08B88F44(sp + 0xC, name) with sp[0x10] = k is inserted into the tree func_089F9E54() with func_08C0DEF8(tree, sp + 0xC)
// and the string cell is released (release_cell with the flag sp).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (count >= 2u)`; `if (element != p40)`; `if (data - 0xCu != 0x08D74FD0u)`; `if (signed_of(references) <= 0) [the call it guards]`
void func_089FAFD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t k = ctx.gpr[4], name = ctx.gpr[5], rec = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), pC = frame.at(0xCu), p20 = frame.at(0x20u), p40 = frame.at(0x40u);
    ctx.gpr[21] = name;
    ctx.gpr[19] = rec;
    ctx.gpr[18] = k;
    const std::uint32_t where = call_set(rt, ctx, 0x089F9BFCu, {});  // the delay slot of the jal stores s0 (dead)
    ctx.gpr[3] = k * 4u;
    const std::uint32_t base = word(rt, where);
    ctx.gpr[4] = base;
    mtc1(ctx, 0, 0x3F800000u);
    const std::uint32_t offset = k * 12u;
    ctx.gpr[17] = offset;
    ctx.gpr[2] = 0xFFFFFFFFu;
    const std::uint32_t t = base + offset;
    ctx.gpr[8] = t;
    put(rt, frame.at(0x30u), 0xFFFFFFFFu);
    ctx.gpr[4] = t;
    ctx.gpr[7] = p20;
    put_float(rt, p20, ctx.fpr[0]);
    put_float(rt, frame.at(0x2Cu), ctx.fpr[0]);
    put_float(rt, frame.at(0x28u), ctx.fpr[0]);
    put_float(rt, frame.at(0x24u), ctx.fpr[0]);
    const std::uint32_t end = word(rt, t + 4u), begin = word(rt, t);
    ctx.gpr[3] = end;
    ctx.gpr[5] = begin;
    ctx.gpr[6] = end;
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(end - begin) >> 5);
    ctx.gpr[2] = count;
    ctx.gpr[9] = count;
    if (count >= 2u) {
        ctx.gpr[5] = begin + 0x20u;  // the delay slot of the bnez
        (void)call_set(rt, ctx, 0x08C0DBBCu, {t, begin + 0x20u, end});
    } else {
        ctx.gpr[5] = begin + 0x20u;  // the delay slot of the bnez
        ctx.gpr[6] = 1u - count;
        ctx.gpr[4] = t;
        ctx.gpr[5] = end;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08C0E3B8u, {t, end, 1u - count, p20});
    }
    const std::uint32_t where2 = call_set(rt, ctx, 0x089F9BFCu, {});
    const std::uint32_t table = word(rt, where2);
    ctx.gpr[3] = offset + table;
    ctx.gpr[16] = p40;
    const std::uint32_t element = word(rt, offset + table);  // the delay slot of the jal below
    ctx.gpr[17] = element;
    (void)call_set(rt, ctx, 0x0896418Cu, {p40, rec});
    const std::uint32_t extra = word(rt, rec + 0x10u);
    ctx.gpr[20] = pC;
    ctx.gpr[5] = p40;
    ctx.gpr[4] = element;
    put(rt, frame.at(0x50u), extra);  // the delay slot of the beq
    if (element != p40) {
        (void)call_set(rt, ctx, 0x089641C4u, {element, p40});
        ctx.gpr[3] = word(rt, frame.at(0x50u));
        put(rt, element + 0x10u, word(rt, frame.at(0x50u)));
    }
    const std::uint32_t tree = call_set(rt, ctx, 0x089F9E54u, {});
    ctx.gpr[16] = tree;
    (void)call_set(rt, ctx, 0x08B88F44u, {pC, name});
    put(rt, frame.at(0x10u), k);  // the delay slot of the jal below
    const std::uint32_t node = call_set(rt, ctx, 0x08C0DEF8u, {tree, pC});
    const std::uint32_t flag = ctx.gpr[3];
    ctx.gpr[5] = 0xFFFFFFFFu;
    const std::uint32_t data = word(rt, pC);
    ctx.gpr[6] = data;
    put(rt, frame.at(0x64u), flag);
    ctx.gpr[16] = data - 0xCu;
    const std::uint32_t flag_byte = byte_u(rt, frame.at(0x64u));
    ctx.gpr[3] = flag_byte;
    put(rt, frame.at(4u), node);
    ctx.gpr[4] = data - 4u;
    put(rt, frame.at(0x60u), node);
    ctx.gpr[2] = 0x08D74FD0u;
    put8(rt, frame.at(8u), flag_byte);  // the delay slot of the beq
    if (data - 0xCu != 0x08D74FD0u) {
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = data - 0xCu;
        ctx.gpr[5] = p0;  // the delay slot of the bgtz
        if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, p0});
    }
}

// func_08ADE8C4(dt): (f12 = dt, kept in the callee-saved f20) func_08AEB808(); in the table at D_08D40674 (s3) a block on the stack of 4 * D_08D40674[0x28] bytes (rounded up to 16) receives the pointers func_08ADE268(block) lists (n of them); func_08AEB82C(); then for each object o (the bit 0 of the byte o[0x8F] set): the callback c = o[0x90][0xC] (when non-zero)
// is called as c(o) with f12 = dt, a = o[0xC]; with the bit 0x100 of a, o[0xEC] += 1 and o[0xF0] = 0; with the halfword k = o[0xEA] >= 0 and o[0xEC] < D_08D40674[0x24][k * 0x100 + 0xEC], o[0xF0] += dt; then with the bit 0x10 of a set and the bit 0 of the byte o[0x11] clear (or the bit 0x10 clear), the signed byte m = o[0xE9]:
// m > 0 and o[0x34] <= o[0x30] gives, for m < 2, o[0xE9] = m + 1 (the byte) and o[0xFC] = 0, and for m >= 2 (or the bit 0x10 set with bit 0 of o[0x11] clear) func_08ADE0E4(o, 0) (the clear of the fields below is skipped); m <= 0 or o[0x34] > o[0x30] gives o[0xFC] = 0; the halfword o[0x70] and the byte o[0x8E] are cleared in every case except func_08ADE0E4's.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (!next_only)`; `if (ctx.gpr[2] != 0u)`; `the else branch of the test above`; `if (signed_of(ctx.gpr[2]) >= 0)`; `if (ctx.gpr[3] != 0u)`; `if (ctx.gpr[2] == 0u)`; `if (!call_clear)`; `if (signed_of(m) <= 0)`; `if (!(ctx.fpr[1] <= ctx.fpr[0]))`; `if (call_clear)`
void func_08ADE8C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    SavedFloat saved_f20(ctx, 20u);
    ctx.gpr[30] = frame.at(0u);
    ctx.fpr[20] = ctx.fpr[12];
    (void)call_keep(rt, ctx, 0x08AEB808u, {});
    constexpr std::uint32_t table = 0x08D40674u;
    ctx.gpr[17] = table;
    const std::uint32_t slots = word(rt, table + 0x28u);
    ctx.gpr[2] = (((slots << 2) + 0xFu) & ~0xFu);
    GuestFrame array(ctx, (((slots << 2) + 0xFu) & ~0xFu));
    const std::uint32_t listed = call_set(rt, ctx, 0x08ADE268u, {array.at(0u)});
    ctx.gpr[16] = listed;  // the delay slot of the next jal
    (void)call_keep(rt, ctx, 0x08AEB82Cu, {});
    ctx.gpr[19] = table;  // the delay slot of the blez
    if (signed_of(listed) <= 0) return;
    std::uint32_t remaining = listed;
    ctx.gpr[18] = remaining;
    std::uint32_t cursor = array.at(0u);
    ctx.gpr[17] = cursor;
    for (;;) {
        const std::uint32_t o = word(rt, cursor);
        ctx.gpr[16] = o;
        ctx.gpr[2] = sext8(byte_u(rt, o + 0x8Fu));
        ctx.gpr[2] &= 1u;
        const bool next_only = (ctx.gpr[2] == 0u);
        if (!next_only) {
            ctx.gpr[2] = word(rt, o + 0x90u);  // the delay slot of the bnel
            ctx.gpr[2] = word(rt, ctx.gpr[2] + 0xCu);
            ctx.gpr[4] = o;  // the delay slot of the bnez
            std::uint32_t a2;
            if (ctx.gpr[2] != 0u) {
                ctx.fpr[12] = ctx.fpr[20];  // the delay slot of the jalr
                (void)call_keep(rt, ctx, ctx.gpr[2], {o}, {ctx.fpr[20]});
                a2 = word(rt, o + 0xCu);
            } else {
                a2 = word(rt, o + 0xCu);
            }
            ctx.gpr[6] = a2;
            ctx.gpr[2] = a2 & 0x100u;
            if (ctx.gpr[2] != 0u) {
                ctx.gpr[2] = word(rt, o + 0xECu);
                put(rt, o + 0xF0u, 0u);
                ctx.gpr[2] += 1u;
                put(rt, o + 0xECu, ctx.gpr[2]);
            }
            ctx.gpr[2] = sext16(half_u(rt, o + 0xEAu));
            if (signed_of(ctx.gpr[2]) >= 0) {
                ctx.gpr[2] <<= 8;
                const std::uint32_t a1 = word(rt, table + 0x24u);
                ctx.gpr[5] = a1;
                const std::uint32_t v1 = word(rt, o + 0xECu);
                ctx.gpr[2] += a1;
                const std::uint32_t a0 = word(rt, ctx.gpr[2] + 0xECu);
                ctx.gpr[4] = a0;
                ctx.gpr[3] = signed_of(v1) < signed_of(a0) ? 1u : 0u;
                ctx.gpr[2] = a2 & 0x10u;  // the delay slot of the beqz
                if (ctx.gpr[3] != 0u) {
                    ctx.fpr[0] = float_at(rt, o + 0xF0u);
                    ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[20];
                    put_float(rt, o + 0xF0u, ctx.fpr[0]);
                }
            }
            ctx.gpr[2] = a2 & 0x10u;
            bool clear = true;
            bool call_clear = false;
            std::uint32_t v1 = 0u;
            const auto read_state = [&]() {
                v1 = byte_u(rt, o + 0xE9u);
                ctx.gpr[3] = v1;
            };
            if (ctx.gpr[2] == 0u) {
                read_state();  // the delay slot of the beql (taken)
            } else {
                ctx.gpr[2] = byte_u(rt, o + 0x11u);
                ctx.gpr[2] &= 1u;
                ctx.gpr[4] = o;  // the delay slot of the beqz
                if (ctx.gpr[2] == 0u) {
                    call_clear = true;
                } else {
                    read_state();
                }
            }
            if (!call_clear) {
                const std::uint32_t m = sext8(v1);
                ctx.gpr[2] = m;
                if (signed_of(m) <= 0) {
                    put(rt, o + 0xFCu, 0u);  // the delay slot of the blezl (taken)
                } else {
                    ctx.fpr[1] = float_at(rt, o + 0x34u);
                    ctx.fpr[0] = float_at(rt, o + 0x30u);
                    if (!(ctx.fpr[1] <= ctx.fpr[0])) {
                        put(rt, o + 0xFCu, 0u);  // the delay slot of the bc1fl (taken)
                    } else {
                        ctx.gpr[2] = signed_of(m) < 2 ? 1u : 0u;
                        ctx.gpr[4] = o;  // the delay slot of the bnez
                        if (ctx.gpr[2] != 0u) {
                            ctx.gpr[2] = v1 + 1u;
                            put8(rt, o + 0xE9u, ctx.gpr[2]);
                            put(rt, o + 0xFCu, 0u);
                        } else {
                            call_clear = true;
                        }
                    }
                }
                if (!call_clear) {
                    put16(rt, o + 0x70u, 0u);
                    put8(rt, o + 0x8Eu, 0u);
                    clear = true;
                }
            }
            if (call_clear) {
                ctx.gpr[5] = 0u;
                (void)call_set(rt, ctx, 0x08ADE0E4u, {o, 0u});
            }
            (void)clear;
        }
        remaining -= 1u;  // the delay slot of the j (call_clear) or the next instruction
        ctx.gpr[18] = remaining;
        if (remaining == 0u) break;
        cursor += 4u;
        ctx.gpr[17] = cursor;
    }
}

// func_088E6250(this): for i = 0..1 the object s = this[4 * i] (zero skips) with s[0x50C] == 1 is stepped under the try-lock s + 0x508 (set to 1 when it is 0; the ll / sc pairs are read as a load and a store): the state m = s[0x530] (a jump table at D_08C89EF8 for m < 7, otherwise r = 0, f = 0) decides a result r and a flag f:
// m = 0: r = 0, f = 0; m = 1: r = the virtual slot 0x90 of s (s, a1) with a1 = 0 when i != 0 or s[0x534] == 1 and 1 otherwise, f = 1; m = 2, 3, 4, 5: r = the virtual slot 0x94, 0x98, 0x9C, 0xA0 of s (s), f = 1; m = 6: for i != 0, or for i == 0 when this[4] is non-zero and this[4][0x530] != 6, r = the virtual slot 0x8C of s (s) and f = 1;
// otherwise func_088E60B4(this) and a = func_088EE660(s) run and a non-zero a gives r = the virtual slot 0x8C of s (s), a zero a gives r = 0; f = 1. Then s[0x530] = r; a non-zero r runs func_088ECAC8(s), a zero r with f set clears the byte s[0x525]. The lock word is set back to 0 when it is 1 (also when s[0x50C] != 1). v0 = 2 at the end.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_088E6250(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    std::uint32_t &s3 = ctx.gpr[19];
    s3 = 0u;
    for (;;) {
        const std::uint32_t s0 = word(rt, self + 4u * s3);
        ctx.gpr[16] = s0;
        if (s0 == 0u) {
            s3 += 1u;  // the delay slot of the beql (taken)
        } else {
            const std::uint32_t kind = word(rt, s0 + 0x50Cu);
            ctx.gpr[3] = kind;
            ctx.gpr[2] = 1u;
            const std::uint32_t lock = s0 + 0x508u;
            ctx.gpr[17] = lock;  // the delay slot of the bne
            if (kind == 1u) {
                ctx.gpr[3] = 1u;
                const std::uint32_t held = word(rt, lock);
                ctx.gpr[2] = held;
                if (held == 0u) {
                    put(rt, lock, 1u);
                    ctx.gpr[2] = 1u;
                }
                const std::uint32_t state = word(rt, s0 + 0x530u);
                ctx.gpr[3] = state;
                ctx.gpr[2] = state < 7u ? 1u : 0u;
                std::uint32_t result = 0u, flag = 0u;
                const auto call_vtable = [&](std::uint32_t offset) {
                    ctx.gpr[3] = word(rt, s0);
                    return call_set(rt, ctx, slot_of(rt, s0, offset), {s0});
                };
                if (state < 7u) {
                    bool vcall = false, replay = false;
                    switch (state) {
                    case 0u:
                        break;
                    case 1u: {
                        const std::uint32_t function = word(rt, s0);
                        ctx.gpr[2] = function;
                        const std::uint32_t target = word(rt, function + 0x90u);
                        ctx.gpr[6] = target;
                        std::uint32_t a1 = 0u;
                        if (s3 == 0u) {
                            const std::uint32_t mode = word(rt, s0 + 0x534u);
                            ctx.gpr[3] = mode;
                            ctx.gpr[2] = 1u;
                            a1 = 1u;  // the delay slot of the bne
                            if (mode == 1u) a1 = 0u;
                        }
                        ctx.gpr[5] = a1;
                        ctx.gpr[4] = s0;  // the delay slot of the jalr
                        result = call_set(rt, ctx, target, {s0, a1});
                        flag = 1u;
                        break;
                    }
                    case 2u:
                        result = call_vtable(0x94u);
                        flag = 1u;
                        break;
                    case 3u:
                        result = call_vtable(0x98u);
                        flag = 1u;
                        break;
                    case 4u:
                        result = call_vtable(0x9Cu);
                        flag = 1u;
                        break;
                    case 5u:
                        result = call_vtable(0xA0u);
                        flag = 1u;
                        break;
                    default:  // 6
                        if (s3 != 0u) {
                            vcall = true;
                        } else {
                            const std::uint32_t other = word(rt, self + 4u);
                            ctx.gpr[2] = other;
                            if (other == 0u) {
                                replay = true;
                            } else {
                                const std::uint32_t other_state = word(rt, other + 0x530u);
                                ctx.gpr[3] = other_state;
                                if (other_state != 6u) vcall = true;
                                else replay = true;
                            }
                        }
                        if (replay) {
                            ctx.gpr[4] = self;
                            (void)call_set(rt, ctx, 0x088E60B4u, {self});
                            ctx.gpr[4] = s0;
                            const std::uint32_t answer = call_set(rt, ctx, 0x088EE660u, {s0});
                            flag = 1u;  // the delay slot of the beqz
                            if (answer == 0u) {
                                result = 0u;
                            } else {
                                vcall = true;
                            }
                        }
                        if (vcall) {
                            result = call_vtable(0x8Cu);
                            flag = 1u;
                        }
                        break;
                    }
                }
                ctx.gpr[2] = result;
                ctx.gpr[3] = flag;
                put(rt, s0 + 0x530u, result);  // the delay slot of the beqz
                if (result != 0u) {
                    ctx.gpr[4] = s0;
                    (void)call_set(rt, ctx, 0x088ECAC8u, {s0});
                } else if (flag != 0u) {
                    put8(rt, s0 + 0x525u, 0u);
                }
            }
            const std::uint32_t current = word(rt, lock);
            ctx.gpr[2] = (current ^ 1u) != 0u ? 1u : 0u;
            if ((current ^ 1u) == 0u) {
                put(rt, lock, 0u);
                ctx.gpr[2] = 1u;
            }
            s3 += 1u;
        }
        ctx.gpr[2] = 2u;
        if (s3 == 2u) break;
        ctx.gpr[16] = word(rt, self + 4u);  // the delay slot of the bne
    }
}

// func_08A0F344(this): m = byte this[0x63] & 0xF; m == 0 returns v0 = 0. Otherwise a prefix call on this and a tail jump: m = 1..5 call func_08A0F0BC(this), m = 6..10 func_08A0F15C(this), m = 11..15 func_08A0F16C(this) (the jump table at D_08C8E0D0 of the original); then the tail goes to the virtual slot 0x1B0, 0x1B4 or 0x1B8 of this (this) for the
// positions 1-3 of each group of five, or to func_08A0F0CC(this) for the 4th and func_08A0F114(this) for the 5th.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `target = position == 3u ? 0x08A0F0CCu : 0x08A0F114u`
void func_08A0F344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t mode = byte_u(rt, self + 0x63u) & 0xFu;
    std::uint32_t target = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[2] = mode;
        ctx.gpr[3] = 1u;  // the sltiu result
        ctx.gpr[16] = self;  // the delay slot of the beqz
        if (mode == 0u) {
            ctx.gpr[3] = 0x08C8E0D0u;  // the jump table
            ctx.gpr[2] = 0x08C8E0D0u + 4u * mode;
            ctx.gpr[4] = word(rt, 0x08C8E0D0u + 4u * mode);
            return;
        }
        const std::uint32_t group = (mode - 1u) / 5u, position = (mode - 1u) % 5u;
        constexpr std::uint32_t prefixes[3] = {0x08A0F0BCu, 0x08A0F15Cu, 0x08A0F16Cu};
        ctx.gpr[4] = self;  // the delay slot of the jal
        (void)call_set(rt, ctx, prefixes[group], {self});
        if (position < 3u) {
            const std::uint32_t table = word(rt, self);
            ctx.gpr[2] = table;
            ctx.gpr[4] = self;
            const std::uint32_t offsets[3] = {0x1B0u, 0x1B4u, 0x1B8u};
            target = word(rt, table + offsets[position]);
            ctx.gpr[25] = target;
        } else {
            ctx.gpr[4] = self;
            target = position == 3u ? 0x08A0F0CCu : 0x08A0F114u;
        }
    }
    tail_call(ctx, target, {self});
}

// func_088721DC(this, a1, object): with a1 == 0: the pair p = this[0x5C] = {first, second}: a non-zero first / second gets its virtual slot 0x2C called and p[0] / p[4] cleared; then p[0] = (the virtual slot 0x18 of `object`, or 0), and a non-zero first f = p[0] gets, with k = func_0887BD94(3), its method m = (*f)[8] called (f, k), the virtual slot 0x14 of this[0x5C][0] (this), the virtual slot 0x18 (D_08B96E54) and the slot 0x28 of it;
// p[4] = the same virtual slot 0x18 result, and a non-zero second gets the same sequence with k = func_0887BD94(4), the slot 0x14 (this), 0x18 (D_08B96E5C) and the tail jump to its virtual slot 0x28. A non-zero a1 returns at once.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (object != 0u)`; `if (item == 0u) return false`; `const std::uint32_t method = word(rt, word(rt, item) + 8u)`; `if (!tail)`; `the else branch of the test above`; `if (!stage(4u, 4u, 0x08B96E5Cu, true)) [the early return]`
void func_088721DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5], object = ctx.gpr[6];
    std::uint32_t target = 0u, last = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[20] = object;
        ctx.gpr[18] = self;
        if (flag != 0u) return;
        std::uint32_t s0 = word(rt, self + 0x5Cu);
        ctx.gpr[16] = s0;
        const std::uint32_t slot = self + 0x5Cu;  // s3
        ctx.gpr[19] = slot;
        const auto drop = [&](std::uint32_t field) {
            const std::uint32_t item = word(rt, s0 + field);
            ctx.gpr[4] = item;
            if (item != 0u) {
                ctx.gpr[2] = word(rt, item);
                ctx.gpr[3] = word(rt, word(rt, item) + 0x2Cu);
                (void)call_set(rt, ctx, slot_of(rt, item, 0x2Cu), {item});
                put(rt, word(rt, self + 0x5Cu) + field, 0u);
                s0 = word(rt, self + 0x5Cu);
                ctx.gpr[16] = s0;
            }
        };
        drop(0u);
        drop(4u);
        std::uint32_t made = 0u;
        if (object != 0u) {
            ctx.gpr[4] = object;  // the delay slot of the jalr
            made = call_set(rt, ctx, slot_of(rt, object, 0x18u), {object});
        }
        put(rt, s0, made);
        ctx.gpr[2] = made;
        const auto stage = [&](std::uint32_t field, std::uint32_t kind, std::uint32_t constant, bool tail) {
            std::uint32_t current = word(rt, slot);
            ctx.gpr[16] = current;
            if (field != 0u) ctx.gpr[2] = current;  // the second stage loads the pair into v0
            const std::uint32_t item = word(rt, current + field);
            ctx.gpr[17] = item;
            ctx.gpr[4] = kind;  // the delay slot of the beqz
            if (item == 0u) return false;
            const std::uint32_t method = word(rt, word(rt, item) + 8u);
            ctx.gpr[16] = method;  // the delay slot of the jal
            const std::uint32_t code = call_set(rt, ctx, 0x0887BD94u, {kind});
            ctx.gpr[5] = code;
            ctx.gpr[4] = item;  // the delay slot of the jalr
            (void)call_set(rt, ctx, method, {item, code});
            const std::uint32_t holder = word(rt, word(rt, slot) + field);
            ctx.gpr[4] = holder;
            ctx.gpr[5] = self;  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot_of(rt, holder, 0x14u), {holder, self});
            const std::uint32_t again = word(rt, slot);
            ctx.gpr[6] = again;
            ctx.gpr[5] = constant;
            const std::uint32_t item2 = word(rt, again + field);
            ctx.gpr[4] = item2;
            (void)call_set(rt, ctx, slot_of(rt, item2, 0x18u), {item2, constant});
            const std::uint32_t item3 = word(rt, word(rt, slot) + field);
            ctx.gpr[4] = item3;
            if (!tail) {
                (void)call_set(rt, ctx, slot_of(rt, item3, 0x28u), {item3});
            } else {
                target = slot_of(rt, item3, 0x28u);
                last = item3;
            }
            return true;
        };
        (void)stage(0u, 3u, 0x08B96E54u, false);
        s0 = word(rt, slot);
        ctx.gpr[16] = s0;
        std::uint32_t second = 0u;  // the delay slot of the beqz
        if (object != 0u) {
            ctx.gpr[4] = object;
            second = call_set(rt, ctx, slot_of(rt, object, 0x18u), {object});
        }
        put(rt, s0 + 4u, second);
        ctx.gpr[2] = second;
        if (!stage(4u, 4u, 0x08B96E5Cu, true)) return;
        ctx.gpr[4] = last;
    }
    tail_call(ctx, target, {last});
}

// func_08ACECFC(this): a constructor: this[0] = D_08CD6B28; func_08B2EE10(this + 4, 2, 0x300), func_08B2EE10(this + 0x18, 2, 0x303), func_08B2EDD4(this + 0x2C, D_08CB49D4), func_08B0CC04(this + 0x40), the bytes this[0x160] and this[0x162] = 0, the holders this + 0x164, + 0x3B8, + 0x420, + 0x4A0 (func_08B28DCC) and + 0x334, + 0x360, + 0x38C, + 0x448,
// + 0x474 (func_08B28D34) are initialised, this[0x244] = this + 0x240, the words this[0x300], [0x30C], [0x314], [0x31C], [0x328], [0x32C], [0x330], [0x518] and [0x240] = 0, this[0x510] = 4 and this[0x514] = -1; func_08B2EEB8(this + 4), (this + 0x18) and (this + 0x2C) run and the import stub at 0x08C7AD70 is called last (no arguments of its own: a0 is left as the original has it).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_08ACECFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = self + 4u;
    ctx.gpr[5] = 2u;
    ctx.gpr[16] = self;
    ctx.gpr[6] = 0x300u;
    ctx.gpr[18] = self + 0x18u;
    ctx.gpr[17] = self + 0x2Cu;
    put(rt, self, 0x08CD6B28u);
    (void)call_set(rt, ctx, 0x08B2EE10u, {self + 4u, 2u, 0x300u});
    ctx.gpr[6] = 0x303u;
    ctx.gpr[5] = 2u;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B2EE10u, {self + 0x18u, 2u, 0x303u});
    ctx.gpr[5] = 0x08CB49D4u;
    (void)call_set(rt, ctx, 0x08B2EDD4u, {self + 0x2Cu, 0x08CB49D4u});
    (void)call_set(rt, ctx, 0x08B0CC04u, {self + 0x40u});
    put8(rt, self + 0x160u, 0u);
    ctx.gpr[4] = self + 0x164u;
    put8(rt, self + 0x162u, 0u);
    (void)call_set(rt, ctx, 0x08B28DCCu, {self + 0x164u});
    ctx.gpr[2] = self + 0x240u;
    put(rt, self + 0x244u, self + 0x240u);
    ctx.gpr[4] = self + 0x334u;
    for (const std::uint32_t offset : {0x300u, 0x30Cu, 0x314u, 0x31Cu, 0x328u, 0x32Cu}) put(rt, self + offset, 0u);
    put(rt, self + 0x330u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B28D34u, {self + 0x334u});
    (void)call_set(rt, ctx, 0x08B28D34u, {self + 0x360u});
    (void)call_set(rt, ctx, 0x08B28D34u, {self + 0x38Cu});
    (void)call_set(rt, ctx, 0x08B28DCCu, {self + 0x3B8u});
    (void)call_set(rt, ctx, 0x08B28DCCu, {self + 0x420u});
    (void)call_set(rt, ctx, 0x08B28D34u, {self + 0x448u});
    (void)call_set(rt, ctx, 0x08B28D34u, {self + 0x474u});
    (void)call_set(rt, ctx, 0x08B28DCCu, {self + 0x4A0u});
    ctx.gpr[2] = 4u;
    put(rt, self + 0x510u, 4u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[4] = self + 4u;
    put(rt, self + 0x514u, 0xFFFFFFFFu);
    put(rt, self + 0x518u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B2EEB8u, {self + 4u});
    (void)call_set(rt, ctx, 0x08B2EEB8u, {self + 0x18u});
    (void)call_set(rt, ctx, 0x08B2EEB8u, {self + 0x2Cu});
    (void)call_keep(rt, ctx, 0x08C7AD70u, {});
    put(rt, self + 0x240u, 0u);
}

// func_08B2E434(): the 0x6C bytes at sp are zeroed (func_08B36018(sp, 0, 0x6C)), sp[0] = 0x6C, h = the import stub at 0x08C7A9E8 (no arguments), the stub at 0x08C7AA80 (h, sp) fills the record, the stub at 0x08C7A888 gives s1, and i = ~func_08B2E13C(sp[0x30]) is the insertion position in the table D_08D68DC0 of 12-byte rows (count D_08D68D88 = n):
// the rows i..n-1 move up by one row (the 12-byte copies are done with the unaligned word pairs of the original) and row i becomes {h, sp[0x30], sp[0x34] + sp[0x30]}; the count becomes n + 1 and the import stub at 0x08C7A8A8 (s1) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): none: every branch and region of the compared cases ran.
void func_08B2E434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x80u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p30 = frame.at(0x30u), p34 = frame.at(0x34u);
    constexpr std::uint32_t count_address = 0x08D68D88u, table = 0x08D68DC0u;
    ctx.gpr[6] = 0x6Cu;
    ctx.gpr[4] = p0;
    ctx.gpr[5] = 0u;
    (void)call_set(rt, ctx, 0x08B36018u, {p0, 0u, 0x6Cu});
    ctx.gpr[3] = 0x6Cu;
    put(rt, p0, 0x6Cu);  // the delay slot of the jal
    const std::uint32_t handle = call_keep(rt, ctx, 0x08C7A9E8u, {});
    ctx.gpr[4] = handle;
    ctx.gpr[5] = p0;
    ctx.gpr[16] = handle;  // the delay slot of the next jal
    (void)call_keep(rt, ctx, 0x08C7AA80u, {handle, p0});
    const std::uint32_t other = call_keep(rt, ctx, 0x08C7A888u, {});
    const std::uint32_t start = word(rt, p30);
    ctx.gpr[4] = start;
    ctx.gpr[17] = other;  // the delay slot of the next jal
    const std::uint32_t found = call_set(rt, ctx, 0x08B2E13Cu, {start});
    const std::uint32_t count = word(rt, count_address);
    ctx.gpr[11] = count;
    const std::uint32_t position = ~found;
    ctx.gpr[9] = position;
    ctx.gpr[2] = signed_of(position) < signed_of(count) ? 1u : 0u;
    if (signed_of(position) < signed_of(count)) {
        std::uint32_t from = table + (count - 1u) * 12u;
        std::uint32_t t0 = count;
        do {
            for (std::uint32_t i = 0u; i < 12u; i += 4u) put(rt, from + 12u + i, word(rt, from + i));
            t0 -= 1u;
            from -= 12u;
        } while (signed_of(position) < signed_of(t0));
    }
    const std::uint32_t row = table + position * 12u;
    const std::uint32_t second = word(rt, p30), third = word(rt, p34);
    ctx.gpr[4] = other;
    put(rt, row + 8u, third + second);
    put(rt, count_address, count + 1u);
    put(rt, row, handle);
    put(rt, row + 4u, second);  // the delay slot of the jal
    ctx.gpr[2] = row;  // the import stub sets no v0: the function returns the row address
    ctx.gpr[3] = third + second;
    ctx.gpr[5] = count + 1u;
    ctx.gpr[6] = second;
    (void)call_keep(rt, ctx, 0x08C7A8A8u, {other});
}

// func_088A5160(this): a destructor: this[0] = D_08CC29A8, func_088A3750(this); this[0x318] = D_08CC2C30, this[0x31C] = D_08CC2C4C, func_088A8420(this + 0x318); func_08BAE078(this[0x338], this[0x33C], sp + 1), a non-zero this[0x338] is freed with func_08B35254; func_08BAC238(this[0x32C], this[0x330], sp) and a non-zero this[0x32C] is freed;
// this[0x31C] = D_08CC2A98, this[0x318] = D_08CC2AB0; a non-zero this[0x274] gets its virtual slot 0 called (it, this[0x270]) and this[0x274], this[0x270] = 0; this[0x20] = D_08CC2CB8, this[0x24] = D_08CC2DDC; the four 12-byte elements from this + 0x158 down to this + 0x134 and the four 28-byte elements from this + 0x118 down to this + 0xC4 get their
// virtual slot 0 called (element); this[0x20] = D_08CBF428, this[0x24] = D_08CC16A0, this[0x10] = D_08CC2AD8, func_088A7548(this + 8) and this[0] = D_08CC2AE8.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first != 0u) [the call it guards]`; `if (second != 0u) [the call it guards]`; `if (listener != 0u)`
void func_088A5160(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p1 = frame.at(1u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = self + 0x318u;
    ctx.gpr[2] = 0x08CC29A8u;
    put(rt, self, 0x08CC29A8u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088A3750u, {self});
    ctx.gpr[2] = 0x08CC2C30u;
    put(rt, self + 0x318u, 0x08CC2C30u);
    ctx.gpr[2] = 0x08CC2C4Cu;
    put(rt, (self + 0x318u + 4u), 0x08CC2C4Cu);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088A8420u, {self + 0x318u});
    ctx.gpr[6] = p1;
    (void)call_set(rt, ctx, 0x08BAE078u, {word(rt, self + 0x338u), word(rt, self + 0x33Cu), p1});
    const std::uint32_t first = word(rt, self + 0x338u);
    ctx.gpr[4] = first;
    if (first != 0u) (void)call_set(rt, ctx, 0x08B35254u, {first});
    (void)call_set(rt, ctx, 0x08BAC238u, {word(rt, self + 0x32Cu), word(rt, self + 0x330u), p0});
    const std::uint32_t second = word(rt, self + 0x32Cu);
    ctx.gpr[4] = second;
    if (second != 0u) (void)call_set(rt, ctx, 0x08B35254u, {second});
    ctx.gpr[2] = 0x08CC2A98u;
    put(rt, (self + 0x318u + 4u), 0x08CC2A98u);
    ctx.gpr[2] = 0x08CC2AB0u;
    ctx.gpr[16] = self + 0x270u;
    put(rt, self + 0x318u, 0x08CC2AB0u);
    const std::uint32_t listener = word(rt, (self + 0x270u + 4u));
    ctx.gpr[4] = listener;
    if (listener != 0u) {
        ctx.gpr[5] = word(rt, self + 0x270u);  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot_of(rt, listener, 0u), {listener, word(rt, self + 0x270u)});
        put(rt, (self + 0x270u + 4u), 0u);
        put(rt, self + 0x270u, 0u);
    }
    ctx.gpr[20] = self + 0x20u;
    ctx.gpr[2] = 0x08CC2CB8u;
    put(rt, self + 0x20u, 0x08CC2CB8u);
    put(rt, self + 0x24u, 0x08CC2DDCu);  // the delay slot of the beqz
    ctx.gpr[17] = self + 0x164u;
    ctx.gpr[16] = self + 0x158u;
    ctx.gpr[19] = 4u;
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t element = self + 0x158u - 0xCu * i;
        ctx.gpr[2] = word(rt, element);
        ctx.gpr[17] = element;
        ctx.gpr[19] = 3u - i;
        ctx.gpr[4] = element;
        ctx.gpr[16] = element - 0xCu;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot_of(rt, element, 0u), {element});
    }
    ctx.gpr[2] = self + 0x20u + 0xA4u;
    ctx.gpr[17] = self + 0x20u + 0x114u;
    ctx.gpr[16] = self + 0x20u + 0xF8u;
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t element = self + 0x20u + 0xF8u - 0x1Cu * i;
        ctx.gpr[2] = word(rt, element);
        ctx.gpr[17] = element;
        ctx.gpr[19] = 3u - i;
        ctx.gpr[4] = element;
        ctx.gpr[16] = element - 0x1Cu;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot_of(rt, element, 0u), {element});
    }
    ctx.gpr[2] = 0x08CBF428u;
    put(rt, self + 0x20u, 0x08CBF428u);
    ctx.gpr[2] = 0x08CC16A0u;
    ctx.gpr[3] = 0x08CC2AD8u;
    put(rt, self + 0x24u, 0x08CC16A0u);
    ctx.gpr[4] = self + 8u;
    put(rt, self + 0x10u, 0x08CC2AD8u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088A7548u, {self + 8u});
    ctx.gpr[2] = 0x08CC2AE8u;
    put(rt, self, 0x08CC2AE8u);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_089BBF8C(this, x): with the bit 5 of this[0x60] clear it is set and the virtual slot 0x138 of this (this) runs; for each node n of the list this[0x44] -> n[0x18] -> ... and each child c of n[0xC] -> c[0x54] -> ... the virtual slot 0x150 of c (c, x) runs. When the bit 5 was clear and the bit 2 of this[0x60] is set:
// the holders sp + 0xC (func_08A76AA0), sp + 8 (func_08A69B3C(sp + 8, D_08CA5CF4)) and func_08A7BFD0(this, sp + 0xC, sp + 8) are built and sp + 8 is destroyed; with sp[0xC] = h non-zero the holders sp + 0x10 and sp + 0x14 (func_08A76AA0) and sp + 4 (func_08A76AAC(sp + 4, sp) with sp[0] = x) are built, sp + 4 is copied
// to sp + 0x14 with func_08A5BAD8 (unless it is the same) and destroyed; h (null: func_08A5BB7C(D_08CA5CA8)) gets its virtual slot 0xCC (h, sp + 0x10, 1, sp + 0x14); sp + 0x14, sp + 0x10 and sp + 0xC are destroyed with func_08A5B9EC.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (was_set == 0u)`; `if (was_set != 0u) [the early return]`; `if (mode == 0u) [the early return]`; `if (h != 0u)`; `if (p14 != p4) [the call it guards]`; `if (holder == 0u) [the call it guards]`
void func_089BBF8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p14 = frame.at(0x14u);
    ctx.gpr[19] = self;
    std::uint32_t flags = word(rt, self + 0x60u);
    ctx.gpr[3] = flags;
    const std::uint32_t was_set = (flags >> 5) & 1u;
    ctx.gpr[20] = was_set;
    ctx.gpr[18] = x;  // the delay slot of the bnez
    if (was_set == 0u) {
        ctx.gpr[2] = word(rt, self);
        flags |= 0x20u;
        ctx.gpr[3] = flags;
        put(rt, self + 0x60u, flags);
        const std::uint32_t target = slot_of(rt, self, 0x138u);
        ctx.gpr[6] = target;
        (void)call_set(rt, ctx, target, {self});
    }
    std::uint32_t node = word(rt, self + 0x44u);
    ctx.gpr[17] = node;
    while (node != 0u) {
        std::uint32_t child = word(rt, node + 0xCu);
        ctx.gpr[16] = child;
        while (child != 0u) {
            ctx.gpr[3] = word(rt, child);
            ctx.gpr[5] = x;  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot_of(rt, child, 0x150u), {child, x});
            child = word(rt, child + 0x54u);
            ctx.gpr[16] = child;
        }
        node = word(rt, node + 0x18u);
        ctx.gpr[17] = node;
    }
    if (was_set != 0u) return;
    const std::uint32_t mode = word(rt, self + 0x60u) & 4u;
    ctx.gpr[2] = mode;
    if (mode == 0u) return;
    ctx.gpr[20] = pC;
    (void)call_set(rt, ctx, 0x08A76AA0u, {pC});
    ctx.gpr[16] = p8;
    ctx.gpr[5] = 0x08CA5CF4u;
    (void)call_set(rt, ctx, 0x08A69B3Cu, {p8, 0x08CA5CF4u});
    ctx.gpr[6] = p8;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A7BFD0u, {self, pC, p8});
    (void)call_set(rt, ctx, 0x08A69D68u, {p8});
    const std::uint32_t h = word(rt, pC);
    ctx.gpr[2] = h;
    ctx.gpr[19] = p10;  // the delay slot of the beqz
    if (h != 0u) {
        ctx.gpr[4] = p10;
        ctx.gpr[17] = p14;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A76AA0u, {p10});
        ctx.gpr[16] = p4;
        (void)call_set(rt, ctx, 0x08A76AA0u, {p14});
        ctx.gpr[4] = p4;
        ctx.gpr[5] = p0;
        put(rt, p0, x);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A76AACu, {p4, p0});
        ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
        if (p14 != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {p14, word(rt, p4)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
        const std::uint32_t holder = word(rt, pC);
        ctx.gpr[16] = holder;
        if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5CA8u});
        ctx.gpr[2] = word(rt, holder);
        ctx.gpr[4] = holder;
        ctx.gpr[5] = p10;
        const std::uint32_t slot = slot_of(rt, holder, 0xCCu);
        ctx.gpr[3] = slot;
        ctx.gpr[6] = 1u;
        ctx.gpr[7] = p14;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot, {holder, p10, 1u, p14});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p14});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
}

// func_089064D8(this, rec, a2, object, t0, flag): the guard func_08906398(this) (zero returns 0) and rec[4] non-zero (zero returns 0). func_08909928(rec, t0 + 8) runs when the low byte of `flag` is set, and then func_08909998(rec, t0 + 4); rec[0x1C] = this[0][0x13C], rec[0x20] = this[0][0x14C]; the virtual slot 0x5C of `object` gives a word whose bit 7 (inverted) is the first argument of
// k = func_08BD2D74(bit ^ 1, this[0][0x14][0x48][0x28], this[0][0x14][0x48][0x38]); func_089099B8(this[0] + 0x28, k); func_08AF1DC8(rec[4], this[0] + 0x1C, 0x189) and, for the non-zero words this[0][0x68], [0x84], [0xE8], [0xCC], func_08AF1DC8(rec[4], this[0] + 0x64, 0x60000), (rec[4], this[0] + 0x80, 0x20), func_08905AE4(this[0][0x14]), (rec[0xC], this[0] + 0xE4, 8),
// (rec[0xC], this[0] + 0xC8, 0x20); v0 = 1 at the end.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (guard == 0u)`; `if (cell == 0u)`; `if (flag != 0u)`; `if (first != 0u)`; `if (second != 0u)`; `if (third != 0u)`; `if (fourth != 0u)`
void func_089064D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], rec = ctx.gpr[5], object = ctx.gpr[7], fields = ctx.gpr[8], flag = ctx.gpr[9] & 0xFFu;
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = object;
    ctx.gpr[19] = rec;
    ctx.gpr[18] = self;
    ctx.gpr[17] = fields;
    ctx.gpr[16] = flag;  // the delay slot of the jal
    const std::uint32_t guard = call_set(rt, ctx, 0x08906398u, {self});
    ctx.gpr[3] = 0u;  // the delay slot of the beqz
    if (guard == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t cell = word(rt, rec + 4u);
    ctx.gpr[2] = cell;
    ctx.gpr[4] = rec;  // the delay slot of the bnez
    if (cell == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[5] = fields + 8u;
    (void)call_set(rt, ctx, 0x08909928u, {rec, fields + 8u});
    if (flag != 0u) {
        ctx.gpr[5] = fields + 4u;  // the delay slot of the j
        ctx.gpr[4] = rec;
        (void)call_set(rt, ctx, 0x08909998u, {rec, fields + 4u});
    }
    std::uint32_t base = word(rt, self);
    ctx.gpr[3] = base;
    ctx.gpr[4] = object;
    put(rt, rec + 0x1Cu, word(rt, base + 0x13Cu));
    base = word(rt, self);
    put(rt, rec + 0x20u, word(rt, base + 0x14Cu));
    ctx.gpr[3] = word(rt, object);
    base = word(rt, self);
    ctx.gpr[16] = base + 0x1Cu;  // the delay slot of the jalr
    ctx.gpr[17] = base + 0x28u;
    const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, object, 0x5Cu), {object});
    const std::uint32_t a1 = word(rt, self);
    const std::uint32_t inverted = ((answer >> 7) & 1u) ^ 1u;
    ctx.gpr[4] = inverted;
    const std::uint32_t table = word(rt, word(rt, a1 + 0x14u) + 0x48u);
    ctx.gpr[3] = table;
    ctx.gpr[6] = word(rt, table + 0x38u);
    ctx.gpr[5] = word(rt, table + 0x28u);  // the delay slot of the jal
    const std::uint32_t k = call_set(rt, ctx, 0x08BD2D74u, {inverted, word(rt, table + 0x28u), word(rt, table + 0x38u)});
    ctx.gpr[5] = k;
    (void)call_set(rt, ctx, 0x089099B8u, {base + 0x28u, k});
    std::uint32_t current = word(rt, self);
    (void)call_set(rt, ctx, 0x08AF1DC8u, {word(rt, rec + 4u), base + 0x1Cu, 0x189u});
    current = word(rt, self);
    ctx.gpr[5] = current;
    const std::uint32_t first = word(rt, current + 0x68u);
    ctx.gpr[2] = first;
    if (first != 0u) {
        ctx.gpr[4] = word(rt, rec + 4u);
        ctx.gpr[5] = current + 0x64u;
        ctx.gpr[6] = 0x00060000u;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08AF1DC8u, {word(rt, rec + 4u), current + 0x64u, 0x00060000u});
        current = word(rt, self);
        ctx.gpr[5] = current;
    }
    const std::uint32_t second = word(rt, current + 0x84u);
    ctx.gpr[2] = second;
    if (second != 0u) {
        ctx.gpr[4] = word(rt, rec + 4u);
        ctx.gpr[5] = current + 0x80u;
        (void)call_set(rt, ctx, 0x08AF1DC8u, {word(rt, rec + 4u), current + 0x80u, 0x20u});
        current = word(rt, self);
        ctx.gpr[5] = current;
    }
    ctx.gpr[4] = word(rt, current + 0x14u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08905AE4u, {word(rt, current + 0x14u)});
    current = word(rt, self);
    ctx.gpr[5] = current;
    const std::uint32_t third = word(rt, current + 0xE8u);
    ctx.gpr[2] = third;
    if (third != 0u) {
        ctx.gpr[4] = word(rt, rec + 0xCu);
        ctx.gpr[5] = current + 0xE4u;
        ctx.gpr[6] = 8u;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08AF1DC8u, {word(rt, rec + 0xCu), current + 0xE4u, 8u});
        current = word(rt, self);
        ctx.gpr[5] = current;
    }
    const std::uint32_t fourth = word(rt, current + 0xCCu);
    ctx.gpr[2] = fourth;
    ctx.gpr[3] = 1u;  // the delay slot of the beqz
    if (fourth != 0u) {
        ctx.gpr[4] = word(rt, rec + 0xCu);
        ctx.gpr[5] = current + 0xC8u;
        ctx.gpr[6] = 0x20u;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08AF1DC8u, {word(rt, rec + 0xCu), current + 0xC8u, 0x20u});
    }
    ctx.gpr[2] = 1u;
}

// func_08C34358(a0, out, args): o = args[0] (null: func_08A5BB7C(D_08CB30B8)) answers k = the virtual slot 0x94 of o (o) and func_08C31A00(out, k) runs; the iterator holder sp + 8 is built by the virtual slot 0xA8 of args[0] (sp + 8, args[0]); while the object it = sp[8][0x14] (null holder: func_08A5BB7C(D_08CB3150))
// answers non-zero to its virtual slot 8 (it): its virtual slot 0xC runs (it) and, with the holder h = sp + 0xC built by func_08A76ABC(sp + 0xC, it + 4), two holders are built by the virtual slot 0x84 of h[0] (sp + 4, h[0], 0) and (sp, h[0], 1), each appended with func_08C34248(out, holder) and destroyed with func_08A5B9EC; sp + 0xC is destroyed.
// The loop ends by destroying sp + 8.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (s0 == 0u) [the call it guards]`; `if (h == 0u) [the call it guards]`
void func_08C34358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[18] = out;
    std::uint32_t s0 = word(rt, args);
    ctx.gpr[16] = s0;
    ctx.gpr[17] = args;  // the delay slot of the bnez
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
    ctx.gpr[3] = word(rt, s0);
    const std::uint32_t kind = call_set(rt, ctx, slot_of(rt, s0, 0x94u), {s0});
    ctx.gpr[5] = kind;
    ctx.gpr[4] = out;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08C31A00u, {out, kind});
    s0 = word(rt, args);
    ctx.gpr[16] = s0;
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
    ctx.gpr[19] = p8;
    ctx.gpr[5] = s0;
    ctx.gpr[4] = p8;
    (void)call_set(rt, ctx, slot_of(rt, s0, 0xA8u), {p8, s0});
    ctx.gpr[20] = pC;
    ctx.gpr[17] = p4;  // the delay slot of the j
    const auto holder_call = [&](std::uint32_t target, std::uint32_t flag) {
        const std::uint32_t h = word(rt, pC);
        ctx.gpr[16] = h;
        if (h == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
        ctx.gpr[5] = h;
        ctx.gpr[6] = flag;
        (void)call_set(rt, ctx, slot_of(rt, h, 0x84u), {target, h, flag});
        (void)call_set(rt, ctx, 0x08C34248u, {out, target});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {target});
    };
    for (;;) {
        s0 = word(rt, p8);
        ctx.gpr[16] = s0;
        if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB3150u});
        std::uint32_t cursor = word(rt, s0 + 0x14u);
        ctx.gpr[4] = cursor;
        ctx.gpr[3] = word(rt, cursor);
        const std::uint32_t more = call_set(rt, ctx, slot_of(rt, cursor, 8u), {cursor});
        if (more == 0u) break;
        cursor = word(rt, s0 + 0x14u);
        ctx.gpr[4] = cursor;
        ctx.gpr[3] = word(rt, cursor);
        (void)call_set(rt, ctx, slot_of(rt, cursor, 0xCu), {cursor});
        s0 = word(rt, p8);
        ctx.gpr[16] = s0;
        if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB3150u});
        const std::uint32_t item = word(rt, s0 + 0x14u);
        ctx.gpr[5] = item + 4u;  // the delay slot of the bnel
        ctx.gpr[4] = pC;
        (void)call_set(rt, ctx, 0x08A76ABCu, {pC, item + 4u});
        holder_call(p4, 0u);
        holder_call(p0, 1u);
        (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
}

// func_08A1F174(a0, argc, args): argc <= 0 returns. o = args[0] (null: func_08A5BB7C(D_08CAA480)); when func_08A76F20(o, func_08A80ED0()) is non-zero the record holder sp is built by func_08A6BA34(sp, args) and must answer both func_08A6BE64(sp, D_08CAA624) and func_08A6BE64(sp, D_08CAA62C); then the holder sp + 4 is built by func_08A76AAC(sp + 4, D_08CF6ED8) and
// set from func_08A6BBE8(sp, D_08CAA62C) (func_08A5BAD8 unless it is the same); its object (null: func_08A5BB7C(D_08CAA480)) answers q = the virtual slot 0xDC (object, 0) and a non-zero q goes to func_08B59ADC(q); the holder sp + 8 is built by func_08A76AAC(sp + 8, D_08CF6ED8) and set from func_08A6BBE8(sp, D_08CAA624); its object's virtual slot 0xDC (object, 0)
// answers r and a non-zero r goes to func_08AF75A0(r) and func_08B35254(r). sp + 8, sp + 4 and sp are destroyed with func_08A5B9EC.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(argc) <= 0) [the early return]`; `if (s0 == 0u) [the call it guards]`; `if (known == 0u) [the early return]`; `if (first != 0u)`; `if (second != 0u)`; `if (p4 != source) [the call it guards]`; `if (object == 0u) [the call it guards]`; `if (q != 0u) [the call it guards]`; `if (p8 != other)`; `if (r != 0u)`
void func_08A1F174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argc = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u);
    ctx.gpr[17] = args;
    if (signed_of(argc) <= 0) return;
    std::uint32_t s0 = word(rt, args);
    ctx.gpr[16] = s0;
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
    const std::uint32_t key = call_set(rt, ctx, 0x08A80ED0u, {});
    ctx.gpr[5] = key;
    const std::uint32_t known = call_set(rt, ctx, 0x08A76F20u, {s0, key});
    if (known == 0u) return;
    ctx.gpr[5] = args;
    ctx.gpr[4] = p0;
    (void)call_set(rt, ctx, 0x08A6BA34u, {p0, args});
    ctx.gpr[4] = p0;
    const std::uint32_t first = call_set(rt, ctx, 0x08A6BE64u, {p0, 0x08CAA624u});
    if (first != 0u) {
        ctx.gpr[4] = p0;
        const std::uint32_t second = call_set(rt, ctx, 0x08A6BE64u, {p0, 0x08CAA62Cu});
        ctx.gpr[19] = p4;  // the delay slot of the beqz
        if (second != 0u) {
            ctx.gpr[5] = 0x08CF6ED8u;
            (void)call_set(rt, ctx, 0x08A76AACu, {p4, 0x08CF6ED8u});
            ctx.gpr[4] = p0;
            const std::uint32_t source = call_set(rt, ctx, 0x08A6BBE8u, {p0, 0x08CAA62Cu});
            ctx.gpr[5] = word(rt, source);  // the delay slot of the bnel
            if (p4 != source) (void)call_set(rt, ctx, 0x08A5BAD8u, {p4, word(rt, source)});
            const auto virtual_call = [&](std::uint32_t holder_address) {
                const std::uint32_t object = word(rt, holder_address);
                ctx.gpr[16] = object;
                if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
                ctx.gpr[3] = word(rt, object);
                ctx.gpr[4] = object;
                ctx.gpr[5] = 0u;  // the delay slot of the jalr
                return call_set(rt, ctx, slot_of(rt, object, 0xDCu), {object, 0u});
            };
            const std::uint32_t q = virtual_call(p4);
            ctx.gpr[18] = p8;  // the delay slot of the beqz
            if (q != 0u) (void)call_set(rt, ctx, 0x08B59ADCu, {q});
            ctx.gpr[18] = p8;
            ctx.gpr[5] = 0x08CF6ED8u;
            (void)call_set(rt, ctx, 0x08A76AACu, {p8, 0x08CF6ED8u});
            ctx.gpr[4] = p0;
            const std::uint32_t other = call_set(rt, ctx, 0x08A6BBE8u, {p0, 0x08CAA624u});
            ctx.gpr[16] = word(rt, p8);
            if (p8 != other) {
                ctx.gpr[5] = word(rt, other);
                (void)call_set(rt, ctx, 0x08A5BAD8u, {p8, word(rt, other)});
            }
            const std::uint32_t r = virtual_call(p8);
            ctx.gpr[16] = r;  // the delay slot of the beqz
            if (r != 0u) {
                (void)call_set(rt, ctx, 0x08AF75A0u, {r});
                ctx.gpr[4] = r;
                (void)call_set(rt, ctx, 0x08B35254u, {r});
            }
            (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
        }
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
}

// func_0882F7A0(out, key, argc, args): (argc <= 0 returns) the flags f1, f2, f3 are the truthiness of the virtual slot 0x3C of args[1], args[2], args[3] (each read only when argc is large enough, each null checked with func_08A5BB7C(D_08C9A2B4); 0 otherwise); the holder sp + 8 is built by func_088297A8(sp + 8, key) and its object (null: func_08A5BB7C(D_08C9A2F8))
// gives n = func_088294D0(object); sp + 8 is destroyed. With args[0] non-zero the holder sp is built by func_08847008(sp, args) and its object (null: func_08A5BB7C(D_08C9A30C)) gives a1 = func_08846E50(object); r = func_08861CB0(n, a1, f1, f2, f3, 0) and the holder sp + 4 = func_08A97A6C(sp + 4, r) is copied to `out` with func_08A5BAD8 (unless it is the same) and destroyed,
// and sp is destroyed when it was built.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_0882F7A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], key = ctx.gpr[5], argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u);
    ctx.gpr[21] = out;
    ctx.gpr[19] = key;
    ctx.gpr[18] = args;
    ctx.gpr[17] = argc;
    if (signed_of(argc) <= 0) return;
    const auto flag_of = [&](std::uint32_t index, unsigned reg) {
        ctx.gpr[reg] = 0u;
        if (signed_of(argc) < static_cast<std::int32_t>(index + 1u)) return;
        const std::uint32_t object = word(rt, args + 4u * index);
        ctx.gpr[16] = object;
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
        ctx.gpr[3] = word(rt, object);
        const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, object, 0x3Cu), {object});
        ctx.gpr[reg] = answer != 0u ? 1u : 0u;
    };
    flag_of(1u, 23u);
    flag_of(2u, 22u);
    flag_of(3u, 20u);
    ctx.gpr[17] = p8;
    ctx.gpr[5] = key;
    (void)call_set(rt, ctx, 0x088297A8u, {p8, key});
    const std::uint32_t holder = word(rt, p8);
    ctx.gpr[16] = holder;
    if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const std::uint32_t named = call_set(rt, ctx, 0x088294D0u, {holder});
    ctx.gpr[19] = named;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    const std::uint32_t first = word(rt, args);
    ctx.gpr[2] = first;
    ctx.gpr[17] = 0u;
    ctx.gpr[5] = 0u;  // the delay slot of the beqz
    if (first != 0u) {
        ctx.gpr[5] = args;
        ctx.gpr[4] = p0;
        (void)call_set(rt, ctx, 0x08847008u, {p0, args});
        const std::uint32_t object = word(rt, p0);
        ctx.gpr[16] = object;
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A30Cu});
        const std::uint32_t value = call_set(rt, ctx, 0x08846E50u, {object});
        ctx.gpr[5] = value;
        ctx.gpr[17] = 1u;
    }
    const std::uint32_t a1 = ctx.gpr[5];
    ctx.gpr[7] = ctx.gpr[22];
    ctx.gpr[8] = ctx.gpr[20];
    ctx.gpr[9] = 0u;  // the delay slot of the jal
    const std::uint32_t r = call_set(rt, ctx, 0x08861CB0u, {named, a1, ctx.gpr[23], ctx.gpr[22], ctx.gpr[20], 0u});
    ctx.gpr[16] = p4;
    ctx.gpr[5] = r;
    (void)call_set(rt, ctx, 0x08A97A6Cu, {p4, r});
    ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
    if (out != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p4)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    if (ctx.gpr[17] != 0u) (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
}

// The common ending of func_0883C7EC, func_0883CB34 and func_0883D494: the string at `buffer` (sp + 0xC) builds the holder sp + 4 (func_08B85EB8(sp + 4, buffer, sp + 1)); func_08A6D2D4(sp + 8, sp + 4) builds the holder sp + 8, which is copied to `out` with func_08A5BAD8 (unless it is `out`) and destroyed; the string cell of sp[4] is released (release_cell, flag sp);
// func_08932448(context) ends it (the context is the 0x1A0-byte object func_0893600C built at sp + 0x90).
inline void format_ending(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, std::uint32_t out, std::uint32_t context, std::uint32_t buffer) {
    const std::uint32_t p4 = frame.at(4u), p8 = frame.at(8u);
    ctx.gpr[16] = p4;
    ctx.gpr[17] = p8;
    ctx.gpr[5] = buffer;
    ctx.gpr[4] = p4;
    ctx.gpr[6] = frame.at(1u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B85EB8u, {p4, buffer, frame.at(1u)});
    ctx.gpr[5] = p4;
    ctx.gpr[4] = p8;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A6D2D4u, {p8, p4});
    ctx.gpr[5] = word(rt, p8);  // the delay slot of the beq
    if (out != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p8)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    const std::uint32_t data = word(rt, p4);
    ctx.gpr[4] = data;
    ctx.gpr[2] = 0x08D74FD0u;
    release_cell(rt, ctx, data, frame.at(0u), 16u);
    ctx.gpr[4] = context;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08932448u, {context});
}

// func_0883C7EC(out, argc, args): (argc <= 0 returns) the 0x1A0-byte context sp + 0x90 is built by func_0893600C and set up by func_08935B10(context, a1, func_0883B848(args)) (a1 is not read); r = func_08932988(context): zero copies the two bytes of D_08C9C460 to sp + 0xC; otherwise the string at sp + 0xC is
// func_08B3767C(sp + 0xC, 0x80, D_08C9C464, r, func_08B48C30(D_08D6950C)) when argc == 1 or when the virtual slot 0x3C of args[1] (null: func_08A5BB7C(D_08C9BD48)) is non-zero, and func_08B3767C(sp + 0xC, 0x80, D_08C9C46C, r) when it is zero. format_ending finishes.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(argc) <= 0) [the early return]`; `if (r == 0u)`; `if (plain)`; `if (object == 0u) [the call it guards]`; `if (format_with_name)`
void func_0883C7EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x230u);
    SavedRegs saved(ctx);
    const std::uint32_t context = frame.at(0x90u), buffer = frame.at(0xCu);
    ctx.gpr[20] = out;
    ctx.gpr[18] = args;
    ctx.gpr[16] = argc;
    if (signed_of(argc) <= 0) return;
    ctx.gpr[19] = context;
    ctx.gpr[4] = context;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0893600Cu, {context});
    ctx.gpr[4] = args;  // the delay slot of the jal
    const std::uint32_t first = call_set(rt, ctx, 0x0883B848u, {args});
    const std::uint32_t second = ctx.gpr[3];
    (void)call_set(rt, ctx, 0x08935B10u, {context, ctx.gpr[5], first, second});
    const std::uint32_t r = call_set(rt, ctx, 0x08932988u, {context});
    ctx.gpr[17] = r;  // the delay slot of the bnez
    ctx.gpr[18] = buffer;
    if (r == 0u) {
        put8(rt, buffer + 1u, byte_u(rt, 0x08C9C460u + 1u));
        put8(rt, buffer, byte_u(rt, 0x08C9C460u));
    } else {
        bool plain = (argc != 1u);
        bool format_with_name = true;
        if (plain) {
            const std::uint32_t object = word(rt, args + 4u);
            ctx.gpr[16] = object;
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
            ctx.gpr[3] = word(rt, object);
            const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, object, 0x3Cu), {object});
            format_with_name = (answer != 0u);
        }
        ctx.gpr[18] = buffer;
        if (format_with_name) {
            ctx.gpr[4] = 0x08D6950Cu;
            const std::uint32_t name = call_set(rt, ctx, 0x08B48C30u, {0x08D6950Cu});
            ctx.gpr[8] = name;
            ctx.gpr[7] = r;
            ctx.gpr[6] = 0x08C9C464u;
            ctx.gpr[5] = 0x80u;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x80u, 0x08C9C464u, r, name});
        } else {
            ctx.gpr[6] = 0x08C9C46Cu;
            ctx.gpr[7] = r;
            ctx.gpr[5] = 0x80u;
            (void)call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x80u, 0x08C9C46Cu, r});
        }
    }
    format_ending(rt, ctx, frame, out, context, buffer);
}

// func_0883CB34(out, argc, args): the same set-up with func_089329E0(context) (zero copies the unaligned word at D_08C9C45C to sp + 0xC); otherwise func_08ABE514(r, 0, 2, sp + 0xC, 0x80) writes the string and, with argc >= 2 and the virtual slot 0x3C of args[1] (null: func_08A5BB7C(D_08C9BD48)) equal to 1,
// func_08B3767C(sp + 0xC, 0x80, D_08C9C470, sp + 0xC, func_08B48CB8(D_08D6950C)) appends. format_ending finishes.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(argc) <= 0) [the early return]`; `if (r == 0u)`; `if (argc != 1u)`; `if (object == 0u) [the call it guards]`; `if (answer == 1u)`
void func_0883CB34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x230u);
    SavedRegs saved(ctx);
    const std::uint32_t context = frame.at(0x90u), buffer = frame.at(0xCu);
    ctx.gpr[21] = out;
    ctx.gpr[17] = args;
    ctx.gpr[16] = argc;
    if (signed_of(argc) <= 0) return;
    ctx.gpr[19] = context;
    ctx.gpr[4] = context;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0893600Cu, {context});
    ctx.gpr[4] = args;  // the delay slot of the jal
    const std::uint32_t first = call_set(rt, ctx, 0x0883B848u, {args});
    const std::uint32_t second = ctx.gpr[3];
    (void)call_set(rt, ctx, 0x08935B10u, {context, ctx.gpr[5], first, second});
    const std::uint32_t r = call_set(rt, ctx, 0x089329E0u, {context});
    ctx.gpr[18] = buffer;  // the delay slot of the bnez
    if (r == 0u) {
        put_unaligned(rt, buffer, word_unaligned(rt, 0x08C9C45Cu));
    } else {
        ctx.gpr[20] = 1u;
        ctx.gpr[4] = r;
        ctx.gpr[5] = 0u;
        ctx.gpr[6] = 2u;
        ctx.gpr[7] = buffer;
        ctx.gpr[8] = 0x80u;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08ABE514u, {r, 0u, 2u, buffer, 0x80u});
        ctx.gpr[16] = frame.at(4u);  // the delay slot of the beq
        if (argc != 1u) {
            const std::uint32_t object = word(rt, args + 4u);
            ctx.gpr[16] = object;
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
            ctx.gpr[3] = word(rt, object);
            const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, object, 0x3Cu), {object});
            ctx.gpr[16] = frame.at(4u);  // the delay slot of the bne
            if (answer == 1u) {
                const std::uint32_t name = call_set(rt, ctx, 0x08B48CB8u, {0x08D6950Cu});
                ctx.gpr[8] = name;
                ctx.gpr[7] = buffer;  // the delay slot of the jal
                ctx.gpr[5] = 0x80u;
                (void)call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x80u, 0x08C9C470u, buffer, name});
            }
        }
    }
    format_ending(rt, ctx, frame, out, context, buffer);
}

// func_0883D494(out, argc, args): the same set-up with s = func_08936324(context) (a string): a string equal to D_08C9C460 (func_08B35FD8 == 0) copies the unaligned word at D_08C9C45C to sp + 0xC; otherwise with argc == 1 or a non-zero virtual slot 0x3C of args[1] (null: func_08A5BB7C(D_08C9BD48))
// func_08B3767C(sp + 0xC, 0x80, D_08C9C478, s) formats it; with that slot zero and q = func_08B7CA4C(s, D_08C9C48C) non-zero, the n = q - s bytes of s are copied to sp + 0xC with func_08B7C830 and the byte after them is zero; with q zero the same func_08B3767C. format_ending finishes.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(argc) <= 0) [the early return]`; `if (same == 0u)`; `if (argc != 1u)`; `if (object == 0u) [the call it guards]`; `if (answer == 0u)`; `if (found != 0u)`; `if (use_format)`
void func_0883D494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], argc = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x230u);
    SavedRegs saved(ctx);
    const std::uint32_t context = frame.at(0x90u), buffer = frame.at(0xCu);
    ctx.gpr[20] = out;
    ctx.gpr[18] = args;
    ctx.gpr[16] = argc;
    if (signed_of(argc) <= 0) return;
    ctx.gpr[19] = context;
    ctx.gpr[4] = context;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0893600Cu, {context});
    ctx.gpr[4] = args;  // the delay slot of the jal
    const std::uint32_t first = call_set(rt, ctx, 0x0883B848u, {args});
    const std::uint32_t second = ctx.gpr[3];
    (void)call_set(rt, ctx, 0x08935B10u, {context, ctx.gpr[5], first, second});
    const std::uint32_t text = call_set(rt, ctx, 0x08936324u, {context});
    ctx.gpr[5] = 0x08C9C460u;
    ctx.gpr[4] = text;
    ctx.gpr[17] = text;  // the delay slot of the jal
    const std::uint32_t same = call_set(rt, ctx, 0x08B35FD8u, {text, 0x08C9C460u});
    ctx.gpr[2] = 1u;  // the delay slot of the bnez
    ctx.gpr[18] = buffer;
    if (same == 0u) {
        put_unaligned(rt, buffer, word_unaligned(rt, 0x08C9C45Cu));
    } else {
        bool use_format = true;
        ctx.gpr[18] = buffer;  // the delay slot of the beql (taken)
        if (argc != 1u) {
            const std::uint32_t object = word(rt, args + 4u);
            ctx.gpr[16] = object;
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BD48u});
            ctx.gpr[3] = word(rt, object);
            const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, object, 0x3Cu), {object});
            ctx.gpr[18] = buffer;  // the delay slot of the bnez
            if (answer == 0u) {
                ctx.gpr[5] = 0x08C9C48Cu;
                const std::uint32_t found = call_set(rt, ctx, 0x08B7CA4Cu, {text, 0x08C9C48Cu});
                ctx.gpr[16] = found - text;  // the delay slot of the beqz
                if (found != 0u) {
                    ctx.gpr[18] = buffer;
                    ctx.gpr[5] = text;
                    (void)call_set(rt, ctx, 0x08B7C830u, {buffer, text, found - text});
                    ctx.gpr[3] = frame.at(0u) + (found - text);
                    put8(rt, buffer + (found - text), 0u);
                    use_format = false;
                }
            }
        }
        if (use_format) {
            ctx.gpr[18] = buffer;
            ctx.gpr[6] = 0x08C9C478u;
            ctx.gpr[7] = text;
            ctx.gpr[4] = buffer;
            ctx.gpr[5] = 0x80u;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08B3767Cu, {buffer, 0x80u, 0x08C9C478u, text});
        }
    }
    format_ending(rt, ctx, frame, out, context, buffer);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// release_pooled: the release of one counted object `s0` (its count is the word s0[0x10]) of the pool allocator behind func_08BB1F48 (a registry whose rows at registry[0x24 .. 0x28) are found by func_08BB1D0C with the comparator D_08BB1B40; registry[0x34] caches the row of the last size asked for):
// the count goes down by one and, when it reached zero, the block b = s0[0xC] goes back to the free list of the 4-byte row (row[0xC] = b, b[0] = the old head, row[0x1C] -= 1) when it is non-zero, s0[0xC] = 0, func_08BB1FF0(s0) runs, and s0 itself goes back to the free list of the 0x14-byte row the same way.
// Used by func_08BB2340 and func_08BB24B0 (two copies of one function: the words differ only in the operand order of two comparisons and in their own jump targets) and by func_08BB2660 (a pointer assignment that releases the old object).
inline void pool_push(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pool, std::uint32_t block) {
    const std::uint32_t count = word(rt, pool + 0x1Cu);
    ctx.gpr[2] = count;  // the delay slot of the bnel
    const std::uint32_t head = word(rt, pool + 0xCu);
    ctx.gpr[3] = head;
    ctx.gpr[2] = count - 1u;
    put(rt, pool + 0x1Cu, count - 1u);
    put(rt, block, head);
    put(rt, pool + 0xCu, block);
}
inline std::uint32_t pool_row(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, std::uint32_t size) {
    const std::uint32_t registry = call_keep(rt, ctx, 0x08BB1F48u, {});
    ctx.gpr[17] = registry;
    ctx.gpr[3] = size;
    put(rt, frame.at(0u), size);
    std::uint32_t pool = word(rt, registry + 0x34u);
    ctx.gpr[4] = pool;
    if (pool == 0u || word(rt, pool + 0x10u) != size) {
        ctx.gpr[4] = word(rt, registry + 0x24u);
        const std::uint32_t found = call_set(rt, ctx, 0x08BB1D0Cu, {word(rt, registry + 0x24u), word(rt, registry + 0x28u), frame.at(0u), 0x08BB1B40u});
        pool = word(rt, found);
        ctx.gpr[4] = pool;
        put(rt, registry + 0x34u, pool);
    }
    return pool;
}
inline void release_pooled(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, std::uint32_t s0) {
    const std::uint32_t count = word(rt, s0 + 0x10u) - 1u;
    ctx.gpr[2] = count;
    put(rt, s0 + 0x10u, count);  // the delay slot of the bnez
    if (count != 0u) return;
    const std::uint32_t block = word(rt, s0 + 0xCu);
    ctx.gpr[19] = block;  // the delay slot of the jal
    const std::uint32_t pool = pool_row(rt, ctx, frame, 4u);
    if (block != 0u) pool_push(rt, ctx, pool, block);
    ctx.gpr[4] = s0;
    put(rt, s0 + 0xCu, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BB1FF0u, {s0});
    const std::uint32_t second = pool_row(rt, ctx, frame, 0x14u);
    pool_push(rt, ctx, second, s0);
}
inline void release_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t begin = ctx.gpr[4], end = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[20] = end;
    ctx.gpr[18] = begin;
    if (begin == end) return;
    ctx.gpr[21] = 0x003B0000u;
    std::uint32_t cursor = begin;
    for (;;) {
        const std::uint32_t s0 = word(rt, cursor);
        ctx.gpr[16] = s0;
        if (s0 != 0u) release_pooled(rt, ctx, frame, s0);
        cursor += 4u;
        ctx.gpr[18] = cursor;
        if (cursor == end) break;
    }
}

// func_08BB2340(begin, end) and func_08BB24B0(begin, end): release_pooled on every non-null pointer of the array [begin, end).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_08BB2340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_range(rt, ctx); }
void func_08BB24B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_range(rt, ctx); }

// func_08BB2660(this, other): n = *other (when non-zero n[0x10] is incremented); o = this[0]; this[0] = n (stored even when o is null); a non-null o is released with release_pooled. v0 = this.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): none: every branch and region of the compared cases ran.
void func_08BB2660(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t incoming = word(rt, other);
    ctx.gpr[5] = incoming;
    ctx.gpr[18] = self;  // the delay slot of the beqz
    if (incoming != 0u) {
        const std::uint32_t count = word(rt, incoming + 0x10u) + 1u;
        ctx.gpr[2] = count;
        put(rt, incoming + 0x10u, count);
    }
    const std::uint32_t old = word(rt, self);
    ctx.gpr[16] = old;
    put(rt, self, incoming);  // the delay slot of the beqz
    if (old != 0u) release_pooled(rt, ctx, frame, old);
    ctx.gpr[2] = self;
}

// func_08BB13D4(vector, position, value): insert *value before `position` in a vector of words {begin, end, capacity}. With room (end != capacity): a non-null end gets end[0] = end[-4], the end becomes end + 4 and func_08B79BAC(end_old - n, position, n) with n = ((end_old - 4 - position) & ~3) moves the tail up, then *position = *value.
// Without room: size = (end - begin) >> 2 (0x3FFFFFFF calls func_08B5ADDC(D_08CA0A30) first); the new capacity in words is 1 for an empty vector and size * 2 otherwise (-4 bytes when that overflowed, 0 bytes when it is 0), block = func_08B35DA0(bytes); e = func_08BB13A4(begin, position, block, sp + 2); a non-null e gets *e = *value;
// n = func_08BB13A4(position, end, e + 4, sp + 1); func_08BB1314(begin, end, sp) destroys the old range; a non-null begin is freed with func_08B35254; the vector becomes {block, n, block + bytes}. v0 = block + bytes.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (end != word(rt, vector + 8u))`; `if (end != 0u)`; `if (size == 0x3FFFFFFFu)`; `if (size == 0u)`; `the else branch of the test above`; `if (doubled < size)`; `if (doubled == 0u) allocate = false`; `else bytes = doubled << 2`; `if (allocate)`; `if (slot != 0u) put(rt, slot, word(rt, value))`; `if (old_begin != 0u)`
void func_08BB13D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = value;
    ctx.gpr[17] = position;
    ctx.gpr[16] = vector;
    ctx.gpr[3] = word(rt, vector + 4u);
    ctx.gpr[2] = word(rt, vector + 8u);
    std::uint32_t end = ctx.gpr[3];
    if (end != word(rt, vector + 8u)) {
        ctx.gpr[4] = end;
        if (end != 0u) {
            ctx.gpr[2] = word(rt, end - 4u);
            put(rt, end, ctx.gpr[2]);
            end = word(rt, vector + 4u);
            ctx.gpr[3] = end;
            ctx.gpr[4] = end;
        }
        std::uint32_t bytes = ((end - 4u) - position) & ~3u;
        ctx.gpr[6] = bytes;
        ctx.gpr[3] = end + 4u;
        put(rt, vector + 4u, end + 4u);
        const std::uint32_t kept = word(rt, value);
        ctx.gpr[16] = kept;
        ctx.gpr[4] = end - bytes;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08B79BACu, {end - bytes, position, bytes});
        put(rt, position, kept);  // the delay slot of the j
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    ctx.gpr[4] = begin;
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
    ctx.gpr[3] = size;
    ctx.gpr[2] = 0x3FFFFFFFu;
    if (size == 0x3FFFFFFFu) {
        (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0A30u});
    }
    std::uint32_t doubled = 1u, bytes, block = 0u;
    ctx.gpr[5] = 1u;  // the delay slot of the beqz
    bool allocate = true;
    if (size == 0u) {
        bytes = doubled << 2;
    } else {
        doubled = size << 1;
        ctx.gpr[5] = doubled;
        ctx.gpr[2] = doubled < size ? 1u : 0u;
        if (doubled < size) {
            bytes = 0xFFFFFFFCu;
        } else {
            ctx.gpr[19] = 0u;
            bytes = 0u;
            if (doubled == 0u) allocate = false;
            else bytes = doubled << 2;
        }
    }
    ctx.gpr[20] = bytes;
    if (allocate) {
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[4] = word(rt, vector);
        ctx.gpr[19] = block;
    }
    ctx.gpr[6] = block;
    ctx.gpr[5] = position;
    const std::uint32_t slot = call_set(rt, ctx, 0x08BB13A4u, {word(rt, vector), position, block, frame.at(2u)});
    ctx.gpr[6] = slot;  // the delay slot of the beqz
    if (slot != 0u) put(rt, slot, word(rt, value));
    const std::uint32_t last = call_set(rt, ctx, 0x08BB13A4u, {position, word(rt, vector + 4u), slot + 4u, frame.at(1u)});
    ctx.gpr[17] = last;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08BB1314u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_begin = word(rt, vector);
    ctx.gpr[4] = old_begin;
    ctx.gpr[2] = block + bytes;  // the delay slot of the beqz
    if (old_begin != 0u) {
        (void)call_set(rt, ctx, 0x08B35254u, {old_begin});
        ctx.gpr[2] = block + bytes;
    }
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, last);
    put(rt, vector, block);
}

// func_08C3EBD8(vector, position, value): insert the 8-byte element *value (a word and a float) before `position` in a vector of 8-byte elements {begin, end, capacity}. With room (end != capacity): a non-null end gets *end = end[-1] (two words); the end grows by 8 and the n = (end_old - 8 - position) / 8 elements below the old last one move up
// one slot (backwards, one element per step), then *position = *value (the float is loaded into f0 and stored back, bit for bit). Without room: size = (end - begin) / 8 (0x1FFFFFFF calls func_08B5ADDC(D_08CB4A90) first); the new capacity is 1 for an empty vector and size * 2 otherwise (-8 bytes when that overflowed, 0 bytes when it is 0), block = func_08B35DA0(bytes);
// e = func_08C3EB40(begin, position, block, sp + 2), a non-null e gets *e = *value; n = func_08C3EB40(position, end, e + 8, sp + 1); func_08BD319C(begin, end, sp) destroys the old range; a non-null begin is freed with func_08B35254; the vector becomes {block, n, block + bytes}. v0 = block + bytes.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (end != word(rt, vector + 8u))`; `if (end != 0u)`; `if (signed_of(moves) > 0)`; `for (std::uint32_t i = 0u; i < moves; ++i)`; `if (size == 0x1FFFFFFFu) [the call it guards]`; `if (doubled < size)`; `if (doubled == 0u) allocate = false`; `if (allocate)`; `if (slot != 0u)`
void func_08C3EBD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = position;
    ctx.gpr[17] = value;
    ctx.gpr[16] = vector;
    std::uint32_t end = word(rt, vector + 4u);
    ctx.gpr[7] = end;
    ctx.gpr[2] = word(rt, vector + 8u);
    if (end != word(rt, vector + 8u)) {
        if (end != 0u) {
            ctx.gpr[2] = end - 8u;
            const std::uint32_t high = word(rt, end - 4u), low = word(rt, end - 8u);
            ctx.gpr[2] = high;
            ctx.gpr[3] = low;
            put(rt, end + 4u, high);
            put(rt, end, low);
            end = word(rt, vector + 4u);
            ctx.gpr[7] = end;
        }
        const std::uint32_t last = end - 8u;
        ctx.gpr[5] = last;
        ctx.gpr[2] = last - position;
        put(rt, vector + 4u, end + 8u);
        ctx.gpr[7] = static_cast<std::uint32_t>(signed_of(last - position) >> 3);
        const std::uint32_t moves = ctx.gpr[7];
        const std::uint32_t first_word = word(rt, value);
        ctx.fpr[0] = float_at(rt, value + 4u);
        ctx.gpr[8] = first_word;
        if (signed_of(moves) > 0) {
            std::uint32_t to = last, from = last;
            for (std::uint32_t i = 0u; i < moves; ++i) {
                from -= 8u;
                const std::uint32_t low = word(rt, from), high = word(rt, from + 4u);
                ctx.gpr[2] = low;
                ctx.gpr[3] = high;
                put(rt, to, low);
                put(rt, to + 4u, high);
                to -= 8u;
            }
        }
        put(rt, position, first_word);
        put_float(rt, position + 4u, ctx.fpr[0]);
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    ctx.gpr[4] = begin;
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 3);
    ctx.gpr[3] = size;
    ctx.gpr[2] = 0x1FFFFFFFu;
    if (size == 0x1FFFFFFFu) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4A90u});
    std::uint32_t doubled = 1u, bytes, block = 0u;
    ctx.gpr[5] = 1u;  // the delay slot of the beqz
    bool allocate = true;
    if (size == 0u) {
        bytes = doubled << 3;
    } else {
        doubled = size << 1;
        ctx.gpr[5] = doubled;
        ctx.gpr[2] = doubled < size ? 1u : 0u;
        if (doubled < size) {
            bytes = 0xFFFFFFF8u;
        } else {
            ctx.gpr[19] = 0u;
            bytes = 0u;
            if (doubled == 0u) allocate = false;
            else bytes = doubled << 3;
        }
    }
    ctx.gpr[20] = bytes;
    if (allocate) {
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[4] = word(rt, vector);
        ctx.gpr[19] = block;
    }
    ctx.gpr[6] = block;
    ctx.gpr[5] = position;
    const std::uint32_t slot = call_set(rt, ctx, 0x08C3EB40u, {word(rt, vector), position, block, frame.at(2u)});
    ctx.gpr[6] = slot;  // the delay slot of the beqz
    if (slot != 0u) {
        const std::uint32_t high = word(rt, value + 4u), low = word(rt, value);
        ctx.gpr[2] = high;
        ctx.gpr[3] = low;
        put(rt, slot + 4u, high);
        put(rt, slot, low);
    }
    const std::uint32_t tail = call_set(rt, ctx, 0x08C3EB40u, {position, word(rt, vector + 4u), slot + 8u, frame.at(1u)});
    ctx.gpr[17] = tail;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08BD319Cu, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_begin = word(rt, vector);
    ctx.gpr[4] = old_begin;
    ctx.gpr[2] = block + bytes;  // the delay slot of the beqz
    if (old_begin != 0u) {
        (void)call_set(rt, ctx, 0x08B35254u, {old_begin});
        ctx.gpr[2] = block + bytes;
    }
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, tail);
    put(rt, vector, block);
}

// func_0888AD94(this, index, src): e = this + 168 * index + 0x12B8 (a 168-byte entry). With the byte src[0x12] clear and the halfword src[0x14] below 2 it returns v0 = 1. n = 0x10 when src[0x12] is set, else the byte src[0x13]; the byte sp[0] = src[0x13]. For k below n (none when n <= 0): s = src[0x28 + 4k] and p = func_0888A998(this, e[0x28 + 4k]):
// e[0x28 + 4k] = s, e[0x68 + 4k] = src[0x68 + 4k], e[0x18] = src[0x18] and e[0x1C] = src[0x1C] when p is zero, or when s differs from e[0x28 + 4k] and the byte this[0x1C35] differs from (s < e[0x28 + 4k], unsigned). Then the 64-bit sum e[0x20 / 0x24] = 0 + sum of e[0x28 + 4k] over the k = 0..15 while func_0888A998(this, e[0x28 + 4k]) is non-zero (the sum stops at the first zero);
// last, the 16 bytes src[0..0xF] are copied (unaligned) to e, e[0x11] = 1, e[0x12] = 1, e[0x13] = sp[0], e[0x10] = src[0x10], the halfword e[0x14] = 0.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_0888AD94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5], src = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u);
    ctx.gpr[21] = src;
    const std::uint32_t flag = byte_u(rt, src + 0x12u);
    ctx.gpr[6] = flag;
    ctx.gpr[23] = self;  // the delay slot of the bnez
    if (flag == 0u) {
        const std::uint32_t count = sext16(half_u(rt, src + 0x14u));
        ctx.gpr[2] = signed_of(count) < 2 ? 1u : 0u;
        if (signed_of(count) < 2) return;
    }
    const std::uint32_t entry = self + 168u * index + 0x12B8u;
    ctx.gpr[17] = entry;
    ctx.gpr[3] = self + 168u * index;
    std::uint32_t limit;
    ctx.gpr[20] = src;
    if (flag != 0u) {
        const std::uint32_t v0 = byte_u(rt, src + 0x13u);
        ctx.gpr[30] = 0x10u;
        ctx.gpr[2] = v0;
        put8(rt, p0, v0);  // the delay slot of the j
        limit = 0x10u;
    } else {
        const std::uint32_t v1 = byte_u(rt, src + 0x13u);
        limit = v1 & 0xFFu;
        ctx.gpr[30] = limit;
        ctx.gpr[3] = v1;
        put8(rt, p0, v1);  // the delay slot of the blez
    }
    if (signed_of(limit) > 0) {
        std::uint32_t source = src, target = entry;
        std::uint32_t k = 0u;
        do {
            const std::uint32_t s0 = word(rt, target + 0x28u);
            ctx.gpr[16] = s0;
            k += 1u;
            ctx.gpr[22] = k;
            const std::uint32_t s2 = word(rt, source + 0x28u);  // the delay slot of the jal
            ctx.gpr[18] = s2;
            const std::uint32_t present = call_set(rt, ctx, 0x0888A998u, {self, s0});
            bool update = (present == 0u);
            if (!update && s2 != s0) {
                const std::uint32_t below = s2 < s0 ? 1u : 0u;
                const std::uint32_t mode = byte_u(rt, self + 0x1C35u);
                ctx.gpr[2] = mode;
                update = (mode != below);
            }
            if (update) {
                const std::uint32_t v0 = word(rt, src + 0x18u), v1 = word(rt, src + 0x1Cu);
                const std::uint32_t a0 = word(rt, source + 0x68u);
                put(rt, target + 0x28u, s2);
                put(rt, target + 0x68u, a0);
                put(rt, entry + 0x18u, v0);
                put(rt, entry + 0x1Cu, v1);
            }
            source += 4u;
            target += 4u;
            ctx.gpr[20] = source;
            ctx.gpr[19] = target;
        } while (limit != k);
    }
    put(rt, entry + 0x20u, 0u);
    put(rt, entry + 0x24u, 0u);
    std::uint32_t sum_low = 0u, sum_high = 0u;
    std::uint32_t cursor = entry;
    for (;;) {
        const std::uint32_t s2 = word(rt, cursor + 0x28u);
        ctx.gpr[18] = s2;
        cursor += 4u;
        ctx.gpr[16] = cursor;
        const std::uint32_t present = call_set(rt, ctx, 0x0888A998u, {self, s2});
        ctx.gpr[9] = 0u;  // the delay slot of the beqz
        if (present == 0u) break;
        const std::uint32_t low = word(rt, entry + 0x20u), high = word(rt, entry + 0x24u);
        const std::uint32_t total = low + s2;
        const std::uint32_t carry = total < low ? 1u : 0u;
        sum_low = total;
        sum_high = high + 0u + carry;
        put(rt, entry + 0x20u, sum_low);
        put(rt, entry + 0x24u, sum_high);
        if (cursor == entry + 0x40u) break;
    }
    put_unaligned(rt, entry, word_unaligned(rt, src));
    put_unaligned(rt, entry + 4u, word_unaligned(rt, src + 4u));
    put_unaligned(rt, entry + 8u, word_unaligned(rt, src + 8u));
    put_unaligned(rt, entry + 0xCu, word_unaligned(rt, src + 0xCu));
    put8(rt, entry + 0x11u, 1u);
    put8(rt, entry + 0x13u, byte_u(rt, p0));
    put8(rt, entry + 0x12u, 1u);
    put16(rt, entry + 0x14u, 0u);
    put8(rt, entry + 0x10u, byte_u(rt, src + 0x10u));
}

// func_08ACCFF8(this, k, limit, flag): row = this + 236 * k (the byte row[0x41D] zero returns v0 = 0); under the lock s5 = row + 0x334 (func_08B34888 .. func_08B34B24) the list s3 = s5[0xAC] has n = s3[0xC] entries e(i) = func_08B24AB8(s3, i); a scan i = 0..n-1 (n = 0 skips it) leaves the entry `found`:
// with flag zero an entry whose e[0xC] is not below `limit` (unsigned) becomes found with e[0x10] = 1 and ends the scan; the other entries (and, with flag non-zero, every entry) have their counter e[0x10] bumped when it is non-zero (e[0x10] = 0 when the bumped value reaches 4). Without a found entry and with n != 0 the last entry e(n - 1) becomes found with e[0x10] = 1. Then the first i = 0.. with e(i)[0x10] != 0 (the scan stops at n)
// is looked for; when i != 0: func_08B24A60(s3, i) and func_08B34C1C(s5). Unlock; v0 = found.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (active == 0u)`; `if (count != 0u)`; `while (true)`; `if (flag != 0u)`; `if (counter != 0u)`; `if (!(bumped < 4u)) put(rt, entry + 0x10u, 0u)`; `if (i == count) break`; `if (!ended) found = 0u`; `if (found == 0u)`; `if (counter != 0u) break`; `i += 1u`; `if (!(i < count)) break`; `if (i != 0u)`
void func_08ACCFF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], k = ctx.gpr[5], limit = ctx.gpr[6], flag = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t row = self + 236u * k;
    ctx.gpr[17] = flag;
    ctx.gpr[16] = limit;
    std::uint32_t found = 0u;
    ctx.gpr[20] = 0u;
    const std::uint32_t active = byte_u(rt, row + 0x41Du);
    ctx.gpr[2] = active;
    if (active == 0u) {
        ctx.gpr[2] = 0u;  // the delay slot of the beqz
        return;
    }
    const std::uint32_t lock = row + 0x334u;
    ctx.gpr[21] = lock;
    const std::uint32_t list = word(rt, lock + 0xACu);
    ctx.gpr[19] = list;
    (void)call_set(rt, ctx, 0x08B34888u, {lock});
    const std::uint32_t count = word(rt, list + 0xCu);
    ctx.gpr[18] = count;
    ctx.gpr[20] = 0u;  // the delay slot of the beql (taken)
    if (count != 0u) {
        ctx.gpr[22] = flag != 0u ? 1u : 0u;
        std::uint32_t i = 0u;
        bool ended = false;
        while (true) {
            ctx.gpr[5] = i;
            const std::uint32_t entry = call_set(rt, ctx, 0x08B24AB8u, {list, i});
            ctx.gpr[4] = entry;
            const std::uint32_t used = word(rt, entry + 0xCu);
            ctx.gpr[2] = used;
            i += 1u;
            ctx.gpr[20] = i;
            std::uint32_t counter;
            if (flag != 0u) {
                counter = word(rt, entry + 0x10u);
                ctx.gpr[2] = counter;
            } else {
                const std::uint32_t below = used < limit ? 1u : 0u;
                ctx.gpr[2] = below;
                if (below != 0u) {
                    counter = word(rt, entry + 0x10u);
                    ctx.gpr[2] = counter;
                } else {
                    ctx.gpr[2] = 1u;
                    found = entry;
                    ctx.gpr[20] = entry;
                    put(rt, entry + 0x10u, 1u);  // the delay slot of the j
                    ended = true;
                    break;
                }
            }
            const std::uint32_t bumped = counter + 1u;
            ctx.gpr[3] = bumped;
            ctx.gpr[5] = bumped < 4u ? 1u : 0u;
            if (counter != 0u) {
                put(rt, entry + 0x10u, bumped);  // the delay slot of the bnez
                if (!(bumped < 4u)) put(rt, entry + 0x10u, 0u);
            }
            ctx.gpr[5] = i;  // the delay slot of the bne
            if (i == count) break;
        }
        if (!ended) found = 0u;
        ctx.gpr[20] = found;
    }
    ctx.gpr[16] = 0u;  // the delay slot of the bnez
    if (count != 0u) {
        if (found == 0u) {
            ctx.gpr[5] = count - 1u;
            const std::uint32_t last = call_set(rt, ctx, 0x08B24AB8u, {list, count - 1u});
            ctx.gpr[3] = 1u;
            put(rt, last + 0x10u, 1u);
            found = last;
            ctx.gpr[20] = last;
            ctx.gpr[16] = 0u;
        }
        std::uint32_t i = 0u;
        for (;;) {
            ctx.gpr[16] = i;
            ctx.gpr[5] = i;
            const std::uint32_t entry = call_set(rt, ctx, 0x08B24AB8u, {list, i});
            const std::uint32_t counter = word(rt, entry + 0x10u);
            ctx.gpr[3] = counter;
            if (counter != 0u) break;
            i += 1u;
            ctx.gpr[16] = i;
            ctx.gpr[2] = i < count ? 1u : 0u;
            ctx.gpr[5] = i;  // the delay slot of the bnez
            if (!(i < count)) break;
        }
        ctx.gpr[4] = list;  // the delay slot of the beqz
        if (i != 0u) {
            ctx.gpr[5] = i;
            (void)call_set(rt, ctx, 0x08B24A60u, {list, i});
            ctx.gpr[4] = lock;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08B34C1Cu, {lock});
        }
    }
    ctx.gpr[4] = lock;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B34B24u, {lock});
    ctx.gpr[2] = found;
}

// func_088A26E0(this, key, args): a non-zero args[0] goes to func_088A258C(this[0x3C], key, args[0]). A slot is taken from the pool: the first k below the count this[0x14] whose flag byte this[0xC][k] is zero (none, or a count of 0 or less, returns v0 = 0): the flag becomes 1, the object o = this[8] + 1072 * k is the result, the halfword this[0x10][key] = k;
// a non-zero args[4] goes to func_088A403C(o, args[4]); the virtual slot 0x30 of o (o, 0) runs; func_088A6068(o + 8, this[0x4C] + 1680 * k) runs; with the byte args[9] zero func_088A3710(o, this[0x60] + 20 * k) runs; with the byte this[0x18] set func_088A2364(this[0x3C], key) runs; this[0x1C] is incremented; v0 = o.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first != 0u)`; `if (signed_of(count) <= 0)`; `while (k < count && byte_u(rt, flags + k) != 0u) k += 1u`; `if (k == count)`; `if (object == 0u)`; `if (extra != 0u) [the call it guards]`; `if (quiet != 0u)`; `the else branch of the test above`; `if (mode != 0u)`
void func_088A26E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = key;
    ctx.gpr[19] = args;
    const std::uint32_t first = word(rt, args);
    ctx.gpr[6] = first;
    ctx.gpr[18] = self;  // the delay slot of the beqz
    if (first != 0u) {
        ctx.gpr[4] = word(rt, self + 0x3Cu);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x088A258Cu, {word(rt, self + 0x3Cu), key, first});
    }
    const std::uint32_t count = word(rt, self + 0x14u);
    ctx.gpr[4] = count;
    ctx.gpr[17] = 0u;  // the delay slot of the j
    if (signed_of(count) <= 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t flags = word(rt, self + 0xCu);
    ctx.gpr[5] = flags;
    std::uint32_t k = 0u;
    while (k < count && byte_u(rt, flags + k) != 0u) k += 1u;
    ctx.gpr[16] = k;
    if (k == count) {
        ctx.gpr[2] = 0u;
        ctx.gpr[17] = 0u;
        return;
    }
    const std::uint32_t offset = 1072u * k;
    ctx.gpr[6] = offset;
    ctx.gpr[7] = sext16(k);
    ctx.gpr[2] = 1u;
    const std::uint32_t pool = word(rt, self + 8u);
    ctx.gpr[4] = pool;
    put8(rt, flags + k, 1u);
    const std::uint32_t object = pool + offset;
    ctx.gpr[17] = object;
    const std::uint32_t table = word(rt, self + 0x10u);
    ctx.gpr[2] = table;
    put16(rt, table + (key << 1), k);  // the delay slot of the bnez
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t extra = word(rt, args + 4u);
    ctx.gpr[5] = extra;
    if (extra != 0u) (void)call_set(rt, ctx, 0x088A403Cu, {object, extra});
    ctx.gpr[2] = word(rt, object);
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;
    const std::uint32_t slot = slot_of(rt, object, 0x30u);
    ctx.gpr[3] = slot;
    ctx.gpr[20] = k << 4;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {object, 0u});
    const std::uint32_t v1 = word(rt, self + 0x4Cu);
    ctx.gpr[3] = v1;
    const std::uint32_t source = v1 + 1680u * k;
    ctx.gpr[5] = source;
    ctx.gpr[4] = object + 8u;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088A6068u, {object + 8u, source});
    const std::uint32_t quiet = byte_u(rt, args + 9u);
    ctx.gpr[2] = quiet;
    std::uint32_t mode;
    if (quiet != 0u) {
        mode = byte_u(rt, self + 0x18u);  // the delay slot of the bnel (taken)
        ctx.gpr[2] = mode;
    } else {
        const std::uint32_t base = word(rt, self + 0x60u);
        ctx.gpr[2] = base;
        const std::uint32_t target = base + 20u * k;
        ctx.gpr[5] = target;
        ctx.gpr[4] = object;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x088A3710u, {object, target});
        mode = byte_u(rt, self + 0x18u);
        ctx.gpr[2] = mode;
    }
    if (mode != 0u) {
        ctx.gpr[4] = word(rt, self + 0x3Cu);
        ctx.gpr[5] = key;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x088A2364u, {word(rt, self + 0x3Cu), key});
    }
    ctx.gpr[2] = word(rt, self + 0x1Cu);
    ctx.gpr[2] += 1u;
    put(rt, self + 0x1Cu, ctx.gpr[2]);  // the delay slot of the j
    ctx.gpr[2] = object;
}

// func_088738FC(this): with e(p) = the word at p + 8 * byte p[0x25] + 0x14: this[8] non-zero, or e(this[0x14]) non-zero with its word [8] non-zero, calls func_08AF7CB8() and returns. Otherwise E = e(this[0xC]) gives w = (E non-zero and its byte [0x2C] set) ? E[8] : 0, s0 = w[0x10] and v1 = w[0xC] (0 when w is 0). base = this[4] or, when that is zero, E[0x10] (E non-zero with the byte [0x2C] set,
// else s0 = 0). With s0 > 0 and v1 > 0 the quotient q = v1 / s0 (signed) gives n = min(q, 4) calls func_08AF7C8C(i, base + (s0 * s0 * i) / 2, s0, s0, 8) for i = 0..n-1 (v0 = 0 afterwards); otherwise four calls func_08AF7C8C(i, 0, 0, 0, 8) for i = 0..3.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `const auto entry_of = [&](std::uint32_t p) { return word(rt, p + 8u * byte_u(rt, p + 0x25u) + 0x14u); }`; `if (!early)`; `if (a0 != 0u)`; `if (early)`; `const std::uint32_t v1_first = word(rt, self + 0xCu)`; `if (entry != 0u && byte_u(rt, entry + 0x2Cu) != 0u) s0 = word(rt, word(rt, entry + 8u) + 0x10u)`; `if (second != 0u && byte_u(rt, second + 0x2Cu) != 0u)`; `if (base == 0u)`; `if (third == 0u || byte_u(rt, third + 0x2Cu) == 0u)`; `the else branch of the test above`; `if (!four && signed_of(s0) <= 0)`; `if (!four)`; `if (signed_of(v1) <= 0)`; `for (;;)`; and 4 more of the same kind
void func_088738FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u);
    ctx.gpr[5] = self;
    const auto entry_of = [&](std::uint32_t p) { return word(rt, p + 8u * byte_u(rt, p + 0x25u) + 0x14u); };
    std::uint32_t a0 = word(rt, self + 8u);
    ctx.gpr[4] = a0;
    bool early = (a0 != 0u);
    if (!early) {
        const std::uint32_t v1 = word(rt, self + 0x14u);
        ctx.gpr[3] = v1;
        a0 = entry_of(v1);
        ctx.gpr[4] = a0;
        if (a0 != 0u) {
            a0 = word(rt, a0 + 8u);
            ctx.gpr[4] = a0;
            early = (a0 != 0u);
        }
    }
    if (early) {
        (void)call_set(rt, ctx, 0x08AF7CB8u, {});
        return;
    }
    const std::uint32_t v1_first = word(rt, self + 0xCu);  // the delay slot of the beql
    ctx.gpr[3] = v1_first;
    const std::uint32_t entry = entry_of(v1_first);
    ctx.gpr[3] = entry;
    ctx.gpr[4] = self + 0xCu;
    std::uint32_t s0 = 0u;
    if (entry != 0u && byte_u(rt, entry + 0x2Cu) != 0u) s0 = word(rt, word(rt, entry + 8u) + 0x10u);
    ctx.gpr[16] = s0;
    const std::uint32_t head = word(rt, self + 0xCu);
    ctx.gpr[4] = head;
    const std::uint32_t second = entry_of(head);
    std::uint32_t v1 = 0u;
    ctx.gpr[3] = 0u;
    if (second != 0u && byte_u(rt, second + 0x2Cu) != 0u) {
        v1 = word(rt, word(rt, second + 8u) + 0xCu);
        ctx.gpr[3] = v1;
    }
    std::uint32_t base = word(rt, self + 4u);
    ctx.gpr[19] = base;
    bool four = false;
    if (base == 0u) {
        const std::uint32_t third = entry_of(head);
        ctx.gpr[4] = third;
        if (third == 0u || byte_u(rt, third + 0x2Cu) == 0u) {
            s0 = 0u;  // the delay slot of the beql (taken)
            four = true;
        } else {
            base = word(rt, third + 0x10u);
            ctx.gpr[19] = base;
            if (base == 0u) {
                s0 = 0u;
                four = true;
            }
        }
    }
    ctx.gpr[16] = s0;
    if (!four && signed_of(s0) <= 0) {
        s0 = 0u;  // the delay slot of the blezl (taken)
        ctx.gpr[16] = s0;
        four = true;
    }
    if (!four) {
        ctx.gpr[17] = 0u;  // the delay slot of the blez
        if (signed_of(v1) <= 0) {
            s0 = 0u;
            ctx.gpr[16] = s0;
            four = true;
        }
    }
    if (!four) {
        const std::int32_t quotient = signed_of(v1) / signed_of(s0);
        ctx.gpr[22] = 4u;
        ctx.gpr[21] = static_cast<std::uint32_t>(quotient);
        ctx.gpr[18] = 0u;
        const bool small = quotient < 4;
        ctx.gpr[20] = small ? 1u : 0u;
        std::uint32_t s1 = 0u, s2 = 0u;
        put(rt, p4, 4u);
        put(rt, p0, static_cast<std::uint32_t>(quotient));
        for (;;) {
            const std::uint32_t chosen = small ? static_cast<std::uint32_t>(quotient) : 4u;
            ctx.gpr[4] = s1;  // a0 = s1 before the increment
            ctx.gpr[6] = s0;
            ctx.gpr[7] = s0;
            ctx.gpr[8] = 8u;
            ctx.gpr[2] = signed_of(s1) < signed_of(chosen) ? 1u : 0u;
            const std::uint32_t this_index = s1;
            s1 += 1u;
            ctx.gpr[17] = s1;  // the delay slot of the bnez
            if (ctx.gpr[2] == 0u) return;
            const std::uint32_t product = s0 * s2;
            s2 += s0;
            ctx.gpr[18] = s2;
            const std::uint32_t half = static_cast<std::uint32_t>(signed_of(product + (product >> 31)) >> 1);
            ctx.gpr[5] = base + half;
            ctx.gpr[2] = product;
            (void)call_set(rt, ctx, 0x08AF7C8Cu, {this_index, base + half, s0, s0, 8u});
        }
    }
    ctx.gpr[17] = 4u;
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        ctx.gpr[16] = i + 1u;
        (void)call_set(rt, ctx, 0x08AF7C8Cu, {i, 0u, 0u, 0u, 8u});
    }
}

// func_088D7794(this, t): a negative t is replaced by func_088D5E70(this + 4); func_088DAA84(this) non-zero returns 0 and func_088DAAC4(this) non-zero returns 1. When this[0x640] is non-zero func_088E5300() must be non-zero (zero returns 0). The object o = this[0x620] and the word func_088D6358(this + 4) go to q = func_088D2A74(o, ·): a non-zero q clears t;
// with this[0x640] non-zero and the halfword this[0x130][0xA] at least 2, t += 60000 and the next step is skipped; otherwise func_088D6358(this + 4) == 2 leaves things as they are and any other value runs func_088DC7C0(this, 1). The 64-bit time func_088D9C70(this) plus t (sign-extended) is stored at this[0x130][0x38] / [0x3C], the words [0x40] / [0x44] are cleared,
// func_088D776C(this, 0, 0, 0) runs after the byte this[0x1D] = 0, and 1 is returned.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(t) < 0)`; `if (blocked != 0u)`; `if (done != 0u)`; `if (flag != 0u)`; `if (ready == 0u)`; `if (known != 0u)`; `if (again != 0u)`; `if (!(half < 2u))`; `if (!skip)`; `if (state == 2u)`; `const std::uint32_t upper = (sum < low ? 1u : 0u) + (high + sign)`
void func_088D7794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    std::uint32_t t = ctx.gpr[5];
    ctx.gpr[18] = t;
    ctx.gpr[17] = self;
    if (signed_of(t) < 0) {
        ctx.gpr[4] = self + 4u;  // the delay slot of the jal
        t = call_set(rt, ctx, 0x088D5E70u, {self + 4u});
        ctx.gpr[18] = t;
    }
    ctx.gpr[4] = self;
    const std::uint32_t blocked = call_set(rt, ctx, 0x088DAA84u, {self});
    ctx.gpr[3] = 0u;  // the delay slot of the bnez
    if (blocked != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t done = call_set(rt, ctx, 0x088DAAC4u, {self});
    ctx.gpr[3] = 1u;
    if (done != 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    std::uint32_t s0;
    const std::uint32_t flag = word(rt, self + 0x640u);
    ctx.gpr[4] = flag;
    if (flag != 0u) {
        const std::uint32_t ready = call_set(rt, ctx, 0x088E5300u, {});
        ctx.gpr[3] = 0u;  // the delay slot of the beqz
        if (ready == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    s0 = word(rt, self + 0x620u);
    ctx.gpr[16] = s0;
    const std::uint32_t s3 = self + 4u;
    ctx.gpr[19] = s3;
    const std::uint32_t kind = call_set(rt, ctx, 0x088D6358u, {s3});
    ctx.gpr[5] = kind;
    const std::uint32_t known = call_set(rt, ctx, 0x088D2A74u, {s0, kind});
    const std::uint32_t again = word(rt, self + 0x640u);
    ctx.gpr[3] = again;
    if (known != 0u) {  // the movn in the delay slot of the beqz
        t = 0u;
        ctx.gpr[18] = t;
    }
    bool skip = false;
    if (again != 0u) {
        s0 = word(rt, self + 0x130u);
        ctx.gpr[16] = s0;
        const std::uint32_t half = half_u(rt, s0 + 0xAu);
        ctx.gpr[2] = half < 2u ? 1u : 0u;
        if (!(half < 2u)) {
            ctx.gpr[2] = 0xEA60u;
            t += 0xEA60u;
            ctx.gpr[18] = t;
            skip = true;
        }
    }
    if (!skip) {
        const std::uint32_t state = call_set(rt, ctx, 0x088D6358u, {s3});
        ctx.gpr[3] = 2u;
        ctx.gpr[4] = self;  // the delay slot of the beq
        if (state == 2u) {
            s0 = word(rt, self + 0x130u);
            ctx.gpr[16] = s0;
        } else {
            ctx.gpr[5] = 1u;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x088DC7C0u, {self, 1u});
            s0 = word(rt, self + 0x130u);
            ctx.gpr[16] = s0;
        }
    }
    ctx.gpr[4] = self;  // the delay slot of the jal
    const std::uint32_t low = call_set(rt, ctx, 0x088D9C70u, {self});
    const std::uint32_t high = ctx.gpr[3];
    const std::uint32_t sum = low + t;
    const std::uint32_t sign = static_cast<std::uint32_t>(signed_of(t) >> 31);
    ctx.gpr[4] = sum;
    ctx.gpr[7] = sign;
    ctx.gpr[8] = sum < low ? 1u : 0u;
    const std::uint32_t upper = (sum < low ? 1u : 0u) + (high + sign);
    ctx.gpr[5] = upper;
    put(rt, s0 + 0x38u, sum);
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = 0u;
    put(rt, s0 + 0x3Cu, upper);
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0u;
    const std::uint32_t current = word(rt, self + 0x130u);
    ctx.gpr[2] = current;
    put(rt, current + 0x40u, 0u);
    put(rt, current + 0x44u, 0u);
    put8(rt, self + 0x1Du, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088D776Cu, {self, 0u, 0u, 0u});
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = 1u;
}

// func_08877FB0(this): the holder h = sp + 0x10 is built by func_08AB85F0(h), func_088755E4(this, sp + 0x20, h, 3) fills the vector at sp + 0x20 and n = func_08AB856C(h) feeds t = func_08B77BFC(n) (sp[0x2C] = 0 before). With the word sp[0x18] at least 3 the vector (x, y, z at sp + 0x20) is copied to sp + 4,
// func_08B4A720(sp + 4) gives f0 and func_08B4A5E8(sp + 4, 1.0f / sqrt(f0)) normalises the copy, which goes back to sp + 0x20; r = func_0886AFA8(this[0]) and func_0887CA78(r + 4, sp + 0x20) (t zero) or func_0887C930(r + 4, sp + 0x20) (t non-zero) run. The string cell of sp[0x10]
// (the empty cell is D_08D75298) is released with func_08B60804(data - 4, -1) and, when that is not positive, func_08B93554(data - 0xC, sp).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (!(signed_of(count) < 3))`; `if (t == 0u)`; `the else branch of the test above`; `if (data - 0xCu == 0x08D75298u) [the early return]`; `if (signed_of(references) <= 0) [the call it guards]`
void func_08877FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p10 = frame.at(0x10u), p20 = frame.at(0x20u);
    ctx.gpr[16] = p10;
    ctx.gpr[18] = p20;
    ctx.gpr[17] = self;
    ctx.gpr[4] = p10;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08AB85F0u, {p10});
    ctx.gpr[5] = p20;
    ctx.gpr[6] = p10;
    ctx.gpr[7] = 3u;
    ctx.gpr[4] = self;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088755E4u, {self, p20, p10, 3u});
    ctx.gpr[4] = p10;
    put(rt, frame.at(0x2Cu), 0u);  // the delay slot of the jal
    const std::uint32_t n = call_set(rt, ctx, 0x08AB856Cu, {p10});
    ctx.gpr[4] = n;  // the delay slot of the jal
    const std::uint32_t t = call_set(rt, ctx, 0x08B77BFCu, {n});
    ctx.gpr[19] = t;
    const std::uint32_t count = word(rt, frame.at(0x18u));
    ctx.gpr[2] = signed_of(count) < 3 ? 1u : 0u;
    ctx.gpr[4] = word(rt, p10);  // the delay slot of the bnel (taken)
    if (!(signed_of(count) < 3)) {
        ctx.fpr[0] = float_at(rt, p20);
        ctx.fpr[1] = float_at(rt, p20 + 4u);
        ctx.gpr[16] = p4;
        put_float(rt, p4, ctx.fpr[0]);
        ctx.fpr[0] = float_at(rt, p20 + 8u);
        put_float(rt, p4 + 4u, ctx.fpr[1]);
        ctx.gpr[4] = p4;
        put_float(rt, p4 + 8u, ctx.fpr[0]);  // the delay slot of the jal
        (void)call_keep(rt, ctx, 0x08B4A720u, {p4});
        ctx.fpr[0] = std::sqrt(ctx.fpr[0]);
        mtc1(ctx, 12, 0x3F800000u);
        ctx.gpr[4] = p4;
        ctx.fpr[12] = ctx.fpr[12] / ctx.fpr[0];  // the delay slot of the jal
        (void)call_keep(rt, ctx, 0x08B4A5E8u, {p4}, {ctx.fpr[12]});
        ctx.fpr[0] = float_at(rt, p4);
        const std::uint32_t head = word(rt, self);
        ctx.gpr[4] = head;
        ctx.fpr[1] = float_at(rt, p4 + 4u);
        put_float(rt, p20, ctx.fpr[0]);
        ctx.fpr[0] = float_at(rt, p4 + 8u);
        put_float(rt, p20 + 4u, ctx.fpr[1]);
        put_float(rt, p20 + 8u, ctx.fpr[0]);  // the delay slot of the jal
        const std::uint32_t r = call_keep(rt, ctx, 0x0886AFA8u, {head});
        ctx.gpr[4] = r + 4u;  // the delay slot of the bnez / jal
        if (t == 0u) {
            ctx.gpr[5] = ctx.gpr[18];
            (void)call_set(rt, ctx, 0x0887CA78u, {r + 4u, ctx.gpr[18]});
        } else {
            ctx.gpr[5] = ctx.gpr[18];
            (void)call_set(rt, ctx, 0x0887C930u, {r + 4u, ctx.gpr[18]});
        }
        ctx.gpr[4] = word(rt, p10);
    }
    const std::uint32_t data = ctx.gpr[4];
    ctx.gpr[2] = 0x08D75298u;
    ctx.gpr[16] = data - 0xCu;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (data - 0xCu == 0x08D75298u) return;
    const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = data - 0xCu;
    ctx.gpr[5] = p0;  // the delay slot of the bgtz
    if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B93554u, {data - 0xCu, p0});
}

// func_0897AAB4(a0, a1, mode, args): mode 2 only (anything else returns): the holder sp + 0xC is built by func_089C9C34(sp + 0xC), k = sp[0xC] (null: func_08A5BB7C(D_08CA3268)); the virtual slot 0xC of args[0] (null: func_08A5BB7C(D_08CA2F84)) fills the string sp + 8 (sp + 8, args[0]) and that of args[1] the string sp + 4;
// func_089C8C70(k, sp + 8, sp + 4) runs; both strings are released (release_cell: flag sp + 1 for sp + 4, sp for sp + 8) and func_08A5B9EC(sp + 0xC) ends it.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_0897AAB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t mode = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[2] = 2u;
    ctx.gpr[17] = args;
    if (mode != 2u) return;
    ctx.gpr[20] = pC;
    ctx.gpr[4] = pC;
    (void)call_set(rt, ctx, 0x089C9C34u, {pC});
    const std::uint32_t k = word(rt, pC);
    ctx.gpr[18] = k;
    if (k == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA3268u});
    std::uint32_t s0 = word(rt, args);
    ctx.gpr[16] = s0;
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});
    ctx.gpr[19] = p8;
    ctx.gpr[3] = word(rt, s0);
    ctx.gpr[5] = s0;
    (void)call_set(rt, ctx, slot_of(rt, s0, 0xCu), {p8, s0});
    const std::uint32_t s1 = word(rt, args + 4u);
    ctx.gpr[17] = s1;
    if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});
    ctx.gpr[16] = p4;
    ctx.gpr[3] = word(rt, s1);
    ctx.gpr[5] = s1;
    (void)call_set(rt, ctx, slot_of(rt, s1, 0xCu), {p4, s1});
    ctx.gpr[6] = p4;  // the delay slot of the jal
    ctx.gpr[5] = p8;
    (void)call_set(rt, ctx, 0x089C8C70u, {k, p8, p4});
    const std::uint32_t first = word(rt, p4);
    ctx.gpr[4] = first;
    ctx.gpr[17] = 0x08D74FD0u;
    release_cell(rt, ctx, first, frame.at(1u), 16u);
    const std::uint32_t second = word(rt, p8);
    ctx.gpr[4] = second;
    release_cell(rt, ctx, second, p0, 16u);
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
}

// func_089EB064(this, x): like func_089E5BF4 it runs under the lock D_08CF575C (func_08B344E0 .. the tail call func_08B3457C(D_08CF575C)) after func_08A00AB0(), func_08968C38(this) (zero returns) and this[0x19C] >= 0 (otherwise return), then func_089EA1C0(this, x); the rows y from this[0x1C0] to this[0x1C4] and the columns i from this[0x1B8] to this[0x1BC]
// (both read again as the original does) with i < this[0x154] and y < this[0x158]: c = func_089E4DE0(this, i, y), q = func_089E583C(this, c); a non-zero q different from this[0x15C] (or with this[0x15C] zero) goes to func_08A009CC(this, x, q); then, when func_08969330(this) is non-zero and c == this[0x19C], the word this[0x1D8] is tested:
// zero, or the bit 0x01000000 of this[0xE0] set, or p = (this[0x190] when non-zero, else q) zero, or func_08968C84(p) zero skips the call; otherwise func_089AFF3C(x, p) runs; in all those cases this[0x1D8] = 0 follows.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (usable == 0u) [the early return]`; `if (signed_of(ready) < 0) [the early return]`; `if (signed_of(a0) < signed_of(s3)) [the jump to the unlock]`; `if (signed_of(v1) < signed_of(s0))`; `if (!(signed_of(s0) < signed_of(columns)))`; `if (!(signed_of(s3) < signed_of(rows)))`; `if (q != 0u)`; `report = (known == 0u) || (q != known)`; `if (more == 0u)`; `if (bits != 0u)`; `if (preferred != 0u) s2 = preferred`; `if (s2 == 0u)`; `if (valid == 0u)`; `the else branch of the test above`
void func_089EB064(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], x = ctx.gpr[5];
    constexpr std::uint32_t lock = 0x08CF575Cu;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        ctx.gpr[21] = x;
        (void)call_keep(rt, ctx, 0x08A00AB0u, {});
        const std::uint32_t usable = call_set(rt, ctx, 0x08968C38u, {self});
        if (usable == 0u) return;
        const std::uint32_t ready = word(rt, self + 0x19Cu);
        ctx.gpr[2] = ready;
        if (signed_of(ready) < 0) return;
        (void)call_set(rt, ctx, 0x08B344E0u, {lock});
        (void)call_set(rt, ctx, 0x089EA1C0u, {self, x});
        std::uint32_t &s0 = ctx.gpr[16];
        std::uint32_t &s2 = ctx.gpr[18];
        std::uint32_t &s3 = ctx.gpr[19];
        std::uint32_t &s4 = ctx.gpr[20];
        ctx.gpr[22] = 0x01000000u;
        s3 = word(rt, self + 0x1C0u);
        std::uint32_t a0 = word(rt, self + 0x1C4u);
        ctx.gpr[4] = a0;
        ctx.gpr[2] = signed_of(a0) < signed_of(s3) ? 1u : 0u;
        ctx.gpr[4] = lock;  // the delay slot of the bnel (taken)
        if (signed_of(a0) < signed_of(s3)) goto unlock;
        ctx.gpr[4] = a0;
        {
            std::uint32_t v1 = word(rt, self + 0x1BCu);
            s0 = word(rt, self + 0x1B8u);
            ctx.gpr[3] = v1;
            for (;;) {
                ctx.gpr[2] = signed_of(v1) < signed_of(s0) ? 1u : 0u;
                if (signed_of(v1) < signed_of(s0)) {
                    s3 += 1u;  // the delay slot of the bnel (taken)
                } else {
                    for (;;) {
                        const std::uint32_t columns = word(rt, self + 0x154u);
                        ctx.gpr[2] = columns;
                        if (!(signed_of(s0) < signed_of(columns))) {
                            s0 += 1u;  // the delay slot of the beql (taken)
                        } else {
                            const std::uint32_t rows = word(rt, self + 0x158u);
                            ctx.gpr[2] = signed_of(s3) < signed_of(rows) ? 1u : 0u;
                            if (!(signed_of(s3) < signed_of(rows))) {
                                s0 += 1u;  // the delay slot of the beql (taken)
                            } else {
                                ctx.gpr[6] = s3;
                                ctx.gpr[4] = self;
                                ctx.gpr[5] = s0;  // the delay slot of the jal
                                const std::uint32_t c = call_set(rt, ctx, 0x089E4DE0u, {self, s0, s3});
                                ctx.gpr[5] = c;
                                ctx.gpr[4] = self;
                                s4 = c;  // the delay slot of the jal
                                const std::uint32_t q = call_set(rt, ctx, 0x089E583Cu, {self, c});
                                s2 = q;  // the delay slot of the beqz
                                bool report = false;
                                if (q != 0u) {
                                    const std::uint32_t known = word(rt, self + 0x15Cu);
                                    ctx.gpr[2] = known;
                                    ctx.gpr[4] = self;  // the delay slot of the beqz
                                    report = (known == 0u) || (q != known);
                                }
                                if (report) {
                                    ctx.gpr[5] = x;
                                    (void)call_set(rt, ctx, 0x08A009CCu, {self, x, q});
                                }
                                ctx.gpr[4] = self;  // the delay slot of the jal
                                const std::uint32_t more = call_set(rt, ctx, 0x08969330u, {self});
                                bool store = false;
                                if (more == 0u) {
                                    v1 = word(rt, self + 0x1BCu);  // the delay slot of the beql (taken)
                                    ctx.gpr[3] = v1;
                                } else {
                                    const std::uint32_t key = word(rt, self + 0x19Cu);
                                    ctx.gpr[2] = key;
                                    if (s4 != key) {
                                        v1 = word(rt, self + 0x1BCu);  // the delay slot of the bnel (taken)
                                        ctx.gpr[3] = v1;
                                    } else {
                                        const std::uint32_t flag = word(rt, self + 0x1D8u);
                                        ctx.gpr[2] = flag;
                                        store = true;
                                        if (flag == 0u) {
                                            v1 = word(rt, self + 0x1BCu);  // the delay slot of the beql (taken)
                                            ctx.gpr[3] = v1;
                                        } else {
                                            const std::uint32_t bits = word(rt, self + 0xE0u) & ctx.gpr[22];
                                            ctx.gpr[2] = bits;
                                            if (bits != 0u) {
                                                v1 = word(rt, self + 0x1BCu);  // the delay slot of the bnel (taken)
                                                ctx.gpr[3] = v1;
                                            } else {
                                                const std::uint32_t preferred = word(rt, self + 0x190u);
                                                ctx.gpr[2] = preferred;
                                                if (preferred != 0u) s2 = preferred;
                                                if (s2 == 0u) {
                                                    v1 = word(rt, self + 0x1BCu);  // the delay slot of the beql (taken)
                                                    ctx.gpr[3] = v1;
                                                } else {
                                                    ctx.gpr[4] = s2;  // the delay slot of the jal
                                                    const std::uint32_t valid = call_set(rt, ctx, 0x08968C84u, {s2});
                                                    if (valid == 0u) {
                                                        v1 = word(rt, self + 0x1BCu);  // the delay slot of the beql (taken)
                                                        ctx.gpr[3] = v1;
                                                    } else {
                                                        ctx.gpr[5] = s2;
                                                        ctx.gpr[4] = x;  // the delay slot of the jal
                                                        (void)call_set(rt, ctx, 0x089AFF3Cu, {x, s2});
                                                        v1 = word(rt, self + 0x1BCu);
                                                        ctx.gpr[3] = v1;
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                                if (store) put(rt, self + 0x1D8u, 0u);
                                s0 += 1u;
                            }
                        }
                        ctx.gpr[2] = signed_of(v1) < signed_of(s0) ? 1u : 0u;
                        if (signed_of(v1) < signed_of(s0)) break;
                    }
                    a0 = word(rt, self + 0x1C4u);
                    ctx.gpr[4] = a0;
                    s3 += 1u;
                }
                ctx.gpr[2] = signed_of(a0) < signed_of(s3) ? 1u : 0u;
                if (signed_of(a0) < signed_of(s3)) break;
                s0 = word(rt, self + 0x1B8u);  // the delay slot of the beql (taken)
            }
            ctx.gpr[4] = lock;
        }
    unlock:;
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// func_08809E90(a0, a1, argc, args): (argc <= 0 returns) func_08809DBC(sp + 4) gives the object o = sp[4] (the holder is destroyed at once with func_08A5B9EC) and w = o[0x118]; the holder sp is built by func_088297A8(sp, args) and its object (null: func_08A5BB7C(D_08C98248)) gives s = func_088294D0(object); func_0885F3AC(s, 1) and func_08969B98(o, sp + 8, sp + 0xC, sp + 0x10, sp + 0x14)
// fill four floats (a, b, c, d); f12 = a / D_08CDE764, f13 = b / D_08CDE760, f14 = c / D_08CDE764, f15 = d / D_08CDE760, f16 = 1.0f and func_0885E314(s, ...) runs; with argc >= 2 the virtual slot 0x3C of args[1] (null: func_08A5BB7C(D_08C981FC)) gives the second word (0 otherwise); w[0] = s and w[4] = that word.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(argc) <= 0) [the early return]`; `if (holder == 0u) [the call it guards]`; `if (!(signed_of(argc) < 2))`; `if (s0 == 0u) [the call it guards]`
void func_08809E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argc = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p14 = frame.at(0x14u);
    ctx.gpr[16] = p4;
    ctx.gpr[4] = p4;
    ctx.gpr[20] = args;
    ctx.gpr[19] = argc;
    if (signed_of(argc) <= 0) return;
    (void)call_set(rt, ctx, 0x08809DBCu, {p4});
    const std::uint32_t object = word(rt, p4);
    ctx.gpr[18] = object;
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    ctx.gpr[4] = p0;
    ctx.gpr[5] = args;
    ctx.gpr[21] = word(rt, object + 0x118u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088297A8u, {p0, args});
    const std::uint32_t holder = word(rt, p0);
    ctx.gpr[16] = holder;
    if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98248u});
    const std::uint32_t value = call_set(rt, ctx, 0x088294D0u, {holder});
    ctx.gpr[17] = value;
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
    ctx.gpr[5] = 1u;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0885F3ACu, {value, 1u});
    ctx.gpr[4] = object;
    ctx.gpr[5] = p8;
    ctx.gpr[6] = pC;
    ctx.gpr[7] = p10;
    ctx.gpr[8] = p14;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08969B98u, {object, p8, pC, p10, p14});
    ctx.fpr[0] = float_at(rt, 0x08CDE764u);
    ctx.fpr[1] = float_at(rt, 0x08CDE760u);
    ctx.fpr[12] = float_at(rt, p8);
    ctx.fpr[14] = float_at(rt, p10);
    ctx.fpr[15] = float_at(rt, p14);
    ctx.fpr[13] = float_at(rt, pC);
    ctx.fpr[12] = ctx.fpr[12] / ctx.fpr[0];
    ctx.fpr[14] = ctx.fpr[14] / ctx.fpr[0];
    ctx.fpr[15] = ctx.fpr[15] / ctx.fpr[1];
    ctx.fpr[13] = ctx.fpr[13] / ctx.fpr[1];
    mtc1(ctx, 16, 0x3F800000u);
    ctx.gpr[4] = value;  // the delay slot of the jal
    (void)call_keep(rt, ctx, 0x0885E314u, {value}, {ctx.fpr[12], ctx.fpr[13], ctx.fpr[14], ctx.fpr[15]});
    ctx.gpr[2] = argc < 2u ? 1u : 0u;
    ctx.gpr[3] = 0u;  // the delay slot of the bnez
    std::uint32_t second = 0u;
    if (!(signed_of(argc) < 2)) {
        const std::uint32_t s0 = word(rt, args + 4u);
        ctx.gpr[16] = s0;
        if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C981FCu});
        ctx.gpr[3] = word(rt, s0);
        second = call_set(rt, ctx, slot_of(rt, s0, 0x3Cu), {s0});
        ctx.gpr[3] = second;
    }
    const std::uint32_t target = ctx.gpr[21];
    put(rt, target, value);
    put(rt, target + 4u, second);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08861FD0(this): b = this[0] (a holder of the record); when the byte b[0x3FDB] is set, func_08861CB0(this, b[0x5AC] + 4, b[0x3FD8], b[0x3FD9], b[0x3FDA], 1) runs. The 8 bytes b[0x3E98 + 0x18 * i] are set to -1; the word b[0x3E78] is flipped (^ 1) and with it
// e = b + 0x600 + 7224 * flipped (the flipped word is the index of one of two blocks): func_0888AB4C(e + 8, byte b[0x5AC][0x27] ^ 1); b[0x5B8][0x34] = e + 8; then for i = 0..3 with r = b + 0x4120 + 0x50 * i (a source record): when the byte r[0x17] is set,
// func_0888A9B8(e + 8, i, r + 6, a3, t0, t1, 1) runs with (t0, t1) = (r[0x30], r[0x34]) when the byte r[0x1A] is set and (-1, -1) otherwise; then the unaligned word at r is stored at e + 0x1578 + 0x1B0 * i + 8 (an unaligned store), the words r[0x38], r[0x3C] at e + 0x1578 + 0x1B0 * i + 0 / + 4
// and p = func_0885E41C(this, i, r[0x38], r[0x3C]) gets the halfword p[0xA] = 0 and the byte p[8] = i. Last, the halfword at (e + 8) + 0x1C32 is func_0885E4B8(this). v0 is that call's result.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (flag != 0u)`; `if (v0 != 0u)`
void func_08861FD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t base = word(rt, self);
    ctx.gpr[18] = base;
    const std::uint32_t flag = byte_u(rt, base + 0x3FDBu);
    ctx.gpr[2] = flag;
    ctx.gpr[21] = self;  // the delay slot of the beqz
    if (flag != 0u) {
        const std::uint32_t record = word(rt, base + 0x5ACu);
        ctx.gpr[5] = record + 4u;
        ctx.gpr[6] = byte_u(rt, base + 0x3FD8u);
        ctx.gpr[7] = byte_u(rt, base + 0x3FD9u);
        ctx.gpr[8] = byte_u(rt, base + 0x3FDAu);
        (void)call_set(rt, ctx, 0x08861CB0u, {self, record + 4u, ctx.gpr[6], ctx.gpr[7], ctx.gpr[8], 1u});
    }
    for (std::uint32_t i = 0u; i < 8u; ++i) put8(rt, base + 0x3E98u + 0x18u * i, 0xFFFFFFFFu);
    ctx.gpr[3] = 8u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = 8u;
    ctx.gpr[2] = base + 0x3E98u + 0x18u * 8u;
    const std::uint32_t flipped = word(rt, base + 0x3E78u) ^ 1u;
    const std::uint32_t current = word(rt, base + 0x5ACu);
    ctx.gpr[6] = current;
    ctx.gpr[17] = base + 0x4120u;
    put(rt, base + 0x3E78u, flipped);
    const std::uint32_t scaled = flipped * 56u;
    const std::uint32_t entry = base + (scaled + (scaled << 7)) + 0x600u;  // s0
    const std::uint32_t block = entry + 8u;                                    // s4
    ctx.gpr[16] = entry;
    ctx.gpr[20] = block;
    (void)call_set(rt, ctx, 0x0888AB4Cu, {block, byte_u(rt, current + 0x27u) ^ 1u});
    const std::uint32_t owner = word(rt, base + 0x5B8u);
    ctx.gpr[19] = 0u;
    ctx.gpr[18] = entry + 0x1578u;
    put(rt, owner + 0x34u, block);
    ctx.gpr[22] = 4u;
    ctx.gpr[16] = entry + 0x1568u;
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        const std::uint32_t record = base + 0x4120u + 0x50u * i;
        const std::uint32_t target = entry + 0x1578u + 0x1B0u * i;
        ctx.gpr[19] = i;
        ctx.gpr[17] = record;
        ctx.gpr[18] = target;
        ctx.gpr[16] = entry + 0x1568u + 0x1B0u * i;
        ctx.gpr[5] = i;
        ctx.gpr[6] = record + 6u;
        ctx.gpr[4] = block;
        ctx.gpr[10] = 1u;
        ctx.gpr[8] = 0xFFFFFFFFu;
        std::uint32_t v0 = byte_u(rt, record + 0x17u);
        ctx.gpr[2] = v0;
        ctx.gpr[9] = 0xFFFFFFFFu;  // the delay slot of the beqz
        if (v0 != 0u) {
            v0 = byte_u(rt, record + 0x1Au);
            ctx.gpr[2] = v0;
            if (v0 != 0u) {
                ctx.gpr[8] = word(rt, record + 0x30u);
                ctx.gpr[9] = word(rt, record + 0x34u);
            }
            (void)call_set(rt, ctx, 0x0888A9B8u, {block, i, record + 6u, ctx.gpr[7], ctx.gpr[8], ctx.gpr[9], 1u});
            const std::uint32_t unaligned = word_unaligned(rt, record);
            ctx.gpr[2] = unaligned;
            ctx.gpr[4] = self;
            put_unaligned(rt, target + 8u, unaligned);
            const std::uint32_t third = word(rt, record + 0x38u), fourth = word(rt, record + 0x3Cu);
            put(rt, target, third);
            put(rt, target + 4u, fourth);  // the delay slot of the jal below
            const std::uint32_t object = call_set(rt, ctx, 0x0885E41Cu, {self, ctx.gpr[5], third, fourth});
            put16(rt, object + 0xAu, 0u);
            put8(rt, object + 8u, i);
        }
        ctx.gpr[19] = i + 1u;
        ctx.gpr[17] = record + 0x50u;
        ctx.gpr[16] = entry + 0x1568u + 0x1B0u * (i + 1u);
        ctx.gpr[18] = target + 0x1B0u;
    }
    const std::uint32_t result = call_set(rt, ctx, 0x0885E4B8u, {self});
    put16(rt, block + 0x1C32u, result);
}

// func_08865B98(this, a1, a2, a3, t0, t1, t2, t3): the five arguments a2, a3, t0, t1, t2 that are zero are replaced by words of the table w = this[0]: a2 -> w[0x68], a3 -> w[0x6C], t0 -> w[0x70], t1 -> w[0x74], t2 -> w[0x88]. With p = a3 * a1 and q = w[0x7C] * a1 (32-bit products):
// func_088646E4(this, a1, p, t0, q, w[0x80]); func_088659D0(this, a1, t1, w[0x84]); func_088635CC(this, a2 - p, w[0x78] - q); func_088638A8(this, t2); func_088D88A8(this[0][0x5AC], a2 - p, p, t0, t1, t2, a1). The stack word at sp + 0 is t3 (the function stores it for no callee):
// a negative t3 returns it as it is, any other value is the second argument of the tail call func_0885E498(this, t3).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (a2 == 0u)`; `if (a3 == 0u) a3 = word(rt, table + 0x6Cu)`; `if (t0 == 0u) t0 = word(rt, table + 0x70u)`; `if (t1 == 0u) t1 = word(rt, table + 0x74u)`; `if (t2 == 0u) t2 = word(rt, table + 0x88u)`; `if (signed_of(kept) < 0) [the early return]`
void func_08865B98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5];
    std::uint32_t a2 = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8], t1 = ctx.gpr[9], t2 = ctx.gpr[10];
    const std::uint32_t t3 = ctx.gpr[11];
    std::uint32_t after = 0u;
    {
        GuestFrame frame(ctx, 0x40u);
        SavedRegs saved(ctx);
        ctx.gpr[30] = t2;
        ctx.gpr[23] = t1;
        ctx.gpr[22] = t0;
        ctx.gpr[21] = a1;
        ctx.gpr[20] = self;
        ctx.gpr[19] = a2;
        put(rt, frame.at(0u), t3);  // the delay slot of the bnez
        const std::uint32_t table = word(rt, self);
        ctx.gpr[3] = table;
        if (a2 == 0u) {
            a2 = word(rt, table + 0x68u);
            ctx.gpr[19] = a2;
        }
        if (a3 == 0u) a3 = word(rt, table + 0x6Cu);
        if (t0 == 0u) t0 = word(rt, table + 0x70u);
        ctx.gpr[22] = t0;
        if (t1 == 0u) t1 = word(rt, table + 0x74u);
        ctx.gpr[23] = t1;
        if (t2 == 0u) t2 = word(rt, table + 0x88u);
        ctx.gpr[30] = t2;
        const std::uint32_t factor = word(rt, table + 0x7Cu);
        const std::uint32_t last = word(rt, table + 0x80u);
        const std::uint32_t rest = word(rt, table + 0x78u);
        const std::uint32_t tail = word(rt, table + 0x84u);
        const std::uint32_t p = a3 * a1;
        const std::uint32_t q = factor * a1;
        ctx.gpr[17] = p;
        ctx.gpr[18] = tail;
        const std::uint32_t remaining = a2 - p;
        ctx.gpr[19] = remaining;
        ctx.gpr[16] = rest - q;  // the delay slot of the first jal
        ctx.gpr[7] = ctx.gpr[22];
        (void)call_set(rt, ctx, 0x088646E4u, {self, a1, p, t0, q, last});
        ctx.gpr[16] = rest - q;
        (void)call_set(rt, ctx, 0x088659D0u, {self, a1, t1, tail});
        (void)call_set(rt, ctx, 0x088635CCu, {self, remaining, rest - q});
        (void)call_set(rt, ctx, 0x088638A8u, {self, t2});
        const std::uint32_t owner = word(rt, self);
        ctx.gpr[2] = owner;
        ctx.gpr[9] = t2;
        ctx.gpr[10] = a1;  // the delay slot of the jal below
        (void)call_set(rt, ctx, 0x088D88A8u, {word(rt, owner + 0x5ACu), remaining, p, t0, t1, t2, a1});
        const std::uint32_t kept = word(rt, frame.at(0u));
        ctx.gpr[2] = kept;
        ctx.gpr[4] = self;  // the delay slot of the bltz
        if (signed_of(kept) < 0) return;
        ctx.gpr[5] = kept;
        after = kept;
    }
    tail_call(ctx, 0x0885E498u, {self, after});
}

// func_08898AB4(this): when func_088B1330(this) is non-zero, for i = 0..3: the virtual slot 0x1C of o = this[0x40] (o, i) and, when it returns non-zero, func_08898744(this[0x34], this[0x40]). Then the virtual slot 0x18 of this[0x40] (o) runs and, for i = 0..3 and j = 0..1,
// c = func_088F8CC0(this, i, j) goes to func_08898EB0(c) when it is non-zero. Last, for k = 0..3: c = func_088F8D58(this, k) (non-zero) with the holder h = c + 0x20: when func_088977D4(h) == 1, r = func_0889854C(this, k), s = func_088970E0(h) and func_088984E4(this[0x40], k, s, r) run.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (enabled != 0u)`; `for (std::uint32_t i = 0u; i < 4u; ++i)`; `if (answer != 0u)`; `if (cell != 0u)`
void func_08898AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    const std::uint32_t enabled = call_set(rt, ctx, 0x088B1330u, {self});
    ctx.gpr[16] = 0u;  // the delay slot of the beqz
    if (enabled != 0u) {
        ctx.gpr[17] = 4u;
        for (std::uint32_t i = 0u; i < 4u; ++i) {
            const std::uint32_t object = word(rt, self + 0x40u);
            ctx.gpr[4] = object;
            const std::uint32_t slot = word(rt, word(rt, object) + 0x1Cu);
            ctx.gpr[2] = slot;
            ctx.gpr[3] = word(rt, object);
            ctx.gpr[5] = i;  // the delay slot of the jalr
            const std::uint32_t answer = call_set(rt, ctx, slot, {object, i});
            ctx.gpr[6] = i;
            ctx.gpr[16] = i + 1u;  // the delay slot of the beqz
            if (answer != 0u) {
                const std::uint32_t first = word(rt, self + 0x34u), second = word(rt, self + 0x40u);
                (void)call_set(rt, ctx, 0x08898744u, {first, second});
            }
            ctx.gpr[4] = word(rt, self + 0x40u);  // the delay slot of the bnel (taken) or the next instruction
        }
    }
    const std::uint32_t object = word(rt, self + 0x40u);
    ctx.gpr[4] = object;
    ctx.gpr[17] = 0u;
    ctx.gpr[19] = 2u;
    const std::uint32_t slot = word(rt, word(rt, object) + 0x18u);
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = slot;
    ctx.gpr[20] = 4u;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {object});
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        ctx.gpr[17] = i;
        ctx.gpr[16] = 0u;
        for (std::uint32_t j = 0u; j < 2u; ++j) {
            ctx.gpr[6] = j;
            ctx.gpr[16] = j;
            const std::uint32_t cell = call_set(rt, ctx, 0x088F8CC0u, {self, i, j});
            ctx.gpr[16] = j + 1u;
            ctx.gpr[4] = cell;  // the delay slot of the beqz
            if (cell != 0u) (void)call_set(rt, ctx, 0x08898EB0u, {cell});
            ctx.gpr[6] = j + 1u;  // the delay slot of the bne
        }
        ctx.gpr[17] = i + 1u;
    }
    ctx.gpr[20] = 0u;
    ctx.gpr[22] = 1u;
    ctx.gpr[21] = 4u;
    for (std::uint32_t k = 0u; k < 4u; ++k) {
        ctx.gpr[20] = k;
        const std::uint32_t cell = call_set(rt, ctx, 0x088F8D58u, {self, k});
        const std::uint32_t holder = cell + 0x20u;
        ctx.gpr[19] = holder;
        ctx.gpr[4] = holder;  // the delay slot of the beqz
        if (cell != 0u) {
            const std::uint32_t kind = call_set(rt, ctx, 0x088977D4u, {holder});
            ctx.gpr[5] = k;
            ctx.gpr[4] = self;  // the delay slot of the bne
            if (kind == 1u) {
                const std::uint32_t found = call_set(rt, ctx, 0x0889854Cu, {self, k});
                const std::uint32_t owner = word(rt, self + 0x40u);
                ctx.gpr[16] = owner;
                ctx.gpr[4] = holder;
                ctx.gpr[17] = found;  // the delay slot of the jal below
                const std::uint32_t size = call_set(rt, ctx, 0x088970E0u, {holder});
                ctx.gpr[6] = size;
                ctx.gpr[4] = owner;
                ctx.gpr[5] = k;
                ctx.gpr[7] = found;  // the delay slot of the jal below
                (void)call_set(rt, ctx, 0x088984E4u, {owner, k, size, found});
            }
        }
        ctx.gpr[20] = k + 1u;
        ctx.gpr[4] = self;  // the delay slot of the bne
    }
}

// func_088890AC(this): n = func_088884D4(this); for i = 0..n-1 (n <= 0 returns): the virtual slot 0x28 of this fills the pair at sp = {object, counted} (this, i); the virtual slot 0x30 of `object` (when non-zero) runs; the counted holder c = sp[4] (when non-zero) loses one reference at c + 4 (when that was the last one the virtual slot 8
// of c runs, then the counter at c + 8 loses one and, when that was its last, the virtual slot 0xC of c runs). The same two steps follow the virtual slot 0x30 of this (sp, this, i): its pair is released the same way. The counters are ll / sc loops (one thread: a load and a store).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_088890AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t out = frame.at(0u);
    ctx.gpr[17] = self;
    const std::uint32_t count = call_set(rt, ctx, 0x088884D4u, {self});
    ctx.gpr[20] = count;  // the delay slot of the blez
    if (signed_of(count) <= 0) return;
    ctx.gpr[18] = 0u;
    ctx.gpr[19] = 1u;
    // The release of the pair: the object's virtual slot 0x30, then the counters of the counted holder.
    const auto release = [&](bool second) {
        const std::uint32_t object = word(rt, out);
        ctx.gpr[4] = object;
        ctx.gpr[16] = word(rt, out + 4u);  // the delay slot of the beqz
        if (object != 0u) {
            ctx.gpr[3] = word(rt, object);
            (void)call_set(rt, ctx, slot_of(rt, object, 0x30u), {object});
            ctx.gpr[16] = word(rt, out + 4u);
        }
        const std::uint32_t held = ctx.gpr[16];
        ctx.gpr[3] = held + 4u;  // the delay slot of the beqz
        if (held != 0u) {
            if (atomic_decrement(rt, ctx, held + 4u) == 1u) {
                (void)call_set(rt, ctx, slot_of(rt, held, 8u), {held});
                ctx.gpr[3] = held + 8u;
                if (atomic_decrement(rt, ctx, held + 8u) == 1u) {
                    (void)call_set(rt, ctx, slot_of(rt, held, 0xCu), {held});
                    ctx.gpr[18] += second ? 1u : 0u;
                } else {
                    ctx.gpr[18] += second ? 1u : 0u;  // the delay slot of the bnel (taken)
                    if (!second) ctx.gpr[3] = word(rt, self);
                }
            } else {
                ctx.gpr[18] += second ? 1u : 0u;  // the delay slot of the bnel (taken)
                if (!second) ctx.gpr[3] = word(rt, self);
            }
        } else {
            ctx.gpr[3] = word(rt, self);
            if (second) ctx.gpr[18] += 1u;
        }
    };
    for (std::uint32_t i = 0u; i < count; ++i) {
        ctx.gpr[18] = i;
        ctx.gpr[4] = out;
        ctx.gpr[5] = self;
        ctx.gpr[6] = i;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot_of(rt, self, 0x28u), {out, self, i});
        release(false);
        ctx.gpr[6] = i;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot_of(rt, self, 0x30u), {out, self, i});
        release(true);
        ctx.gpr[4] = word(rt, self + 0u);  // the delay slot of the bnel (taken) or the last loaded word
    }
}

// func_088A2074(this, x, y): with x != 0: this[0x48] = x; when the byte this[0x40] is 0 the object this[0x44] (when non-zero) gets its virtual slot 4 called, o = func_08B62344(8) is built by func_088AFAF0(o), this[0x44] = o and this[0x40] = 1; then this[0x44][4] = x. With x == 0: when the byte this[0x40] is non-zero the object this[0x44]
// (when non-zero) gets its virtual slot 4 called, o = func_08B62344(4) gets the word D_08CC2F00, this[0x44] = o and this[0x40] = 0. Then, for both: this[0x54][0x14] = this[0x44], this[0x54][0x18] = this[0x50], func_0894D094(this[0x54], byte this[0x18], y, 1), and a non-zero this[0x50] gets [0x14] = x and the byte [0x20] = 1. v0 is the last word loaded or 1.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (old != 0u)`; `if (flag == 0u)`; `if (flag != 0u)`; `if (target == 0u) [the early return]`
void func_088A2074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], x = ctx.gpr[5], y = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = y;
    ctx.gpr[18] = x;
    ctx.gpr[17] = self;
    // The old object this[0x44] is told to go (its virtual slot 4) when there is one.
    const auto drop_old = [&]() {
        const std::uint32_t old = word(rt, self + 0x44u);
        ctx.gpr[4] = old;
        if (old != 0u) {
            ctx.gpr[3] = word(rt, old);
            (void)call_set(rt, ctx, slot_of(rt, old, 4u), {old});
        }
    };
    std::uint32_t a0;
    if (x != 0u) {
        const std::uint32_t flag = byte_u(rt, self + 0x40u);
        ctx.gpr[2] = flag;
        put(rt, self + 0x48u, x);  // the delay slot of the bnez
        if (flag == 0u) {
            drop_old();
            const std::uint32_t created = call_set(rt, ctx, 0x08B62344u, {8u});
            ctx.gpr[16] = created;  // the delay slot of the next jal
            (void)call_set(rt, ctx, 0x088AFAF0u, {created});
            ctx.gpr[2] = 1u;
            put(rt, self + 0x44u, created);
            put8(rt, self + 0x40u, 1u);
        }
        ctx.gpr[2] = word(rt, self + 0x44u);
        put(rt, ctx.gpr[2] + 4u, x);  // the delay slot of the j
        a0 = word(rt, self + 0x54u);
    } else {
        const std::uint32_t flag = byte_u(rt, self + 0x40u);
        ctx.gpr[2] = flag;
        if (flag != 0u) {
            drop_old();
            const std::uint32_t created = call_set(rt, ctx, 0x08B62344u, {4u});
            ctx.gpr[3] = 0x08CC2F00u;
            put(rt, created, 0x08CC2F00u);
            put(rt, self + 0x44u, created);
            put8(rt, self + 0x40u, 0u);
        }
        a0 = word(rt, self + 0x54u);
    }
    ctx.gpr[4] = a0;
    const std::uint32_t holder = word(rt, self + 0x44u);
    ctx.gpr[2] = holder;
    ctx.gpr[6] = y;
    ctx.gpr[7] = 1u;
    put(rt, a0 + 0x14u, holder);
    const std::uint32_t link = word(rt, self + 0x50u);
    ctx.gpr[2] = link;
    ctx.gpr[3] = word(rt, self + 0x54u);
    put(rt, word(rt, self + 0x54u) + 0x18u, link);
    a0 = word(rt, self + 0x54u);
    ctx.gpr[5] = byte_u(rt, self + 0x18u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0894D094u, {a0, ctx.gpr[5], y, 1u});
    const std::uint32_t target = word(rt, self + 0x50u);
    ctx.gpr[2] = target;
    if (target == 0u) return;
    put(rt, target + 0x14u, x);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = word(rt, self + 0x50u);
    put8(rt, ctx.gpr[3] + 0x20u, 1u);
}

// func_088D31B0(this, a1, k): o = func_088DA588(this[0]) (zero returns); with e = D_08CDD244 + 24 * k (a table of 24-byte rows): o[0x10] = 0, then the row's word e[4] picks the virtual calls on o: 0 -> slot 0x10 (o, 1) and slot 0x1C (o, 0); 1 -> slot 0x10 (o, 0) and slot 0x1C (o, 1);
// 2 -> slot 0x10 (o, 0) and slot 0x1C (o, 0); anything else none. When e[0xC] is non-zero v0 = e and it returns; else r = func_088D62FC(this[0] + 4): non-zero returns r, zero stores o[0x10] = func_088D30B0(this) (and returns it).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (object == 0u) [the early return]`; `if (kind == 1u)`; `else if (kind == 2u)`; `else if (kind == 0u)`; `if (used != 0u) [the early return]`; `const std::uint32_t found = call_set(rt, ctx, 0x088D62FCu, {word(rt, self) + 4u})`; `if (found != 0u) [the early return]`; `const std::uint32_t created = call_set(rt, ctx, 0x088D30B0u, {self})`
void func_088D31B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], k = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = self;
    ctx.gpr[16] = k;
    const std::uint32_t object = call_set(rt, ctx, 0x088DA588u, {word(rt, self)});
    ctx.gpr[17] = object;  // the delay slot of the beqz
    if (object == 0u) return;
    constexpr std::uint32_t table = 0x08CDD244u;
    ctx.gpr[19] = k << 5;
    ctx.gpr[16] = k << 3;
    const std::uint32_t entry = table + 24u * k;
    put(rt, object + 0x10u, 0u);
    const std::uint32_t kind = word(rt, entry + 4u);
    ctx.gpr[3] = kind;
    ctx.gpr[2] = 1u;
    ctx.gpr[17] = object;
    const auto call_slot = [&](std::uint32_t offset, std::uint32_t flag) {
        ctx.gpr[3] = word(rt, object);
        (void)call_set(rt, ctx, slot_of(rt, object, offset), {object, flag});
    };
    if (kind == 1u) {
        call_slot(0x10u, 0u);
        call_slot(0x1Cu, 1u);
    } else if (kind == 2u) {
        call_slot(0x10u, 0u);
        call_slot(0x1Cu, 0u);
    } else if (kind == 0u) {
        call_slot(0x10u, 1u);
        call_slot(0x1Cu, 0u);
    }
    ctx.gpr[2] = entry;
    const std::uint32_t used = word(rt, entry + 0xCu);
    ctx.gpr[4] = used;
    if (used != 0u) return;
    const std::uint32_t found = call_set(rt, ctx, 0x088D62FCu, {word(rt, self) + 4u});
    if (found != 0u) return;
    const std::uint32_t created = call_set(rt, ctx, 0x088D30B0u, {self});
    put(rt, object + 0x10u, created);
}

// func_08898918(this, a1, a2, a3): with p = this[0x3C] and q = this[0x38] both non-zero: q[0x10] = a1; r = func_088CBFD4(), func_088CB44C(r, this[0x38]), r2 = func_088CA548(), func_088C97C4(r2, this[0x38]); a record of five words at sp is zeroed for func_088C4D5C(this[0x38], sp); when a2 (a pointer) and *a2 are non-zero the word sp[4]
// is *(*a2 + 4); the virtual slot 0xC of this[0x3C] (p, sp) runs. Then, when o = this[0x40] is non-zero: c = func_088B1330() decides whether the virtual slot 0x10 of func_088B1330()'s object runs (object, q == 0 ? 0 : q + 8) and the record sp (zeroed again, nine words: sp[0] = this[0x3C], sp[4] = q ? q + 8 : 0, sp[8] = q ? q + 0xC : 0, sp[0xC] = a3, sp[0x10] = 1,
// sp[0x14..0x20] = the four words at *a2 + 8 when a2 and *a2 are non-zero, else 0) is passed to the virtual slot 8 of o (o, sp). v0 is that call's result.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (q == 0u)`; `const std::uint32_t inner = a2 != 0u ? word(rt, a2) : 0u`; `if (a2 != 0u && inner != 0u)`; `if (enabled != 0u)`; `const std::uint32_t selector = q != 0u ? q + 8u : 0u`; `put(rt, record + 4u, q != 0u ? q + 8u : 0u)`; `put(rt, record + 8u, q != 0u ? q + 0xCu : 0u)`; `if (a2 != 0u)`; `if (source != 0u)`
void func_08898918(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t record = frame.at(0u);
    ctx.gpr[18] = a3;
    ctx.gpr[17] = a2;
    ctx.gpr[16] = self;
    std::uint32_t o;
    const std::uint32_t p = word(rt, self + 0x3Cu);
    ctx.gpr[2] = p;
    bool skip = (p == 0u);
    if (!skip) {
        const std::uint32_t q = word(rt, self + 0x38u);
        ctx.gpr[2] = q;
        if (q == 0u) {
            skip = true;
            ctx.gpr[2] = word(rt, self + 0x40u);  // the delay slot of the beql (taken)
        } else {
            put(rt, q + 0x10u, a1);  // the delay slot of the jal
            const std::uint32_t r = call_set(rt, ctx, 0x088CBFD4u, {self});
            ctx.gpr[4] = r;
            ctx.gpr[5] = word(rt, self + 0x38u);
            (void)call_set(rt, ctx, 0x088CB44Cu, {r, word(rt, self + 0x38u)});
            const std::uint32_t r2 = call_set(rt, ctx, 0x088CA548u, {r, word(rt, self + 0x38u)});
            ctx.gpr[5] = word(rt, self + 0x38u);
            (void)call_set(rt, ctx, 0x088C97C4u, {r2, word(rt, self + 0x38u)});
            ctx.gpr[4] = word(rt, self + 0x38u);
            ctx.gpr[5] = record;
            for (std::uint32_t offset = 0u; offset < 0x14u; offset += 4u) put(rt, record + offset, 0u);
            (void)call_set(rt, ctx, 0x088C4D5Cu, {word(rt, self + 0x38u), record});
            const std::uint32_t inner = a2 != 0u ? word(rt, a2) : 0u;
            ctx.gpr[2] = inner;
            if (a2 != 0u && inner != 0u) {
                ctx.gpr[2] = word(rt, inner + 4u);
                put(rt, record + 4u, ctx.gpr[2]);
            }
            const std::uint32_t holder = word(rt, self + 0x3Cu);
            ctx.gpr[3] = word(rt, holder);
            ctx.gpr[2] = slot_of(rt, holder, 0xCu);
            ctx.gpr[5] = record;  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot_of(rt, holder, 0xCu), {holder, record});
        }
    }
    (void)skip;
    o = word(rt, self + 0x40u);
    ctx.gpr[2] = o;
    if (o == 0u) return;
    const std::uint32_t enabled = call_set(rt, ctx, 0x088B1330u, {});
    ctx.gpr[2] = 1u;  // the delay slot of the beqz
    if (enabled != 0u) {
        const std::uint32_t object = call_set(rt, ctx, 0x088B1330u, {});
        const std::uint32_t q = word(rt, self + 0x38u);
        const std::uint32_t selector = q != 0u ? q + 8u : 0u;  // the movz in the delay slot of the jalr
        ctx.gpr[3] = q;
        (void)call_set(rt, ctx, slot_of(rt, object, 0x10u), {object, selector});
        ctx.gpr[2] = 1u;
    }
    put(rt, record + 0x10u, 1u);
    for (std::uint32_t offset = 0u; offset < 0x24u; offset += 4u) {
        if (offset != 0x10u) put(rt, record + offset, 0u);
    }
    const std::uint32_t q = word(rt, self + 0x38u);
    const std::uint32_t first = word(rt, self + 0x3Cu);
    put(rt, record + 0xCu, a3);
    put(rt, record, first);
    put(rt, record + 4u, q != 0u ? q + 8u : 0u);
    put(rt, record + 8u, q != 0u ? q + 0xCu : 0u);  // the delay slot of the beqz
    if (a2 != 0u) {
        const std::uint32_t source = word(rt, a2);
        if (source != 0u) {
            for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, record + 0x14u + 4u * i, word(rt, source + 8u + 4u * i));
        }
    }
    const std::uint32_t owner = word(rt, self + 0x40u);
    ctx.gpr[5] = record;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot_of(rt, owner, 8u), {owner, record});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// The assignment of a counted pointer (`*slot = incoming`, the caller has checked that the two differ): the counter of `incoming` (at +4) goes up when it is non-zero; the old object loses one at +4 and, when that was the last, its virtual slot 8 runs and the counter at +8 goes down; when that was the last too,
// its virtual slot 0xC runs. The store of `incoming` comes last. The counters are ll / sc loops (one thread: a load and a store). s1 = incoming, s0 = the old object, s4 = the slot, as in the originals.
inline void assign_counted_pointer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t incoming) {
    ctx.gpr[17] = incoming;
    ctx.gpr[20] = slot;
    if (incoming != 0u) {
        put(rt, incoming + 4u, word(rt, incoming + 4u) + 1u);
        ctx.gpr[3] = incoming + 4u;
        ctx.gpr[2] = 1u;
    }
    const std::uint32_t old = word(rt, slot);
    ctx.gpr[16] = old;
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

// func_088FEB9C(this, source): w = this[0]; w[0x10] = source[0]; the counted pointer w[0x14] becomes source[4] (when they differ), w[0x18] = source[8], the counted pointer w[0x1C] becomes source[0xC] (when they differ) and the byte this[0xB] = 1 (v0 = 1).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_088FEB9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = self;
    ctx.gpr[18] = source;
    const std::uint32_t w = word(rt, self);
    const std::uint32_t current = word(rt, w + 0x14u);
    const std::uint32_t fields = w + 0x10u;  // s3
    put(rt, fields, word(rt, source));
    ctx.gpr[19] = fields;
    const std::uint32_t first = word(rt, source + 4u);
    ctx.gpr[17] = first;
    ctx.gpr[20] = w + 0x14u;  // the delay slot of the beq
    if (first != current) assign_counted_pointer(rt, ctx, w + 0x14u, first);
    const std::uint32_t second_value = word(rt, source + 8u);
    const std::uint32_t current2 = word(rt, fields + 0xCu);
    put(rt, fields + 8u, second_value);
    const std::uint32_t second = word(rt, source + 0xCu);
    ctx.gpr[17] = second;
    ctx.gpr[20] = fields + 0xCu;  // the delay slot of the beq
    if (second != current2) assign_counted_pointer(rt, ctx, fields + 0xCu, second);
    ctx.gpr[2] = 1u;
    put8(rt, self + 0xBu, 1u);
}

// func_088F9F00(this): o = this[0]; each word e of the vector o[0x10 .. 0x14) goes to func_08B03A40(o[0x1C], e) and the vector (func_08BBB908(o + 0x10, o[0x10], o[0x14])) is destroyed; each halfword (at a 4-byte stride) of the vector o[4 .. 8) goes to func_08B03A04(o[0x1C], half) and func_08BBB6F8(o + 4, o[4], o[8]) runs.
// Then the two vectors are moved out: the triples o[0x10..0x1C) and o[4..0x10) go to locals (and become zero), func_08BBB838(first, last, sp + 1) and func_08BBBA34(first, last, sp) destroy them, and a non-zero first element is freed with func_08B35254(first) in each case.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_088F9F00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p10 = frame.at(0x10u);
    ctx.gpr[17] = self;
    std::uint32_t o = word(rt, self);
    ctx.gpr[3] = o;
    std::uint32_t begin = word(rt, o + 0x10u), end = word(rt, o + 0x14u);
    ctx.gpr[5] = begin;
    ctx.gpr[2] = end;
    if (begin != end) {
        std::uint32_t cursor = begin;
        do {
            ctx.gpr[16] = cursor + 4u;  // the delay slot of the jal
            ctx.gpr[4] = word(rt, o + 0x1Cu);
            (void)call_set(rt, ctx, 0x08B03A40u, {word(rt, o + 0x1Cu), word(rt, cursor)});
            cursor += 4u;
            o = word(rt, self);
            ctx.gpr[3] = o;
            end = word(rt, o + 0x14u);
            ctx.gpr[2] = end;
            ctx.gpr[5] = cursor;  // the delay slot of the bne
        } while (cursor != end);
    }
    (void)call_set(rt, ctx, 0x08BBB908u, {o + 0x10u, word(rt, o + 0x10u), word(rt, o + 0x14u)});
    o = word(rt, self);
    ctx.gpr[3] = o;
    begin = word(rt, o + 4u);
    end = word(rt, o + 8u);
    ctx.gpr[5] = begin;
    ctx.gpr[2] = end;
    ctx.gpr[4] = o + 4u;  // the delay slot of the beq
    if (begin != end) {
        std::uint32_t cursor = begin;
        ctx.gpr[16] = begin;
        do {
            ctx.gpr[16] = cursor + 4u;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08B03A04u, {word(rt, o + 0x1Cu), sext16(half_u(rt, cursor))});
            cursor += 4u;
            o = word(rt, self);
            ctx.gpr[3] = o;
            end = word(rt, o + 8u);
            ctx.gpr[2] = end;
            ctx.gpr[5] = cursor;  // the delay slot of the bne
        } while (cursor != end);
        ctx.gpr[4] = o + 4u;
        ctx.gpr[5] = word(rt, o + 4u);
    }
    (void)call_set(rt, ctx, 0x08BBB6F8u, {o + 4u, word(rt, o + 4u), word(rt, o + 8u)});
    o = word(rt, self);
    ctx.gpr[3] = o;
    put(rt, frame.at(0x14u), 0u);
    ctx.gpr[6] = frame.at(1u);
    put(rt, p10, 0u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, p10, word(rt, o + 0x10u));
    put(rt, o + 0x10u, 0u);
    ctx.gpr[4] = 0u;
    put(rt, frame.at(0x14u), word(rt, o + 0x14u));
    put(rt, o + 0x14u, 0u);
    ctx.gpr[5] = 0u;
    put(rt, frame.at(0x18u), word(rt, o + 0x18u));
    put(rt, o + 0x18u, 0u);
    (void)call_set(rt, ctx, 0x08BBB838u, {word(rt, p10), word(rt, frame.at(0x14u)), frame.at(1u)});
    const std::uint32_t first = word(rt, p10);
    ctx.gpr[4] = first;
    if (first != 0u) (void)call_set(rt, ctx, 0x08B35254u, {first});
    o = word(rt, self);
    ctx.gpr[3] = o;
    put(rt, frame.at(8u), 0u);
    ctx.gpr[6] = p0;
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(4u), word(rt, o + 4u));
    put(rt, o + 4u, 0u);
    ctx.gpr[4] = 0u;
    put(rt, frame.at(8u), word(rt, o + 8u));
    put(rt, o + 8u, 0u);
    ctx.gpr[5] = 0u;
    put(rt, frame.at(0xCu), word(rt, o + 0xCu));
    put(rt, o + 0xCu, 0u);
    (void)call_set(rt, ctx, 0x08BBBA34u, {word(rt, frame.at(4u)), word(rt, frame.at(8u)), p0});
    const std::uint32_t second = word(rt, frame.at(4u));
    ctx.gpr[4] = second;
    if (second != 0u) (void)call_set(rt, ctx, 0x08B35254u, {second});
}

// func_088F03E4(this, dest): count = the virtual slot 0x40 of this (this) is the third argument of func_08B36018(dest, 0, count); the pair at sp + 4 = {dest...}: sp[0] = this[0xCC][4], sp[4] = dest, then func_08BB8ABC(sp + 4, this + 0xC8), func_08BB8ABC(sp + 4, this + 0xC9), func_08BB8ABC(result, this + 0xCA), func_08BB8B14(sp + 4, this + 0xCB),
// func_08BB8AE8(sp + 4, sp) (which sets the count n = sp[0]). With n != 0: func_088F1268(this[0xCC][0] + 0x54 * i, sp + 4) for i < n, func_088F0F9C(this[0xCC][8] + 8 * i, sp + 4) for i < n, and func_08BB8B14(sp + 4, this[0xD4] + i) for i < n.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (n == 0u) [the early return]`; `} while (i < n)`
void func_088F03E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], dest = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), slot = self + 0xCCu;
    ctx.gpr[20] = slot;
    ctx.gpr[19] = p4;
    ctx.gpr[18] = self;
    ctx.gpr[16] = dest;  // the delay slot of the jalr
    const std::uint32_t count = call_set(rt, ctx, slot_of(rt, self, 0x40u), {self});
    (void)call_set(rt, ctx, 0x08B36018u, {dest, 0u, count});
    const std::uint32_t table = word(rt, slot);
    const std::uint32_t head = word(rt, table + 4u);
    put(rt, p4, dest);
    put(rt, p0, head);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BB8ABCu, {p4, self + 0xC8u});
    const std::uint32_t next = call_set(rt, ctx, 0x08BB8ABCu, {p4, self + 0xC9u});
    const std::uint32_t last = call_set(rt, ctx, 0x08BB8ABCu, {next, self + 0xCAu});
    (void)last;
    (void)call_set(rt, ctx, 0x08BB8B14u, {p4, self + 0xCBu});
    (void)call_set(rt, ctx, 0x08BB8AE8u, {p4, p0});
    std::uint32_t n = word(rt, p0);
    ctx.gpr[3] = n;
    if (n == 0u) return;
    std::uint32_t i = 0u, offset = 0u;
    ctx.gpr[17] = 0u;
    ctx.gpr[16] = 0u;
    do {
        const std::uint32_t array = word(rt, slot);
        ctx.gpr[5] = p4;
        ++i;
        ctx.gpr[17] = i;
        (void)call_set(rt, ctx, 0x088F1268u, {word(rt, array) + offset, p4});
        n = word(rt, p0);
        ctx.gpr[3] = n;
        offset += 0x54u;  // the delay slot of the bnez
        ctx.gpr[16] = offset;
    } while (i < n);
    if (n == 0u) return;
    i = 0u;
    ctx.gpr[16] = 0u;
    do {
        const std::uint32_t array = word(rt, slot);
        ctx.gpr[3] = i << 3;
        ctx.gpr[5] = p4;
        ++i;
        ctx.gpr[16] = i;
        (void)call_set(rt, ctx, 0x088F0F9Cu, {word(rt, array + 8u) + ((i - 1u) << 3), p4});
        n = word(rt, p0);
        ctx.gpr[3] = n;
    } while (i < n);
    if (n == 0u) return;
    i = 0u;
    ctx.gpr[16] = 0u;
    do {
        const std::uint32_t base = word(rt, self + 0xD4u);
        ctx.gpr[5] = base + i;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08BB8B14u, {p4, base + i});
        n = word(rt, p0);
        ctx.gpr[2] = (i + 1u) < n ? 1u : 0u;
        ++i;
        ctx.gpr[16] = i;
    } while (i < n);
}

// func_088F024C(this, pair): the checks this[4], [8], [0xC], [0x10], [0x14], [0x18] all non-zero, the byte this[0x24] and this[0xE8] non-zero, func_088D9CCC(this[4]) == 0 and func_088DAA84(this[4]) != 0, then with x = pair[0] and y = pair[4]: x >= 0 and x < w = this[0xCC][4], y < 0 or y >= w,
// the signed byte k = this[0xD4][x] non-zero; then n = func_088F12C0(this[0xCC][0] + 84 * x, x, k, this + 4, pair) and for j below n func_088EFD54(this, e[4], e[0], x) with e = this[0xCC][0] + 84 * x + 4 + 8 * j. Every other case returns at once with the v0 the original has there.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_088F024C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], pair = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = pair;
    ctx.gpr[18] = self;
    const std::uint32_t item = word(rt, self + 4u);
    ctx.gpr[17] = item;
    ctx.gpr[16] = self + 4u;  // the delay slot of the beqz
    if (item == 0u) return;
    for (const std::uint32_t offset : {8u, 0xCu, 0x10u, 0x14u, 0x18u}) {  // s0 + 4 ... s0 + 0x14 with s0 = this + 4
        const std::uint32_t value = word(rt, self + offset);
        ctx.gpr[2] = value;
        if (value == 0u) return;
    }
    const std::uint32_t byte = byte_u(rt, self + 0x24u);
    ctx.gpr[2] = byte;
    if (byte == 0u) return;
    const std::uint32_t last_word = word(rt, self + 0xE8u);
    ctx.gpr[2] = last_word;
    if (last_word == 0u) return;
    ctx.gpr[4] = item;  // the delay slot of the jal
    const std::uint32_t blocked = call_set(rt, ctx, 0x088D9CCCu, {item});
    if (blocked != 0u) return;
    ctx.gpr[4] = item;
    const std::uint32_t usable = call_set(rt, ctx, 0x088DAA84u, {item});
    ctx.gpr[20] = self + 0xCCu;  // the delay slot of the beqz
    if (usable == 0u) return;
    std::uint32_t table = word(rt, self + 0xCCu);
    ctx.gpr[4] = table;
    const std::uint32_t x = word(rt, pair), y = word(rt, pair + 4u);
    ctx.gpr[17] = x;
    ctx.gpr[5] = y;
    if (signed_of(x) < 0) return;
    const std::uint32_t width = word(rt, table + 4u);
    ctx.gpr[3] = width;
    const bool in_range = signed_of(x) < signed_of(width);
    ctx.gpr[2] = in_range ? 1u : 0u;
    if (!in_range) return;
    if (signed_of(y) >= 0) {
        const bool inside = signed_of(y) < signed_of(width);
        ctx.gpr[2] = inside ? 1u : 0u;
        if (inside) return;
    }
    const std::uint32_t flags = word(rt, self + 0xD4u) + x;
    ctx.gpr[2] = flags;
    const std::uint32_t kind = sext8(byte_u(rt, flags));
    ctx.gpr[6] = kind;
    if (kind == 0u) return;
    const std::uint32_t offset = x * 84u;
    ctx.gpr[3] = x * 96u;
    ctx.gpr[2] = x * 12u;
    table = word(rt, table);
    ctx.gpr[21] = offset;
    ctx.gpr[8] = pair;
    ctx.gpr[7] = self + 4u;
    ctx.gpr[5] = x;  // the delay slot of the jal
    const std::uint32_t count = call_set(rt, ctx, 0x088F12C0u, {table + offset, x, kind, self + 4u, pair});
    ctx.gpr[19] = count;  // the delay slot of the beqz
    if (count == 0u) return;
    for (std::uint32_t j = 0u; j < count; ++j) {
        ctx.gpr[16] = j;
        const std::uint32_t array = word(rt, self + 0xCCu);
        const std::uint32_t first = word(rt, array);
        const std::uint32_t element = offset + first + 4u + (j << 3);
        ctx.gpr[16] = j + 1u;
        ctx.gpr[7] = x;
        ctx.gpr[6] = word(rt, element);
        (void)call_set(rt, ctx, 0x088EFD54u, {self, word(rt, element + 4u), word(rt, element), x});
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08905D18(this): func_08905BE0(w) with w = this[0x48]; each non-zero pointer w[4], w[8], w[0xC], w[0x10], w[0x14] goes to func_08AF98F0(pointer, this[0x58 / 0x68 / 0x78 / 0x88 / 0x98]) (w is read again after each call) and a non-zero w[0x84] to func_08AF771C(pointer, this[0xB8]). When w[0x84] is zero afterwards and the record r = this[0xA8] is non-zero with
// the five signed bytes r[0] == 0x44, r[0x54] == 0x44, r[0x55] == 0x58, r[0x56] == 0x54, r[0x57] == 0x31: r[0] = 0x64 and, with n = (r[0xC] * r[0x10]) / 16 (the 32-bit product, rounded toward zero), the n word pairs from r + 0x80 are swapped (r[0x80 + 8i] <-> r[0x84 + 8i]). v0 is the last value the original leaves there (see the code).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (pointer != 0u)`; `if (last != 0u) [the early return]`; `const std::uint32_t record = word(rt, self + 0xA8u)`; `if (record == 0u) [the early return]`; `if (first != 0x44u) [the early return]`; `if (ctx.gpr[2] != first) [the early return]`; `if (ctx.gpr[3] != 0x58u) [the early return]`; `if (ctx.gpr[3] != 0x54u) [the early return]`; `if (ctx.gpr[3] != 0x31u) [the early return]`; `const std::uint32_t width_times_height = word(rt, record + 0xCu) * word(rt, record + 0x10u)`; `if (signed_of(pairs) <= 0) [the early return]`; `std::uint32_t cursor = record + 0x80u`; `for (std::uint32_t i = 0u; i < pairs; ++i)`
void func_08905D18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    const std::uint32_t fields = self + 0x40u;  // s0
    ctx.gpr[16] = fields;
    (void)call_set(rt, ctx, 0x08905BE0u, {word(rt, fields + 8u)});
    std::uint32_t w = word(rt, fields + 8u);
    ctx.gpr[2] = w;
    const struct Step {
        std::uint32_t field, source;
    } steps[] = {{4u, 0x58u}, {8u, 0x68u}, {0xCu, 0x78u}, {0x10u, 0x88u}, {0x14u, 0x98u}};
    for (const Step &step : steps) {
        std::uint32_t pointer = word(rt, w + step.field);
        ctx.gpr[4] = pointer;
        if (pointer != 0u) {
            ctx.gpr[5] = word(rt, self + step.source);  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08AF98F0u, {pointer, word(rt, self + step.source)});
            w = word(rt, fields + 8u);
            ctx.gpr[2] = w;
        }
    }
    std::uint32_t pointer = word(rt, w + 0x84u);
    ctx.gpr[4] = pointer;
    if (pointer != 0u) {
        ctx.gpr[5] = word(rt, self + 0xB8u);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08AF771Cu, {pointer, word(rt, self + 0xB8u)});
        w = word(rt, fields + 8u);
        ctx.gpr[2] = w;
    }
    std::uint32_t last = word(rt, w + 0x84u);
    ctx.gpr[2] = last;
    if (last != 0u) return;
    const std::uint32_t record = word(rt, self + 0xA8u);
    ctx.gpr[5] = record;
    if (record == 0u) return;
    ctx.gpr[2] = 0x44u;
    const std::uint32_t first = sext8(byte_u(rt, record));
    ctx.gpr[3] = first;
    if (first != 0x44u) return;
    ctx.gpr[2] = sext8(byte_u(rt, record + 0x54u));
    if (ctx.gpr[2] != first) return;
    ctx.gpr[3] = sext8(byte_u(rt, record + 0x55u));
    ctx.gpr[2] = 0x58u;
    if (ctx.gpr[3] != 0x58u) return;
    ctx.gpr[3] = sext8(byte_u(rt, record + 0x56u));
    ctx.gpr[2] = 0x54u;
    if (ctx.gpr[3] != 0x54u) return;
    ctx.gpr[3] = sext8(byte_u(rt, record + 0x57u));
    ctx.gpr[2] = 0x31u;
    if (ctx.gpr[3] != 0x31u) return;
    const std::uint32_t width_times_height = word(rt, record + 0xCu) * word(rt, record + 0x10u);
    put8(rt, record, 0x64u);
    ctx.gpr[3] = signed_of(width_times_height) < 0 ? 1u : 0u;
    ctx.gpr[4] = width_times_height + 0xFu;
    ctx.gpr[2] = signed_of(width_times_height) < 0 ? width_times_height + 0xFu : width_times_height;
    const std::uint32_t pairs = static_cast<std::uint32_t>(signed_of(ctx.gpr[2]) >> 4);
    ctx.gpr[6] = pairs;
    ctx.gpr[5] = record + 0x80u;  // the delay slot of the blez
    if (signed_of(pairs) <= 0) return;
    std::uint32_t cursor = record + 0x80u;
    ctx.gpr[4] = 0u;
    for (std::uint32_t i = 0u; i < pairs; ++i) {
        const std::uint32_t low = word(rt, cursor), high = word(rt, cursor + 4u);
        ctx.gpr[2] = low;
        ctx.gpr[3] = high;
        put(rt, cursor + 4u, low);
        put(rt, cursor, high);
        cursor += 8u;  // the delay slot of the bne
        ctx.gpr[5] = cursor;
        ctx.gpr[4] = i + 1u;
    }
}

// func_0890B62C(this): func_0890B3D0(this); the counted object c = this[0x100] (when non-zero) loses one reference at c + 4 and, when that was the last, its virtual slot 8 runs, the counter at c + 8 goes down and, when that was the last too, its virtual slot 0xC runs. Then the pairs (object, key) at this + 0x28, + 0x20, + 0x18, + 0x10, + 8, + 0 are released from the first:
// when the object at pair[4] is non-zero its virtual slot 0 runs with (object, pair[0]) and both words of the pair are zeroed.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (counted != 0u)`; `if (atomic_decrement(rt, ctx, counted + 4u) == 1u)`; `if (atomic_decrement(rt, ctx, counted + 8u) == 1u)`; `if (object == 0u) [the early return]`
void func_0890B62C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[16] = self;
    (void)call_set(rt, ctx, 0x0890B3D0u, {self});
    const std::uint32_t counted = word(rt, self + 0x100u);
    ctx.gpr[17] = counted;
    ctx.gpr[3] = counted + 4u;  // the delay slot of the beqz
    std::uint32_t pair = self + 0x28u;
    if (counted != 0u) {
        if (atomic_decrement(rt, ctx, counted + 4u) == 1u) {
            ctx.gpr[2] = 1u;
            (void)call_set(rt, ctx, slot_of(rt, counted, 8u), {counted});
            ctx.gpr[3] = counted + 8u;
            if (atomic_decrement(rt, ctx, counted + 8u) == 1u) {
                ctx.gpr[2] = 1u;
                (void)call_set(rt, ctx, slot_of(rt, counted, 0xCu), {counted});
            }
        }
    }
    ctx.gpr[17] = pair;
    for (const std::uint32_t offset : {0x28u, 0x20u, 0x18u, 0x10u, 8u}) {
        pair = self + offset;
        ctx.gpr[17] = pair;
        const std::uint32_t object = word(rt, pair + 4u);
        ctx.gpr[4] = object;
        if (object != 0u) {
            ctx.gpr[3] = word(rt, object);
            ctx.gpr[5] = word(rt, self + offset);  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot_of(rt, object, 0u), {object, word(rt, self + offset)});
            put(rt, pair + 4u, 0u);
            put(rt, self + offset, 0u);
        }
    }
    const std::uint32_t object = word(rt, self + 4u);
    ctx.gpr[4] = object;
    if (object == 0u) return;
    ctx.gpr[5] = word(rt, self);  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot_of(rt, object, 0u), {object, word(rt, self)});
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
}

// func_08918170(this, k, flag): with u = func_08917888(this): u != 0 selects (a1, a2) by the bit 1 << k when k < 0xF: bits 0x6485 give (0xB, 0), bits 0xB72 give (0xB, 1), the others (3, -1); k >= 0xF gives (3, -1). With u == 0 and func_08917890(this) != 0 it is (0xC, -1); otherwise w = func_08917898(this):
// w != 0 returns (v0 = k < 0xD) when the low byte of `flag` is non-zero, else (3, -1); w == 0 selects by 1 << k (k < 0xD): bits 0x25A give (5, -1), bits 0x1924 give (0, -1), all else and k >= 0xD give (0xB, 0) when the flag byte is zero and (0, -1) otherwise. The pair (a1, a2) is the tail call func_08917ECC(this, a1, a2).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first != 0u)`; `if (k >= 0xFu)`; `the else branch of the test above`; `if ((bit & 0x6485u) != 0u)`; `else if ((bit & 0xB72u) == 0u)`; `if (second != 0u)`; `const std::uint32_t below = k < 0xDu ? 1u : 0u`; `if (third != 0u)`; `if (flag != 0u) [the early return]`; `a1 = 3u`; `if (!plain)`; `if ((bit & 0x25Au) != 0u)`; `if ((bit & 0x1924u) != 0u)`; `if (plain)`; and 1 more of the same kind
void func_08918170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], k = ctx.gpr[5];
    const std::uint32_t flag = ctx.gpr[6] & 0xFFu;
    std::uint32_t a1 = 0u, a2 = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = flag;
        ctx.gpr[17] = self;
        ctx.gpr[16] = k;  // the delay slot of the jal
        const std::uint32_t first = call_set(rt, ctx, 0x08917888u, {self});
        if (first != 0u) {
            ctx.gpr[4] = self;  // the delay slot of the beqz
            if (k >= 0xFu) {
                ctx.gpr[2] = 0u;
                a1 = 3u;
                a2 = 0xFFFFFFFFu;
            } else {
                const std::uint32_t bit = 1u << k;
                ctx.gpr[2] = bit;
                ctx.gpr[3] = bit & 0x6485u;
                if ((bit & 0x6485u) != 0u) {
                    a1 = 0xBu;
                    a2 = 0u;
                } else if ((bit & 0xB72u) == 0u) {
                    ctx.gpr[2] = 0u;
                    a1 = 3u;
                    a2 = 0xFFFFFFFFu;
                } else {
                    ctx.gpr[2] = bit & 0xB72u;
                    a1 = 0xBu;
                    a2 = 1u;
                }
            }
        } else {
            ctx.gpr[4] = self;  // the delay slot of the jal
            const std::uint32_t second = call_set(rt, ctx, 0x08917890u, {self});
            ctx.gpr[4] = self;  // the delay slot of the beqz
            if (second != 0u) {
                a1 = 0xCu;
                a2 = 0xFFFFFFFFu;
            } else {
                ctx.gpr[4] = self;  // the delay slot of the jal
                const std::uint32_t third = call_set(rt, ctx, 0x08917898u, {self});
                const std::uint32_t below = k < 0xDu ? 1u : 0u;  // the delay slot of the beqz
                ctx.gpr[2] = below;
                ctx.gpr[4] = self;
                if (third != 0u) {
                    if (flag != 0u) return;
                    a1 = 3u;
                    a2 = 0xFFFFFFFFu;
                } else {
                    // The v0 the original has here is `below`; the branch to the table test needs k < 0xD.
                    ctx.gpr[2] = 1u;  // the delay slot of the beqz
                    bool plain = (below == 0u);
                    if (!plain) {
                        const std::uint32_t bit = 1u << k;
                        ctx.gpr[2] = bit;
                        ctx.gpr[3] = bit & 0x25Au;
                        if ((bit & 0x25Au) != 0u) {
                            a1 = 5u;
                            a2 = 0xFFFFFFFFu;
                        } else {
                            ctx.gpr[2] = bit & 0x1924u;
                            ctx.gpr[5] = 0u;  // the delay slot of the beqz
                            if ((bit & 0x1924u) != 0u) {
                                a1 = 0u;
                                a2 = 0xFFFFFFFFu;
                            } else {
                                plain = true;
                            }
                        }
                    }
                    if (plain) {
                        if (flag == 0u) {
                            a1 = 0xBu;
                            a2 = 0u;
                        } else {
                            a1 = 0u;
                            a2 = 0xFFFFFFFFu;
                        }
                    }
                }
            }
        }
        ctx.gpr[5] = a1;
        ctx.gpr[6] = a2;
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08917ECCu, {self, a1, a2});
}

// func_089E4F44(this, a1, event): r = func_0896EFAC(this) (non-zero returns r); k = event[0x28]: the keys 0xFEE4 - 0xFEE7, 0xFF51, 0xFF53, 0xFF96 and 0xFF98 store this[0x1AC] = func_089E4EA0(this) and the keys 0xFEE4 - 0xFEE7, 0xFF52, 0xFF54, 0xFF97 and 0xFF99 store this[0x1B0] = func_089E4EC8(this) (a key in both lists does both);
// v0 is 1 when one of the two was stored and 0 otherwise.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (r != 0u) [the early return]`
void func_089E4F44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], event = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = self;
    ctx.gpr[16] = event;  // the delay slot of the jal
    const std::uint32_t r = call_set(rt, ctx, 0x0896EFACu, {self});
    ctx.gpr[3] = r;  // the delay slot of the bnez
    if (r != 0u) return;
    const std::uint32_t key = word(rt, event + 0x28u);
    ctx.gpr[16] = key;
    std::uint32_t stored = 0u;
    for (const std::uint32_t candidate : {0xFEE4u, 0xFEE5u, 0xFEE6u, 0xFEE7u, 0xFF51u, 0xFF53u, 0xFF96u, 0xFF98u}) {
        if (key == candidate) {
            ctx.gpr[4] = self;
            put(rt, self + 0x1ACu, call_set(rt, ctx, 0x089E4EA0u, {self}));
            stored = 1u;
            break;
        }
    }
    ctx.gpr[3] = stored;
    for (const std::uint32_t candidate : {0xFEE4u, 0xFEE5u, 0xFEE6u, 0xFEE7u, 0xFF52u, 0xFF54u, 0xFF97u, 0xFF99u}) {
        if (key == candidate) {
            ctx.gpr[4] = self;
            put(rt, self + 0x1B0u, call_set(rt, ctx, 0x089E4EC8u, {self}));
            stored = 1u;
            break;
        }
    }
    ctx.gpr[3] = stored;
    ctx.gpr[2] = stored;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08BD4118(this): this[0] = D_08CC5530; the counted object this[0x84] (when non-zero) is released (release_counted); this[0] = D_08CC5550; the counted object this[0x2C] is released the same way; the object a = this[0x24] (when non-zero) loses one reference at a + 8 and, when that was the last, its virtual slot 0xC runs;
// this[0] = D_08CC3000 and the tail call func_08B64654(this).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first != 0u) release_counted(rt, ctx, first)`; `if (second != 0u) release_counted(rt, ctx, second)`; `if (third != 0u)`; `if (atomic_decrement(rt, ctx, third + 8u) == 1u)`
void func_08BD4118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        ctx.gpr[2] = 0x08CC5530u;
        const std::uint32_t first = word(rt, self + 0x84u);
        ctx.gpr[16] = first;
        put(rt, self, 0x08CC5530u);  // the delay slot of the beqz
        if (first != 0u) release_counted(rt, ctx, first);
        const std::uint32_t second = word(rt, self + 0x2Cu);
        ctx.gpr[16] = second;
        ctx.gpr[2] = 0x08CC5550u;
        put(rt, self, 0x08CC5550u);  // the delay slot of the beqz
        if (second != 0u) release_counted(rt, ctx, second);
        const std::uint32_t third = word(rt, self + 0x24u);
        ctx.gpr[4] = third;
        if (third != 0u) {
            ctx.gpr[3] = third + 8u;
            if (atomic_decrement(rt, ctx, third + 8u) == 1u) {
                ctx.gpr[2] = 1u;
                (void)call_set(rt, ctx, slot_of(rt, third, 0xCu), {third});
            }
        }
        ctx.gpr[2] = 0x08CC3000u;
        put(rt, self, 0x08CC3000u);
        ctx.gpr[4] = self;
    }
    tail_call(ctx, 0x08B64654u, {self});
}

// func_08A12658(this, x): with the byte this[0x10] set: when the byte this[0x11] is set the record t (sp + 8, 60 bytes) is a copy of the record func_08B10204(sp + 0x50, *x, 1) builds; a record whose first word is non-zero returns 0; otherwise o = t[0x10] (sp + 0x18) goes to sp[4], s = t[0x20] (sp + 0x28) and the byte sp[0] = 1 and the virtual slot 0x1C of this (this, s, sp[4], sp[0]) runs
// (non-zero answer: return 1; zero: when sp[0] is zero return 0, else func_08B35254(s) and return 0). When the byte this[0x11] is zero func_08A17710(x, this) and func_08A17818() run and 1 is returned. With the byte this[0x10] clear: p = this[0xC] (zero returns 0), s = func_089A3348(p, x, sp, sp + 4) (zero returns 0), then the virtual slot 0x1C of this the same way.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (answer != 0u)`; `const std::uint32_t again = byte_u(rt, p0)`; `if (again == 0u)`; `if (flag10 != 0u)`; `if (flag11 != 0u)`; `for (std::uint32_t offset = 0u; offset < 3u * 16u + 12u; offset += 4u) put(rt, frame.at(8u) + offset, word(rt, record + offset))`; `if (head != 0u)`; `s = word(rt, frame.at(0x28u))`; `(void)call_set(rt, ctx, 0x08A17710u, {x, self})`; `if (holder == 0u)`; `if (found == 0u)`
void func_08A12658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], x = ctx.gpr[5];
    GuestFrame frame(ctx, 0xA0u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u);
    ctx.gpr[17] = self;
    std::uint32_t result = 0u;  // v1
    std::uint32_t s = 0u;
    const auto finish = [&](std::uint32_t value) {
        ctx.gpr[3] = value;
        ctx.gpr[2] = value;
    };
    const auto call_virtual = [&]() {
        const std::uint32_t table = word(rt, self);
        ctx.gpr[3] = table;
        const std::uint32_t sp4 = word(rt, p4);
        const std::uint32_t flag = byte_u(rt, p0);
        ctx.gpr[6] = sp4;
        ctx.gpr[7] = flag;
        ctx.gpr[5] = s;  // the delay slot of the jalr
        const std::uint32_t answer = call_set(rt, ctx, word(rt, table + 0x1Cu), {self, s, sp4, flag});
        ctx.gpr[3] = 1u;  // the delay slot of the bnez
        if (answer != 0u) {
            finish(1u);
            return;
        }
        const std::uint32_t again = byte_u(rt, p0);
        ctx.gpr[2] = again;
        if (again == 0u) {
            finish(0u);  // the delay slot of the beql (taken)
            return;
        }
        ctx.gpr[4] = s;
        (void)call_set(rt, ctx, 0x08B35254u, {s});
        finish(0u);
    };
    const std::uint32_t flag10 = byte_u(rt, self + 0x10u);
    ctx.gpr[2] = flag10;
    ctx.gpr[4] = x;  // the delay slot of the beqz
    if (flag10 != 0u) {
        const std::uint32_t flag11 = byte_u(rt, self + 0x11u);
        ctx.gpr[2] = flag11;
        if (flag11 != 0u) {
            const std::uint32_t record = frame.at(0x50u);
            ctx.gpr[16] = record;
            ctx.gpr[5] = word(rt, x);
            ctx.gpr[6] = 1u;
            (void)call_set(rt, ctx, 0x08B10204u, {record, word(rt, x), 1u});
            for (std::uint32_t offset = 0u; offset < 3u * 16u + 12u; offset += 4u) put(rt, frame.at(8u) + offset, word(rt, record + offset));  // three 16-byte blocks and a 12-byte tail
            ctx.gpr[16] = frame.at(0x80u);
            const std::uint32_t head = word(rt, frame.at(8u));
            ctx.gpr[2] = head;
            if (head != 0u) {
                finish(0u);
                return;
            }
            s = word(rt, frame.at(0x28u));
            ctx.gpr[16] = s;
            ctx.gpr[2] = word(rt, frame.at(0x18u));
            ctx.gpr[3] = 1u;
            put(rt, p4, ctx.gpr[2]);
            put8(rt, p0, 1u);  // the delay slot of the j
            call_virtual();
            return;
        }
        (void)call_set(rt, ctx, 0x08A17710u, {x, self});
        (void)call_set(rt, ctx, 0x08A17818u, {});
        finish(1u);  // the delay slot of the j
        return;
    }
    const std::uint32_t holder = word(rt, self + 0xCu);
    ctx.gpr[4] = holder;
    if (holder == 0u) {
        finish(0u);
        return;
    }
    ctx.gpr[6] = p0;  // the delay slot of the beqz
    const std::uint32_t found = call_set(rt, ctx, 0x089A3348u, {holder, x, p0, p4});
    if (found == 0u) {
        finish(0u);
        return;
    }
    s = found;
    ctx.gpr[16] = s;
    call_virtual();
    (void)result;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_089CE3CC(this, x): func_08A01C30(); u = func_08968C38(this) (zero returns v0 = 0); under the lock D_08CF5134 (func_08B344E0 .. the tail call func_08B3457C(D_08CF5134)): k = this[0x198]; func_089CE170(this, k) < 0 ends; for i from lo = this[0x1B0] while i <= hi = this[0x1B4] (both read again after a call), unless hi < lo:
// when i != k, r = func_089CE23C(this, i) and a non-zero r goes to func_08A01B7C(this, x, r) (k and hi are read again after it); then, with lo = this[0x1B0], k inside [lo, hi] (the registers keep the values the original compares), r = func_089CE23C(this, k) and a non-zero r goes to func_08A01B7C(this, x, r).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (usable == 0u) [the early return]`; `if (signed_of(found) < 0) [the jump to the unlock]`; `if (!(signed_of(high) < signed_of(low)))`; `if (cell != 0u)`; `the else branch of the test above`; `if (signed_of(s2) < signed_of(low)) [the jump to the unlock]`; `if (signed_of(high) < signed_of(s2)) [the jump to the unlock]`; `if (cell != 0u) [the call it guards]`
void func_089CE3CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], third = ctx.gpr[5];
    constexpr std::uint32_t lock = 0x08CF5134u;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        ctx.gpr[19] = third;
        (void)call_keep(rt, ctx, 0x08A01C30u, {});
        const std::uint32_t usable = call_set(rt, ctx, 0x08968C38u, {self});
        if (usable == 0u) return;
        (void)call_set(rt, ctx, 0x08B344E0u, {lock});
        std::uint32_t &s0 = ctx.gpr[16];
        std::uint32_t &s2 = ctx.gpr[18];
        s2 = word(rt, self + 0x198u);
        const std::uint32_t found = call_set(rt, ctx, 0x089CE170u, {self, s2});
        ctx.gpr[4] = lock;  // the delay slot of the bltz
        if (signed_of(found) < 0) goto unlock;
        {
            std::uint32_t low = word(rt, self + 0x1B0u), high = word(rt, self + 0x1B4u);
            ctx.gpr[3] = low;
            ctx.gpr[6] = high;
            ctx.gpr[2] = signed_of(high) < signed_of(low) ? 1u : 0u;
            s0 = low;  // the delay slot of the bnez
            if (!(signed_of(high) < signed_of(low))) {
                for (;;) {
                    ctx.gpr[5] = s0;
                    ctx.gpr[4] = self;  // the delay slot of the beq
                    if (s0 != s2) {
                        const std::uint32_t cell = call_set(rt, ctx, 0x089CE23Cu, {self, s0});
                        ctx.gpr[6] = cell;
                        ctx.gpr[4] = self;
                        ctx.gpr[5] = third;  // the delay slot of the beqz
                        if (cell != 0u) {
                            s0 += 1u;  // the delay slot of the jal
                            (void)call_set(rt, ctx, 0x08A01B7Cu, {self, third, cell});
                            high = word(rt, self + 0x1B4u);
                            ctx.gpr[6] = high;
                            s2 = word(rt, self + 0x198u);  // the delay slot of the j
                        } else {
                            s2 = word(rt, self + 0x198u);
                            high = word(rt, self + 0x1B4u);
                            ctx.gpr[6] = high;
                            s0 += 1u;
                        }
                    } else {
                        s0 += 1u;
                    }
                    ctx.gpr[2] = signed_of(high) < signed_of(s0) ? 1u : 0u;
                    if (signed_of(high) < signed_of(s0)) break;
                }
                low = word(rt, self + 0x1B0u);
                ctx.gpr[3] = low;
            }
            ctx.gpr[2] = signed_of(s2) < signed_of(low) ? 1u : 0u;
            ctx.gpr[4] = lock;  // the delay slot of the bnez
            if (signed_of(s2) < signed_of(low)) goto unlock;
            ctx.gpr[2] = signed_of(high) < signed_of(s2) ? 1u : 0u;
            if (signed_of(high) < signed_of(s2)) goto unlock;
            ctx.gpr[5] = s2;
            const std::uint32_t cell = call_set(rt, ctx, 0x089CE23Cu, {self, s2});
            if (cell != 0u) (void)call_set(rt, ctx, 0x08A01B7Cu, {self, third, cell});
            ctx.gpr[4] = lock;
        }
    unlock:;
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// func_089E5BF4(this, x): func_08A01C30(); u = func_08968C38(this) and this[0x19C] >= 0 (otherwise return); under the lock D_08CF575C (func_08B344E0 .. the tail call func_08B3457C(D_08CF575C)): rows y from this[0x1C0] to this[0x1C4], columns i from this[0x1B8] to this[0x1BC] (both read again as the original does),
// i < this[0x154] and y < this[0x158]: c = func_089E4DE0(this, i, y); c == this[0x19C] only records it (best = c); otherwise q = func_089E583C(this, c) and a non-zero q goes to func_08A01B7C(this, x, q). After the loops a recorded best >= 0 gives q = func_089E583C(this, best): non-zero and different
// from this[0x190] (or this[0x190] zero) goes to func_08A01B7C(this, x, q).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (usable == 0u) [the early return]`; `if (signed_of(ready) < 0) [the early return]`; `if (signed_of(v1) < signed_of(s3)) [the jump to the unlock]`; `if (signed_of(s2) < signed_of(s0))`; `if (!(signed_of(s0) < signed_of(columns)))`; `if (!(signed_of(s3) < signed_of(rows)))`; `if (q == 0u)`; `if (signed_of(s4) < 0)`; `if (q == 0u) [the jump to the unlock]`; `if (known != 0u && q == known)`
void func_089E5BF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], third = ctx.gpr[5];
    constexpr std::uint32_t lock = 0x08CF575Cu;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        ctx.gpr[21] = third;
        (void)call_keep(rt, ctx, 0x08A01C30u, {});
        const std::uint32_t usable = call_set(rt, ctx, 0x08968C38u, {self});
        if (usable == 0u) return;
        const std::uint32_t ready = word(rt, self + 0x19Cu);
        ctx.gpr[2] = ready;
        if (signed_of(ready) < 0) return;
        (void)call_set(rt, ctx, 0x08B344E0u, {lock});
        std::uint32_t &s0 = ctx.gpr[16];
        std::uint32_t &s2 = ctx.gpr[18];
        std::uint32_t &s3 = ctx.gpr[19];
        std::uint32_t &s4 = ctx.gpr[20];
        s3 = word(rt, self + 0x1C0u);
        std::uint32_t v1 = word(rt, self + 0x1C4u);
        ctx.gpr[3] = v1;
        ctx.gpr[2] = signed_of(v1) < signed_of(s3) ? 1u : 0u;
        ctx.gpr[4] = lock;  // the delay slot of the bnel (taken)
        if (signed_of(v1) < signed_of(s3)) goto unlock;
        s2 = word(rt, self + 0x1BCu);
        s4 = 0xFFFFFFFFu;
        s0 = word(rt, self + 0x1B8u);
        for (;;) {
            if (signed_of(s2) < signed_of(s0)) {
                s3 += 1u;  // the delay slot of the bnel (taken)
            } else {
                for (;;) {
                    const std::uint32_t columns = word(rt, self + 0x154u);
                    ctx.gpr[2] = columns;
                    if (!(signed_of(s0) < signed_of(columns))) {
                        s0 += 1u;  // the delay slot of the beql (taken)
                    } else {
                        const std::uint32_t rows = word(rt, self + 0x158u);
                        ctx.gpr[5] = s0;
                        ctx.gpr[6] = s3;
                        ctx.gpr[4] = self;
                        if (!(signed_of(s3) < signed_of(rows))) {
                            s0 += 1u;
                        } else {
                            const std::uint32_t c = call_set(rt, ctx, 0x089E4DE0u, {self, s0, s3});
                            ctx.gpr[3] = c;
                            const std::uint32_t ready_value = word(rt, self + 0x19Cu);
                            ctx.gpr[2] = ready_value;
                            ctx.gpr[5] = c;
                            ctx.gpr[4] = self;
                            if (c == ready_value) {
                                s4 = c;  // the delay slot of the j
                                s0 += 1u;
                            } else {
                                const std::uint32_t q = call_set(rt, ctx, 0x089E583Cu, {self, c});
                                ctx.gpr[6] = q;
                                ctx.gpr[4] = self;
                                ctx.gpr[5] = third;  // the delay slot of the beqz
                                if (q == 0u) {
                                    s2 = word(rt, self + 0x1BCu);
                                    s0 += 1u;
                                } else {
                                    s0 += 1u;  // the delay slot of the jal
                                    (void)call_set(rt, ctx, 0x08A01B7Cu, {self, third, q});
                                    s2 = word(rt, self + 0x1BCu);  // the delay slot of the j
                                }
                            }
                        }
                    }
                    ctx.gpr[2] = signed_of(s2) < signed_of(s0) ? 1u : 0u;
                    if (signed_of(s2) < signed_of(s0)) break;
                }
                v1 = word(rt, self + 0x1C4u);
                ctx.gpr[3] = v1;
                s3 += 1u;
            }
            ctx.gpr[2] = signed_of(v1) < signed_of(s3) ? 1u : 0u;
            if (signed_of(v1) < signed_of(s3)) break;
            s0 = word(rt, self + 0x1B8u);  // the delay slot of the beql (taken)
        }
        ctx.gpr[5] = s4;  // the delay slot of the bltz
        if (signed_of(s4) < 0) {
            ctx.gpr[4] = lock;
            goto unlock;
        }
        ctx.gpr[4] = self;
        {
            const std::uint32_t q = call_set(rt, ctx, 0x089E583Cu, {self, s4});
            ctx.gpr[4] = lock;  // the delay slot of the beql (taken)
            if (q == 0u) goto unlock;
            const std::uint32_t known = word(rt, self + 0x190u);
            ctx.gpr[3] = known;
            ctx.gpr[4] = self;  // the delay slot of the beqz
            if (known != 0u && q == known) {
                ctx.gpr[4] = lock;  // the delay slot of the beql (taken)
                goto unlock;
            }
            ctx.gpr[5] = third;
            (void)call_set(rt, ctx, 0x08A01B7Cu, {self, third, q});
            ctx.gpr[4] = lock;
        }
    unlock:;
    }
    tail_call(ctx, 0x08B3457Cu, {lock});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08B0E358(this, flag): under the lock this + 4 (func_08B34888 .. func_08B34B24) the byte this[0xE9] = 1 and the mode m = this[0xA4] picks: 0 -> func_08B0DF0C(this), this[0xA8] = 1, the virtual slot 0x20 of this; 1 -> when func_08B0DD88(this) is non-zero func_08B11434(this[0x38], this) and, as for 2, this[0xA8] = 1, the virtual slot 0x20, func_08B0E31C(this);
// 2 -> the same with func_08B10CC4(this[0x38], this) in place of func_08B11434; any other mode (or a zero func_08B0DD88) does nothing. After the unlock, when something ran and the low byte of `flag` is set, the function jumps to the virtual slot 0x24 of this (this); otherwise it returns the unlock's v0.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `const auto finish_pair = [&]()`; `if (mode == 1u || mode == 2u)`; `if (ready == 0u)`; `the else branch of the test above`; `(void)call_set(rt, ctx, mode == 1u ? 0x08B11434u : 0x08B10CC4u, {holder, self})`; `else if (mode == 0u)`; `if (ran == 0u || flag == 0u) [the early return]`
void func_08B0E358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5] & 0xFFu;
    std::uint32_t target = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = self + 4u;
        ctx.gpr[16] = self;
        ctx.gpr[19] = flag;
        ctx.gpr[17] = 1u;
        ctx.gpr[4] = self + 4u;
        (void)call_set(rt, ctx, 0x08B34888u, {self + 4u});
        ctx.gpr[2] = 1u;
        const std::uint32_t mode = word(rt, self + 0xA4u);
        ctx.gpr[3] = mode;
        put8(rt, self + 0xE9u, 1u);  // the delay slot of the beq
        std::uint32_t ran = 0u;
        const auto finish_pair = [&]() {
            put(rt, self + 0xA8u, 1u);
            ctx.gpr[3] = word(rt, self);
            ctx.gpr[4] = self;  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot_of(rt, self, 0x20u), {self});
            (void)call_set(rt, ctx, 0x08B0E31Cu, {self});
        };
        if (mode == 1u || mode == 2u) {
            ctx.gpr[4] = self;
            const std::uint32_t ready = call_set(rt, ctx, 0x08B0DD88u, {self});
            if (ready == 0u) {
                ctx.gpr[20] = 0u;  // the delay slot of the beql (taken)
            } else {
                ctx.gpr[20] = 1u;  // the delay slot of the jal
                ran = 1u;
                const std::uint32_t holder = word(rt, self + 0x38u);
                (void)call_set(rt, ctx, mode == 1u ? 0x08B11434u : 0x08B10CC4u, {holder, self});
                finish_pair();
            }
        } else if (mode == 0u) {
            ctx.gpr[4] = self;
            (void)call_set(rt, ctx, 0x08B0DF0Cu, {self});
            put(rt, self + 0xA8u, 1u);
            ctx.gpr[3] = word(rt, self);
            ctx.gpr[4] = self;
            ctx.gpr[20] = 1u;  // the delay slot of the jalr
            ran = 1u;
            (void)call_set(rt, ctx, slot_of(rt, self, 0x20u), {self});
        } else {
            ctx.gpr[20] = 0u;  // the delay slot of the bnel (taken)
        }
        ctx.gpr[4] = self + 4u;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08B34B24u, {self + 4u});
        if (ran == 0u || flag == 0u) return;
        ctx.gpr[2] = word(rt, self);
        ctx.gpr[4] = self;
        target = word(rt, word(rt, self) + 0x24u);
        ctx.gpr[25] = target;
    }
    tail_call(ctx, target, {self});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_0898BDB4(out, a1, mode, p): mode 0: func_089896CC(sp + 8) fills the holder h = sp[8] (null: func_08A5BB7C(D_08CA39F0) first); with the byte h[0x128] set, func_08A85B1C(sp + 4, h[0x12C]) and with it clear func_08A85B1C(sp, -1) build the holder g, which is copied to `out` with func_08A5BAD8(out, g[0]) unless it is `out`, func_08A5B9EC(g) and func_08A5B9EC(sp + 8) end.
// mode 1: after func_089896CC(sp + 8), the object o = *p (null: func_08A5BB7C(D_08CA39E8) first) answers r = the virtual slot 0x3C of o (o); a negative r stores the byte h[0x128] = 0, otherwise h[0x128] = 1 (the low byte of the mode) and h[0x12C] = r; every store checks h for null first (func_08A5BB7C(D_08CA39F0)), then func_08A5B9EC(sp + 8). Any other mode returns at once.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): not registered, see its HOT92 line (read against the asm only).
[[maybe_unused]] void func_0898BDB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], pointer = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u);
    ctx.gpr[18] = out;
    ctx.gpr[17] = pointer;
    ctx.gpr[16] = mode;
    if (mode != 0u) {
        ctx.gpr[2] = 1u;
        if (mode != 1u) return;
        ctx.gpr[19] = p8;
        (void)call_set(rt, ctx, 0x089896CCu, {p8});
        const std::uint32_t object = word(rt, pointer);
        ctx.gpr[17] = object;
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39E8u});
        const std::uint32_t answer = call_set(rt, ctx, slot_of(rt, object, 0x3Cu), {object});
        ctx.gpr[18] = answer;  // the delay slot of the bltz
        const auto holder = [&]() {
            const std::uint32_t h = word(rt, p8);
            ctx.gpr[17] = h;
            if (h == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
            return h;
        };
        if (signed_of(answer) >= 0) {
            const std::uint32_t h = holder();
            put8(rt, h + 0x128u, mode);
            const std::uint32_t g = word(rt, p8);
            ctx.gpr[16] = g;
            if (g == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
            put(rt, g + 0x12Cu, answer);
        } else {
            const std::uint32_t g = word(rt, p8);
            ctx.gpr[16] = g;
            if (g == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
            put8(rt, g + 0x128u, 0u);
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
        return;
    }
    ctx.gpr[19] = p8;
    (void)call_set(rt, ctx, 0x089896CCu, {p8});
    std::uint32_t h = word(rt, p8);
    ctx.gpr[16] = h;
    if (h == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
    const std::uint32_t flag = byte_u(rt, h + 0x128u);
    ctx.gpr[2] = flag;
    if (flag != 0u) {
        h = word(rt, p8);
        ctx.gpr[16] = h;
        if (h == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
        const std::uint32_t key = word(rt, h + 0x12Cu);
        ctx.gpr[16] = p4;
        (void)call_set(rt, ctx, 0x08A85B1Cu, {p4, key});
        ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
        if (out != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p4)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    } else {
        (void)call_set(rt, ctx, 0x08A85B1Cu, {p0, 0xFFFFFFFFu});
        ctx.gpr[5] = word(rt, p0);  // the delay slot of the beq
        if (out != p0) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p0)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
}

// func_08990720(this): this[0] = D_08CCC840, this[0x38] = D_08CCCA9C; the holder at sp + 4 is built by func_08A399C0(sp + 4, D_08CF6ED8), copied to this + 0x12C with func_08A5BAD8 (unless it is the same) and destroyed with func_08A5B9EC. The 127 string pointers from this + 0x424 down to this + 0x228 are released (release_cell with the flag sp + 3). this[0x130] = D_08CCE158,
// the vectors of words at this + 0x214 and this + 0x1FC are destroyed (func_08BEEA84(begin, end, sp + 2 / sp + 1) and, when begin is non-zero, func_08A5BC54(begin, size, 4)), the string at this[0x160] is released (flag sp), func_0897D548(this + 0x130), func_08A5B9EC(this + 0x12C), func_08995714(this) and func_08A5BC54(this, 0x4D0, 4) end.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (self + 0x12Cu != p4) [the call it guards]`; `if (data - 0xCu != 0x08D74FD0u)`; `if (signed_of(references) <= 0) [the call it guards]`; `if (again != 0u) [the call it guards]`
void func_08990720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u);
    ctx.gpr[20] = self + 0x12Cu;
    ctx.gpr[18] = self;
    ctx.gpr[16] = p4;
    put(rt, self, 0x08CCC840u);
    put(rt, self + 0x38u, 0x08CCCA9Cu);
    ctx.gpr[5] = 0x08CF6ED8u;
    (void)call_set(rt, ctx, 0x08A399C0u, {p4, 0x08CF6ED8u});
    ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
    if (self + 0x12Cu != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + 0x12Cu, word(rt, p4)});
    ctx.gpr[19] = self + 0x228u;
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    ctx.gpr[21] = 0x08D74FD0u;
    for (std::uint32_t end = self + 0x428u;; ) {
        const std::uint32_t slot = end - 4u;
        end = slot;
        ctx.gpr[17] = slot;
        ctx.gpr[3] = slot + 4u;
        const std::uint32_t data = word(rt, slot);
        ctx.gpr[2] = data;
        ctx.gpr[5] = 0xFFFFFFFFu;
        ctx.gpr[16] = data - 0xCu;
        ctx.gpr[4] = data - 4u;  // the delay slot of the beq
        if (data - 0xCu != 0x08D74FD0u) {
            const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
            ctx.gpr[4] = data - 0xCu;
            ctx.gpr[5] = frame.at(3u);  // the delay slot of the bgtz
            if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, frame.at(3u)});
        }
        if (slot == self + 0x228u) break;
    }
    put(rt, self + 0x130u, 0x08CCE158u);
    const auto destroy_words = [&](std::uint32_t field, std::uint32_t flag) {
        ctx.gpr[16] = field;
        const std::uint32_t begin = word(rt, field), end = word(rt, field + 4u);
        ctx.gpr[17] = self + 0x130u;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08BEEA84u, {begin, end, flag});
        const std::uint32_t size = word(rt, field + 8u) - word(rt, field);
        const std::uint32_t again = word(rt, field);
        ctx.gpr[4] = again;
        ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(size) >> 2);
        if (again != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {again, static_cast<std::uint32_t>(signed_of(size) >> 2) << 2, 4u});
    };
    destroy_words(self + 0x214u, frame.at(2u));
    destroy_words(self + 0x1FCu, frame.at(1u));
    const std::uint32_t last = word(rt, self + 0x160u);
    ctx.gpr[4] = last;
    release_cell(rt, ctx, last, p0, 16u);
    (void)call_set(rt, ctx, 0x0897D548u, {self + 0x130u});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {self + 0x12Cu});
    (void)call_set(rt, ctx, 0x08995714u, {self});
    (void)call_set(rt, ctx, 0x08A5BC54u, {self, 0x4D0u, 4u});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08998B70(this): func_08994B6C(this); a non-zero this[0x154] goes to func_08A5B930 and this[0x154] = 0, this[0x130] = 0; for i = 0..1 the vector triple at this + 0x158 + 0xC * i is moved out to sp + 0xC (and zeroed), func_08BF0F4C(begin, end, sp) and, when begin is non-zero, func_08A5BC54(begin, size, 4) free it;
// this[0x170 + 4 * i] = 0. this[0x12C] gets bit 2 cleared and the field 3..4 set to 2, this[0x178] = 1.0f, this[0x17C] = 0; the holder sp + 8 is built by func_08A76AAC(sp + 8, D_08CF6ED8) and copied to this + 0x180 with func_08A5BAD8 (unless it is the same) and destroyed with func_08A5B9EC; a non-zero this[0x184] goes to func_089D87E0(this[0x184])
// with f12 = this[0x188]; the holder sp + 4 is built by func_089D8A68(sp + 4, D_08CF6ED8) and copied to this + 0x184 and destroyed the same way.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (first != 0u) [the call it guards]`; `if (self + 0x180u != p8) [the call it guards]`; `if (pointer != 0u)`; `if (self + 0x184u != p4) [the call it guards]`
void func_08998B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[17] = self;
    ctx.gpr[16] = 0u;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08994B6Cu, {self});
    const std::uint32_t buffer = word(rt, self + 0x154u);
    ctx.gpr[4] = buffer;
    if (buffer != 0u) (void)call_set(rt, ctx, 0x08A5B930u, {buffer});
    put(rt, self + 0x154u, 0u);
    ctx.gpr[19] = pC;
    ctx.gpr[18] = self;
    put(rt, self + 0x130u, 0u);
    ctx.gpr[20] = 0u;
    ctx.gpr[21] = 2u;
    for (std::uint32_t i = 0u; i < 2u; ++i) {
        const std::uint32_t entry = self + 0x150u + 0xCu * i;
        ctx.gpr[16] = entry;
        ctx.gpr[18] = self + 4u * i;
        const std::uint32_t begin = word(rt, entry + 8u), end = word(rt, entry + 0xCu), capacity = word(rt, entry + 0x10u);
        put(rt, pC, begin);
        ctx.gpr[6] = p0;
        ctx.gpr[20] = i + 1u;
        put(rt, frame.at(0x10u), end);
        ctx.gpr[7] = capacity;
        put(rt, entry + 8u, 0u);
        put(rt, entry + 0xCu, 0u);
        put(rt, entry + 0x10u, 0u);
        ctx.gpr[16] = entry + 0xCu;
        put(rt, frame.at(0x14u), capacity);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08BF0F4Cu, {begin, end, p0});
        ctx.gpr[6] = 4u;
        const std::uint32_t first = word(rt, pC);
        const std::uint32_t last = word(rt, frame.at(0x14u));
        ctx.gpr[2] = first;
        ctx.gpr[4] = first;
        ctx.gpr[5] = static_cast<std::uint32_t>(signed_of(last - first) >> 2) << 2;
        if (first != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {first, static_cast<std::uint32_t>(signed_of(last - first) >> 2) << 2, 4u});
        put(rt, self + 0x170u + 4u * i, 0u);
        ctx.gpr[18] = self + 4u * (i + 1u);
    }
    std::uint32_t flags = word(rt, self + 0x12Cu);
    flags = (flags & ~0x4u);
    flags = (flags & ~(3u << 3)) | (2u << 3);
    ctx.gpr[3] = 2u;
    ctx.gpr[2] = flags;
    ctx.gpr[18] = p8;
    ctx.gpr[1] = 0x3F800000u;
    ctx.fpr[0] = 1.0f;
    ctx.gpr[16] = self + 0x180u;
    put_float(rt, self + 0x178u, 1.0f);
    ctx.gpr[4] = p8;
    ctx.gpr[5] = 0x08CF6ED8u;
    put(rt, self + 0x12Cu, flags);
    put(rt, self + 0x17Cu, 0u);
    (void)call_set(rt, ctx, 0x08A76AACu, {p8, 0x08CF6ED8u});
    ctx.gpr[5] = word(rt, p8);  // the delay slot of the beq
    if (self + 0x180u != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + 0x180u, word(rt, p8)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    const std::uint32_t pointer = word(rt, self + 0x184u);
    ctx.gpr[4] = pointer;
    ctx.gpr[16] = p4;  // the delay slot of the beqz
    if (pointer != 0u) {
        ctx.fpr[12] = float_at(rt, self + 0x188u);  // the delay slot of the jal
        (void)call_keep(rt, ctx, 0x089D87E0u, {pointer}, {float_at(rt, self + 0x188u)});
        ctx.gpr[16] = p4;
    }
    ctx.gpr[17] = self + 0x184u;
    ctx.gpr[5] = 0x08CF6ED8u;
    (void)call_set(rt, ctx, 0x089D8A68u, {p4, 0x08CF6ED8u});
    ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
    if (self + 0x184u != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + 0x184u, word(rt, p4)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
}

// func_089C80DC(a0, a1, mode, p): mode 1 only (anything else returns v0 = 1): holders sp + 4 and sp + 8 are built by func_089C5850(sp + 4) and func_08A02AB8(sp + 8); o = *p (null: func_08A5BB7C(D_08CA6304) first) and k = func_089B92A4(); when func_08A76F20(o, k) is non-zero, the holder sp + 0xC is built by func_089B9660(sp + 0xC, p) and its
// member s = sp[0xC][0x70] (null holder: func_08A5BB7C(D_08CA6608)) starts a chain: while s is non-zero, a = the virtual slot 0x178 of s (s), next = s[0x54], b = the virtual slot 0x16C of s (s): a non-zero a goes to func_089B8F30(sp[8] + 0x84, a), otherwise a non-zero b to func_089B8F30(sp[8] + 0xE8, b) (sp[8] null: func_08A5BB7C(D_08CA65FC)); s then becomes next (the loop ends when next is zero). When func_08A76F20 gave zero: func_08A01338(sp) fills a holder copied to sp + 8 with func_08A5BAD8 and destroyed. Then func_089C7F40(sp[4] (null: func_08A5BB7C(D_08CA63A0)), sp + 8) and the two holders are destroyed with func_08A5B9EC.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (mode != 1u) [the early return]`; `if (s1 == 0u) [the call it guards]`; `if (found != 0u)`; `if (h == 0u) [the call it guards]`; `if (s0 != 0u)`; `for (;;)`; `if (a != 0u)`; `if (holder == 0u) [the call it guards]`; `the else branch of the test above`; `if (b != 0u)`; `if (next == 0u) break`; `if (s0 == 0u) [the call it guards]`
void func_089C80DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t pointer = ctx.gpr[7], mode = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[2] = 1u;
    ctx.gpr[16] = pointer;
    if (mode != 1u) return;
    ctx.gpr[21] = p4;
    ctx.gpr[19] = p8;
    (void)call_set(rt, ctx, 0x089C5850u, {p4});
    (void)call_set(rt, ctx, 0x08A02AB8u, {p8});
    std::uint32_t s1 = word(rt, pointer);
    ctx.gpr[17] = s1;
    if (s1 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
    const std::uint32_t key = call_set(rt, ctx, 0x089B92A4u, {});
    ctx.gpr[5] = key;
    const std::uint32_t found = call_set(rt, ctx, 0x08A76F20u, {s1, key});
    ctx.gpr[5] = pointer;  // the delay slot of the beqz
    std::uint32_t s0;
    if (found != 0u) {
        ctx.gpr[22] = pC;
        (void)call_set(rt, ctx, 0x089B9660u, {pC, pointer});
        const std::uint32_t h = word(rt, pC);
        ctx.gpr[16] = h;
        if (h == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6608u});
        s0 = word(rt, h + 0x70u);
        ctx.gpr[16] = s0;
        if (s0 != 0u) {
            for (;;) {
                const std::uint32_t next = word(rt, s0 + 0x54u);  // the delay slot of the first jalr
                ctx.gpr[3] = word(rt, s0);
                ctx.gpr[18] = next;
                const std::uint32_t a = call_set(rt, ctx, slot_of(rt, s0, 0x178u), {s0});
                ctx.gpr[17] = a;
                ctx.gpr[3] = word(rt, s0);
                const std::uint32_t b = call_set(rt, ctx, slot_of(rt, s0, 0x16Cu), {s0});
                ctx.gpr[16] = b;  // the delay slot of the beqz
                if (a != 0u) {
                    const std::uint32_t holder = word(rt, p8);
                    ctx.gpr[16] = holder;
                    if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA65FCu});
                    (void)call_set(rt, ctx, 0x089B8F30u, {holder + 0x84u, a});
                    ctx.gpr[16] = next;  // the delay slot of the j
                } else {
                    ctx.gpr[17] = word(rt, p8);  // the delay slot of the beqz
                    if (b != 0u) {
                        const std::uint32_t holder = word(rt, p8);
                        ctx.gpr[17] = holder;
                        if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA65FCu});
                        (void)call_set(rt, ctx, 0x089B8F30u, {holder + 0xE8u, b});
                    }
                    ctx.gpr[16] = next;
                }
                s0 = next;
                if (next == 0u) break;
            }
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
        s0 = word(rt, p4);
        ctx.gpr[16] = s0;
    } else {
        (void)call_set(rt, ctx, 0x08A01338u, {p0});
        (void)call_set(rt, ctx, 0x08A5BAD8u, {p8, word(rt, p0)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
        s0 = word(rt, p4);
        ctx.gpr[16] = s0;
    }
    ctx.gpr[4] = s0;  // the delay slot of the bnel (taken)
    if (s0 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA63A0u});
    (void)call_set(rt, ctx, 0x089C7F40u, {s0, p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08B5985C(this): four slots k = 0..3 at this + 0x190 + 0x28 * k are packed into the words at p = this[0x5C]: a slot whose bytes [0x32] and [0x33] are both set writes three words (a3 / t0 / t1 = the 24-bit values slot[0x18] / [0x1C] / [0x20] shifted right by 8, each in the low 24 bits of a constant high byte: 0x63 + 3k, 0x64 + 3k, 0x65 + 3k),
// clears the byte [0x33] and, when the byte [0x30] is set, three more words (high bytes 0x6F + 3k, 0x70 + 3k, 0x71 + 3k with the values [0x24], [0x28], [0x2C] shifted right by 8, the mode 2, or 1 when the byte [0x31] is zero); then one flag word (high byte 0x5F + k with the mode in bits 8-9 and 1 in bits 0-1). p advances past what was written and is stored back at this[0x5C]. v0 = 4.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (third != 0u)`; `if (mode == 0u) s3 = 1u`
void func_08B5985C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    std::uint32_t slot = self + 0x190u;  // a1
    std::uint32_t cursor = word(rt, self + 0x5Cu);  // t2
    ctx.gpr[8 + 4] = 0u;
    ctx.gpr[20] = self;
    ctx.gpr[6] = 0x03000000u;
    ctx.gpr[30] = 0x01000000u;
    ctx.gpr[2] = 0u;
    for (std::uint32_t k = 0u; k < 4u; ++k) {
        const std::uint32_t t6 = 0x63000000u + 0x03000000u * k, t7 = 0x64000000u + 0x03000000u * k, t8 = 0x65000000u + 0x03000000u * k;
        const std::uint32_t t9 = 0x5F000000u + 0x01000000u * k, s0 = 0x70000000u + 0x03000000u * k, s1 = 0x71000000u + 0x03000000u * k, s2 = 0x6Fu + 3u * k;
        ctx.gpr[12] = k + 1u;
        ctx.gpr[14] = t6;
        ctx.gpr[15] = t7;
        ctx.gpr[24] = t8;
        ctx.gpr[25] = t9;
        ctx.gpr[16] = s0;
        ctx.gpr[17] = s1;
        ctx.gpr[18] = s2;
        ctx.gpr[5] = slot;
        ctx.gpr[10] = cursor;
        const std::uint32_t first = byte_u(rt, slot + 0x32u);
        ctx.gpr[2] = first;
        if (first != 0u) {
            const std::uint32_t second = byte_u(rt, slot + 0x33u);
            ctx.gpr[2] = second;
            std::uint32_t a3 = t6, t0 = t7, t1 = t8, t5 = t9, t3 = cursor + 0xCu, s3 = 0u;
            ctx.gpr[19] = 0u;  // the delay slot of the beqz
            if (second != 0u) {
                const std::uint32_t v1 = word(rt, slot + 0x1Cu) >> 8, a0 = word(rt, slot + 0x20u) >> 8, v0 = word(rt, slot + 0x18u) >> 8;
                ctx.gpr[3] = v1;
                ctx.gpr[4] = a0;
                ctx.gpr[2] = v0;
                a3 = ins_bits(a3, v0, 0u, 24u);
                t0 = ins_bits(t0, v1, 0u, 24u);
                t1 = ins_bits(t1, a0, 0u, 24u);
                put8(rt, slot + 0x33u, 0u);
                std::uint32_t s5 = s2 << 24, s6 = s0, s7 = s1;
                put(rt, cursor, a3);
                put(rt, cursor + 4u, t0);
                put(rt, cursor + 8u, t1);
                const std::uint32_t third = byte_u(rt, slot + 0x30u);
                ctx.gpr[2] = third;
                if (third != 0u) {
                    const std::uint32_t w1 = word(rt, slot + 0x28u) >> 8, w2 = word(rt, slot + 0x2Cu) >> 8, w0 = word(rt, slot + 0x24u) >> 8;
                    ctx.gpr[3] = w1;
                    ctx.gpr[4] = w2;
                    ctx.gpr[2] = w0;
                    s5 = ins_bits(s5, w0, 0u, 24u);
                    s6 = ins_bits(s6, w1, 0u, 24u);
                    s7 = ins_bits(s7, w2, 0u, 24u);
                    put(rt, cursor + 0xCu, s5);
                    s3 = 2u;
                    ctx.gpr[2] = 1u;
                    put(rt, cursor + 0x10u, s6);
                    t3 = cursor + 0x18u;
                    put(rt, cursor + 0x14u, s7);
                    const std::uint32_t mode = byte_u(rt, slot + 0x31u);
                    ctx.gpr[3] = mode;
                    if (mode == 0u) s3 = 1u;
                }
                ctx.gpr[19] = s3;
                t5 = ins_bits(t5, s3, 8u, 2u);
                ctx.gpr[3] = 1u;
                const std::uint32_t flags = ins_bits(t5, 1u, 0u, 2u);
                ctx.gpr[2] = flags;
                cursor = t3 + 4u;
                put(rt, t3, flags);
            }
        }
        ctx.gpr[2] = 4u;
        slot += 0x28u;
    }
    put(rt, self + 0x5Cu, cursor);
}

// func_08C3029C(out, a1, lo, hi): the 64-bit value n = out[4]:out[0] is divided by d = hi:lo in three steps (the quotient is small): up to two subtractions of 4 * d (adding 4 each), one of 2 * d (adding 2) and one of d (adding 1), each when n is not below the step; n becomes the remainder (low word out[0], high word out[4]) and v0 is the quotient.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `std::uint32_t v1 = (static_cast<std::uint32_t>(n >> 32) < static_cast<std::uint32_t>(d4 >> 32)) ? 1u : 0u`
void func_08C3029C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], lo = ctx.gpr[6], hi = ctx.gpr[7];
    const std::uint64_t d = (static_cast<std::uint64_t>(hi) << 32) | lo;
    const std::uint64_t d4 = d << 2, d2 = d << 1;
    std::uint64_t n = (static_cast<std::uint64_t>(word(rt, out + 4u)) << 32) | word(rt, out);
    std::uint32_t quotient = 0u;
    std::uint32_t v1 = (static_cast<std::uint32_t>(n >> 32) < static_cast<std::uint32_t>(d4 >> 32)) ? 1u : 0u;
    if (n >= d4) {
        n -= d4;
        quotient = 4u;
        if (n >= d4) {
            n -= d4;
            v1 = static_cast<std::uint32_t>(n >> 32);
            quotient = 8u;
        }
    }
    if (n >= d2) {
        n -= d2;
        v1 = static_cast<std::uint32_t>(n >> 32);
        quotient += 2u;
    }
    if (n >= d) {
        n -= d;
        v1 = static_cast<std::uint32_t>(n >> 32);
        quotient += 1u;
    }
    ctx.gpr[2] = quotient;
    ctx.gpr[3] = v1;
    ctx.gpr[5] = quotient;
    put(rt, out, static_cast<std::uint32_t>(n));
    put(rt, out + 4u, static_cast<std::uint32_t>(n >> 32));
}

// func_0895B454(): a leaf that copies the bytes of the record D_08CDE508 + 0x1C5 .. 0x1D7 into the record D_08CF35A0: plain bytes (+0, +4, +0x14, +0x24, +0x28, +0x2C, +0x30, +0x3C, +0x40), percentages (bytes 0x1C7, 0x1C8, 0x1C9, 0x1CB, 0x1CC, 0x1CD divided by 100.0f, stored at +8, +0xC, +0x10, +0x18, +0x1C, +0x20) and
// scaled bytes (0x1D2, 0x1D3, 0x1D7, 0x1D6 times D_3E8E38E4, stored at +0x34, +0x38, +0x48, +0x44). The registers are reproduced one by one (mtc1 moves the raw integer, cvt.s.w converts it).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): none: every branch and region of the compared cases ran.
void func_0895B454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t source = 0x08CDE508u, target = 0x08CF35A0u;
    ctx.gpr[3] = source;
    const std::uint32_t v0 = byte_u(rt, source + 0x1C7u), a0 = byte_u(rt, source + 0x1C8u), a1 = byte_u(rt, source + 0x1C5u);
    mtc1(ctx, 0, v0);
    mtc1(ctx, 1, a0);
    const std::uint32_t v0b = byte_u(rt, source + 0x1C9u), a0b = byte_u(rt, source + 0x1CBu);
    cvt_s_w(ctx, 6, 0);
    mtc1(ctx, 2, v0b);
    mtc1(ctx, 3, a0b);
    const std::uint32_t v0c = byte_u(rt, source + 0x1CCu), a0c = byte_u(rt, source + 0x1CDu);
    cvt_s_w(ctx, 7, 1);
    mtc1(ctx, 4, v0c);
    mtc1(ctx, 5, a0c);
    cvt_s_w(ctx, 8, 2);
    cvt_s_w(ctx, 9, 3);
    cvt_s_w(ctx, 10, 4);
    cvt_s_w(ctx, 11, 5);
    mtc1(ctx, 0, 0x42C80000u);
    ctx.fpr[6] = ctx.fpr[6] / ctx.fpr[0];
    ctx.fpr[7] = ctx.fpr[7] / ctx.fpr[0];
    ctx.fpr[11] = ctx.fpr[11] / ctx.fpr[0];
    ctx.fpr[8] = ctx.fpr[8] / ctx.fpr[0];
    ctx.fpr[9] = ctx.fpr[9] / ctx.fpr[0];
    ctx.fpr[10] = ctx.fpr[10] / ctx.fpr[0];
    const std::uint32_t v0d = byte_u(rt, source + 0x1D2u), a0d = byte_u(rt, source + 0x1D3u);
    mtc1(ctx, 4, 0x3E8E38E4u);
    mtc1(ctx, 1, v0d);
    mtc1(ctx, 0, a0d);
    const std::uint32_t a2 = byte_u(rt, source + 0x1C6u);
    cvt_s_w(ctx, 2, 1);
    cvt_s_w(ctx, 3, 0);
    const std::uint32_t a3 = byte_u(rt, source + 0x1CAu), t0 = byte_u(rt, source + 0x1CEu);
    ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[4]);
    ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[4]);
    const std::uint32_t t1 = byte_u(rt, source + 0x1CFu), t2 = byte_u(rt, source + 0x1D0u), t3 = byte_u(rt, source + 0x1D1u);
    const std::uint32_t t4 = byte_u(rt, source + 0x1D4u), t5 = byte_u(rt, source + 0x1D5u);
    ctx.gpr[4] = 0x00010000u;
    ctx.gpr[2] = target;
    put(rt, target, a1);
    put_float(rt, target + 0x34u, ctx.fpr[2]);
    put_float(rt, target + 0x38u, ctx.fpr[3]);
    put(rt, target + 4u, a2);
    put_float(rt, target + 8u, ctx.fpr[6]);
    put_float(rt, target + 0xCu, ctx.fpr[7]);
    put_float(rt, target + 0x10u, ctx.fpr[8]);
    put(rt, target + 0x14u, a3);
    put_float(rt, target + 0x18u, ctx.fpr[9]);
    put_float(rt, target + 0x1Cu, ctx.fpr[10]);
    put_float(rt, target + 0x20u, ctx.fpr[11]);
    put(rt, target + 0x24u, t0);
    put(rt, target + 0x28u, t1);
    put(rt, target + 0x2Cu, t2);
    put(rt, target + 0x30u, t3);
    put(rt, target + 0x3Cu, t4);
    put(rt, target + 0x40u, t5);
    const std::uint32_t last_a = byte_u(rt, source + 0x1D7u), last_b = byte_u(rt, source + 0x1D6u);
    ctx.gpr[4] = last_a;
    ctx.gpr[3] = last_b;
    ctx.gpr[8] = t0;
    ctx.gpr[9] = t1;
    ctx.gpr[10] = t2;
    ctx.gpr[11] = t3;
    ctx.gpr[12] = t4;
    ctx.gpr[13] = t5;
    ctx.gpr[5] = a1;
    ctx.gpr[6] = a2;
    ctx.gpr[7] = a3;
    mtc1(ctx, 2, last_a);
    mtc1(ctx, 3, last_b);
    cvt_s_w(ctx, 0, 2);
    cvt_s_w(ctx, 1, 3);
    ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[4]);
    ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[4]);
    put_float(rt, target + 0x48u, ctx.fpr[0]);
    put_float(rt, target + 0x44u, ctx.fpr[1]);
}

// func_08AF2ECC(out, a, b): o = func_08B62344(0xA8) gets a record: o[0] = a, o[4] = b, 0.5f at o[0x18], 2000.0f at o[0x60], 1.0f at o[0x50], o[0x58], o[0x5C], the bytes o[0x54], o[0x55], o[0x4C] = 1 and zero in o[8..0x14], o[0x1C..0x24], o[0x28..0x30], o[0x34..0x40], o[0x44], o[0x48], o[0xA4]; out[0] = o.
// The guarded static D_08D77280: when its first byte is zero and func_08B6441C(D_08D77280) is non-zero, D_08D77288 = D_08CD7138 and func_08B64494(D_08D77280) run. out[4] = D_08D77288.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (acquired != 0u)`
void func_08AF2ECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], first = ctx.gpr[5], second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t guard = 0x08D77280u, instance = 0x08D77288u;
    ctx.gpr[18] = out;
    ctx.gpr[17] = second;
    ctx.gpr[16] = first;  // the delay slot of the jal
    const std::uint32_t o = call_set(rt, ctx, 0x08B62344u, {0xA8u});
    ctx.gpr[4] = o + 0x40u;
    ctx.gpr[5] = 1u;
    mtc1(ctx, 2, 0x3F800000u);
    mtc1(ctx, 0, 0x3F000000u);
    mtc1(ctx, 1, 0x44FA0000u);
    put(rt, o + 0x40u + 8u, 0u);
    put(rt, o + 0x40u + 4u, 0u);
    ctx.gpr[19] = guard;
    ctx.gpr[20] = instance;
    put(rt, o, first);
    put(rt, o + 4u, second);
    put_float(rt, o + 0x18u, ctx.fpr[0]);
    put_float(rt, o + 0x60u, ctx.fpr[1]);
    put8(rt, o + 0x54u, 1u);
    put_float(rt, o + 0x50u, ctx.fpr[2]);
    put(rt, o + 0x40u, 0u);
    for (const std::uint32_t offset : {8u, 0xCu, 0x10u, 0x14u, 0x1Cu, 0x20u, 0x24u, 0x34u}) put(rt, o + offset, 0u);
    put8(rt, o + 0x55u, 1u);
    put_float(rt, o + 0x58u, ctx.fpr[2]);
    put_float(rt, o + 0x5Cu, ctx.fpr[2]);
    put(rt, o + 0x38u, 0u);
    put(rt, o + 0x3Cu, 0u);
    put8(rt, o + 0x4Cu, 1u);
    put(rt, o + 0xA4u, 0u);
    put(rt, o + 0x28u, 0u);
    put(rt, o + 0x2Cu, 0u);
    put(rt, o + 0x30u, 0u);
    put(rt, out, o);
    ctx.gpr[2] = sext8(byte_u(rt, guard));
    ctx.gpr[4] = guard;  // the delay slot of the bnez
    if (ctx.gpr[2] == 0u) {
        const std::uint32_t acquired = call_set(rt, ctx, 0x08B6441Cu, {guard});
        ctx.gpr[5] = 0x000A0000u;
        ctx.gpr[4] = guard;  // the delay slot of the beqz
        if (acquired != 0u) {
            ctx.gpr[2] = 0x08CD7138u;
            put(rt, instance, 0x08CD7138u);
            (void)call_set(rt, ctx, 0x08B64494u, {guard});
        }
    }
    put(rt, out + 4u, instance);
}

// func_08B14864(this, rec): s = rec[8] (s[0x7C] non-zero returns -1); the virtual slot 0xA0 of this fills a 64-byte record at sp + 0x50 (this, s), which is copied to sp + 8 and its first 60 bytes to s + 0xA8 (s[0xA8] non-zero afterwards returns -1); with k = the record word at sp + 0x44 and m = s[0xC0]:
// sp[0] = -1 and r = func_08B147E0(this, k, sp); a negative r stores rec[4] = 9 and returns r. Otherwise func_08B1065C(this, rec), then under the lock D_08D477F0 (func_08B344E0 .. func_08B3457C) the cell c = func_08B2B7D8(D_08D477D4, 0) gets c[0] = r, c[4] = (k << 6, plus this[0x254] when sp[0] is negative), c[0xC] = m, c[8] = 0, c[0x10] = sp[0]; rec[0x10] = c and v0 = r.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (busy != 0u)`; `if (taken != 0u)`; `if (signed_of(r) < 0)`; `if (signed_of(mode) < 0)`
void func_08B14864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], rec = ctx.gpr[5];
    GuestFrame frame(ctx, 0xB0u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p8 = frame.at(8u), p50 = frame.at(0x50u);
    ctx.gpr[19] = self;
    const std::uint32_t s = word(rt, rec + 8u);
    ctx.gpr[17] = s;
    const std::uint32_t busy = word(rt, s + 0x7Cu);
    ctx.gpr[2] = busy;
    ctx.gpr[18] = rec;  // the delay slot of the bnez
    if (busy != 0u) {
        ctx.gpr[17] = 0xFFFFFFFFu;
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    ctx.gpr[16] = p50;
    ctx.gpr[6] = s;
    const std::uint32_t slot = slot_of(rt, self, 0xA0u);
    ctx.gpr[3] = slot;
    ctx.gpr[5] = self;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {p50, self, s});
    for (std::uint32_t offset = 0u; offset < 4u * 16u; offset += 4u) put(rt, p8 + offset, word(rt, p50 + offset));
    for (std::uint32_t offset = 0u; offset < 3u * 16u + 12u; offset += 4u) put(rt, s + 0xA8u + offset, word(rt, p8 + offset));
    const std::uint32_t taken = word(rt, s + 0xA8u);
    ctx.gpr[2] = taken;
    if (taken != 0u) {
        ctx.gpr[17] = 0xFFFFFFFFu;  // the delay slot of the bnel (taken)
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const std::uint32_t k = word(rt, frame.at(0x44u));
    ctx.gpr[21] = k;
    ctx.gpr[2] = 0xFFFFFFFFu;
    const std::uint32_t m = word(rt, s + 0xC0u);
    ctx.gpr[22] = m;
    put(rt, p0, 0xFFFFFFFFu);  // the delay slot of the jal
    const std::uint32_t r = call_set(rt, ctx, 0x08B147E0u, {self, k, p0});
    ctx.gpr[17] = r;  // the delay slot of the bgez
    if (signed_of(r) < 0) {
        ctx.gpr[2] = 9u;
        put(rt, rec + 4u, 9u);
        ctx.gpr[2] = r;
        return;
    }
    (void)call_set(rt, ctx, 0x08B1065Cu, {self, rec});
    (void)call_set(rt, ctx, 0x08B344E0u, {0x08D477F0u});
    const std::uint32_t cell = call_set(rt, ctx, 0x08B2B7D8u, {0x08D477D4u, 0u});
    ctx.gpr[20] = cell;
    (void)call_set(rt, ctx, 0x08B3457Cu, {0x08D477F0u});
    put(rt, cell, r);
    const std::uint32_t mode = word(rt, p0);
    ctx.gpr[2] = mode;
    std::uint32_t a0 = k << 6;  // the delay slot of the bgez
    if (signed_of(mode) < 0) {
        const std::uint32_t table = word(rt, self + 0x254u);
        ctx.gpr[3] = table;
        a0 += table;
    }
    ctx.gpr[4] = a0;
    put(rt, cell + 4u, a0);
    put(rt, cell + 0xCu, m);
    put(rt, cell + 8u, 0u);
    const std::uint32_t again = word(rt, p0);
    ctx.gpr[2] = again;
    put(rt, cell + 0x10u, again);
    put(rt, rec + 0x10u, cell);  // the delay slot of the j
    ctx.gpr[2] = r;
}

// func_08C1E634(this): with the byte this[0xC] set, the holder sp + 0xC is built by func_08A98C1C(sp + 0xC, 2); the element e = sp[0xC][0x14] gets the holder func_08A85B1C(sp + 8, this[0x10]) (copied with func_08A5BAD8(e, sp[8]) unless it is sp + 8, destroyed with func_08A5B9EC); o = this[8] (null: func_08A5BB7C(D_08CAFB14)) answers
// through its virtual slot 0x84 (sp + 4, o, this[0x10]) a holder that is copied to sp[0xC][0x14] + 4 and destroyed; the holder sp + 0xC is copied to this + 4 and destroyed. With the byte clear only the virtual slot 0x84 (sp, o, this[0x10]) runs and its holder is copied to this + 4 and destroyed. this[0x10] is incremented and returned.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (flag != 0u)`; `if (element != p8) [the call it guards]`; `if (o == 0u) [the call it guards]`; `if (destination != p4) [the call it guards]`; `if (self + 4u != pC) [the call it guards]`; `if (self + 4u != p0)`
void func_08C1E634(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    const std::uint32_t flag = byte_u(rt, self + 0xCu);
    ctx.gpr[2] = flag;
    ctx.gpr[17] = self;  // the delay slot of the beqz
    if (flag != 0u) {
        ctx.gpr[20] = pC;
        (void)call_set(rt, ctx, 0x08A98C1Cu, {pC, 2u});
        const std::uint32_t list = word(rt, pC);
        ctx.gpr[2] = list;
        ctx.gpr[5] = word(rt, self + 0x10u);
        ctx.gpr[16] = p8;
        const std::uint32_t element = word(rt, list + 0x14u);
        ctx.gpr[18] = element;
        (void)call_set(rt, ctx, 0x08A85B1Cu, {p8, word(rt, self + 0x10u)});
        ctx.gpr[5] = word(rt, p8);  // the delay slot of the beq
        if (element != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {element, word(rt, p8)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
        const std::uint32_t list2 = word(rt, pC);
        ctx.gpr[2] = list2;
        std::uint32_t o = word(rt, self + 8u);
        ctx.gpr[16] = o;
        const std::uint32_t destination = word(rt, list2 + 0x14u) + 4u;
        ctx.gpr[3] = destination - 4u;
        ctx.gpr[19] = destination;  // the delay slot of the bnez
        if (o == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAFB14u});
        ctx.gpr[18] = p4;
        const std::uint32_t slot = word(rt, word(rt, o) + 0x84u);
        ctx.gpr[3] = slot;
        ctx.gpr[5] = o;
        ctx.gpr[4] = p4;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot, {p4, o, word(rt, self + 0x10u)});
        ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
        if (destination != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, p4)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
        ctx.gpr[4] = self + 4u;
        if (self + 4u != pC) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + 4u, word(rt, pC)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
        ctx.gpr[2] = word(rt, self + 0x10u);  // the delay slot of the j
    } else {
        const std::uint32_t o = word(rt, self + 8u);
        ctx.gpr[16] = o;
        if (o == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAFB14u});
        ctx.gpr[3] = word(rt, o);
        ctx.gpr[6] = word(rt, self + 0x10u);
        ctx.gpr[4] = p0;
        ctx.gpr[5] = o;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot_of(rt, o, 0x84u), {p0, o, word(rt, self + 0x10u)});
        ctx.gpr[4] = self + 4u;
        if (self + 4u != p0) {
            ctx.gpr[5] = word(rt, p0);
            (void)call_set(rt, ctx, 0x08A5BAD8u, {self + 4u, word(rt, p0)});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
        ctx.gpr[2] = word(rt, self + 0x10u);
    }
    ctx.gpr[2] += 1u;
    put(rt, self + 0x10u, ctx.gpr[2]);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

}  // namespace
}  // namespace hot_batch_92
}  // namespace gt::game

#define HOT92(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_92::name, gt::ReturnKind::kind)
#define HOT92N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_92::name, gt::ReturnKind::kind)
HOT92(0x08861FD0u, func_08861FD0, Word);
HOT92(0x08865B98u, func_08865B98, Word);
HOT92(0x08898AB4u, func_08898AB4, Word);
// Not registered (its 1 compared case, equal, changed no memory): HOT92(0x088890ACu, func_088890AC, Word);
HOT92(0x088A2074u, func_088A2074, Word);
HOT92(0x088D31B0u, func_088D31B0, Word);
HOT92(0x08898918u, func_08898918, Word);
// Not registered (17 compared cases in the fast gate and 3,000 dense ones on the race route (GT_DECOMP_DENSE, --decomp-cases 3000), all equal, none changed memory): HOT92(0x088F05BCu, func_088F05BC, Word);
HOT92(0x088FC45Cu, func_088FC45C, Word);
HOT92(0x089FAFD4u, func_089FAFD4, Word);
HOT92(0x08ADE8C4u, func_08ADE8C4, Word);
// Not registered (99 compared cases in the fast gate and 3,000 dense ones on the race route (GT_DECOMP_DENSE, --decomp-cases 3000), all equal, none changed memory): HOT92(0x088E6250u, func_088E6250, Word);
HOT92(0x08A0F344u, func_08A0F344, Word);
HOT92(0x088721DCu, func_088721DC, Word);
// Not registered (every case stops on the import stub 0x08C7B168 (sceUtility::0x2A2B3DE0) (not comparable), compared 0): HOT92(0x08ACECFCu, func_08ACECFC, Word);
HOT92(0x08B2E434u, func_08B2E434, Word);
HOT92(0x088A5160u, func_088A5160, Word);
HOT92(0x089BBF8Cu, func_089BBF8C, Word);
HOT92N(0x089064D8u, func_089064D8, Word);
HOT92(0x08C34358u, func_08C34358, Word);
HOT92(0x08A1F174u, func_08A1F174, Word);
// Not registered (its only case stops on 'kernel isolation (sleep would block)' (not comparable), compared 0): HOT92(0x0882F7A0u, func_0882F7A0, Word);
HOT92(0x0883C7ECu, func_0883C7EC, Word);
HOT92(0x0883CB34u, func_0883CB34, Word);
HOT92(0x0883D494u, func_0883D494, Word);
// Not registered (15 compared cases in the fast gate and 3,000 dense ones on the race route, all equal, none changed memory (func_08BB24B0, the same function again, did)): HOT92(0x08BB2340u, func_08BB2340, Word);
HOT92(0x08BB24B0u, func_08BB24B0, Word);
HOT92(0x08BB2660u, func_08BB2660, Word);
HOT92(0x08BB13D4u, func_08BB13D4, Word);
HOT92(0x08C3EBD8u, func_08C3EBD8, Word);
// Not registered (its 4 compared cases, equal, changed no memory): HOT92(0x0888AD94u, func_0888AD94, Word);
HOT92(0x08ACCFF8u, func_08ACCFF8, Word);
HOT92(0x088A26E0u, func_088A26E0, Word);
HOT92(0x088738FCu, func_088738FC, Word);
HOT92(0x088D7794u, func_088D7794, Word);
HOT92(0x08877FB0u, func_08877FB0, Word);
// Not registered (its only case stops on 'kernel isolation (LwMutex contention)' (not comparable), compared 0): HOT92(0x0897AAB4u, func_0897AAB4, Word);
HOT92(0x089EB064u, func_089EB064, Word);
HOT92(0x08809E90u, func_08809E90, Word);
// Not registered (calls 0 on all five routes of the fast gate although the batch-89 probe counted 1 hit: the original is only called from code the dispatcher does not see, so nothing is compared): HOT92(0x088FEB9Cu, func_088FEB9C, Word);
// Not registered (its 2 compared cases, equal, changed no memory): HOT92(0x088F9F00u, func_088F9F00, Word);
HOT92(0x088F03E4u, func_088F03E4, Word);
// Not registered (its 1 compared case, equal, changed no memory): HOT92(0x088F024Cu, func_088F024C, Word);
HOT92(0x08905D18u, func_08905D18, Word);
HOT92(0x0890B62Cu, func_0890B62C, Word);
HOT92(0x08918170u, func_08918170, Word);
HOT92(0x089E4F44u, func_089E4F44, Word);
HOT92(0x08BD4118u, func_08BD4118, Word);
HOT92(0x08A12658u, func_08A12658, Word);
HOT92(0x089CE3CCu, func_089CE3CC, Word);
HOT92(0x089E5BF4u, func_089E5BF4, Word);
HOT92(0x08B0E358u, func_08B0E358, Word);
// Not registered (3 compared cases, all equal, none changed memory (its writes live in callees that did not write in those cases)): HOT92(0x0898BDB4u, func_0898BDB4, Word);
HOT92(0x08990720u, func_08990720, Word);
HOT92(0x08998B70u, func_08998B70, Word);
HOT92(0x089C80DCu, func_089C80DC, Word);
HOT92(0x08B5985Cu, func_08B5985C, Word);
HOT92(0x08C3029Cu, func_08C3029C, Word);
HOT92N(0x0895B454u, func_0895B454, Word);
HOT92(0x08AF2ECCu, func_08AF2ECC, Word);
HOT92(0x08B14864u, func_08B14864, Word);
HOT92(0x08C1E634u, func_08C1E634, Word);
