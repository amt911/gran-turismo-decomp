// Hot batch 75: the remaining reached members of the shape families that earlier batches (and src/core/, hot_leaves.cpp, organizer.cpp) have a
// template for. The call probe is the one of batches 70-74 (every function of up to 70 instructions not yet decompiled, run on the five routes with
// --decomp off), the reached ones grouped by instruction shape with `tools.family_members` (batches 73 and 74 excluded with --exclude-ref).
// Every member here was compared with its family's template word by word (`tools.family_members`: the words that differ are only the relocated
// constants, a displacement and the jal / j target, never a register or a stack offset; no `!!` line), its constants and call targets come from the
// member's own relocated words, and `--check` re-reads every row of this file against them. Each helper below was read by hand against the asm of
// one member of its family (the generated code was only a reading aid). Each helper's comment lists the branches the compared cases of the fast gate
// do not take; those were compared against the asm by hand, reading the branch TARGET (a value set in a delay slot is the result only if the target
// does not overwrite it). Their classes aren't known yet, so they keep their addresses as names.
//
// Families (the implementation is the one of the template, with this member's constants):
//  - the 2-instruction leaves: "return the first argument" (func_089304C8 of src/core/leaves.cpp), "jump to another function" (src/core/thunks.cpp),
//    "set a1 to 0 / 1, jump", "set a0 to 1, jump", "add a displacement to a0, jump" (func_088DA458), "load a0 = *(a0 + n), jump"
//    (func_0885E314 of src/core/accessors.cpp), "return *(a0 + n)", "return a constant" and the empty function;
//  - "load, then call and return" (7 instructions, func_08891608 of hot_leaves.cpp): v0 = target(*(a0 + n));
//  - "store a vtable and return it" (4 instructions, func_088AFB14);
//  - "call, then tail call with the same this" (10 instructions, func_0890B5A0); "call, then tail call with this + n" (func_088F12E0);
//    "vtable, then tail call with this + n" (func_08B0DDDC); "swap the two arguments and jump" (func_089A8978);
//  - "record query" (15 instructions, func_08932AEC): the record func_08936584 fills from a key stored in the object, one halfword read back;
//  - "function-local static" (9 instructions, func_08809AF8): *slot = target();
//  - "holder-core getter" (37 instructions, the shape of func_0882F404 of src/game/race/organizer.cpp; the four members left over by batch 74): the engine object of
//    a holder, one of its getters, the integer written into a result holder (func_08A85B1C here, func_08A97A6C in the template);
//  - "use of a holder's object" (19 instructions, MWatcher_append): the holder made by `make`, `use(object)`, the holder released;
//  - "object factories" (func_0880A414 and func_0880A464 of src/core/factories.cpp).
//
// Left out (they stay recompiled): the "clamp step" (batches 63 / 65 / 68), "range erase" and "holder conversion" members that batch 74 wrote but did not
// register (no compared case changed memory / not comparable), func_08825AE8 and func_08825B74 (checkIn waits on the UMD drive, allocateResource creates
// threads: not comparable), the organizer setters func_0885E9EC / func_0885E9F8 / func_0885E778 / func_0885E788 (organizer.cpp lists them as compared 0 in
// every run), func_08A042F4 / func_08A0BE48 / func_0886FC04 and the other single members of families whose template is a one-off and whose body differs from
// it in more than constants.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_75 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kReleaseLocal = 0x08A5B9ECu; // destroys a temporary holder
constexpr std::uint32_t kAssignSmart = 0x08A5BAD8u;  // (dst, value): smart-pointer assignment
constexpr std::uint32_t kReportNull = 0x08A5BB7Cu;   // (message): the original's null-object assert
constexpr std::uint32_t kAllocate = 0x08A5BC20u;     // (size, align): the object factories' allocator

// func_08A5BB7C(message) when `object` is null (the original's null-object assert); the code goes on regardless.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the null object (the bnez falls through to the call,
// whose delay slot loads a0 = message, then the code continues with the null pointer as on the other path).
void null_check(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t message) {
    if (object == 0u) (void)call_guest(rt, ctx, kReportNull, {message});
}

// return_first(a): v0 = a0.
void return_first(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4]; }

// thunk(...): the jump to `target`, every argument as it is.
void thunk(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target) { tail_call(ctx, target, {}); }

// tail_a1_zero / tail_a1_one(a0, ...): target(a0, 0 / 1) (a1 set in the delay slot of the jump).
void tail_a1_zero(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target) { tail_call(ctx, target, {ctx.gpr[4], 0u}); }
void tail_a1_one(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target) { tail_call(ctx, target, {ctx.gpr[4], 1u}); }

// tail_a0_one(...): target(1) (a0 set in the delay slot of the jump).
void tail_a0_one(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target) { tail_call(ctx, target, {1u}); }

// forward_add(this): target(this + offset).
void forward_add(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t offset) {
    tail_call(ctx, target, {ctx.gpr[4] + offset});
}

// forward_load(this): target(*(this + offset)).
void forward_load(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t offset) {
    tail_call(ctx, target, {word(rt, ctx.gpr[4] + offset)});
}

// field_getter(this): v0 = *(this + offset).
void field_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset) { ctx.gpr[2] = word(rt, ctx.gpr[4] + offset); }

// nothing(): the original is a bare `jr ra`.
void nothing(psprecomp::Runtime &, psprecomp::AllegrexContext &) {}

// constant(): v0 = the constant (set in the delay slot of the `jr ra`).
void constant(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t value) { ctx.gpr[2] = value; }

// load_then_call(this, a1, a2, a3): v0 = target(*(this + offset), a1, a2, a3).
void load_then_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t offset) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + offset);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, target, {object, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
}

// store_vtable_return(this): *this = the vtable; v0 = the vtable (the store sits in the delay slot of the `jr ra`).
void store_vtable_return(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    put(rt, ctx.gpr[4], vtable);
    ctx.gpr[2] = vtable;
}

// two_step(this, a1, a2, a3): first(this, a1, a2, a3), then second(this) (tail call; v0 as the first leaves it).
void two_step(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, first, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    tail_call(ctx, second, {self});
}

// call_then_tail_add(this, a1, a2, a3): first(this, a1, a2, a3), then second(this + offset) (tail call; v0 as the first leaves it).
void call_then_tail_add(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t offset, std::uint32_t second) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[2] = call_guest(rt, ctx, first, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    tail_call(ctx, second, {self + offset});
}

// vtable_then_tail(this): *this = the vtable (v0 = it too), then target(this + offset) (tail call).
void vtable_then_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable, std::uint32_t target, std::uint32_t offset) {
    put(rt, ctx.gpr[4], vtable);
    ctx.gpr[2] = vtable;
    tail_call(ctx, target, {ctx.gpr[4] + offset});
}

// swapped_tail(a, b): v0 = a, then target(b, a) (tail call).
void swapped_tail(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const std::uint32_t a = ctx.gpr[4];
    ctx.gpr[2] = a;
    tail_call(ctx, target, {ctx.gpr[5], a});
}

// static_init_store(): *slot = v0 = target() (a function-local static's initialiser).
void static_init_store(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t slot) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, target, {});
    put(rt, slot, ctx.gpr[2]);
}

// record_query(this, a1): the record func_08936584 fills (registry func_089363FC(), a1, the key words this[key_a] and this[key_b], a local at sp + 0);
// v0 = the halfword at sp + `half_offset` of the local.
void record_query(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t key_a, std::uint32_t key_b, std::uint32_t half_offset) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t registry = call_guest(rt, ctx, 0x089363FCu, {});
    (void)call_guest(rt, ctx, 0x08936584u, {registry, ctx.gpr[5], word(rt, self + key_a), word(rt, self + key_b), frame.at(0u)});
    ctx.gpr[2] = rt.memory().load16(frame.at(half_offset));
}

// holder_core_getter(out): the holder (sp + 4) `as` fills gives an object (`message` when null) whose engine object `core_of` returns; the holder is
// released; fill(result holder at sp + 0, method(engine object)); the result is assigned to `out` (func_08A5BAD8) when out is not that local holder;
// the local holder is released (v0 = that release's result).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the null object (null_check); out being the local
// result holder (the beq skips func_08A5BAD8; its delay slot a0 = out is dead, the holder is released next).
void holder_core_getter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t as, std::uint32_t message, std::uint32_t core_of,
                      std::uint32_t method, std::uint32_t fill) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(4u), result = frame.at(0u);
    (void)call_guest(rt, ctx, as, {holder});
    const std::uint32_t object = word(rt, holder);
    null_check(rt, ctx, object, message);
    const std::uint32_t core = call_guest(rt, ctx, core_of, {object});
    (void)call_guest(rt, ctx, kReleaseLocal, {holder});
    const std::uint32_t value = call_guest(rt, ctx, method, {core});
    (void)call_guest(rt, ctx, fill, {result, value});
    if (out != result) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, result)});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {result});
}

// holder_use(): the holder (sp + 0) `make` fills gives an object (`message` when null); use(object); the holder is released (v0 = that release's result).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the null object (null_check).
void holder_use(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t make, std::uint32_t message, std::uint32_t use) {
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, make, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    null_check(rt, ctx, object, message);
    (void)call_guest(rt, ctx, use, {object});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// make_instance(out): an object of `size` bytes (allocator func_08A5BC20(size, 4)) built by construct(object), kept in the local at sp + 0 and handed to
// init(out, &local); v0 = init's result.
void make_instance(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t size, std::uint32_t construct, std::uint32_t init) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_guest(rt, ctx, kAllocate, {size, 4u});
    (void)call_guest(rt, ctx, construct, {object});
    put(rt, frame.at(0u), object);
    ctx.gpr[2] = call_guest(rt, ctx, init, {out, frame.at(0u)});
}

// assign_make(out): the holder (sp + 0) `make` fills, assigned to out (func_08A5BAD8) when out is not that local, then released (v0 = that release's result).
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): out being the local holder (the beq skips
// func_08A5BAD8; its delay slot a0 = out is dead, the holder is released next).
void assign_make(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t make) {
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t local = frame.at(0u);
    (void)call_guest(rt, ctx, make, {local});
    if (out != local) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, local)});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {local});
}


// return_first rows
void func_08BAC230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_first(rt, ctx); }
void func_08C45D74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_first(rt, ctx); }
void func_08C45DD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_first(rt, ctx); }
void func_08C45DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_first(rt, ctx); }
void func_08C45EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_first(rt, ctx); }
void func_08C45F94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_first(rt, ctx); }
void func_08C45F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { return_first(rt, ctx); }

// thunk rows
void func_089C4558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { thunk(rt, ctx, 0x08B599E0u); }
void func_08ACE760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { thunk(rt, ctx, 0x08ACE6E0u); }
void func_08AD0974(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { thunk(rt, ctx, 0x08B2E98Cu); }
void func_08B2E2B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { thunk(rt, ctx, 0x08C7A988u); }
void func_08C31384(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { thunk(rt, ctx, 0x08AA1D7Cu); }

// tail_a1_zero rows
void func_08B34888(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_a1_zero(rt, ctx, 0x08B34748u); }
void func_08B34B24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_a1_zero(rt, ctx, 0x08B349C4u); }
void func_08B77BF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_a1_zero(rt, ctx, 0x08B7DC1Cu); }

// tail_a1_one rows
void func_08B34880(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_a1_one(rt, ctx, 0x08B34748u); }
void func_08B34B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_a1_one(rt, ctx, 0x08B349C4u); }

// tail_a0_one rows
void func_08B2EFF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_a0_one(rt, ctx, 0x08B599E0u); }

// forward_add rows
void func_0886924C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08869EF0u, 432u); }
// Batch 141 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
void func_0885F3C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x0896275Cu, 17596u); }
void func_08869244(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08869E60u, 432u); }
void func_0889117C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08890788u, 8u); }
void func_0889118C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08890554u, 8u); }
void func_0889DD64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08899D60u, 40u); }
void func_088A3710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x088A5E5Cu, 8u); }
void func_088A3978(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x088A8420u, 792u); }
void func_088A3980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x088A7C6Cu, 792u); }
void func_088F187C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x088F1588u, 20u); }
void func_08916AB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x0890AFC0u, 4u); }
void func_089272FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08925B68u, 8148u); }
void func_08939F4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B344E0u, 20u); }
void func_08939F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B3457Cu, 20u); }
void func_08968F90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B855E0u, 204u); }
void func_08977318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B28C44u, 72u); }
void func_0897D804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x089C3CF0u, 20u); }
void func_0897D80C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x089C3D4Cu, 20u); }
void func_0897E6A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B855E0u, 480u); }
void func_089B5828(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B2080Cu, 312u); }
void func_089F0B28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B0B87Cu, 12u); }
void func_089FCA70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B855E0u, 100u); }
void func_08A36DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B487B8u, 20u); }
void func_08A6C5C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B855E0u, 20u); }
void func_08A89F54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B855E0u, 20u); }
void func_08BBFF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08BBFE68u, 152u); }
void func_08C53F84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B61664u, 36u); }
void func_089272EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08925B70u, 8148u); }
void func_089F0AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B0B8CCu, 12u); }
void func_08B52CDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_add(rt, ctx, 0x08B54760u, 32u); }

// forward_load rows
void func_088A5E54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_load(rt, ctx, 0x088A5D50u, 0x00000000u); }
void func_08A4D2F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_load(rt, ctx, 0x08A503C8u, 0x00000078u); }
void func_08ACAFCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_load(rt, ctx, 0x08ACA4F4u, 0x00000000u); }
void func_08ACAFDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_load(rt, ctx, 0x08ACAC58u, 0x00000000u); }

// field_getter rows
void func_08C45D7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { field_getter(rt, ctx, 0x00000000u); }

// nothing rows
void func_08B34DB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { nothing(rt, ctx); }

// constant rows
void func_0881AF28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { constant(rt, ctx, 30u); }

// load_then_call rows
void func_08875090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08875974u, 0x00000000u); }
void func_088750AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08875D60u, 0x00000000u); }
void func_08A4D2D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08A50758u, 0x00000078u); }
void func_08AC3308(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08AC31B4u, 0x00000000u); }

// store_vtable rows
void func_08884B0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CBF428u); }
// Batch 141 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
void func_0880BEE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CBCDC0u); }
void func_08865EE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CBF5D8u); }
void func_08925AE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC6078u); }
void func_08925B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC6078u); }
void func_08925FF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC60B8u); }
void func_08929CB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC62F8u); }
void func_08929D88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC6318u); }
void func_08929DD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC6318u); }
void func_08929DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC6318u); }
void func_08929DF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC6318u); }
void func_08AB8888(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD5EC0u); }
void func_08AB94E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD5F48u); }
void func_08AB9E30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD5FB8u); }
void func_08ABB390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD6140u); }
void func_08ABBCD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD61C0u); }
void func_08ABC75C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD6248u); }
void func_08ABF6C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD6420u); }
void func_08AC2680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD6628u); }
void func_08AD7DF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD6FC8u); }
void func_08AF6434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD7348u); }
void func_08B08420(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD79F8u); }
void func_08B0C1D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD7B50u); }
void func_08B0CEB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD7B98u); }
void func_08B1E0EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD9360u); }
void func_08B1E11C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD9370u); }
void func_08B21928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD9518u); }
void func_08B219C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD9518u); }
void func_08B219D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD9518u); }
void func_08B24D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD9630u); }
void func_08B27270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD9660u); }
void func_08B2B0A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CDA6C0u); }
void func_08B5C280(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CDAE78u); }
void func_08B6258C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CDBF58u); }
void func_08B640F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CDC070u); }
void func_08B89CF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CBE300u); }
void func_08C1B490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD1E70u); }
void func_088F897C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CC3EC8u); }
void func_08B1FD74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_vtable_return(rt, ctx, 0x08CD9498u); }

// two_step rows
void func_089B45EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_step(rt, ctx, 0x089BA59Cu, 0x089B44E4u); }
void func_08A5176C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_step(rt, ctx, 0x08B2E924u, 0x08A5159Cu); }
void func_08B1B99C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { two_step(rt, ctx, 0x08B2E224u, 0x08B2E28Cu); }

// query rows
void func_08932A38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_query(rt, ctx, 0x00000028u, 0x0000002Cu, 0x00000008u); }
void func_08932A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_query(rt, ctx, 0x00000028u, 0x0000002Cu, 0x0000000Au); }
void func_08932AB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { record_query(rt, ctx, 0x00000078u, 0x0000007Cu, 0x00000008u); }

// holder-core getter rows (0x0881ECEC / 0x0881ED80 are MCarCollection::getFavoriteSize / getCarCountFavorite in config/symbol_addrs.txt)
void func_0881ECEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x0881BD98u, 0x08C98F48u, 0x0881B068u, 0x0881AF28u, 0x08A85B1Cu); }
void func_0881ED80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x0881BD98u, 0x08C98F48u, 0x0881B068u, 0x0881AF30u, 0x08A85B1Cu); }
void func_08A1B720(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08A1B570u, 0x08CAA104u, 0x08A1B3CCu, 0x08A1B388u, 0x08A85B1Cu); }
void func_08A1B848(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08A1B570u, 0x08CAA104u, 0x08A1B3CCu, 0x08A1B398u, 0x08A85B1Cu); }

// static_init rows
void func_08B52DDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_init_store(rt, ctx, 0x08B57E40u, 0x08CE0DCCu); }
void func_08B581F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_init_store(rt, ctx, 0x08B57E40u, 0x08D6CEA4u); }

// swapped rows
void func_08B5378C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swapped_tail(rt, ctx, 0x08B531B4u); }
void func_08B5379C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { swapped_tail(rt, ctx, 0x08B53528u); }

// vtable_tail rows
void func_0886A858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vtable_then_tail(rt, ctx, 0x08CBF710u, 0x0887C644u, 100u); }
void func_0887D080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vtable_then_tail(rt, ctx, 0x08CBFF08u, 0x0887D034u, 4u); }

// two_step_add rows
void func_08B52B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_then_tail_add(rt, ctx, 0x08B5284Cu, 32u, 0x08B54808u); }
void func_08BE4084(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_then_tail_add(rt, ctx, 0x08BE4008u, 12u, 0x08B28DC4u); }

// holder_use rows
void func_089C11D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_use(rt, ctx, 0x089BEE4Cu, 0x08CA5EB4u, 0x089C10C8u); }
void func_08A14FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_use(rt, ctx, 0x08A14610u, 0x08CA9A6Cu, 0x08A14D94u); }
void func_08A37010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_use(rt, ctx, 0x08A36FD0u, 0x08CAD0CCu, 0x08A36DE0u); }

// make rows
void func_08A12E38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_instance(rt, ctx, 1984u, 0x08A12C54u, 0x089C15F4u); }
void func_08A1BFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_instance(rt, ctx, 28u, 0x08A1BF20u, 0x08A76AB4u); }

// assign rows
void func_08A04254(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_make(rt, ctx, 0x08A04204u); }
void func_08A1CA40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { assign_make(rt, ctx, 0x08A1C9F0u); }

} // namespace
// Batch 158: more members of the load_then_call family (tools.family_rows), verified by synthetic cases.
void func_08838E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08838DA8u, 0x00000014u); }
void func_08838E68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08838E4Cu, 0x00000014u); }
void func_0885E170(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x088D9CCCu, 0x000005ACu); }
void func_088750C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08876850u, 0x00000000u); }
void func_088750E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08875CD0u, 0x00000000u); }
void func_08891798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08891710u, 0x00000000u); }
void func_08891838(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x088917B4u, 0x00000000u); }
void func_08892084(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08891F04u, 0x00000000u); }
void func_088CE8F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x088DA0BCu, 0x00000004u); }
void func_088E4B48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x088ECB1Cu, 0x00000000u); }
void func_08A4CDD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08A506B8u, 0x00000078u); }
void func_08A85658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08B71CE0u, 0x00000014u); }
void func_08AC3324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08AC322Cu, 0x00000000u); }
void func_08AC4050(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08AC3FD4u, 0x00000000u); }
void func_08AC416C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08AC406Cu, 0x00000000u); }
void func_08ACBBF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08ACBBA8u, 0x00000000u); }
void func_08ACC624(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08ACC5DCu, 0x00000000u); }
void func_08B166DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_then_call(rt, ctx, 0x08B23E50u, 0x000000A4u); }
// Batch 160: more members of the 0x0881ECEC family (tools.family_rows), verified by synthetic cases.
void func_0882F370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885FCA4u, 0x08A97A6Cu); }
void func_0882F52C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885E9B8u, 0x08A97A6Cu); }
void func_0882F674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885EA5Cu, 0x08A97A6Cu); }
void func_0882FCF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885EBDCu, 0x08A97A6Cu); }
void func_0882FD88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885EBFCu, 0x08A97A6Cu); }
void func_08833AFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08862A90u, 0x08A97A6Cu); }
void func_08839204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x08931784u, 0x08A85B1Cu); }
void func_08839298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x0893205Cu, 0x08A85B1Cu); }
void func_08839440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x08931790u, 0x08A85B1Cu); }
void func_088394D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x0893170Cu, 0x08A85B1Cu); }
void func_08839568(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x089318B8u, 0x08A85B1Cu); }
void func_088395FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x089318C4u, 0x08A85B1Cu); }
void func_088398D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x089316DCu, 0x08A85B1Cu); }
void func_08841C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088418B4u, 0x08C9C584u, 0x0884177Cu, 0x0888EB90u, 0x08A5FA8Cu); }
void func_088421FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088418B4u, 0x08C9C584u, 0x0884177Cu, 0x0888EBBCu, 0x08848DB8u); }
void func_08842290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088418B4u, 0x08C9C584u, 0x0884177Cu, 0x0888EBC8u, 0x0884642Cu); }
void func_08842324(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088418B4u, 0x08C9C584u, 0x0884177Cu, 0x0888EBD4u, 0x08845694u); }
void func_08844C88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088447E8u, 0x08C9C948u, 0x08844634u, 0x08893D14u, 0x08A85B1Cu); }
void func_0884DD14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x0884D660u, 0x08C9D198u, 0x0884D3D8u, 0x0889779Cu, 0x08A85B1Cu); }
void func_088556D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088547C4u, 0x08C9E188u, 0x08854678u, 0x088A6028u, 0x08A85B1Cu); }
void func_088564B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088547C4u, 0x08C9E188u, 0x08854678u, 0x088A6144u, 0x08A85B1Cu); }
void func_08856544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088547C4u, 0x08C9E188u, 0x08854678u, 0x088A5984u, 0x08A85B1Cu); }
void func_088574F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088547C4u, 0x08C9E188u, 0x08854678u, 0x088A5804u, 0x08A85B1Cu); }
void func_08857704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x088547C4u, 0x08C9E188u, 0x08854678u, 0x088A7618u, 0x08A85B1Cu); }
void func_08A1B7B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08A1B570u, 0x08CAA104u, 0x08A1B3CCu, 0x08A1B390u, 0x08A85B1Cu); }
void func_08A1B8DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08A1B570u, 0x08CAA104u, 0x08A1B3CCu, 0x08A1B3A0u, 0x08A85B1Cu); }
void func_08A1B970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08A1B570u, 0x08CAA104u, 0x08A1B3CCu, 0x08A1B3A8u, 0x08A85B1Cu); }
void func_08A1BA04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08A1B570u, 0x08CAA104u, 0x08A1B3CCu, 0x08A1B3B0u, 0x08A85B1Cu); }
void func_08A1BC30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08A1B570u, 0x08CAA104u, 0x08A1B3CCu, 0x08A1B3C4u, 0x08A85B1Cu); }
void func_08A1C408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_core_getter(rt, ctx, 0x08A1C370u, 0x08CAA224u, 0x08A1C0F4u, 0x08A1C11Cu, 0x08A85B1Cu); }
} // namespace hot_batch_75
} // namespace gt::game

#define HOT75(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_75::name, gt::ReturnKind::kind)
HOT75(0x08BAC230u, func_08BAC230, Word);
HOT75(0x08C45D74u, func_08C45D74, Word);
HOT75(0x08C45DD8u, func_08C45DD8, Word);
HOT75(0x08C45DF8u, func_08C45DF8, Word);
HOT75(0x08C45EC0u, func_08C45EC0, Word);
HOT75(0x08C45F94u, func_08C45F94, Word);
HOT75(0x08C45F9Cu, func_08C45F9C, Word);
HOT75(0x08C31384u, func_08C31384, Word);
HOT75(0x08B34888u, func_08B34888, Word);
HOT75(0x08B34B24u, func_08B34B24, Word);
HOT75(0x08B34880u, func_08B34880, Word);
HOT75(0x08B34B1Cu, func_08B34B1C, Word);
HOT75(0x0886924Cu, func_0886924C, Word);
// Batch 141 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
HOT75(0x0885F3C0u, func_0885F3C0, Word);
HOT75(0x08869244u, func_08869244, Word);
HOT75(0x0889117Cu, func_0889117C, Word);
HOT75(0x0889118Cu, func_0889118C, Word);
HOT75(0x0889DD64u, func_0889DD64, Word);
// HOT75(0x088A3710u, func_088A3710, Word);  // no comparable synthetic case yet
HOT75(0x088A3978u, func_088A3978, Word);
HOT75(0x088A3980u, func_088A3980, Word);
HOT75(0x088F187Cu, func_088F187C, Word);
HOT75(0x08916AB8u, func_08916AB8, Word);
HOT75(0x089272FCu, func_089272FC, Word);
HOT75(0x08939F4Cu, func_08939F4C, Word);
HOT75(0x08939F54u, func_08939F54, Word);
HOT75(0x08968F90u, func_08968F90, Word);
// HOT75(0x08977318u, func_08977318, Word);  // no comparable synthetic case yet
HOT75(0x0897D804u, func_0897D804, Word);
HOT75(0x0897D80Cu, func_0897D80C, Word);
HOT75(0x0897E6A4u, func_0897E6A4, Word);
HOT75(0x089B5828u, func_089B5828, Word);
HOT75(0x089F0B28u, func_089F0B28, Word);
HOT75(0x089FCA70u, func_089FCA70, Word);
HOT75(0x08A36DE0u, func_08A36DE0, Word);
HOT75(0x08A6C5C8u, func_08A6C5C8, Word);
HOT75(0x08A89F54u, func_08A89F54, Word);
// HOT75(0x08BBFF7Cu, func_08BBFF7C, Word);  // no comparable synthetic case yet
HOT75(0x08C53F84u, func_08C53F84, Word);
HOT75(0x089272ECu, func_089272EC, Word);
HOT75(0x08B52CDCu, func_08B52CDC, Word);
HOT75(0x08C45D7Cu, func_08C45D7C, Word);
HOT75(0x08B34DB0u, func_08B34DB0, Word);
HOT75(0x08875090u, func_08875090, Word);
HOT75(0x088750ACu, func_088750AC, Word);
HOT75(0x08884B0Cu, func_08884B0C, Word);
// Batch 141 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
HOT75(0x0880BEE8u, func_0880BEE8, Word);
HOT75(0x08865EE4u, func_08865EE4, Word);
HOT75(0x08925AE4u, func_08925AE4, Word);
HOT75(0x08925B48u, func_08925B48, Word);
HOT75(0x08925FF8u, func_08925FF8, Word);
HOT75(0x08929CB0u, func_08929CB0, Word);
HOT75(0x08929D88u, func_08929D88, Word);
HOT75(0x08929DD0u, func_08929DD0, Word);
HOT75(0x08929DE0u, func_08929DE0, Word);
HOT75(0x08929DF0u, func_08929DF0, Word);
HOT75(0x08AB8888u, func_08AB8888, Word);
HOT75(0x08AB94E4u, func_08AB94E4, Word);
HOT75(0x08AB9E30u, func_08AB9E30, Word);
HOT75(0x08ABB390u, func_08ABB390, Word);
HOT75(0x08ABBCD8u, func_08ABBCD8, Word);
HOT75(0x08ABC75Cu, func_08ABC75C, Word);
HOT75(0x08ABF6C0u, func_08ABF6C0, Word);
HOT75(0x08AC2680u, func_08AC2680, Word);
HOT75(0x08AD7DF8u, func_08AD7DF8, Word);
HOT75(0x08AF6434u, func_08AF6434, Word);
HOT75(0x08B08420u, func_08B08420, Word);
HOT75(0x08B0C1D8u, func_08B0C1D8, Word);
HOT75(0x08B0CEB4u, func_08B0CEB4, Word);
HOT75(0x08B1E0ECu, func_08B1E0EC, Word);
HOT75(0x08B1E11Cu, func_08B1E11C, Word);
HOT75(0x08B21928u, func_08B21928, Word);
HOT75(0x08B219C0u, func_08B219C0, Word);
HOT75(0x08B219D0u, func_08B219D0, Word);
HOT75(0x08B24D3Cu, func_08B24D3C, Word);
HOT75(0x08B27270u, func_08B27270, Word);
// HOT75(0x08B2B0A8u, func_08B2B0A8, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT75(0x08B5C280u, func_08B5C280, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT75(0x08B6258Cu, func_08B6258C, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT75(0x08B640F4u, func_08B640F4, Word);
// HOT75(0x08B89CF0u, func_08B89CF0, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT75(0x08C1B490u, func_08C1B490, Word);
HOT75(0x088F897Cu, func_088F897C, Word);
HOT75(0x08B1FD74u, func_08B1FD74, Word);
HOT75(0x08932A38u, func_08932A38, Word);
HOT75(0x08932A74u, func_08932A74, Word);
HOT75(0x08932AB0u, func_08932AB0, Word);
HOT75(0x0881ECECu, func_0881ECEC, Word);
HOT75(0x0881ED80u, func_0881ED80, Word);
HOT75(0x08B52DDCu, func_08B52DDC, Word);
HOT75(0x08B581F4u, func_08B581F4, Word);
HOT75(0x08B5378Cu, func_08B5378C, Word);
HOT75(0x08B5379Cu, func_08B5379C, Word);
HOT75(0x0886A858u, func_0886A858, Word);
HOT75(0x0887D080u, func_0887D080, Word);
HOT75(0x08B52B48u, func_08B52B48, Word);
HOT75(0x08BE4084u, func_08BE4084, Word);
HOT75(0x08A37010u, func_08A37010, Word);
HOT75(0x08A1BFD0u, func_08A1BFD0, Word);
HOT75(0x08A04254u, func_08A04254, Word);
HOT75(0x08A1CA40u, func_08A1CA40, Word);
// Written but not registered: none of the compared cases of the five fast routes (compared 0, nested in 0) reached these; they run as the recompiled originals.
// HOT75(0x089C4558u, func_089C4558, Word);
// HOT75(0x08ACE760u, func_08ACE760, Word);
// HOT75(0x08AD0974u, func_08AD0974, Word);
// HOT75(0x08B2E2B8u, func_08B2E2B8, Word);
// HOT75(0x08B2EFF0u, func_08B2EFF0, Word);
// HOT75(0x089F0AF4u, func_089F0AF4, Word);
// HOT75(0x088A5E54u, func_088A5E54, Word);
HOT75(0x08A4D2F8u, func_08A4D2F8, Word);
// HOT75(0x08ACAFCCu, func_08ACAFCC, Word);
// HOT75(0x08ACAFDCu, func_08ACAFDC, Word);
// HOT75(0x0881AF28u, func_0881AF28, Word);
// HOT75(0x08A4D2D4u, func_08A4D2D4, Word);  // batch 104: still not comparable (it chains five sceFont imports: Open, GetFontInfo, Close, GetCharInfo, then 0xCA1E6945; only the constant 0x02D7F94B one was unlocked)
// HOT75(0x08AC3308u, func_08AC3308, Word);
// HOT75(0x089B45ECu, func_089B45EC, Word);
// HOT75(0x08A5176Cu, func_08A5176C, Word);
// HOT75(0x08B1B99Cu, func_08B1B99C, Word);
// HOT75(0x08A1B720u, func_08A1B720, Word);
// HOT75(0x08A1B848u, func_08A1B848, Word);
// HOT75(0x089C11D0u, func_089C11D0, Word);
// HOT75(0x08A14FF0u, func_08A14FF0, Word);
// HOT75(0x08A12E38u, func_08A12E38, Word);
// func_08B77BF4 is compared but the tool's contract audit flags it: three original callers read v1 after the call and the ReturnKind leaves v1 uncompared.
// HOT75(0x08B77BF4u, func_08B77BF4, Word);
HOT75(0x08838E28u, func_08838E28, Word);
HOT75(0x08838E68u, func_08838E68, Word);
HOT75(0x0885E170u, func_0885E170, Word);
// HOT75(0x088750C8u, func_088750C8, Word);  // no comparable synthetic case yet
// HOT75(0x088750E4u, func_088750E4, Word);  // no comparable synthetic case yet
HOT75(0x08891798u, func_08891798, Word);
HOT75(0x08891838u, func_08891838, Word);
// HOT75(0x08892084u, func_08892084, Word);  // no comparable synthetic case yet
HOT75(0x088CE8F0u, func_088CE8F0, Word);
HOT75(0x088E4B48u, func_088E4B48, Word);
HOT75(0x08A4CDD8u, func_08A4CDD8, Word);
HOT75(0x08A85658u, func_08A85658, Word);
HOT75(0x08AC3324u, func_08AC3324, Word);
HOT75(0x08AC4050u, func_08AC4050, Word);
HOT75(0x08AC416Cu, func_08AC416C, Word);
// HOT75(0x08ACBBF4u, func_08ACBBF4, Word);  // no comparable synthetic case yet
// HOT75(0x08ACC624u, func_08ACC624, Word);  // no comparable synthetic case yet
HOT75(0x08B166DCu, func_08B166DC, Word);
// HOT75(0x0882F370u, func_0882F370, Word);  // no comparable synthetic case yet
// HOT75(0x0882F52Cu, func_0882F52C, Word);  // no comparable synthetic case yet
// HOT75(0x0882F674u, func_0882F674, Word);  // no comparable synthetic case yet
// HOT75(0x0882FCF4u, func_0882FCF4, Word);  // no comparable synthetic case yet
// HOT75(0x0882FD88u, func_0882FD88, Word);  // no comparable synthetic case yet
// HOT75(0x08833AFCu, func_08833AFC, Word);  // no comparable synthetic case yet
// HOT75(0x08839204u, func_08839204, Word);  // no comparable synthetic case yet
// HOT75(0x08839298u, func_08839298, Word);  // no comparable synthetic case yet
// HOT75(0x08839440u, func_08839440, Word);  // no comparable synthetic case yet
// HOT75(0x088394D4u, func_088394D4, Word);  // no comparable synthetic case yet
// HOT75(0x08839568u, func_08839568, Word);  // no comparable synthetic case yet
// HOT75(0x088395FCu, func_088395FC, Word);  // no comparable synthetic case yet
// HOT75(0x088398D4u, func_088398D4, Word);  // no comparable synthetic case yet
// HOT75(0x08841C70u, func_08841C70, Word);  // no comparable synthetic case yet
// HOT75(0x088421FCu, func_088421FC, Word);  // no comparable synthetic case yet
// HOT75(0x08842290u, func_08842290, Word);  // no comparable synthetic case yet
// HOT75(0x08842324u, func_08842324, Word);  // no comparable synthetic case yet
// HOT75(0x08844C88u, func_08844C88, Word);  // no comparable synthetic case yet
// HOT75(0x0884DD14u, func_0884DD14, Word);  // no comparable synthetic case yet
// HOT75(0x088556D8u, func_088556D8, Word);  // no comparable synthetic case yet
// HOT75(0x088564B0u, func_088564B0, Word);  // no comparable synthetic case yet
// HOT75(0x08856544u, func_08856544, Word);  // no comparable synthetic case yet
// HOT75(0x088574F0u, func_088574F0, Word);  // no comparable synthetic case yet
// HOT75(0x08857704u, func_08857704, Word);  // no comparable synthetic case yet
// HOT75(0x08A1B7B4u, func_08A1B7B4, Word);  // no comparable synthetic case yet
// HOT75(0x08A1B8DCu, func_08A1B8DC, Word);  // no comparable synthetic case yet
// HOT75(0x08A1B970u, func_08A1B970, Word);  // no comparable synthetic case yet
// HOT75(0x08A1BA04u, func_08A1BA04, Word);  // no comparable synthetic case yet
// HOT75(0x08A1BC30u, func_08A1BC30, Word);  // no comparable synthetic case yet
// HOT75(0x08A1C408u, func_08A1C408, Word);  // no comparable synthetic case yet
