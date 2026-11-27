// New batch 12 (main session): functions emitted by the recompiler that no src/ file defines or
// registers. Written by hand from `tools/disasm` as structured C; this is a draft batch, not yet
// verified by the differential gate. The helpers below cover the shapes the batch repeats: a call
// that leaves v0/v1/f0 as the original leaves them (the predicate shapes read the callee's v1, the
// high word of a 64-bit result), the compiler's virtual call, the sret store-and-release tail, and
// the four predicate families.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_12 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u64 = std::uint64_t;
using s64 = std::int64_t;
using s16 = std::int16_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 as the callee left them: the predicate shapes read the callee's
// v1 after the call (the high word of a 64-bit result), and a callee that never sets v0 hands the
// previous one on to the code after it.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments (f12-f15).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The compiler's virtual call: v0 = the vtable (object[0]), the callee = vtable[offset], a0 = object.
// v0 stays the vtable at the call, as the original's `lw v0, 0(s0)` leaves it.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;
    return call_keep(rt, ctx, function, arguments);
}
// The sret tail the predicate shapes share: when the destination is not the temporary, move the
// constructed value there (the delay slot of the release call reads it), then release the
// temporary; v0 is the release's result, as the asm leaves it.
void release_result(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 temporary) {
    if (destination != temporary) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, temporary)});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {temporary});
}

// The 64-bit equality predicate: the vtable method at a3's object +0x40 returns a 64-bit value in
// v0/v1, compared with the word pair at a0+0x18/+0x1c; the bool goes through 0x08A97A6C at sp.
void predicate_pair64(Runtime &rt, AllegrexContext &ctx, u32 message, bool equal) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const u32 low = word(rt, self + 0x18u);
    const u32 high = word(rt, self + 0x1Cu);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult method = call_virtual(rt, ctx, object, 0x40u, {object});
    const bool match = method.v0 == low && method.v1 == high;
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0), (equal ? match : !match) ? 1u : 0u});
    release_result(rt, ctx, destination, frame.at(0));
}

// How the paired comparison result at v0 is turned into the bool.
enum class PairOrder {
    LessThanOneUnsigned,     // sltiu a1, v0, 1
    GreaterThanZeroUnsigned, // sltu a1, zero, v0
    GreaterThanZeroSigned,   // slt a1, zero, v0
    NonNegative,             // slti v0, v0, 0; xori a1, v0, 1
    Negative,                // slti a1, v0, 0
    LessThanOneSigned,       // slti a1, v0, 1
};
// a3's object method at +0x58 returns a 64-bit value; `compare` combines it with a0's +0x18/+0x1c
// pair and returns another 64-bit value whose low word the order test turns into the bool.
void predicate_pair_compare(Runtime &rt, AllegrexContext &ctx, u32 message, u32 compare, PairOrder order) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const u32 low = word(rt, self + 0x18u);
    const u32 high = word(rt, self + 0x1Cu);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult method = call_virtual(rt, ctx, object, 0x58u, {object});
    const GuestResult compared = call_keep(rt, ctx, compare, {low, high, method.v0, method.v1});
    const s32 signed_value = static_cast<s32>(compared.v0);
    u32 flag = 0u;
    switch (order) {
    case PairOrder::LessThanOneUnsigned: flag = compared.v0 < 1u ? 1u : 0u; break;
    case PairOrder::GreaterThanZeroUnsigned: flag = 0u < compared.v0 ? 1u : 0u; break;
    case PairOrder::GreaterThanZeroSigned: flag = 0 < signed_value ? 1u : 0u; break;
    case PairOrder::NonNegative: flag = signed_value < 0 ? 0u : 1u; break;
    case PairOrder::Negative: flag = signed_value < 0 ? 1u : 0u; break;
    case PairOrder::LessThanOneSigned: flag = signed_value < 1 ? 1u : 0u; break;
    }
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0), flag});
    release_result(rt, ctx, destination, frame.at(0));
}

// The object variant: the comparison result (v0/v1) builds a predicate object at sp through
// 0x08A8B098 instead of a bool.
void predicate_pair_object(Runtime &rt, AllegrexContext &ctx, u32 message, u32 compare) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const u32 low = word(rt, self + 0x18u);
    const u32 high = word(rt, self + 0x1Cu);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult method = call_virtual(rt, ctx, object, 0x58u, {object});
    const GuestResult compared = call_keep(rt, ctx, compare, {low, high, method.v0, method.v1});
    call_keep(rt, ctx, 0x08A8B098u, {frame.at(0), 0u, compared.v0, compared.v1});
    release_result(rt, ctx, destination, frame.at(0));
}

// The float predicate: a signed byte (or unsigned half) at a0+0x14 and the vtable method's int
// result are converted to float, passed to 0x08B67720, truncated back to int and built at sp.
void predicate_float(Runtime &rt, AllegrexContext &ctx, u32 message, u32 offset, bool unsigned_half) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const float left = unsigned_half
                           ? static_cast<float>(rt.memory().load16(self + 0x14u))
                           : static_cast<float>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(self + 0x14u))));
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult method = call_virtual(rt, ctx, object, offset, {object});
    const float right = static_cast<float>(static_cast<s32>(method.v0));
    const GuestResult result = call_keepf(rt, ctx, 0x08B67720u, {}, {left, right});
    call(rt, ctx, 0x08A85B1Cu, {frame.at(0), static_cast<u32>(static_cast<s32>(result.f0))});
    release_result(rt, ctx, destination, frame.at(0));
}

// The member order of the accessor predicate's comparison.
enum class MemberOrder {
    GreaterEqual, // sltu a1, v0, limit; xori a1, a1, 1
    LessEqual,    // sltu a1, limit, v0; xori a1, a1, 1
    NotEqual,     // xor a1, limit, v0; sltu a1, zero, a1
    Less,         // sltu a1, v0, limit
};
// The accessor predicate: the object's method at +0x5C fills the word at sp (a0 = sp, a1 = object);
// the word is compared with a0+0x14, the bool built at sp+4 and stored, and 0x08A69D68 releases sp.
void accessor_predicate(Runtime &rt, AllegrexContext &ctx, u32 message, MemberOrder order) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 table = word(rt, object);
    const u32 method = word(rt, table + 0x5Cu);
    call_keep(rt, ctx, method, {frame.at(0), object});
    const u32 limit = word(rt, self + 0x14u);
    const u32 value = word(rt, frame.at(0));
    u32 flag = 0u;
    switch (order) {
    case MemberOrder::GreaterEqual: flag = value >= limit ? 1u : 0u; break;
    case MemberOrder::LessEqual: flag = limit >= value ? 1u : 0u; break;
    case MemberOrder::NotEqual: flag = value != limit ? 1u : 0u; break;
    case MemberOrder::Less: flag = value < limit ? 1u : 0u; break;
    }
    call(rt, ctx, 0x08A97A6Cu, {frame.at(4), flag});
    if (destination != frame.at(4)) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(4))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4)});
    ctx.gpr[2] = call(rt, ctx, 0x08A69D68u, {frame.at(0)});
}

// A guarded getter wrapper: build a handle at sp+4, read the word at its +0x14 before releasing it
// (the release call's delay slot), and only when a2 <= 0 build the result at sp and store it.
// v0 is the release's result on the guarded-out path, the final release's otherwise.
void wrapper_guard(Runtime &rt, AllegrexContext &ctx, u32 first, u32 getter, u32 builder, u32 message) {
    GuestFrame frame(ctx, 0x30u);
    const u32 destination = ctx.gpr[4];
    const u32 flag = ctx.gpr[6];
    call(rt, ctx, first, {frame.at(4)});
    const u32 handle = word(rt, frame.at(4));
    if (handle == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 field = word(rt, handle + 0x14u);
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(4)});
    if (static_cast<s32>(flag) > 0) return;
    const u32 value = call(rt, ctx, getter, {field});
    call(rt, ctx, builder, {frame.at(0), value});
    release_result(rt, ctx, destination, frame.at(0));
}

// A virtual dispatch through a holder in t0: index = t0[1], offset = t0[0]. An odd index means the
// offset is a vtable slot (base + index/2 is the object), an even one the function itself. The
// result is built at sp+4 (int through 0x08A85B1C or float through 0x08A87EEC) and both
// temporaries are released.
void dispatch_builder(Runtime &rt, AllegrexContext &ctx, u32 first, u32 builder, bool float_result) {
    GuestFrame frame(ctx, 0x20u);
    const u32 destination = ctx.gpr[4];
    const u32 holder = ctx.gpr[8];
    const u32 index = word(rt, holder + 4u);
    const u32 offset = word(rt, holder);
    call(rt, ctx, first, {frame.at(0)});
    const u32 base = word(rt, frame.at(0));
    const u32 half = static_cast<u32>(static_cast<s32>(index) >> 1);
    const u32 address = base + half;
    const u32 target = (index & 1u) != 0u ? word(rt, word(rt, address) + offset) : offset;
    const u32 temporary = frame.at(4);
    if (float_result) {
        const GuestResult result = call_keep(rt, ctx, target, {address});
        call_keepf(rt, ctx, builder, {temporary}, {result.f0});
    } else {
        const GuestResult result = call_keep(rt, ctx, target, {address});
        call(rt, ctx, builder, {temporary, result.v0});
    }
    if (destination != temporary) call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, temporary)});
    call(rt, ctx, 0x08A5B9ECu, {temporary});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
}

// 0x08A83824 (a0 = comparison, a1 = result, a3 = holder): 64-bit equality of a0+0x18/+0x1c with the
// holder object's vtable method at +0x40.
void func_08A83824(Runtime &rt, AllegrexContext &ctx) { predicate_pair64(rt, ctx, 0x08CB168Cu, true); }
// 0x08A838C0: the same pair, inequality.
void func_08A838C0(Runtime &rt, AllegrexContext &ctx) { predicate_pair64(rt, ctx, 0x08CB168Cu, false); }
// 0x08A9661C: a signed byte at a0+0x14 scaled by the object's +0x34 method.
// 0x08AB0788: an unsigned half at a0+0x14 scaled by the object's +0x48 method.
// 0x08A8B678: v0 < 1.
// 0x08A8B718: 0 < v0 (unsigned).
// 0x08A8B7B8: 0 < v0 (signed).
// 0x08A8B858: v0 >= 0.
// 0x08A8B8FC: v0 < 0.
// 0x08A8B99C: v0 < 1 (signed).
void func_08A8B99C(Runtime &rt, AllegrexContext &ctx) {
    predicate_pair_compare(rt, ctx, 0x08CB1BE8u, 0x08B71C58u, PairOrder::LessThanOneSigned);
}
// 0x08A8B224..0x08A8B5D4: the pair through a comparison that returns a 64-bit value built by
// 0x08A8B098.
void func_08A8B224(Runtime &rt, AllegrexContext &ctx) { predicate_pair_object(rt, ctx, 0x08CB1BE8u, 0x08B71328u); }
// 0x08A69E50 / 0x08A69F94 / 0x08A6A0D8 / 0x08A6A038: the +0x5C accessor compared with a0+0x14.
// 0x088288DC / 0x0882897C / 0x08828A1C: build a handle, guard on a2, then getter and builder.
// 0x0882E98C: build a handle at sp+4, unwrap it through 0x088294D0, release, then chain two getters
// and build the result at sp.
// 0x08BFFC10 / 0x08C0AF24 / 0x08C0B060 / 0x08C0B204 / 0x08C1984C / 0x08C1B07C: dispatch through
// the holder in t0 and build the result (int or float).

// 0x08A22210 (a2 = flag, a3 = holder): build a handle at sp, guard on the flag, then call the
// holder object's +0x54 method and pass its float to 0x08AB87B4 at handle+0x14.

// 0x08A72AF4 (a0 = list): reset through 0x08A728FC / 0x08A729F8, then for every entry of the
// array at a0 the count at +0xC selects, build a temporary and assign it into the entry.

// 0x08ABA868 (a0 = object): allocate a 0x80-byte block per a global count, set up the four calls,
// then register a handler and store it at a0+0x40.

// 0x08AC36E0 (a0 = object, a1 = text): with the object's word non-negative, re-run the reset; when
// the text is non-empty, parse it into a0+0xC and store the result at a0/+4/+8; the object's
// +0xC byte is cleared on failure; v0 = 1 on success, 0 otherwise.

// 0x08AC7384 (a0 = object, a1/a2/a3 = outputs): lock through 0x08B344E0, read three words
// through 0x08C7AB30; on a negative result set a0's +0x28 flag, else store the words into the
// outputs; v0 = 1 on success, 0 otherwise.

// 0x089ADA98: build a handle at sp+8, unwrap it through 0x089AA5A0, release it, and when non-null
// build the result at sp+4 through 0x08969188 and store it.

// 0x089C5BB8 (a0 = destination, a1 = source): build the wrapper at sp+8, the handle at sp, then
// pass both to 0x08A9D890, release the handle, release the source and the wrapper.
void func_089C5BB8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 wrapper_slot = frame.at(8);
    const u32 value = call(rt, ctx, 0x08A7BDBCu, {source});
    put(rt, frame.at(4), value);
    call(rt, ctx, 0x08A7C0E0u, {wrapper_slot, frame.at(4)});
    call(rt, ctx, 0x08A9DD14u, {frame.at(0)});
    const u32 handle = word(rt, frame.at(0));
    if (handle == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6398u});
    call(rt, ctx, 0x08A9D890u, {handle, destination, wrapper_slot});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
    call(rt, ctx, 0x089C4EFCu, {source});
    ctx.gpr[2] = call(rt, ctx, 0x08A5B9ECu, {wrapper_slot});
}

// 0x0887A038 (a0 = destination, a1 = source): build the temporary at sp+4 through 0x08AB85F0 and
// two 0x08AB856C calls, merge it into a0 through 0x088757EC / 0x08879E14, then release the word.

// 0x08A3AD70 (a1 = source, a2 = flag, a3 = holder): with the flag set use the holder object's +0x54
// method as the scale (3.0f otherwise), build a handle at sp through 0x08A3A9D0 and pass the scale
// to 0x08A3A480.

// 0x08B100D0 (a0 = destination, a3 = byte flag): fill sp+0x10 through 0x08B0F4E0 (the masked flag
// in t0) and copy the 0x3C bytes to a0; v0 = a0.

// 0x08B47FD8 (a0 = object, a1/a2/a3 = record words): allocate two 0x10-byte blocks, fill the first
// with the record, hand both to 0x08C524C4, then tail-call 0x08B1FBEC with the second.

// 0x08896B40 (a0 = object): store the vtable, release a0+0xDC, then release the two member blocks
// at +0xD0 / +0x8C through their vtable's first slot, and tail-call 0x08898F78.

// 0x08C32210 (a1 = input, a2 = holder): build at sp+8 through 0x08C31850, then either wrap the
// temporary at sp+4 (null holder) or call the holder object's +0xF4 virtual with the frame; the
// result temporary is released and its v0 returned.

// ---- main-session batch 12, part 2: the remaining functions of the batch ----

s32 sext8(u32 value) { return static_cast<std::int8_t>(value); }
u32 bswap32(u32 value) { return __builtin_bswap32(value); }

// 0x08B01154 (a0 = index holder, a1 = table holder): clear the flag byte at +0x51 of every node from
// the one a0+4 selects (nodes are 84 bytes, the next index is the node's signed halfword at +0x52)
// until a node is null or a child index is negative.

// 0x08B3209C (a0 = result, a1 = record): copy the record's 0x14 bytes around a 0x08B7C830 call and
// return 1 when 0x08C7AEE0's result is non-negative.

// 0x08B85CB8 (a0 = object): store the vtable, release the 26 pointers at +0x3C..+0xA0 and the one at
// +0x38 through 0x08A5B930, then tail-call 0x08A7D4E4.
void func_08B85CB8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CB6FE0u);
    if (self + 0x3Cu != 0u && self + 0x3Cu != self + 0xA4u) {
        u32 slot = self + 0xA0u;
        for (u32 count = 0x1Au; count != 0u; --count) {
            const u32 value = word(rt, slot);
            if (value != 0u) call(rt, ctx, 0x08A5B930u, {value});
            slot -= 4u;
        }
    }
    const u32 holder = word(rt, self + 0x38u);
    if (holder != 0u) call(rt, ctx, 0x08A5B930u, {holder});
    tail_call(ctx, 0x08A7D4E4u, {self});
}

// 0x08BB8FA8 (a0 = object): store the vtable, destroy a0+0x844, call the two embedded vtables' first
// slots (the objects at a0+0x438 / a0+0x30, a0 = the decremented slot pointers), then 0x08B28DC4(a0+4),
// 0x08AF20E8(a0) and tail-call 0x08B64654(a0).
void func_08BB8FA8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CC3B30u);
    call(rt, ctx, 0x088F6284u, {self + 0x844u});
    if (self + 0x30u != 0u && self + 0x30u != self + 0x840u) {
        u32 slot = self + 0x840u;
        u32 object = self + 0x438u;
        for (u32 count = 2u; count != 0u; --count) {
            const u32 value = word(rt, object);
            slot -= 0x408u;
            call_keep(rt, ctx, word(rt, value), {slot});
            object -= 0x408u;
        }
    }
    call(rt, ctx, 0x08B28DC4u, {self + 4u});
    call(rt, ctx, 0x08AF20E8u, {self});
    tail_call(ctx, 0x08B64654u, {self});
}

// 0x08C5B9A4 (a0-a3, t0-t3): insert the byte a1 into the buffer and copy the tail. When t0 is zero
// the function returns a2 untouched; otherwise it recurses into 0x08C5942C, stores a1 at the returned
// position, copies [a2v or s0-value, s0) and stores the new cursor at t3.

// 0x0884D8B0 (a2 = flag, a3 = holder): with the flag positive, build a handle at sp, unwrap it
// through 0x0884D3D8, then call the holder object's +0x3C virtual and pass both results to
// 0x088970A0.
void func_0884D8B0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 holder = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) <= 0) return;
    call(rt, ctx, 0x0884D660u, {frame.at(0)});
    const u32 handle = word(rt, frame.at(0));
    if (handle == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D198u});
    const u32 inner = call(rt, ctx, 0x0884D3D8u, {handle});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0)});
    const u32 object = word(rt, holder);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D158u});
    const GuestResult value = call_virtual(rt, ctx, object, 0x3Cu, {object});
    ctx.gpr[2] = call(rt, ctx, 0x088970A0u, {inner, value.v0});
}

// 0x0885EA5C (a0 = object): the +0x5AC object's two probes decide the returned flag: 1 for a
// non-positive 64-bit value or an exact zero within 0x3A99, 0 for a large value (after 0x2710 ticks)
// or a non-zero small one (after 0x3E8 ticks); v0 = the flag.

// 0x08895BFC (a0 = object): zero the 0x08B36018 range, then fill the record 0x08894B60 returns with
// zeros (and a byte-swapped word at +0x14), reset the object's counters and zero the two 0x0892B440
// blocks.

// 0x088EC09C (a0 = object): read the count through 0x08B0979C / 0x08B099A4 and return 4 when the
// +0xAC virtual reports, 3 when neither +0xB8 nor the 0x560 limit trips, and 0 with a 0x08B0A998
// tick (0x2E or 0x2D) otherwise.
void func_088EC09C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 block = call(rt, ctx, 0x08B0979Cu, {});
    const u32 count = call(rt, ctx, 0x08B099A4u, {block, 0u});
    const u32 total = (count << 6) - (count << 2);
    const u32 object = word(rt, self + 0x538u);
    const GuestResult first = call_keep(rt, ctx, word(rt, word(rt, object) + 0xACu), {object});
    if (first.v0 != 0u) { ctx.gpr[2] = 4u; return; }
    const u32 object2 = word(rt, self + 0x538u);
    const GuestResult second = call_keep(rt, ctx, word(rt, word(rt, object2) + 0xB8u), {object2});
    if (second.v0 != 0u) {
        call(rt, ctx, 0x08B0A998u, {0x2Eu});
        ctx.gpr[2] = 0u;
        return;
    }
    if (total < word(rt, self + 0x560u)) {
        call(rt, ctx, 0x08B0A998u, {0x2Du});
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = 3u;
}

// 0x08928880 (a0 = source, a1 = destination): copy a batch of fields (two words, one virtual's word,
// two floats and another word) from the a0 object into a1, through the 0x0892xxxx accessors.

// 0x0897BB68 (a0 = object): clear the bits from a0+0x74/a0+0x78 to a0+0x7C/a0+0x80 (one bit per
// iteration, the word pointer advancing every 0x20 bits); v0 = 1 << the last bit index.

// 0x08A1D398 (a0 = object, a1/a3 = words, a2 = record): call 0x08A1CDBC on a0+8, store a1 at +0x38,
// copy the record's two strings into +0x44 / +0x54, store a3 at +0x68, then run the 0x08ACB1BC loop
// and tail-call 0x08A1CB1C.

// 0x08A32974 (a0 = object): store the vtable, release a0+0x42C through its second virtual, free the
// a0+0x420 array, release a0+0x14 and call 0x08A780B0(a0); v0 = that last call's result.
void func_08A32974(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    put(rt, self, 0x08CB8AC8u);
    const u32 inner = word(rt, self + 0x42Cu);
    if (inner != 0u) call_keep(rt, ctx, word(rt, word(rt, inner) + 4u), {inner});
    put(rt, self + 0x42Cu, 0u);
    const u32 array = self + 0x420u;
    call(rt, ctx, 0x08B876E0u, {word(rt, array), word(rt, array + 4u), frame.at(0)});
    const u32 base = word(rt, array);
    const u32 count = static_cast<u32>(((static_cast<s32>(word(rt, array + 8u) - base)) >> 2) << 2);
    if (base != 0u) call(rt, ctx, 0x08A5BC54u, {base, count, 4u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x14u});
    ctx.gpr[2] = call(rt, ctx, 0x08A780B0u, {self});
}

// 0x08A6C528 (a0 = vector, a1 = offset, a2 = source, a3 = length): grow the a0+0x14 vector when needed
// and tail-call 0x08B79A88(data + a1, source, length).
void func_08A6C528(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 offset = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const u32 length = ctx.gpr[7];
    const u32 end = offset + length;
    u32 table = word(rt, self + 0x14u);
    if (end > word(rt, table - 0xCu)) {
        call(rt, ctx, 0x08B86674u, {self + 0x14u, end, 0u});   // a1 = offset + length, a2 = 0 (the asm sets both)
        table = word(rt, self + 0x14u);
    }
    if (static_cast<s32>(word(rt, table - 4u)) >= 0) {
        call(rt, ctx, 0x08B88FF8u, {self + 0x14u});
        table = word(rt, self + 0x14u);
    }
    tail_call(ctx, 0x08B79A88u, {table + offset, source, length});
}

// 0x08AA9AC8 (a0 = object): build a 0x08B85EB8 record, unwrap it through 0x08C31E0C, then use the
// object's +0xC word as a sentinel: the word at +0x14 of the result when it matches, 0 otherwise.

// 0x08B05F9C (a0 = object, a1 = other, f12/f13/f14): result = (first + second) - clamp(f14, -f13,
// f13) * f12, where first / second are the two 0x08BBBEC4 samples.
void func_08B05F9C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const float f23 = ctx.fpr[12];
    const float f21 = ctx.fpr[13];
    const float f22 = ctx.fpr[14];
    const float first = call_keepf(rt, ctx, 0x08BBBEC4u, {self, object + 0x38u}, {}).f0;
    const float second = call_keepf(rt, ctx, 0x08BBBEC4u, {self + 0xCu, object + 0x50u}, {}).f0;
    const float sum = first + second;
    float clamped = (f22 < -f21) ? -f21 : f22;
    if (!(clamped < f21)) clamped = f21;
    ctx.fpr[0] = sum - clamped * f23;
}

// 0x08B72704 (a0 = code, a1 = argument): dispatch on the low byte / 0x70 group; 0 for 0xFF, 0x50 and
// the 0 / 0x10 groups, a tail call for 0x20 / 0x40 / 0x30, and a 0x08B77B04 tick before the 0x60 /
// 0x70 default (which tail-calls 0x08B726FC).
void func_08B72704(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 code = ctx.gpr[4] & 0xFFu;
    const u32 argument = ctx.gpr[5];
    if (code == 0xFFu) { ctx.gpr[2] = 0u; return; }
    const u32 group = code & 0x70u;
    if (group == 0x20u) { tail_call(ctx, 0x08B726ECu, {argument}); return; }
    if (group < 0x21u) { ctx.gpr[2] = 0u; return; }
    if (group == 0x40u) { tail_call(ctx, 0x08B726FCu, {argument}); return; }
    if (group == 0x50u) { ctx.gpr[2] = 0u; return; }
    if (group == 0x30u) { tail_call(ctx, 0x08B726F4u, {argument}); return; }
    call(rt, ctx, 0x08B77B04u, {});
    if (group == 0x10u) { ctx.gpr[2] = 0u; return; }
    call(rt, ctx, 0x08B77B04u, {});
    tail_call(ctx, 0x08B726FCu, {argument});
}

// 0x08B95080 (a0 = object, a1 = node): walk the +0xC chain, recurse into each node's +0xC child,
// destroy the +0x14 object and release the node; v0 = the last 0x08B35254 result.

// 0x08BDD7D8 / 0x08BDDCC8 (a0 = first, a1 = end): destroy the 0x1C-byte records in [first, end),
// running 0x08BDD7C8 on the two words at +0x10/+0x14, releasing the +0x10 object, then 0x08BDD7B8 on
// the two words at +0 and +4 and releasing the +0 object.
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

// 0x08C2E724 (a0 = first, a1 = end, a2 = object): call the object's +8 virtual with each slot from
// a0-4 downward; a non-zero answer destroys the slot (0x08A5BAD8) and loops, a zero answer stops and
// tail-calls 0x08A5BAD8 with the end slot.

// 0x08C5942C (a0, a1 = byte, a2 = source, a3, t0, t1): insert the byte a1 before a2 when the source
// byte is in range and positive (recurse), then copy [a2 or t1-byte, t1) forward; v0 = the end cursor.
void func_08C5942C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    const u32 t1 = ctx.gpr[9];
    const u32 s2 = static_cast<u32>(sext8(a1));
    const s32 span = static_cast<s32>(t1 - t0);
    const s32 value = static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(a2)));
    u32 source = t0;
    u32 destination = a0;
    if (value < span && value > 0) {
        const u32 bit = (a3 ^ 1u) != 0u ? 1u : 0u;
        const u32 t1v = t1 - static_cast<u32>(value);
        const u32 result = call(rt, ctx, 0x08C5942Cu, {a0, s2, a2 + bit, a3 - bit, t0, t1v, ctx.gpr[10], ctx.gpr[11]});
        destination = result + 1u;
        rt.memory().store8(result, static_cast<std::uint8_t>(s2));
        source = t1 - static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(a2))));
    }
    u32 from = source;
    u32 to = destination;
    while (from != t1) {
        rt.memory().store8(to, rt.memory().load8(from));
        ++from;
        ++to;
    }
    ctx.gpr[2] = to;
}

// 0x08C74D38 (a0 = destination, a1 = offset, a2 = length, a3 = source, t0 = count): run 0x08C74B68,
// then copy `t0` bytes from a3 to word(a0)+a1 (one byte directly when t0 is 1); v0 = a0.

// 0x0887F458 (a0 = object): store the two vtables, run 0x0886AA0C with modes 5, 2, 4 and 3, release
// the member at +0x74 through its first virtual, store the final vtable and tail-call 0x0886A858.

// 0x088B4894 (a0 = object): zero a0+0xF0, initialize a0+4 through 0x088C37C0, call the a0+0x38
// object's +0xC virtual with a0+4, write 0x0002BF20 at the a0+0x28 object's +8 when the a0+0xC float
// is at least 13.89f, and set a0+0x120 to 1.0f when 0x088B19F4(a0+4) returns zero.

// 0x088EB7C4 (a0 = object): store the four vtable results at +0x548/+0x54C/+0x550, return 6 when the
// +0xBC virtual reports, else run the +0x10 virtual and return 0.
void func_088EB7C4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x088EB530u, {self});
    const u32 object1 = word(rt, self + 0x538u);
    put(rt, self + 0x548u, call_keep(rt, ctx, word(rt, word(rt, object1) + 0x94u), {object1}).v0);
    const u32 object2 = word(rt, self + 0x538u);
    put(rt, self + 0x54Cu, call_keep(rt, ctx, word(rt, word(rt, object2) + 0x90u), {object2}).v0);
    const u32 object3 = word(rt, self + 0x538u);
    put(rt, self + 0x550u, call_keep(rt, ctx, word(rt, word(rt, object3) + 0x98u), {object3}).v0);
    const u32 object4 = word(rt, self + 0x538u);
    const u32 report = call_keep(rt, ctx, word(rt, word(rt, object4) + 0xBCu), {object4}).v0;
    if (report != 0u) { ctx.gpr[2] = 6u; return; }
    const u32 table = word(rt, self);
    call_keep(rt, ctx, word(rt, table + 0x10u), {self});
    ctx.gpr[2] = 0u;
}

// 0x088EC328 (a0 = index, a1 = value, a2 = object, a3 = flag): call the +0x74 virtual, store a1 and
// the result in the a0*4-selected slot at +0x598/+0x5A8, then tail-call 0x08B0A338 with the +0xB
// and +0x38 fields of the stored result.

// 0x0891AB20 (a0 = object, a1 = value): advance the a0+0x1F8 object's timer by 0.2 and set its float,
// store a1 at its +8, then run 0x0892632C(a0+0x10) and 0x08B4A774(a0+0x1E4, a0+0x10, {0,0,0,1.0f}).

// 0x08937BFC / 0x08937EBC (a0 = object): build the 0x3C-byte record 0x08938DC8 returns, copy it into
// the frame, store its +0x10 and +0x20 words at a0+8 / a0+4, call 0x08937B0C (or 0x08937DCC) and
// return the +0x10 word when the call reports, -1 otherwise.
void copy_record_pair(Runtime &rt, AllegrexContext &ctx, u32 target) {
    GuestFrame frame(ctx, 0x90u);
    const u32 self = ctx.gpr[4];
    call(rt, ctx, 0x08938DC8u, {frame.at(0x40)});
    for (u32 i = 0; i < 0x3Cu; i += 4) put(rt, frame.at(i), word(rt, frame.at(0x40) + i));
    const u32 field8 = word(rt, frame.at(0x10));
    const u32 field4 = word(rt, frame.at(0x20));
    put(rt, self + 8u, field8);
    put(rt, self + 4u, field4);  // sw in the jal's delay slot: the callee reads a0+4 first
    const u32 result = call(rt, ctx, target, {self});
    ctx.gpr[2] = (result != 0u) ? field8 : 0xFFFFFFFFu;
}

// 0x089C4C20 (a0/a1 inputs, a2 = output): build the 0x3C-byte record 0x08B10204 returns with mode 1,
// copy it into the frame, and when the first word is zero store the +0x10 word at a2 and return the
// +0x20 word (0 otherwise).
void func_089C4C20(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x90u);
    const u32 output = ctx.gpr[6];
    put(rt, output, 0u);
    call(rt, ctx, 0x08B10204u, {frame.at(0x40), ctx.gpr[5], 1u});
    u32 source = frame.at(0x40);
    u32 dest = frame.at(0);
    const u32 source_end = frame.at(0x70);
    for (; source != source_end; source += 0x10u, dest += 0x10u) {
        put(rt, dest, word(rt, source));
        put(rt, dest + 4u, word(rt, source + 4u));
        put(rt, dest + 8u, word(rt, source + 8u));
        put(rt, dest + 0xCu, word(rt, source + 0xCu));
    }
    put(rt, dest + 8u, word(rt, source + 8u));
    put(rt, dest, word(rt, source));
    put(rt, dest + 4u, word(rt, source + 4u));
    u32 result = 0u;
    if (word(rt, frame.at(0)) == 0u) {
        put(rt, output, word(rt, frame.at(0x10)));
        result = word(rt, frame.at(0x20));
    }
    ctx.gpr[2] = result;
}

// 0x08A5E294 / 0x08A8350C (a0 = comparison, a1 = result, a3 = holder): the 64-bit product of
// a0+0x18/+0x1c and the object's vtable method's v0/v1, handed to the builder at sp.
void multiply64(Runtime &rt, AllegrexContext &ctx, u32 message, u32 method_offset, u32 builder) {
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 object = word(rt, ctx.gpr[7]);
    const u32 low = word(rt, self + 0x18u);
    const u32 high = word(rt, self + 0x1Cu);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const GuestResult method = call_virtual(rt, ctx, object, method_offset, {object});
    const s64 a = static_cast<s64>((static_cast<u64>(high) << 32) | low);
    const s64 b = static_cast<s64>((static_cast<u64>(method.v1) << 32) | method.v0);
    const s64 product = a * b;
    call_keep(rt, ctx, builder,
              {frame.at(0), destination, static_cast<u32>(product),
               static_cast<u32>(static_cast<u64>(product) >> 32)});
    release_result(rt, ctx, destination, frame.at(0));
}

// 0x08B26E14 (a0 = node table, a1 = index, a2 = count): store count+1 at node+0xE and return the
// larger of the two recursive child counts (the node's signed halfwords at +8 and +0xA), or count when
// a child is negative.
u32 count_tree(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 base = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 next = count + 1u;
    const u32 node = base + (index << 4);
    const s32 child1 = static_cast<s16>(rt.memory().load16(node + 8u));
    rt.memory().store16(node + 0xEu, static_cast<std::uint16_t>(count));
    u32 result = count;
    if (child1 >= 0) {
        ctx.gpr[4] = base;
        ctx.gpr[5] = static_cast<u32>(child1);
        ctx.gpr[6] = next;
        result = count_tree(rt, ctx);
    }
    const s32 child2 = static_cast<s16>(rt.memory().load16(node + 0xAu));
    u32 other = count;
    if (child2 >= 0) {
        ctx.gpr[4] = base;
        ctx.gpr[5] = static_cast<u32>(child2);
        ctx.gpr[6] = next;
        other = count_tree(rt, ctx);
    }
    if (result < other) result = other;
    return result;
}
void func_08B26E14(Runtime &rt, AllegrexContext &ctx) { ctx.gpr[2] = count_tree(rt, ctx); }

// 0x08AD1F9C (a0 = object, a1/a2/a3 = record): with the a0+0x29C registry check zero, release the
// old +0x1C member, create a new one from a2, store a1 at +4 and the flag byte at +0x23; v0 = 1 on
// success, 0 otherwise.
void func_08AD1F9C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 flag = ctx.gpr[7];
    const u32 member = first + 0x14u;
    u32 result = 1u;
    if (call(rt, ctx, 0x08AD5ED8u, {word(rt, self + 0x29Cu), word(rt, member + 8u)}) == 0u) {
        const u32 old = word(rt, member + 8u);
        if (old != 0u) call(rt, ctx, 0x08AD6398u, {word(rt, self + 0x29Cu), old});
        const u32 created = call(rt, ctx, 0x08AD6D8Cu, {word(rt, self + 0x29Cu), second});
        put(rt, member + 8u, created);
        if (created == 0u) {
            result = 0u;
        } else {
            rt.memory().store8(created + 0x23u, static_cast<std::uint8_t>(flag));
            put(rt, created + 4u, first);
            result = 1u;
        }
    }
    ctx.gpr[2] = result;
}

// 0x08B1D860 (a0 = id, a1 = fallback): look the id up through 0x08B1DEB4 and decode the record's
// +0 word: kinds 0-3 return the +8 word, kind 4 the truncated +8 float, kind 5 0x08B71DC8's result;
// a null record, a null lookup or a kind of 6 or more returns the fallback.
void func_08B1D860(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    const u32 fallback = ctx.gpr[5];
    const u32 outer = call(rt, ctx, 0x08B1DEB4u, {0x08D68A94u, ctx.gpr[4]});
    if (outer == 0u) { ctx.gpr[2] = fallback; return; }
    const u32 record = word(rt, outer);
    if (record == 0u) { ctx.gpr[2] = fallback; return; }
    const u32 kind = word(rt, record);
    if (kind >= 6u) { ctx.gpr[2] = fallback; return; }
    switch (kind) {
    case 0u:
    case 1u:
    case 2u:
    case 3u: ctx.gpr[2] = word(rt, record + 8u); break;
    case 4u: ctx.gpr[2] = static_cast<u32>(static_cast<s32>(float_at(rt, record + 8u))); break;
    default: ctx.gpr[2] = call(rt, ctx, 0x08B71DC8u, {word(rt, record + 8u), word(rt, record + 0xCu)}); break;
    }
}

// 0x08833954 (a0 = destination, a2 = out slot): unwrap the handle at sp+4, pass the result and sp+8
// to 0x0885EE7C, and when non-null build the result at sp through 0x08A44D9C and store it.

// 0x08AC87E4: tear down the global state: store 0 at 0x08CFFF99, release 0x08CFFF9C, clear it,
// spin on 0x08CFFF98 through 0x08B2EFDC while it is zero, then run the four shutdown calls and
// tail-call the last one.

// 0x08B55F14 (a0 = state, f12 = angle): build the rotation matrix in VFPU M000 (columns 0/1 from vrot.q,
// columns 2/3 from vidt.q), multiply it by the matrix in M300 (loaded from the state's +0xD10-indexed
// table), write the result back through M100 and store it at the table entry; v0 = 0x3C360B61.

} // namespace
} // namespace new_batch_main_12
} // namespace gt::game

#define NEW12(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_12::name, gt::ReturnKind::kind)
NEW12(0x08A83824u, func_08A83824, Word);
NEW12(0x08A838C0u, func_08A838C0, Word);
NEW12(0x08A8B224u, func_08A8B224, Word);
NEW12(0x08A8B99Cu, func_08A8B99C, Word);
NEW12(0x089C5BB8u, func_089C5BB8, Word);
NEW12(0x08B85CB8u, func_08B85CB8, Word);
NEW12(0x08BB8FA8u, func_08BB8FA8, Word);
NEW12(0x0884D8B0u, func_0884D8B0, Word);
NEW12(0x088EC09Cu, func_088EC09C, Word);
NEW12(0x08A32974u, func_08A32974, Word);
NEW12(0x08A6C528u, func_08A6C528, Word);
NEW12(0x08AD1F9Cu, func_08AD1F9C, Word);
NEW12(0x08B05F9Cu, func_08B05F9C, Word);
NEW12(0x08B1D860u, func_08B1D860, Word);
NEW12(0x08B72704u, func_08B72704, Word);
NEW12(0x08C5942Cu, func_08C5942C, Word);
NEW12(0x088EB7C4u, func_088EB7C4, Word);
NEW12(0x089C4C20u, func_089C4C20, Word);
NEW12(0x08B26E14u, func_08B26E14, Word);