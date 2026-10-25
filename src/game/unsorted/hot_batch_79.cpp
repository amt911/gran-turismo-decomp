// Hot batch 79: shape families found with `tools.family_members --list --limit 0 --any-template` (the list of batch 77 and 78 was cut at 60
// families; the ones past it are these) over the call probe of batch 78 (every function of up to 70 instructions not yet decompiled, run on the five
// routes with --decomp off; batch 78 excluded with --exclude-ref, and no address that appears anywhere in src/ — also in a commented-out line — or in
// batch 78 is taken). For each family one member was read in full by hand against the asm (rabbitizer disassembly with the relocations applied; the
// generated code was not used), and every other member was compared with it word by word by the tool: the words that differ are only the relocated
// constants, a displacement and the jal / j targets, never a register or a stack offset (no `!!` line). The constants of each member come from its own
// relocated words and `--check` re-reads every row of this file against them. A template that is itself reached is registered too. Their classes
// aren't known yet, so they keep their addresses as names. The singles at the end were read by hand one by one.
//
// Families (the implementation is the one of the helper, with this member's constants); 51 are registered:
//  - "pointer-to-member chain, then a fill" (51 instructions) (3), "... then a call with a holder" (50) (2) and "... with a returned holder" (54) (2): two
//    member-function-pointer calls whose result is moved into a holder;
//  - "release a range of strings" (33) (2; the template func_08BBEBE4 is written but its address is never called by the routes: not registered);
//    "vector parameter with a default w" (49) (3); "object parameter lookup" (48) (3);
//  - "scaled sum of two vectors" (52, three-float vectors, plain FPU) (2); "build two holders" (36) (2); "make a node and call" (30) (2);
//  - "for each node of a list, call a member function" (59) (2); "for each element at a stride, call a member function" (50) (3); "shared callback" (67) (4);
//  - small ones: "query then truncate" (22) (2), "call a slot with a flag" (20) (2), "init flagged" (17) (2), "call through a static pair" (19) (2),
//    "alloc 0x14 then call" (21) (2), "store the result" (13) (2; the template func_08BBD380 is written but never called by the routes: not registered),
//    "shared acquire" (21) (2), "first non-zero" (3) (2);
//  - seven singles read by hand one by one: func_08C22D58 and func_08B879E8 (destructors), func_08BE8B78, func_08B9432C (string copy), func_08966C70, func_08C309B4,
//    func_08B8A004.
//
// Left out (they stay recompiled): the families func_0884A3AC, func_08BA0D34 and func_08BD74BC (62-68 instructions, 5-22 calls on all routes: not read in this batch) and
// the hot singles with VFPU code (func_089A9064, func_08B4ACE0, func_08B5A7F0).
//
// ptmf_function's two arms are shared by every pointer-to-member call below: the coverage run saw both taken (by some caller), so the comments name the virtual form of each caller
// as not taken only from the asm, read by hand.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3): the ones it leaves alone keep the
// caller's value, which call_guest copies from ctx.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_79 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kReleaseLocal = 0x08A5B9ECu; // destroys a temporary holder
constexpr std::uint32_t kAssignSmart = 0x08A5BAD8u;  // (dst, value): smart-pointer assignment
constexpr std::uint32_t kStringEmpty = 0x08D75298u;  // the shared empty representation of a string (its characters follow at +0xC)

inline std::uint32_t sar1(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> 1); }

// The call target of a pointer to member function {pointer, adjust}: the object is `base + (adjust >> 1)`; when adjust bit 0 is set `pointer` is a
// vtable offset (the function is *(*object + pointer)), else it is the function itself.
inline std::uint32_t ptmf_function(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t pointer, std::uint32_t adjust) {
    return (adjust & 1u) != 0u ? word(rt, word(rt, object) + pointer) : pointer;
}

// The tail of the string-holding functions: `chars` is a string's characters pointer, its representation is chars - 0xC. When it is not the shared
// empty one `empty`: func_08B60804(chars - 4, -1) (the shared counter step); when that is not above zero, drop(rep, frame0) (the string's last
// reference). Returns v0 as the code leaves it: skipped_v0 when the representation is the empty one, else the last call's.
std::uint32_t release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t frame0, std::uint32_t chars, std::uint32_t empty,
                             std::uint32_t drop, std::uint32_t skipped_v0) {
    const std::uint32_t rep = chars - 0xCu;
    if (rep == empty) return skipped_v0;
    const std::uint32_t steps = call_guest(rt, ctx, 0x08B60804u, {chars - 4u, 0xFFFFFFFFu});
    if (static_cast<std::int32_t>(steps) > 0) return steps;
    return call_guest(rt, ctx, drop, {rep, frame0});
}

// Pointer-to-member chain, then a fill (51 instructions): this = a0, a1 = the destination holder, a2 = the address of a pointer to an object o. The first
// pointer {this[0x14], this[0x18]} is called on o (a0 = o + adjust >> 1, a1 = the function word, a2 = o, a3 = the adjust word) and returns p; the second
// {this[0x1C], this[0x20]} is called on p (a0 = p + adjust >> 1, a1 = p, a2 = o, a3 = the first adjust) and returns q; fill(&h, q) fills the frame's holder h; h
// is assigned to a1 (func_08A5BAD8) unless a1 is the frame slot itself, and released. v0 = the release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): both virtual forms (adjust bit 0: the `bnez` / `beqz` land on
// the vtable loads, *(*object + word); the first `j` reaches the shared `jalr`, a0 = o + adjust >> 1 set in its delay slot) and the destination being the
// frame slot (the `beq` lands on the release; its delay slot only sets a0).
void ptmf_chain_fill(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t fill) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5], slot = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t adjust = word(rt, self + 0x18u);
    const std::uint32_t object = word(rt, slot);
    const std::uint32_t pointer = word(rt, self + 0x14u);
    const std::uint32_t receiver = object + sar1(adjust);
    const std::uint32_t first = call_guest(rt, ctx, ptmf_function(rt, receiver, pointer, adjust), {receiver, pointer, object, adjust});
    const std::uint32_t adjust2 = word(rt, self + 0x20u);
    const std::uint32_t receiver2 = first + sar1(adjust2);
    const std::uint32_t second = call_guest(rt, ctx, ptmf_function(rt, receiver2, word(rt, self + 0x1Cu), adjust2), {receiver2, first});  // a2/a3: what the first callee left (the asm doesn't set them)
    (void)call_guest(rt, ctx, fill, {frame.at(0u), second});
    if (out != frame.at(0u)) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, frame.at(0u))});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// Pointer-to-member chain, then a call with a holder (50 instructions): this = a0, a1 = the address of a pointer to an object o, a2 = an argument. The first pointer
// {this[0x14], this[0x18]} is called on o (a0 = o + adjust >> 1, a1 = o, a2 = the function word, a3 = the adjust word) and returns p; the second {this[0x24], this[0x28]}
// is resolved on p (the virtual form reads the vtable before the next call), func_08BF7BE0(&h, argument) fills the frame's holder h, then the second function runs
// with a0 = p + adjust >> 1, a1 = &h; h is released. v0 = the release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): both virtual forms (adjust bit 0: the `bnez` / `beqz` land on the
// vtable loads; the first `j` reaches the shared `jalr` with a0 = o + adjust >> 1 set in its delay slot; for the second the `j` skips the non-virtual
// `lw s1, 0x24(s1)`).
void ptmf_chain_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], slot = ctx.gpr[5], argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t adjust = word(rt, self + 0x18u);
    const std::uint32_t object = word(rt, slot);
    const std::uint32_t pointer = word(rt, self + 0x14u);
    const std::uint32_t receiver = object + sar1(adjust);
    const std::uint32_t first = call_guest(rt, ctx, ptmf_function(rt, receiver, pointer, adjust), {receiver, object, pointer, adjust});
    const std::uint32_t adjust2 = word(rt, self + 0x28u);
    const std::uint32_t receiver2 = first + sar1(adjust2);
    const std::uint32_t function = ptmf_function(rt, receiver2, word(rt, self + 0x24u), adjust2);
    (void)call_guest(rt, ctx, 0x08BF7BE0u, {frame.at(0u), argument});
    (void)call_guest(rt, ctx, function, {receiver2, frame.at(0u)});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// Pointer-to-member chain with a returned holder (54 instructions): this = a0, a1 = the destination holder, a2 = the address of a pointer to an object o. The first
// pointer {this[0x14], this[0x18]} is called on o as in "pointer-to-member chain, then a fill" and returns p; the second {this[0x1C], this[0x20]} is called on p
// with a0 = &h0 (the frame slot the callee fills: a hidden return slot), a1 = p + adjust >> 1; then fill(&h1, &h0) (the frame's second slot), h1 is assigned to a1
// (func_08A5BAD8) unless a1 is h1 itself, and h1, h0 are released. v0 = the last release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): both virtual forms (adjust bit 0: the `bnez` / `beqz` land on the
// vtable loads; the second also sets a1 = p + adjust >> 1 in its non-virtual arm after `lw v0, 0x1C(s0)`) and the destination being h1 itself (the `beq` lands on
// the release of h1; its delay slot only sets a0).
void ptmf_chain_returned(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t fill) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5], slot = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t adjust = word(rt, self + 0x18u);
    const std::uint32_t object = word(rt, slot);
    const std::uint32_t pointer = word(rt, self + 0x14u);
    const std::uint32_t receiver = object + sar1(adjust);
    const std::uint32_t first = call_guest(rt, ctx, ptmf_function(rt, receiver, pointer, adjust), {receiver, pointer, object, adjust});
    const std::uint32_t adjust2 = word(rt, self + 0x20u);
    const std::uint32_t receiver2 = first + sar1(adjust2);
    const std::uint32_t function = ptmf_function(rt, receiver2, word(rt, self + 0x1Cu), adjust2);
    (void)call_guest(rt, ctx, function, {frame.at(0u), receiver2});  // a2/a3 as the first callee left them
    (void)call_guest(rt, ctx, fill, {frame.at(4u), frame.at(0u)});
    if (out != frame.at(4u)) (void)call_guest(rt, ctx, kAssignSmart, {out, word(rt, frame.at(4u))});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// Release a range of strings (33 instructions): a0 = begin, a1 = end; for each element (stride `stride`) its word is a string's characters pointer, released
// with release_string (drop = `drop`; the shared empty representation is `empty`). v0 = the last element's (the characters pointer when its representation
// is the empty one, else the last call's); an empty range leaves v0 alone.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a string holding the shared empty representation (the `beq` against
// `s3` lands on the loop test, v0 = the loaded characters pointer; the delay slot only sets a0 = chars - 4). The empty range, a counter step that stays above zero and a
// string that is dropped were all taken.
void release_strings(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t empty, std::uint32_t stride, std::uint32_t drop) {
    const std::uint32_t begin = ctx.gpr[4], end = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (begin == end) return;
    std::uint32_t cursor = begin, result = ctx.gpr[2];
    do {
        const std::uint32_t chars = word(rt, cursor);
        cursor += stride;
        result = release_string(rt, ctx, frame.at(0u), chars, empty, drop, chars);
    } while (cursor != end);
    ctx.gpr[2] = result;
}

// Vector parameter with a default w (49 instructions): a0 = this. func_08AB85F0(&name) builds a string holder (frame +4); func_088755E4(this, &v, &name, 4)
// reads the named parameter into the four floats at frame +0x14, the count of values it found being the word at frame +0xC; when the count is 3 or more
// (equal to 3: v.w = 1.0f, the store to frame +0x20), object = func_0886AFA8(this[0]) and apply(object + 4, &v) runs. The holder's string is released
// (release_string) and v0 is that code's (the empty representation 0x08D75298 when nothing was released).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a count of 4 or more (the `bne` lands on the `jal func_0886AFA8`
// without the 1.0f store), everything of a count below 3 (the `bnez` lands on the string release: `lw v1, 4(sp)`) and a holder with the shared empty representation
// (the `beq` lands on the epilogue with v0 = 0x08D75298, set before it; a0 = chars - 4 and a1 = -1 only).
void vector_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t apply) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    (void)call_guest(rt, ctx, 0x08AB85F0u, {frame.at(4u)});
    (void)call_guest(rt, ctx, 0x088755E4u, {self, frame.at(0x14u), frame.at(4u), 4u});
    const std::int32_t count = static_cast<std::int32_t>(word(rt, frame.at(0xCu)));
    if (count >= 3) {
        if (count == 3) put(rt, frame.at(0x20u), 0x3F800000u);
        const std::uint32_t object = call_guest(rt, ctx, 0x0886AFA8u, {word(rt, self)});
        (void)call_guest(rt, ctx, apply, {object + 4u, frame.at(0x14u)});
    }
    ctx.gpr[2] = release_string(rt, ctx, frame.at(0u), word(rt, frame.at(4u)), kStringEmpty, 0x08B93554u, kStringEmpty);
}

// Object parameter lookup (48 instructions): a0 = this. func_08AB85F0(&name) builds a string holder (frame +0x10); func_088755E4(this, &v, &name, 3) reads the
// named parameter (frame +4); when the count of values it found (the word at frame +0x18) is 3 or more, object = func_088757B0(this, 0x08CA0074) and, when
// it is not null, apply(object + 0xC, &v) runs. The holder's string is released (release_string).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a count below 3 (the `bnez` lands on the string release, `lw v1,
// 0x10(sp)`), a null object (the `beqz` lands there too; its delay slot only sets a0 = v0 + 0xC) and a holder with the shared empty representation (the `beq` lands on the
// epilogue with v0 = 0x08D75298, set before it).
void object_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t apply) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    (void)call_guest(rt, ctx, 0x08AB85F0u, {frame.at(0x10u)});
    (void)call_guest(rt, ctx, 0x088755E4u, {self, frame.at(4u), frame.at(0x10u), 3u});
    if (static_cast<std::int32_t>(word(rt, frame.at(0x18u))) >= 3) {
        const std::uint32_t object = call_guest(rt, ctx, 0x088757B0u, {self, 0x08CA0074u});
        if (object != 0u) (void)call_guest(rt, ctx, apply, {object + 0xCu, frame.at(4u)});
    }
    ctx.gpr[2] = release_string(rt, ctx, frame.at(0u), word(rt, frame.at(0x10u)), kStringEmpty, 0x08B93554u, kStringEmpty);
}

// Scaled sum of two vectors (52 instructions, plain FPU floats): a0 = this, a1 = out. this[first] points to an object whose three floats at +off are the vector
// A; this[second] points to the object whose three floats at +off are B; this[scale_a] and this[scale_b] are the floats they are scaled by (func_08B4A5E8(&v, s)
// scales a vector in place). A and B are copied to the frame (+0xC, +0), scaled, and summed into the frame's third vector (+0x18) with func_08B4AA34(&c, &c, &b)
// (the copy of A first, then c = c + b); func_08BADADC(&out_slot, &c) is the result, a0 = the frame slot (+0x30) that holds a1. v0 = the word the slot holds (what that call leaves in v0).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void scaled_vector_sum(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t offset, std::uint32_t second,
                       std::uint32_t scale_a, std::uint32_t scale_b) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    const std::uint32_t a_object = word(rt, self + first);
    put(rt, frame.at(0x30u), ctx.gpr[5]);
    const std::uint32_t a = a_object + offset;
    const std::uint32_t a_x = word(rt, a);
    const std::uint32_t b_object = word(rt, self + second);
    const float a_scale = float_at(rt, self + scale_a);
    put(rt, frame.at(0xCu), a_x);
    const std::uint32_t b = b_object + offset;
    const std::uint32_t a_y = word(rt, a + 4u);
    const float b_scale = float_at(rt, self + scale_b);
    put(rt, frame.at(0x10u), a_y);
    const std::uint32_t a_z = word(rt, a + 8u);
    put(rt, frame.at(0x14u), a_z);
    (void)call_guest(rt, ctx, 0x08B4A5E8u, {frame.at(0xCu), a}, {a_scale});
    const std::uint32_t b_x = word(rt, b);
    put(rt, frame.at(0u), b_x);
    put(rt, frame.at(4u), word(rt, b + 4u));
    put(rt, frame.at(8u), word(rt, b + 8u));
    (void)call_guest(rt, ctx, 0x08B4A5E8u, {frame.at(0u)}, {b_scale});
    put(rt, frame.at(0x18u), word(rt, frame.at(0xCu)));
    put(rt, frame.at(0x1Cu), word(rt, frame.at(0x10u)));
    put(rt, frame.at(0x20u), word(rt, frame.at(0x14u)));
    (void)call_guest(rt, ctx, 0x08B4AA34u, {frame.at(0x18u), frame.at(0x18u), frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08BADADCu, {frame.at(0x30u), frame.at(0x18u)});
    // func_08BADADC leaves in v0 the word it read last through its a0: the slot's own word (it is registered as Void, so its v0 is not read from the call; the slot is in
    // this function's new frame, which nothing the callee stores to can alias).
    ctx.gpr[2] = word(rt, frame.at(0x30u));
}

// Build two holders (36 instructions): a0 = out, a1 = x, a2 = y. first(&h0, y) fills the frame's holder h0 (frame +0), second(&h1, &h0) h1 (+4), then
// func_08A744F0(&h2, x, &h1) h2 (+8) and func_08A7C5C8(out, &h2); h2, h1, h0 are released. v0 = the last release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void build_two_holders(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t second) {
    const std::uint32_t out = ctx.gpr[4], x = ctx.gpr[5], y = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    (void)call_guest(rt, ctx, first, {frame.at(0u), y});
    (void)call_guest(rt, ctx, second, {frame.at(4u), frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A744F0u, {frame.at(8u), x, frame.at(4u)});
    (void)call_guest(rt, ctx, 0x08A7C5C8u, {out, frame.at(8u)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(8u)});
    (void)call_guest(rt, ctx, kReleaseLocal, {frame.at(4u)});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// Make a node and call (30 instructions): a0 = s1. node = func_08A5BC20(0x38, 4); func_08A69B3C(&h, text) builds a holder h (frame +0), func_08A52660(node, &h)
// stores it, func_08A69D68(&h) releases it, node[0] = vtable, frame[4] = node and func_08A52AB0(a0, &frame[4]) registers it. v0 = that call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void make_node(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t text, std::uint32_t vtable) {
    const std::uint32_t owner = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t node = call_guest(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    (void)call_guest(rt, ctx, 0x08A69B3Cu, {frame.at(0u), text});
    (void)call_guest(rt, ctx, 0x08A52660u, {node, frame.at(0u)});
    (void)call_guest(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    put(rt, node, vtable);
    put(rt, frame.at(4u), node);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A52AB0u, {owner, frame.at(4u)});
}

// Store the result (13 instructions): this = a0; *this = target(a1 (sign-extended low byte of a2), a3 ...): a0 = a1, a1 = seb(a2), a2 = a3. v0 = the result.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void store_result_byte(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t result = call_guest(rt, ctx, target, {ctx.gpr[5], static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(ctx.gpr[6]))), ctx.gpr[7]});
    put(rt, self, result);
    ctx.gpr[2] = result;
}

// Alloc 0x14 then call (21 instructions): a0 = this. p = func_08B62344(0x14) (a block), target(p, this[0xC], this[0x18]); v0 = p.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void alloc_then_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t second = word(rt, self + 0xCu);
    const std::uint32_t block = call_guest(rt, ctx, 0x08B62344u, {0x14u});
    (void)call_guest(rt, ctx, target, {block, second, word(rt, self + 0x18u)});
    ctx.gpr[2] = block;
}

// Call through a static pair (19 instructions): a0 = a, a1 = b, a2 = c, a3 = pointer to two words {p, q}. The frame gets {static[0], static[1], p, q}; then
// target(a, b, &frame[0], c, &frame[2], p, q, static[1]) (a2 = the frame, a3 = the old a2, t0 = &frame[2], t1 = p, t2 = q, t3 = static[1]). v0 = the call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void call_with_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t pair, std::uint32_t target) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t t2 = word(rt, ctx.gpr[7] + 4u);
    const std::uint32_t t1 = word(rt, ctx.gpr[7]);
    const std::uint32_t t3 = word(rt, pair + 4u);
    const std::uint32_t v0 = word(rt, pair);
    put(rt, frame.at(0xCu), t2);
    put(rt, frame.at(8u), t1);
    put(rt, frame.at(0u), v0);
    put(rt, frame.at(4u), t3);
    ctx.gpr[2] = call_guest(rt, ctx, target, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), ctx.gpr[6], frame.at(8u), t1, t2, t3});
}

// Query then truncate (22 instructions): a0 = this, a1 stays. r = func_089363FC(this); func_08936584(r, a1, this[0x30], this[0x34], &frame) fills the frame;
// target(0x08D6950C, f12 = (float) halfword at frame + half) answers a float, and v0 = trunc(that float) (the saturating cvt.w.s of the PSP).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void query_truncate(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t half, std::uint32_t target) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x90u);
    const std::uint32_t handle = call_guest(rt, ctx, 0x089363FCu, {self});
    (void)call_guest(rt, ctx, 0x08936584u, {handle, ctx.gpr[5], word(rt, self + 0x30u), word(rt, self + 0x34u), frame.at(0u)});
    const float value = static_cast<float>(static_cast<std::int32_t>(rt.memory().load16(frame.at(half))));
    const GuestResult answer = call_guest(rt, ctx, target, {0x08D6950Cu}, {value});
    const std::uint32_t truncated = ctx.fpu_float_to_word(answer.f0, 1u);
    ctx.fpr[0] = std::bit_cast<float>(truncated);
    ctx.gpr[2] = truncated;
}

// Call a slot with a flag (20 instructions): a0 = this, a2 = argument. func_08A76AA0(&h) fills the frame's holder, then this[slot](&h, 1, argument) and h is
// released. v0 = the release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void call_slot_with_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot) {
    const std::uint32_t self = ctx.gpr[4], argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    (void)call_guest(rt, ctx, word(rt, self + slot), {frame.at(0u), 1u, argument});
    ctx.gpr[2] = call_guest(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// Init flagged (17 instructions): this = a0, flag = a1: this[4] = flag != 0, this[0] = vtable, then this[8] = func_08B5C230(this, flag != 0); v0 = that result.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line; a zero flag is `sltu a1, zero, a1` = 0: no
// route passes it, and the stored word and the argument are then 0).
void init_flagged(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4], flag = ctx.gpr[5] != 0u ? 1u : 0u;
    GuestFrame frame(ctx, 0x10u);
    put(rt, self + 4u, flag);
    put(rt, self, vtable);
    const std::uint32_t result = call_guest(rt, ctx, 0x08B5C230u, {self, flag});
    put(rt, self + 8u, result);
    ctx.gpr[2] = result;
}

// Shared acquire (21 instructions): a0 = dest, a1 = source. dest[1] = source[9]; when that block pointer is not null and its counter (block[1]) is not 0,
// the counter is incremented (ll / sc: one thread, a plain store); then dest[0] = source[8] and v0 = dest, v1 = the word stored.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null block (the `beqz` lands on `lw v1, 0(v1)`), a zero counter
// (the `beql` takes its delay slot `lw v1, 0(v1)` and lands on the epilogue) and an ll / sc that fails (the retry loop: never with one thread).
void shared_acquire(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4], source = ctx.gpr[5];
    const std::uint32_t block = word(rt, source + 0x24u);
    put(rt, dest + 4u, block);
    if (block != 0u) {
        const std::uint32_t count = word(rt, block + 4u);
        if (count != 0u) put(rt, block + 4u, count + 1u);
    }
    const std::uint32_t value = word(rt, source + 0x20u);
    put(rt, dest, value);
    ctx.gpr[3] = value;
    ctx.gpr[2] = dest;
}

// First non-zero (3 instructions): v0 = a0 when it is not 0, else a1.
void first_non_zero(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4] != 0u ? ctx.gpr[4] : ctx.gpr[5];
}

// func_08C22D58(this) (33 instructions): this[0] = vtable 0x08CD3150, the string characters pointer at this[0x14] is released (release_string; empty representation
// 0x08D74FD0, drop func_08B8542C), func_08A780B0(this) destroys the base and func_08A5BC54(this, 0x1C, 4) frees the block (the call's result is v0). The load of
// this[0x14] comes before the store of the vtable.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the string being the empty representation (the `beq` lands on
// `jal func_08A780B0`, a0 = this from the delay slot of the first release call it skips) and a counter step that stays above zero (the `bgtz` lands on the same call).
void func_08C22D58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t chars = word(rt, self + 0x14u);
    put(rt, self, 0x08CD3150u);
    (void)release_string(rt, ctx, frame.at(0u), chars, 0x08D74FD0u, 0x08B8542Cu, 0u);
    (void)call_guest(rt, ctx, 0x08A780B0u, {self});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BC54u, {self, 0x1Cu, 4u});
}

// func_08B879E8(this) (34 instructions): this[0] = vtable; func_08B876E0(this[0x14], this[0x18], &frame) runs (a0 = this[0x14] is loaded in the delay slot),
// then, when this[0x14] (reloaded) is not null, func_08A5BC54(this[0x14], ((this[0x1C] - this[0x14]) >> 2) << 2, 4) frees the storage; func_08A780B0(this) and the
// block free func_08A5BC54(this, 0x20, 4) follow (their result is v0).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null storage pointer (the `beqz` lands on `jal func_08A780B0`; the
// delay slot only computes the size).
void func_08B879E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CB89B8u);
    const std::uint32_t second = word(rt, self + 0x18u);
    (void)call_guest(rt, ctx, 0x08B876E0u, {word(rt, self + 0x14u), second, frame.at(0u)});
    const std::uint32_t storage = word(rt, self + 0x14u);
    const std::uint32_t end = word(rt, self + 0x1Cu);
    const std::uint32_t bytes = static_cast<std::uint32_t>(static_cast<std::int32_t>(end - storage) >> 2) << 2;
    if (storage != 0u) (void)call_guest(rt, ctx, 0x08A5BC54u, {storage, bytes, 4u});
    (void)call_guest(rt, ctx, 0x08A780B0u, {self});
    ctx.gpr[2] = call_guest(rt, ctx, 0x08A5BC54u, {self, 0x20u, 4u});
}

// func_08BE8B78(this): this[0] = vtable 0x08CC9628; when this[0x24] is not null func_08A5B930(this[0x24]); func_08965A3C(this); then the tail call
// func_08A5BC54(this, 0x40, 4) frees the block.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null this[0x24] (the `beqz` lands on `jal func_08965A3C`; its delay slot only
// sets a0 = v1).
void func_08BE8B78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t held = word(rt, self + 0x24u);
        put(rt, self, 0x08CC9628u);
        if (held != 0u) (void)call_guest(rt, ctx, 0x08A5B930u, {held});
        (void)call_guest(rt, ctx, 0x08965A3Cu, {self});
    }
    tail_call(ctx, 0x08A5BC54u, {self, 0x40u, 4u});
}

// func_08B9432C(this, source, 0) (29 instructions): chars = *source (a string's characters pointer), rep = chars - 0xC. A negative length (rep[2] < 0, the string cannot be shared)
// is copied: *this = func_08B94288(rep, &frame, 0); else the shared representation is referenced: unless rep is the empty one (0x08D75298) func_08B6085C(chars - 4, 1) runs and
// *this = chars. v0 = the stored word.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the negative length (the `bltz` lands on `jal func_08B94288`, with a0 = rep from its
// delay slot, a1 = sp and a2 = 0 set before; the word it returns is stored) and a representation other than the empty one (the `beq` falls into `jal func_08B6085C`, a0 = chars - 4
// set before the `beq` and a1 = 1 in its delay slot; the `j` after it stores chars, loaded into v0 in its delay slot): the routes only copy the empty string.
void func_08B9432C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t chars = word(rt, source);
    const std::uint32_t rep = chars - 0xCu;
    std::uint32_t stored = chars;
    if (static_cast<std::int32_t>(word(rt, rep + 8u)) < 0) {
        stored = call_guest(rt, ctx, 0x08B94288u, {rep, frame.at(0u), 0u});
    } else if (rep != kStringEmpty) {
        (void)call_guest(rt, ctx, 0x08B6085Cu, {chars - 4u, 1u});
    }
    put(rt, self, stored);
    ctx.gpr[2] = stored;
}

// func_08966C70(this, out0, out1) (32 instructions): when func_08966174(this, out0, out1) answers non-zero, each non-null out gets a float of the object at this[0x80]: out0 the one at
// +0x10, out1 the one at +0x14; when it answers 0 each non-null out gets 0. v0 = this[0x80] if an out was written on the non-zero path (it is reloaded for each), else the answer.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the zero answer (the `beqz` lands on `bnel s0, zero`, whose delay slot stores 0
// to *s0 when it is taken; the same for s1), a null out0 and a null out1 on the non-zero path (the `beqz` lands on the next test / the epilogue).
void func_08966C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out0 = ctx.gpr[5], out1 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t answer = call_guest(rt, ctx, 0x08966174u, {self, out0, out1});
    ctx.gpr[2] = answer;
    if (answer == 0u) {
        if (out0 != 0u) put(rt, out0, 0u);
        if (out1 != 0u) put(rt, out1, 0u);
        return;
    }
    if (out0 != 0u) {
        const std::uint32_t object = word(rt, self + 0x80u);
        ctx.gpr[2] = object;
        put(rt, out0, word(rt, object + 0x10u));
    }
    if (out1 != 0u) {
        const std::uint32_t object = word(rt, self + 0x80u);
        ctx.gpr[2] = object;
        put(rt, out1, word(rt, object + 0x14u));
    }
}

// func_08C309B4(this, a, b, c) (13 instructions): *this = func_08C308D4(a, b, c); v0 = the result.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void func_08C309B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t result = call_guest(rt, ctx, 0x08C308D4u, {ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    put(rt, self, result);
    ctx.gpr[2] = result;
}

// func_08B8A004(a, b, c) (14 instructions): func_08B89F70(a, b, &frame, c, static[0], static[1]) with the frame = {static[0], static[1]} and the static pair at 0x08C87E04
// (a2 = the frame, a3 = the old a2, t0 = static[0], t1 = static[1]). v0 = the call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none (straight line).
void func_08B8A004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t second = word(rt, 0x08C87E04u + 4u);
    const std::uint32_t first = word(rt, 0x08C87E04u);
    put(rt, frame.at(0u), first);
    put(rt, frame.at(4u), second);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B89F70u, {ctx.gpr[4], ctx.gpr[5], frame.at(0u), ctx.gpr[6], first, second});
}

// For each node of a list, call a member function (59 instructions): a0 = out, a1 = the address of the begin node pointer, a2 = the address of the end node pointer, a3 = a functor
// {s6 = a3[0], s7 = a3[4], s5 = a3[8], function = a3[0xC], adjust = a3[0x10]}. Each node (word 1 of a node is the next pointer) gets the pointer-to-member call
// (a0 = node + adjust >> 1, a1 = s6, a2 = s7, a3 = s5; adjust bit 0: the function is a vtable offset); the loop ends when the next pointer equals the end. The functor is
// then copied to out (+0, +4, +8, +0xC, +0x10 from the saved adjust) and v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual form (adjust bit 0: the `beqz` falls into the vtable loads
// *(*(node + adjust >> 1) + function)) and an empty list (the first `beq` lands on the copy). A null node would read the word at address 0 (`movz v0, zero, s0`): never.
void for_each_node(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], begin_slot = ctx.gpr[5], end_slot = ctx.gpr[6], functor = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t adjust = word(rt, functor + 0x10u);
    const std::uint32_t begin = word(rt, begin_slot);
    const std::uint32_t end = word(rt, end_slot);
    put(rt, frame.at(0u), adjust);
    const std::uint32_t third = word(rt, functor + 8u), second = word(rt, functor + 4u), first = word(rt, functor);
    const std::uint32_t function = word(rt, functor + 0xCu);
    if (begin != end) {
        std::uint32_t node = begin;
        do {
            const std::uint32_t object = node + sar1(adjust);
            (void)call_guest(rt, ctx, ptmf_function(rt, object, function, adjust), {object, first, second, third});
            node = word(rt, node != 0u ? node + 4u : 0u);
        } while (node != end);
    }
    put(rt, out + 8u, third);
    put(rt, out + 4u, second);
    put(rt, out, first);
    put(rt, out + 0xCu, function);
    put(rt, out + 0x10u, word(rt, frame.at(0u)));
    ctx.gpr[2] = out;
}

// For each element at a 0x400 stride, call a member function (50 instructions): a0 = out, a1 = begin, a2 = end, a3 = a functor {function = a3[0], adjust = a3[4], argument = a3[8]}.
// Each element e from begin up to end (stride 0x400) gets the pointer-to-member call (a0 = e + adjust >> 1, a1 = argument; adjust bit 0: the function is a vtable offset). The
// functor is then copied to out (+0, +4, +8) and v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual form (the `beqz` falls into the vtable loads) and an empty
// range (the `beq` lands on the copy; its delay slot only loads fp).
void for_each_stride(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], begin = ctx.gpr[5], end = ctx.gpr[6], functor = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t argument = word(rt, functor + 8u), function = word(rt, functor);
    const std::uint32_t adjust = word(rt, functor + 4u);
    if (begin != end) {
        std::uint32_t element = begin;
        do {
            const std::uint32_t object = element + sar1(adjust);
            element += 0x400u;
            (void)call_guest(rt, ctx, ptmf_function(rt, object, function, adjust), {object, argument});
        } while (end != element);
    }
    put(rt, out, function);
    put(rt, out + 4u, adjust);
    put(rt, out + 8u, argument);
    ctx.gpr[2] = out;
}

// Shared callback (67 instructions): a0 = this. this[0xB] is a shared block (or null) whose counter (block[1]) is incremented while the callback runs; the pointer to
// member function {this[0xC], this[0xD]} is called on this[0xA] (a0 = this[0xA] + adjust >> 1, a1 = the adjust word, a2 = this[0xA]); when the byte at this[0xE] is 0 then
// func_08AC05F4(this) runs and then this[2] = 0 (a non-zero byte skips both: the `bnez` lands on the null-block test); the block's counter is decremented and, when it was 1, block->vtable[2](block) runs and the block's second counter (block[2]) is
// decremented; when that was 1 too, the code ends with a tail call to block->vtable[3](block) (v0 = 1 then). v0 = the last value the code leaves otherwise (the byte or the
// func_08AC05F4 result, or 1 once the block's counter was touched).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual form of the call (bit 0 of the adjust word: the `beqz` falls into the
// vtable loads and the `j` reaches `sra a0, a1, 1`), a null block (the first `beqz` lands on `lw a1, 0x34(s0)`, the second on the epilogue) and a second counter that is not 1
// (the `bne` lands on the epilogue with v0 = 1; its delay slot restores ra). The routes do take: a block counter that is not 1 and the last-reference path with the tail call.
void shared_callback(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t block = word(rt, self + 0x2Cu);
    {
        GuestFrame frame(ctx, 0x10u);
        if (block != 0u) put(rt, block + 4u, word(rt, block + 4u) + 1u);
        const std::uint32_t adjust = word(rt, self + 0x34u);
        const std::uint32_t base = word(rt, self + 0x28u);
        const std::uint32_t function = ptmf_function(rt, base + sar1(adjust), word(rt, self + 0x30u), adjust);
        (void)call_guest(rt, ctx, function, {base + sar1(adjust), adjust, base});
        std::uint32_t result = rt.memory().load8(self + 0xEu);
        if (result == 0u) {
            result = call_guest(rt, ctx, 0x08AC05F4u, {self});
            put(rt, self + 8u, 0u);
        }
        ctx.gpr[2] = result;
        if (block == 0u) return;
        const std::uint32_t before = word(rt, block + 4u);
        put(rt, block + 4u, before - 1u);
        ctx.gpr[2] = 1u;
        if (before != 1u) return;
        (void)call_guest(rt, ctx, word(rt, word(rt, block) + 8u), {block});
        const std::uint32_t before_weak = word(rt, block + 8u);
        put(rt, block + 8u, before_weak - 1u);
        ctx.gpr[2] = 1u;
        if (before_weak != 1u) return;
    }
    const std::uint32_t target = word(rt, word(rt, block) + 0xCu);
    ctx.gpr[25] = target;
    tail_call(ctx, target, {block});
}


} // namespace

// func_XXXXXXXX (ptmf chain fill): see ptmf_chain_fill.
void func_08BF833C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BF7C30u); }
// Batch 141 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
void func_08B8CA4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08BEE5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08BEE6AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BE9040u); }
void func_08BF22DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BE9040u); }
void func_08BF29AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08BF3C94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BF3BD0u); }
void func_08BF4578(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08BFAE80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BF3BD0u); }
void func_08BFD9C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08BFE104(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08BFF174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08BFF2CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08C00298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08C055B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BE9040u); }
void func_08C13EC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BE9040u); }
void func_08C1D7F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08C1E78C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08C2671C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08C26AC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BF3BD0u); }
void func_08C37078(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BF3BD0u); }
void func_08C39F94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08C31598u); }
void func_08C3A410(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BF3BD0u); }
void func_08C3A4DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08C31490u); }
void func_08C3A5A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08C314D8u); }
void func_08C3A744(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BE9040u); }
void func_08C3A810(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08C31550u); }
void func_08BFBE18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08B8C564u); }
void func_08C1E150(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_fill(rt, ctx, 0x08BF3BD0u); }

// func_XXXXXXXX (ptmf chain holder): see ptmf_chain_holder.
void func_08BF8574(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_holder(rt, ctx); }
void func_08C155C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_holder(rt, ctx); }

// func_XXXXXXXX (ptmf chain returned): see ptmf_chain_returned.
void func_08C26644(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_returned(rt, ctx, 0x08BF7C60u); }
void func_08C2E300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ptmf_chain_returned(rt, ctx, 0x08C2C9C0u); }

// func_XXXXXXXX (release strings): see release_strings.
void func_08BBEBE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_strings(rt, ctx, 0x08D75298u, 16u, 0x08B93554u); }
void func_08C2FBF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_strings(rt, ctx, 0x08D74FD0u, 4u, 0x08B8542Cu); }

// func_XXXXXXXX (vector parameter): see vector_parameter.
void func_08877D64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_parameter(rt, ctx, 0x0887CAF0u); }
void func_08877E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_parameter(rt, ctx, 0x0887CAC8u); }
void func_08877EEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { vector_parameter(rt, ctx, 0x0887CAA0u); }

// func_XXXXXXXX (object parameter): see object_parameter.
void func_08877418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object_parameter(rt, ctx, 0x08AF43ECu); }
void func_088774D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object_parameter(rt, ctx, 0x08AF43D0u); }
void func_08877598(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { object_parameter(rt, ctx, 0x08AF43B4u); }

// func_XXXXXXXX (scaled vector sum): see scaled_vector_sum.
void func_088A076C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scaled_vector_sum(rt, ctx, 0x00000008u, 16u, 0x0000000Cu, 0x00000010u, 0x00000014u); }
void func_088CE644(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { scaled_vector_sum(rt, ctx, 0x00000004u, 20u, 0x00000008u, 0x0000000Cu, 0x00000010u); }

// func_XXXXXXXX (build two holders): see build_two_holders.
void func_08C00A44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_two_holders(rt, ctx, 0x089BAC2Cu, 0x08C00A04u); }
void func_08C3055C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_two_holders(rt, ctx, 0x08A983E8u, 0x08C2C9C0u); }

// func_XXXXXXXX (make node): see make_node.
void func_08B8C1E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08C99740u, 0x08CB79B8u); }
void func_08B8C25C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_node(rt, ctx, 0x08C99748u, 0x08CB7F98u); }

// func_XXXXXXXX (store result byte): see store_result_byte.
void func_08BBD380(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_result_byte(rt, ctx, 0x08BBD2DCu); }
void func_08C193EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_result_byte(rt, ctx, 0x08C19348u); }

// func_XXXXXXXX (alloc then call): see alloc_then_call.
void func_08C3C2FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { alloc_then_call(rt, ctx, 0x08B84C84u); }
void func_08C3E010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { alloc_then_call(rt, ctx, 0x08B84E84u); }

// func_XXXXXXXX (call with pair): see call_with_pair.
void func_08C0C1EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_pair(rt, ctx, 0x08C8D9FCu, 0x08C0C0E4u); }
void func_08C15C70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_with_pair(rt, ctx, 0x08C8E178u, 0x08C15B68u); }

// func_XXXXXXXX (query truncate): see query_truncate.
void func_08932988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { query_truncate(rt, ctx, 0x0000001Eu, 0x08B48F04u); }
void func_089329E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { query_truncate(rt, ctx, 0x00000020u, 0x08B48F54u); }

// func_XXXXXXXX (call slot with flag): see call_slot_with_flag.
void func_08BE5F88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_slot_with_flag(rt, ctx, 0x00000018u); }
void func_08BE5FD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { call_slot_with_flag(rt, ctx, 0x00000014u); }

// func_XXXXXXXX (init flagged): see init_flagged.
void func_08B647C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_flagged(rt, ctx, 0x08CDC110u); }
void func_08C5E298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_flagged(rt, ctx, 0x08CDB870u); }

// func_XXXXXXXX (shared acquire): see shared_acquire.
void func_08BC0DA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_acquire(rt, ctx); }
void func_08BD3C3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_acquire(rt, ctx); }

// func_XXXXXXXX (first non zero): see first_non_zero.
void func_08BCF6F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { first_non_zero(rt, ctx); }
void func_08BD2D68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { first_non_zero(rt, ctx); }

// func_XXXXXXXX (for each node): see for_each_node.
void func_08BAA850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_node(rt, ctx); }
void func_08BAA93C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_node(rt, ctx); }

// func_XXXXXXXX (for each stride): see for_each_stride.
void func_08BB0F28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_stride(rt, ctx); }
void func_08BB0FF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_stride(rt, ctx); }
void func_08BB10B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { for_each_stride(rt, ctx); }

// func_XXXXXXXX (shared callback): see shared_callback.
void func_08BCFDEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_callback(rt, ctx); }
void func_08BD3718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_callback(rt, ctx); }
void func_08BE08B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_callback(rt, ctx); }
void func_08BE0EB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { shared_callback(rt, ctx); }

} // namespace hot_batch_79
} // namespace gt::game

#define HOT79(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_79::name, gt::ReturnKind::kind)
HOT79(0x08BF833Cu, func_08BF833C, Word);
// Batch 141 (2026-10-25): the rest of this family, which no route reaches — verified by synthetic cases.
// HOT79(0x08B8CA4Cu, func_08B8CA4C, Word);  // no comparable synthetic case yet
// HOT79(0x08BEE5E0u, func_08BEE5E0, Word);  // no comparable synthetic case yet
// HOT79(0x08BEE6ACu, func_08BEE6AC, Word);  // no comparable synthetic case yet
HOT79(0x08BF22DCu, func_08BF22DC, Word);
// HOT79(0x08BF29ACu, func_08BF29AC, Word);  // no comparable synthetic case yet
// HOT79(0x08BF3C94u, func_08BF3C94, Word);  // no comparable synthetic case yet
HOT79(0x08BF4578u, func_08BF4578, Word);
// HOT79(0x08BFAE80u, func_08BFAE80, Word);  // no comparable synthetic case yet
// HOT79(0x08BFD9C0u, func_08BFD9C0, Word);  // no comparable synthetic case yet
// HOT79(0x08BFE104u, func_08BFE104, Word);  // no comparable synthetic case yet
HOT79(0x08BFF174u, func_08BFF174, Word);
// HOT79(0x08BFF2CCu, func_08BFF2CC, Word);  // no comparable synthetic case yet
// HOT79(0x08C00298u, func_08C00298, Word);  // no comparable synthetic case yet
// HOT79(0x08C055B0u, func_08C055B0, Word);  // no comparable synthetic case yet
HOT79(0x08C13EC0u, func_08C13EC0, Word);
// HOT79(0x08C1D7F8u, func_08C1D7F8, Word);  // no comparable synthetic case yet
// HOT79(0x08C1E78Cu, func_08C1E78C, Word);  // no comparable synthetic case yet
// HOT79(0x08C2671Cu, func_08C2671C, Word);  // no comparable synthetic case yet
// HOT79(0x08C26AC0u, func_08C26AC0, Word);  // no comparable synthetic case yet
// HOT79(0x08C37078u, func_08C37078, Word);  // no comparable synthetic case yet
// HOT79(0x08C39F94u, func_08C39F94, Word);  // no comparable synthetic case yet
HOT79(0x08C3A410u, func_08C3A410, Word);
// HOT79(0x08C3A4DCu, func_08C3A4DC, Word);  // no comparable synthetic case yet
// HOT79(0x08C3A5A8u, func_08C3A5A8, Word);  // no comparable synthetic case yet
// HOT79(0x08C3A744u, func_08C3A744, Word);  // no comparable synthetic case yet
// HOT79(0x08C3A810u, func_08C3A810, Word);  // no comparable synthetic case yet
HOT79(0x08BFBE18u, func_08BFBE18, Word);
HOT79(0x08C1E150u, func_08C1E150, Word);
HOT79(0x08BF8574u, func_08BF8574, Word);
HOT79(0x08C155C4u, func_08C155C4, Word);
HOT79(0x08C26644u, func_08C26644, Word);
HOT79(0x08C2E300u, func_08C2E300, Word);
// HOT79(0x08BBEBE4u, func_08BBEBE4, Word); // the routes never call this member (compared 0 in the fast gate: its sibling func_08C2FBF8 is the one they reach)
HOT79(0x08C2FBF8u, func_08C2FBF8, Word);
HOT79(0x08877D64u, func_08877D64, Word);
HOT79(0x08877E28u, func_08877E28, Word);
HOT79(0x08877EECu, func_08877EEC, Word);
HOT79(0x08877418u, func_08877418, Word);
HOT79(0x088774D8u, func_088774D8, Word);
HOT79(0x08877598u, func_08877598, Word);
HOT79(0x088A076Cu, func_088A076C, Word);
HOT79(0x088CE644u, func_088CE644, Word);
HOT79(0x08C00A44u, func_08C00A44, Word);
HOT79(0x08C3055Cu, func_08C3055C, Word);
HOT79(0x08B8C1E4u, func_08B8C1E4, Word);
HOT79(0x08B8C25Cu, func_08B8C25C, Word);
// HOT79(0x08BBD380u, func_08BBD380, Word); // the routes never call this member (compared 0 in the fast gate: its sibling func_08C193EC is the one they reach)
HOT79(0x08C193ECu, func_08C193EC, Word);
HOT79(0x08C3C2FCu, func_08C3C2FC, Word);
HOT79(0x08C3E010u, func_08C3E010, Word);
HOT79(0x08C0C1ECu, func_08C0C1EC, Word);
HOT79(0x08C15C70u, func_08C15C70, Word);
HOT79(0x08932988u, func_08932988, Word);
HOT79(0x089329E0u, func_089329E0, Word);
HOT79(0x08BE5F88u, func_08BE5F88, Word);
HOT79(0x08BE5FD8u, func_08BE5FD8, Word);
HOT79(0x08B647C0u, func_08B647C0, Word);
HOT79(0x08C5E298u, func_08C5E298, Word);
HOT79(0x08BC0DA8u, func_08BC0DA8, Word);
HOT79(0x08BD3C3Cu, func_08BD3C3C, Word);
HOT79(0x08BCF6F4u, func_08BCF6F4, Word);
HOT79(0x08BD2D68u, func_08BD2D68, Word);
HOT79(0x08BAA850u, func_08BAA850, Word);
HOT79(0x08BAA93Cu, func_08BAA93C, Word);
HOT79(0x08BB0F28u, func_08BB0F28, Word);
HOT79(0x08BB0FF0u, func_08BB0FF0, Word);
HOT79(0x08BB10B8u, func_08BB10B8, Word);
HOT79(0x08BCFDECu, func_08BCFDEC, Word);
HOT79(0x08BD3718u, func_08BD3718, Word);
HOT79(0x08BE08B4u, func_08BE08B4, Word);
HOT79(0x08BE0EB8u, func_08BE0EB8, Word);
HOT79(0x08C22D58u, func_08C22D58, Word);
HOT79(0x08B879E8u, func_08B879E8, Word);
HOT79(0x08BE8B78u, func_08BE8B78, Word);
HOT79(0x08B9432Cu, func_08B9432C, Word);
HOT79(0x08966C70u, func_08966C70, Word);
HOT79(0x08C309B4u, func_08C309B4, Word);
HOT79(0x08B8A004u, func_08B8A004, Word);
