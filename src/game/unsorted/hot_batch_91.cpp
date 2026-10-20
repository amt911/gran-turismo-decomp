// Hot batch 91: reached functions that no earlier batch has written, from the batch-89 probe (`tools.decomp_gate probe b89p`: the five routes with --decomp off over the 1,403 not-yet-done functions of 71-120 instructions). (main includes batch 90, 6,229 functions.) Before reading any function its address
// was searched for in src/ (main and the open PR branches: `gh pr list` shows only the draft spec PR #214, no code; `git branch -r --no-merged origin/main` shows three branches without a function of this list).
//
// After batches 89 and 90 177 of the probe's reached functions were still absent from src/. 28 of them were read and left by those batches (their head comments list them), 7 have VFPU, 17 call the import stubs from 0x08C70000 or jump through a register, and about 25 are float-heavy (more than 12 FPU
// instructions, left for a float batch). `tools.family_members --list --any-template` over the probe logs finds no cluster of two or more pending members that was not already read (the two it lists, func_0883C988 / func_0883CCC8 and func_08BC0DFC / func_08BD0B30, were deferred or unregistered by batches 89-90), so this batch has no
// new masked-shape cluster; the two pairs it does share a body for were found by reading:
//  - `vector_assign<Shift>` (func_08B8AD88 with 2-byte elements, func_08BFBB38 with 4-byte ones): `this = other` for a vector {begin, end, capacity}; the shift is an instruction field (sra / sll / ins), the three callees are the row;
//  - `vector_insert_grow` (the reallocating half of func_08C1ABE0 and func_08C25730, two insertions into a vector of 4-byte elements whose room-left halves differ): its four values (the argument of func_08B5ADDC, the element construct, the range copy, the range destroy) are the row of each member. Both were
//    checked by hand against the words of their members: `tools.family_members --check` reads no row of this file (a template argument is not matched, and the grow rows are arguments of an inner call, not one-line wrappers), so 0 rows were checked that way.
// Every hexadecimal literal of every function was also compared with the function's own disassembly by the new `tools.literal_check` (52 functions, 0 literals missing; its one `--allow` is 0x08AD6AB8:0x3C, the displacement 8 + 0x34 of a store through s3 = s1 + 0x34): a literal typed wrongly on a path no route runs is
// exactly what the gate cannot see (batch 90's 0x11111111 for 0x01111111). The lui / ori values 0x1FFFFFFF, 0xFFF7EEC5, 0x80008001, 0x3FFFFFFF and 0x3E8E38E4 and the bare lui 0x3F800000 were also read digit by digit against their instructions.
//
// Registered (47): func_0882222C, func_0885C5C0, func_0889D738, func_089CB164, func_08A9CB38, func_08AD6AB8, func_08B166F8, func_08B6F37C (a 64-bit pair, `ReturnKind::DoubleWord`), func_08B20A78, func_089BDD18, func_08C20540, func_08A54830, func_08C34FE8, func_08A46DCC, func_08A9FFD0, func_08991490, func_08A206E8, func_0896DFCC, func_08A42E10,
// func_08A32678, func_08A4BBC0, func_08A4412C, func_08A3C544, func_08A4A960, func_08A4BA50, func_08A10468, func_089A44F0, func_08B9A5C4, func_0894D484, func_08988BB8, func_08A6CFC0, func_08C22B54, func_08C1ABE0, func_08C25730, func_08AA6B80, func_08B8AD88, func_08BFBB38, func_08C287AC, func_0880602C, func_08A6F704, func_08A954B8,
// func_089B6408, func_08826C50, func_0899CE20, func_0881D004, func_08898C10, func_0880C9B8. The role words in the comments (holder, string, vector, node, tree, cell, record, list, object, virtual slot, key) describe the shape of the accesses; none is a name from the game. Fourteen of the functions are registered under Adhoc script names in
// config/symbol_addrs.txt (from the registrations of the script classes); the comments keep the func_ names and say only what the asm does.
//
// Written but not registered (the commented HOT91 lines say why): func_089B439C (every case stops on `kernel isolation (LwMutex contention)`), func_089CA118 (the import stub sceIoRead), func_08A235F8 (the import stub sceSasCore::0x33D4AB37), func_08A243FC (`kernel isolation (sleep would block)`) - none compares - and func_08B666E8
// (`calls 0` on all five routes: its probe hits come from calls the dispatcher does not see). Read and left for a later batch: func_08ABEAEC, func_08ACCFF8, func_0884085C, func_088D7794, func_08877B68 and the float-heavy ones.
//
// Compared on their main path but with side paths read from the asm only: each function's comment lists, from `tools.decomp_gate coverage`, what the compared cases did not take and what was checked against the asm by hand. The weakest proofs are in the comments of func_08A9FFD0 (only the early return ran), func_08A4A960 (the loop never ran),
// func_08B9A5C4, func_08C1ABE0 and func_08C25730 (the room-left halves never ran), func_08A10468 and func_08A6F704 (one of two modes ran) and func_08B20A78 (three of four paths). Every registered function was compared at least once.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value. Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's), and the callee-saved
// registers (s0-s7, fp) hold the original's values around the calls (`SavedRegs` restores the entry values on the way out, like the original's epilogue; `SavedFloat` does the same for $f20): a callee may store the caller's s0 (FINDINGS, batch 84). The delay slot of a `jal` / `jalr` runs BEFORE the call (func_0899CE20's first run faulted
// because its `addu s1, v0, zero` in the delay slot of `jal func_08A5BA08` was read as the call's result), and a `bnel` / `beql` / `bgtzl` delay slot only when the branch is taken. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path, including the early returns; the other caller-saved registers are set to the
// original's value where it is cheap and a callee could read them, not otherwise (the gate compares v0, s0-s7, sp, ra and RAM). The dead stack (spills the original makes and nothing reads) is not reproduced, the harness ignores it; the frames that callees read (holders, records passed by address) are.
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_91 {
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


// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_0882222C(this): with p8 = sp + 8, p14 = sp + 0x14, p18 = sp + 0x18, p10 = sp + 0x10 (holders, all passed by address): func_08A69B3C(p8, D_08C99708); o = func_08A5BC20(0x5C, 4); func_08A54F3C(o, p8, 0x10); the word o is stored at sp + 0xC and
// func_08A52AB0(p18, sp + 0xC) runs; func_08A69D68(p8); l = the word at p18 (null: func_08A5BB7C(D_08C9971C) first). Then func_08A69B3C(p14, D_08C99728); n = func_08A5BC20(0x38, 4); func_08A69B3C(sp, D_08C99730); func_08A52660(n, sp); func_08A69D68(sp);
// n[0] = D_08CB8110; the word n is stored at sp + 4 and func_08A52AB0(p10, sp + 4) runs; func_08A535A4(l, p14, p10, 4, 0); func_08A5B9EC(p10); func_08A69D68(p14); h = this[0] (null: func_08A5BB7C(D_08C99738) first); func_08A7CE50(h, p18);
// the word at p18 is stored at D_08CE2B1C (in the delay slot of the last call, so before it) and func_08A5B9EC(p18) ends the function: v0 is its result, v1 the stored word was in it until then.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the two null checks never fired: the list sp[0x18] (0x088222B0, null-case call D_08C9971C) and the holder this[0] (0x08822350, null-case call D_08C99738); each null-case call call is followed by the same code as the non-null case (targets 0x088222C0 and 0x08822360).
void func_0882222C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p14 = frame.at(0x14u), p18 = frame.at(0x18u);
    ctx.gpr[16] = p8;
    ctx.gpr[21] = self;
    ctx.gpr[20] = p14;
    ctx.gpr[19] = p18;
    ctx.gpr[18] = p10;
    (void)call_set(rt, ctx, 0x08A69B3Cu, {p8, 0x08C99708u});
    const std::uint32_t first = call_set(rt, ctx, 0x08A5BC20u, {0x5Cu, 4u});
    ctx.gpr[17] = first;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A54F3Cu, {first, p8, 0x10u});
    put(rt, pC, first);  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08A52AB0u, {p18, pC});
    (void)call_set(rt, ctx, 0x08A69D68u, {p8});
    const std::uint32_t list = word(rt, p18);
    ctx.gpr[17] = list;
    if (list == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9971Cu});
    (void)call_set(rt, ctx, 0x08A69B3Cu, {p14, 0x08C99728u});
    const std::uint32_t second = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[16] = second;
    (void)call_set(rt, ctx, 0x08A69B3Cu, {p0, 0x08C99730u});
    (void)call_set(rt, ctx, 0x08A52660u, {second, p0});
    (void)call_set(rt, ctx, 0x08A69D68u, {p0});
    ctx.gpr[2] = 0x08CB8110u;
    put(rt, second, 0x08CB8110u);
    put(rt, p4, second);  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08A52AB0u, {p10, p4});
    (void)call_set(rt, ctx, 0x08A535A4u, {list, p14, p10, 4u, 0u});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
    (void)call_set(rt, ctx, 0x08A69D68u, {p14});
    const std::uint32_t holder = word(rt, self);
    ctx.gpr[16] = holder;
    if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99738u});
    (void)call_set(rt, ctx, 0x08A7CE50u, {holder, p18});
    const std::uint32_t stored = word(rt, p18);
    ctx.gpr[3] = stored;
    ctx.gpr[2] = high_half(0x08CE2B1Cu);
    put(rt, 0x08CE2B1Cu, stored);  // the delay slot of the last jal
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p18});
}

// func_0885C5C0(this): this[1] = 0; func_08B28DCC(this + 8); b = func_08B62344(0x138) is stored at this[0] and cleared by func_08B36018(b, 0, 0x138); a = func_08B6461C(0x628) gets the count 4 in a[1] and its four 0x188-byte elements from a + 8 are set up by
// func_0885D720; b[0] = a + 8, b[0x78] = 0, b[0x8C] = func_08B0980C(func_08B0979C()); then four more objects are allocated with func_08B62344 and set up (8 bytes + func_088D65DC -> b[0x7C], 8 + func_08894444 -> b[0x80], 8 + func_08893F7C -> b[0x84],
// 0x88 + func_088D441C -> b[0x88]; b is re-read from this[0] before each); b[0x28] = -1, b[0x2C] = 0, then a tail call into func_0885C3C8(this, b) with v0 = -1 and v1 = 0.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): none: every branch and region of the cases ran (the loop of four elements ran four times).
void func_0885C5C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t block = 0u;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = self;
        ctx.gpr[17] = 4u;
        put(rt, self + 4u, 0u);
        (void)call_set(rt, ctx, 0x08B28DCCu, {self + 8u});
        block = call_set(rt, ctx, 0x08B62344u, {0x138u});
        put(rt, self, block);  // the delay slot of the jal below
        (void)call_set(rt, ctx, 0x08B36018u, {block, 0u, 0x138u});
        ctx.gpr[20] = word(rt, self);  // the delay slot of the next jal
        const std::uint32_t array = call_set(rt, ctx, 0x08B6461Cu, {0x628u});
        ctx.gpr[19] = array + 8u;
        ctx.gpr[3] = 4u;
        put(rt, array + 4u, 4u);
        ctx.gpr[16] = array + 8u;
        for (unsigned element = 0u; element < 4u; ++element) {
            ctx.gpr[17] -= 1u;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x0885D720u, {ctx.gpr[16]});
            ctx.gpr[16] += 0x188u;  // the delay slot of the bnez
        }
        put(rt, ctx.gpr[20], ctx.gpr[19]);
        ctx.gpr[2] = word(rt, self);
        put(rt, ctx.gpr[2] + 0x78u, 0u);
        ctx.gpr[16] = word(rt, self);  // the delay slot of the next jal
        const std::uint32_t root = call_keep(rt, ctx, 0x08B0979Cu, {});
        ctx.gpr[4] = root;  // the delay slot of the next jal
        put(rt, ctx.gpr[16] + 0x8Cu, call_keep(rt, ctx, 0x08B0980Cu, {root}));
        const struct Part {
            std::uint32_t size, init, offset;
        } parts[] = {{8u, 0x088D65DCu, 0x7Cu}, {8u, 0x08894444u, 0x80u}, {8u, 0x08893F7Cu, 0x84u}, {0x88u, 0x088D441Cu, 0x88u}};
        for (const Part &part : parts) {
            ctx.gpr[4] = part.size;
            ctx.gpr[17] = word(rt, self);  // the delay slot of the jal
            const std::uint32_t object = call_keep(rt, ctx, 0x08B62344u, {part.size});
            ctx.gpr[16] = object;
            ctx.gpr[4] = object;  // the delay slot of the next jal
            (void)call_keep(rt, ctx, part.init, {object});
            put(rt, ctx.gpr[17] + part.offset, ctx.gpr[16]);
        }
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[3] = 0u;
        block = word(rt, self);
        ctx.gpr[5] = block;
        ctx.gpr[4] = self;
        put(rt, block + 0x28u, 0xFFFFFFFFu);
        put(rt, block + 0x2Cu, 0u);
    }
    tail_call(ctx, 0x0885C3C8u, {self, block});
}

// func_0889D738(a, b, x, y, target, f12): a record at sp is built for the callee func_0889D6B8(target, sp): sp[0] = a, sp[4] = b, sp[8] = x, sp[0xC] = y, sp[0x10] = f12 (the float), and two flag bytes at sp[0x14] (from the pair (x, y)) and sp[0x15] (from (y, x), only when y is
// non-zero; it stays 0 otherwise). A byte is r1 | 2 (r2 non-zero) | 4 (r3 non-zero) | 8 (r4 non-zero) with r1..r4 the results of func_088A4570, func_088A45B0, func_088A45F0, func_088A36C0 on the pair, r1 sign-extended from its low byte with the other bits
// (the original does r1 | 2 | 4 in registers and takes the low byte at the store). target == 0 or x == 0 returns at once (v0 = a, v1 = b).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): target == 0 (0x0889D770) and x == 0 (0x0889D7A4) never returned early, the movn of the third and fourth result (0x0889D7E4, 0x0889D7F8) never saw a non-zero result (the flags 4 and 8 never set), and y was never 0 (0x0889D808 beqz s2 to 0x0889D868 never taken): the second flag byte was always computed. The flag values 2, 4 and 8 (s5, s6, s7 set at 0x0889D780-0x0889D788) and the sign extension at 0x0889D7EC were compared only with the first two results.
void func_0889D738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], second = ctx.gpr[5], x = ctx.gpr[6], y = ctx.gpr[7], target = ctx.gpr[8];
    const float f12 = ctx.fpr[12];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    ctx.gpr[2] = first;
    ctx.gpr[3] = second;
    ctx.gpr[20] = target;
    ctx.gpr[19] = x;
    ctx.gpr[18] = y;
    if (target == 0u) return;
    ctx.gpr[4] = x;
    ctx.gpr[5] = y;
    ctx.gpr[21] = 2u;
    ctx.gpr[22] = 4u;
    ctx.gpr[23] = 8u;
    put(rt, frame.at(0u), first);
    put(rt, frame.at(4u), second);
    put_float(rt, frame.at(0x10u), f12);
    put(rt, frame.at(8u), x);
    put(rt, frame.at(0xCu), y);
    put8(rt, frame.at(0x14u), 0u);
    put8(rt, frame.at(0x15u), 0u);  // the delay slot of the beqz below
    if (x == 0u) return;
    const auto flags = [&](std::uint32_t p, std::uint32_t q) -> std::uint32_t {
        ctx.gpr[16] = 0u;  // the delay slot of the first jal
        const std::uint32_t r1 = call_set(rt, ctx, 0x088A4570u, {p, q});
        ctx.gpr[17] = r1;  // the delay slot of the next jal
        const std::uint32_t r2 = call_set(rt, ctx, 0x088A45B0u, {p, q});
        ctx.gpr[3] = r2 != 0u ? 2u : 0u;
        ctx.gpr[17] = r1 | ctx.gpr[3];  // the delay slot of the next jal
        const std::uint32_t r3 = call_set(rt, ctx, 0x088A45F0u, {p, q});
        ctx.gpr[16] = r3 != 0u ? 4u : 0u;
        ctx.gpr[17] = sext8(ctx.gpr[17]);  // the delay slot of the next jal
        const std::uint32_t r4 = call_set(rt, ctx, 0x088A36C0u, {p, q});
        ctx.gpr[16] |= ctx.gpr[17];
        ctx.gpr[3] = (r4 != 0u ? 8u : 0u) | ctx.gpr[16];
        return ctx.gpr[3];
    };
    put8(rt, frame.at(0x14u), flags(x, y));
    ctx.gpr[4] = y;
    ctx.gpr[5] = x;
    if (y != 0u) put8(rt, frame.at(0x15u), flags(y, x));
    (void)call_set(rt, ctx, 0x0889D6B8u, {target, frame.at(0u)});
}

// func_089B439C(this, object): a holder at sp + 8 is built by func_08A0387C; the record at sp + 0xC = {D_08CCED90, sp + 0x10 cell = D_08D74FD0 + 0xC, -1, sp + 8} is passed to the virtual slot 8 of object (a1 = sp + 0xC); afterwards the record's vtable word is D_08CCD990 and the cell (sp[0x10] - 0xC,
// unless it is D_08D74FD0) is released: func_08B60804(cell - 4, -1) and, when that is not positive, func_08B8542C(cell - 0xC, sp). Then func_089B396C(this, sp + 8), func_08966040(this, 2); the object h = sp[8] (null: func_08A5BB7C(D_08CA5670) first) gets its virtual slot 0x12C called;
// func_089C9C34(sp + 4); the object g = sp[4] (null: func_08A5BB7C(D_08CA56DC) first) is passed to func_089C8294(g, sp + 8); the holders sp + 4 and sp + 8 are destroyed with func_08A5B9EC (v0 is the second one's).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): not compared (see its HOT91 line).
[[maybe_unused]] void func_089B439C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t base = 0x08D74FD0u;
    const std::uint32_t p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[16] = base;
    ctx.gpr[19] = self;
    ctx.gpr[20] = p4;
    ctx.gpr[18] = p8;
    ctx.gpr[17] = object;  // the delay slot of the first jal
    (void)call_set(rt, ctx, 0x08A0387Cu, {p8});
    const std::uint32_t vtable = word(rt, object);
    put(rt, frame.at(0x10u), base + 0xCu);
    put(rt, frame.at(0x14u), 0xFFFFFFFFu);
    put(rt, pC, 0x08CCED90u);
    put(rt, frame.at(0x18u), p8);
    const std::uint32_t slot = word(rt, vtable + 8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    (void)call_set(rt, ctx, slot, {object, pC});
    const std::uint32_t cell = word(rt, frame.at(0x10u));
    ctx.gpr[3] = cell;
    ctx.gpr[2] = 0x08CCD990u;
    ctx.gpr[17] = cell - 0xCu;
    ctx.gpr[5] = 0xFFFFFFFFu;
    put(rt, pC, 0x08CCD990u);
    ctx.gpr[4] = cell - 4u;  // the delay slot of the beq
    if (cell - 0xCu != base) {
        const std::uint32_t count = call_set(rt, ctx, 0x08B60804u, {cell - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = cell - 0xCu;
        ctx.gpr[5] = frame.at(0u);  // the delay slot of the bgtz
        if (signed_of(count) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {cell - 0xCu, frame.at(0u)});
    }
    (void)call_set(rt, ctx, 0x089B396Cu, {self, p8});
    (void)call_set(rt, ctx, 0x08966040u, {self, 2u});
    const std::uint32_t holder = word(rt, p8);
    ctx.gpr[16] = holder;
    if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5670u});
    const std::uint32_t holder_vtable = word(rt, holder);
    const std::uint32_t holder_slot = word(rt, holder_vtable + 0x12Cu);
    ctx.gpr[2] = holder_vtable;
    ctx.gpr[3] = holder_slot;
    (void)call_set(rt, ctx, holder_slot, {holder});
    (void)call_set(rt, ctx, 0x089C9C34u, {p4});
    const std::uint32_t second = word(rt, p4);
    ctx.gpr[16] = second;
    if (second == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA56DCu});
    (void)call_set(rt, ctx, 0x089C8294u, {second, p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
}

// func_089CB164(this): this[0] = D_08CC9FD8 and this[0x38] = D_08CCA230; then the four vectors {begin, end, capacity} at this[0x150], this[0x144], this[0x138] and this[0x12C] are each let go: the first two call func_08C01F00(begin, end, sp + 3) (sp + 2 for the second), the other two
// func_08BEEA84(begin, end, sp + 1) (sp + 0 for the last); then a non-null begin is freed with func_08A5BC54(begin, capacity - begin, 4) (the last two sizes are (capacity - begin) with the low two bits cleared). func_08A0DD40(this) and func_08A5BC54(this, 0x170, 4) end the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): none: every branch and region ran.
void func_089CB164(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[2] = 0x08CCA230u;
    ctx.gpr[20] = self + 0x12Cu;
    ctx.gpr[19] = self + 0x138u;
    ctx.gpr[18] = self + 0x144u;
    ctx.gpr[17] = self;
    ctx.gpr[16] = self + 0x150u;
    ctx.gpr[6] = frame.at(3u);
    put(rt, self, 0x08CC9FD8u);
    put(rt, self + 0x38u, 0x08CCA230u);
    const struct Vector {
        std::uint32_t offset, scan, flag;
        bool rounded;
    } vectors[] = {{0x150u, 0x08C01F00u, 3u, false}, {0x144u, 0x08C01F00u, 2u, false}, {0x138u, 0x08BEEA84u, 1u, true}, {0x12Cu, 0x08BEEA84u, 0u, true}};
    for (const Vector &vector : vectors) {
        ctx.gpr[5] = word(rt, self + vector.offset + 4u);
        (void)call_set(rt, ctx, vector.scan, {word(rt, self + vector.offset), word(rt, self + vector.offset + 4u), frame.at(vector.flag)});
        const std::uint32_t begin = word(rt, self + vector.offset);
        std::uint32_t size = word(rt, self + vector.offset + 8u) - begin;
        if (vector.rounded) size = static_cast<std::uint32_t>(signed_of(size) >> 2) << 2;
        ctx.gpr[2] = begin;
        ctx.gpr[6] = 4u;
        ctx.gpr[4] = begin;
        ctx.gpr[5] = size;
        if (begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {begin, size, 4u});
    }
    (void)call_set(rt, ctx, 0x08A0DD40u, {self});
    (void)call_set(rt, ctx, 0x08A5BC54u, {self, 0x170u, 4u});
}

// func_08A9CB38(this, key, value): func_08B344E0(this + 0x4C); n = func_08C2FB58(this + 0xC, key) (a lower-bound search; this + 0x10 is the end node). When n is not the end node the key string's length (the word at data - 0xC) is compared with the one of n's key (n[0x10]): func_08B36170(key data, n's key data,
// the smaller length) gives r (r, or when it is 0 the length difference, is the answer): a non-negative answer stores value at n[0x14]. Otherwise (the end node, or a negative answer) a new node comes from func_08C2FED8(this + 0xC, n, h) with the holder h at sp + 0xC filled by func_08B88F44(h, key) (sp[0x10] = 0 first), its value is stored at [0x14], and
// h's string data is released (func_08B60804(data - 4, -1), then func_08B8542C(data - 0xC, sp) when that is not positive; skipped when it is D_08D74FD0 + 0xC). func_08B3457C(this + 0x4C) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the key being found with a non-negative answer (0x08A9CBDC bgezl: node[0x14] = value, 0x08A9CC48) never happened (every case inserted a new node), func_08B36170 never returned 0 (the length difference, 0x08A9CBD8, was never the answer) and the string data was never released by func_08B8542C (0x08A9CC38-0x08A9CC40: its reference count was never <= 0 at 0x08A9CC20-0x08A9CC24 bgtz, which was always taken). Read from the asm only: those three paths.
void func_08A9CB38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t empty = 0x08D74FD0u;
    const std::uint32_t holder = frame.at(0xCu);
    ctx.gpr[22] = self + 0x4Cu;
    ctx.gpr[16] = self + 0x10u;
    ctx.gpr[21] = self + 0xCu;
    ctx.gpr[20] = key;
    ctx.gpr[30] = holder;
    ctx.gpr[23] = value;
    (void)call_set(rt, ctx, 0x08B344E0u, {self + 0x4Cu});
    const std::uint32_t node = call_set(rt, ctx, 0x08C2FB58u, {self + 0xCu, key});
    ctx.gpr[19] = node;
    ctx.gpr[18] = node;
    ctx.gpr[6] = frame.at(8u);  // the delay slot of the beq
    bool inserted = true;
    if (self + 0x10u != node) {
        const std::uint32_t text = word(rt, key);
        std::uint32_t length = word(rt, text - 0xCu);
        ctx.gpr[16] = length;
        put(rt, frame.at(4u), length);
        const std::uint32_t node_text = word(rt, node + 0x10u);
        ctx.gpr[2] = node_text;
        const std::uint32_t node_length = word(rt, node_text - 0xCu);
        ctx.gpr[17] = node_length;
        ctx.gpr[2] = frame.at(4u);
        ctx.gpr[3] = node_length < length ? 1u : 0u;
        put(rt, frame.at(8u), node_length);
        std::uint32_t shorter = node_length < length ? frame.at(8u) : frame.at(4u);
        ctx.gpr[6] = shorter;
        const std::uint32_t difference = length - node_length;
        ctx.gpr[16] = difference;
        shorter = word(rt, shorter);
        ctx.gpr[6] = shorter;
        const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {text, word(rt, node + 0x10u), shorter});
        ctx.gpr[16] = order != 0u ? order : difference;
        if (signed_of(ctx.gpr[16]) >= 0) {
            put(rt, node + 0x14u, value);  // the delay slot of the bgezl, only when taken
            inserted = false;
        }
    }
    if (inserted) {
        (void)call_set(rt, ctx, 0x08B88F44u, {holder, key});
        put(rt, frame.at(0x10u), 0u);  // the delay slot of the jal below
        const std::uint32_t fresh = call_set(rt, ctx, 0x08C2FED8u, {self + 0xCu, node, holder});
        ctx.gpr[17] = fresh;
        const std::uint32_t data = word(rt, holder);
        ctx.gpr[2] = empty;
        ctx.gpr[5] = 0xFFFFFFFFu;
        ctx.gpr[4] = data - 4u;
        ctx.gpr[16] = data - 0xCu;
        ctx.gpr[18] = fresh;  // the delay slot of the beq
        if (data - 0xCu != empty) {
            const std::uint32_t count = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
            ctx.gpr[4] = data - 0xCu;
            ctx.gpr[5] = frame.at(0u);  // the delay slot of the bgtz
            if (signed_of(count) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, frame.at(0u)});
        }
        put(rt, fresh + 0x14u, value);
    }
    (void)call_set(rt, ctx, 0x08B3457Cu, {self + 0x4Cu});
}

// func_08AD6AB8(this, count, half): this[0] = 1.0f, the halfwords this[4] = count and this[6] = half (the low halves of a1 and a2); func_08AD6854(this + 8, 0x38, count); this[0x34] = this[0x38] = this[0x3C] = 0; a count above 0x1FFFFFFF calls func_08B5A9C8() first. b = func_08B62344(count * 8) is the
// block (this[0x34] = this[0x38] = b, this[0x3C] = b + count * 8; after the next call this[0x38] = b + count * 8 too), sp[8] = sp[0xC] = 0 and func_08C3E1CC(b, count, sp + 8, sp); func_08B2AE10(this + 0x40 / 0x50 / 0x60 / 0x70 / 0x80 / 0x90, count) six times.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): a count above 0x1FFFFFFF (0x08AD6B14 bnez to 0x08AD6B1C func_08B5A9C8) never occurred.
void func_08AD6AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], count = ctx.gpr[5], half = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[1] = 0x3F800000u;
    ctx.fpr[0] = 1.0f;
    ctx.gpr[18] = count;
    ctx.gpr[17] = self;
    ctx.gpr[19] = self + 0x34u;
    put16(rt, self + 6u, half);
    put_float(rt, self, 1.0f);
    put16(rt, self + 4u, count);  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08AD6854u, {self + 8u, 0x38u, count});
    const bool too_many = 0x1FFFFFFFu < count;
    ctx.gpr[2] = too_many ? 1u : 0u;
    put(rt, self + 0x34u, 0u);
    put(rt, self + 0x38u, 0u);
    put(rt, self + 0x3Cu, 0u);  // the delay slot of the beqz
    if (too_many) (void)call_set(rt, ctx, 0x08B5A9C8u, {});
    const std::uint32_t bytes = count << 3;
    ctx.gpr[16] = bytes;
    const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {bytes});
    ctx.gpr[3] = 0u;
    put(rt, self + 0x3Cu, block + bytes);
    put(rt, self + 0x34u, block);
    put(rt, self + 0x38u, block);
    ctx.gpr[2] = 0u;
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(8u), 0u);  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08C3E1CCu, {block, count, frame.at(8u), frame.at(0u)});
    const std::uint32_t begin = word(rt, self + 0x34u);
    ctx.gpr[2] = begin;
    ctx.gpr[16] = bytes + begin;
    put(rt, self + 0x38u, bytes + begin);  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08B2AE10u, {self + 0x40u, count});
    for (std::uint32_t offset = 0x50u; offset <= 0x90u; offset += 0x10u) (void)call_set(rt, ctx, 0x08B2AE10u, {self + offset, count});
}

// func_08B166F8(this, text, node): node == 0 takes n = func_08B17B84(0) (null: return 0, v0 = s4 = 0 with nothing else done), otherwise func_08B17A20(node) runs and n = node. n[0x38] = this; len = func_08B35F60(the word this[0xA0]) + 2 + func_08B35F60(text);
// b = func_08B17A88(len); func_08B17AE4(n, b); d = func_08B17A98(b) gives the record at sp = {d, len, 0} with d[0] = 0; func_08B23928(sp, the word this[0xA0]); q = func_08B237D8(the word this[0xA4], text); func_08B23A70(sp, q); the virtual slot 0x9C of this is called with (this, d, sp[8]);
// func_08B17AA8(b); func_08B10BB4(this, n); v0 = n.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the node argument being 0 (0x08B16724 bnez falls into 0x08B1672C: func_08B17B84(0), the early return of v0 = 0 at 0x08B1673C and the s4 = v0 path) never occurred: every case passed a node. That path is read from the asm only.
void func_08B166F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], text = ctx.gpr[5], given = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = text;
    ctx.gpr[20] = given;
    ctx.gpr[19] = self;
    ctx.gpr[4] = given;
    if (given == 0u) {
        const std::uint32_t created = call_set(rt, ctx, 0x08B17B84u, {given});
        ctx.gpr[20] = created;  // the delay slot of the bnez
        if (created == 0u) {
            ctx.gpr[2] = created;
            return;
        }
    } else {
        (void)call_set(rt, ctx, 0x08B17A20u, {given});
    }
    const std::uint32_t node = ctx.gpr[20];
    put(rt, node + 0x38u, self);
    const std::uint32_t first = call_set(rt, ctx, 0x08B35F60u, {word(rt, self + 0xA0u)});
    ctx.gpr[16] = first + 2u;  // the delay slot of the next jal
    const std::uint32_t second = call_set(rt, ctx, 0x08B35F60u, {text});
    const std::uint32_t length = first + 2u + second;
    ctx.gpr[16] = length;
    const std::uint32_t block = call_set(rt, ctx, 0x08B17A88u, {length});
    ctx.gpr[18] = block;
    (void)call_set(rt, ctx, 0x08B17AE4u, {node, block});
    const std::uint32_t data = call_set(rt, ctx, 0x08B17A98u, {block});
    put(rt, frame.at(0u), data);
    ctx.gpr[17] = data;
    put(rt, frame.at(4u), length);
    put(rt, frame.at(8u), 0u);
    put8(rt, data, 0u);
    (void)call_set(rt, ctx, 0x08B23928u, {frame.at(0u), word(rt, self + 0xA0u)});
    const std::uint32_t found = call_set(rt, ctx, 0x08B237D8u, {word(rt, self + 0xA4u), text});
    (void)call_set(rt, ctx, 0x08B23A70u, {frame.at(0u), found});
    const std::uint32_t vtable = word(rt, self);
    const std::uint32_t third = word(rt, frame.at(8u));
    const std::uint32_t slot = word(rt, vtable + 0x9Cu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    (void)call_set(rt, ctx, slot, {self, data, third});
    (void)call_set(rt, ctx, 0x08B17AA8u, {block});
    (void)call_set(rt, ctx, 0x08B10BB4u, {self, node});
    ctx.gpr[2] = node;
}

// func_08B6F37C(): a pair of calls on 64-bit values held in (v0, v1) pairs: x = func_08B70F28(); y = func_08B71394(x, D_08CB6930 pair); z = func_08B6F110(y); w = func_08B71CE0(z); with z's low word negative (z being the register pair's first word) the pair w is first
// replaced by func_08B71328(w, D_08CB6938 pair); then u = func_08B71394(w, D_08CB6940 pair), t = func_08B71328(x, u), and the result is func_08B6F110(t): v0 is its low word, v1 is z's low word (the original ORs s2 = 0 into v0 and s3 = z into v1).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the negative first result (0x08B6F3E8 bltz to 0x08B6F454: func_08B71328 on the pair first, then the same tail from 0x08B6F46C) never occurred; the constants D_08CB6938 / D_08CB693C of that path were read by hand.
void func_08B6F37C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = 0u;
    const std::uint32_t x_low = call_set(rt, ctx, 0x08B70F28u, {});
    const std::uint32_t x_high = ctx.gpr[3];
    ctx.gpr[20] = x_low;
    ctx.gpr[2] = high_half(0x08CB6930u);
    ctx.gpr[21] = x_high;  // the delay slot of the next jal
    const std::uint32_t y_low = call_set(rt, ctx, 0x08B71394u, {x_low, x_high, word(rt, 0x08CB6930u), word(rt, 0x08CB6934u)});
    const std::uint32_t y_high = ctx.gpr[3];
    const std::uint32_t z = call_set(rt, ctx, 0x08B6F110u, {y_low, y_high});
    ctx.gpr[4] = z;
    ctx.gpr[16] = z;
    ctx.gpr[19] = z;  // the delay slot of the next jal
    std::uint32_t w_low = call_keep(rt, ctx, 0x08B71CE0u, {z});
    std::uint32_t w_high = ctx.gpr[3];
    ctx.gpr[8] = w_low;
    ctx.gpr[9] = w_high;
    ctx.gpr[4] = w_low;
    ctx.gpr[5] = w_high;
    if (signed_of(z) < 0) {
        w_low = call_set(rt, ctx, 0x08B71328u, {w_low, w_high, word(rt, 0x08CB6938u), word(rt, 0x08CB693Cu)});
        ctx.gpr[8] = w_low;
        w_high = ctx.gpr[3];
        ctx.gpr[9] = w_high;
    }
    ctx.gpr[2] = high_half(0x08CB6940u);
    const std::uint32_t u_low = call_set(rt, ctx, 0x08B71394u, {w_low, w_high, word(rt, 0x08CB6940u), word(rt, 0x08CB6944u)});
    const std::uint32_t u_high = ctx.gpr[3];
    const std::uint32_t t_low = call_set(rt, ctx, 0x08B71328u, {x_low, x_high, u_low, u_high});
    const std::uint32_t t_high = ctx.gpr[3];
    const std::uint32_t result = call_set(rt, ctx, 0x08B6F110u, {t_low, t_high});
    ctx.gpr[5] = 0u;
    ctx.gpr[2] = result;
    ctx.gpr[3] = z;
}

// func_08B20A78(this, record): kind = the halfword record[2]; with i = kind - 1 and d = kind - 0x10: a kind of 1..7 gives s0 = s1 = i; a kind outside 1..7 whose d is below 0x38 gives s0 = s1 = d % 7 (the original divides by the multiplicative inverse 0x24924925); any other kind gives s0 = s1 = 0.
// r = func_08B21894(record); func_08B217A0(sp) writes the record at sp; sp[4] = 0x4B (a byte); when s0 is non-zero, or kind == 0x48, func_08B217AC(sp, r, 0x4B, a3, 0) runs with a3 = 0x4B when d is below 0x38 and 0 otherwise (0 for kind == 0x48 with s0 == 0);
// then func_08B215B8(this, s1, record + 4, sp, 0); v0 = this.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): kinds 1..7 (0x08B20ABC bnez to 0x08B20B00, s0 = s1 = kind - 1), the division by 7 for kinds with d = kind - 0x10 below 0x38 (0x08B20AD0-0x08B20AFC: s0 = s1 = d % 7), the call of func_08B217AC with s0 non-zero (0x08B20B38 bnez to 0x08B20B58, a3 = 0x4B when d is below 0x38) and kinds other than 0x48 with s0 == 0 (0x08B20B50 bne to 0x08B20B60, skipping the call) never occurred: every case had a kind outside 1..7 with d >= 0x38 and kind == 0x48 (the call with a3 = 0 ran). Read from the asm only: the other three paths and the constants 7, 0x24924925 (the division), 0x38 and 0x4B.
void func_08B20A78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], record = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = record + 4u;
    ctx.gpr[20] = self;
    const std::uint32_t kind = half_u(rt, record + 2u);
    ctx.gpr[18] = kind;
    const std::uint32_t index = kind - 1u, offset = kind - 0x10u;
    ctx.gpr[17] = index;
    ctx.gpr[6] = offset;
    ctx.gpr[2] = index < 7u ? 1u : 0u;
    ctx.gpr[3] = offset < 0x38u ? 1u : 0u;
    ctx.gpr[16] = index;  // the delay slot of the bnez
    if (index >= 7u) {
        ctx.gpr[16] = 0u;
        ctx.gpr[17] = 0u;  // the delay slot of the beqz
        if (offset < 0x38u) {
            const std::uint32_t remainder = offset % 7u;
            ctx.gpr[16] = remainder;
            ctx.gpr[17] = remainder;
        }
    }
    const std::uint32_t built = call_set(rt, ctx, 0x08B21894u, {record});
    ctx.gpr[19] = built;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08B217A0u, {frame.at(0u)});
    const std::uint32_t in_range = (kind - 0x10u) < 0x38u ? 1u : 0u;
    const std::uint32_t last = in_range != 0u ? 0x4Bu : 0u;
    ctx.gpr[2] = in_range;
    ctx.gpr[7] = last;
    put8(rt, frame.at(4u), 0x4Bu);  // the delay slot of the bnez
    if (ctx.gpr[16] != 0u) {
        (void)call_set(rt, ctx, 0x08B217ACu, {frame.at(0u), built, 0x4Bu, last, 0u});
    } else {
        ctx.gpr[2] = 0x48u;
        ctx.gpr[7] = 0u;  // the delay slot of the bne
        if (kind == 0x48u) (void)call_set(rt, ctx, 0x08B217ACu, {frame.at(0u), built, 0x4Bu, 0u, 0u});
    }
    (void)call_set(rt, ctx, 0x08B215B8u, {self, ctx.gpr[17], record + 4u, frame.at(0u), 0u});
    ctx.gpr[2] = self;
}

// func_089BDD18(this, out, key): with e = the word D_08CF6F10 and key[0] == e: r = func_089BAB0C(this, out, key) is stored at sp[0], func_08A7C0E0(sp + 4, sp), func_08A7B544(sp + 8, key, sp + 4); out != sp + 8 copies the holder with func_08A5BAD8(out, sp[8]); func_08A5B9EC(sp + 8) and func_08A5B9EC(sp + 4) end it.
// Otherwise, when this[0x5C] is non-zero func_089BD8EC(this, out, key) runs first and a non-zero word at out ends the function (v0 = that word); then func_08A7D708(this, out, key), again a non-zero out[0] ends it; p = this[0x3C] (null: end, v0 = 0), p[4] == 0 ends it (v0 = 0), otherwise the virtual slot 0x10C of p is
// called with (p, out, key) and func_08A7BEBC(this, key, out) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the first part (key[0] equal to the word D_08CF6F10, 0x089BDD44 falling into 0x089BDD4C-0x089BDD9C) never ran; neither did this[0x5C] != 0 (func_089BD8EC at 0x089BDDAC, the early return at 0x089BDDB8 and 0x089BDDD4 with v0 = the word at out) nor out[0] != 0 after func_08A7D708 and nor this[0x3C] == 0 / p[4] == 0 (0x089BDDE0, 0x089BDDEC): every case reached the virtual call of slot 0x10C. Read from the asm only: those paths.
void func_089BDD18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5], key = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[18] = key;
    ctx.gpr[17] = self;
    const std::uint32_t expected = word(rt, 0x08CF6F10u), found = word(rt, key);
    ctx.gpr[2] = high_half(0x08CF6F10u);
    ctx.gpr[7] = expected;
    ctx.gpr[3] = found;
    ctx.gpr[16] = out;  // the delay slot of the bne
    if (found == expected) {
        ctx.gpr[19] = frame.at(4u);  // the delay slot of the next jal
        const std::uint32_t built = call_set(rt, ctx, 0x089BAB0Cu, {self, out, key});
        ctx.gpr[17] = frame.at(8u);
        put(rt, frame.at(0u), built);  // the delay slot of the jal below
        (void)call_set(rt, ctx, 0x08A7C0E0u, {frame.at(4u), frame.at(0u)});
        (void)call_set(rt, ctx, 0x08A7B544u, {frame.at(8u), key, frame.at(4u)});
        const std::uint32_t copied = word(rt, frame.at(8u));
        ctx.gpr[5] = copied;  // the delay slot of the beq
        if (out != frame.at(8u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, copied});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        return;
    }
    const std::uint32_t hook = word(rt, self + 0x5Cu);
    ctx.gpr[2] = hook;
    if (hook != 0u) {
        (void)call_set(rt, ctx, 0x089BD8ECu, {self, out, key});
        const std::uint32_t first = word(rt, out);
        ctx.gpr[2] = first;
        if (first != 0u) return;
    }
    (void)call_set(rt, ctx, 0x08A7D708u, {self, out, key});
    const std::uint32_t second = word(rt, out);
    ctx.gpr[2] = second;
    if (second != 0u) return;
    const std::uint32_t target = word(rt, self + 0x3Cu);
    ctx.gpr[4] = target;
    if (target == 0u) return;
    const std::uint32_t armed = word(rt, target + 4u);
    ctx.gpr[2] = armed;
    if (armed == 0u) return;
    const std::uint32_t vtable = word(rt, target);
    const std::uint32_t slot = word(rt, vtable + 0x10Cu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    (void)call_set(rt, ctx, slot, {target, out, key});
    (void)call_set(rt, ctx, 0x08A7BEBCu, {self, key, out});
}

// func_08C20540(this, a): r = func_08A6158C(a) gives the holder cell at r (r[0]: null -> func_08A5BB7C(D_08CB0284) first); func_08A76AA0(sp); the virtual slot 0xC0 of r[0] is called with (r[0], sp); h = sp[0] (null: func_08A5BB7C(D_08CB0284) first); the virtual slot 0x60 of h
// with (sp + 4, h); func_08A76AA0(sp + 8); func_08A7858C(sp + 8, this + 8, 1, sp + 4); r[0] again (null: func_08A5BB7C(D_08CB0284) first) with its virtual slot 0xC4 called with (r[0], sp + 4); func_08A61584(a, sp + 8); the three holders sp + 8, sp + 4 and sp are destroyed with func_08A5B9EC (v0 is the last one's).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the three null checks of r[0] / h / r[0] (0x08C2058C, 0x08C205B8, 0x08C205FC; null-case call D_08CB0284) never fired.
void func_08C20540(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u);
    ctx.gpr[20] = self;
    ctx.gpr[21] = argument;
    ctx.gpr[19] = p4;
    ctx.gpr[17] = p8;
    ctx.gpr[4] = argument;
    const std::uint32_t cell = call_set(rt, ctx, 0x08A6158Cu, {argument});
    ctx.gpr[18] = cell;
    (void)call_set(rt, ctx, 0x08A76AA0u, {p0});
    std::uint32_t object = word(rt, cell);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    std::uint32_t vtable = word(rt, object);
    std::uint32_t slot = word(rt, vtable + 0xC0u);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = slot;
    ctx.gpr[4] = object;
    (void)call_set(rt, ctx, slot, {object, p0});  // the delay slot sets a1 = sp
    object = word(rt, p0);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0x60u);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = slot;
    ctx.gpr[5] = object;
    (void)call_set(rt, ctx, slot, {p4, object});
    (void)call_set(rt, ctx, 0x08A76AA0u, {p8});
    (void)call_set(rt, ctx, 0x08A7858Cu, {p8, self + 8u, 1u, p4});
    object = word(rt, cell);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0284u});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0xC4u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    (void)call_set(rt, ctx, slot, {object, p4});
    ctx.gpr[5] = p8;
    (void)call_set(rt, ctx, 0x08A61584u, {argument, p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
}

// func_08A54830(this, tree, strings, hint): func_08A76AA0(this); n = func_08C1D1AC(tree + 0x38, hint); n == tree + 0x3C (the end) returns this at once. Otherwise c = n[0x14] (null: func_08A5BB7C(D_08CAF594) first), c[0x10] is a base offset to which the word strings[0x1C]
// is added (when that word is 0: strings[0x18], null -> func_08A5BB7C(D_08CAF540), its word [0xC] is the base instead). With the cell d = n[0x14] (null: func_08A5BB7C(D_08CAF594) again) func_08A57B54(sp, d + 0xC, strings + 0x18, sum) fills the holder at sp; l = sp[0] (null: func_08A5BB7C(D_08CAF5E0) first);
// func_08A56884(sp + 4, l); sp + 4 != this copies the holder to this with func_08A5BAD8(this, sp[4]); func_08A5B9EC(sp + 4) and func_08A5B9EC(sp); v0 = this.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): n == tree + 0x3C (0x08A54870 beql, the early return of this), the null checks of c (0x08A5487C, D_08CAF594), of strings[0x18] (0x08A548A4, D_08CAF540), of the cell n[0x14] again (0x08A548C4) and of the list sp[0] (0x08A548F0, D_08CAF5E0) never fired, and this == sp + 4 (0x08A54914) never happened.
void func_08A54830(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], tree = ctx.gpr[5], strings = ctx.gpr[6], hint = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = hint;
    ctx.gpr[16] = tree;
    ctx.gpr[19] = self;
    ctx.gpr[18] = strings;
    (void)call_set(rt, ctx, 0x08A76AA0u, {self});
    const std::uint32_t node = call_set(rt, ctx, 0x08C1D1ACu, {tree + 0x38u, hint});
    ctx.gpr[16] = tree + 0x3Cu;
    if (node == tree + 0x3Cu) {
        ctx.gpr[2] = self;  // the delay slot of the beql, only when taken
        return;
    }
    const std::uint32_t cell = word(rt, node + 0x14u);
    ctx.gpr[16] = cell;
    ctx.gpr[20] = node + 0x14u;  // the delay slot of the bnez
    if (cell == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
    const std::uint32_t base = word(rt, cell + 0x10u);
    ctx.gpr[16] = base;
    std::uint32_t pointer = word(rt, strings + 0x1Cu);
    ctx.gpr[2] = pointer;
    ctx.gpr[17] = pointer + base;  // the delay slot of the bnez
    if (pointer == 0u) {
        const std::uint32_t other = word(rt, strings + 0x18u);
        ctx.gpr[17] = other;
        if (other == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF540u});
        pointer = word(rt, other + 0xCu);
        ctx.gpr[2] = pointer;
        ctx.gpr[17] = pointer + base;
    }
    const std::uint32_t target = word(rt, node + 0x14u);
    ctx.gpr[16] = target;
    ctx.gpr[7] = ctx.gpr[17];  // the delay slot of the bnez
    if (target == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
        ctx.gpr[7] = ctx.gpr[17];
    }
    (void)call_set(rt, ctx, 0x08A57B54u, {frame.at(0u), target + 0xCu, strings + 0x18u, ctx.gpr[17]});
    const std::uint32_t list = word(rt, frame.at(0u));
    ctx.gpr[17] = list;
    ctx.gpr[16] = frame.at(4u);  // the delay slot of the bnez
    if (list == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF5E0u});
        ctx.gpr[16] = frame.at(4u);
    }
    (void)call_set(rt, ctx, 0x08A56884u, {frame.at(4u), list});
    const std::uint32_t copied = word(rt, frame.at(4u));
    ctx.gpr[5] = copied;  // the delay slot of the beq
    if (self != frame.at(4u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {self, copied});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = self;
}

// func_08C34FE8(this, key): a search in the tree whose end node is this + 4 and whose root is this[8] (a node: [8] left, [0xC] right, [0x10] a string pointer, [0x14] a word). With w = key[4]: from the root, a node whose [0x14] is below w goes right, one above goes left (remembering it as the candidate
// s4); on equal words the strings are compared (the lengths are the words at data - 0xC; func_08B36170(node's data, key data, the smaller length) gives r, or the length difference when r is 0): a negative answer goes right, otherwise left (and the node is the candidate). Afterwards the candidate (this + 4 when none) is checked: the end node
// is returned as it is; a candidate whose word is above w is returned as the end node; one whose word is below w is the answer; on equal words func_08B36170(key data, candidate's data, the smaller length) (key length - candidate length when 0) negative returns the end node, else the candidate.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the root being null (0x08C35018 beqz), a non-zero func_08B36170 result (it returned 0 each time, so the length difference decided, and the node's string was never shorter than the key's: the minimum was always the key's length, the stack word sp), a negative answer (0x08C3507C bltzl), a candidate equal to the end node (0x08C35094 beq), a candidate with a word below w (0x08C350B4 bnel) and, in the tail comparison, a non-zero result (0x08C350F0 movn) or a negative one (0x08C350F4 bltz) never occurred: every case ended with equal words and equal strings. Read from the asm only: those paths and the minimum-length selections (0x08C35064 movn, 0x08C350DC movz).
void func_08C34FE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t end = self + 4u;
    ctx.gpr[22] = end;
    ctx.gpr[21] = key;
    std::uint32_t node = word(rt, self + 8u);
    ctx.gpr[18] = node;
    ctx.gpr[20] = end;  // the delay slot of the beqz
    if (node != 0u) {
        const std::uint32_t wanted = word(rt, key + 4u);
        ctx.gpr[19] = wanted;
        std::uint32_t next = 0u;
        for (;;) {
            const std::uint32_t current = word(rt, node + 0x14u);
            ctx.gpr[2] = current;
            ctx.gpr[3] = wanted < current ? 1u : 0u;
            if (current < wanted) {
                next = word(rt, node + 0xCu);
            } else if (wanted < current) {
                next = word(rt, node + 8u);
                ctx.gpr[20] = node;
            } else {
                const std::uint32_t node_text = word(rt, node + 0x10u);
                const std::uint32_t key_text = word(rt, key);
                ctx.gpr[2] = node_text;
                ctx.gpr[4] = key_text;
                ctx.gpr[5] = key_text;
                const std::uint32_t node_length = word(rt, node_text - 0xCu), key_length = word(rt, key_text - 0xCu);
                ctx.gpr[16] = node_length;
                put(rt, frame.at(4u), node_length);
                ctx.gpr[17] = key_length;
                put(rt, frame.at(0u), key_length);
                ctx.gpr[3] = key_length < node_length ? 1u : 0u;
                const std::uint32_t shorter = key_length < node_length ? key_length : node_length;
                ctx.gpr[2] = key_length < node_length ? frame.at(0u) : frame.at(4u);
                const std::uint32_t difference = node_length - key_length;
                ctx.gpr[16] = difference;
                ctx.gpr[6] = shorter;
                const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {word(rt, node + 0x10u), key_text, shorter});
                ctx.gpr[16] = order != 0u ? order : difference;
                if (signed_of(ctx.gpr[16]) < 0) {
                    next = word(rt, node + 0xCu);  // the delay slot of the bltzl, only when taken
                } else {
                    next = word(rt, node + 8u);
                    ctx.gpr[20] = node;
                }
            }
            ctx.gpr[2] = next;
            ctx.gpr[18] = next;  // the delay slot of the bnez
            node = next;
            if (next == 0u) break;
        }
    }
    const std::uint32_t candidate = ctx.gpr[20];
    ctx.gpr[18] = end;  // the delay slot of the beq
    if (candidate == end) {
        ctx.gpr[2] = end;
        return;
    }
    const std::uint32_t wanted = word(rt, key + 4u);
    const std::uint32_t found = word(rt, candidate + 0x14u);
    ctx.gpr[4] = wanted;
    ctx.gpr[3] = found;
    ctx.gpr[2] = end;
    if (wanted < found) return;
    if (found < wanted) {
        ctx.gpr[18] = candidate;
        ctx.gpr[2] = candidate;
        return;
    }
    const std::uint32_t key_text = word(rt, key);
    ctx.gpr[4] = key_text;
    const std::uint32_t key_length = word(rt, key_text - 0xCu);
    ctx.gpr[16] = key_length;
    put(rt, frame.at(0u), key_length);
    const std::uint32_t candidate_text = word(rt, candidate + 0x10u);
    const std::uint32_t candidate_length = word(rt, candidate_text - 0xCu);
    ctx.gpr[17] = candidate_length;
    ctx.gpr[2] = frame.at(4u);
    ctx.gpr[3] = candidate_length < key_length ? 1u : 0u;
    put(rt, frame.at(4u), candidate_length);
    const std::uint32_t shorter = candidate_length < key_length ? candidate_length : key_length;
    ctx.gpr[2] = candidate_length < key_length ? frame.at(4u) : frame.at(0u);
    const std::uint32_t difference = key_length - candidate_length;
    ctx.gpr[16] = difference;
    ctx.gpr[6] = shorter;
    const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {key_text, word(rt, candidate + 0x10u), shorter});
    ctx.gpr[16] = order != 0u ? order : difference;
    ctx.gpr[2] = end;  // the delay slot of the bltz
    if (signed_of(ctx.gpr[16]) < 0) return;
    ctx.gpr[18] = candidate;
    ctx.gpr[2] = candidate;
}

// func_08A46DCC(this): func_08A69B3C(sp, D_08CAEAA4); o = func_08A5BC20(0x38, 4); func_08A7E914(o, sp); the word o is stored at sp + 4 and func_08A76AB4(sp + 8, sp + 4) fills the holder at sp + 8; func_08A69D68(sp). With h = sp[8] (re-read, null: func_08A5BB7C(D_08CAEAB4) first, five times) func_08B86FB0(h, name, callback) registers
// five pairs: (D_08CAEABC, 0x08A47294), (D_08CAEAC4, 0x08A471A0), (D_08CAEAD0, 0x08A470A8), (D_08CAEADC, 0x08A46F64), (D_08CAEAEC, 0x08A46F64). Then g = this[0] (null: func_08A5BB7C(D_08CAEAB4) first), func_08A7CE50(g, sp + 8) and func_08A5B9EC(sp + 8) (v0 is its result).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the five null checks of h = sp[8] (0x08A46E34, 0x08A46E64, 0x08A46E94, 0x08A46EC4, 0x08A46EF4; null-case call D_08CAEAB4) and the null check of this[0] (0x08A46F20) never fired.
void func_08A46DCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u);
    ctx.gpr[19] = self;
    ctx.gpr[18] = p8;
    (void)call_set(rt, ctx, 0x08A69B3Cu, {p0, 0x08CAEAA4u});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[16] = object;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A7E914u, {object, p0});
    put(rt, p4, object);  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08A76AB4u, {p8, p4});
    (void)call_set(rt, ctx, 0x08A69D68u, {p0});
    const struct Pair {
        std::uint32_t name, callback;
    } pairs[] = {{0x08CAEABCu, 0x08A47294u}, {0x08CAEAC4u, 0x08A471A0u}, {0x08CAEAD0u, 0x08A470A8u}, {0x08CAEADCu, 0x08A46F64u}, {0x08CAEAECu, 0x08A46F64u}};
    for (unsigned index = 0u; index < 5u; ++index) {
        const std::uint32_t holder = word(rt, p8);
        ctx.gpr[16] = holder;
        if (index == 3u) ctx.gpr[17] = high_half(0x08A46F64u);  // the delay slot of the bnez
        if (holder == 0u) {
            (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAEAB4u});
            if (index == 3u) ctx.gpr[17] = high_half(0x08A46F64u);
        }
        (void)call_set(rt, ctx, 0x08B86FB0u, {holder, pairs[index].name, pairs[index].callback});
    }
    const std::uint32_t target = word(rt, self);
    ctx.gpr[16] = target;
    if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAEAB4u});
    (void)call_set(rt, ctx, 0x08A7CE50u, {target, p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
}

// func_08A9FFD0(this, a, b): func_08B85EB8(sp + 8, b, sp + 1); r = func_08A69530(sp + 8); the string data d = sp[8] is released when it is not D_08D74FD0 + 0xC (func_08B60804(d - 4, -1), then func_08B8542C(d - 0xC, sp) when that is not positive); r == 0 returns 0.
// Otherwise func_08A9DD14(sp + 0xC); func_08A6D2D4(sp + 0x10, D_08CFF348); u = sp[0xC] (null: func_08A5BB7C(D_08CB277C) first); k = func_08A9BD30(u); m = k[0] (null: func_08A5BB7C(D_08CB2784) first); func_08A69B3C(sp + 4, b); func_08A7BFD0(m, sp + 0x10, sp + 4); func_08A69D68(sp + 4); q = sp[0x10]:
// non-zero gives func_08A6BFFC(q) and func_08B7C830(this, that result, a) with v0 = 1, zero leaves v0 = 0; the holders sp + 0x10 and sp + 0xC are destroyed with func_08A5B9EC.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): only the shortest path ran: func_08A69530 always returned 0 (0x08AA0050 beqz s1 to 0x08AA0108), the string data was always released with func_08B8542C (0x08AA0040 bgtz never taken). Everything from 0x08AA0058 to 0x08AA0104 is read from the asm only: the constants D_08CFF348, D_08CB277C, D_08CB2784, the holder copies and the bnel / beqz on sp[0x10], including the (v0 = 1) result.
void func_08A9FFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], second = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u);
    ctx.gpr[18] = second;
    ctx.gpr[16] = p8;
    ctx.gpr[21] = first;
    ctx.gpr[20] = self;
    (void)call_set(rt, ctx, 0x08B85EB8u, {p8, second, frame.at(1u)});
    const std::uint32_t result = call_set(rt, ctx, 0x08A69530u, {p8});
    const std::uint32_t text = word(rt, p8);
    ctx.gpr[4] = text;
    ctx.gpr[17] = result;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[16] = text - 0xCu;
    ctx.gpr[4] = text - 4u;  // the delay slot of the beq
    if (text - 0xCu != 0x08D74FD0u) {
        const std::uint32_t count = call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = text - 0xCu;
        if (signed_of(count) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {text - 0xCu, frame.at(0u)});
    }
    ctx.gpr[16] = 0u;  // the delay slot of the beqz
    if (result == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[22] = pC;
    (void)call_set(rt, ctx, 0x08A9DD14u, {pC});
    ctx.gpr[19] = p10;
    (void)call_set(rt, ctx, 0x08A6D2D4u, {p10, 0x08CFF348u});
    const std::uint32_t unit = word(rt, pC);
    ctx.gpr[16] = unit;
    if (unit == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB277Cu});
    const std::uint32_t slot = call_set(rt, ctx, 0x08A9BD30u, {unit});
    const std::uint32_t middle = word(rt, slot);
    ctx.gpr[17] = middle;
    if (middle != 0u) {
        ctx.gpr[16] = p4;  // the delay slot of the bnel, only when taken
    } else {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2784u});
        ctx.gpr[16] = p4;
    }
    (void)call_set(rt, ctx, 0x08A69B3Cu, {p4, second});
    (void)call_set(rt, ctx, 0x08A7BFD0u, {middle, p10, p4});
    (void)call_set(rt, ctx, 0x08A69D68u, {p4});
    const std::uint32_t pending = word(rt, p10);
    ctx.gpr[4] = pending;
    ctx.gpr[16] = 0u;  // the delay slot of the beqz
    if (pending != 0u) {
        ctx.gpr[16] = 1u;  // the delay slot of the jal below
        const std::uint32_t made = call_set(rt, ctx, 0x08A6BFFCu, {pending});
        ctx.gpr[5] = made;
        (void)call_set(rt, ctx, 0x08B7C830u, {self, made, first});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
    ctx.gpr[2] = ctx.gpr[16];
}

// func_08991490(this): func_08A69B3C(sp, D_08CA3F0C); o = func_08A5BC20(0x38, 4); func_08A7E914(o, sp); the word o is stored at sp + 4 and func_08A76AB4(this, sp + 4) runs; func_08A69D68(sp). With h = this[0] (re-read before each, null: func_08A5BB7C(D_08CA3F18) first) func_08B86FB0(h, name, callback)
// registers five pairs: (D_08CA3F20, 0x08A11E08), (D_08CA3F28, 0x08A11E10), (D_08CA3F2C, 0x08A11E18), (D_08CA3F34, 0x08A11E20), (D_08CA3F44, 0x08A11E28). v0 = this.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the null check of this[0] (0x089914EC, 0x0899151C, 0x0899154C, 0x0899157C, 0x089915AC; null-case call D_08CA3F18) never fired.
void func_08991490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u);
    ctx.gpr[17] = self;
    (void)call_set(rt, ctx, 0x08A69B3Cu, {p0, 0x08CA3F0Cu});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[16] = object;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A7E914u, {object, p0});
    put(rt, p4, object);  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08A76AB4u, {self, p4});
    (void)call_set(rt, ctx, 0x08A69D68u, {p0});
    const struct Pair {
        std::uint32_t name, callback;
    } pairs[] = {{0x08CA3F20u, 0x08A11E08u}, {0x08CA3F28u, 0x08A11E10u}, {0x08CA3F2Cu, 0x08A11E18u}, {0x08CA3F34u, 0x08A11E20u}, {0x08CA3F44u, 0x08A11E28u}};
    for (const Pair &pair : pairs) {
        const std::uint32_t holder = word(rt, self);
        ctx.gpr[16] = holder;
        if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA3F18u});
        (void)call_set(rt, ctx, 0x08B86FB0u, {holder, pair.name, pair.callback});
    }
    ctx.gpr[2] = self;
}

// func_08A206E8(out): a = func_08AC32A4(); b = func_08AC329C(); func_08C193EC(sp + 0xC, b, 0, sp + 1) fills the string cell at sp + 0xC; when its reference word (data - 4) is not negative func_08B88FF8(sp + 0xC) runs first, then the data pointer and its length (the word at data - 0xC) are read again;
// func_08AC3554(a, data, length) decides: a non-zero answer repeats that read and calls func_08A950D0(sp + 8, data, length, 0x10) and copies the holder sp + 8 to `out` with func_08A5BAD8(out, sp[8]) unless it is `out`; zero calls func_08A94CC4(sp + 4, D_08CF6ED8) and copies sp + 4 the same way.
// func_08A5B9EC(that holder) and the string data d = sp[0xC] is released when it is not D_08D74FD0 + 0xC (func_08B60804(d - 4, -1), then func_08B8542C(d - 0xC, sp) when that is not positive).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): a zero answer of func_08AC3554 (0x08A2074C beqz to 0x08A20798: func_08A94CC4(sp + 4, D_08CF6ED8) and the copy of sp + 4), `out` equal to the holder (0x08A20780, 0x08A207AC), a shared empty cell (0x08A207D4) and a positive reference count (0x08A207E8) never occurred. Read from the asm only: the zero-answer path and the constant D_08CF6ED8.
void func_08A206E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t pC = frame.at(0xCu);
    ctx.gpr[17] = out;
    const std::uint32_t first = call_set(rt, ctx, 0x08AC32A4u, {});
    ctx.gpr[18] = first;
    ctx.gpr[16] = pC;  // the delay slot of the next jal
    const std::uint32_t second = call_set(rt, ctx, 0x08AC329Cu, {});
    (void)call_set(rt, ctx, 0x08C193ECu, {pC, second, 0u, frame.at(1u)});
    // The data pointer and length of the string cell, after func_08B88FF8 when the reference word is not negative (the delay slot of the bltzl reads the length only when the branch is taken).
    const auto read_string = [&](std::uint32_t &data, std::uint32_t &length) {
        data = word(rt, pC);
        const std::uint32_t references = word(rt, data - 4u);
        ctx.gpr[2] = references;
        if (signed_of(references) >= 0) {
            (void)call_set(rt, ctx, 0x08B88FF8u, {pC});
            data = word(rt, pC);
        }
        length = word(rt, data - 0xCu);
        ctx.gpr[5] = data;
        ctx.gpr[6] = length;
    };
    std::uint32_t data = 0u, length = 0u;
    read_string(data, length);
    const std::uint32_t decided = call_set(rt, ctx, 0x08AC3554u, {first, data, length});
    if (decided != 0u) {
        ctx.gpr[5] = word(rt, pC);  // the delay slot of the beqz
        read_string(data, length);
        ctx.gpr[16] = frame.at(8u);
        ctx.gpr[7] = 0x10u;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A950D0u, {frame.at(8u), data, length, 0x10u});
        if (out != frame.at(8u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(8u))});
    } else {
        ctx.gpr[5] = word(rt, pC);  // the delay slot of the beqz
        ctx.gpr[16] = frame.at(4u);
        (void)call_set(rt, ctx, 0x08A94CC4u, {frame.at(4u), 0x08CF6ED8u});
        if (out != frame.at(4u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(4u))});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {ctx.gpr[16]});
    const std::uint32_t text = word(rt, pC);
    ctx.gpr[4] = text;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[16] = text - 0xCu;
    if (text - 0xCu != 0x08D74FD0u) {
        ctx.gpr[4] = text - 4u;
        const std::uint32_t count = call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = text - 0xCu;
        if (signed_of(count) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {text - 0xCu, frame.at(0u)});
    }
}

// func_0896DFCC(out, a1, mode, source): mode 0: func_08969244(sp), u = sp[0] (null: func_08A5BB7C(D_08CA2880) first), sp[4] = u[0x90], func_08A0FD28(sp + 8, sp + 4), the holder sp + 8 is copied to `out` with func_08A5BAD8(out, sp[8]) unless it is `out`, and func_08A5B9EC(sp + 8), func_08A5B9EC(sp) end it.
// Mode 1: func_08969244(sp + 0xC); u = sp[0xC] (null: func_08A5BB7C(D_08CA2880) first); func_089B8BA0(u + 0x84); when source[0] is non-zero: func_08A0FD80(sp + 0x10, source), the object u again (sp[0xC], null: func_08A5BB7C first), func_089B8F30(u + 0x84, sp[0x10]) and func_08A5B9EC(sp + 0x10); func_08A5B9EC(sp + 0xC) ends it.
// Any other mode returns with v0 = 1 at once.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the null check of u (0x0896DFFC, 0x0896E06C, 0x0896E0A0; null-case call D_08CA2880), `out` equal to sp + 8 (0x0896E028) and any mode other than 0 or 1 (0x0896E054, v0 = 1) never occurred; mode 1 always found source[0] non-zero (0x0896E088 beqz never taken). Mode 1 itself ran.
void func_0896DFCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u);
    ctx.gpr[18] = source;
    ctx.gpr[17] = out;
    if (mode == 0u) {
        (void)call_set(rt, ctx, 0x08969244u, {p0});
        const std::uint32_t object = word(rt, p0);
        ctx.gpr[16] = object;
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2880u});
        const std::uint32_t field = word(rt, object + 0x90u);
        ctx.gpr[2] = field;
        ctx.gpr[16] = p8;
        put(rt, p4, field);  // the delay slot of the jal below
        (void)call_set(rt, ctx, 0x08A0FD28u, {p8, p4});
        if (out != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p8)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    ctx.gpr[19] = pC;
    (void)call_set(rt, ctx, 0x08969244u, {pC});
    std::uint32_t object = word(rt, pC);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2880u});
    (void)call_set(rt, ctx, 0x089B8BA0u, {object + 0x84u});
    const std::uint32_t armed = word(rt, source);
    ctx.gpr[2] = armed;
    ctx.gpr[17] = p10;  // the delay slot of the beqz
    if (armed != 0u) {
        (void)call_set(rt, ctx, 0x08A0FD80u, {p10, source});
        object = word(rt, pC);
        ctx.gpr[16] = object;
        ctx.gpr[5] = word(rt, p10);  // the delay slot of the bnez
        if (object == 0u) {
            (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2880u});
            ctx.gpr[5] = word(rt, p10);
        }
        (void)call_set(rt, ctx, 0x089B8F30u, {object + 0x84u, word(rt, p10)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
}

// func_08A42E10(out, a1, count, args): count <= 0 returns with v0 untouched. Otherwise func_08A40E90(sp + 8); h = sp[8] (null: func_08A5BB7C(D_08CADBFC) first); o = args[0] (null: func_08A5BB7C(D_08CADB98) first); the virtual slot 0xC of o[0] is called with (sp + 4, o);
// func_08A425E4(sp + 0xC, h, sp + 4, 0); func_08A6D2D4(sp + 0x10, sp + 0xC); the holder sp + 0x10 is copied to `out` with func_08A5BAD8(out, sp[0x10]) unless it is `out`; func_08A5B9EC(sp + 0x10); the string data d = sp[0xC] is released when it is not D_08D74FD0 + 0xC (func_08B60804(d - 4, -1), then
// func_08B8542C(d - 0xC, sp + 1) when that is not positive), the data e = sp[4] the same way (the flag pointer is sp), and func_08A5B9EC(sp + 8) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x08A42E30), the null checks of h and o (0x08A42E48, 0x08A42E60; D_08CADBFC and D_08CADB98), `out` equal to sp + 0x10 (0x08A42EB4), a shared empty cell for sp[0xC] (0x08A42EDC beql taken) and the calls of func_08B8542C (0x08A42EF0, 0x08A42F40 bgtz never taken) never occurred.
void func_08A42E10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t empty = 0x08D74FD0u;
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u);
    ctx.gpr[19] = out;
    ctx.gpr[16] = args;
    if (signed_of(count) <= 0) return;
    ctx.gpr[20] = p8;
    (void)call_set(rt, ctx, 0x08A40E90u, {p8});
    const std::uint32_t holder = word(rt, p8);
    ctx.gpr[18] = holder;
    if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADBFCu});
    std::uint32_t first = word(rt, args);
    ctx.gpr[17] = first;
    if (first == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADB98u});
    }
    const std::uint32_t vtable = word(rt, first);
    const std::uint32_t slot = word(rt, vtable + 0xCu);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = slot;
    ctx.gpr[16] = p4;
    ctx.gpr[5] = first;
    ctx.gpr[4] = p4;
    ctx.gpr[17] = pC;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {p4, first});
    ctx.gpr[16] = p10;
    (void)call_set(rt, ctx, 0x08A425E4u, {pC, holder, p4, 0u});
    (void)call_set(rt, ctx, 0x08A6D2D4u, {p10, pC});
    if (out != p10) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p10)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
    const std::uint32_t data = word(rt, pC);
    ctx.gpr[4] = data;
    ctx.gpr[17] = empty;
    ctx.gpr[16] = data - 0xCu;
    std::uint32_t other = 0u;
    if (data - 0xCu == empty) {
        other = word(rt, p4);  // the delay slot of the beql, only when taken
        ctx.gpr[4] = other;
    } else {
        ctx.gpr[4] = data - 4u;
        ctx.gpr[5] = 0xFFFFFFFFu;
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        other = word(rt, p4);  // the delay slot of the bgtz
        ctx.gpr[4] = other;
        if (signed_of(references) <= 0) {
            ctx.gpr[4] = data - 0xCu;
            (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, frame.at(1u)});
            other = word(rt, p4);
            ctx.gpr[4] = other;
        }
    }
    ctx.gpr[16] = other - 0xCu;
    ctx.gpr[4] = other - 4u;  // the delay slot of the beq
    if (empty != other - 0xCu) {
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {other - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = other - 0xCu;  // the delay slot of the bgtz
        if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {other - 0xCu, p0});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
}

// func_08A32678(a0, source, count, args): count <= 0 returns with v0 untouched. s4 = 1; a count of 2 or more takes func_08A85940(sp + 4, args + 4), e = sp[4] (null: func_08A5BB7C(D_08CACBA0) first), r = func_08A855E8(e), func_08A5B9EC(sp + 4) and s4 = (r == 1).
// Then func_08A32598(sp, source); t = sp[0] (null: func_08A5BB7C(D_08CACBA8) first); o = args[0] (null: func_08A5BB7C(D_08CACB6C) first) with its virtual slot 0xDC called with (o, 0) giving p; o again (args[0], null: func_08A5BB7C(D_08CACB6C) first) with its virtual slot 0xD8 called with (o) giving q;
// func_08A32350(t, p, q, s4); func_08A5B9EC(sp) (v0 is its result).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x08A32698) never occurred, count 2 or more (0x08A326A4 beq: func_08A85940, func_08A855E8, s4 = (r == 1)) never happened: every case had count 1 (s4 = 1); the null checks of t, o and o again (0x08A32700, 0x08A32718, 0x08A32744) never fired. Read from the asm only: the count >= 2 part (0x08A326AC-0x08A326EC), including D_08CACBA0 and the xori / sltiu pair.
void func_08A32678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t source = ctx.gpr[5], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u);
    ctx.gpr[18] = source;
    ctx.gpr[17] = args;
    if (signed_of(count) <= 0) return;
    ctx.gpr[2] = count < 2u ? 1u : 0u;  // slti: signed, count is positive here
    ctx.gpr[20] = 1u;  // the delay slot of the bnez
    if (signed_of(count) >= 2) {
        ctx.gpr[19] = p4;
        (void)call_set(rt, ctx, 0x08A85940u, {p4, args + 4u});
        const std::uint32_t entry = word(rt, p4);
        ctx.gpr[16] = entry;
        if (entry == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CACBA0u});
        const std::uint32_t answer = call_set(rt, ctx, 0x08A855E8u, {entry});
        ctx.gpr[16] = answer;
        ctx.gpr[16] = answer ^ 1u;  // the delay slot of the next jal
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
        ctx.gpr[20] = (answer ^ 1u) == 0u ? 1u : 0u;
    }
    (void)call_set(rt, ctx, 0x08A32598u, {p0, source});
    const std::uint32_t target = word(rt, p0);
    ctx.gpr[19] = target;
    std::uint32_t object = word(rt, args);
    ctx.gpr[16] = object;  // the delay slot of the bnel, only when taken
    if (target == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CACBA8u});
        object = word(rt, args);
        ctx.gpr[16] = object;
    }
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CACB6Cu});
    std::uint32_t vtable = word(rt, object);
    std::uint32_t slot = word(rt, vtable + 0xDCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;  // the delay slot of the jalr
    const std::uint32_t first = call_set(rt, ctx, slot, {object, 0u});
    object = word(rt, args);
    ctx.gpr[16] = object;
    ctx.gpr[18] = first;  // the delay slot of the bnez
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CACB6Cu});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0xD8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    const std::uint32_t second = call_set(rt, ctx, slot, {object});
    ctx.gpr[6] = second;
    (void)call_set(rt, ctx, 0x08A32350u, {target, first, second, ctx.gpr[20]});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
}

// func_08A4BBC0(out, count, args): count <= 0 returns with v0 untouched. o = args[0] (null: func_08A5BB7C(D_08CAED38) first) with its virtual slot 0xDC called with (o, 0) gives p; args[0] again (null: func_08A5BB7C(D_08CAED38) first) with its virtual slot 0xD8 called with (o) (v0 unused). The unaligned word at p equal to 0xFFF7EEC5:
// s0 = -(the unaligned word at p + 4), c = func_08B35DA0(s0), func_08B26B74(sp + 4, c, p, 1), func_08B24DB4(sp + 4), func_08A44D9C(sp, c, s0, 1) (c is the result of func_08B26B74), the holder sp copied to `out` with func_08A5BAD8(out, sp[0]) unless it is `out`, func_08A5B9EC(sp) ends it;
// any other word copies the holder args (s1 = args) to `out` with func_08A5BAD8(out, args[0]) unless that is `out`.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x08A4BBE0), the null checks of the two reads of args[0] (0x08A4BBEC, 0x08A4BC18; D_08CAED38) and a tag word other than 0xFFF7EEC5 (0x08A4BC4C bne to 0x08A4BCBC: copy of args to `out`) never occurred; `out` equal to sp (0x08A4BC9C) never either. Every case took the 0xFFF7EEC5 path; the other path is read from the asm only.
void func_08A4BBC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x1850u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u);
    ctx.gpr[20] = out;
    ctx.gpr[17] = args;
    if (signed_of(count) <= 0) return;
    std::uint32_t object = word(rt, args);
    ctx.gpr[16] = object;
    if (object == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    }
    std::uint32_t vtable = word(rt, object);
    std::uint32_t slot = word(rt, vtable + 0xDCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;  // the delay slot of the jalr
    const std::uint32_t value = call_set(rt, ctx, slot, {object, 0u});
    object = word(rt, args);
    ctx.gpr[16] = object;
    ctx.gpr[19] = value;  // the delay slot of the bnez
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0xD8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    (void)call_set(rt, ctx, slot, {object});
    const auto unaligned = [&](std::uint32_t address) {
        return static_cast<std::uint32_t>(byte_u(rt, address) | (byte_u(rt, address + 1u) << 8) | (byte_u(rt, address + 2u) << 16) | (byte_u(rt, address + 3u) << 24));
    };
    const std::uint32_t tag = unaligned(value);
    ctx.gpr[3] = tag;
    ctx.gpr[2] = 0xFFF7EEC5u;
    ctx.gpr[18] = p4;  // the delay slot of the bne
    if (tag == 0xFFF7EEC5u) {
        const std::uint32_t negated = 0u - unaligned(value + 4u);
        ctx.gpr[16] = negated;
        const std::uint32_t block = call_set(rt, ctx, 0x08B35DA0u, {negated});
        ctx.gpr[17] = block;  // the delay slot of the next jal
        (void)call_set(rt, ctx, 0x08B26B74u, {p4, block, value, 1u});
        (void)call_set(rt, ctx, 0x08B24DB4u, {p4});
        (void)call_set(rt, ctx, 0x08A44D9Cu, {p0, block, negated, 1u});
        if (out != p0) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p0)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
        return;
    }
    if (args != out) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, args)});
}

// func_08A4412C(out, count, args): count <= 0 returns with v0 untouched. o = args[0] (null: func_08A5BB7C(D_08CADD5C) first) with its virtual slot 0xDC called with (o, 0) gives a; args[0] again (null: func_08A5BB7C(D_08CADD5C) first) with the virtual slot 0xD8 called with (o) gives b; b1 = func_08B2A3B8(b), b2 = func_08B2B5E0(b1);
// t0 = 0x3039, or (for a count of 2 or more) the virtual slot 0x3C of args[1] (null: func_08A5BB7C(D_08CADD5C) first) called with (args[1]); r = func_08B2A594(b2, b1, a, b, t0); zero returns 0. Otherwise func_08A44D9C(sp, b2, b1, 1), the holder sp copied to `out` with func_08A5BAD8(out, sp[0]) unless it is `out`, func_08A5B9EC(sp).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x08A4415C), the three null checks (0x08A44168, 0x08A44194, 0x08A441E4; D_08CADD5C), a count of 2 or more (0x08A441D8: the virtual slot 0x3C of args[1] gives t0, instead of the constant 0x3039; read from the asm only), a zero func_08B2A594 result (0x08A44220) and `out` equal to sp (0x08A4423C) never occurred.
void func_08A4412C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u);
    ctx.gpr[23] = out;
    ctx.gpr[21] = count;
    ctx.gpr[18] = args;
    if (signed_of(count) <= 0) return;
    std::uint32_t object = word(rt, args);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADD5Cu});
    std::uint32_t vtable = word(rt, object);
    std::uint32_t slot = word(rt, vtable + 0xDCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;  // the delay slot of the jalr
    const std::uint32_t first = call_set(rt, ctx, slot, {object, 0u});
    object = word(rt, args);
    ctx.gpr[16] = object;
    ctx.gpr[22] = first;  // the delay slot of the bnez
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADD5Cu});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0xD8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    const std::uint32_t second = call_set(rt, ctx, slot, {object});
    ctx.gpr[4] = second;
    ctx.gpr[19] = second;  // the delay slot of the next jal
    const std::uint32_t third = call_set(rt, ctx, 0x08B2A3B8u, {second});
    ctx.gpr[4] = third;
    ctx.gpr[17] = third;  // the delay slot of the next jal
    const std::uint32_t fourth = call_set(rt, ctx, 0x08B2B5E0u, {third});
    ctx.gpr[20] = fourth;
    ctx.gpr[2] = signed_of(count) < 2 ? 1u : 0u;
    ctx.gpr[8] = 0x3039u;  // the delay slot of the bnez
    if (signed_of(count) >= 2) {
        object = word(rt, args + 4u);
        ctx.gpr[16] = object;
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CADD5Cu});
        vtable = word(rt, object);
        slot = word(rt, vtable + 0x3Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = slot;
        ctx.gpr[8] = call_set(rt, ctx, slot, {object});
    }
    ctx.gpr[6] = first;
    ctx.gpr[7] = second;
    const std::uint32_t matched = call_set(rt, ctx, 0x08B2A594u, {fourth, third, first, second, ctx.gpr[8]});
    if (matched == 0u) return;
    (void)call_set(rt, ctx, 0x08A44D9Cu, {p0, fourth, third, 1u});
    if (out != p0) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p0)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
}

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

// func_08A3C544(out): func_08A3A9D0(sp + 0xC); u = sp[0xC] (null: func_08A5BB7C(D_08CAD544) first), s5 = u[0x14]; func_08A5B9EC(sp + 0xC); n = func_08ABCA48(s5); func_08A98C1C(sp + 0x10, n); for each i below n (n <= 0 skips the loop):
// d = the element sp[0x10][0x14] + 4 * i, func_08ABCA74(s5, i) is stored into the string at sp + 4 by func_08B85EB8(sp + 4, that, sp + 1), func_08A6D2D4(sp + 8, sp + 4), d gets the holder copied with func_08A5BAD8(d, sp[8]) unless d is sp + 8, func_08A5B9EC(sp + 8) and the string data
// sp[4] is released (func_08B60804(data - 4, -1), then func_08B8542C(data - 0xC, sp) when not positive; nothing when data - 0xC is D_08D74FD0). After the loop the holder sp + 0x10 is copied to `out` with func_08A5BAD8(out, sp[0x10]) unless it is `out`, and func_08A5B9EC(sp + 0x10) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the null check of u (0x08A3C584, D_08CAD544) never fired, n was never 0 or less (0x08A3C5C0 blez: the loop never skipped), the destination equal to sp + 8 (0x08A3C61C) and `out` equal to sp + 0x10 (0x08A3C670) never happened.
void func_08A3C544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p1 = frame.at(1u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u);
    put(rt, frame.at(0x20u), out);
    ctx.gpr[17] = pC;
    (void)call_set(rt, ctx, 0x08A3A9D0u, {pC});
    const std::uint32_t object = word(rt, pC);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAD544u});
    const std::uint32_t source = word(rt, object + 0x14u);
    ctx.gpr[21] = source;
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
    const std::uint32_t count = call_set(rt, ctx, 0x08ABCA48u, {source});
    ctx.gpr[22] = p10;
    ctx.gpr[19] = count;
    (void)call_set(rt, ctx, 0x08A98C1Cu, {p10, count});
    ctx.gpr[2] = word(rt, frame.at(0x20u));  // the delay slot of the blez
    if (signed_of(count) > 0) {
        ctx.gpr[30] = 0x08D74FD0u;
        ctx.gpr[18] = p8;
        ctx.gpr[17] = 0u;
        ctx.gpr[20] = p4;
        ctx.gpr[23] = p1;
        std::uint32_t index = 0u;
        do {
            const std::uint32_t vector = word(rt, p10);
            ctx.gpr[3] = vector;
            const std::uint32_t element = word(rt, vector + 0x14u);
            ctx.gpr[17] = index + 1u;
            ctx.gpr[16] = element + (index << 2);  // the delay slot of the jal below
            const std::uint32_t item = call_set(rt, ctx, 0x08ABCA74u, {source, index});
            (void)call_set(rt, ctx, 0x08B85EB8u, {p4, item, p1});
            (void)call_set(rt, ctx, 0x08A6D2D4u, {p8, p4});
            ctx.gpr[4] = element + (index << 2);
            if (element + (index << 2) != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {element + (index << 2), word(rt, p8)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
            release_cell(rt, ctx, word(rt, p4), p0, 16u);
            ++index;
        } while (count != index);
        ctx.gpr[3] = word(rt, p10);
        ctx.gpr[2] = word(rt, frame.at(0x20u));
    }
    const std::uint32_t destination = ctx.gpr[2];
    ctx.gpr[5] = word(rt, p10);  // the delay slot of the beq
    if (destination != p10) (void)call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, p10)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
}

// func_08A4A960(out): n = func_08B36278() (stored in s4 before the next call), a = func_08B36284(); func_08A98C1C(sp + 0xC, n); then for each i below n (n <= 0 skips the loop): func_08B85EB8(sp + 4, a[i], sp + 1) fills the string at sp + 4, func_08A6D2D4(sp + 8, sp + 4), the destination
// d = sp[0xC][0x14] + 4 * i gets the holder copied with func_08A5BAD8(d, sp[8]) unless d is sp + 8, func_08A5B9EC(sp + 8), and the string data sp[4] is released (func_08B60804(data - 4, -1), then func_08B8542C(data - 0xC, sp) when not positive; nothing when data - 0xC is D_08D74FD0).
// After the loop the holder sp + 0xC is copied to `out` with func_08A5BAD8(out, sp[0xC]) unless it is `out`, and func_08A5B9EC(sp + 0xC) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the loop never ran: n was always 0 or less (0x08A4A9B0 blez to 0x08A4AA5C); the whole loop body (0x08A4A9C0-0x08A4AA58: func_08B85EB8, func_08A6D2D4, the copy to d, the release of sp[4]) is read from the asm only, and `out` equal to sp + 0xC (0x08A4AA5C) never happened.
void func_08A4A960(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p1 = frame.at(1u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    put(rt, frame.at(0x10u), out);
    ctx.gpr[23] = pC;
    const std::uint32_t count = call_set(rt, ctx, 0x08B36278u, {});
    ctx.gpr[20] = count;  // the delay slot of the next jal
    const std::uint32_t items = call_set(rt, ctx, 0x08B36284u, {});
    ctx.gpr[16] = items;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A98C1Cu, {pC, count});
    ctx.gpr[2] = word(rt, frame.at(0x10u));  // the delay slot of the blez
    if (signed_of(count) > 0) {
        ctx.gpr[30] = 0x08D74FD0u;
        ctx.gpr[17] = items;
        ctx.gpr[19] = p8;
        ctx.gpr[18] = 0u;
        ctx.gpr[21] = p4;
        ctx.gpr[22] = p1;
        std::uint32_t index = 0u;
        do {
            const std::uint32_t vector = word(rt, pC);
            ctx.gpr[2] = vector;
            const std::uint32_t element = word(rt, vector + 0x14u);
            ctx.gpr[3] = element;
            const std::uint32_t item = word(rt, items + (index << 2));
            ctx.gpr[16] = (index << 2) + element;  // the delay slot of the jal below
            (void)call_set(rt, ctx, 0x08B85EB8u, {p4, item, p1});
            (void)call_set(rt, ctx, 0x08A6D2D4u, {p8, p4});
            const std::uint32_t destination = (index << 2) + element;
            ++index;
            ctx.gpr[18] = index;
            ctx.gpr[17] += 4u;
            ctx.gpr[4] = destination;
            if (destination != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, p8)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
            release_cell(rt, ctx, word(rt, p4), p0, 16u);
        } while (count != index);
        ctx.gpr[2] = word(rt, pC);
        ctx.gpr[2] = word(rt, frame.at(0x10u));
    }
    const std::uint32_t destination = ctx.gpr[2];
    ctx.gpr[5] = word(rt, pC);  // the delay slot of the beq
    if (destination != pC) (void)call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, pC)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
}

// func_08A4BA50(out, count, args): count <= 0 returns with v0 untouched. func_08B27FA8(sp + 4); sp[8] = 9, sp[0xC] = 3; o = args[0] (null: func_08A5BB7C(D_08CAED38) first) with its virtual slot 0xDC called with (o, 0) gives a; args[0] again (null: func_08A5BB7C(D_08CAED38) first) with its virtual slot 0xD8 called with (o) gives b;
// size = b + (b + 0xFFFE) / 0xFFFF * 8 + 0x20 (the division is the multiplication by 0x80008001, high word, shifted right 15); block = func_08B35DA0(size); r = func_08B28AA4(sp + 4, a, b, block, size) (the block is s2). A non-zero r: func_08A44D9C(sp, block, sp[0x98], 1), the holder sp copied to `out` with func_08A5BAD8(out, sp[0]) unless it is `out`, func_08A5B9EC(sp).
// Zero: func_08B35254(block), then `args` is copied to `out` with func_08A5BAD8(out, args[0]) unless it is `out`. func_08B27FEC(sp + 4) ends both.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x08A4BA78) and the two null checks of args[0] (0x08A4BA9C, 0x08A4BAC8; D_08CAED38) never occurred; func_08B28AA4 never returned 0 (0x08A4BB3C beqz not taken), so the path 0x08A4BB74-0x08A4BB8C (func_08B35254(block) and the copy of args to `out` unless it is `out`) is read from the asm only; `out` equal to sp (0x08A4BB54) never happened.
void func_08A4BA50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0xD0u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u);
    ctx.gpr[20] = out;
    ctx.gpr[19] = args;
    if (signed_of(count) <= 0) return;
    ctx.gpr[22] = p4;
    (void)call_set(rt, ctx, 0x08B27FA8u, {p4});
    std::uint32_t object = word(rt, args);
    ctx.gpr[16] = object;
    ctx.gpr[2] = 9u;
    ctx.gpr[3] = 3u;
    put(rt, frame.at(8u), 9u);
    put(rt, frame.at(0xCu), 3u);  // the delay slot of the bnez
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    std::uint32_t vtable = word(rt, object);
    std::uint32_t slot = word(rt, vtable + 0xDCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;  // the delay slot of the jalr
    const std::uint32_t first = call_set(rt, ctx, slot, {object, 0u});
    object = word(rt, args);
    ctx.gpr[16] = object;
    ctx.gpr[21] = first;  // the delay slot of the bnez
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAED38u});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0xD8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    ctx.gpr[16] = 0xFFFEu;  // the delay slot of the jalr
    const std::uint32_t second = call_set(rt, ctx, slot, {object});
    ctx.gpr[17] = second;
    const std::uint32_t biased = second + 0xFFFEu;
    const std::uint32_t quotient = static_cast<std::uint32_t>((static_cast<std::uint64_t>(biased) * 0x80008001u) >> 32) >> 15;
    const std::uint32_t size = second + (quotient << 3) + 0x20u;
    ctx.gpr[16] = size;
    ctx.gpr[2] = 0x80008001u;
    const std::uint32_t block = call_set(rt, ctx, 0x08B35DA0u, {size});
    ctx.gpr[7] = block;
    ctx.gpr[18] = block;  // the delay slot of the next jal
    const std::uint32_t built = call_set(rt, ctx, 0x08B28AA4u, {p4, first, second, block, size});
    ctx.gpr[6] = word(rt, frame.at(0x98u));  // the delay slot of the beqz
    if (built != 0u) {
        (void)call_set(rt, ctx, 0x08A44D9Cu, {p0, block, word(rt, frame.at(0x98u)), 1u});
        ctx.gpr[5] = word(rt, p0);  // the delay slot of the beq
        if (out != p0) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p0)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
    } else {
        (void)call_set(rt, ctx, 0x08B35254u, {block});
        if (args != out) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, args)});
    }
    (void)call_set(rt, ctx, 0x08B27FECu, {p4});
}

// func_08A10468(out, a1, mode, args): mode 0: func_08A0FD80(sp + 8, a1); h = sp[8] (null: func_08A5BB7C(D_08CA97A4) first); func_08A97A6C(sp + 0xC, bit 19 of h[0x60]); the holder sp + 0xC is copied to `out` with func_08A5BAD8(out, sp[0xC]) unless it is `out`; func_08A5B9EC(sp + 0xC), func_08A5B9EC(sp + 8).
// Mode 1: o = args[0] (null: func_08A5BB7C(D_08CA9714) first); a non-zero result of its virtual slot 0x30 gives func_08A0FD80(sp + 4, a1), h = sp[4] (null: func_08A5BB7C(D_08CA97A4) first), the virtual slot 0x1B4 of h[0] called with (h), func_08A5B9EC(sp + 4); a zero one gives func_08A0FD80(sp, a1), h = sp[0] (same null check),
// the virtual slot 0x1B0 called with (h), func_08A5B9EC(sp). Any other mode returns with v0 = 1.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): mode 0 (0x08A10480 bnez not taken: func_08A0FD80(sp + 8, a1), the bit 19 of h[0x60], func_08A97A6C, the copy of sp + 0xC) never ran, nor did any mode other than 0 or 1 (0x08A104EC, v0 = 1) or the three null checks (0x08A10498, 0x08A104F8, 0x08A10534 / 0x08A10574; D_08CA97A4, D_08CA9714). Mode 1 ran with both answers of the virtual slot 0x30. Read from the asm only: mode 0 and the `ext a1, a1, 19, 1` (bit 19 of the word at h + 0x60).
void func_08A10468(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], source = ctx.gpr[5], mode = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[18] = out;
    ctx.gpr[17] = source;
    if (mode == 0u) {
        ctx.gpr[17] = p8;
        (void)call_set(rt, ctx, 0x08A0FD80u, {p8});
        const std::uint32_t object = word(rt, p8);
        ctx.gpr[16] = object;
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA97A4u});
        const std::uint32_t flags = word(rt, object + 0x60u);
        ctx.gpr[5] = flags;
        ctx.gpr[16] = pC;
        ctx.gpr[5] = (flags >> 19) & 1u;  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A97A6Cu, {pC, (flags >> 19) & 1u});
        ctx.gpr[5] = word(rt, pC);  // the delay slot of the beq
        if (out != pC) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, pC)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    std::uint32_t object = word(rt, args);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA9714u});
    std::uint32_t vtable = word(rt, object);
    std::uint32_t slot = word(rt, vtable + 0x30u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    const std::uint32_t decided = call_set(rt, ctx, slot, {object});
    ctx.gpr[5] = source;  // the delay slot of the beqz
    const std::uint32_t holder = decided != 0u ? p4 : p0;
    if (decided != 0u) ctx.gpr[18] = p4;
    (void)call_set(rt, ctx, 0x08A0FD80u, {holder, source});
    object = word(rt, holder);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA97A4u});
    vtable = word(rt, object);
    slot = word(rt, vtable + (decided != 0u ? 0x1B4u : 0x1B0u));
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    (void)call_set(rt, ctx, slot, {object});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_089A44F0(out, a1, count, args): count <= 0 returns with v0 untouched. o = args[0] (null: func_08A5BB7C(D_08CA4BF4) first) with its virtual slot 0xC called with (sp + 0x10, o); func_089A354C(sp + 0xC, a1); u = sp[0xC] (null: func_08A5BB7C(D_08CA4D28) first);
// r = func_089A3348(u, sp + 0x10, sp + 1, sp + 0x14); func_08A5B9EC(sp + 0xC). r == 0: func_08A44960(sp + 8, D_08CF6ED8, sp[0x14]) and the holder sp + 8 is copied to `out` with func_08A5BAD8(out, sp[8]) unless it is `out`; otherwise func_08A44D9C(sp + 4, r, sp[0x14], the byte sp[1]) and the holder sp + 4 is copied the same way.
// func_08A5B9EC(that holder); the string data d = sp[0x10] is released when it is not D_08D74FD0 + 0xC (func_08B60804(d - 4, -1), then func_08B8542C(d - 0xC, sp) when that is not positive).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x089A4510), the null checks of o and u (0x089A451C, 0x089A455C; D_08CA4BF4, D_08CA4D28), a zero func_089A3348 result (0x089A4590 bnez falling into 0x089A4598-0x089A45BC: func_08A44960(sp + 8, D_08CF6ED8, sp[0x14]) and the copy of sp + 8), `out` equal to sp + 4 (0x089A45D8), a shared empty cell (0x089A4600 beq) and the call of func_08B8542C (0x089A4614 bgtz never taken) never occurred. Read from the asm only: the zero-result path and the constant D_08CF6ED8.
void func_089A44F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], source = ctx.gpr[5], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p14 = frame.at(0x14u);
    ctx.gpr[18] = out;
    ctx.gpr[17] = source;
    if (signed_of(count) <= 0) return;
    const std::uint32_t object = word(rt, args);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4BF4u});
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t slot = word(rt, vtable + 0xCu);
    ctx.gpr[19] = p10;
    ctx.gpr[3] = slot;
    ctx.gpr[2] = vtable;
    ctx.gpr[20] = pC;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {p10, object});
    (void)call_set(rt, ctx, 0x089A354Cu, {pC, source});
    const std::uint32_t unit = word(rt, pC);
    ctx.gpr[16] = unit;
    if (unit == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4D28u});
    }
    const std::uint32_t found = call_set(rt, ctx, 0x089A3348u, {unit, p10, frame.at(1u), p14});
    ctx.gpr[17] = found;
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
    ctx.gpr[6] = word(rt, p14);  // the delay slot of the bnez
    std::uint32_t holder = 0u;
    if (found == 0u) {
        holder = p8;
        ctx.gpr[16] = p8;
        (void)call_set(rt, ctx, 0x08A44960u, {p8, 0x08CF6ED8u, word(rt, p14)});
        ctx.gpr[5] = word(rt, p8);  // the delay slot of the beq
        if (out != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p8)});
    } else {
        holder = p4;
        ctx.gpr[7] = byte_u(rt, frame.at(1u));
        ctx.gpr[16] = p4;
        (void)call_set(rt, ctx, 0x08A44D9Cu, {p4, found, word(rt, p14), byte_u(rt, frame.at(1u))});
        ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
        if (out != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p4)});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
    const std::uint32_t text = word(rt, p10);
    ctx.gpr[4] = text;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[16] = text - 0xCu;
    if (text - 0xCu != 0x08D74FD0u) {
        ctx.gpr[4] = text - 4u;
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = text - 0xCu;
        if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {text - 0xCu, p0});
    }
}

// func_08B9A5C4(vector, position, value): insert *value before position in a vector of 4-byte elements {begin, end, capacity} (the original moves the words through $f0 and $f20 and restores $f20).
// Room left (end != capacity): end != 0 copies end[-1] to *end (through $f0), the new end is end + 4, f20 = *value is read, func_08B79BAC(end - n, position, n) shifts the words with n = (end - 4 - position) with the low two bits cleared, and *position = f20; v0 is the shift's result.
// Full: the size is (end - begin) >> 2; size 0x3FFFFFFF calls func_08B5ADDC(D_08CA0058) first; the new capacity is 1 for an empty vector and size * 2 otherwise (an overflow gives a 4 - byte request of -4 and 0 is kept as 0 with no allocation); block = func_08B35DA0(capacity * 4) when it is not 0;
// func_08B9A594(begin, position, block, sp + 2) returns e; a non-null e gets *e = *value (through $f0); func_08B9A594(position, end, e + 4, sp + 1) returns the new end; func_08B9A584(begin, end, sp) runs; a non-null begin is freed with func_08B35254(begin); the vector gets {block, new end, block + capacity * 4}; v0 = block + capacity * 4.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the room-left path (end != capacity, 0x08B9A5F4 beq not taken: 0x08B9A5F8-0x08B9A638) never ran: every case grew the vector. Read from the asm only: that path (end[-1] copied to *end through $f0, the end becomes end + 4, n = (end - 4 - position) with the low two bits cleared, f20 = *value, func_08B79BAC(end - n, position, n), *position = f20), including the null-end case (0x08B9A5FC beql). In the grow path size 0x3FFFFFFF (0x08B9A650, func_08B5ADDC(D_08CA0058)), the doubling overflow (0x08B9A674: bytes = -4), a zero doubled size (0x08B9A680) and a zero result of func_08B9A594 (0x08B9A6AC beql) never occurred. Constants on the unexercised paths: 0x3FFFFFFF, -4, the mask ~3.
void func_08B9A5C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    SavedFloat saved_float(ctx, 20u);
    ctx.gpr[18] = value;
    ctx.gpr[17] = position;
    std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t capacity = word(rt, vector + 8u);
    ctx.gpr[3] = end;
    ctx.gpr[2] = capacity;
    ctx.gpr[16] = vector;  // the delay slot of the beq
    if (end != capacity) {
        std::uint32_t cursor = end;
        if (end == 0u) {
            ctx.gpr[4] = end;  // the delay slot of the beql, only when taken
        } else {
            ctx.fpr[0] = float_at(rt, end - 4u);
            put_float(rt, end, ctx.fpr[0]);
            cursor = word(rt, vector + 4u);
            ctx.gpr[3] = cursor;
            ctx.gpr[4] = cursor;
        }
        const std::uint32_t last = ctx.gpr[4] - 4u;
        ctx.gpr[3] = cursor + 4u;
        put(rt, vector + 4u, cursor + 4u);
        std::uint32_t bytes = last - position;
        bytes &= ~3u;
        ctx.gpr[6] = bytes;
        ctx.fpr[20] = float_at(rt, value);
        ctx.gpr[4] = ctx.gpr[4] - bytes;  // the delay slot of the jal
        (void)call_keep(rt, ctx, 0x08B79BACu, {ctx.gpr[4], position, bytes});
        put_float(rt, position, ctx.fpr[20]);  // the delay slot of the j
        return;
    }
    const std::uint32_t begin = word(rt, vector);
    ctx.gpr[4] = begin;
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
    ctx.gpr[3] = size;
    ctx.gpr[2] = 0x3FFFFFFFu;
    if (size == 0x3FFFFFFFu) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CA0058u});
    std::uint32_t count = 1u;
    std::uint32_t block = 0u, bytes = 0u;
    ctx.gpr[5] = 1u;  // the delay slot of the beqz
    bool allocate = true;
    if (size != 0u) {
        count = size << 1;
        ctx.gpr[5] = count;
        ctx.gpr[2] = count < size ? 1u : 0u;
        if (count < size) {
            bytes = 0xFFFFFFFCu;
            ctx.gpr[20] = bytes;  // the delay slot of the bnez
        } else {
            ctx.gpr[19] = 0u;
            if (count == 0u) {
                ctx.gpr[20] = 0u;  // the delay slot of the beqz
                allocate = false;
            } else {
                bytes = count << 2;
                ctx.gpr[20] = bytes;
            }
        }
    } else {
        bytes = count << 2;
        ctx.gpr[20] = bytes;
    }
    if (allocate) {
        block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[4] = word(rt, vector);
        ctx.gpr[19] = block;
    }
    const std::uint32_t copied = call_set(rt, ctx, 0x08B9A594u, {word(rt, vector), position, block, frame.at(2u)});
    ctx.gpr[5] = word(rt, vector + 4u);  // the delay slot of the beql, only when not taken
    if (copied != 0u) {
        ctx.fpr[0] = float_at(rt, value);
        put_float(rt, copied, ctx.fpr[0]);
        ctx.gpr[5] = word(rt, vector + 4u);
    } else {
        ctx.gpr[5] = word(rt, vector + 4u);
    }
    const std::uint32_t tail = call_set(rt, ctx, 0x08B9A594u, {position, word(rt, vector + 4u), copied + 4u, frame.at(1u)});
    ctx.gpr[17] = tail;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08B9A584u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
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

// func_0894D484(a, index, vec1, vec2, steps): index >= a[8] returns 0. Otherwise r = func_0894CF40() (v1 is r too): r[1] == 0 returns 0 leaving r[2] as it is; otherwise r[2] = 0 (stored in the delay slot of the next jal); o = func_0894CFF8(a, index); func_08941510(o, -1, -1, 0) with f12 = vec1[0], f13 = -vec1[2], f14 = vec1[1], f15 = vec2[1];
// func_089559B8(o) with f12 = 0.0f; steps > 0 calls func_08940610(o) with f12 = (float)steps * 0x3E8E38E4 (as a float constant); func_08945EE8(o), func_08956540(o), func_089567C0(o), func_08956B20(o), func_0895900C(a, index); v0 = 1.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): steps > 0 (0x0894D528 blez not taken: f12 = (float)steps * 0x3E8E38E4 and func_08940610) never occurred, so that product and its constant are read from the asm only; index >= a[8] (0x0894D4BC beqz) and r[1] == 0 (0x0894D4D8 beqz) never occurred either.
void func_0894D484(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t list = ctx.gpr[4], index = ctx.gpr[5], vec1 = ctx.gpr[6], vec2 = ctx.gpr[7], steps = ctx.gpr[8];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = steps;
    ctx.gpr[20] = vec2;
    ctx.gpr[19] = list;
    ctx.gpr[18] = index;
    const std::uint32_t limit = word(rt, list + 8u);
    ctx.gpr[2] = signed_of(index) < signed_of(limit) ? 1u : 0u;
    ctx.gpr[17] = vec1;  // the delay slot of the beqz
    if (signed_of(index) >= signed_of(limit)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t entry = call_keep(rt, ctx, 0x0894CF40u, {});
    ctx.gpr[3] = entry;
    const std::uint32_t armed = word(rt, entry + 4u);
    ctx.gpr[2] = armed;
    ctx.gpr[5] = index;
    ctx.gpr[4] = list;
    if (armed == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, entry + 8u, 0u);  // the delay slot of the jal to func_0894CFF8: only when r[1] != 0
    const std::uint32_t object = call_set(rt, ctx, 0x0894CFF8u, {list, index});
    ctx.gpr[4] = object;
    ctx.fpr[13] = -float_at(rt, vec1 + 8u);
    ctx.fpr[14] = float_at(rt, vec1 + 4u);
    ctx.fpr[15] = float_at(rt, vec2 + 4u);
    ctx.fpr[12] = float_at(rt, vec1);
    ctx.gpr[6] = 0xFFFFFFFFu;
    ctx.gpr[7] = 0xFFFFFFFFu;
    ctx.gpr[8] = 0u;
    ctx.gpr[16] = object;  // the delay slot of the jal
    (void)call_keep(rt, ctx, 0x08941510u, {object, ctx.gpr[5], 0xFFFFFFFFu, 0xFFFFFFFFu, 0u}, {ctx.fpr[12], ctx.fpr[13], ctx.fpr[14], ctx.fpr[15]});
    ctx.fpr[12] = 0.0f;
    ctx.gpr[4] = object;  // the delay slot of the jal
    (void)call_keep(rt, ctx, 0x089559B8u, {object}, {0.0f});
    ctx.fpr[1] = std::bit_cast<float>(steps);
    ctx.gpr[4] = object;
    ctx.fpr[0] = static_cast<float>(signed_of(steps));  // the delay slot of the blez
    if (signed_of(steps) > 0) {
        ctx.fpr[12] = psp_mul(ctx.fpr[0], std::bit_cast<float>(0x3E8E38E4u));
        ctx.gpr[1] = 0x3E8E38E4u;
        (void)call_keep(rt, ctx, 0x08940610u, {object}, {ctx.fpr[12]});
    }
    (void)call_set(rt, ctx, 0x08945EE8u, {object});
    (void)call_set(rt, ctx, 0x08956540u, {object});
    (void)call_set(rt, ctx, 0x089567C0u, {object});
    (void)call_set(rt, ctx, 0x08956B20u, {object});
    (void)call_set(rt, ctx, 0x0895900Cu, {list, index});
    ctx.gpr[2] = 1u;
}

// func_08988BB8(this, parser): func_08B88F44(sp + 8, this + 4) copies a string into the holder at sp + 8, func_08B85EB8(sp + 4, D_08CA3960, sp + 3) fills the string at sp + 4; with a = sp[8] and b = sp[4] (string data, the lengths are the words at data - 0xC) func_08B36170(a, b, the smaller length) gives r, or the length difference when r is 0,
// the string b is released (func_08B60804(b - 4, -1), then func_08B8542C(b - 0xC, sp + 2) when that is not positive; nothing when b - 0xC is D_08D74FD0), then a (flag sp + 1). A non-zero answer returns (v0 as the last call left it). Otherwise sp[0x14] = D_08D74FD0 + 0xC, func_08987A74(parser, sp + 0x14) twice, and when the second result is 0x101:
// d = func_08B77BF4(sp[0x14]), then func_08B71EC8(d) leaves a float in f0 that is stored at the address this[0xC]. func_0898682C(parser, 1); the string data sp[0x14] is released (flag sp).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): a non-zero string comparison (0x08988C9C bnez to 0x08988D28: the order was always 0 and the movn at 0x08988C50 kept the length difference, which was 0 too), a shorter b than a (the minimum was always the stack word sp + 0xC), the shared empty cell for b or a (0x08988C4C, 0x08988C7C), a non-positive reference count for a (0x08988C8C bgtz was always taken, so func_08B8542C(a - 0xC, sp + 1) never ran) and a positive count for b (0x08988C60 bgtzl was never taken) never occurred; the float parse (the answer 0x101 at 0x08988CC8: func_08B77BF4, func_08B71EC8 and the store of f0 at this[0xC]) never ran either. Read from the asm only: that parse and its store.
void func_08988BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], parser = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t empty = 0x08D74FD0u;
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p14 = frame.at(0x14u);
    ctx.gpr[22] = self;
    ctx.gpr[21] = parser;
    (void)call_set(rt, ctx, 0x08B88F44u, {p8, self + 4u});
    (void)call_set(rt, ctx, 0x08B85EB8u, {p4, 0x08CA3960u, frame.at(3u)});
    std::uint32_t first = word(rt, p8);
    const std::uint32_t second = word(rt, p4);
    ctx.gpr[17] = first;
    ctx.gpr[16] = second;
    const std::uint32_t first_length = word(rt, first - 0xCu);
    ctx.gpr[7] = first_length;
    put(rt, pC, first_length);
    ctx.gpr[19] = second - 0xCu;
    const std::uint32_t second_length = word(rt, second - 0xCu);
    ctx.gpr[3] = second_length;
    ctx.gpr[6] = second_length < first_length ? 1u : 0u;
    put(rt, p10, second_length);
    const std::uint32_t shorter_at = second_length < first_length ? p10 : pC;
    ctx.gpr[8] = shorter_at;
    ctx.gpr[18] = first_length - second_length;
    const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {first, second, word(rt, shorter_at)});
    ctx.gpr[20] = empty;
    if (order != 0u) ctx.gpr[18] = order;  // the movn in the delay slot of the beq
    if (second - 0xCu != empty) {
        ctx.gpr[4] = second - 4u;
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {second - 4u, 0xFFFFFFFFu});
        if (signed_of(references) > 0) {
            first = word(rt, p8);  // the delay slot of the bgtzl, taken
            ctx.gpr[17] = first;
        } else {
            ctx.gpr[4] = second - 0xCu;
            (void)call_set(rt, ctx, 0x08B8542Cu, {second - 0xCu, frame.at(2u)});
            first = word(rt, p8);
            ctx.gpr[17] = first;
        }
    }
    ctx.gpr[16] = first - 0xCu;
    ctx.gpr[4] = first - 4u;  // the delay slot of the beq
    if (empty != first - 0xCu) {
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {first - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = first - 0xCu;  // the delay slot of the bgtz
        if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {first - 0xCu, frame.at(1u)});
    }
    ctx.gpr[16] = p14;  // the delay slot of the bnez
    if (ctx.gpr[18] != 0u) return;
    put(rt, p14, empty + 0xCu);  // the delay slot of the jal
    ctx.gpr[2] = empty + 0xCu;
    (void)call_set(rt, ctx, 0x08987A74u, {parser, p14});
    const std::uint32_t kind = call_set(rt, ctx, 0x08987A74u, {parser, p14});
    ctx.gpr[3] = 0x101u;
    ctx.gpr[4] = parser;  // the delay slot of the bne
    if (kind == 0x101u) {
        const std::uint32_t text = word(rt, p14);
        ctx.gpr[4] = text;
        ctx.gpr[16] = word(rt, self + 0xCu);  // the delay slot of the jal below
        const std::uint32_t value = call_set(rt, ctx, 0x08B77BF4u, {text});
        ctx.gpr[4] = value;
        const GuestResult narrowed = call_keep(rt, ctx, 0x08B71EC8u, {value, ctx.gpr[3]}, {});
        put_float(rt, ctx.gpr[16], narrowed.f0);
        ctx.gpr[4] = parser;
    }
    (void)call_set(rt, ctx, 0x0898682Cu, {parser, 1u});
    const std::uint32_t data = word(rt, p14);
    ctx.gpr[4] = data;
    ctx.gpr[16] = data - 0xCu;
    if (empty != data - 0xCu) {
        ctx.gpr[4] = data - 4u;
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[4] = data - 0xCu;
        if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, p0});
    }
}

// func_08A6CFC0(out, a1, count, items): count <= 0 returns with v0 untouched. Holders at sp + 4 (func_08A6C410) and sp + 8 (func_08A98A70); for each of the `count` words of `items` the object o of the holder sp + 8 (null: func_08A5BB7C(D_08CB0774) first) has the vector at o + 0x14 {begin, end, capacity} appended: begin == end calls func_08B8A03C(v, end, item), otherwise a
// non-null end gets func_08A76ABC(end, item) and the vector's end becomes end + 4 (a null end stores 4). Then p = the object of the holder sp + 4 (null: func_08A5BB7C(D_08CB0754) first) gives func_08AB37F0(sp, p, count, items); the holder sp is copied to `out` with func_08A5BAD8(out, sp[0]) unless it is `out`; func_08A5B9EC on sp, sp + 8 and sp + 4 ends it.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x08A6CFF4), the null check of o (0x08A6D028; D_08CB0774), a free slot in the vector (end != capacity, 0x08A6D048 not taken: func_08A76ABC and the end + 4 store, with the null-end case 0x08A6D054), the null check of the holder (0x08A6D08C; D_08CB0754) and `out` equal to sp (0x08A6D0B4) never occurred: every append found the vector full (func_08B8A03C). Read from the asm only: the free-slot path.
void func_08A6CFC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[6], items = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u);
    ctx.gpr[22] = out;
    ctx.gpr[21] = items;
    ctx.gpr[19] = count;
    if (signed_of(count) <= 0) return;
    ctx.gpr[30] = p4;
    ctx.gpr[23] = p8;  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08A6C410u, {p4});
    (void)call_set(rt, ctx, 0x08A98A70u, {p8});
    ctx.gpr[17] = items;
    ctx.gpr[18] = 0u;
    ctx.gpr[20] = high_half(0x08CB0774u);
    std::uint32_t index = 0u;
    do {
        const std::uint32_t object = word(rt, p8);
        ctx.gpr[2] = object;
        ctx.gpr[4] = 0x08CB0774u;
        ctx.gpr[16] = object + 0x14u;  // the delay slot of the bnez
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0774u});
        const std::uint32_t vector = ctx.gpr[16];
        std::uint32_t end = word(rt, vector + 4u);
        const std::uint32_t capacity = word(rt, vector + 8u);
        ctx.gpr[3] = end;
        ctx.gpr[2] = capacity;
        ctx.gpr[4] = vector;
        ctx.gpr[5] = end;
        ctx.gpr[6] = ctx.gpr[17];  // the delay slot of the beq
        if (end == capacity) {
            (void)call_set(rt, ctx, 0x08B8A03Cu, {vector, end, ctx.gpr[17]});
        } else {
            ctx.gpr[4] = end;
            ctx.gpr[5] = ctx.gpr[17];  // the delay slot of the beqz
            if (end != 0u) {
                (void)call_set(rt, ctx, 0x08A76ABCu, {end, ctx.gpr[17]});
                end = word(rt, vector + 4u);
                ctx.gpr[3] = end;
            }
            ctx.gpr[3] = end + 4u;
            put(rt, vector + 4u, end + 4u);
        }
        ++index;
        ctx.gpr[18] = index;
        ctx.gpr[17] += 4u;  // the delay slot of the bne
    } while (count != index);
    const std::uint32_t holder = word(rt, p4);
    ctx.gpr[16] = holder;
    ctx.gpr[5] = holder;
    if (holder == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0754u});
        ctx.gpr[5] = holder;
    }
    (void)call_set(rt, ctx, 0x08AB37F0u, {p0, holder, count, items});
    ctx.gpr[5] = word(rt, p0);  // the delay slot of the beq
    if (out != p0) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p0)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
}

// func_08C22B54(this, a): s2 = D_08D74FD0, sp[0x10] = s2 + 0xC; for i from 0 while i < this[8] (the loop runs at least once only when this[8] > 0): c = func_08A6158C(a), o = c[0]; a non-null o gives its virtual slot 0xC called with (sp + 8, o), func_08B89124(sp + 0xC, sp + 8, sp + 0x10), func_08B855E0(sp + 0x10, sp + 0xC), and the string data
// sp[0xC] is released (func_08B60804(data - 4, -1), func_08B8542C(data - 0xC, sp + 2) when not positive; nothing when data - 0xC is D_08D74FD0), then sp[8] the same way (flag sp + 1). After the loop func_08A6D2D4(sp + 4, sp + 0x10), func_08A61584(a, sp + 4), func_08A5B9EC(sp + 4) and the data sp[0x10] is released (flag sp).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the object c[0] of the cell was never 0 (0x08C22BC0 beqz never taken): the body ran for every element.
void func_08C22B54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t empty = 0x08D74FD0u;
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u);
    ctx.gpr[3] = high_half(empty);
    ctx.gpr[21] = p10;
    ctx.gpr[20] = self;
    ctx.gpr[19] = argument;
    ctx.gpr[18] = empty;
    const std::uint32_t limit = word(rt, self + 8u);
    ctx.gpr[2] = limit;
    ctx.gpr[4] = empty + 0xCu;
    put(rt, p10, empty + 0xCu);  // the delay slot of the blez
    if (signed_of(limit) > 0) {
        ctx.gpr[17] = 0u;
        ctx.gpr[22] = p8;
        ctx.gpr[23] = pC;
        std::uint32_t index = 0u;
        do {
            const std::uint32_t cell = call_set(rt, ctx, 0x08A6158Cu, {argument});
            const std::uint32_t object = word(rt, cell);
            ctx.gpr[2] = object;
            ++index;
            ctx.gpr[17] = index;
            ctx.gpr[4] = p8;
            ctx.gpr[5] = object;  // the delay slot of the beqz
            if (object != 0u) {
                const std::uint32_t vtable = word(rt, object);
                const std::uint32_t slot = word(rt, vtable + 0xCu);
                ctx.gpr[2] = vtable;
                ctx.gpr[3] = slot;
                (void)call_set(rt, ctx, slot, {p8, object});
                (void)call_set(rt, ctx, 0x08B89124u, {pC, p8, p10});
                (void)call_set(rt, ctx, 0x08B855E0u, {p10, pC});
                ctx.gpr[3] = word(rt, pC);
                release_cell(rt, ctx, word(rt, pC), frame.at(2u), 16u);
                ctx.gpr[2] = word(rt, p8);
                release_cell(rt, ctx, word(rt, p8), frame.at(1u), 16u);
            }
            ctx.gpr[2] = word(rt, self + 8u);
            ctx.gpr[2] = signed_of(index) < signed_of(word(rt, self + 8u)) ? 1u : 0u;
        } while (ctx.gpr[2] != 0u);
    }
    ctx.gpr[16] = p4;
    (void)call_set(rt, ctx, 0x08A6D2D4u, {p4, p10});
    (void)call_set(rt, ctx, 0x08A61584u, {argument, p4});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    release_cell(rt, ctx, word(rt, p10), p0, 16u);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// vector_insert_grow(message, construct, copy, destroy): the reallocating half of two insertions into a vector of 4-byte elements {begin, end, capacity} (func_08C1ABE0, func_08C25730; each row holds the four values of its own function: the argument of the call of func_08B5ADDC, the element construct, the
// range copy and the range destroy). With the entry registers s0 = vector, s5 = position, s2 = value and a0 = end: size = (end - begin) >> 2; size 0x3FFFFFFF calls func_08B5ADDC(message) first; the new size is 1 for an empty vector and size * 2 otherwise, in bytes 4 * that (-4 when the doubling overflowed);
// block = func_08A5BC20(bytes, 4); e = copy(begin, position, block, sp + 2); a non-null e gets construct(e, value); n = copy(position, end, e + 4, sp + 1); destroy(begin, end, sp); a non-null begin is freed with func_08A5BC54(begin, ((capacity - begin) >> 2) << 2, 4); the vector becomes {block, n, block + bytes}; v0 = block + bytes.
inline void vector_insert_grow(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, std::uint32_t message, std::uint32_t construct, std::uint32_t copy, std::uint32_t destroy) {
    const std::uint32_t vector = ctx.gpr[16], position = ctx.gpr[21], value = ctx.gpr[18], end = ctx.gpr[4];
    std::uint32_t begin = word(rt, vector);
    ctx.gpr[2] = begin;
    ctx.gpr[3] = 0x3FFFFFFFu;
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
    ctx.gpr[2] = size;
    ctx.gpr[4] = high_half(message);  // the delay slot of the bne
    if (size == 0x3FFFFFFFu) (void)call_set(rt, ctx, 0x08B5ADDCu, {message});
    std::uint32_t doubled = 1u;
    ctx.gpr[3] = 1u;  // the delay slot of the beqz
    std::uint32_t bytes = 0u;
    if (size == 0u) {
        bytes = doubled << 2;
    } else {
        doubled = size << 1;
        ctx.gpr[3] = doubled;
        ctx.gpr[2] = doubled < size ? 1u : 0u;
        bytes = doubled < size ? 0xFFFFFFFCu : doubled << 2;
    }
    ctx.gpr[20] = bytes;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    ctx.gpr[19] = block;  // the delay slot of the next jal
    const std::uint32_t first = call_set(rt, ctx, copy, {word(rt, vector), position, block, frame.at(2u)});
    ctx.gpr[17] = first;  // the delay slot of the beqz
    if (first != 0u) (void)call_set(rt, ctx, construct, {first, value});
    const std::uint32_t last = call_set(rt, ctx, copy, {position, word(rt, vector + 4u), first + 4u, frame.at(1u)});
    ctx.gpr[17] = last;  // the delay slot of the next jal
    (void)call_set(rt, ctx, destroy, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    begin = word(rt, vector);
    const std::uint32_t capacity = word(rt, vector + 8u);
    ctx.gpr[4] = begin;
    ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(capacity - begin) >> 2);
    if (begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {begin, static_cast<std::uint32_t>(signed_of(capacity - begin) >> 2) << 2, 4u});
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, last);
    put(rt, vector, block);
}

// func_08C1ABE0(vector, position, value) and func_08C25730: insert `value` before `position` into a vector of 4-byte elements; the two functions differ in their callees (the row of each is below) and in how the elements are moved: func_08C1ABE0 assigns the word at the source with func_08A5BAD8(dest, *source) and
// func_08C25730 passes both addresses to func_08A6A23C(dest, source). Without a free slot (end == capacity) vector_insert_grow runs. Otherwise a non-null end constructs end[0] from end[-1] (construct(end, end - 4)), the end becomes end + 4, a temporary at sp + 4 is constructed from the value, n = (end' - 8 - position) >> 2 elements are moved up one slot starting from the last
// (dest = end' - 8 - 4k, source = dest - 4 for k from 0 below n; nothing when n <= 0), the temporary goes to the position (func_08A5BAD8(position, sp[4]) unless position is the temporary, resp. func_08A6A23C(position, sp + 4)) and the temporary is destroyed (func_08A5B9EC resp. func_08A69D68(sp + 4)).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the room-left path (end != capacity, 0x08C1AC10 not taken: 0x08C1AC18-0x08C1ACA8) never ran: every case grew the vector, with a non-null copy result e (0x08C1AD10 beqz never taken). Read from the asm only: the room-left path (the construct from end[-1], the temporary at sp + 4, the backward move with func_08A5BAD8, the self test of 0x08C1AC6C, the final assignment and destruction), and in the grow path size 0x3FFFFFFF (0x08C1ACC0, D_08CACBB0) and the overflow of the doubling (0x08C1ACE0).
void func_08C1ABE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = position;
    ctx.gpr[16] = vector;
    ctx.gpr[18] = value;  // the delay slot of the beq
    std::uint32_t end = word(rt, vector + 4u);
    ctx.gpr[4] = end;
    if (end == word(rt, vector + 8u)) {
        vector_insert_grow(rt, ctx, frame, 0x08CACBB0u, 0x08A40C24u, 0x08C1AB7Cu, 0x08C1AB38u);
        return;
    }
    ctx.gpr[5] = end - 4u;  // the delay slot of the beqz
    if (end != 0u) {
        (void)call_set(rt, ctx, 0x08A40C24u, {end, end - 4u});
        end = word(rt, vector + 4u);
    }
    ctx.gpr[4] = end + 4u;
    ctx.gpr[20] = frame.at(4u);
    put(rt, vector + 4u, end + 4u);
    (void)call_set(rt, ctx, 0x08A40C24u, {frame.at(4u), value});
    const std::uint32_t top = word(rt, vector + 4u);
    ctx.gpr[3] = top;
    ctx.gpr[17] = top - 8u;
    const std::uint32_t moves = static_cast<std::uint32_t>(signed_of(top - 8u - position) >> 2);
    ctx.gpr[19] = moves;
    ctx.gpr[4] = top - 4u;  // the delay slot of the blez
    if (signed_of(moves) > 0) {
        ctx.gpr[16] = top - 4u;
        ctx.gpr[18] = 0u;
        ctx.gpr[16] -= 4u;
        for (;;) {
            ctx.gpr[17] -= 4u;
            if (ctx.gpr[16] == ctx.gpr[17]) {
                ctx.gpr[18] += 1u;  // the delay slot of the beql, only when taken
            } else {
                const std::uint32_t source = word(rt, ctx.gpr[17]);
                ctx.gpr[5] = source;
                (void)call_set(rt, ctx, 0x08A5BAD8u, {ctx.gpr[16], source});
                ctx.gpr[18] += 1u;
            }
            const bool again = ctx.gpr[19] != ctx.gpr[18];
            ctx.gpr[16] -= 4u;  // the delay slot of the bne
            if (!again) break;
        }
    }
    ctx.gpr[5] = word(rt, frame.at(4u));  // the delay slot of the beq
    if (position != frame.at(4u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {position, word(rt, frame.at(4u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}

// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the room-left path (end != capacity, 0x08C25760 not taken: 0x08C25768-0x08C257EC) never ran: every case grew the vector. Read from the asm only: that path (the construct from end[-1], the temporary at sp + 4, the backward move with func_08A6A23C(dest, source), the final func_08A6A23C(position, sp + 4) and func_08A69D68), and in the grow path size 0x3FFFFFFF (0x08C25804, D_08CB0DEC) and the overflow of the doubling (0x08C25824).
void func_08C25730(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    ctx.gpr[21] = position;
    ctx.gpr[16] = vector;
    ctx.gpr[18] = value;  // the delay slot of the beq
    std::uint32_t end = word(rt, vector + 4u);
    ctx.gpr[4] = end;
    if (end == word(rt, vector + 8u)) {
        vector_insert_grow(rt, ctx, frame, 0x08CB0DECu, 0x08A6972Cu, 0x08C25298u, 0x08C24F98u);
        return;
    }
    ctx.gpr[5] = end - 4u;  // the delay slot of the beqz
    if (end != 0u) {
        (void)call_set(rt, ctx, 0x08A6972Cu, {end, end - 4u});
        end = word(rt, vector + 4u);
    }
    ctx.gpr[4] = end + 4u;
    ctx.gpr[20] = frame.at(4u);
    put(rt, vector + 4u, end + 4u);
    (void)call_set(rt, ctx, 0x08A6972Cu, {frame.at(4u), value});
    const std::uint32_t top = word(rt, vector + 4u);
    ctx.gpr[3] = top;
    ctx.gpr[17] = top - 8u;
    const std::uint32_t moves = static_cast<std::uint32_t>(signed_of(top - 8u - position) >> 2);
    ctx.gpr[19] = moves;
    ctx.gpr[4] = top - 4u;  // the delay slot of the blez
    if (signed_of(moves) > 0) {
        ctx.gpr[16] = top - 4u;
        ctx.gpr[18] = 0u;
        ctx.gpr[16] -= 4u;
        for (;;) {
            ctx.gpr[17] -= 4u;
            ctx.gpr[18] += 1u;
            (void)call_set(rt, ctx, 0x08A6A23Cu, {ctx.gpr[16], ctx.gpr[17]});
            const bool again = ctx.gpr[19] != ctx.gpr[18];
            ctx.gpr[16] -= 4u;  // the delay slot of the bne
            if (!again) break;
        }
    }
    (void)call_set(rt, ctx, 0x08A6A23Cu, {position, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(4u)});
}

// func_08AA6B80(this): n = func_08AA65E4(this) (0 returns 0); with the vector v = this + 0x1C {begin, end, ...} and its size s = (end - begin) >> 2: n < s calls func_08C311E0(v, begin + 4 * n, end), otherwise func_08C30F28(v, end, n - s, sp + 4) with the cell pointer sp[4] = D_08D74FD0 + 0xC set first; the string data sp[4] is released (func_08B60804(data - 4, -1), func_08B8542C(data - 0xC, sp + 1) when
// not positive; nothing for the shared empty cell). Then for each i below n: s0 = v[0] (read again), func_08AA6908(sp + 8, this), func_08B855E0(s0 + 4 * i, sp + 8), and the string data sp[8] is released (flag sp).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): n == 0 (0x08AA6BA8 beqz) and n < size (0x08AA6BD4 beqz not taken: func_08C311E0(v, begin + 4 * n, end)) never occurred: every case appended with func_08C30F28. Read from the asm only: the n < size path.
void func_08AA6B80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t empty = 0x08D74FD0u;
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), vector = self + 0x1Cu;
    ctx.gpr[18] = self;
    const std::uint32_t count = call_set(rt, ctx, 0x08AA65E4u, {self});
    ctx.gpr[19] = count;  // the delay slot of the beqz
    if (count == 0u) return;
    ctx.gpr[4] = vector;
    const std::uint32_t begin = word(rt, vector), end = word(rt, vector + 4u);
    ctx.gpr[7] = begin;
    ctx.gpr[21] = empty;
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
    ctx.gpr[6] = size;
    ctx.gpr[2] = empty + 0xCu;
    ctx.gpr[3] = count < size ? 1u : 0u;
    put(rt, p4, empty + 0xCu);  // the delay slot of the beqz
    if (count < size) {
        ctx.gpr[5] = count << 2;
        ctx.gpr[5] = begin + (count << 2);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08C311E0u, {vector, begin + (count << 2), word(rt, self + 0x20u)});
    } else {
        (void)call_set(rt, ctx, 0x08C30F28u, {vector, word(rt, self + 0x20u), count - size, p4});
    }
    ctx.gpr[17] = 0u;  // the delay slots of the beq and the bgtz
    release_cell(rt, ctx, word(rt, p4), frame.at(1u), 16u);
    ctx.gpr[20] = p8;
    std::uint32_t array = word(rt, vector);
    ctx.gpr[16] = array;
    (void)call_set(rt, ctx, 0x08AA6908u, {p8, self});
    std::uint32_t index = 0u;
    for (;;) {
        (void)call_set(rt, ctx, 0x08B855E0u, {array + (index << 2), p8});
        ++index;
        ctx.gpr[17] = index;
        release_cell(rt, ctx, word(rt, p8), p0, 16u);
        if (count == index) break;
        array = word(rt, vector);  // the delay slot of the bnel, only when taken
        ctx.gpr[16] = array;
        (void)call_set(rt, ctx, 0x08AA6908u, {p8, self});
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// vector_assign<Shift>(copy, destroy, tail): func_08B8AD88 (2-byte elements, Shift 1) and func_08BFBB38 (4-byte elements, Shift 2), the same function twice: this = other for vectors {begin, end, capacity} (this == other returns this). With the other vector's range [ob, oe), bytes = oe - ob, n = bytes >> Shift and nb = n << Shift:
//   - capacity (in elements) below n: block = func_08A5BC20(nb, 4); copy(ob, oe, block, sp); destroy(begin, end, sp + 3); a non-null begin is freed with func_08A5BC54(begin, ((capacity - begin) >> Shift) << Shift, 4); the vector becomes {block, ., block + nb};
//   - size (elements) at least n: func_08B79BAC(begin, ob, bytes) moves the bytes, then tail(begin + bytes, end, sp + 2);
//   - otherwise func_08B79BAC(begin, ob, size << Shift), then copy(ob + ((end - begin) with its low Shift bits cleared), oe, end, sp + 1).
// The end becomes begin' + nb (begin' read again); v0 = this. The shift is an instruction field (`sra` / `sll` / `ins`), so it is a template argument; the three callees are the wrapper's arguments.
template <unsigned Shift>
inline void vector_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t copy, std::uint32_t destroy, std::uint32_t tail) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = other;
    ctx.gpr[16] = self;
    if (other == self) {
        ctx.gpr[2] = self;
        return;
    }
    const std::uint32_t begin = word(rt, self), capacity_end = word(rt, self + 8u);
    const std::uint32_t other_begin = word(rt, other), other_end = word(rt, other + 4u);
    ctx.gpr[18] = begin;
    ctx.gpr[19] = other_begin;
    ctx.gpr[22] = other_end;
    const std::uint32_t capacity = static_cast<std::uint32_t>(signed_of(capacity_end - begin) >> Shift);
    const std::uint32_t bytes = other_end - other_begin;
    ctx.gpr[20] = bytes;
    const std::uint32_t count = static_cast<std::uint32_t>(signed_of(bytes) >> Shift);
    ctx.gpr[21] = count;
    const std::uint32_t count_bytes = count << Shift;
    ctx.gpr[23] = count_bytes;
    ctx.gpr[2] = capacity < count ? 1u : 0u;
    ctx.gpr[5] = 4u;
    ctx.gpr[4] = count_bytes;  // the delay slot of the beqz
    if (capacity < count) {
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {count_bytes, 4u});
        ctx.gpr[17] = block;  // the delay slot of the next jal
        (void)call_set(rt, ctx, copy, {other_begin, other_end, block, frame.at(0u)});
        (void)call_set(rt, ctx, destroy, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
        const std::uint32_t old_begin = word(rt, self);
        const std::uint32_t old_bytes = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - old_begin) >> Shift) << Shift;
        ctx.gpr[2] = old_begin;
        ctx.gpr[5] = old_bytes;
        ctx.gpr[6] = 4u;
        ctx.gpr[4] = old_begin;
        if (old_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_begin, old_bytes, 4u});
        ctx.gpr[2] = block + count_bytes;
        ctx.gpr[4] = count_bytes;
        put(rt, self + 8u, block + count_bytes);
        put(rt, self, block);  // the delay slot of the j
    } else {
        const std::uint32_t end = word(rt, self + 4u);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> Shift);
        ctx.gpr[2] = size;
        ctx.gpr[3] = size < count ? 1u : 0u;
        ctx.gpr[6] = bytes;  // the delay slot of the bnez
        if (size >= count) {
            (void)call_set(rt, ctx, 0x08B79BACu, {begin, other_begin, bytes});
            (void)call_set(rt, ctx, tail, {begin + bytes, word(rt, self + 4u), frame.at(2u)});
        } else {
            (void)call_set(rt, ctx, 0x08B79BACu, {begin, other_begin, size << Shift});
            const std::uint32_t this_end = word(rt, self + 4u), this_begin = word(rt, self);
            const std::uint32_t head = word(rt, other), other_stop = word(rt, other + 4u);
            const std::uint32_t done = (this_end - this_begin) & ~((1u << Shift) - 1u);
            (void)call_set(rt, ctx, copy, {head + done, other_stop, this_end, frame.at(1u)});
        }
        ctx.gpr[4] = count << Shift;
    }
    const std::uint32_t front = word(rt, self);
    ctx.gpr[2] = ctx.gpr[4] + front;
    put(rt, self + 4u, ctx.gpr[4] + front);
    ctx.gpr[2] = self;
}
void func_08B8AD88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_assign<1u>(rt, ctx, 0x08B8A654u, 0x08B8A644u, 0x08B8A7C0u); }
void func_08BFBB38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_assign<2u>(rt, ctx, 0x08BF8DE8u, 0x08BF8B48u, 0x08BF91D8u); }

// func_08C287AC(this, key): a walk of the tree whose end node is this + 4 and root this[8] (nodes: [8] left, [0xC] right, [0x10] a holder); sp[0x10] = this + 4 is the candidate returned in v0. For each node n from the root: func_08A76AA0 initialises the holders sp, sp + 4 and sp + 8; the holder n + 0x10 is copied into sp + 4 with func_08A5BAD8(sp + 4, n[0x10])
// (unless that is sp + 4) and key into sp + 8 (func_08A5BAD8(sp + 8, key[0]) unless key is sp + 8); o = this[0][0x2C] (null: func_08A5BB7C(D_08CB15A0) first), the virtual slot 0xCC of o[0] is called with (o, sp, 2, sp + 4); h = sp[0] (null: func_08A5BB7C(D_08CB15A0) first) and the virtual slot 0x30 of h[0] with (h) gives r;
// func_08A5B9EC on sp + 8, sp + 4 and sp. A non-zero r continues with the right child n[0xC]; zero stores n as the candidate (sp[0x10] = n) and continues with the left child n[8]. The walk ends at a null child.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the node holder being sp + 4 (0x08C28824 beq), the key being sp + 8 (0x08C28838 beql taken) and the three null checks (0x08C28850, 0x08C28884; D_08CB15A0) never occurred.
void func_08C287AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u);
    ctx.gpr[2] = self + 4u;
    ctx.gpr[22] = key;
    ctx.gpr[21] = self;
    std::uint32_t node = word(rt, self + 8u);
    ctx.gpr[18] = node;
    put(rt, p10, self + 4u);  // the delay slot of the beqz
    if (node != 0u) {
        ctx.gpr[20] = p4;
        ctx.gpr[23] = pC;
        ctx.gpr[30] = p8;
        for (;;) {
            ctx.gpr[4] = p0;
            ctx.gpr[19] = word(rt, self);
            ctx.gpr[17] = node + 0x10u;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08A76AA0u, {p0});
            ctx.gpr[16] = p4 + 4u;
            (void)call_set(rt, ctx, 0x08A76AA0u, {p4});
            ctx.gpr[16] = p8 + 4u;
            (void)call_set(rt, ctx, 0x08A76AA0u, {p8});
            ctx.gpr[4] = p8 + 4u;
            if (p4 != node + 0x10u) (void)call_set(rt, ctx, 0x08A5BAD8u, {p4, word(rt, node + 0x10u)});
            std::uint32_t holder = 0u;
            if (p8 == key) {
                holder = word(rt, ctx.gpr[19] + 0x2Cu);  // the delay slot of the beql, only when taken
            } else {
                (void)call_set(rt, ctx, 0x08A5BAD8u, {p8, word(rt, key)});
                holder = word(rt, ctx.gpr[19] + 0x2Cu);
            }
            ctx.gpr[16] = holder;
            if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
            std::uint32_t vtable = word(rt, holder);
            std::uint32_t slot = word(rt, vtable + 0xCCu);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = slot;
            ctx.gpr[7] = p4;  // the delay slot of the jalr
            (void)call_set(rt, ctx, slot, {holder, p0, 2u, p4});
            const std::uint32_t result_holder = word(rt, p0);
            ctx.gpr[16] = result_holder;
            if (result_holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB15A0u});
            vtable = word(rt, result_holder);
            slot = word(rt, vtable + 0x30u);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = slot;
            ctx.gpr[16] = pC;  // the delay slot of the jalr
            const std::uint32_t verdict = call_set(rt, ctx, slot, {result_holder});
            ctx.gpr[17] = verdict;
            ctx.gpr[16] = pC - 4u;
            (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
            ctx.gpr[16] = p4;  // the delay slot of the bnel, taken
            (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
            std::uint32_t next = 0u;
            if (verdict != 0u) {
                next = word(rt, node + 0xCu);
            } else {
                next = word(rt, node + 8u);
                put(rt, p10, node);
            }
            ctx.gpr[2] = next;
            ctx.gpr[18] = next;  // the delay slot of the bnez
            node = next;
            if (node == 0u) break;
        }
    }
    ctx.gpr[2] = word(rt, p10);
}

// func_0880602C(out, a1, count, args): count <= 0 returns with v0 untouched. o = args[0] (null: func_08A5BB7C(D_08C97DA8) first); the virtual slot 0x50 of o[0] is called with (o) and returns the pair (v0, v1) for func_08930204(v0, v1) = k; a negative k returns (v0 = k) and so does k >= 0x5A (v0 = 0).
// Otherwise func_08A57854(sp + 0xC, a1); u = sp[0xC] (null: func_08A5BB7C(D_08C97DD8) first), f = the word at u[0][0x9C]; func_08A69B3C(sp, D_08C97DE4); b = func_08A5BC20(0x18, 4); func_08A76ACC(b); b[0] = D_08CB6B88; func_08A6972C(b + 0x14, sp); the word b is stored at sp + 4 and func_08A76AB4(sp + 8, sp + 4) fills the holder sp + 8; func_08A69D68(sp);
// f(sp + 0x10, u, sp + 8) is called; h = sp[0x10] (null: func_08A5BB7C(D_08C97DA8) first) with its virtual slot 0x84 called as (sp + 0x14, h, k); the holder sp + 0x14 is copied to `out` with func_08A5BAD8(out, sp[0x14]) unless it is `out`; func_08A5B9EC on sp + 0x14, sp + 0x10, sp + 8 and sp + 0xC end it (v0 is the last one's).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x08806058), the null checks (0x08806064, 0x088060BC, 0x0880614C; D_08C97DA8, D_08C97DD8), a negative k (0x08806094 bltz), k >= 0x5A (0x088060A0 beqz) and `out` equal to sp + 0x14 (0x0880617C) never occurred.
void func_0880602C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], source = ctx.gpr[5], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p14 = frame.at(0x14u);
    ctx.gpr[21] = out;
    ctx.gpr[17] = source;
    if (signed_of(count) <= 0) return;
    std::uint32_t object = word(rt, args);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97DA8u});
    std::uint32_t vtable = word(rt, object);
    std::uint32_t slot = word(rt, vtable + 0x50u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;  // the delay slot of the jalr
    const GuestResult pair = call_keep(rt, ctx, slot, {object}, {});
    ctx.gpr[4] = pair.v0;
    ctx.gpr[5] = pair.v1;
    const std::uint32_t kind = call_set(rt, ctx, 0x08930204u, {pair.v0, pair.v1});
    ctx.gpr[18] = kind;  // the delay slot of the bltz
    if (signed_of(kind) < 0) return;
    ctx.gpr[2] = signed_of(kind) < 0x5A ? 1u : 0u;
    if (signed_of(kind) >= 0x5A) return;
    ctx.gpr[23] = pC;
    (void)call_set(rt, ctx, 0x08A57854u, {pC, source});
    const std::uint32_t unit = word(rt, pC);
    ctx.gpr[19] = unit;
    if (unit == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97DD8u});
    vtable = word(rt, unit);
    ctx.gpr[2] = vtable;
    ctx.gpr[5] = 0x08C97DE4u;
    ctx.gpr[17] = word(rt, vtable + 0x9Cu);  // the delay slot of the jal
    const std::uint32_t function = ctx.gpr[17];
    (void)call_set(rt, ctx, 0x08A69B3Cu, {p0, 0x08C97DE4u});
    ctx.gpr[5] = 4u;
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    ctx.gpr[4] = block;
    ctx.gpr[16] = block;  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A76ACCu, {block});
    ctx.gpr[2] = 0x08CB6B88u;
    put(rt, block, 0x08CB6B88u);
    (void)call_set(rt, ctx, 0x08A6972Cu, {block + 0x14u, p0});
    ctx.gpr[20] = p8;  // the delay slot of the jal above
    put(rt, p4, block);
    ctx.gpr[22] = p10;  // the delay slot of the jal below
    (void)call_set(rt, ctx, 0x08A76AB4u, {p8, p4});
    (void)call_set(rt, ctx, 0x08A69D68u, {p0});
    (void)call_set(rt, ctx, function, {p10, unit, p8});
    object = word(rt, p10);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97DA8u});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0x84u);
    ctx.gpr[17] = p14;
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = slot;
    (void)call_set(rt, ctx, slot, {p14, object, kind});
    ctx.gpr[5] = word(rt, p14);  // the delay slot of the beq
    if (out != p14) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p14)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p14});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
}

// func_08A235F8(out, a1, count, args): sp[0x10..0x20] = 0 (five words); t = func_08ABC2B8(D_08CAAAC0) (0 returns 0); func_08ABC608(t, sp + 0x10) (0 returns 0). count > 0: o = args[0] (null: func_08A5BB7C(D_08CAAA48) first) with its virtual slot 0xC called with (sp + 0xC, o); r = func_08B20A54(D_08CF64CC, sp[0xC], -1); a non-negative r is stored at sp[0x10] and
// func_08ABBE30(t, sp + 0x10) runs; the string data sp[0xC] is released (func_08B60804(data - 4, -1), func_08B8542C(data - 0xC, sp + 1) when that is not positive; nothing for the shared empty cell D_08D74FD0 + 0xC); the function ends. count <= 0: n = func_08B20968(D_08CF64CC), func_08B85EB8(sp + 4, n, sp + 2), func_08A6D2D4(sp + 8, sp + 4),
// the holder sp + 8 is copied to `out` with func_08A5BAD8(out, sp[8]) unless it is `out`, func_08A5B9EC(sp + 8), and the data sp[4] is released (flag sp). v0 is the last call's.
// Registered in batch 103: sceSasSetEffectType (0x08C7B040) became recordable, and the cases that reach it (6 in the fast gate, count <= 0) compare equal; the count > 0 arm (the script-object call, the string release after func_08B20A54) never ran in a compared case and is read from the asm only.
void func_08A235F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u);
    ctx.gpr[20] = out;
    ctx.gpr[4] = 0x08CAAAC0u;
    ctx.gpr[18] = args;
    ctx.gpr[16] = count;
    for (unsigned offset = 0x10u; offset <= 0x1Cu; offset += 4u) put(rt, frame.at(offset), 0u);
    put(rt, frame.at(0x20u), 0u);  // the delay slot of the jal
    const std::uint32_t table = call_set(rt, ctx, 0x08ABC2B8u, {0x08CAAAC0u});
    ctx.gpr[17] = table;
    if (table == 0u) return;
    ctx.gpr[19] = p10;
    const std::uint32_t found = call_set(rt, ctx, 0x08ABC608u, {table, p10});
    if (found == 0u) return;
    if (signed_of(count) > 0) {
        ctx.gpr[16] = word(rt, args);
        std::uint32_t object = ctx.gpr[16];
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAAA48u});
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t slot = word(rt, vtable + 0xCu);
        ctx.gpr[3] = slot;
        ctx.gpr[2] = vtable;
        ctx.gpr[4] = pC;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot, {pC, object});
        const std::uint32_t value = call_set(rt, ctx, 0x08B20A54u, {0x08CF64CCu, word(rt, pC), 0xFFFFFFFFu});
        ctx.gpr[4] = table;  // the delay slot of the bltz
        if (signed_of(value) >= 0) {
            put(rt, p10, value);  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x08ABBE30u, {table, p10});
        }
        const std::uint32_t data = word(rt, pC);
        ctx.gpr[4] = data;
        ctx.gpr[2] = 0x08D74FD0u;
        ctx.gpr[16] = data - 0xCu;
        if (data - 0xCu == 0x08D74FD0u) return;
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        if (signed_of(references) > 0) return;
        ctx.gpr[4] = data - 0xCu;
        (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, frame.at(1u)});
        return;
    }
    ctx.gpr[5] = word(rt, p10);  // the delay slot of the blez
    ctx.gpr[4] = 0x08CF64CCu;
    const std::uint32_t name = call_set(rt, ctx, 0x08B20968u, {0x08CF64CCu});
    ctx.gpr[16] = p4;
    ctx.gpr[17] = p8;
    (void)call_set(rt, ctx, 0x08B85EB8u, {p4, name, frame.at(2u)});
    (void)call_set(rt, ctx, 0x08A6D2D4u, {p8, p4});
    ctx.gpr[5] = word(rt, p8);  // the delay slot of the beq
    if (out != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p8)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
    release_cell(rt, ctx, word(rt, p4), p0, 16u);
}

// func_08A243FC(out, count, args): count < 2 returns with v0 untouched. Holders sp + 0x10 and sp + 0x14 are built from args[0] and args[1] with func_08A6C410; t = func_08ABB9E8(0); h = sp[0x10] (null: func_08A5BB7C(D_08CAACD8) first) with its virtual slot 0xC called as (sp + 0xC, h); h2 = sp[0x14] (same) with the slot 0xC as (sp + 8, h2);
// r = func_08ABBAE0(t, sp[0xC], sp[8]); the string data sp[8] and then sp[0xC] are released (func_08B60804(data - 4, -1), func_08B8542C(data - 0xC, sp + 1 / sp) when that is not positive; nothing for the shared empty cell D_08D74FD0 + 0xC); func_08A97A6C(sp + 4, r); the holder sp + 4 is copied to `out` with func_08A5BAD8(out, sp[4]) unless it is `out`;
// func_08A5B9EC on sp + 4, sp + 0x14 and sp + 0x10 (v0 is the last one's).
// Not compared by any case (see its HOT91 line): none of its branches was exercised, every path is read from the asm only.
[[maybe_unused]] void func_08A243FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t empty = 0x08D74FD0u;
    const std::uint32_t p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p14 = frame.at(0x14u);
    ctx.gpr[5] = signed_of(count) < 2 ? 1u : 0u;
    ctx.gpr[19] = out;
    ctx.gpr[16] = args;
    if (signed_of(count) < 2) return;
    ctx.gpr[21] = p10;
    ctx.gpr[20] = p14;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A6C410u, {p10, args});
    (void)call_set(rt, ctx, 0x08A6C410u, {p14, args + 4u});
    const std::uint32_t table = call_set(rt, ctx, 0x08ABB9E8u, {0u});
    ctx.gpr[18] = table;  // the delay slot of the bnez
    std::uint32_t object = word(rt, p10);
    ctx.gpr[16] = object;
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAACD8u});
    std::uint32_t vtable = word(rt, object);
    std::uint32_t slot = word(rt, vtable + 0xCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[5] = object;
    (void)call_set(rt, ctx, slot, {pC, object});
    object = word(rt, p14);
    ctx.gpr[16] = object;
    ctx.gpr[17] = word(rt, pC);  // the delay slot of the bnez
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAACD8u});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0xCu);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = slot;
    (void)call_set(rt, ctx, slot, {p8, object});
    const std::uint32_t result = call_set(rt, ctx, 0x08ABBAE0u, {table, ctx.gpr[17], word(rt, p8)});
    ctx.gpr[18] = result;
    const std::uint32_t first = word(rt, p8);
    ctx.gpr[17] = empty;
    ctx.gpr[16] = first - 0xCu;
    std::uint32_t second = 0u;
    if (first - 0xCu == empty) {
        second = word(rt, pC);  // the delay slot of the beql, only when taken
        ctx.gpr[4] = second;
    } else {
        ctx.gpr[4] = first - 4u;
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {first - 4u, 0xFFFFFFFFu});
        second = word(rt, pC);  // the delay slot of the bgtz
        ctx.gpr[4] = second;
        if (signed_of(references) <= 0) {
            ctx.gpr[4] = first - 0xCu;
            (void)call_set(rt, ctx, 0x08B8542Cu, {first - 0xCu, frame.at(1u)});
            second = word(rt, pC);
            ctx.gpr[4] = second;
        }
    }
    ctx.gpr[16] = second - 0xCu;
    bool skip_release = second - 0xCu == empty;
    if (skip_release) {
        ctx.gpr[16] = p4;  // the delay slot of the beql, only when taken
    } else {
        ctx.gpr[4] = second - 4u;
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {second - 4u, 0xFFFFFFFFu});
        if (signed_of(references) > 0) {
            ctx.gpr[16] = p4;  // the delay slot of the bgtzl, taken
        } else {
            ctx.gpr[4] = ctx.gpr[16];
            (void)call_set(rt, ctx, 0x08B8542Cu, {second - 0xCu, frame.at(0u)});
            ctx.gpr[16] = p4;
        }
    }
    (void)call_set(rt, ctx, 0x08A97A6Cu, {p4, result});
    ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
    if (out != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p4)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p14});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
}

// func_08A6F704(out, item, mode, args): mode 1: func_08A6C410(sp + 0xC, ...) (a0 only), u = sp[0xC] (null: func_08A5BB7C(D_08CB0754) first), o = args[0] (null: func_08A5BB7C(D_08CB06B8) first) with its virtual slot 0xC called as (sp + 4, o); func_08A6F4D8(sp + 8, u, sp + 4); the holder sp + 8 is copied to `out` with func_08A5BAD8(out, sp[8]) unless it is `out`;
// func_08A5B9EC(sp + 8); the data sp[4] is released (func_08B60804(data - 4, -1), func_08B8542C(data - 0xC, sp) when not positive; nothing for the shared empty cell D_08D74FD0 + 0xC); func_08A5B9EC(sp + 0xC) ends it. Any other mode: func_08A98A70(sp + 0xC); u = sp[0xC] (null: func_08A5BB7C(D_08CB0774) first), the vector v = u + 0x14 {., end, capacity}: end == capacity calls func_08B8A03C(v, u[0x18], item),
// otherwise a non-null end gets func_08A76ABC(end, item) and the vector's end becomes end + 4 (a null end stores 4); the holder sp + 0xC is copied to `out` with func_08A5BAD8(out, sp[0xC]) unless it is `out`; func_08A5B9EC(sp + 0xC) ends it.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): mode 1 always: every case took the first branch, so the other mode (0x08A6F72C bne taken: 0x08A6F7EC-0x08A6F85C, the append to the vector at u + 0x14 with func_08B8A03C / func_08A76ABC and D_08CB0774) is read from the asm only; the null checks of u and o (0x08A6F744, 0x08A6F75C; D_08CB0754, D_08CB06B8), `out` equal to sp + 8 (0x08A6F79C), a shared empty cell (0x08A6F7C4) and the call of func_08B8542C (0x08A6F7D4) never occurred.
void func_08A6F704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], item = ctx.gpr[5], mode = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[19] = item;
    ctx.gpr[18] = out;
    ctx.gpr[16] = args;
    ctx.gpr[2] = 1u;
    if (mode == 1u) {
        ctx.gpr[20] = pC;
        (void)call_set(rt, ctx, 0x08A6C410u, {pC});
        const std::uint32_t unit = word(rt, pC);
        ctx.gpr[19] = unit;
        ctx.gpr[17] = word(rt, args);  // the delay slot of the bnel, only when taken
        if (unit == 0u) {
            (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0754u});
            ctx.gpr[17] = word(rt, args);
        }
        std::uint32_t object = ctx.gpr[17];
        ctx.gpr[2] = word(rt, object);  // the delay slot of the bnel, only when taken
        if (object == 0u) {
            (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB06B8u});
            ctx.gpr[2] = word(rt, object);
        }
        const std::uint32_t vtable = ctx.gpr[2];
        const std::uint32_t slot = word(rt, vtable + 0xCu);
        ctx.gpr[16] = p4;
        ctx.gpr[3] = slot;
        ctx.gpr[17] = p8;  // the delay slot of the jalr
        (void)call_set(rt, ctx, slot, {p4, object});
        (void)call_set(rt, ctx, 0x08A6F4D8u, {p8, unit, p4});
        ctx.gpr[5] = word(rt, p8);  // the delay slot of the beq
        if (out != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p8)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
        const std::uint32_t data = word(rt, p4);
        ctx.gpr[4] = data;
        ctx.gpr[2] = 0x08D74FD0u;
        ctx.gpr[16] = data - 0xCu;
        ctx.gpr[4] = data - 4u;  // the delay slot of the beq
        if (data - 0xCu != 0x08D74FD0u) {
            const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
            ctx.gpr[4] = data - 0xCu;  // the delay slot of the bgtz
            if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, p0});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
        return;
    }
    ctx.gpr[20] = pC;
    (void)call_set(rt, ctx, 0x08A98A70u, {pC});
    const std::uint32_t object = word(rt, pC);
    ctx.gpr[17] = object;
    ctx.gpr[16] = object + 0x14u;  // the delay slot of the bnez
    if (object == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0774u});
        ctx.gpr[16] = object + 0x14u;
    }
    const std::uint32_t vector = object + 0x14u;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t capacity = word(rt, vector + 8u);
    ctx.gpr[4] = end;
    ctx.gpr[2] = capacity;
    if (end == capacity) {
        ctx.gpr[5] = word(rt, object + 0x18u);  // the delay slot of the beql, only when taken
        ctx.gpr[4] = vector;
        ctx.gpr[6] = item;
        (void)call_set(rt, ctx, 0x08B8A03Cu, {vector, word(rt, object + 0x18u), item});
    } else {
        ctx.gpr[2] = end + 4u;  // the delay slot of the beqz
        std::uint32_t after = end + 4u;
        if (end != 0u) {
            (void)call_set(rt, ctx, 0x08A76ABCu, {end, item});
            const std::uint32_t reloaded = word(rt, vector + 4u);
            ctx.gpr[4] = reloaded;
            after = reloaded + 4u;
            ctx.gpr[2] = after;
        }
        put(rt, vector + 4u, after);  // the delay slot of the j
    }
    ctx.gpr[5] = word(rt, pC);  // the delay slot of the beq
    if (out != pC) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, pC)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
}

// func_089CA118(out, a1, count, args): the holder sp + 8 = func_089B395C(sp + 8, D_08CF6ED8); count <= 0 ends with func_08A5B9EC(sp + 8). Otherwise func_089C5850(sp + 0xC, a1); s6 = 1, but for a count of exactly 2: o = args[1] (null: func_08A5BB7C(D_08CA6304) first) and s6 = its virtual slot 0x30 called with (o).
// u = sp[0xC] (null: func_08A5BB7C(D_08CA63A0) first), p = args[0] (null: func_08A5BB7C(D_08CA6304) first) with its virtual slot 0xC called as (sp + 4, p); func_089C9F24(u, sp + 8, sp + 4, s6); the string data sp[4] is released (func_08B60804(data - 4, -1), func_08B8542C(data - 0xC, sp) when not positive; nothing for the shared empty cell D_08D74FD0 + 0xC);
// the holder sp + 8 is copied to `out` with func_08A5BAD8(out, sp[8]) unless it is `out`; func_08A5B9EC(sp + 0xC) and func_08A5B9EC(sp + 8) end it.
// Not compared by any case (see its HOT91 line): none of its branches was exercised, every path is read from the asm only.
[[maybe_unused]] void func_089CA118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], source = ctx.gpr[5], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[20] = p8;
    ctx.gpr[17] = source;
    ctx.gpr[21] = out;
    ctx.gpr[16] = count;
    ctx.gpr[19] = args;
    (void)call_set(rt, ctx, 0x089B395Cu, {p8, 0x08CF6ED8u});
    ctx.gpr[23] = pC;  // the delay slot of the blez
    if (signed_of(count) > 0) {
        (void)call_set(rt, ctx, 0x089C5850u, {pC, source});
        ctx.gpr[2] = 2u;
        ctx.gpr[22] = 1u;  // the delay slot of the bne
        if (count == 2u) {
            const std::uint32_t object = word(rt, args + 4u);
            ctx.gpr[16] = object;
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t slot = word(rt, vtable + 0x30u);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = slot;
            ctx.gpr[22] = call_set(rt, ctx, slot, {object});
        }
        const std::uint32_t unit = word(rt, pC);
        ctx.gpr[18] = unit;
        ctx.gpr[17] = word(rt, args);  // the delay slot of the bnel, only when taken
        if (unit == 0u) {
            (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA63A0u});
            ctx.gpr[17] = word(rt, args);
        }
        const std::uint32_t first = ctx.gpr[17];
        if (first == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
        const std::uint32_t vtable = word(rt, first);
        const std::uint32_t slot = word(rt, vtable + 0xCu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = slot;
        ctx.gpr[16] = p4;
        (void)call_set(rt, ctx, slot, {p4, first});  // the delay slot sets a1 = s1
        (void)call_set(rt, ctx, 0x089C9F24u, {unit, p8, p4, ctx.gpr[22]});
        const std::uint32_t data = word(rt, p4);
        ctx.gpr[4] = data;
        ctx.gpr[2] = 0x08D74FD0u;
        ctx.gpr[16] = data - 0xCu;
        ctx.gpr[4] = data - 4u;  // the delay slot of the beq
        if (data - 0xCu != 0x08D74FD0u) {
            const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
            ctx.gpr[4] = data - 0xCu;  // the delay slot of the bgtz
            if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, p0});
        }
        ctx.gpr[5] = word(rt, p8);  // the delay slot of the beq
        if (out != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p8)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
}

// func_08A954B8(this, out, a2, args): a = the virtual slot 0xD8 of this[0] called with (this); o = args[0] (null: func_08A5BB7C(D_08CB211C) first); b = the virtual slot 0xD8 of o[0] with (o). a != b: func_08A97A6C(sp, 1), the holder sp copied to `out` with func_08A5BAD8(out, sp[0]) unless it is `out`, func_08A5B9EC(sp).
// a == b: x = the slot 0xDC of this[0] with (this, 0); o again (args[0], null: func_08A5BB7C(D_08CB211C) first) with its slot 0xDC as (o, 0) gives y; z = the slot 0xD8 of this[0] with (this); r = func_08B36170(x, y, z); func_08A97A6C(sp + 4, r != 0); the holder sp + 4 is copied to `out` with func_08A5BAD8(out, sp[4]) unless it is `out`; func_08A5B9EC(sp + 4).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the null checks of args[0] (0x08A954F4, 0x08A95538; D_08CB211C) and the unequal answers (0x08A95518 bne taken: func_08A97A6C(sp, 1) and the copy of sp) never occurred, `out` equal to sp + 4 (0x08A95594) never either, and func_08B36170 always returned non-zero (a1 = 1).
void func_08A954B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u);
    ctx.gpr[20] = out;
    ctx.gpr[19] = args;
    std::uint32_t vtable = word(rt, self);
    std::uint32_t slot = word(rt, vtable + 0xD8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[18] = self;  // the delay slot of the jalr
    const std::uint32_t first = call_set(rt, ctx, slot, {self});
    std::uint32_t object = word(rt, args);
    ctx.gpr[16] = object;
    ctx.gpr[17] = first;  // the delay slot of the bnez
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB211Cu});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0xD8u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;  // the delay slot of the jalr
    const std::uint32_t second = call_set(rt, ctx, slot, {object});
    ctx.gpr[4] = p0;  // the delay slot of the bne
    if (first != second) {
        (void)call_set(rt, ctx, 0x08A97A6Cu, {p0, 1u});
        ctx.gpr[5] = word(rt, p0);  // the delay slot of the beq
        if (out != p0) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p0)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
        return;
    }
    vtable = word(rt, self);
    slot = word(rt, vtable + 0xDCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = self;
    ctx.gpr[5] = 0u;  // the delay slot of the jalr
    const std::uint32_t x = call_set(rt, ctx, slot, {self, 0u});
    object = word(rt, args);
    ctx.gpr[16] = object;
    ctx.gpr[21] = x;  // the delay slot of the bnez
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB211Cu});
    vtable = word(rt, object);
    slot = word(rt, vtable + 0xDCu);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;
    ctx.gpr[5] = 0u;
    ctx.gpr[17] = p4;  // the delay slot of the jalr
    const std::uint32_t y = call_set(rt, ctx, slot, {object, 0u});
    vtable = word(rt, self);
    ctx.gpr[3] = vtable;
    ctx.gpr[16] = y;  // the delay slot of the next load
    slot = word(rt, vtable + 0xD8u);
    ctx.gpr[2] = slot;
    ctx.gpr[4] = self;  // the delay slot of the jalr
    const std::uint32_t z = call_set(rt, ctx, slot, {self});
    ctx.gpr[6] = z;
    ctx.gpr[5] = y;
    ctx.gpr[4] = x;
    const std::uint32_t order = call_set(rt, ctx, 0x08B36170u, {x, y, z});
    ctx.gpr[5] = order != 0u ? 1u : 0u;
    ctx.gpr[4] = p4;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A97A6Cu, {p4, order != 0u ? 1u : 0u});
    ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
    if (out != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p4)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
}

// func_089B6408(this, a1, event): this[0x158] (a byte) == 0 calls func_0896ED94(this) and returns its v0. Otherwise s2 = this[0x138]; when func_0896ED94(this) is 0 the key word event[0x28] decides: 0xFF52 or 0xFF97 call func_089B5E70(this, -1), 0xFF54 or 0xFF99 call func_089B5E70(this, 1); when this[0x138] has changed from s2, func_089B6244(this, a1).
// o = func_089AAC8C(a1) (0 returns v0 = 2); the key 0xFF0D: c = func_089A3130(o), func_08A11BF0(sp, a1, c), h = sp[0] (null: func_08A5BB7C(D_08CA592C) first), the virtual slot 0x108 of h[0] called with (h), func_08A5B9EC(sp), v0 = 2; the key 0xFF1B: the same with func_08A0BE48 and D_08CA593C. Any other key returns v0 = 2.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): func_0896ED94 returning non-zero (0x089B643C bnel taken), the keys 0xFF52, 0xFF97 and 0xFF99 (0x089B6448-0x089B6490: func_089B5E70 with -1 or 1), func_089AAC8C returning 0 (0x089B64AC), the key 0xFF1B (0x089B64FC on: func_08A0BE48, D_08CA593C) and the null check of h (0x089B64E0; D_08CA592C) never occurred; the keys 0xFF54 and 0xFF0D and the byte this[0x158] == 0 did. Read from the asm only: those keys and their constants (0xFF52, 0xFF97, 0xFF99, 0xFF1B).
void func_089B6408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[5], event = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u);
    ctx.gpr[19] = event;
    ctx.gpr[17] = argument;
    ctx.gpr[2] = byte_u(rt, self + 0x158u);
    ctx.gpr[16] = self;  // the delay slot of the beqz
    if (byte_u(rt, self + 0x158u) == 0u) {
        (void)call_set(rt, ctx, 0x0896ED94u, {});
        return;
    }
    ctx.gpr[18] = word(rt, self + 0x138u);  // the delay slot of the jal
    const std::uint32_t armed = call_set(rt, ctx, 0x0896ED94u, {});
    std::uint32_t current = word(rt, self + 0x138u);
    ctx.gpr[2] = current;
    if (armed == 0u) {
        const std::uint32_t key = word(rt, event + 0x28u);
        ctx.gpr[3] = key;
        ctx.gpr[4] = self;
        if (key == 0xFF52u || key == 0xFF97u) {
            (void)call_set(rt, ctx, 0x089B5E70u, {self, 0xFFFFFFFFu});
            current = word(rt, self + 0x138u);
            ctx.gpr[2] = current;
        } else if (key == 0xFF54u || key == 0xFF99u) {
            (void)call_set(rt, ctx, 0x089B5E70u, {self, 1u});
            current = word(rt, self + 0x138u);
            ctx.gpr[2] = current;
        } else {
            current = word(rt, self + 0x138u);
            ctx.gpr[2] = current;
        }
    }
    ctx.gpr[4] = self;  // the delay slot of the beq
    if (ctx.gpr[18] != current) (void)call_set(rt, ctx, 0x089B6244u, {self, argument});
    const std::uint32_t target = call_set(rt, ctx, 0x089AAC8Cu, {argument});
    ctx.gpr[4] = target;  // the delay slot of the beqz
    if (target == 0u) {
        ctx.gpr[2] = 2u;
        return;
    }
    const std::uint32_t key = word(rt, event + 0x28u);
    ctx.gpr[6] = key;
    ctx.gpr[2] = 0xFF0Du;
    std::uint32_t function = 0u, null_target = 0u;
    ctx.gpr[2] = 0xFF1Bu;  // the delay slot of the bne
    if (key == 0xFF0Du) {
        function = 0x08A11BF0u;
        null_target = 0x08CA592Cu;
    } else if (key == 0xFF1Bu) {
        function = 0x08A0BE48u;
        null_target = 0x08CA593Cu;
    } else {
        ctx.gpr[2] = 2u;  // the delay slot of the bne
        return;
    }
    const std::uint32_t value = call_set(rt, ctx, 0x089A3130u, {target});
    ctx.gpr[6] = value;
    ctx.gpr[17] = argument;
    (void)call_set(rt, ctx, function, {p0, argument, value});
    const std::uint32_t object = word(rt, p0);
    ctx.gpr[16] = object;
    ctx.gpr[3] = word(rt, object);  // the delay slot of the bnel (v1 = object[0])
    if (object == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {null_target});
        ctx.gpr[3] = word(rt, object);
    }
    const std::uint32_t slot = word(rt, ctx.gpr[3] + 0x108u);
    ctx.gpr[2] = slot;
    ctx.gpr[4] = object;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {object});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
    ctx.gpr[2] = 2u;
}

// func_08826C50(out): func_088248E4(sp + 4); u = sp[4] (null: func_08A5BB7C(D_08C99B4C) first), s4 = u[0x14]; func_08A5B9EC(sp + 4); n = func_0885C2B4(s4); func_08A98C1C(sp + 8, n); then for i from 0 while i < n and i < 4 (n <= 0 skips the loop): item = func_0885C1AC(s4, i), func_08A98C1C(sp + 0xC, 2),
// the destination d = sp[8][0x14] + 4 * i gets func_08857EE4(sp, item, 0), the holder sp copied to d with func_08A5BAD8(d, sp[0]) unless d is sp, and func_08A5B9EC on sp and sp + 0xC. After the loop the holder sp + 8 is copied to `out` with func_08A5BAD8(out, sp[8]) unless it is `out`, and func_08A5B9EC(sp + 8) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): the null check of u (0x08826C90; D_08C99B4C), n <= 0 (0x08826CCC blez), the destination equal to sp (0x08826D2C beq), the loop ending by `i == n` (0x08826CE4 beq) rather than by `i == 4` (0x08826D4C) and `out` equal to sp + 8 (0x08826D54) never occurred.
void func_08826C50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu);
    ctx.gpr[17] = p4;
    ctx.gpr[21] = out;
    (void)call_set(rt, ctx, 0x088248E4u, {p4});
    const std::uint32_t unit = word(rt, p4);
    ctx.gpr[16] = unit;
    if (unit == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
    const std::uint32_t list = word(rt, unit + 0x14u);
    ctx.gpr[20] = list;
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    const std::uint32_t count = call_set(rt, ctx, 0x0885C2B4u, {list});
    ctx.gpr[19] = p8;
    ctx.gpr[17] = count;
    (void)call_set(rt, ctx, 0x08A98C1Cu, {p8, count});
    ctx.gpr[30] = p0;  // the delay slot of the blez
    if (signed_of(count) > 0) {
        ctx.gpr[22] = pC;
        ctx.gpr[18] = 0u;
        ctx.gpr[23] = 4u;  // the delay slot of the j
        std::uint32_t index = 0u;
        for (;;) {
            const std::uint32_t item = call_set(rt, ctx, 0x0885C1ACu, {list, index});
            ctx.gpr[16] = item;  // the delay slot of the next jal
            (void)call_set(rt, ctx, 0x08A98C1Cu, {pC, 2u});
            const std::uint32_t array = word(rt, p8);
            const std::uint32_t destination = word(rt, array + 0x14u) + (index << 2);
            ctx.gpr[2] = array;
            ctx.gpr[3] = index << 2;
            ctx.gpr[16] = destination;
            ctx.gpr[18] = index + 1u;  // the delay slot of the jal below
            (void)call_set(rt, ctx, 0x08857EE4u, {p0, item, 0u});
            ++index;
            ctx.gpr[4] = destination;
            if (destination != p0) (void)call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, p0)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {p0});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
            if (index == 4u) break;
            if (count == index) break;
        }
    }
    ctx.gpr[5] = word(rt, p8);  // the delay slot of the beq
    if (out != p8) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p8)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
}

// func_0899CE20(a0, a1, count, args): count < 3 returns with v0 = 1 (the slti). Holders sp + 0xC (func_0899C4B4) and sp + 0x10 (func_089AB370(sp + 0x10, args)); u = sp[0xC] (null: func_08A5BB7C(D_08CA482C) first), u[0x1C] = sp[0x10]; func_089BB3A8(sp + 0x14, args + 4); u (null: the null-case call again), w = sp[0x14]: non-null runs func_08A5BA08(w) with s1 = w set before the call (the delay slot of the jal), else s1 = 0;
// u[0x20] (non-zero: func_08A5B930 of it first) = s1; u again (sp[0xC], null: the null-case call) and o = args[2] (null: func_08A5BB7C(D_08CA47D4) first) with its virtual slot 0xC called as (sp + 8, o); func_0899C2CC(u, sp + 8); the string data sp[8] is released (func_08B60804(data - 4, -1), func_08B8542C(data - 0xC, sp) when not positive; nothing for the shared empty cell D_08D74FD0 + 0xC).
// A count of 4 or more: u (sp[0xC], null: the null-case call) , func_08A98674(sp + 4, args + 0xC), func_0899C2B0(u, sp + 4), func_08A5B9EC(sp + 4). func_08A5B9EC on sp + 0x14, sp + 0x10 and sp + 0xC end it (v0 is the last one's).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count < 3 (0x0899CE50), the null checks of u / u / s2 / o (0x0899CE78, 0x0899CEA8, 0x0899CEF0, 0x0899CF08), a null func_08A5BA08 argument (0x0899CEC0 beqz taken: s1 = 0), a non-zero old u[0x20] (0x0899CED8 beql not taken: func_08A5B930), the call of func_08B8542C (0x08B60804 never non-positive), and count 4 or more (0x0899CF7C bnez: func_08A98674(sp + 4, args + 0xC), func_0899C2B0, func_08A5B9EC and the null check 0x0899CF84) never occurred. Read from the asm only: the count >= 4 part (0x0899CF80-0x0899CFB8) and the old u[0x20] release.
void func_0899CE20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p14 = frame.at(0x14u);
    ctx.gpr[2] = signed_of(count) < 3 ? 1u : 0u;
    ctx.gpr[20] = count;
    ctx.gpr[19] = args;
    if (signed_of(count) < 3) return;
    ctx.gpr[23] = pC;
    ctx.gpr[22] = p10;  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0899C4B4u, {pC});
    (void)call_set(rt, ctx, 0x089AB370u, {p10, args});
    std::uint32_t unit = word(rt, pC);
    ctx.gpr[16] = unit;
    ctx.gpr[2] = word(rt, p10);  // the delay slot of the bnez
    if (unit == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA482Cu});
        ctx.gpr[2] = word(rt, p10);
    }
    ctx.gpr[21] = p14;
    put(rt, unit + 0x1Cu, ctx.gpr[2]);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x089BB3A8u, {p14, args + 4u});
    unit = word(rt, pC);
    ctx.gpr[16] = unit;
    ctx.gpr[2] = word(rt, p14);  // the delay slot of the bnez
    if (unit == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA482Cu});
        ctx.gpr[2] = word(rt, p14);
    }
    const std::uint32_t pending = ctx.gpr[2];
    ctx.gpr[17] = 0u;  // the delay slot of the beqz
    if (pending != 0u) {
        ctx.gpr[17] = pending;  // the delay slot of the jal: s1 is the word, not the call's result
        (void)call_set(rt, ctx, 0x08A5BA08u, {pending});
    }
    const std::uint32_t old = word(rt, unit + 0x20u);
    ctx.gpr[4] = old;
    if (old == 0u) {
        put(rt, unit + 0x20u, ctx.gpr[17]);  // the delay slot of the beql, taken
    } else {
        (void)call_set(rt, ctx, 0x08A5B930u, {old});
        put(rt, unit + 0x20u, ctx.gpr[17]);
    }
    const std::uint32_t second = word(rt, pC);
    ctx.gpr[18] = second;
    ctx.gpr[17] = word(rt, args + 8u);  // the delay slot of the bnel, only when taken
    if (second == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA482Cu});
        ctx.gpr[17] = word(rt, args + 8u);
    }
    const std::uint32_t object = ctx.gpr[17];
    ctx.gpr[3] = word(rt, object);  // the delay slot of the bnel, only when taken
    if (object == 0u) {
        (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA47D4u});
        ctx.gpr[3] = word(rt, object);
    }
    const std::uint32_t slot = word(rt, ctx.gpr[3] + 0xCu);
    ctx.gpr[16] = p8;
    ctx.gpr[2] = slot;
    ctx.gpr[5] = object;  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {p8, object});
    (void)call_set(rt, ctx, 0x0899C2CCu, {second, p8});
    const std::uint32_t data = word(rt, p8);
    ctx.gpr[4] = data;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[16] = data - 0xCu;
    ctx.gpr[2] = signed_of(count) < 4 ? 1u : 0u;  // the delay slot of the beq
    if (data - 0xCu != 0x08D74FD0u) {
        ctx.gpr[4] = data - 4u;
        const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
        ctx.gpr[2] = signed_of(count) < 4 ? 1u : 0u;  // the delay slot of the bgtz
        ctx.gpr[4] = data - 0xCu;
        if (signed_of(references) <= 0) {
            (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, p0});
            ctx.gpr[2] = signed_of(count) < 4 ? 1u : 0u;
        }
    }
    const std::uint32_t last = word(rt, pC);
    ctx.gpr[17] = last;
    if (signed_of(count) >= 4) {
        ctx.gpr[16] = p4;  // the delay slot of the bnez
        if (last == 0u) {
            (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA482Cu});
            ctx.gpr[16] = p4;
        }
        ctx.gpr[5] = args + 0xCu;
        (void)call_set(rt, ctx, 0x08A98674u, {p4, args + 0xCu});
        (void)call_set(rt, ctx, 0x0899C2B0u, {last, p4});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p14});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p10});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
}

// func_0881D004(out, a1, count, args): count <= 0 returns with v0 untouched. func_0881BD98(sp + 0xC); h = sp[0xC] (null: func_08A5BB7C(D_08C98F48) first); p = func_0881B068(h); func_08A5B9EC(sp + 0xC); func_0881BF5C(p, count, args, sp + 0x18, sp + 0x10). With x = sp[0x18], y = sp[0x1C], z = sp[0x10]:
// x == -1 and y == -1: p[4] = p[5] = -1 (words at p + 0x10 and p + 0x14), p[3] = z (p + 0xC), func_08A97A6C(sp + 8, 0), the holder sp + 8 copied to `out` with func_08A5BAD8(out, sp[8]) unless it is `out`, func_08A5B9EC(sp + 8). Otherwise r = func_0881A8C0(p, count, x, y, z): r > 0 stores p + 0x10 = x, p + 0x14 = y, p + 0xC = z, func_0881AC68(p, z, x, y, z),
// func_08A97A6C(sp, 1) and the holder sp is copied to `out` the same way; r <= 0 gives func_08A97A6C(sp + 4, 0) and the holder sp + 4 is copied to `out`. The holder is destroyed with func_08A5B9EC in each case (v0 is that call's).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x0881D028), the null check of h (0x0881D040; D_08C98F48), a verdict of 0 or less from func_0881A8C0 (0x0881D0E8 bgtz not taken: 0x0881D0F0-0x0881D118, func_08A97A6C(sp + 4, 0)) and `out` equal to the holder (0x0881D100, 0x0881D154) never occurred; y was always -1 when x was. Read from the asm only: the verdict <= 0 path.
void func_0881D004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t p0 = frame.at(0u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u), p18 = frame.at(0x18u);
    ctx.gpr[20] = out;
    ctx.gpr[18] = args;
    ctx.gpr[17] = count;
    if (signed_of(count) <= 0) return;
    ctx.gpr[19] = pC;
    (void)call_set(rt, ctx, 0x0881BD98u, {pC});
    const std::uint32_t holder = word(rt, pC);
    ctx.gpr[16] = holder;
    ctx.gpr[4] = high_half(0x08C98F48u);  // the delay slot of the bnez
    if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    const std::uint32_t target = call_set(rt, ctx, 0x0881B068u, {holder});
    ctx.gpr[16] = target;
    (void)call_set(rt, ctx, 0x08A5B9ECu, {pC});
    (void)call_set(rt, ctx, 0x0881BF5Cu, {target, count, args, p18, p10});
    const std::uint32_t x = word(rt, p18), y = word(rt, frame.at(0x1Cu)), z = word(rt, p10);
    ctx.gpr[6] = x;
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[7] = y;  // the delay slot of the bne
    std::uint32_t result_holder = 0u;
    if (x == 0xFFFFFFFFu && y == 0xFFFFFFFFu) {
        ctx.gpr[8] = z;
        ctx.gpr[2] = 0xFFFFFFFFu;
        ctx.gpr[3] = 0xFFFFFFFFu;
        put(rt, target + 0x10u, 0xFFFFFFFFu);
        ctx.gpr[17] = p8;
        put(rt, target + 0x14u, 0xFFFFFFFFu);
        ctx.gpr[2] = z;
        put(rt, target + 0xCu, z);  // the delay slot of the jal
        (void)call_set(rt, ctx, 0x08A97A6Cu, {p8, 0u});
        result_holder = p8;
    } else {
        ctx.gpr[8] = z;  // the delay slot of the bne
        const std::uint32_t verdict = call_set(rt, ctx, 0x0881A8C0u, {target, ctx.gpr[5], x, y, z});
        ctx.gpr[2] = word(rt, p18);  // the delay slot of the bgtz
        if (signed_of(verdict) > 0) {
            ctx.gpr[3] = word(rt, frame.at(0x1Cu));
            put(rt, target + 0x10u, ctx.gpr[2]);
            put(rt, target + 0x14u, ctx.gpr[3]);
            ctx.gpr[5] = word(rt, p10);
            put(rt, target + 0xCu, ctx.gpr[5]);
            ctx.gpr[6] = word(rt, p18);
            ctx.gpr[7] = word(rt, frame.at(0x1Cu));
            ctx.gpr[8] = word(rt, p10);  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x0881AC68u, {target, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7], ctx.gpr[8]});
            (void)call_set(rt, ctx, 0x08A97A6Cu, {p0, 1u});
            result_holder = p0;
        } else {
            ctx.gpr[16] = p4;
            (void)call_set(rt, ctx, 0x08A97A6Cu, {p4, 0u});
            result_holder = p4;
        }
    }
    ctx.gpr[5] = word(rt, result_holder);  // the delay slot of the beq
    if (out != result_holder) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, result_holder)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {result_holder});
}

// func_08B666E8(a, b): (s2, s3) = the register pair func_08B67E04() leaves (a0 / a1 are the entry ones). With m = the word D_08C96698: m == -1 returns that pair; otherwise func_08B6E4FC(a, b) non-zero returns it too. Otherwise (u, w) = func_08B6E268(a, b) and func_08B71A90(u, w, D_08CB65D8 pair) not positive returns the pair.
// A positive answer builds the record at sp = {1, D_08CB5E30, a, b, a, b, 0, 0, 0} (words 0..3 and 4..8, sp[0x20] = 0) and, unless m == 2, calls func_08B6E524(sp); m == 2 or a zero answer calls func_08B77D40() and stores 0x21 through the pointer it returns. Then, in both cases: sp[0x20] non-zero calls func_08B77D40() and stores sp[0x20] through its pointer;
// the pair returned is (s2, s3) = (sp[0x18], sp[0x1C]). v0 / v1 are that pair.
// Not compared by any case (see its HOT91 line): none of its branches was exercised, every path is read from the asm only.
[[maybe_unused]] void func_08B666E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], second = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = second;
    ctx.gpr[16] = first;
    (void)call_keep(rt, ctx, 0x08B67E04u, {});
    ctx.gpr[18] = ctx.gpr[2];
    const std::uint32_t mode = word(rt, 0x08C96698u);
    ctx.gpr[20] = mode;
    ctx.gpr[19] = ctx.gpr[3];
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[4] = first;  // the delay slot of the beq
    const auto finish = [&]() {
        ctx.gpr[2] = ctx.gpr[18];
        ctx.gpr[3] = ctx.gpr[19];
    };
    if (mode != 0xFFFFFFFFu) {
        const std::uint32_t known = call_set(rt, ctx, 0x08B6E4FCu, {first, second});
        ctx.gpr[4] = first;  // the delay slot of the beqz
        if (known != 0u) return finish();
    } else {
        return finish();
    }
    const std::uint32_t reduced = call_set(rt, ctx, 0x08B6E268u, {first, second});
    ctx.gpr[5] = second;
    const std::uint32_t reduced_high = ctx.gpr[3];
    ctx.gpr[6] = word(rt, 0x08CB65D8u);
    ctx.gpr[7] = word(rt, 0x08CB65DCu);
    ctx.gpr[5] = reduced_high;
    ctx.gpr[4] = reduced;  // the delay slot of the jal
    const std::uint32_t answer = call_set(rt, ctx, 0x08B71A90u, {reduced, reduced_high, ctx.gpr[6], ctx.gpr[7]});
    ctx.gpr[2] = 1u;  // the delay slot of the blez
    if (signed_of(answer) <= 0) return finish();
    ctx.gpr[3] = 0x08CB5E30u;
    put(rt, frame.at(0u), 1u);
    put(rt, frame.at(4u), 0x08CB5E30u);
    ctx.gpr[2] = 2u;
    put(rt, frame.at(8u), first);
    put(rt, frame.at(0xCu), second);
    put(rt, frame.at(0x20u), 0u);
    put(rt, frame.at(0x10u), first);
    put(rt, frame.at(0x14u), second);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);  // the delay slot of the beq
    bool failed = mode == 2u;
    if (!failed) {
        const std::uint32_t handled = call_set(rt, ctx, 0x08B6E524u, {frame.at(0u)});
        failed = handled == 0u;
    }
    if (failed) {
        const std::uint32_t slot = call_keep(rt, ctx, 0x08B77D40u, {});
        ctx.gpr[3] = 0x21u;
        put(rt, slot, 0x21u);
    }
    const std::uint32_t errno_value = word(rt, frame.at(0x20u));
    ctx.gpr[2] = errno_value;
    if (errno_value != 0u) {
        const std::uint32_t slot = call_keep(rt, ctx, 0x08B77D40u, {});
        ctx.gpr[3] = word(rt, frame.at(0x20u));
        put(rt, slot, ctx.gpr[3]);
    }
    ctx.gpr[18] = word(rt, frame.at(0x18u));
    ctx.gpr[19] = word(rt, frame.at(0x1Cu));
    finish();
}

// func_08898C10(this, key): o = this[0x40]. When o is non-zero: s0 = func_08898744(this[0x34], o, key); the record at sp = {func_0894CF58(this[0x34], key), func_0894CFB8(this[0x34], key), key, s0} (sp[8] = -1 and sp[0xC] = 0 first) is passed to the virtual slot 0x10 of o[0] as (o, key, sp, o).
// n = func_088F8AA4(this, key); o = this[0x40] zero or n <= 0 returns (v0 as the last call left it). Otherwise p = func_088F8D58(this) (0 returns), q = p + 0x20; func_088977D4(q) == 1 continues: func_088970E0(q) gives s0, w = func_0889854C(this, key) and func_088984E4(this[0x40], key, s0, w) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): this[0x40] == 0 at the start (0x08898C34 beqz taken: the first call chain was always run), this[0x40] == 0 / n <= 0 at 0x08898CB4 / 0x08898CBC and func_088F8D58 returning 0 (0x08898CD0) never occurred; the final calls (0x08898CE4 on, with func_088977D4 == 1) did run.
void func_08898C10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[6] = key;
    ctx.gpr[18] = key;
    ctx.gpr[17] = self;
    const std::uint32_t object = word(rt, self + 0x40u);
    ctx.gpr[2] = object;
    ctx.gpr[5] = object;  // the delay slot of the beqz
    if (object != 0u) {
        ctx.gpr[4] = word(rt, self + 0x34u);  // the delay slot of the jal
        const std::uint32_t found = call_keep(rt, ctx, 0x08898744u, {word(rt, self + 0x34u), object, key});
        ctx.gpr[16] = found;
        put(rt, frame.at(8u), 0xFFFFFFFFu);
        put(rt, frame.at(0u), 0u);
        put(rt, frame.at(4u), 0u);
        put(rt, frame.at(0xCu), 0u);  // the delay slot of the jal
        const std::uint32_t first = call_set(rt, ctx, 0x0894CF58u, {word(rt, self + 0x34u), key});
        put(rt, frame.at(0u), first);  // the delay slot of the jal below
        const std::uint32_t second = call_set(rt, ctx, 0x0894CFB8u, {word(rt, self + 0x34u), key});
        const std::uint32_t target = word(rt, self + 0x40u);
        put(rt, frame.at(4u), second);
        put(rt, frame.at(0xCu), found);
        put(rt, frame.at(8u), key);
        const std::uint32_t vtable = word(rt, target);
        const std::uint32_t slot = word(rt, vtable + 0x10u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = slot;
        ctx.gpr[7] = target;
        ctx.gpr[6] = frame.at(0u);
        ctx.gpr[5] = key;  // the delay slot of the jalr
        (void)call_keep(rt, ctx, slot, {target, key, frame.at(0u), target});
    }
    const std::uint32_t count = call_set(rt, ctx, 0x088F8AA4u, {self, key});
    ctx.gpr[3] = count;
    const std::uint32_t armed = word(rt, self + 0x40u);
    ctx.gpr[2] = armed;
    ctx.gpr[5] = key;  // the delay slot of the beqz
    if (armed == 0u) return;
    ctx.gpr[4] = self;  // the delay slot of the blez
    if (signed_of(count) <= 0) return;
    const std::uint32_t entry = call_set(rt, ctx, 0x088F8D58u, {self});
    ctx.gpr[16] = entry + 0x20u;
    ctx.gpr[4] = entry + 0x20u;  // the delay slot of the beqz
    if (entry == 0u) return;
    const std::uint32_t kind = call_set(rt, ctx, 0x088977D4u, {entry + 0x20u});
    ctx.gpr[3] = 1u;
    ctx.gpr[4] = entry + 0x20u;  // the delay slot of the bne
    if (kind != 1u) return;
    const std::uint32_t value = call_set(rt, ctx, 0x088970E0u, {entry + 0x20u});
    ctx.gpr[16] = value;  // the delay slot of the next jal
    const std::uint32_t wide = call_set(rt, ctx, 0x0889854Cu, {self, key});
    ctx.gpr[7] = wide;
    (void)call_set(rt, ctx, 0x088984E4u, {word(rt, self + 0x40u), key, value, wide});
}

// func_0880C9B8(out, a1, count, args): count <= 0 gives v0 = 0 and ends with func_08A85B1C(sp + 4, 0); otherwise func_0880B8F8(sp + 8); o1 = args[0] and o2 = args[1] (null: func_08A5BB7C(D_08C98418) first) with their virtual slot 0xC called as (sp + 0xC, o1) and (sp + 0x10, o2); o3 = args[2] (same null check) with the virtual slot 0x54
// called as (o3) leaves a float in f0 (kept in $f20, which the function restores); u = sp[8] (null: func_08A5BB7C(D_08C9848C) first); func_0880BB94(u[0xE8], sp + 0xC, sp + 0x10) with f12 = that float; the string data sp[0x10] then sp[0xC] are released (func_08B60804(data - 4, -1), func_08B8542C(data - 0xC, sp + 1 resp. sp) when not positive; nothing for the shared empty cell D_08D74FD0 + 0xC);
// func_08A5B9EC(sp + 8) and v0 = 1. Then func_08A85B1C(sp + 4, v0), the holder sp + 4 is copied to `out` with func_08A5BAD8(out, sp[4]) unless it is `out`, and func_08A5B9EC(sp + 4) ends the function.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, then checked against the asm by hand, reading the branch TARGETS): count <= 0 (0x0880C9E4: v0 = 0 and func_08A85B1C(sp + 4, 0)), the null checks of args[0..2] (0x0880C9FC, 0x0880CA2C, 0x0880CA5C; D_08C98418) and of u (0x0880CA84; D_08C9848C) and `out` equal to sp + 4 (0x0880CB28) never occurred.
void func_0880C9B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    SavedRegs saved(ctx);
    SavedFloat saved_float(ctx, 20u);
    const std::uint32_t p0 = frame.at(0u), p1 = frame.at(1u), p4 = frame.at(4u), p8 = frame.at(8u), pC = frame.at(0xCu), p10 = frame.at(0x10u);
    ctx.gpr[2] = 0u;
    ctx.gpr[19] = out;
    ctx.gpr[17] = args;
    if (signed_of(count) > 0) {
        ctx.gpr[21] = p8;
        (void)call_set(rt, ctx, 0x0880B8F8u, {p8});
        const auto virtual_call = [&](std::uint32_t offset, std::uint32_t slot_offset, std::uint32_t destination, bool with_destination) -> std::uint32_t {
            const std::uint32_t object = word(rt, args + offset);
            ctx.gpr[16] = object;
            if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98418u});
            const std::uint32_t vtable = word(rt, object);
            const std::uint32_t slot = word(rt, vtable + slot_offset);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = slot;
            if (with_destination) {
                const GuestResult result = call_keep(rt, ctx, slot, {destination, object}, {});
                return result.v0;
            }
            const GuestResult result = call_keep(rt, ctx, slot, {object}, {});
            ctx.fpr[20] = result.f0;
            return result.v0;
        };
        ctx.gpr[20] = pC;
        (void)virtual_call(0u, 0xCu, pC, true);
        ctx.gpr[18] = p10;
        (void)virtual_call(4u, 0xCu, p10, true);
        (void)virtual_call(8u, 0x54u, 0u, false);
        const std::uint32_t unit = word(rt, p8);
        ctx.gpr[16] = unit;
        if (unit == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9848Cu});
        ctx.gpr[17] = 0x08D74FD0u;
        const float scale = ctx.fpr[20];
        ctx.fpr[12] = scale;
        (void)call_keep(rt, ctx, 0x0880BB94u, {word(rt, unit + 0xE8u), pC, p10}, {scale});
        release_cell(rt, ctx, word(rt, p10), p1, 16u);
        ctx.gpr[17] = 0x08D74FD0u;
        release_cell(rt, ctx, word(rt, pC), p0, 16u);
        (void)call_set(rt, ctx, 0x08A5B9ECu, {p8});
        ctx.gpr[2] = 1u;
    }
    ctx.gpr[16] = p4;
    (void)call_set(rt, ctx, 0x08A85B1Cu, {p4, ctx.gpr[2]});
    ctx.gpr[5] = word(rt, p4);  // the delay slot of the beq
    if (out != p4) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, p4)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {p4});
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------

}  // namespace
}  // namespace hot_batch_91
}  // namespace gt::game

#define HOT91(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_91::name, gt::ReturnKind::kind)
#define HOT91N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_91::name, gt::ReturnKind::kind)
HOT91(0x0882222Cu, func_0882222C, Word);
HOT91(0x0885C5C0u, func_0885C5C0, Word);
HOT91(0x0889D738u, func_0889D738, Word);
// Not registered (every case stops on 'kernel isolation (LwMutex contention)': 17 not comparable cases in the fast gate, compared 0): HOT91(0x089B439Cu, func_089B439C, Word);
HOT91(0x089CB164u, func_089CB164, Word);
HOT91(0x08A9CB38u, func_08A9CB38, Word);
HOT91(0x08AD6AB8u, func_08AD6AB8, Word);
HOT91(0x08B166F8u, func_08B166F8, Word);
HOT91(0x08B6F37Cu, func_08B6F37C, DoubleWord);
HOT91(0x08B20A78u, func_08B20A78, Word);
HOT91(0x089BDD18u, func_089BDD18, Word);
HOT91(0x08C20540u, func_08C20540, Word);
HOT91(0x08A54830u, func_08A54830, Word);
HOT91(0x08C34FE8u, func_08C34FE8, Word);
HOT91(0x08A46DCCu, func_08A46DCC, Word);
HOT91(0x08A9FFD0u, func_08A9FFD0, Word);
HOT91(0x08991490u, func_08991490, Word);
HOT91(0x08A206E8u, func_08A206E8, Word);
HOT91(0x0896DFCCu, func_0896DFCC, Word);
HOT91(0x08A42E10u, func_08A42E10, Word);
HOT91(0x08A32678u, func_08A32678, Word);
HOT91(0x08A4BBC0u, func_08A4BBC0, Word);
HOT91(0x08A4412Cu, func_08A4412C, Word);
HOT91(0x08A3C544u, func_08A3C544, Word);
HOT91(0x08A4A960u, func_08A4A960, Word);
HOT91(0x08A4BA50u, func_08A4BA50, Word);
HOT91(0x08A10468u, func_08A10468, Word);
HOT91(0x089A44F0u, func_089A44F0, Word);
HOT91(0x08B9A5C4u, func_08B9A5C4, Word);
HOT91(0x0894D484u, func_0894D484, Word);
HOT91(0x08988BB8u, func_08988BB8, Word);
HOT91(0x08A6CFC0u, func_08A6CFC0, Word);
HOT91(0x08C22B54u, func_08C22B54, Word);
HOT91(0x08C1ABE0u, func_08C1ABE0, Word);
HOT91(0x08C25730u, func_08C25730, Word);
HOT91(0x08AA6B80u, func_08AA6B80, Word);
HOT91(0x08B8AD88u, func_08B8AD88, Word);
HOT91(0x08BFBB38u, func_08BFBB38, Word);
HOT91(0x08C287ACu, func_08C287AC, Word);
HOT91(0x0880602Cu, func_0880602C, Word);
HOT91(0x08A235F8u, func_08A235F8, Word);  // re-registered in batch 103: the import that blocked it became recordable (FINDINGS "Batch 103")
// Not registered (all 10 cases stop on 'kernel isolation (sleep would block)', compared 0): HOT91(0x08A243FCu, func_08A243FC, Word);
HOT91(0x08A6F704u, func_08A6F704, Word);
// Not registered (all 12 cases stop on the import stub sceIoRead (not comparable), compared 0): HOT91(0x089CA118u, func_089CA118, Word);
HOT91(0x08A954B8u, func_08A954B8, Word);
HOT91(0x089B6408u, func_089B6408, Word);
HOT91(0x08826C50u, func_08826C50, Word);
HOT91(0x0899CE20u, func_0899CE20, Word);
HOT91(0x0881D004u, func_0881D004, Word);
// Not registered (calls 0 on all five routes of the fast gate although the batch-89 probe counted 12 hits: the original is only called from code the dispatcher does not see, so nothing is compared): HOT91(0x08B666E8u, func_08B666E8, DoubleWord);
HOT91(0x08898C10u, func_08898C10, Word);
HOT91(0x0880C9B8u, func_0880C9B8, Word);
