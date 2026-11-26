// New batch 944 (main session): hand-written structured C from `tools/disasm`. Draft only; the
// differential gate decides. The shared shapes are the virtual-call predicates (the pair comparison
// family, the accessor predicate, the float predicates), the guarded getter wrappers, the destructor
// pair and the dispatch builder. Every call site keeps the register the asm leaves in v0 at the jalr
// (some sites leave the vtable there, most leave the callee's own address).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_944 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that keeps the callee's v0 / v1 / f0 (the 64-bit results and the float builders).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v1, 0(object); lw v0, slot(v1); jalr v0`: the callee starts with v0 = its own address.
GuestResult call_through(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_keep(rt, ctx, target, arguments);
}
// `beq destination, temporary; lw a1, 0(temporary); jal release`: move the built value when the
// destination is not the temporary, then release the temporary; v0 is the release's result.
void release_result(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 temporary) {
    if (destination != temporary) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, temporary)});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {temporary});
}
s32 sext8(u32 value) { return static_cast<std::int8_t>(value); }
u32 bswap32(u32 value) { return __builtin_bswap32(value); }

// ---- the float predicates: a signed byte (or unsigned half) at a0+0x14 and the object's method at
// `offset` are converted to float, passed to 0x08B67720, truncated back and built at sp. ----
void predicate_float(Runtime &rt, AllegrexContext &ctx, u32 message, u32 offset, bool unsigned_half) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const float left = unsigned_half
                          ? static_cast<float>(static_cast<s32>(rt.memory().load16(self + 0x14u)))
                          : static_cast<float>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(self + 0x14u))));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 target = word(rt, word(rt, object) + offset);
    const GuestResult method = call_through(rt, ctx, target, {object});
    const float right = static_cast<float>(static_cast<s32>(method.v0));
    const float scaled = call_keepf(rt, ctx, 0x08B67720u, {}, {left, right}).f0;
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0), static_cast<u32>(static_cast<s32>(scaled))});
    release_result(rt, ctx, destination, frame.at(0));
}

// How the comparison result's v0 becomes the boolean handed to 0x08A97A6C.
enum class Order {
    LessThanOneUnsigned,     // sltiu a1, v0, 1
    GreaterThanZeroUnsigned, // sltu a1, zero, v0
    GreaterThanZeroSigned,   // slt a1, zero, v0
    Negative,                // slti a1, v0, 0
};

// The 64-bit pair predicates: a3's object method at +0x58 leaves v0/v1 (the asm leaves the vtable in
// v0 and the callee in a1 at the jalr), the comparison combines it with a0+0x18/+0x1C, and its low
// word turns into the bool built at sp.
void predicate_pair_compare(Runtime &rt, AllegrexContext &ctx, u32 message, u32 compare, Order order) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const u32 low = word(rt, self + 0x18u);
    const u32 high = word(rt, self + 0x1Cu);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + 0x58u);
    ctx.gpr[2] = table;   // v0 = the vtable at the jalr
    ctx.gpr[5] = target;  // a1 = the callee's address at the jalr
    const GuestResult method = call_keep(rt, ctx, target, {object});
    const GuestResult compared = call_keep(rt, ctx, compare, {low, high, method.v0, method.v1});
    u32 flag = 0u;
    switch (order) {
    case Order::LessThanOneUnsigned: flag = compared.v0 < 1u ? 1u : 0u; break;
    case Order::GreaterThanZeroUnsigned: flag = 0u < compared.v0 ? 1u : 0u; break;
    case Order::GreaterThanZeroSigned: flag = static_cast<s32>(compared.v0) > 0 ? 1u : 0u; break;
    case Order::Negative: flag = static_cast<s32>(compared.v0) < 0 ? 1u : 0u; break;
    }
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0), flag});
    release_result(rt, ctx, destination, frame.at(0));
}

// The accessor predicate: the object's +0x5C method fills the word at sp (a0 = sp, a1 = object); the
// word is compared with a0+0x14 (sltu: value < limit), the bool is built at sp+4 and stored, and
// 0x08A69D68 releases sp.
void accessor_predicate(Runtime &rt, AllegrexContext &ctx, u32 message) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 target = word(rt, word(rt, object) + 0x5Cu);
    call_through(rt, ctx, target, {frame.at(0), object});
    const u32 limit = word(rt, self + 0x14u);
    const u32 value = word(rt, frame.at(0));
    call(rt, ctx, 0x08A97A6Cu, {frame.at(4), value < limit ? 1u : 0u});
    if (destination != frame.at(4)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(4))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4)});
    ctx.gpr[2] = call(rt, ctx, 0x08A69D68u, {frame.at(0)});
}

// A guarded getter wrapper: build a handle at sp+4, read the word at its +0x14 before releasing it
// (the release call's delay slot), and only when a2 <= 0 build the result at sp and store it. v0 is
// the release's result on the guarded-out path, the final release's otherwise.
void wrapper_guard(Runtime &rt, AllegrexContext &ctx, u32 first, u32 getter, u32 builder) {
    GuestFrame frame(ctx, 0x30u);
    const u32 destination = ctx.gpr[4];
    const u32 flag = ctx.gpr[6];
    call(rt, ctx, first, {frame.at(4)});
    const u32 handle = word(rt, frame.at(4));
    if (handle == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
    const u32 field = word(rt, handle + 0x14u);
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(4)});
    if (static_cast<s32>(flag) > 0) return;
    const u32 value = call(rt, ctx, getter, {field});
    call(rt, ctx, builder, {frame.at(0), value});
    release_result(rt, ctx, destination, frame.at(0));
}

// ---- 0x08A9661C: a signed byte at a0+0x14 scaled by the object's +0x34 method. ----
// ---- 0x08AB0788: an unsigned half at a0+0x14 scaled by the object's +0x48 method. ----

// The pair predicates through the comparison helpers at the given addresses.
void func_08A8B678(Runtime &rt, AllegrexContext &ctx) {
    predicate_pair_compare(rt, ctx, 0x08CB1BE8u, 0x08B71980u, Order::LessThanOneUnsigned);
}
// ---- 0x08A6A038: the +0x5C accessor compared with a0+0x14 (value < limit). ----

// ---- 0x088288DC / 0x0882897C / 0x08828A1C: build a handle, guard on a2, then getter and builder. ----

// ---- 0x0882E98C: build a handle at sp+4, unwrap it through 0x088294D0, release, then chain two
// getters and build the result at sp. ----

// ---- 0x0885EA5C: the +0x5AC object's two probes decide the returned flag: 1 for a non-positive
// 64-bit value or an exact zero within 0x3A99, 0 for a large value (after 0x2710 ticks) or a
// non-zero small one (after 0x3E8 ticks). ----

// ---- 0x0887A038 (a0 = destination, a1 = source): build the temporary at sp+4 through 0x08AB85F0
// and two 0x08AB856C calls, merge it into a0, then release the word (0x08D75298 is the sentinel that
// skips the release and is what the asm leaves in v0). ----

// ---- 0x08895BFC (a0 = object): zero the 0x08B36018 range, fill the record 0x08894B60 returns with
// zeros (and a byte-swapped word at +0x14), reset the object's counters and zero the two 0x0892B440
// blocks; v0 is the second block call's result, as the asm leaves it. ----

// ---- 0x08896B40 (a0 = object): store the vtable, release a0+0xDC, then release the two member
// blocks at +0xD0 / +0x8C through their vtable's first slot, and tail-call 0x08898F78. ----

// ---- 0x08928880 (a0 = source, a1 = destination): copy a batch of fields from the a0 object into
// a1 through the 0x0892xxxx accessors. ----

// ---- 0x0897BB68 (a0 = object): clear the bits from a0+0x74/a0+0x78 to a0+0x7C/a0+0x80 (one bit per
// iteration, the word pointer advancing every 0x20 bits); v0 = 1 << the last bit index. ----

// ---- 0x089ADA98: build a handle at sp+8, unwrap it through 0x089AA5A0, release it, and when
// non-null build the result at sp+4 through 0x08969188 and store it. ----

// ---- 0x08A1D398 (a0 = object, a1/a3 = words, a2 = record): call 0x08A1CDBC on a0+8, store a1 at
// +0x38, copy the record's two strings into +0x44 / +0x54, store a3 at +0x68, then run the
// 0x08ACB1BC loop and tail-call 0x08A1CB1C. ----

// ---- 0x08A3AD70 (a1 = source, a2 = flag, a3 = holder): with the flag set use the holder object's
// +0x54 method as the scale (3.0f otherwise), build a handle at sp through 0x08A3A9D0 and pass the
// scale to 0x08A3A480. ----

// ---- 0x08A72AF4 (a0 = list): reset through 0x08A728FC / 0x08A729F8, then for every entry of the
// array the count at +0xC selects, build a temporary and assign it into the entry. ----
void func_08A72AF4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 list = ctx.gpr[4];
    call(rt, ctx, 0x08A728FCu, {list, 0u});
    call(rt, ctx, 0x08A729F8u, {list, 0u});
    u32 count = word(rt, list + 0xCu);
    if (static_cast<s32>(count) <= 0) {
        ctx.gpr[2] = count;
        return;
    }
    u32 index = 0u;
    do {
        const u32 element = word(rt, list) + (index << 2);
        call(rt, ctx, 0x08A76AA0u, {frame.at(0)});
        if (element != frame.at(0)) call(rt, ctx, 0x08A5BAD8u, {element, word(rt, frame.at(0))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
        count = word(rt, list + 0xCu);
        ++index;
        ctx.gpr[2] = 0u;  // the loop's slt leaves 0 in v0 on the exit path
    } while (index < count);
}

// ---- 0x08B100D0 (a0 = destination, a3 = byte flag): fill sp+0x10 through 0x08B0F4E0 (the masked
// flag in t0) and copy the 0x3C bytes to a0; v0 = a0. ----

// ---- 0x08B3209C (a0 = result, a1 = record): copy the record's 0x14 bytes around a 0x08B7C830 call
// and return 1 when 0x08C7AEE0's result is non-negative. ----

// ---- 0x08B47FD8 (a0 = object, a1/a2/a3 = record words): allocate two 0x10-byte blocks, fill the
// first with the record, hand both to 0x08C524C4, then tail-call 0x08B1FBEC with the second. ----
void func_08B47FD8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 word1 = ctx.gpr[5];
    const u32 word2 = ctx.gpr[6];
    const u32 word3 = ctx.gpr[7];
    const u32 first = call(rt, ctx, 0x08B2CB18u, {word(rt, self + 0x28u), 0xCu, 0x10u});
    if (first != 0u) {
        put(rt, first, word1);
        put(rt, first + 4u, word2);
        put(rt, first + 8u, word3);
    }
    const u32 second = call(rt, ctx, 0x08B2CB18u, {word(rt, self + 0x28u), 0xCu, 0x10u});
    if (second != 0u) call(rt, ctx, 0x08C524C4u, {second, first});
    tail_call(ctx, 0x08B1FBECu, {self + 0xCu, second});
}

// ---- 0x08BDD7D8 / 0x08BDDCC8 (a0 = first, a1 = end): destroy the 0x1C-byte records in
// [first, end), running 0x08BDD7C8 on the two words at +0x10/+0x14, releasing the +0x10 object, then
// 0x08BDD7B8 on the two words at +0 and +4 and releasing the +0 object. ----
void destroy_range(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 first = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    if (first == end) return;
    u32 current = first;
    u32 slot = first + 4u;
    for (;;) {
        call(rt, ctx, 0x08BDD7C8u, {word(rt, slot + 0xCu), word(rt, slot + 0x10u), frame.at(1)});
        current += 0x1Cu;
        u32 value = word(rt, slot + 0xCu);
        if (value != 0u) call(rt, ctx, 0x08B35254u, {value});
        call(rt, ctx, 0x08BDD7B8u, {word(rt, slot), word(rt, slot + 4u), frame.at(0)});
        value = word(rt, slot);
        slot += 0x1Cu;
        if (value != 0u) call(rt, ctx, 0x08B35254u, {value});
        if (end == current) return;
    }
}

// ---- 0x08BFFC10 (a0 = destination, t0 = holder): index = t0[1], offset = t0[0]. An odd index means
// the offset is a vtable slot (base + index/2 is the object), an even one the function itself. The
// result is built at sp+4 and both temporaries are released. ----

// ---- 0x08AC87E4: tear down the global state: store 0 at 0x08CFFF99, release 0x08CFFF9C, clear it,
// spin on 0x08CFFF98 through 0x08B2EFDC while it is zero, then run the shutdown calls and tail-call
// the last one. ----

// ---- 0x08C5B9A4 (a0 unused, a1 = byte, a2 = source, a3 = next byte, t0 = distance, t1 = buffer,
// t2 = base, t3 = cursor): insert the byte and copy the tail. The zero-distance path still runs the
// insertion, with the cursor's own word as the gap. ----

// ---- 0x08C32210 (a1 = input, a2 = holder): build at sp+8 through 0x08C31850, then either wrap the
// temporary at sp+4 (null holder) or call the holder object's +0xF4 virtual with the frame. ----

} // namespace
} // namespace new_batch_main_944
} // namespace gt::game

#define NEW944(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_944::name, gt::ReturnKind::kind)
NEW944(0x08A72AF4u, func_08A72AF4, Word);
NEW944(0x08A8B678u, func_08A8B678, Word);
NEW944(0x08B47FD8u, func_08B47FD8, Word);
