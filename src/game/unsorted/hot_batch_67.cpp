// Hot batch 67: medium-size functions that the five pad routes reach (call probe over every function of up to 70 instructions not
// yet decompiled, run on the five routes with --decomp off, as in batches 57-66; the reached ones grouped by instruction shape with the
// registers kept, the constants decoded from each member's own instructions). As in batches 60-66, each function's comment lists the
// branches the compared cases of the fast gate do not take (found with `tools.decomp_gate coverage`); those were compared against the
// asm by hand, reading the branch TARGET (a value set in a delay slot is the result only if the target does not overwrite it).
// Their classes aren't known yet, so they keep their addresses as names. Written from the asm; the generated code was only a reading
// aid (it gives the relocated constants).
//
// Families (each member has the same instruction shape; the constants differ and were read from its own words):
//  - "map find" (65 instructions): the lookup of a map keyed by reference-counted strings (length at key[-12], compared by
//    func_08B36170 = memcmp, then by length): the lower bound of the key, then the not-less check;
//  - "getter, then setter with arguments" (49 instructions): two pointers to member function stored in the object (+20/+24 and
//    +28/+32) - the first called on *a2, its result is the object of the second, which is tail called with a1, a3, t0;
//  - "getter, then setter with the value" (47 instructions): the same shape with the pointers at +20/+24 and +36/+40 (+28/+32 for one
//    member) and the value converted by a thunk of a2 (func_08B8C590 & co.) in between;
//  - "static record" (44 instructions): a function-local static of 52 bytes built on its first call under the C++ guard
//    (func_08B6441C / func_08B64494), whose accessor returns its address;
//  - "static linked object" (30 instructions): the same for a 12-byte object that func_08B18E40 appends to a global list.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_67 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kMemcmp = 0x08B36170u;       // (a, b, count) -> difference of the first differing bytes (gt::libc::memcmp)
constexpr std::uint32_t kGuardAcquire = 0x08B6441Cu; // (guard) -> whether this caller must construct (C++ static locals)
constexpr std::uint32_t kGuardRelease = 0x08B64494u; // (guard)
constexpr std::uint32_t kListAppend = 0x08B18E40u;   // (object): appends the object to a global list and counts it

std::uint32_t half(std::uint32_t tagged) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(tagged) >> 1); }

// A pointer to member function is two words {function or vtable offset, adjustment * 2 | virtual flag}: it is called on base + adjustment; a
// virtual one looks the function up in the vtable of that object.
std::uint32_t member_function(psprecomp::Runtime &rt, std::uint32_t target, std::uint32_t function_field, std::uint32_t tagged) {
    return (tagged & 1u) != 0u ? word(rt, word(rt, target) + function_field) : function_field;
}

// map_find(this, key): the map is {+4 the end node, +8 the root}, a node {+8 left, +12 right, +16 key}; a key is a pointer to the characters of
// a reference-counted string whose length is at key[-12]. Descends from the root to the lower bound (the last node not less than the key, going
// left from it), then returns it unless the key is less than it (then the end node this + 4). Strings compare by their common prefix first
// (memcmp), the lengths decide a tie. v0 = the node.
void map_find(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    const std::uint32_t end_node = self + 4u;
    std::uint32_t node = word(rt, self + 8u);
    std::uint32_t bound = end_node;
    GuestFrame frame(ctx, 0x30u);
    if (node != 0u) {
        const std::uint32_t key_string = word(rt, key);
        do {
            const std::uint32_t node_string = word(rt, node + 16u);
            const std::uint32_t node_length = word(rt, node_string - 12u), key_length = word(rt, key_string - 12u);
            const std::uint32_t common = key_length < node_length ? key_length : node_length;
            std::uint32_t order = node_length - key_length;
            const std::uint32_t compared = call_guest(rt, ctx, kMemcmp, {node_string, key_string, common});
            if (compared != 0u) order = compared;
            if (static_cast<std::int32_t>(order) < 0) {
                node = word(rt, node + 12u);
            } else {
                bound = node;
                node = word(rt, node + 8u);
            }
        } while (node != 0u);
    }
    if (bound != end_node) {
        const std::uint32_t key_string = word(rt, key);
        const std::uint32_t key_length = word(rt, key_string - 12u);
        const std::uint32_t bound_string = word(rt, bound + 16u);
        const std::uint32_t bound_length = word(rt, bound_string - 12u);
        const std::uint32_t common = bound_length < key_length ? bound_length : key_length;
        std::uint32_t order = key_length - bound_length;
        const std::uint32_t compared = call_guest(rt, ctx, kMemcmp, {key_string, bound_string, common});
        if (compared != 0u) order = compared;
        if (static_cast<std::int32_t>(order) < 0) bound = end_node;
    }
    ctx.gpr[2] = bound;
}

// getter_then_setter_args(this, a1, a2, a3, t0): getter = this[20] / this[24] called on *a2 (a0 = the adjusted object, a1 = this[20], a2 = *a2,
// a3 and t0 as they came, t1 = this[24]); then setter = this[28] / this[32] on the getter's result, tail called with (object, a1, a3, t0).
void getter_then_setter_args(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], first = ctx.gpr[5], third = ctx.gpr[7], fourth = ctx.gpr[8];
    const std::uint32_t getter_tagged = word(rt, self + 24u);
    const std::uint32_t base = word(rt, ctx.gpr[6]);
    const std::uint32_t getter_field = word(rt, self + 20u);
    const std::uint32_t getter_object = base + half(getter_tagged);
    const std::uint32_t getter = member_function(rt, getter_object, getter_field, getter_tagged);
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x20u);
        result = call_guest(rt, ctx, getter, {getter_object, getter_field, base, third, fourth, getter_tagged, ctx.gpr[10], ctx.gpr[11]});
    }
    const std::uint32_t setter_tagged = word(rt, self + 32u);
    const std::uint32_t setter_object = result + half(setter_tagged);
    const std::uint32_t setter_field = word(rt, self + 28u);
    const std::uint32_t setter = member_function(rt, setter_object, setter_field, setter_tagged);
    ctx.gpr[2] = result;  // v0 as the original leaves it for the setter (a plain setter keeps it)
    tail_call(ctx, setter, {setter_object, first, third, fourth});
}

// getter_then_setter_value(this, a1, a2): the getter = this[20] / this[24] is called on *a1 (a0 = the adjusted object, a1 = *a1, a2 = this[20],
// a3 = this[24]); the setter (this[SETTER] / this[SETTER + 4], its object and, for a virtual one, its vtable slot looked up now) is chosen
// from the getter's result; the value is convert(a2) (func_08B8C590 and the thunks func_08BE9068 / func_08BF3BF8, each a smart-pointer virtual of a2); then the setter is
// tail called with (object, value). `setter_tagged_offset` is the offset of the setter's tagged word in this member (40; 32 for func_08C150EC),
// its function field is 4 before it.
void getter_then_setter_value(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t setter_tagged_offset,
                              std::uint32_t convert) {
    const std::uint32_t self = ctx.gpr[4], third = ctx.gpr[6];
    const std::uint32_t getter_tagged = word(rt, self + 24u);
    const std::uint32_t base = word(rt, ctx.gpr[5]);
    const std::uint32_t getter_field = word(rt, self + 20u);
    const std::uint32_t getter_object = base + half(getter_tagged);
    const std::uint32_t getter = member_function(rt, getter_object, getter_field, getter_tagged);
    std::uint32_t result, converted, setter_object, setter;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, getter, {getter_object, base, getter_field, getter_tagged});
        const std::uint32_t setter_tagged = word(rt, self + setter_tagged_offset);
        setter_object = result + half(setter_tagged);
        setter = member_function(rt, setter_object, word(rt, self + setter_tagged_offset - 4u), setter_tagged);
        converted = call_guest(rt, ctx, convert, {third});
    }
    ctx.gpr[2] = converted;  // v0 as the original leaves it for the setter (a plain setter keeps it)
    tail_call(ctx, setter, {setter_object, converted});
}

// static_object_record(): the accessor of a function-local static of 52 bytes. While its guard byte is 0 and func_08B6441C(guard) lets this caller
// build it, the object is made (+0 = its class pointer, +8 = a static pointer, every other field of +4 .. +48 that the builder touches cleared),
// func_08B64494(guard) releases the guard. v0 = the object.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08B6441C answering 0 (another caller is building
// it: the beqz after the call lands on the epilogue, whose delay slot a0 = guard is dead there, and v0 = the object is set at the epilogue itself).
void static_object_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                          std::uint32_t pointer, std::uint32_t class_pointer) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) == 0 && call_guest(rt, ctx, kGuardAcquire, {guard}) != 0u) {
            put(rt, object + 8u, pointer);
            put(rt, object + 4u, 0u);
            put(rt, object + 12u, 0u);
            put(rt, object + 20u, 0u);
            put(rt, object + 36u, 0u);
            put(rt, object + 16u, 0u);
            put(rt, object + 32u, 0u);
            put(rt, object + 28u, 0u);
            put(rt, object + 44u, 0u);
            put(rt, object, class_pointer);
            put(rt, object + 48u, 0u);
            (void)call_guest(rt, ctx, kGuardRelease, {guard});
        }
    }
    ctx.gpr[2] = object;
}

// static_object_linked(): the accessor of a function-local static of 12 bytes: under the guard, +4 = a static pointer, +8 = 0, +0 = 0, then
// func_08B18E40(object) appends it to the global list and func_08B64494(guard) releases the guard. v0 = the object.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08B6441C answering 0 (the beqz skips the stores and the
// two calls; its delay slot loads v0 = the object, which is the result on that path as on the others).
void static_object_linked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                          std::uint32_t pointer) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) == 0 && call_guest(rt, ctx, kGuardAcquire, {guard}) != 0u) {
            put(rt, object + 4u, pointer);
            put(rt, object + 8u, 0u);
            put(rt, object, 0u);
            (void)call_guest(rt, ctx, kListAppend, {object});
            (void)call_guest(rt, ctx, kGuardRelease, {guard});
        }
    }
    ctx.gpr[2] = object;
}

} // namespace

// func_XXXXXXXX (map find): see map_find.
void func_08BEE8B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }
void func_08BF6204(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }
void func_08BF63A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }
void func_08C00C10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }
void func_08C0DA48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }
void func_08C1BB98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }
void func_08C315C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }

// func_XXXXXXXX (getter, then setter with arguments): see getter_then_setter_args.
void func_08C1F0C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }
void func_08C1F2F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }
void func_08C24248(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }
void func_08C24620(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }
void func_08C2A0C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }
void func_08C2A5E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }
void func_08C2D57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }
void func_08C2D98C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }

// func_XXXXXXXX (getter, then setter with the value): see getter_then_setter_value; the argument is the offset of the setter's tagged word.
void func_08BEB334(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08BF367C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BE9068u); }
void func_08BF6910(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BE9068u); }
void func_08BFBEE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08BFC86C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BF3BF8u); }
void func_08C041DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BE9068u); }
void func_08C0BC20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C0F1D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BF3BF8u); }
void func_08C150EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 32u, 0x08B8C590u); }
void func_08C1530C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }

// func_XXXXXXXX (static record): see static_object_record (guard, object, the pointer stored at +8, the class pointer).
void func_08B9CEBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D757A0u, 0x08D758F4u, 0x08C89848u, 0x08CC0E88u); }
void func_08B9DDB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D757A8u, 0x08D75A78u, 0x08C89848u, 0x08CC0290u); }
void func_08B9E8D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D757B0u, 0x08D75B58u, 0x08C89848u, 0x08CBFFE0u); }
void func_08BC415C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D76168u, 0x08D764D0u, 0x08C8A47Cu, 0x08CC5118u); }
void func_08BC42D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D76170u, 0x08D76504u, 0x08C8A47Cu, 0x08CC50D0u); }
void func_08BC444C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D76178u, 0x08D76538u, 0x08C8A47Cu, 0x08CC5088u); }
void func_08BC45C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D76180u, 0x08D7656Cu, 0x08C8A47Cu, 0x08CC5040u); }
void func_08BC473C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D76188u, 0x08D765A0u, 0x08C8A47Cu, 0x08CC4FF8u); }
void func_08BC48B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D76190u, 0x08D765D4u, 0x08C8A47Cu, 0x08CC4FB0u); }
void func_08BC4A2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D76198u, 0x08D76608u, 0x08C8A47Cu, 0x08CC4F68u); }
void func_08BC4BA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D761A0u, 0x08D7663Cu, 0x08C8A47Cu, 0x08CC4F20u); }
void func_08BC4D2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D761A8u, 0x08D76670u, 0x08C8A47Cu, 0x08CC4ED8u); }
void func_08BC4EB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D761C8u, 0x08D766A4u, 0x08C8A47Cu, 0x08CC4EA8u); }
void func_08C4D18C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D77370u, 0x08D773A4u, 0x08C93168u, 0x08CD9298u); }
void func_08B9BCB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D75440u, 0x08D75884u, 0x08C89848u, 0x08CBFF28u); }

// func_XXXXXXXX (static linked object): see static_object_linked (guard, object, the pointer stored at +4).
void func_08B9BDE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D754C0u, 0x08D758B8u, 0x08C89848u); }
void func_08B9BE58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D754C8u, 0x08D758C4u, 0x08C89848u); }
void func_08B9BED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D75558u, 0x08D758D0u, 0x08C89848u); }
void func_08B9BF48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D75630u, 0x08D758DCu, 0x08C89848u); }
void func_08BB9194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D76030u, 0x08D76068u, 0x08C8A21Cu); }
void func_08BBD42C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D76268u, 0x08D76480u, 0x08C8A47Cu); }
void func_08BBD4A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D76270u, 0x08D7648Cu, 0x08C8A47Cu); }
void func_08C4CC3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D77368u, 0x08D7738Cu, 0x08C93168u); }
void func_08C4CCB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D77360u, 0x08D77398u, 0x08C93168u); }

// Batch 160: more members of the 0x08BEB334 family (tools.family_rows), verified by synthetic cases.
void func_08B8CB18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08BEB5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BE9068u); }
void func_08BF2A78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08BF3D60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BF3BF8u); }
void func_08BF41D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08BF6664(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08BFDA8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08BFF398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C00364(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C0567C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BE9068u); }
void func_08C05BCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C07970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C0A460(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C1E858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C37144(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BF3BF8u); }
} // namespace hot_batch_67
} // namespace gt::game

#define HOT67(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_67::name, gt::ReturnKind::kind)
HOT67(0x08BEE8B8u, func_08BEE8B8, Word);
HOT67(0x08BF6204u, func_08BF6204, Word);
HOT67(0x08BF63A8u, func_08BF63A8, Word);
HOT67(0x08C00C10u, func_08C00C10, Word);
HOT67(0x08C0DA48u, func_08C0DA48, Word);
HOT67(0x08C1BB98u, func_08C1BB98, Word);
HOT67(0x08C315C4u, func_08C315C4, Word);
HOT67(0x08C1F0C4u, func_08C1F0C4, Word);
HOT67(0x08C1F2F4u, func_08C1F2F4, Word);
HOT67(0x08C24248u, func_08C24248, Word);
HOT67(0x08C24620u, func_08C24620, Word);
HOT67(0x08C2A0C0u, func_08C2A0C0, Word);
HOT67(0x08C2A5E4u, func_08C2A5E4, Word);
HOT67(0x08C2D57Cu, func_08C2D57C, Word);
HOT67(0x08C2D98Cu, func_08C2D98C, Word);
HOT67(0x08BEB334u, func_08BEB334, Word);
HOT67(0x08BF367Cu, func_08BF367C, Word);
HOT67(0x08BF6910u, func_08BF6910, Word);
HOT67(0x08BFBEE4u, func_08BFBEE4, Word);
HOT67(0x08BFC86Cu, func_08BFC86C, Word);
HOT67(0x08C041DCu, func_08C041DC, Word);
// Written but not registered: they write memory and no compared case changed any (compared 10 / 15, 0 cases that changed memory).
// HOT67(0x08C0BC20u, func_08C0BC20, Word);
HOT67(0x08C0F1D8u, func_08C0F1D8, Word);
HOT67(0x08C150ECu, func_08C150EC, Word);
HOT67(0x08C1530Cu, func_08C1530C, Word);
HOT67(0x08B9CEBCu, func_08B9CEBC, Word);
HOT67(0x08B9DDB0u, func_08B9DDB0, Word);
HOT67(0x08B9E8D4u, func_08B9E8D4, Word);
HOT67(0x08BC415Cu, func_08BC415C, Word);
HOT67(0x08BC42D4u, func_08BC42D4, Word);
HOT67(0x08BC444Cu, func_08BC444C, Word);
HOT67(0x08BC45C4u, func_08BC45C4, Word);
HOT67(0x08BC473Cu, func_08BC473C, Word);
// HOT67(0x08BC48B4u, func_08BC48B4, Word);
HOT67(0x08BC4A2Cu, func_08BC4A2C, Word);
HOT67(0x08BC4BA4u, func_08BC4BA4, Word);
HOT67(0x08BC4D2Cu, func_08BC4D2C, Word);
HOT67(0x08BC4EB4u, func_08BC4EB4, Word);
HOT67(0x08C4D18Cu, func_08C4D18C, Word);
HOT67(0x08B9BCB8u, func_08B9BCB8, Word);
HOT67(0x08B9BDE0u, func_08B9BDE0, Word);
HOT67(0x08B9BE58u, func_08B9BE58, Word);
HOT67(0x08B9BED0u, func_08B9BED0, Word);
HOT67(0x08B9BF48u, func_08B9BF48, Word);
HOT67(0x08BB9194u, func_08BB9194, Word);
HOT67(0x08BBD42Cu, func_08BBD42C, Word);
HOT67(0x08BBD4A4u, func_08BBD4A4, Word);
HOT67(0x08C4CC3Cu, func_08C4CC3C, Word);
HOT67(0x08C4CCB4u, func_08C4CCB4, Word);
// HOT67(0x08B8CB18u, func_08B8CB18, Word);  // no comparable synthetic case yet
// HOT67(0x08BEB5E0u, func_08BEB5E0, Word);  // no comparable synthetic case yet
// HOT67(0x08BF2A78u, func_08BF2A78, Word);  // no comparable synthetic case yet
// HOT67(0x08BF3D60u, func_08BF3D60, Word);  // no comparable synthetic case yet
// HOT67(0x08BF41D0u, func_08BF41D0, Word);  // no comparable synthetic case yet
// HOT67(0x08BF6664u, func_08BF6664, Word);  // no comparable synthetic case yet
// HOT67(0x08BFDA8Cu, func_08BFDA8C, Word);  // no comparable synthetic case yet
// HOT67(0x08BFF398u, func_08BFF398, Word);  // no comparable synthetic case yet
HOT67(0x08C00364u, func_08C00364, Word);
// HOT67(0x08C0567Cu, func_08C0567C, Word);  // no comparable synthetic case yet
// HOT67(0x08C05BCCu, func_08C05BCC, Word);  // no comparable synthetic case yet
// HOT67(0x08C07970u, func_08C07970, Word);  // no comparable synthetic case yet
// HOT67(0x08C0A460u, func_08C0A460, Word);  // no comparable synthetic case yet
// HOT67(0x08C1E858u, func_08C1E858, Word);  // no comparable synthetic case yet
// HOT67(0x08C37144u, func_08C37144, Word);  // no comparable synthetic case yet
