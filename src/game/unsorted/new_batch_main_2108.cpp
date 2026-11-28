// New batch main 2108 (main-session batch 2108). Phase A functions, structured C from tools.disasm. UNVERIFIED draft.
// func_08B708C4: unsigned 64-bit remainder (a1:a0 mod a3:a2), returned in v0 (low) / v1 (high). The original traps on a
//   zero divisor (`break 0,7`); that path is not reproduced here (C++ `%` by zero is undefined), so the gate must not
//   synthesise a zero divisor for it.
// func_08925424: constructor of a large object at a0 (writes its fields, allocates sub-objects through func_08B62344 and
//   hands each to a builder). Delay-slot copies of v0 are read as the value before the call (the previous allocation).
//   The last builder's v0 is the return value and is left in ctx.gpr[2].
// func_089A81C4 is skipped (stack-array loops and vtable slots not yet understood).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2108 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using u64 = std::uint64_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A callee that also reads t0 as its fifth argument: the caller sets the register before the jump.
u32 call_with_t0(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, u32 t0) {
    ctx.gpr[8] = t0;
    return call(rt, ctx, target, arguments);
}

// The game's allocator (func_08B62344): size in a0, pointer in v0.
u32 alloc(Runtime &rt, AllegrexContext &ctx, u32 size) { return call(rt, ctx, 0x08B62344u, {size}); }

// An 8-byte object: vtable word at +0, zero word at +4 (the asm stores the zero in the delay slot of the next allocation).
u32 vtable_object(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 object = alloc(rt, ctx, 8u);
    put(rt, object, vtable);
    put(rt, object + 4u, 0u);
    return object;
}

constexpr u32 kFloat50 = 0x42480000u;  // 50.0f
constexpr u32 kFloat33 = 0x42040000u;  // 33.0f

}  // namespace

// func_08B708C4: u64 remainder, dividend = a1:a0, divisor = a3:a2; v0 = low word, v1 = high word.
void func_08B708C4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u64 dividend = (static_cast<u64>(ctx.gpr[5]) << 32) | ctx.gpr[4];
    const u64 divisor = (static_cast<u64>(ctx.gpr[7]) << 32) | ctx.gpr[6];
    const u64 remainder = dividend % divisor;
    ctx.gpr[2] = static_cast<u32>(remainder);
    ctx.gpr[3] = static_cast<u32>(remainder >> 32);
}

// func_08925424: builds the object at a0. subA = this+0x200, subB = this+0x220.
void func_08925424(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 subA = self + 0x200u;
    const u32 subB = self + 0x220u;
    const u32 kObjectVtable = 0x08CC5E20u;

    call(rt, ctx, 0x08926D60u, {self});
    put(rt, self, 0x08CC5DC8u);
    put(rt, self + 0x248u, 0xFFFFFFFFu);
    put(rt, self + 0x244u, 0u);
    put(rt, self + 0x24Cu, 0u);  // four bytes 0x24C..0x24F
    put(rt, self + 0x1FCu, 0u);
    put(rt, self + 0x200u, 0u);
    put(rt, subA + 4u, 0u);
    put(rt, subA + 8u, 0u);
    put(rt, self + 0x20Cu, 0u);
    put(rt, self + 0x210u, 0u);
    put(rt, self + 0x214u, 0u);
    put(rt, subA + 0x18u, kFloat50);
    put(rt, subA + 0x1Cu, 0u);
    put(rt, self + 0x240u, 0u);
    put(rt, subB, 0u);
    put(rt, subB + 4u, 0u);
    put(rt, subB + 8u, 0u);
    put(rt, self + 0x22Cu, 0u);
    put(rt, self + 0x230u, 0u);
    put(rt, self + 0x234u, 0u);
    put(rt, subB + 0x18u, kFloat50);
    put(rt, subB + 0x1Cu, 0u);
    put(rt, self + 0x1F8u, kFloat33);
    for (u32 i = 0; i < 13u; ++i) put(rt, self + 0x250u + 4u * i, 0u);
    put(rt, self + 0x284u, 0u);

    // Chain of builders: each allocation is handed to the builder with the previous object.
    const u32 first = vtable_object(rt, ctx, kObjectVtable);
    u32 chain = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {chain, first, subA, self}, 1u);
    u32 prev = chain;

    const u32 o3 = alloc(rt, ctx, 0x38u);
    call(rt, ctx, 0x08922B98u, {o3, prev, self});
    u32 holder = o3;

    const u32 o4 = alloc(rt, ctx, 0x2E8u);
    call(rt, ctx, 0x08924B30u, {o4, holder, self, 0u});
    prev = o4;

    const u32 o5 = alloc(rt, ctx, 0x1D4u);
    call(rt, ctx, 0x08920E78u, {o5, prev, self, 0u});
    holder = o5;

    const u32 o6 = alloc(rt, ctx, 0x10u);
    call(rt, ctx, 0x0891FC94u, {o6, holder, self});
    prev = o6;

    const u32 o7 = alloc(rt, ctx, 0x14u);
    call(rt, ctx, 0x08920218u, {o7, prev, self});
    put(rt, self + 0x250u, o7);

    const u32 o8 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o9 = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {o9, o8, subA, self}, 7u);
    prev = o9;

    const u32 o10 = alloc(rt, ctx, 0xF8u);
    call(rt, ctx, 0x08922CF8u, {o10, prev, self});
    holder = o10;

    const u32 o11 = alloc(rt, ctx, 0x2E8u);
    call(rt, ctx, 0x08924B30u, {o11, holder, self, 1u});
    prev = o11;

    const u32 o12 = alloc(rt, ctx, 0x1D4u);
    call(rt, ctx, 0x08920E78u, {o12, prev, self, 1u});
    holder = o12;

    const u32 o13 = alloc(rt, ctx, 0x10u);
    call(rt, ctx, 0x0891FC94u, {o13, holder, self});
    prev = o13;

    const u32 o14 = alloc(rt, ctx, 0x14u);
    call(rt, ctx, 0x08922B04u, {o14, prev, self});
    put(rt, self + 0x254u, o14);

    const u32 o15 = alloc(rt, ctx, 0x30u);
    call(rt, ctx, 0x089211D4u, {o15, 0u, self});
    holder = o15;

    const u32 o16 = alloc(rt, ctx, 0x10u);
    call(rt, ctx, 0x0891FC94u, {o16, holder, self});
    prev = o16;

    const u32 o17 = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {o17, prev, subA, self}, 9u);

    const u32 o18 = alloc(rt, ctx, 0x10u);
    call(rt, ctx, 0x0891FCD8u, {o18, o17, subA, self});
    put(rt, self + 0x25Cu, o18);

    const u32 o19 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o20 = alloc(rt, ctx, 0x10u);
    call(rt, ctx, 0x0891FC94u, {o20, o19, self});
    prev = o20;

    const u32 o21 = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {o21, prev, subA, self}, 0x25u);
    put(rt, self + 0x260u, o21);

    const u32 o22 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o23 = alloc(rt, ctx, 0x10u);
    call(rt, ctx, 0x0891FC94u, {o23, o22, self});
    prev = o23;

    const u32 o24 = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {o24, prev, subA, self}, 0x25u);
    put(rt, self + 0x264u, o24);

    const u32 o25 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o26 = alloc(rt, ctx, 0x10u);
    call(rt, ctx, 0x0891FC94u, {o26, o25, self});
    prev = o26;

    const u32 o27 = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {o27, prev, subA, self}, 0x25u);
    const u32 held2 = o27;

    const u32 o28 = alloc(rt, ctx, 0x14u);
    call(rt, ctx, 0x08920218u, {o28, held2, self});
    put(rt, self + 0x268u, o28);

    const u32 o29 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o30 = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {o30, o29, subA, self}, 0x27u);
    prev = o30;
    put(rt, self + 0x26Cu, prev);

    const u32 o31 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o32 = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {o32, o31, subA, self}, 7u);
    prev = o32;

    const u32 o33 = alloc(rt, ctx, 0x2E8u);
    call(rt, ctx, 0x08924B30u, {o33, prev, self, 2u});
    put(rt, self + 0x258u, o33);

    const u32 o34 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o35 = alloc(rt, ctx, 0xCu);
    call(rt, ctx, 0x0891FD28u, {o35, o34, self});
    prev = o35;

    const u32 o36 = alloc(rt, ctx, 0x14u);
    call_with_t0(rt, ctx, 0x0891FC34u, {o36, prev, subB, self}, 0x27u);
    put(rt, self + 0x278u, o36);

    // Object 0x08CC5E40 with its own vtable; the 0x08CC5E60 object gets no zero word.
    const u32 o37 = alloc(rt, ctx, 8u);
    put(rt, o37, 0x08CC5E40u);
    put(rt, o37 + 4u, 0u);
    const u32 o38 = alloc(rt, ctx, 0x10u);
    call(rt, ctx, 0x0891FC94u, {o38, o37, self});
    prev = o38;

    const u32 o39 = alloc(rt, ctx, 8u);
    call(rt, ctx, 0x08925FE4u, {o39, prev});
    put(rt, o39, 0x08CC5E60u);
    put(rt, self + 0x270u, o39);

    const u32 o40 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o41 = alloc(rt, ctx, 0x38u);
    call(rt, ctx, 0x089214F0u, {o41, o40, self, 0u});
    prev = o41;

    const u32 o42 = alloc(rt, ctx, 0x2E8u);
    call(rt, ctx, 0x08924B30u, {o42, prev, self, 2u});
    put(rt, self + 0x280u, o42);

    const u32 o43 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o44 = alloc(rt, ctx, 0x38u);
    call(rt, ctx, 0x089214F0u, {o44, o43, self, 0u});
    put(rt, self + 0x274u, o44);

    const u32 o45 = vtable_object(rt, ctx, kObjectVtable);
    const u32 o46 = alloc(rt, ctx, 0x38u);
    call(rt, ctx, 0x089214F0u, {o46, o45, self, 1u});
    put(rt, self + 0x27Cu, o46);
    // v0 is the last builder's return value, left in ctx.gpr[2].
}

}  // namespace new_batch_main_2108
}  // namespace gt::game

#define NEW3(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2108::name, gt::ReturnKind::kind)
NEW3(0x08B708C4u, func_08B708C4, Word);
NEW3(0x08925424u, func_08925424, Word);
