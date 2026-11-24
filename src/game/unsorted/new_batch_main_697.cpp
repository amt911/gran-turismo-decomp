// New batch (main session 697): six functions the gate could not run with useful inputs, so their synthetic shapes are
// recorded by hand in /home/andres/repos/gt-ops/shapes/shapes_main697.txt. Written as structured C from `tools.disasm`;
// no recompiler output was read. Each body keeps the original's order of calls and stores (a store in a `jal`'s delay
// slot happens before the call, and the callee may read it), and leaves v0 as the original leaves it on every exit path.
// The three 0x08B2xxxx / 0x08Bxxxxx bodies start from the earlier drafts of commits 4428e4c0 and 0c7c062d, re-checked
// against the asm and corrected where they differed. Draft only: nothing here has been through the differential gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_697 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the `jal` (a0-a3, t0-t3 in order),
// and v0 / v1 / f0 afterwards left as the callee left them.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : floats) {
        if (freg <= 19u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, address, arguments).v0;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, address, arguments).f0;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }

// A run of `lwc1`/`swc1` pairs: three floats moved verbatim.
void copy_floats(Runtime &rt, u32 to, u32 from) {
    for (unsigned i = 0; i < 3u; ++i) put_float(rt, to + 4u * i, float_at(rt, from + 4u * i));
}

// 0x088C9088 (a0 = the object, f12 = a base distance): lays 20 pairs of vectors into the object at +0x8EC: it asks the
// object's +4 block for two step counts (0x088D2C30 at 9/2 and at 8/1, each pair falling back to 6), samples four
// three-float vectors around the base distance and the base distance + 1.2 (0x088D32E0), normalises two of them
// (0x08B4A720's length, 0x08B4A5E8 scaling them by 1/length), and walks the loop index along each of them up to that
// length in 20 steps, writing each step's two vectors through 0x088C896C into the object (the +4 word of each pair from
// the second sample, the +0x10 word from the first). v0 = 0x28, the loop bound the asm leaves behind.

// 0x08860C80 (a0 = the object, a1 = the record index, a2 = an argument, a3 = a mode or -1): prepares the record at
// object + index * 0x50 + 0x4120 through the caller's scratch blocks. The two blocks at sp+0x50 / sp+0x80 are built by
// 0x08897B8C, the record's +0x44 / +0x48 pair and the scratch block go through 0x08866510 and, when a2 is set,
// 0x088A6C18; then the record's byte at +0x18 picks one of two paths: set — the signed byte at sp+0x23 becomes the
// record's +0x24 word and two bytes at sp+0x38 are copied into +4; clear — a table row is picked by the signed half at
// +0x1E (0x188 bytes per row under the object's +0x3F50 pointer), two bytes of it are copied to sp+0x38 and +4, the row
// is initialised through 0x0885DD1C and its +0x15 byte cleared. In the -1 mode the record is looked up through
// 0x088A26E0 and 0x088D7B04, the pair 0x088A5EE0 answers goes to +0x30/+0x34, and 0x0888A9B8 fills the +0x3E78 table
// row when 0x088DA0BC says so. The scratch is torn down (0x088978B4 on sp+0x80 and 0x088A7548). v0 = 0x088D7B04's
// answer, or 0 when it is null.

// 0x08B29930 (a0 = a 16-word output block, a1 = a name, a2 = its length): memsets a 0x20-byte scratch area, copies at
// most 32 bytes of the name there and lays the digest out in the 16 words: the words of the name (little-endian, 4 bytes
// each) go twice (a short name fills 4 words at +4..+0x10 and +0x2C..+0x38, 17 bytes or more fills 5 at +4..+0x14 and
// +0x2C..+0x30 plus +0x34/+0x38), and the global words at 0x08C95408 / +0x0C / +0x10 / +0x14 (short) or +0x18 / +0x1C /
// +0x20 / +0x24 (long) are XORed with the global salt at 0x08C95428 into +0, +0x14, +0x28, +0x3C. Whatever the path
// leaves uncovered at +0x18..+0x24 is zeroed. v0 = the salt (short name) or the last packed word (long name).

// The ll/sc decrement of a shared holder's use count: stores old - 1, leaves v1 = the address, a0 = the old value and
// v0 = 1 (the successful sc), and answers whether the count reached zero.
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old == 1u;
}
// `lw v1, 0(holder); lw v0, offset(v1); jalr v0` with a0 = the holder; v0 / v1 / f0 afterwards are the callee's.
u32 call_vtable(Runtime &rt, AllegrexContext &ctx, u32 holder, u32 offset) {
    const u32 table = word(rt, holder);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    const GuestResult result = call_guest(rt, ctx, function, {holder}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// One member of a chain of shared holders: release the count at +4 and, when that was the last reference, run the
// object's vtable +8 virtual and then the second count at +8 and the +0xC virtual.
void release_holder(Runtime &rt, AllegrexContext &ctx, u32 holder) {
    if (holder == 0u) return;
    if (!count_down(rt, ctx, holder + 4u)) return;
    call_vtable(rt, ctx, holder, 8u);
    if (!count_down(rt, ctx, holder + 8u)) return;
    call_vtable(rt, ctx, holder, 0xCu);
}
// A string-ish block: its header sits 0xC bytes before the data (the shared empty header 0x08D75298 is never freed), and
// the reference count 4 bytes before the data decides whether 0x08B93554 frees it.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 scratch) {
    const u32 header = data - 0xCu;
    if (header == 0x08D75298u) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B93554u, {header, scratch});
}

// 0x08BE207C (a0 = the object): destructor of a class holding a chain of shared holders: 0x84, 0x7C, 0x74, the string
// block at 0x50, 0x4C, 0x44, 0x3C and 0x2C are released, the vtable moves from 0x08CC6868 to 0x08CC6888 halfway and to
// 0x08CC3000 at the end, and the last holder at 0x24 only releases its +8 count through the +0xC virtual.
// v0 = 0x08CC3000.

// 0x08BD9C44 (a0 = the object): the same chain shape with 0x168, 0x160, 0x154, 0x14C, 0x144, the string block at 0x54,
// 0x50, 0x48 and 0x3C, the vtable 0x08CC5588 installed first, then a call of 0x08BD6D84(this), whose result is v0.

// 0x08977B14 (a0 = an object with its mode at +0x178, a1 = a second object, a2 / a3 = the two float outputs): the
// steering factor: the two axle angles from 0x08977290 / 0x089771F4, the magnitude's cubed falloff against the weight at
// 0x08CDE764 (times 5, over 640), the per-mode adjustment (0: a decay of the value at 0x08CF4078, 1: the maximum at
// 0x08CF4074 and the flag byte at 0x08CDE7B8, 2: dropped when the axes did not move by 0.015625, 3: kept unless bit
// 0x400 of 0x08977270's answer), then the clamped (against the weight and zero), normalised pair.

} // namespace
} // namespace new_batch_main_697
} // namespace gt::game

#define NEW697(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_697::name, gt::ReturnKind::kind)
