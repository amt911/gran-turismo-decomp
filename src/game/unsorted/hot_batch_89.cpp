// Hot batch 89: reached functions that no earlier batch has written, from the call probes of batch 78 and of this batch (the same selection as batches 85-88, widened: see below).
// (main includes batch 88, 6,114 functions.) Before reading any function its address was searched for in src/ (main and the open PR branches: `gh pr list` shows only the draft spec PR #214, no code).
//
// The batch-78 probe (every function of up to 70 instructions not yet decompiled, five routes, --decomp off) reached 1,177 functions; of those, 48 of 10-70 instructions were still absent from src/, and 23 of the 48 have VFPU (9) or call the import
// stubs from 0x08C70000 on (14). The pool of that selection was therefore used up with 25 candidates, and a new probe (`tools.decomp_gate probe b89p`, the same five routes, --decomp off) was run over the 1,403 not-yet-done functions of 71-120
// instructions: 281 were reached, 264 of them without VFPU, import-stub calls or register jumps. Their shape clusters (`tools.shapes` signature) hold two big families, both of the SAME 75-instruction function in two register allocations - a table-driven
// registration function (a holder is built, an object is allocated, and each {name, value} pair of a table in .data goes to func_08B86E98; each member is called once per route) - 46 + 10 members, with no member decompiled before.
//
//  - register_table (56 functions: variant b func_08858F4C and 45 more at 0x0885xxxx / 0x08A2xxxx / 0x08A4xxxx, variant a func_08858E20 and 9 more): one template with the constants of each member in its table row (text, table begin and end, assertion string),
//    decoded from the member's own relocated words; `tools.family_members --check` re-reads the 58 rows of this file (56 here and the 2 `float_property` rows) against the words: 58 match, 0 bad, 0 unparsed;
//  - three "rebind child" functions (func_0886E9FC, func_088702AC, func_088713A4: a child object of a holder is released and replaced, then a tail call into the child's virtual slot 0x28), sharing `bind_child`;
//  - func_0887D4EC (two reference-counted objects let go, ll / sc counters read as plain load and store), func_088DAEB0 (an object stored and wired by three virtual calls, then a tail call), func_088A1854 (a virtual call, two guest calls with four floats and a tail
//    call), func_0888B4E0 (two loops over a halfword table), func_088D5ED0 (a byte-swapped 64-bit pair, Allegrex wsbw / wsbh), and the script property natives func_0898CC60 (getter / setter of a float) and `float_property` (func_089A0F88, func_089A1084).
//
// Read but left for a later batch (float-heavy or with tangled control flow, not yet written): func_08877354, func_0887871C, func_088DE24C, func_0890778C, func_089191DC, func_0892063C, func_089281E4, func_08951304, func_0896B4D4, func_08AC800C,
// func_08AF8AB0, func_08B1F2B8, func_08C053E8. Written but not registered: func_08B0A458 (no compared case changed memory, see its comment).
//
// Compared on their main path but with a side path read from the asm only (each comment lists what the cases did not take): the null registrar / class object of register_table, the second object of func_0887D4EC, the release path of the three rebind
// functions (and the whole creation path of func_088713A4), the getters / setters that the routes did not call (func_0898CC60 setter, `float_property` getter), func_088D5ED0 with a non-zero flag. Every function was compared at least once; the
// 56 table functions once each per route (one case, which changed memory).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest copies from ctx. Every call goes through `call_keep` (the callee's v0,
// v1 and f0 stay in the registers like the original's), and the callee-saved registers (s0-s7, fp) hold the original's values around the calls (`SavedRegs` restores the entry values on the way out, like the original's epilogue; `SavedFloat` does the same for $f20):
// a callee may store the caller's s0 (FINDINGS, batch 84). The delay slot of a `jal` / `jalr` runs BEFORE the call, and a `bnel` delay slot only when the branch is taken. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on
// every path, including the early returns; the other caller-saved registers are set to the original's value where it is cheap and a callee could read them, not otherwise (the gate compares v0, s0-s7, sp, ra and RAM).
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_89 {
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
inline std::int16_t half_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Table-driven registration functions (75 instructions, two register allocations of one function, 46 + 10 members; a reading of the shape, the classes they belong to are not named).
//
// register_table(text, begin, end, assertion) with a0 = `self` (a pointer to the pointer to the class object):
//   func_08A69B3C(frame0, text) builds a holder in frame0 from `text`; block = func_08A5BC20(0x38, 4) (an allocation of 0x38 bytes, alignment 4); func_08A7E914(block, frame0); the block pointer is stored at frame4 and
//   func_08A76AB4(frame0xC, frame4) makes the registrar object frame0xC from it; func_08A69D68(frame0) destroys the first holder. Then for each 8-byte entry {name, value} of the table
//   [begin, end) (at least once: the loop test is at its end): m = the word at frame0xC (null: func_08A5BB7C(assertion) first); func_08A85B1C(frame8, value) builds a temporary, func_08B86E98(m, name, frame8)
//   registers the pair and func_08A5B9EC(frame8) destroys the temporary. After the loop c = *self (null: func_08A5BB7C(assertion) first), func_08A7CE50(c, frame0xC) hands the registrar over
//   and func_08A5B9EC(frame0xC) destroys it. v0 is that last call's. `assertion` is also the string whose address the original builds in two halves (lui into a saved register, then the addiu in the
//   delay slot of the test).
// The two variants differ only in which saved registers hold what (variant a: s4 = frame0xC, s5 = the high half of the assertion address, s6 = the table end; variant b: s6 = frame0xC,
// s4 = the high half, s5 = the end); s2 = the cursor, s3 = frame8, s7 = self and s1 = the registrar's object (m) in both. A callee may read the caller's saved registers (FINDINGS, batch 84), so they hold
// the original's values around every call.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the null registrar object m (the assertion call, 0x08858EB8 in variant a) and the null class
// object c (0x08858F04) never occurred in a compared case.
inline void register_table(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, bool variant_b, std::uint32_t text, std::uint32_t begin, std::uint32_t end, std::uint32_t assertion) {
    const unsigned high_register = variant_b ? 20u : 21u, registrar_register = variant_b ? 22u : 20u, end_register = variant_b ? 21u : 22u;
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegs saved(ctx);
    const std::uint32_t holder = frame.at(0u), block_slot = frame.at(4u), temporary = frame.at(8u), registrar = frame.at(0xCu);
    // %hi of the assertion address (the addiu that follows has the signed %lo).
    const std::uint32_t high = (assertion + ((assertion & 0x8000u) != 0u ? 0x10000u : 0u)) & 0xFFFF0000u;
    ctx.gpr[5] = text;
    ctx.gpr[23] = self;
    ctx.gpr[4] = holder;
    ctx.gpr[registrar_register] = registrar;
    ctx.gpr[high_register] = high;
    ctx.gpr[19] = temporary;
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {holder, text});
    ctx.gpr[4] = 0x38u;
    ctx.gpr[5] = 4u;
    const std::uint32_t block = call_keep(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[4] = block;
    ctx.gpr[5] = holder;
    ctx.gpr[16] = block;
    (void)call_keep(rt, ctx, 0x08A7E914u, {block, holder});
    ctx.gpr[5] = block_slot;
    ctx.gpr[4] = registrar;
    put(rt, block_slot, block);  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x08A76AB4u, {registrar, block_slot});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A69D68u, {holder});
    ctx.gpr[18] = begin;
    ctx.gpr[end_register] = end;
    std::uint32_t cursor = begin;
    std::uint32_t object = word(rt, registrar);
    ctx.gpr[17] = object;
    do {
        ctx.gpr[4] = assertion;  // the delay slot of the test: it runs on both paths
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {assertion});
        const std::uint32_t name = word(rt, cursor), value = word(rt, cursor + 4u);
        ctx.gpr[16] = name;
        ctx.gpr[5] = value;
        ctx.gpr[4] = temporary;
        cursor += 8u;
        ctx.gpr[18] = cursor;  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x08A85B1Cu, {temporary, value});
        ctx.gpr[4] = object;
        ctx.gpr[5] = name;
        ctx.gpr[6] = temporary;
        (void)call_keep(rt, ctx, 0x08B86E98u, {object, name, temporary});
        ctx.gpr[4] = temporary;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {temporary});
        object = word(rt, registrar);  // the delay slot of the loop branch: it runs on both paths
        ctx.gpr[17] = object;
    } while (cursor != end);
    const std::uint32_t target = word(rt, self);
    ctx.gpr[16] = target;
    if (target == 0u) {
        ctx.gpr[4] = assertion;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {assertion});
    }
    ctx.gpr[4] = target;
    ctx.gpr[5] = registrar;  // the delay slot of the jal: it runs before the call
    (void)call_keep(rt, ctx, 0x08A7CE50u, {target, registrar});
    ctx.gpr[4] = registrar;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {registrar});
}
inline void register_table_a(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t text, std::uint32_t begin, std::uint32_t end, std::uint32_t assertion) {
    register_table(rt, ctx, false, text, begin, end, assertion);
}
inline void register_table_b(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t text, std::uint32_t begin, std::uint32_t end, std::uint32_t assertion) {
    register_table(rt, ctx, true, text, begin, end, assertion);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Class registration functions, variant b (46: the template func_08858F4C and 45 more), arguments: the text, the table begin and end, the assertion string.
void func_08858F4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9E684u, 0x08CDC628u, 0x08CDC650u, 0x08C9E690u); }
void func_088591A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9E8C8u, 0x08CDC660u, 0x08CDC7C8u, 0x08C9E8D4u); }
void func_088592D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9E914u, 0x08CDC7C8u, 0x08CDC7F0u, 0x08C9E8D4u); }
void func_088593FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9E9A4u, 0x08CDC7F0u, 0x08CDC890u, 0x08C9E8D4u); }
void func_08859528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9E9D8u, 0x08CDC890u, 0x08CDC8A8u, 0x08C9E9ECu); }
void func_08859780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EAE0u, 0x08CDC8B8u, 0x08CDC938u, 0x08C9EAECu); }
void func_088598AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EBD4u, 0x08CDC938u, 0x08CDC9B0u, 0x08C9EAECu); }
void func_088599D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EBF8u, 0x08CDC9B0u, 0x08CDC9D8u, 0x08C9EAECu); }
void func_08859B04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EC30u, 0x08CDC9D8u, 0x08CDCA28u, 0x08C9EAECu); }
void func_08859C30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EC70u, 0x08CDCA28u, 0x08CDCA48u, 0x08C9EC84u); }
void func_08859D5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9ED20u, 0x08CDCA48u, 0x08CDCAB0u, 0x08C9EC84u); }
void func_08859E88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EE54u, 0x08CDCAB0u, 0x08CDCBA8u, 0x08C9EC84u); }
void func_08859FB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EE88u, 0x08CDCBA8u, 0x08CDCBC8u, 0x08C9EC84u); }
void func_0885A0E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EEACu, 0x08CDCBC8u, 0x08CDCBF8u, 0x08C9EC84u); }
void func_0885A20C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EED0u, 0x08CDCBF8u, 0x08CDCC18u, 0x08C9EC84u); }
void func_0885A338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EF10u, 0x08CDCC18u, 0x08CDCC48u, 0x08C9EF18u); }
void func_0885A464(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EF50u, 0x08CDCC48u, 0x08CDCC68u, 0x08C9EF5Cu); }
void func_0885A590(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9EF8Cu, 0x08CDCC68u, 0x08CDCC80u, 0x08C9EFA0u); }
void func_0885A7E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F028u, 0x08CDCC98u, 0x08CDCCC0u, 0x08C9EFE8u); }
void func_0885A914(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F078u, 0x08CDCCC0u, 0x08CDCCE8u, 0x08C9EFE8u); }
void func_0885AA40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F130u, 0x08CDCCE8u, 0x08CDCD60u, 0x08C9EFE8u); }
void func_0885AB6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F154u, 0x08CDCD60u, 0x08CDCD80u, 0x08C9EFE8u); }
void func_0885ADC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F2A8u, 0x08CDCD90u, 0x08CDCEC8u, 0x08C9EFE8u); }
void func_0885B01C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F32Cu, 0x08CDCEE8u, 0x08CDCF10u, 0x08C9EFE8u); }
void func_0885B148(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F3D0u, 0x08CDCF10u, 0x08CDCF68u, 0x08C9EFE8u); }
void func_0885B3A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F440u, 0x08CDCF78u, 0x08CDCF90u, 0x08C9F450u); }
void func_0885B4CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F470u, 0x08CDCF90u, 0x08CDCFB0u, 0x08C9F450u); }
void func_0885B5F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F4E4u, 0x08CDCFB0u, 0x08CDD000u, 0x08C9F450u); }
void func_0885B724(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F538u, 0x08CDD000u, 0x08CDD028u, 0x08C9F450u); }
void func_0885B850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08C9F568u, 0x08CDD028u, 0x08CDD048u, 0x08C9F450u); }
void func_08A2A5F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CABCD8u, 0x08CDEC04u, 0x08CDEC44u, 0x08CABCECu); }
void func_08A2A950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CABF4Cu, 0x08CDEC5Cu, 0x08CDECFCu, 0x08CABF60u); }
void func_08A45404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CADEF0u, 0x08CDED48u, 0x08CDED60u, 0x08CADF00u); }
void func_08A45530(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CADF30u, 0x08CDED60u, 0x08CDED78u, 0x08CADF44u); }
void func_08A4565C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CADF88u, 0x08CDED78u, 0x08CDED98u, 0x08CADF44u); }
void func_08A458B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE02Cu, 0x08CDEDA8u, 0x08CDEDD8u, 0x08CAE040u); }
void func_08A459E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE068u, 0x08CDEDD8u, 0x08CDEDF8u, 0x08CAE040u); }
void func_08A45C38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE0D0u, 0x08CDEE08u, 0x08CDEE40u, 0x08CAE040u); }
void func_08A45D64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE10Cu, 0x08CDEE40u, 0x08CDEE58u, 0x08CAE040u); }
void func_08A45FBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE174u, 0x08CDEE68u, 0x08CDEE98u, 0x08CAE040u); }
void func_08A460E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE1A4u, 0x08CDEE98u, 0x08CDEEB8u, 0x08CAE040u); }
void func_08A46214(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE1D8u, 0x08CDEEB8u, 0x08CDEED0u, 0x08CAE040u); }
void func_08A46340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE234u, 0x08CDEED0u, 0x08CDEF40u, 0x08CAE240u); }
void func_08A4691C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE680u, 0x08CDF038u, 0x08CDF190u, 0x08CAE370u); }
void func_08A46A48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAE90Cu, 0x08CDF190u, 0x08CDF3E8u, 0x08CAE370u); }
void func_08A46CA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_b(rt, ctx, 0x08CAEA64u, 0x08CDF3F8u, 0x08CDF4A8u, 0x08CAEA70u); }

// Class registration functions, variant a (10: the template func_08858E20 and 9 more).
void func_08858E20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08C9E618u, 0x08CDC618u, 0x08CDC628u, 0x08C9E62Cu); }
void func_08859078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08C9E6B8u, 0x08CDC650u, 0x08CDC660u, 0x08C9E6CCu); }
void func_08859654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08C9EA08u, 0x08CDC8A8u, 0x08CDC8B8u, 0x08C9E9ECu); }
void func_0885AC98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08C9F174u, 0x08CDCD80u, 0x08CDCD90u, 0x08C9EFE8u); }
void func_0885B274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08C9F3F0u, 0x08CDCF68u, 0x08CDCF78u, 0x08C9EFE8u); }
void func_08A2A824(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08CABD3Cu, 0x08CDEC4Cu, 0x08CDEC5Cu, 0x08CABCECu); }
void func_08A45788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08CADFCCu, 0x08CDED98u, 0x08CDEDA8u, 0x08CADFE0u); }
void func_08A45B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08CAE088u, 0x08CDEDF8u, 0x08CDEE08u, 0x08CAE040u); }
void func_08A45E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08CAE128u, 0x08CDEE58u, 0x08CDEE68u, 0x08CAE040u); }
void func_08A46B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_table_a(rt, ctx, 0x08CAE940u, 0x08CDF3E8u, 0x08CDF3F8u, 0x08CAE954u); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// Where a function that ends with a tail call goes: the target (0 when the function simply returns), the object it is called on and the third argument register it leaves.
struct TailTarget {
    std::uint32_t target = 0u, object = 0u, holder = 0u;
};

// The part that func_0886E9FC, func_088702AC and func_088713A4 share once the new child is stored: with h = self[`container`] and c = h[`slot`] (v0 = h; a0 = `index` in the delay slot of the test): a null c returns. Otherwise
// the id = func_0887BD94(`index`) is passed to the virtual slot 8 of c (that slot was loaded into s0 in the delay slot of the jal), c's slot 0x14 is called with (c, self), then, with h and c read again, c's slot 0x18 with
// (c, `text`), and the function ends with a tail call to the virtual slot 0x28 of the child read once more (a0 = that child, a1 = `text`, a2 = h of the second read; v0 = its vtable, v1 = the third h).
inline TailTarget bind_child(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self, std::uint32_t container, std::uint32_t slot, std::uint32_t index, std::uint32_t text) {
    const std::uint32_t holder = word(rt, self + container);
    ctx.gpr[2] = holder;
    const std::uint32_t child = word(rt, holder + slot);
    ctx.gpr[17] = child;
    ctx.gpr[4] = index;
    if (child == 0u) return {};
    const std::uint32_t vtable = word(rt, child);
    ctx.gpr[2] = vtable;
    const std::uint32_t function = word(rt, vtable + 8u);
    ctx.gpr[16] = function;
    const std::uint32_t id = call_keep(rt, ctx, 0x0887BD94u, {index});
    ctx.gpr[5] = id;
    ctx.gpr[4] = child;
    (void)call_keep(rt, ctx, function, {child, id});
    const std::uint32_t second_holder = word(rt, self + container);
    ctx.gpr[2] = second_holder;
    const std::uint32_t second = word(rt, second_holder + slot);
    const std::uint32_t second_vtable = word(rt, second), second_function = word(rt, second_vtable + 0x14u);
    ctx.gpr[4] = second;
    ctx.gpr[3] = second_vtable;
    ctx.gpr[2] = second_function;
    ctx.gpr[5] = self;  // the delay slot of the jalr
    (void)call_keep(rt, ctx, second_function, {second, self});
    const std::uint32_t third_holder = word(rt, self + container);
    ctx.gpr[6] = third_holder;
    ctx.gpr[5] = text;
    const std::uint32_t third = word(rt, third_holder + slot);
    const std::uint32_t third_vtable = word(rt, third), third_function = word(rt, third_vtable + 0x18u);
    ctx.gpr[4] = third;
    ctx.gpr[3] = third_vtable;
    ctx.gpr[2] = third_function;
    (void)call_keep(rt, ctx, third_function, {third, text});
    const std::uint32_t last_holder = word(rt, self + container);
    ctx.gpr[3] = last_holder;
    const std::uint32_t last = word(rt, last_holder + slot);
    ctx.gpr[4] = last;
    const std::uint32_t last_vtable = word(rt, last);
    ctx.gpr[2] = last_vtable;
    const std::uint32_t target = word(rt, last_vtable + 0x28u);
    ctx.gpr[25] = target;
    return {target, last, third_holder};
}

// func_0886E9FC(this, flag, object): func_0886A8B8(this, flag, object) first; a non-zero flag returns that call's v0. Otherwise the child c = this[0x70][0x78], when set, has its virtual slot 0x2C called (c) and is cleared; a null `object`
// returns; else this[0x70][0x78] = object's virtual slot 0x18 (answer of the call (object)) and bind_child(this, 0x70, 0x78, 0x12, D_08B95638) finishes (the tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the release of an existing child (this[0x70][0x78] non-null: the virtual slot 0x2C call and the clearing store, 0x0886EA38-0x0886EA4C) never ran: the slot was empty in every compared case. A non-zero flag, a null object and a new child (the virtual slot 0x18 call, bind_child and the tail call) were compared.
void func_0886E9FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5], object = ctx.gpr[6];
    TailTarget tail;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = self;
        ctx.gpr[16] = flag;
        ctx.gpr[17] = object;
        (void)call_keep(rt, ctx, 0x0886A8B8u, {self, flag, object});
        if (flag != 0u) return;
        const std::uint32_t current = word(rt, word(rt, self + 0x70u) + 0x78u);
        ctx.gpr[2] = current;
        ctx.gpr[4] = current;
        if (current != 0u) {
            const std::uint32_t vtable = word(rt, current), function = word(rt, vtable + 0x2Cu);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {current});
            const std::uint32_t holder = word(rt, self + 0x70u);
            ctx.gpr[2] = holder;
            put(rt, holder + 0x78u, 0u);
        }
        ctx.gpr[4] = object;  // the delay slot of the test: it runs on both paths
        if (object == 0u) return;
        const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 0x18u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        const std::uint32_t holder = word(rt, self + 0x70u);
        ctx.gpr[16] = holder;  // the delay slot of the jalr: it runs before the call
        const std::uint32_t created = call_keep(rt, ctx, function, {object});
        put(rt, holder + 0x78u, created);
        tail = bind_child(rt, ctx, self, 0x70u, 0x78u, 0x12u, 0x08B95638u);
    }
    if (tail.target != 0u) tail_call(ctx, tail.target, {tail.object, 0x08B95638u, tail.holder});
}

// func_088702AC(this, flag, object): a non-zero flag returns at once (v0 untouched). Otherwise the child c = this[0xC][0x80], when set, has its virtual slot 0x2C called (c) and is cleared; a null `object` returns; else
// this[0xC][0x80] = object's virtual slot 0x18 (answer of the call (object)) and bind_child(this, 0xC, 0x80, 0xD, D_08B964C4) finishes (the tail call).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the release of an existing child (this[0xC][0x80] non-null: the virtual slot 0x2C call and the clearing store, 0x088702DC-0x088702F0) never ran: the slot was empty in every compared case. A non-zero flag, a null object and a new child (the virtual slot 0x18 call, bind_child and the tail call) were compared.
void func_088702AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5], object = ctx.gpr[6];
    TailTarget tail;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = self;
        ctx.gpr[16] = object;
        if (flag != 0u) return;
        const std::uint32_t current = word(rt, word(rt, self + 0xCu) + 0x80u);
        ctx.gpr[2] = current;
        ctx.gpr[4] = current;
        if (current != 0u) {
            const std::uint32_t vtable = word(rt, current), function = word(rt, vtable + 0x2Cu);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {current});
            const std::uint32_t holder = word(rt, self + 0xCu);
            ctx.gpr[2] = holder;
            put(rt, holder + 0x80u, 0u);
        }
        ctx.gpr[4] = object;  // the delay slot of the test: it runs on both paths
        if (object == 0u) return;
        const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 0x18u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        const std::uint32_t holder = word(rt, self + 0xCu);
        ctx.gpr[16] = holder;  // the delay slot of the jalr: it runs before the call
        const std::uint32_t created = call_keep(rt, ctx, function, {object});
        put(rt, holder + 0x80u, created);
        tail = bind_child(rt, ctx, self, 0xCu, 0x80u, 0xDu, 0x08B964C4u);
    }
    if (tail.target != 0u) tail_call(ctx, tail.target, {tail.object, 0x08B964C4u, tail.holder});
}

// func_088713A4(this, flag, object): the same with the child stored at the first word of the holder this[0xC] (no field offset): a non-zero flag returns at once (v0 untouched); the child c = this[0xC][0] (s0 = this[0xC]), when set, has
// its virtual slot 0x2C called (c) and is cleared (s0 read again); then this[0xC][0] = object's virtual slot 0x18 (answer of the call (object); v0 = 0 when object is null, the delay slot of the test sets it before it) and
// bind_child(this, 0xC, 0, 5, D_08B96774) finishes (a null child returns with v0 = this[0xC]).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-null `object` (the virtual slot 0x18 call and the store of its answer, 0x08871404-0x08871414) never occurred, so bind_child only ever saw a null child and returned (v0 = this[0xC]); the part behind a non-null child (0x08871424-0x08871498, the tail call) was compared only through func_0886E9FC and func_088702AC, which share it with other offsets, and is read from this function's asm only. The release of an existing child (0x088713D0-0x088713F4) and a non-zero flag were compared.
void func_088713A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5], object = ctx.gpr[6];
    TailTarget tail;
    {
        GuestFrame frame(ctx, 0x20u);
        SavedRegs saved(ctx);
        ctx.gpr[18] = self;
        ctx.gpr[17] = object;
        if (flag != 0u) return;
        std::uint32_t container = word(rt, self + 0xCu);
        ctx.gpr[16] = container;
        ctx.gpr[19] = self + 0xCu;
        const std::uint32_t current = word(rt, container);
        ctx.gpr[2] = current;
        ctx.gpr[4] = current;
        if (current != 0u) {
            const std::uint32_t vtable = word(rt, current), function = word(rt, vtable + 0x2Cu);
            ctx.gpr[2] = vtable;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {current});
            const std::uint32_t holder = word(rt, self + 0xCu);
            ctx.gpr[2] = holder;
            put(rt, holder, 0u);
            container = word(rt, self + 0xCu);
            ctx.gpr[16] = container;
        }
        ctx.gpr[4] = object;  // the delay slot of the test: it runs on both paths
        ctx.gpr[2] = 0u;      // ... and so does `v0 = 0`
        std::uint32_t created = 0u;
        if (object != 0u) {
            const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 0x18u);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            created = call_keep(rt, ctx, function, {object});
        }
        put(rt, container, created);
        tail = bind_child(rt, ctx, self, 0xCu, 0u, 5u, 0x08B96774u);
    }
    if (tail.target != 0u) tail_call(ctx, tail.target, {tail.object, 0x08B96774u, tail.holder});
}

// The ll / sc decrement of a counter (one thread: a plain load and store): the old value is left in a0, v0 = 1 (the sc's success flag, which the delay slot of its retry branch re-sets) and v1 the address.
inline std::uint32_t atomic_decrement(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}

// func_0887D4EC(this): func_0887D3BC(this) first; then two reference-counted objects, o1 = this[0x5C] and o2 = this[0x54], are let go the same way: the strong counter at +4 is decremented and when it was 1 the virtual slot 8 runs on the object,
// then the weak counter at +8 is decremented and when it was 1 the virtual slot 0xC runs. For o1 both are calls; for o2 the weak step is the tail call (the object's virtual slot 0xC, a0 = o2). A null object is skipped (o1) or
// returns (o2). v0 is 1 after a decrement that did not lead to a call, the call's v0 after one that did, and (a null o2) whatever the earlier code left.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): o2 = this[0x54] was null in every compared case (the return at 0x0887D570), so its half - the strong decrement (0x0887D578-0x0887D58C), the virtual slot 8 (0x0887D594-0x0887D5A0), the weak decrement (0x0887D5A4-0x0887D5BC) and the tail call to the virtual slot 0xC (0x0887D5C4-0x0887D5DC) - and the exits of o1 whose counter was not 1 (the `bnel` at 0x0887D524 and 0x0887D554) are read from the asm only. o1 was never null and both of its counters were always 1, so the slots 8 and 0xC always ran.
void func_0887D4EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t tail_target = 0u, tail_object = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[17] = self;
        (void)call_keep(rt, ctx, 0x0887D3BCu, {self});
        std::uint32_t object = word(rt, self + 0x5Cu);
        ctx.gpr[16] = object;
        ctx.gpr[3] = object + 4u;  // the delay slot of the test: it runs on both paths
        if (object != 0u && atomic_decrement(rt, ctx, object + 4u) == 1u) {
            const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 8u);
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            ctx.gpr[4] = object;
            (void)call_keep(rt, ctx, function, {object});
            if (atomic_decrement(rt, ctx, object + 8u) == 1u) {
                const std::uint32_t weak_vtable = word(rt, object), weak_function = word(rt, weak_vtable + 0xCu);
                ctx.gpr[3] = weak_vtable;
                ctx.gpr[2] = weak_function;
                ctx.gpr[4] = object;
                (void)call_keep(rt, ctx, weak_function, {object});
            }
        }
        object = word(rt, self + 0x54u);
        ctx.gpr[16] = object;
        ctx.gpr[3] = object + 4u;
        if (object == 0u) return;
        if (atomic_decrement(rt, ctx, object + 4u) != 1u) return;
        const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 8u);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = object;
        (void)call_keep(rt, ctx, function, {object});
        if (atomic_decrement(rt, ctx, object + 8u) != 1u) return;
        const std::uint32_t weak_vtable = word(rt, object);
        ctx.gpr[2] = weak_vtable;
        ctx.gpr[4] = object;
        tail_target = word(rt, weak_vtable + 0xCu);
        tail_object = object;
        ctx.gpr[25] = tail_target;
    }
    tail_call(ctx, tail_target, {tail_object});
}

// func_088DAEB0(this, object): this[0x148] = object; a null object returns (v0 = 0). Otherwise the object's virtual slot 0x1C is called with (object, func_088E97B0, this), then (object read again from this[0x148]) slot 0x20 with
// (object, func_088E9804, this) and slot 0x84 with (object). A null this[0x154] returns (v0 = the slot 0x84's answer); otherwise func_088F4AA0(this[0x154]) answers x and the function ends with a tail call to the virtual slot 0x88 of
// the object (read once more) with (object, x). func_088E97B0 and func_088E9804 are code addresses (function entries).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null `object` (the `beqz` at 0x088DAED8, target 0x088DAF5C, v0 = 0) and a null this[0x154] (the `beqz` at 0x088DAF24, target 0x088DAF5C, v0 = the slot 0x84 answer) never occurred: every compared case reached the tail call.
void func_088DAEB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    std::uint32_t tail_target = 0u, tail_object = 0u, tail_argument = 0u;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegs saved(ctx);
        ctx.gpr[16] = self;
        ctx.gpr[2] = object;
        ctx.gpr[6] = self;
        ctx.gpr[4] = object;
        ctx.gpr[5] = 0x088E97B0u;
        put(rt, self + 0x148u, object);  // the delay slot of the test: it runs on both paths
        if (object == 0u) return;
        const std::uint32_t vtable = word(rt, object), function = word(rt, vtable + 0x1Cu);
        ctx.gpr[2] = vtable;
        ctx.gpr[3] = function;
        (void)call_keep(rt, ctx, function, {object, 0x088E97B0u, self});
        const std::uint32_t second = word(rt, self + 0x148u);
        const std::uint32_t second_vtable = word(rt, second), second_function = word(rt, second_vtable + 0x20u);
        ctx.gpr[4] = second;
        ctx.gpr[5] = 0x088E9804u;
        ctx.gpr[3] = second_vtable;
        ctx.gpr[2] = second_function;
        ctx.gpr[6] = self;  // the delay slot of the jalr
        (void)call_keep(rt, ctx, second_function, {second, 0x088E9804u, self});
        const std::uint32_t third = word(rt, self + 0x148u);
        const std::uint32_t third_vtable = word(rt, third), third_function = word(rt, third_vtable + 0x84u);
        ctx.gpr[4] = third;
        ctx.gpr[3] = third_vtable;
        ctx.gpr[2] = third_function;
        (void)call_keep(rt, ctx, third_function, {third});
        const std::uint32_t pending = word(rt, self + 0x154u);
        ctx.gpr[3] = pending;
        ctx.gpr[4] = pending;  // the delay slot of the test: it runs on both paths
        if (pending == 0u) return;
        const std::uint32_t last = word(rt, self + 0x148u);
        ctx.gpr[16] = last;
        const std::uint32_t last_vtable = word(rt, last);
        ctx.gpr[2] = last_vtable;
        const std::uint32_t last_function = word(rt, last_vtable + 0x88u);
        ctx.gpr[18] = last_function;  // s2, loaded in the delay slot of the jal
        const std::uint32_t answer = call_keep(rt, ctx, 0x088F4AA0u, {pending});
        ctx.gpr[5] = answer;
        ctx.gpr[4] = last;
        ctx.gpr[25] = last_function;
        tail_target = last_function;
        tail_object = last;
        tail_argument = answer;
    }
    tail_call(ctx, tail_target, {tail_object, tail_argument});
}

// func_089A0F88(out, arg, mode, third) and func_089A1084 (the same with other callees: getter / setter): a float property of the object o that func_089A0D48(holder, arg, ...) fills in (a null o: func_08A5BB7C(D_08CA4AE0) first).
// Mode 0 (getter): value = getter(o, 0) (a float in f0), func_08B8C5AC(result, value) builds the result holder at sp+8, which func_08A5BAD8(out, [sp+8]) copies to `out` unless it is `out`; both holders are destroyed (func_08A5B9EC).
// Mode 1 (setter): value = func_08B8C5D4(third) (a float), o is looked up with holder sp, setter(o, value) runs and the holder is destroyed. Any other mode: v0 = 1 and nothing else. f20 is saved and holds the value in mode 1.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): only the setter was compared (func_089A0F88 on the garage route, func_089A1084 on the buy route, one case each): the getter (mode 0, 0x089A0FB0-0x089A1010 and 0x089A10AC-0x089A110C), the other modes (v0 = 1, 0x089A1018 and 0x089A1114) and the null-object assertion of the setter (0x089A1048 and 0x089A1144) are read from the asm only.
void float_property(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t getter, std::uint32_t setter) {
    const std::uint32_t out = ctx.gpr[4], arg = ctx.gpr[5], mode = ctx.gpr[6], third = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegs saved(ctx);
    SavedFloat saved_f20(ctx, 20u);
    ctx.gpr[17] = out;
    ctx.gpr[4] = third;
    ctx.gpr[16] = arg;
    if (mode == 0u) {
        const std::uint32_t holder = frame.at(4u);
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x089A0D48u, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        if (object == 0u) {
            ctx.gpr[4] = 0x08CA4AE0u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA4AE0u});
        }
        ctx.gpr[4] = object;
        ctx.gpr[5] = 0u;
        const GuestResult value = call_keep(rt, ctx, getter, {object, 0u}, {});
        ctx.fpr[12] = value.f0;
        const std::uint32_t result = frame.at(8u);
        ctx.gpr[16] = result;
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08B8C5ACu, {result}, {value.f0});
        ctx.gpr[5] = word(rt, result);  // the delay slot of the compare: it runs on both paths
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const GuestResult value = call_keep(rt, ctx, 0x08B8C5D4u, {third}, {});
    ctx.fpr[20] = value.f0;
    const std::uint32_t holder = frame.at(0u);
    ctx.gpr[5] = arg;
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x089A0D48u, {holder, arg});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[16] = object;
    ctx.fpr[12] = value.f0;  // the delay slot of the test: it runs on both paths
    if (object == 0u) {
        ctx.gpr[4] = 0x08CA4AE0u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA4AE0u});
        ctx.fpr[12] = value.f0;
    }
    ctx.gpr[4] = object;
    (void)call_keep(rt, ctx, setter, {object}, {value.f0});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}
void func_089A0F88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_property(rt, ctx, 0x089A0B98u, 0x089A0A40u); }
void func_089A1084(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { float_property(rt, ctx, 0x089A0BB4u, 0x089A0AB4u); }

// func_0898CC60(out, a1, mode, args): mode 0 (getter): o = the object that func_089896CC(sp+4) fills in (null: func_08A5BB7C(D_08CA39F0) first), func_08A87EEC(sp+8, func_089833AC(o + 0xF0)) builds the result holder (the float from
// func_089833AC is the argument in f12), which func_08A5BAD8(out, [sp+8]) copies to `out` unless it is `out`; both holders are destroyed (func_08A5B9EC). Mode 1 (setter): func_089896CC(sp) is asked and its object (null: func_08A5BB7C(D_08CA39F0))
// is only checked; the argument object *args (null: func_08A5BB7C(D_08CA39E8) first) has its virtual slot 0x54 called with (argument) and the holder is destroyed. Any other mode: v0 = 1, nothing else.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): only the getter (mode 0) was compared, one case: the setter (mode 1, 0x0898CCE0-0x0898CD3C), the other modes (v0 = 1, 0x0898CCE0), the null-object assertion of the getter (0x0898CC98) and `out` being the result holder (0x0898CCB8) are read from the asm only.
void func_0898CC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], mode = ctx.gpr[6], args = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    ctx.gpr[17] = out;
    ctx.gpr[16] = args;
    if (mode == 0u) {
        const std::uint32_t holder = frame.at(4u);
        ctx.gpr[18] = holder;
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x089896CCu, {holder});
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[16] = object;
        ctx.gpr[4] = 0x08CA0000u;  // the delay slot of the test: it runs on both paths
        if (object == 0u) {
            ctx.gpr[4] = 0x08CA39F0u;
            (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
        }
        ctx.gpr[4] = object + 0xF0u;
        const GuestResult value = call_keep(rt, ctx, 0x089833ACu, {object + 0xF0u}, {});
        ctx.fpr[12] = value.f0;
        const std::uint32_t result = frame.at(8u);
        ctx.gpr[16] = result;
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A87EECu, {result}, {value.f0});
        ctx.gpr[5] = word(rt, result);  // the delay slot of the compare: it runs on both paths
        if (out != result) (void)call_keep(rt, ctx, 0x08A5BAD8u, {out, word(rt, result)});
        ctx.gpr[4] = result;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {result});
        ctx.gpr[4] = holder;
        (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 1u;
    if (mode != 1u) return;
    const std::uint32_t holder = frame.at(0u);
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x089896CCu, {holder});
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[2] = object;
    if (object == 0u) {
        ctx.gpr[4] = 0x08CA39F0u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
    }
    const std::uint32_t argument = word(rt, args);
    ctx.gpr[16] = argument;
    if (argument == 0u) {
        ctx.gpr[4] = 0x08CA39E8u;
        (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA39E8u});
    }
    const std::uint32_t vtable = word(rt, argument);
    const std::uint32_t function = word(rt, vtable + 0x54u);
    ctx.gpr[3] = vtable;
    ctx.gpr[2] = function;
    ctx.gpr[4] = argument;
    (void)call_keep(rt, ctx, function, {argument});
    ctx.gpr[4] = holder;
    (void)call_keep(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_08B0A458(this, kind): under the lock D_08D473E8 (func_08B344E0 .. func_08B3457C), kind 0 is replaced by the word at D_08CE0A24; c = (*this)[0x110]. With kind 5: a non-null c whose virtual slot 0x18 answers 5 is kept; a non-null c with another
// answer, and any c with another kind, give 0; a null c with kind 5 is created: o = operator new (func_08B62344)(0x20CE0), func_08B451A0(o) and (*this)[0x110] = o (the pointer *this read before the call). v0 = c (0 for the cases above).
// Not registered: the five compared cases (one per route) changed no memory; the only writes, the creation of the child (operator new, func_08B451A0 and the store of this[0x110], 0x08B0A4CC-0x08B0A4F4), were not reached, and the answer of the child's virtual slot 0x18 could not be told from the compare of v0 alone.
void func_08B0A458(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t kind = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegs saved(ctx);
    constexpr std::uint32_t kLock = 0x08D473E8u;
    ctx.gpr[19] = 0x08D40000u;
    ctx.gpr[18] = self;
    ctx.gpr[16] = kind;
    ctx.gpr[4] = kLock;
    (void)call_keep(rt, ctx, 0x08B344E0u, {kLock});
    if (kind == 0u) {
        ctx.gpr[2] = 0x08CE0000u;
        kind = word(rt, 0x08CE0A24u);
        ctx.gpr[16] = kind;
    }
    const std::uint32_t object = word(rt, self);
    ctx.gpr[2] = object;
    ctx.gpr[3] = 5u;
    std::uint32_t child = word(rt, object + 0x110u);
    ctx.gpr[17] = child;
    if (kind == 5u) {
        if (child != 0u) {
            const std::uint32_t vtable = word(rt, child), function = word(rt, vtable + 0x18u);
            ctx.gpr[4] = child;
            ctx.gpr[3] = vtable;
            ctx.gpr[2] = function;
            const std::uint32_t answer = call_keep(rt, ctx, function, {child});
            if (answer != kind) {
                child = 0u;  // the delay slot of the test below clears s1 on both of its paths
                ctx.gpr[17] = child;
            }
        } else {
            ctx.gpr[4] = 0x20CE0u;
            const std::uint32_t holder = word(rt, self);
            ctx.gpr[17] = holder;  // the delay slot of the jal: it runs before the call
            const std::uint32_t created = call_keep(rt, ctx, 0x08B62344u, {0x20CE0u});
            ctx.gpr[16] = created;
            ctx.gpr[4] = created;
            (void)call_keep(rt, ctx, 0x08B451A0u, {created});
            put(rt, holder + 0x110u, created);
            const std::uint32_t again = word(rt, self);
            ctx.gpr[2] = again;
            child = word(rt, again + 0x110u);
            ctx.gpr[17] = child;
        }
    } else {
        ctx.gpr[4] = 0x20CE0u;
        child = 0u;
        ctx.gpr[17] = child;
    }
    ctx.gpr[4] = kLock;
    (void)call_keep(rt, ctx, 0x08B3457Cu, {kLock});
    ctx.gpr[2] = child;
}

// func_088D5ED0(object, a1, lo, hi, flag): b = *object. With a non-zero flag (low byte): b[0x2C] = bswap(lo), b[0x28] = bswap(hi), v0 = b. Otherwise the pair (b[0x28], b[0x2C]) is read back (bswapped) and when it already equals (hi, lo) nothing happens
// (v0 = 0); else b[0x28] = bswap(hi), b[0x2C] = bswap(lo) and the halfword b[0x1A] gets: lo == -1 and hi == -1: 0x0800 (the halfword swap of 8; v0 = b); otherwise the swapped low byte of func_0893067C(func_088D5E80()) (v0, v1 of the first call are
// its arguments; v0 = the swapped byte). bswap is the Allegrex wsbw.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): only calls with a zero flag were compared, so the flag setter (0x088D5EF4-0x088D5F04), the pair that differs from the old one only in its high word and the (-1, -1) halfword case (0x088D5F58-0x088D5F90, the stored 0x0800) are read from the asm only. The no-change return (v0 = 0) and the path through func_088D5E80 / func_0893067C were compared.
void func_088D5ED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], low = ctx.gpr[6], high = ctx.gpr[7], flag = ctx.gpr[8] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    SavedRegs saved(ctx);
    ctx.gpr[8] = flag;
    ctx.gpr[12] = low;
    ctx.gpr[13] = high;
    const std::uint32_t swapped_low = __builtin_bswap32(low);
    ctx.gpr[5] = swapped_low;
    ctx.gpr[16] = self;
    if (flag != 0u) {
        const std::uint32_t block = word(rt, self);
        ctx.gpr[2] = block;
        const std::uint32_t swapped_high = __builtin_bswap32(high);
        ctx.gpr[3] = swapped_high;
        put(rt, block + 0x2Cu, swapped_low);
        put(rt, block + 0x28u, swapped_high);
        return;
    }
    const std::uint32_t block = word(rt, self);
    ctx.gpr[5] = block;
    ctx.gpr[15] = swapped_low;
    ctx.gpr[7] = 0u;
    ctx.gpr[14] = block + 0x28u;
    ctx.gpr[24] = 0xFFFFFFFFu;
    const std::uint32_t old_high = __builtin_bswap32(word(rt, block + 0x28u));
    const std::uint32_t old_low = __builtin_bswap32(word(rt, block + 0x2Cu));
    ctx.gpr[3] = old_high;
    ctx.gpr[6] = old_low;
    ctx.gpr[2] = 0u;
    ctx.gpr[9] = old_high;
    ctx.gpr[8] = old_low;
    if (old_low == low && old_high == high) return;  // v0 = 0
    const std::uint32_t swapped_high = __builtin_bswap32(high);
    ctx.gpr[2] = swapped_high;
    put(rt, block + 0x28u, swapped_high);
    put(rt, block + 0x2Cu, swapped_low);  // the delay slot of the next test: it runs on both paths
    if (low == 0xFFFFFFFFu) {
        ctx.gpr[2] = 8u;
        ctx.gpr[3] = 0x0800u;  // the delay slot of the next test: wsbh(8)
        if (high == 0xFFFFFFFFu) {
            const std::uint32_t outer = word(rt, self);
            ctx.gpr[2] = outer;
            rt.memory().store16(outer + 0x1Au, 0x0800u);
            return;
        }
    }
    const GuestResult pair = call_keep(rt, ctx, 0x088D5E80u, {}, {});
    ctx.gpr[4] = pair.v0;
    ctx.gpr[5] = pair.v1;
    const std::uint32_t outer = word(rt, self);
    ctx.gpr[16] = outer;  // the delay slot of the jal: it runs before the call
    const std::uint32_t byte = call_keep(rt, ctx, 0x0893067Cu, {pair.v0, pair.v1}) & 0xFFu;
    const std::uint32_t swapped_byte = byte << 8u;  // wsbh of a value below 0x100
    ctx.gpr[2] = swapped_byte;
    rt.memory().store16(outer + 0x1Au, static_cast<std::uint16_t>(swapped_byte));
}

// func_0888B4E0(this, base, table, count): for each of the `count` halfwords h of `table` (while the count > 0): w = func_088D6358(base), func_088D2E78(this[4][0x620], h, w), then func_088D089C(func_088DA788(this[4], h)) (this[4] read again before each
// call). Then, for each halfword h again, a request on the stack, {sp[0] = 9, sp[8] = h}, goes to func_088936A4(this[0x18], sp). v0 is the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a count of 0 or less (the return at 0x0888B514) never occurred.
void func_0888B4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], base = ctx.gpr[5], table = ctx.gpr[6], count = ctx.gpr[7];
    GuestFrame frame(ctx, 0xD0u);
    SavedRegs saved(ctx);
    ctx.gpr[23] = table;
    ctx.gpr[22] = base;
    ctx.gpr[21] = count;
    ctx.gpr[19] = self;
    if (signed_of(count) <= 0) return;
    ctx.gpr[18] = table;
    ctx.gpr[20] = 0u;
    std::uint32_t cursor = table, done = 0u;
    do {
        const std::uint32_t pool = word(rt, self + 4u);
        ctx.gpr[2] = pool;
        const std::uint32_t id = half_u(rt, cursor);
        ctx.gpr[17] = id;
        ctx.gpr[4] = base;
        const std::uint32_t head = word(rt, pool + 0x620u);
        ctx.gpr[16] = head;  // the delay slot of the jal: it runs before the call
        const std::uint32_t size = call_keep(rt, ctx, 0x088D6358u, {base});
        ctx.gpr[6] = size;
        ctx.gpr[4] = head;
        ctx.gpr[5] = id;
        (void)call_keep(rt, ctx, 0x088D2E78u, {head, id, size});
        const std::uint32_t second_pool = word(rt, self + 4u);
        ctx.gpr[4] = second_pool;
        ctx.gpr[5] = id;
        ++done;
        ctx.gpr[20] = done;  // the delay slot of the jal: it runs before the call
        const std::uint32_t found = call_keep(rt, ctx, 0x088DA788u, {second_pool, id});
        ctx.gpr[4] = found;
        cursor += 2u;
        ctx.gpr[18] = cursor;  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x088D089Cu, {found});
    } while (count != done);
    ctx.gpr[16] = table;
    ctx.gpr[17] = 0u;
    ctx.gpr[18] = 9u;
    cursor = table;
    done = 0u;
    do {
        const std::uint32_t entry = half_u(rt, cursor);
        ctx.gpr[2] = entry;
        const std::uint32_t target = word(rt, self + 0x18u);
        ctx.gpr[4] = target;
        ++done;
        ctx.gpr[17] = done;
        ctx.gpr[5] = frame.at(0u);
        put(rt, frame.at(8u), entry);
        cursor += 2u;
        ctx.gpr[16] = cursor;
        put(rt, frame.at(0u), 9u);  // the delay slot of the jal: it runs before the call
        (void)call_keep(rt, ctx, 0x088936A4u, {target, frame.at(0u)});
    } while (count != done);
}

// func_088A1854(this, index, position, f12 = distance): the element e = this[8] + 1072 * (halfword this[0x10][index]); when e's virtual slot 0x7C (called with (e)) answers non-zero: the pool p = func_0894CFF8(this[0x54], index) and
// func_0894106C(p, index, -1, -1, 1, f12..f14 = the three floats at *position, f15 = distance), then the function ends with a tail call to e's virtual slot 0x30 with (e, 0). A zero answer returns (v0 = 0).
// f20 holds the distance and is saved.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a zero answer of the virtual slot 0x7C (the `beqz` at 0x088A18BC, target 0x088A1920, v0 = 0) never occurred: every compared case made the calls and the tail call.
void func_088A1854(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], index = ctx.gpr[5], position = ctx.gpr[6];
    const float distance = ctx.fpr[12];
    std::uint32_t tail_target = 0u, tail_element = 0u;
    {
        GuestFrame frame(ctx, 0x30u);
        SavedRegs saved(ctx);
        SavedFloat saved_f20(ctx, 20u);
        ctx.gpr[19] = position;
        ctx.gpr[18] = self;
        ctx.gpr[16] = index;
        ctx.fpr[20] = distance;
        const std::int32_t entry = half_at(rt, word(rt, self + 0x10u) + index * 2u);
        const std::uint32_t array = word(rt, self + 8u);
        ctx.gpr[5] = array;
        const std::uint32_t element = array + static_cast<std::uint32_t>(entry) * 1072u;
        ctx.gpr[17] = element;
        const std::uint32_t vtable = word(rt, element), function = word(rt, vtable + 0x7Cu);
        ctx.gpr[3] = vtable;
        ctx.gpr[2] = function;
        ctx.gpr[4] = element;
        const std::uint32_t answer = call_keep(rt, ctx, function, {element});
        ctx.gpr[5] = index;  // the delay slot of the test: it runs on both paths
        if (answer == 0u) return;
        const std::uint32_t pool = call_keep(rt, ctx, 0x0894CFF8u, {word(rt, self + 0x54u), index});
        const float x = float_at(rt, position), y = float_at(rt, position + 4u), z = float_at(rt, position + 8u);
        ctx.fpr[15] = distance;
        ctx.fpr[14] = z;
        ctx.fpr[12] = x;
        ctx.fpr[13] = y;
        ctx.gpr[4] = pool;
        ctx.gpr[6] = 0xFFFFFFFFu;
        ctx.gpr[7] = 0xFFFFFFFFu;
        (void)call_keep(rt, ctx, 0x0894106Cu, {pool, index, 0xFFFFFFFFu, 0xFFFFFFFFu, 1u}, {x, y, z, distance});
        const std::uint32_t last_vtable = word(rt, element);
        ctx.gpr[3] = last_vtable;
        ctx.gpr[4] = element;
        tail_target = word(rt, last_vtable + 0x30u);
        ctx.gpr[25] = tail_target;
        ctx.gpr[5] = 0u;
        tail_element = element;
    }
    tail_call(ctx, tail_target, {tail_element, 0u});
}


}  // namespace
}  // namespace hot_batch_89
}  // namespace gt::game

#define HOT89(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_89::name, gt::ReturnKind::kind)
#define HOT89N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_89::name, gt::ReturnKind::kind)
HOT89(0x08858F4Cu, func_08858F4C, Word);
HOT89(0x088591A4u, func_088591A4, Word);
HOT89(0x088592D0u, func_088592D0, Word);
HOT89(0x088593FCu, func_088593FC, Word);
HOT89(0x08859528u, func_08859528, Word);
HOT89(0x08859780u, func_08859780, Word);
HOT89(0x088598ACu, func_088598AC, Word);
HOT89(0x088599D8u, func_088599D8, Word);
HOT89(0x08859B04u, func_08859B04, Word);
HOT89(0x08859C30u, func_08859C30, Word);
HOT89(0x08859D5Cu, func_08859D5C, Word);
HOT89(0x08859E88u, func_08859E88, Word);
HOT89(0x08859FB4u, func_08859FB4, Word);
HOT89(0x0885A0E0u, func_0885A0E0, Word);
HOT89(0x0885A20Cu, func_0885A20C, Word);
HOT89(0x0885A338u, func_0885A338, Word);
HOT89(0x0885A464u, func_0885A464, Word);
HOT89(0x0885A590u, func_0885A590, Word);
HOT89(0x0885A7E8u, func_0885A7E8, Word);
HOT89(0x0885A914u, func_0885A914, Word);
HOT89(0x0885AA40u, func_0885AA40, Word);
HOT89(0x0885AB6Cu, func_0885AB6C, Word);
HOT89(0x0885ADC4u, func_0885ADC4, Word);
HOT89(0x0885B01Cu, func_0885B01C, Word);
HOT89(0x0885B148u, func_0885B148, Word);
HOT89(0x0885B3A0u, func_0885B3A0, Word);
HOT89(0x0885B4CCu, func_0885B4CC, Word);
HOT89(0x0885B5F8u, func_0885B5F8, Word);
HOT89(0x0885B724u, func_0885B724, Word);
HOT89(0x0885B850u, func_0885B850, Word);
HOT89(0x08A2A5F8u, func_08A2A5F8, Word);
HOT89(0x08A2A950u, func_08A2A950, Word);
HOT89(0x08A45404u, func_08A45404, Word);
HOT89(0x08A45530u, func_08A45530, Word);
HOT89(0x08A4565Cu, func_08A4565C, Word);
HOT89(0x08A458B4u, func_08A458B4, Word);
HOT89(0x08A459E0u, func_08A459E0, Word);
HOT89(0x08A45C38u, func_08A45C38, Word);
HOT89(0x08A45D64u, func_08A45D64, Word);
HOT89(0x08A45FBCu, func_08A45FBC, Word);
HOT89(0x08A460E8u, func_08A460E8, Word);
HOT89(0x08A46214u, func_08A46214, Word);
HOT89(0x08A46340u, func_08A46340, Word);
HOT89(0x08A4691Cu, func_08A4691C, Word);
HOT89(0x08A46A48u, func_08A46A48, Word);
HOT89(0x08A46CA0u, func_08A46CA0, Word);
HOT89(0x08858E20u, func_08858E20, Word);
HOT89(0x08859078u, func_08859078, Word);
HOT89(0x08859654u, func_08859654, Word);
HOT89(0x0885AC98u, func_0885AC98, Word);
HOT89(0x0885B274u, func_0885B274, Word);
HOT89(0x08A2A824u, func_08A2A824, Word);
HOT89(0x08A45788u, func_08A45788, Word);
HOT89(0x08A45B0Cu, func_08A45B0C, Word);
HOT89(0x08A45E90u, func_08A45E90, Word);
HOT89(0x08A46B74u, func_08A46B74, Word);
HOT89(0x0886E9FCu, func_0886E9FC, Word);
HOT89(0x088702ACu, func_088702AC, Word);
HOT89(0x088713A4u, func_088713A4, Word);
HOT89(0x0887D4ECu, func_0887D4EC, Word);
HOT89(0x088DAEB0u, func_088DAEB0, Word);
HOT89(0x089A0F88u, func_089A0F88, Word);
HOT89(0x089A1084u, func_089A1084, Word);
HOT89(0x0898CC60u, func_0898CC60, Word);
// Not registered (no compared case changed memory): HOT89(0x08B0A458u, func_08B0A458, Word);
HOT89(0x088D5ED0u, func_088D5ED0, Word);
HOT89(0x0888B4E0u, func_0888B4E0, Word);
HOT89(0x088A1854u, func_088A1854, Word);
