// New batch (main session 698): hand-written structured C from `tools/disasm`; nothing here has been through the
// differential gate yet. Each body keeps the original's order of calls and stores (a store in a `jal`'s delay slot
// happens before the call, and the callee may read it), passes every argument register the asm sets, and leaves v0 as
// the original leaves it on every exit path. GuestFrame matches the prologue's `addiu sp, sp, -N` so a callee that
// reads the caller's frame (out-params at sp+0, sp+1, sp+2, sp+0xC, sp+4, sp+8) sees what the original wrote there.
// No names: none of these has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_698 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// `lw v1, 0(object); lw v0, offset(v1); jalr v0`: the object's vtable slot, called with `arguments` in a0.. The callee
// starts with v1 = the vtable and v0 = its own address, as in the asm; afterwards v0 / v1 / f0 are the callee's.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// `lw v0, 0(object); lw v1, offset(v0); jalr v1`: the other register order of the same call (v0 = the vtable, v1 = the
// target at entry, as the asm leaves them).
u32 call_virtual_second(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + offset);
    ctx.gpr[2] = table;
    ctx.gpr[3] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// The `ll`/`sc` decrement of a shared holder's use count: stores old - 1, leaves v1 = the address, a0 = the old value
// and v0 = 1 (the successful `sc`), and answers whether the count reached zero.
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old == 1u;
}

// One member of a chain of shared holders: release the count at +4 and, when that was the last reference, run the
// object's vtable +8 virtual and then the second count at +8 and the +0xC virtual.
void release_holder(Runtime &rt, AllegrexContext &ctx, u32 holder) {
    if (holder == 0u) return;
    if (!count_down(rt, ctx, holder + 4u)) return;
    call_virtual(rt, ctx, holder, 8u, {holder});
    if (!count_down(rt, ctx, holder + 8u)) return;
    call_virtual(rt, ctx, holder, 0xCu, {holder});
}

// A string-ish block: its header sits 0xC bytes before the data (the shared empty header 0x08D75298 is never freed),
// and the reference count 4 bytes before the data decides whether 0x08B93554 frees it.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 scratch) {
    const u32 header = data - 0xCu;
    if (header == 0x08D75298u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B93554u, {header, scratch});
}

// 0x08BDA004 (a0 = the object): destructor of a class holding a chain of shared holders. The vtable 0x08CC5588 goes to
// +0 in the `beqz`'s delay slot, then the holders at 0x168, 0x160, 0x154, 0x14C, 0x144 are released (each: count at +4,
// then the +8 count and the +8 / +0xC virtuals when it was the last reference), the string block at 0x54, the holders at
// 0x50 (through 0x44 + 4), 0x48 and 0x3C, and the body ends with 0x08BD6D84(this) and 0x08B64654(this). v0 = the last
// call's result; every path reaches it.

// 0x08862C90 (a0 = the object): constructor of a large object. It builds the two manager objects 0x08ABF3CC hands out
// for the type records 0x08C9F5AC / 0x08C9F5C8 (both at +0x90 / +0x94, each initialised through its own +0xC virtual and
// then told 0 with the +0x30 virtual), allocates the 0x110-byte block at +0x98 and the four blocks at +0x5AC (0x788),
// +0x5B0 (0x64) and +0x5B4 (0x44), registers the three records at 0x088612B8 / 0x0885DEE8 / 0x0885DEF0 with
// 0x08896CC8 on the block at +0x5BC (whose first word is the object itself), wires the sub-objects from +0x3F58, +0x600,
// +0xD8, +0xA0, +0x5BC and +0x5CC through 0x088914B4 / 0x0889364C / 0x08865F04 / 0x088DA0AC..0x088DA77C, reads a pair of
// values into the bytes at sp+1 / sp+2 (stored to +0x5F0 / +0x5F4), and with a third value at +0x5F8 runs its own
// virtual with a three-word struct at sp+0xC. The blocks at +0x5FC / +0x5F0 / +0x5F4 / +0x5F8 are then linked, and the
// body ends by allocating 0x38 bytes at +0x5B8 (also stored at sp+8, with the type record 0x08CBF368 at sp+4) and
// building it through 0x0888E7BC with the sub-object pointers. v0 = 0x0888E7BC's result.

} // namespace
} // namespace new_batch_main_698
} // namespace gt::game


#define NEW698(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_698::name, gt::ReturnKind::kind)
