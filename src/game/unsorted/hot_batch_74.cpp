// Hot batch 74: more members of the shape families of batches 59-68 (call probe over every function of up to 70 instructions not yet
// decompiled, run on the five routes with --decomp off, as in batches 57-73; the reached ones grouped by instruction shape with
// `tools.family_members`). Each family's implementation is the one of the batch that first read it by hand against the asm. Every member here
// was compared with its family's template word by word (`tools.family_members`: the words that differ are only the relocated constants, the
// one displacement named below and the jal / j target, never a register or a stack offset; `--check` re-reads every row of this file against
// the member's own words), and the generated code of each member was diffed against its template's with the numbers masked (the only
// differences are how the generator spells a call that leaves its code unit). The constants and call targets were read from each member's own
// relocated words (the generated code only gave the relocation; it was not copied) and the jal / j targets checked against the raw word
// (target * 4 + the load base 0x08804000). As in batches 60-73, each function's comment lists the branches the compared cases of the fast gate
// do not take; those were compared against the asm by hand, reading the branch TARGET (a value set in a delay slot is the result only if the
// target does not overwrite it). Their classes aren't known yet, so they keep their addresses as names.
//
// Families (the implementation is the one of the earlier batch, with this member's constants):
//  - "empty container accessor" (32 instructions, batches 63 / 68): a function-local static container built under the C++ guard. These six
//    have the words of batch 68's members; func_08BF0A98 was read in full by hand against the asm. The object sits at the guard + 12 (+ 8 for
//    func_08BEE9BC, + 24 for func_08C10DE0), so the guard and the object are separate columns;
//  - "target setting" (68 instructions, func_088492F8 of hot_leaves.cpp): with n > 0 the held object's virtual +0x3C is stored into the
//    property of a target, else the property is read into the result holder;
//  - "getter, then setter with the value" (47 instructions, batch 67);
//  - "static record" (44 instructions, batch 67);
//  - "range erase" (32 instructions, batch 59), "convert, then tail" (12 instructions, batch 59), "call with a constant" (11 instructions,
//    batch 66), "holder conversion" (22 instructions, func_08818064 of hot_leaves.cpp), "base constructor, then two stores" (16 instructions,
//    func_0891FD28 of hot_leaves.cpp; the member's second store is at +16, not +8) and the "clamp step" (batches 63 / 65 / 68).
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep
// the caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace hot_batch_74 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kGuardAcquire = 0x08B6441Cu; // (guard) -> whether this caller must construct (C++ static locals)
constexpr std::uint32_t kGuardRelease = 0x08B64494u; // (guard)
constexpr std::uint32_t kAtExit = 0x08B77B1Cu;       // (function): registers a destructor for exit
constexpr std::uint32_t kReleaseLocal = 0x08A5B9ECu; // destroys a temporary holder
constexpr std::uint32_t kAssignSmart = 0x08A5BAD8u;  // (dst, value): smart-pointer assignment
constexpr std::uint32_t kReportNull = 0x08A5BB7Cu;   // (message): the original's null-object assert
constexpr std::uint32_t kHolderFill = 0x08A85B1Cu;   // (holder, value): the holder takes the value

std::uint32_t half(std::uint32_t tagged) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(tagged) >> 1); }

// A pointer to member function is two words {function or vtable offset, adjustment * 2 | virtual flag}: it is called on base + adjustment; a
// virtual one looks the function up in the vtable of that object.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the virtual flag set, for the getter or the setter
// (e.g. L_08C03EE4 / L_08C03F10 of func_08C03E9C: v0 = the word at the object's vtable + the function field, called on the adjusted object).
std::uint32_t member_function(psprecomp::Runtime &rt, std::uint32_t target, std::uint32_t function_field, std::uint32_t tagged) {
    return (tagged & 1u) != 0u ? word(rt, word(rt, target) + function_field) : function_field;
}

// The method at `offset` of an object's vtable.
std::uint32_t method_of(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t offset) { return word(rt, word(rt, object) + offset); }

// func_08A5BB7C(message) when `object` is null (the original's null-object assert); the code goes on regardless.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the null object (the bnez falls through to the call, whose
// delay slot loads a0 = message, then the code continues with the null pointer as on the other path).
void null_check(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t message) {
    if (object == 0u) (void)call_guest(rt, ctx, kReportNull, {message});
}

// The accessor of a function-local static container: while its guard byte is 0 and func_08B6441C(guard) lets this caller build it, the container is
// made empty (+16 = +12 = its header at +4; +4, +8, +20 cleared), func_08B64494(guard) releases the guard and func_08B77B1C(destructor) registers
// the destructor for exit. v0 = the container's address.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08B6441C answering 0 (another caller is
// building it: the beqz lands on the epilogue, and v0 = the object is set there; the delay slot a0 = guard is dead).
void get_empty_container(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object,
                         std::uint32_t destructor) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) == 0 && call_guest(rt, ctx, kGuardAcquire, {guard}) != 0u) {
            put(rt, object + 16u, object + 4u);
            put(rt, object + 20u, 0u);
            put(rt, object + 4u, 0u);
            put(rt, object + 8u, 0u);
            put(rt, object + 12u, object + 4u);
            (void)call_guest(rt, ctx, kGuardRelease, {guard});
            (void)call_guest(rt, ctx, kAtExit, {destructor});
        }
    }
    ctx.gpr[2] = object;
}

// The script setting of a target: the target `make` gives (`convert` of it; `target_message` when the made object is null). With n > 0
// `set`(target, the held object's virtual +0x3C, as a bool when `as_bool`) (`held_message` when the held object is null), v0 = its result; else
// out = func_08A85B1C of `get`(target), assigned (func_08A5BAD8) when out is not the local holder, the holder released, v0 = the release's result.
// The made holder sits at sp + `slot` (n > 0) or sp + 4; the result holder at sp + 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): the null checks of the made and the held object
// (null_check); out being the local holder (the beq skips func_08A5BAD8; its delay slot a0 = out is dead, the holder is released next).
struct TargetSetting {
    std::uint32_t make, target_message, convert, held_message, set, get;
    bool as_bool;
    std::uint32_t slot;
};

void target_setting(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const TargetSetting &s) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t n = ctx.gpr[6], holder = ctx.gpr[7];
    const bool assign = static_cast<std::int32_t>(n) > 0;
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t made = frame.at(assign ? s.slot : 4u);
    (void)call_guest(rt, ctx, s.make, {made, ctx.gpr[5], n, holder});
    const std::uint32_t object = word(rt, made);
    null_check(rt, ctx, object, s.target_message);
    const std::uint32_t target = call_guest(rt, ctx, s.convert, {object});
    (void)call_guest(rt, ctx, kReleaseLocal, {made});
    if (assign) {
        const std::uint32_t held = word(rt, holder);
        null_check(rt, ctx, held, s.held_message);
        const std::uint32_t value = call_guest(rt, ctx, method_of(rt, held, 60u), {held});
        ctx.gpr[2] = call_guest(rt, ctx, s.set, {target, s.as_bool ? (value != 0u ? 1u : 0u) : value});
        return;
    }
    (void)call_guest(rt, ctx, kHolderFill, {frame.at(0u), call_guest(rt, ctx, s.get, {target})});
    if (out != frame.at(0u)) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, frame.at(0u))});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// target_setting_row: the value is stored as it is (func_088492F8 and its siblings, the made holder at sp + 8 when n > 0).
void target_setting_row(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t make, std::uint32_t target_message,
                        std::uint32_t convert, std::uint32_t held_message, std::uint32_t set, std::uint32_t get) {
    target_setting(rt, ctx, {make, target_message, convert, held_message, set, get, false, 8u});
}

// target_setting_bool_row: the value is stored as a bool (func_0884DF90 and its siblings, the made holder at sp + 4).
void target_setting_bool_row(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t make, std::uint32_t target_message,
                             std::uint32_t convert, std::uint32_t held_message, std::uint32_t set, std::uint32_t get) {
    target_setting(rt, ctx, {make, target_message, convert, held_message, set, get, true, 4u});
}

// getter_then_setter_value(this, a1, a2): the getter = this[20] / this[24] is called on *a1 (a0 = the adjusted object, a1 = *a1, a2 = this[20],
// a3 = this[24]); the setter (this[SETTER] / this[SETTER + 4], its object and, for a virtual one, its vtable slot looked up now) is chosen
// from the getter's result; the value is convert(a2) (func_08B8C590 and the thunks func_08BE9068 / func_08BF3BF8, each a smart-pointer virtual of a2); then the setter is
// tail called with (object, value). Branches the routes do not take (checked against the asm by hand): the virtual flag of the getter or of the setter
// (member_function). `setter_tagged_offset` is the offset of the setter's tagged word in this member (40 in all of these), its function
// field is 4 before it.
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

// Range erase of a vector of words: (vec, first, last) -> first. The tail [last, end) is moved down (memmove, func_08B79BAC), a per-type routine
// `after_move` gets (the new end of the moved words, the old end, a stack local), and the vector's end drops by the erased bytes.
void erase_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t after_move) {
    const std::uint32_t vec = ctx.gpr[4], first = ctx.gpr[5], last = ctx.gpr[6];
    const std::uint32_t bytes = (last - first) & ~3u;
    const std::uint32_t tail = word(rt, vec + 4u) - last;
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08B79BACu, {first, last, tail});
    (void)call_guest(rt, ctx, after_move, {tail + first, word(rt, vec + 4u), frame.at(0u)});
    put(rt, vec + 4u, word(rt, vec + 4u) - bytes);
    ctx.gpr[2] = first;
}

// convert_tail(_, a, c): tail(a, convert(c)) (tail call; the conversion's result stays in v0).
void convert_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t convert, std::uint32_t tail) {
    const std::uint32_t a = ctx.gpr[5];
    std::uint32_t converted;
    {
        GuestFrame frame(ctx, 0x10u);
        converted = call_guest(rt, ctx, convert, {ctx.gpr[6]});
    }
    ctx.gpr[2] = converted;
    tail_call(ctx, tail, {a, converted});
}

// call_with_constant(this, a1): func_088BA424(this, a1, constant); v0 = this.
void call_with_constant(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t constant) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t self = ctx.gpr[4];
    (void)call_guest(rt, ctx, 0x088BA424u, {self, ctx.gpr[5], constant});
    ctx.gpr[2] = self;
}

// holder_conversion() -> final(convert(the object a holder made by `make` gives)); the holder (sp + 0) is released before the last call (`message` when the object is null).
void holder_conversion(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t make, std::uint32_t message, std::uint32_t convert,
                       std::uint32_t final_call) {
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, make, {frame.at(0u)});
    const std::uint32_t object = word(rt, frame.at(0u));
    null_check(rt, ctx, object, message);
    const std::uint32_t value = call_guest(rt, ctx, convert, {object});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
    ctx.gpr[2] = call_guest(rt, ctx, final_call, {value});
}

// base_then_stores(this, a, b, c): base(this, a, b, c), then this[offset] = b and this[0] = vtable.
void base_then_stores(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t base, std::uint32_t vtable, std::uint32_t offset) {
    const std::uint32_t self = ctx.gpr[4], b = ctx.gpr[6];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, base, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    put(rt, self + offset, b);
    put(rt, self, vtable);
}

// map_find(this, key): the map is {+4 the end node, +8 the root}, a node {+8 left, +12 right, +16 key}; a key is a pointer to the characters of
// a reference-counted string whose length is at key[-12]. Descends from the root to the lower bound (the last node not less than the key, going
// left from it), then returns it unless the key is less than it (then the end node this + 4). Strings compare by their common prefix first
// (memcmp), the lengths decide a tie. v0 = the node.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets, in func_08C00DB4): in the descent a key shorter than the node's
// string (the slt picks the key's length at sp + 0 as the count), in the final check a bound shorter than the key (the count is the bound's length), a nonzero
// memcmp there (it replaces the length difference) and a key less than the bound (the end node stays the result; otherwise the bound replaces it).
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
            const std::uint32_t compared = call_guest(rt, ctx, 0x08B36170u, {node_string, key_string, common});
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
        const std::uint32_t compared = call_guest(rt, ctx, 0x08B36170u, {key_string, bound_string, common});
        if (compared != 0u) order = compared;
        if (static_cast<std::int32_t>(order) < 0) bound = end_node;
    }
    ctx.gpr[2] = bound;
}

// getter_then_setter_args(this, a1, a2, a3, t0) (branches the routes do not take, checked against the asm by hand: the virtual flag of the getter or of the setter,
// see member_function): getter = this[20] / this[24] called on *a2 (a0 = the adjusted object, a1 = this[20], a2 = *a2,
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

// static_object_linked(): the accessor of a function-local static of 12 bytes: under the guard, +4 = a static pointer, +8 = 0, +0 = 0, then
// func_08B18E40(object) appends it to the global list and func_08B64494(guard) releases the guard. v0 = the object.
// Branches the routes do not take (checked against the asm by hand, reading the branch targets): func_08B6441C answering 0 (the beqz skips the stores and the
// two calls; its delay slot loads v0 = the object, which is the result on that path as on the others).
void static_object_linked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t guard, std::uint32_t object, std::uint32_t pointer) {
    {
        GuestFrame frame(ctx, 0x10u);
        if (static_cast<std::int8_t>(rt.memory().load8(guard)) == 0 && call_guest(rt, ctx, kGuardAcquire, {guard}) != 0u) {
            put(rt, object + 4u, pointer);
            put(rt, object + 8u, 0u);
            put(rt, object, 0u);
            (void)call_guest(rt, ctx, 0x08B18E40u, {object});
            (void)call_guest(rt, ctx, kGuardRelease, {guard});
        }
    }
    ctx.gpr[2] = object;
}

// destroy_range: the virtual call vtable[0](element) on each `stride`-byte element of [first, last); v0 is the last call's result.
void destroy_range(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t stride) {
    std::uint32_t element = ctx.gpr[4];
    const std::uint32_t last = ctx.gpr[5];
    if (element == last) return;
    GuestFrame frame(ctx, 0x10u);
    do {
        const std::uint32_t current = element;
        const std::uint32_t function = word(rt, word(rt, current));
        element += stride;
        ctx.gpr[2] = call_guest(rt, ctx, function, {current});
    } while (last != element);
}

// clamp_step: the two-bound step shared with batches 63, 65 and 68. `limits` holds the bounds (+8 lower, +12 upper) and the words +0 / +4 added to
// the value; the result replaces *p, and is v0.
void clamp_step(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t limits = ctx.gpr[4], p = ctx.gpr[5];
    const std::uint32_t x = word(rt, p);
    std::uint32_t result = 0u;
    if (x != 0u) {
        const std::uint32_t upper = word(rt, limits + 12u);
        std::uint32_t lower = 0u;
        bool bounded = true;
        if (upper != 0u) {
            lower = word(rt, limits + 8u);
        } else if (word(rt, limits + 4u) != 0u) {
            lower = word(rt, limits + 8u);
        } else {
            bounded = false;
            result = word(rt, limits) + x;
        }
        if (bounded) {
            std::uint32_t base;
            if (lower < upper) base = x < upper ? word(rt, limits) : word(rt, limits + 4u);
            else base = x < lower ? word(rt, limits + 4u) : word(rt, limits);
            result = base + x;
        }
    }
    put(rt, p, result);
    ctx.gpr[2] = result;
}

} // namespace

// func_XXXXXXXX (empty container accessor): see get_empty_container.
void func_08BEE9BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76A98u, 0x08D76AA0u, 0x0897D65Cu); }
void func_08BF0A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76B80u, 0x08D76B8Cu, 0x089959C4u); }
void func_08BF37C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76BF8u, 0x08D76C04u, 0x0899B170u); }
void func_08BFE5F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76D08u, 0x08D76D14u, 0x089B9C6Cu); }
void func_08C0F084(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D76FD0u, 0x08D76FDCu, 0x089FFF5Cu); }
void func_08C10DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { get_empty_container(rt, ctx, 0x08D77020u, 0x08D77038u, 0x08A05BD4u); }

// func_XXXXXXXX (target setting): see target_setting_row.
void func_0882EB50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x0885E734u, 0x0885E728u); }
void func_08848EB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5A24u, 0x088D62FCu); }
void func_08848FC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5A14u, 0x088D6358u); }
void func_088490D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5A04u, 0x088D63D4u); }
void func_088491E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D59F4u, 0x088D6204u); }
void func_08849518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5958u, 0x088D5E70u); }
void func_0884996C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5A70u, 0x088D5DA8u); }
void func_0884A8FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D4CC4u, 0x088D4CB8u); }

// func_XXXXXXXX (getter, then setter with the value): see getter_then_setter_value.
void func_08BF23A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BE9068u); }
void func_08C03E9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C0A70C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BE9068u); }
void func_08C1D8C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08B8C590u); }
void func_08C2CF48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_value(rt, ctx, 40u, 0x08BF3BF8u); }

// func_XXXXXXXX (static record): see static_object_record.
void func_08B9E208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D754D0u, 0x08D75AACu, 0x08C89848u, 0x08CC01D0u); }
void func_08BBD034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D761C0u, 0x08D7644Cu, 0x08C8A47Cu, 0x08CC43B8u); }
void func_08BC3FE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_record(rt, ctx, 0x08D76160u, 0x08D7649Cu, 0x08C8A47Cu, 0x08CC5160u); }

// func_XXXXXXXX (range erase): see erase_range.
void func_08BBB908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_range(rt, ctx, 0x08BBB8F8u); }
void func_08BEF1A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_range(rt, ctx, 0x08BEF140u); }
void func_08C49DC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { erase_range(rt, ctx, 0x08C46FD4u); }

// func_XXXXXXXX (convert, then tail): see convert_tail.
void func_08C31A28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_tail(rt, ctx, 0x08BF3BF8u, 0x08C31A00u); }
void func_08C31BB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_tail(rt, ctx, 0x08C31534u, 0x08C31B84u); }
void func_08C31CB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_tail(rt, ctx, 0x08C3157Cu, 0x08C31C8Cu); }

// func_XXXXXXXX (call with a constant): see call_with_constant.
void func_08BB0E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 8u); }
void func_08BB0E78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 1u); }
void func_08BB15E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 4u); }
void func_08BB163C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_constant(rt, ctx, 1u); }

// func_XXXXXXXX (holder conversion): see holder_conversion.
void func_08829E58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_conversion(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885F46Cu); }
void func_0882A068(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_conversion(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885E940u); }
void func_0882A1C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { holder_conversion(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x0885E848u); }

// func_XXXXXXXX (base constructor, then two stores): see base_then_stores.
void func_08B84C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { base_then_stores(rt, ctx, 0x08B84BC4u, 0x08CB6AC8u, 0x00000010u); }
void func_08B84C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { base_then_stores(rt, ctx, 0x08B84BF4u, 0x08CB6AF8u, 0x00000010u); }
void func_08B84E84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { base_then_stores(rt, ctx, 0x08B84E14u, 0x08CB6B28u, 0x00000010u); }
void func_08B84ED4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { base_then_stores(rt, ctx, 0x08B84E44u, 0x08CB6B58u, 0x00000010u); }
void func_08B88CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { base_then_stores(rt, ctx, 0x08B88C94u, 0x08CBCD48u, 0x00000010u); }

// func_XXXXXXXX (target setting, bool): see target_setting_bool_row.
void func_088343FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_bool_row(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x0885EF78u, 0x0885EF5Cu); }
void func_0884E180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_bool_row(rt, ctx, 0x0884D660u, 0x08C9D198u, 0x0884D3D8u, 0x08C9D158u, 0x08896FECu, 0x08896FDCu); }

// func_XXXXXXXX (map find): see map_find.
void func_08C00DB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }
void func_08C1B954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { map_find(rt, ctx); }

// func_XXXXXXXX (getter, then setter with arguments): see getter_then_setter_args.
void func_08C29904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }
void func_08C2CBC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { getter_then_setter_args(rt, ctx); }

// func_XXXXXXXX (static linked object): see static_object_linked.
void func_08B9ADA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D75628u, 0x08D757D0u, 0x08C89848u); }
void func_08B9BFC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { static_object_linked(rt, ctx, 0x08D75638u, 0x08D758E8u, 0x08C89848u); }

// func_XXXXXXXX (destroy range): see destroy_range.
void func_08BE5B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 64u); }
void func_08BE5C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { destroy_range(rt, ctx, 96u); }

// func_XXXXXXXX (clamp step): see clamp_step.
void func_08C401C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C40FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C413DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C41934(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }
void func_08C41A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { clamp_step(rt, ctx); }

// Batch 160: more members of the 0x0882EB50 family (tools.family_rows), verified by synthetic cases.
void func_08849408(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5AA8u, 0x088D5D88u); }
void func_08849628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5948u, 0x088D5938u); }
void func_08849738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5A8Cu, 0x088D5DF8u); }
void func_08849A7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5968u, 0x088D5D44u); }
void func_08849E5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D4C90u, 0x088D4C84u); }
void func_0884AA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D4DFCu, 0x088D4DF0u); }
void func_0884AB1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D5A34u, 0x088D5910u); }
void func_0884AC2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D4FB4u, 0x088D4FA8u); }
void func_08850574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x088547C4u, 0x08C9D3BCu, 0x08854678u, 0x08C9D394u, 0x088A5D30u, 0x088A5D24u); }
void func_088565D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { target_setting_row(rt, ctx, 0x088547C4u, 0x08C9E188u, 0x08854678u, 0x08C9E14Cu, 0x088A5974u, 0x088A746Cu); }
} // namespace hot_batch_74
} // namespace gt::game

#define HOT74(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_74::name, gt::ReturnKind::kind)
HOT74(0x08BEE9BCu, func_08BEE9BC, Word);
HOT74(0x08BF0A98u, func_08BF0A98, Word);
HOT74(0x08BF37C0u, func_08BF37C0, Word);
HOT74(0x08BFE5F4u, func_08BFE5F4, Word);
HOT74(0x08C0F084u, func_08C0F084, Word);
HOT74(0x08C10DE0u, func_08C10DE0, Word);
HOT74(0x0882EB50u, func_0882EB50, Void);
HOT74(0x08848EB8u, func_08848EB8, Void);
HOT74(0x08848FC8u, func_08848FC8, Void);
HOT74(0x088490D8u, func_088490D8, Void);
HOT74(0x088491E8u, func_088491E8, Void);
HOT74(0x08849518u, func_08849518, Void);
HOT74(0x0884996Cu, func_0884996C, Void);
HOT74(0x0884A8FCu, func_0884A8FC, Void);
HOT74(0x08BF23A8u, func_08BF23A8, Word);
HOT74(0x08C03E9Cu, func_08C03E9C, Word);
HOT74(0x08C0A70Cu, func_08C0A70C, Word);
HOT74(0x08C1D8C4u, func_08C1D8C4, Word);
HOT74(0x08C2CF48u, func_08C2CF48, Word);
HOT74(0x08B9E208u, func_08B9E208, Word);
HOT74(0x08BBD034u, func_08BBD034, Word);
HOT74(0x08BC3FE4u, func_08BC3FE4, Word);
HOT74(0x08BEF1A8u, func_08BEF1A8, Word);
HOT74(0x08C31A28u, func_08C31A28, Word);
HOT74(0x08C31BB0u, func_08C31BB0, Word);
HOT74(0x08C31CB8u, func_08C31CB8, Word);
HOT74(0x08BB0E4Cu, func_08BB0E4C, Word);
HOT74(0x08BB0E78u, func_08BB0E78, Word);
HOT74(0x08BB15E4u, func_08BB15E4, Word);
HOT74(0x08BB163Cu, func_08BB163C, Word);
HOT74(0x0882A1C8u, func_0882A1C8, Word);
HOT74(0x08B84C34u, func_08B84C34, Void);
HOT74(0x08B84C84u, func_08B84C84, Void);
HOT74(0x08B84E84u, func_08B84E84, Void);
HOT74(0x08B84ED4u, func_08B84ED4, Void);
HOT74(0x08B88CD4u, func_08B88CD4, Void);
HOT74(0x088343FCu, func_088343FC, Void);
HOT74(0x0884E180u, func_0884E180, Void);
HOT74(0x08C00DB4u, func_08C00DB4, Word);
HOT74(0x08C1B954u, func_08C1B954, Word);
HOT74(0x08C29904u, func_08C29904, Word);
HOT74(0x08C2CBC4u, func_08C2CBC4, Word);
HOT74(0x08B9ADA8u, func_08B9ADA8, Word);
HOT74(0x08B9BFC0u, func_08B9BFC0, Word);
HOT74(0x08BE5B74u, func_08BE5B74, Word);
HOT74(0x08BE5C58u, func_08BE5C58, Word);
// Written but not registered:
// func_08829E58 and func_0882A068 (holder conversion): their only call (on the options route) is "not comparable" (it reaches a Sony import the harness can neither replay nor record) and no route compares them (compared 0).
// HOT74(0x08829E58u, func_08829E58, Word);
// HOT74(0x0882A068u, func_0882A068, Word);
// func_08BBB908 and func_08C49DC0 (range erase) write memory (the vector end) and none of their compared cases (2 and 1) changed any (an empty range: the store of the same end).
// HOT74(0x08BBB908u, func_08BBB908, Word);
// HOT74(0x08C49DC0u, func_08C49DC0, Word);
// the five clamp-step members write *p and none of their compared cases (49, 17, 17, 2, 1) changed any (every case saw a zero *p, so the store of 0 over 0 changed nothing; as func_08C401C4 of batch 68).
// HOT74(0x08C401C4u, func_08C401C4, Word);
// HOT74(0x08C40FA4u, func_08C40FA4, Word);
// HOT74(0x08C413DCu, func_08C413DC, Word);
// HOT74(0x08C41934u, func_08C41934, Word);
// HOT74(0x08C41A04u, func_08C41A04, Word);
// HOT74(0x08849408u, func_08849408, Void);  // no comparable synthetic case yet
// HOT74(0x08849628u, func_08849628, Void);  // no comparable synthetic case yet
// HOT74(0x08849738u, func_08849738, Void);  // no comparable synthetic case yet
// HOT74(0x08849A7Cu, func_08849A7C, Void);  // no comparable synthetic case yet
// HOT74(0x08849E5Cu, func_08849E5C, Void);  // no comparable synthetic case yet
// HOT74(0x0884AA0Cu, func_0884AA0C, Void);  // no comparable synthetic case yet
// HOT74(0x0884AB1Cu, func_0884AB1C, Void);  // no comparable synthetic case yet
// HOT74(0x0884AC2Cu, func_0884AC2C, Void);  // no comparable synthetic case yet
// HOT74(0x08850574u, func_08850574, Void);  // no comparable synthetic case yet
// HOT74(0x088565D8u, func_088565D8, Void);  // no comparable synthetic case yet
