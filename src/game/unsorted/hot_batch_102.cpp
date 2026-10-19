// Hot batch 102: reached, unregistered functions of main with batch 101 (PR #242) excluded (probe `b101p`: the 13,968 recompiled functions that no `src/` file registered, up to 300 instructions, run on the five routes), filtered to those with no `sce*` import within five calls (a static walk of `jal` and `j` targets past the end of `.text`),
// no VFPU instruction and no definition (registered, commented or written-but-unregistered) elsewhere in `src/` or in batch 101. Written from `tools.disasm` one function at a time. Role words in the comments describe access shapes; none is a game name.
//
// Written 54, registered 42 (28 compared in the real-time full gate, all 42 in the five-route fast gate, 0 mismatches, sabotage pinned on all of them). Not registered (12, each has a commented HOT102 line with the reason): ten whose compared cases are all "not comparable" (their callees reach sce* imports the harness cannot record, or the routes never call them:
// func_08BD74BC, func_08B35418, func_08AEB850, func_08B2DBF0, func_0897BB68, func_08B098A8, func_08825AE8, func_0885E940, func_08B7771C, func_08B6C020) and two that write memory but no compared case changed it, a dense race run of 3,000 cases included (func_0888879C, func_08B00E90).
//
// The pool this time: of the 225 reached, unregistered functions of at most 300 instructions the VFPU matrix helpers are the hottest (func_08B53D2C 10M probed calls) and are kept out; the 62 without VFPU and without an import stub within five calls were read, plus the ones the static walk marked whose stubs looked harmless (those that reach only 0x08C7A888..0x08C7A8B0 mostly compared; func_0897BB68 reaches only those and still did not). Left: the libm wrappers over 64-bit soft-float helpers (func_08B6EF68, func_08B67418, func_08B6728C), the copy-on-write string temporaries (func_0883C988 and its twin) and the 200+ instruction parsers.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f15): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value; `call_setf` also passes the float registers. The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs. The delay slot of a `jal` / `jalr` runs BEFORE the call, a `bnel` / `beql` / `bgtzl` / `blezl` / `bc1fl` / `bc1tl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path; the dead stack is not reproduced, the frames that callees read or write are (`GuestFrame`, offsets as in the original).
// Branches the compared cases do not take are listed in each function's comment; they were read against the asm by hand.
//
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_102 {
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
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// The value `lui` leaves for an address built by a relocated lui/addiu (or lui/load/store) pair: the high half, rounded up when the low half is negative.
inline constexpr std::uint32_t lui_of(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A guest call with integer and float argument registers set like the original sets them before the jal (a0-a3, t0-t3 and f12-f15): v0 / v1 / f0 of the callee stay in the context.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call the way the compiler issues it: v1 = the vtable (object[0]), v0 = the slot's function, then jalr v0. A callee that returns without setting v0 hands the function's address on to the caller's code, so both registers are set like the original sets them.
inline GuestResult call_virtual_full(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                     std::initializer_list<std::uint32_t> arguments) {
    const std::uint32_t table = word(rt, object);
    const std::uint32_t function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
inline std::uint32_t call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                  std::initializer_list<std::uint32_t> arguments) {
    return call_virtual_full(rt, ctx, object, offset, arguments).v0;
}
// ---- functions ----

// The reference-count steps of the shared-object holder: `ll/sc` on a word of guest memory (single-threaded here, the host never preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful `sc`, then `addiu v0, zero, 1` in the delay slot); true when the old value was 1 (the count reached zero).
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// func_08B207A4(object = a0, value = a1): object[0] = a1, then the tail jump func_08B20760 with every argument register as it is (the store sits in the delay slot of the j). v0 untouched. (The other 17 words of the recompiler's span are the entries of sibling functions.)
void func_08B207A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    put(rt, ctx.gpr[4], ctx.gpr[5]);
    tail_call(ctx, 0x08B20760u, {});
}

// func_08891C54(pointer = a0): a0 = *a0 (the word load sits in the delay slot of the j), then the tail jump func_08891BF8. v0 untouched.
void func_08891C54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[4] = word(rt, ctx.gpr[4]);
    tail_call(ctx, 0x08891BF8u, {});
}

// The two list walkers: the head of a singly linked list lives in a static word; every node's second word is a pointer to a vtable whose slot 8 is called with a0 = 0 (and v1 = the vtable, v0 = the slot's function), then the next node is node[0]. v0 ends as the last call's answer (the lui of the address pair when the list is empty), a0 = 0 after a call.
inline void walk_list_calling_slot_8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t head_address) {
    ctx.gpr[2] = lui_of(head_address);
    std::uint32_t node = word(rt, head_address);
    while (node != 0u) {
        const std::uint32_t table = word(rt, node + 4u);
        const std::uint32_t function = word(rt, table + 8u);
        ctx.gpr[3] = table;
        ctx.gpr[2] = function;
        (void)call_set(rt, ctx, function, {0u});
        node = word(rt, node);
    }
}
// func_08C4CB24(): the list whose head is the word 0x08D77354.
void func_08C4CB24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { walk_list_calling_slot_8(rt, ctx, 0x08D77354u); }
// func_08C4CADC(): the list whose head is the word 0x08D77358.
void func_08C4CADC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { walk_list_calling_slot_8(rt, ctx, 0x08D77358u); }

// func_0888879C(holder = a0): holder[0] (a0), when not null, gets the virtual call slot 8 (a0 = it); then the shared block s0 = holder[4]: when null return; else count it down at +4 (when it was not 1 return, v0 = 1), call its slot 8, count it down at +8 (when not 1 return), and tail-jump to its slot 0xC with a0 = s0 (v0 = its vtable).
void func_0888879C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4];
    const std::uint32_t first = word(rt, holder);
    ctx.gpr[4] = first;
    if (first != 0u) (void)call_virtual_full(rt, ctx, first, 8u, {first});
    const std::uint32_t block = word(rt, holder + 4u);
    if (block == 0u) return;
    if (!count_down(rt, ctx, block + 4u)) return;
    (void)call_virtual_full(rt, ctx, block, 8u, {block});
    if (!count_down(rt, ctx, block + 8u)) return;
    const std::uint32_t table = word(rt, block);
    ctx.gpr[2] = table;
    ctx.gpr[4] = block;
    ctx.gpr[25] = word(rt, table + 0xCu);
    tail_call(ctx, ctx.gpr[25], {block});
}

// func_08873EA0(object = a0): the same release sequence for two shared blocks, object[0x18] (skipped when null) and object[0x10]: count +4 down (not 1: next), virtual slot 8; count +8 down (not 1: next), virtual slot 0xC. The last block (object[0x10], skipped when null) tail-jumps to slot 0xC instead of calling it. v0 = 1 when a count did not reach zero.
// Branches the routes do not take (checked against the asm by hand): a null object[0x18] or object[0x10] block and a count that does not reach zero (the early returns with v0 = 1); every compared case destroyed both blocks and took the tail jump.
void func_08873EA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    std::uint32_t block = word(rt, object + 0x18u);
    if (block != 0u) {
        if (count_down(rt, ctx, block + 4u)) {
            (void)call_virtual_full(rt, ctx, block, 8u, {block});
            if (count_down(rt, ctx, block + 8u)) (void)call_virtual_full(rt, ctx, block, 0xCu, {block});
        }
    }
    block = word(rt, object + 0x10u);
    if (block == 0u) return;
    if (!count_down(rt, ctx, block + 4u)) return;
    (void)call_virtual_full(rt, ctx, block, 8u, {block});
    if (!count_down(rt, ctx, block + 8u)) return;
    const std::uint32_t table = word(rt, block);
    ctx.gpr[2] = table;
    ctx.gpr[4] = block;
    ctx.gpr[25] = word(rt, table + 0xCu);
    tail_call(ctx, ctx.gpr[25], {block});
}

// func_08BD74BC(object = a0): when object[0x2C] (s1, a shared block) is not null its count at +4 goes up by one (v1 = the address, v0 = 1); object byte 0xE = 0, object[0x1C] = 0; the member-function pointer pair (object[0x28] = this adjustment base, object[0x34] = the adjustment shifted left once with bit 0 = "virtual", object[0x30] = the function or the vtable offset) is called with a0 = base + (adjustment >> 1) (arithmetic); then s0 = object[0x1C] is re-read; the block (when not null) is counted down at +4 (not 1: return) and +8 like the sequences above, with virtual calls of slot 8 and slot 0xC. v0 = s0 = object[0x1C] on every path.
void func_08BD74BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t block = word(rt, object + 0x2Cu);
    if (block != 0u) {
        const std::uint32_t counter = word(rt, block + 4u);
        ctx.gpr[3] = block + 4u;
        ctx.gpr[2] = 1u;
        put(rt, block + 4u, counter + 1u);
    }
    const std::uint32_t adjustment = word(rt, object + 0x34u);
    put8(rt, object + 0xEu, 0u);
    put(rt, object + 0x1Cu, 0u);
    const std::uint32_t base = word(rt, object + 0x28u);
    const std::uint32_t shifted = static_cast<std::uint32_t>(static_cast<std::int32_t>(adjustment) >> 1);
    std::uint32_t function;
    ctx.gpr[2] = adjustment & 1u;
    if ((adjustment & 1u) != 0u) {
        const std::uint32_t address = base + shifted;
        ctx.gpr[2] = address;
        const std::uint32_t table = word(rt, address);
        function = word(rt, table + word(rt, object + 0x30u));
    } else {
        function = word(rt, object + 0x30u);
    }
    ctx.gpr[3] = function;
    ctx.gpr[4] = base + shifted;
    (void)call_keep(rt, ctx, function, {base + shifted});
    const std::uint32_t result = word(rt, object + 0x1Cu);
    ctx.gpr[2] = result;
    if (block == 0u) return;
    if (!count_down(rt, ctx, block + 4u)) {
        ctx.gpr[2] = result;
        return;
    }
    (void)call_virtual_full(rt, ctx, block, 8u, {block});
    if (count_down(rt, ctx, block + 8u)) (void)call_virtual_full(rt, ctx, block, 0xCu, {block});
    ctx.gpr[2] = result;
}

// func_089D5690(a = a0, b = a1): equality of two objects of one class: the answers of virtual slot 0x24 must match, then words +0x18 and +0x1C, then the answers of slot 0x114, then the pointer words at +0x24 (a pointer counts only when the word it points at, +4, is not zero; otherwise it counts as null), then +0x28, +0x2C and +0x30. v0 = 1 when everything matches, 0 at the first difference.
// Branches the routes do not take (checked against the asm by hand): everything after the first comparison (the single compared case had different slot-0x24 answers): words +0x18 / +0x1C, the slot-0x114 answers, the +0x24 pointer test, +0x28, +0x2C, +0x30 and the result 1.
void func_089D5690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[4], b = ctx.gpr[5];
    const auto different = [&]() {
        ctx.gpr[2] = 0u;
    };
    std::uint32_t first = call_virtual(rt, ctx, a, 0x24u, {a});
    std::uint32_t second = call_virtual(rt, ctx, b, 0x24u, {b});
    if (first != second) return different();
    if (word(rt, a + 0x18u) != word(rt, b + 0x18u)) return different();
    if (word(rt, a + 0x1Cu) != word(rt, b + 0x1Cu)) return different();
    first = call_virtual(rt, ctx, a, 0x114u, {a});
    second = call_virtual(rt, ctx, b, 0x114u, {b});
    if (first != second) return different();
    const auto counted = [&](std::uint32_t pointer) { return (pointer != 0u && word(rt, pointer + 4u) != 0u) ? pointer : 0u; };
    if (counted(word(rt, a + 0x24u)) != counted(word(rt, b + 0x24u))) return different();
    if (word(rt, a + 0x28u) != word(rt, b + 0x28u)) return different();
    if (word(rt, a + 0x2Cu) != word(rt, b + 0x2Cu)) return different();
    ctx.gpr[2] = word(rt, a + 0x30u) == word(rt, b + 0x30u) ? 1u : 0u;
}

// func_089281E4(object = a0, mode = a1): object[0x1FB8] = mode. Modes 0 and 3: func_089273B4(object); when it answers zero, mode 0 tail-jumps to func_089274C8(object, object + 0x2194) and mode 3 to func_08927148(object, 3); otherwise func_089274C8(object, object + 0x2184) and the tail jump func_08927BE4(object, object + 0x2184, 1, 0). Other modes: func_089273B4(object); a non-zero answer, or a mode other than 5, tail-jumps to func_08927148(object, mode); mode 5 with a zero answer calls func_08927148(object, 3) and sets object[0x1FB8] = 3.
// Branches the routes do not take (checked against the asm by hand): mode 0 with a zero answer (the tail jump with object + 0x2194), mode 3, every other mode (the tail jump with the mode, mode 5 with a zero answer); only mode 0 with a non-zero answer ran.
void func_089281E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], mode = ctx.gpr[5];
    put(rt, object + 0x1FB8u, mode);
    const std::uint32_t answer = call_set(rt, ctx, 0x089273B4u, {object});
    ctx.gpr[4] = object;
    if (mode == 0u || mode == 3u) {
        if (answer == 0u) {
            ctx.gpr[5] = 3u;
            if (mode != 0u) return tail_call(ctx, 0x08927148u, {object, 3u});
            return tail_call(ctx, 0x089274C8u, {object, object + 0x2194u});
        }
        const std::uint32_t pair = object + 0x2184u;
        (void)call_set(rt, ctx, 0x089274C8u, {object, pair});
        ctx.gpr[4] = object;
        return tail_call(ctx, 0x08927BE4u, {object, pair, 1u, 0u});
    }
    if (answer != 0u || mode != 5u) return tail_call(ctx, 0x08927148u, {object, mode});
    (void)call_set(rt, ctx, 0x08927148u, {object, 3u});
    put(rt, object + 0x1FB8u, 3u);
}

// func_0890778C(header = a0): when the byte-swapped word 0 is 0x4743616D (v0 = that constant otherwise): s2 = header - swap(header[4]); header[4] = swap(header); header[0x14] = swap(swap(header[0x14]) + s2); then for i below swap(header[0x10]) (re-read each time) func_08907634(swap(header[0x14]) + i * 0x20, s2); header[0x1C] = swap(swap(header[0x1C]) + s2); then the same for i below swap(header[0x18]) with swap(header[0x1C]). v0 = 0 at the end (the last loop test).
// Branches the routes do not take (checked against the asm by hand): the magic-mismatch return and the body of the second loop (header[0x18] was zero in the compared case).
void func_0890778C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto swap = [](std::uint32_t value) { return __builtin_bswap32(value); };
    const std::uint32_t header = ctx.gpr[4];
    ctx.gpr[2] = 0x4743616Du;
    if (swap(word(rt, header)) != 0x4743616Du) return;
    const std::uint32_t base = header - swap(word(rt, header + 4u));
    put(rt, header + 4u, swap(header));
    put(rt, header + 0x14u, swap(swap(word(rt, header + 0x14u)) + base));
    std::uint32_t index = 0u;
    while (true) {
        const std::uint32_t scaled = index << 5;
        const bool more = index < swap(word(rt, header + 0x10u));
        ctx.gpr[2] = more ? 1u : 0u;
        ctx.gpr[3] = scaled;
        ctx.gpr[5] = base;
        if (!more) break;
        ++index;
        (void)call_set(rt, ctx, 0x08907634u, {scaled + swap(word(rt, header + 0x14u)), base});
    }
    put(rt, header + 0x1Cu, swap(swap(word(rt, header + 0x1Cu)) + base));
    index = 0u;
    while (true) {
        const std::uint32_t scaled = index << 5;
        const bool more = index < swap(word(rt, header + 0x18u));
        ctx.gpr[2] = more ? 1u : 0u;
        ctx.gpr[3] = scaled;
        ctx.gpr[5] = base;
        if (!more) break;
        ++index;
        (void)call_set(rt, ctx, 0x08907634u, {scaled + swap(word(rt, header + 0x1Cu)), base});
    }
}

// func_089191DC(object = a0, a1, a2): object[0x1F8] = a1, object[0x1FC] = a2, object[0x200] = 0, func_089262EC(object[0x204]), object[0x208] = 0, the byte 0x08CF212C = 0. Then a1 (re-read from object[0x1F8]) points at a table: count = a1[0x20] (signed), base = a1[0x24]; the entries are 0x7C bytes apart and each holds three pointers at +0x34 / +0x38 / +0x3C to words; the first entry with a pointed-at word equal to 0x64 stops the search at its index. When the search ends at index == count (no entry, or an empty table) the byte 0x08CF212C = 1 and v0 = 1; otherwise v0 / v1 keep the last pointer / word read.
// Branches the routes do not take (checked against the asm by hand): count <= 0, an entry whose pointed-at word is 0x64 (the search stopping early with the pointer / word left in v0 / v1); every compared case scanned the whole table.
void func_089191DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t argument = word(rt, object + 0x204u);
    put(rt, object + 0x1F8u, ctx.gpr[5]);
    put(rt, object + 0x1FCu, ctx.gpr[6]);
    put(rt, object + 0x200u, 0u);
    (void)call_set(rt, ctx, 0x089262ECu, {argument});
    put(rt, object + 0x208u, 0u);
    put8(rt, 0x08CF212Cu, 0u);
    const std::uint32_t table = word(rt, object + 0x1F8u);
    ctx.gpr[2] = table;
    const std::int32_t count = signed_of(word(rt, table + 0x20u));
    std::int32_t index = 0;
    if (count > 0) {
        const std::uint32_t base = word(rt, table + 0x24u);
        for (;; ) {
            const std::uint32_t entry = base + 0x7Cu * static_cast<std::uint32_t>(index);
            bool found = false;
            for (unsigned k = 0u; k < 3u; ++k) {
                const std::uint32_t pointer = word(rt, entry + 0x34u + 4u * k);
                const std::uint32_t value = word(rt, pointer);
                // The first test (and the third of the first entry) loads the pointer into v1 and the word into v0; the others the other way round.
                const bool swapped = (k != 0u) && !(k == 2u && index == 0);
                ctx.gpr[swapped ? 2 : 3] = pointer;
                ctx.gpr[swapped ? 3 : 2] = value;
                if (value == 0x64u) {
                    found = true;
                    break;
                }
            }
            if (found) break;
            ++index;
            ctx.gpr[2] = index < count ? 1u : 0u;
            if (index >= count) break;
        }
    }
    ctx.gpr[4] = static_cast<std::uint32_t>(index);
    if (count == index) {
        ctx.gpr[2] = 1u;
        put8(rt, 0x08CF212Cu, 1u);
    }
}

// func_088BBA00(a0, owner = a1): s1 = owner[0x24] (a block with two floats at +0x58 and +0x5C), s0 = owner[0x1C] (an object). When 5.0 < block[0x58] the object's virtual slot 0x28 is called with (a0), then again with the answer; the second answer replaces the first when it is not negative (s2); the same for slot 0x2C and block[0x5C] (s3). Both start at -1. Then byte block[0x13] picks the result: 1 -> s2 when not negative; 2 -> s3 when not negative; otherwise s2 (not negative, block byte[0x96] != 3 and block byte[0x12] != 2) or s3 (not negative, block byte[0x96] != 4 and block byte[0x12] != 1) or -1.
// Branches the routes do not take (checked against the asm by hand): a second slot answer that is not negative, the s3 results and every result selection except the first (the kind-1 test with s2 >= 0 ran); the -1 results.
void func_088BBA00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t argument = ctx.gpr[4], owner = ctx.gpr[5];
    const std::uint32_t block = word(rt, owner + 0x24u);
    const std::uint32_t object = word(rt, owner + 0x1Cu);
    std::int32_t s2 = -1, s3 = -1;
    if (5.0f < float_at(rt, block + 0x58u)) {
        s2 = signed_of(call_virtual(rt, ctx, object, 0x28u, {object, argument}));
        const std::int32_t again = signed_of(call_virtual(rt, ctx, object, 0x28u, {object, static_cast<std::uint32_t>(s2)}));
        if (again >= 0) s2 = again;
    }
    if (5.0f < float_at(rt, block + 0x5Cu)) {
        s3 = signed_of(call_virtual(rt, ctx, object, 0x2Cu, {object, argument}));
        const std::int32_t again = signed_of(call_virtual(rt, ctx, object, 0x2Cu, {object, static_cast<std::uint32_t>(s3)}));
        if (again >= 0) s3 = again;
    }
    const auto finish = [&](std::int32_t value) { ctx.gpr[2] = static_cast<std::uint32_t>(value); };
    const std::uint32_t kind = byte_u(rt, block + 0x13u);
    if (kind == 1u) {
        if (s2 >= 0) return finish(s2);
    } else {
        ctx.gpr[4] = 2u;
        if (kind == 2u && s3 >= 0) return finish(s3);
        if (s2 >= 0 && sext8(byte_u(rt, block + 0x96u)) != 3u && byte_u(rt, block + 0x12u) != 2u) return finish(s2);
    }
    if (s3 < 0) return finish(-1);
    if (sext8(byte_u(rt, block + 0x96u)) == 4u) return finish(-1);
    if (byte_u(rt, block + 0x12u) != 1u) return finish(s3);
    finish(-1);
}

// A counted-pointer slot assigned: when `replacement` differs from `previous` (the value read before), its count at +4 goes up (v1 = the address, v0 = 1), the block that was in the slot (when not null) is counted down at +4 (not 1: done) and, if that reached zero, destroyed through virtual slot 8 and counted down at +8 (reaching zero: virtual slot 0xC), then the slot gets the replacement.
inline void assign_counted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t replacement, std::uint32_t previous) {
    if (replacement == previous) return;
    if (replacement != 0u) {
        const std::uint32_t counter = word(rt, replacement + 4u);
        ctx.gpr[3] = replacement + 4u;
        ctx.gpr[2] = 1u;
        put(rt, replacement + 4u, counter + 1u);
    }
    const std::uint32_t block = word(rt, slot);
    if (block != 0u && count_down(rt, ctx, block + 4u)) {
        (void)call_virtual_full(rt, ctx, block, 8u, {block});
        if (count_down(rt, ctx, block + 8u)) (void)call_virtual_full(rt, ctx, block, 0xCu, {block});
    }
    put(rt, slot, replacement);
}

// func_088AAAE8(holder = a0, source = a1): holder[0] is a block base b: b[0x10] = source[0]; the counted pointer b[0x14] takes source[4]; b[0x18] = source[8]; the counted pointer b[0x1C] takes source[0xC]. v0 = source[8] unless a count or a virtual call changed it.
void func_088AAAE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4], source = ctx.gpr[5];
    const std::uint32_t base = word(rt, holder);
    const std::uint32_t previous = word(rt, base + 0x14u);
    put(rt, base + 0x10u, word(rt, source));
    assign_counted(rt, ctx, base + 0x14u, word(rt, source + 4u), previous);
    const std::uint32_t middle = word(rt, source + 8u);
    const std::uint32_t previous_last = word(rt, base + 0x1Cu);
    ctx.gpr[2] = middle;
    ctx.gpr[3] = previous_last;
    put(rt, base + 0x18u, middle);
    assign_counted(rt, ctx, base + 0x1Cu, word(rt, source + 0xCu), previous_last);
}

// func_088AAC74(holder = a0, source = a1, value = a2): the same copy one level up: holder[0] is a block base b: b[0x48] = a2, b[0] = source[0], the counted pointer b[4] takes source[4], b[8] = source[8], the counted pointer b[0xC] takes source[0xC].
void func_088AAC74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4], source = ctx.gpr[5];
    put(rt, word(rt, holder) + 0x48u, ctx.gpr[6]);
    const std::uint32_t base = word(rt, holder);
    const std::uint32_t first = word(rt, source);
    const std::uint32_t previous = word(rt, base + 4u);
    ctx.gpr[2] = first;
    ctx.gpr[3] = previous;
    put(rt, base, first);
    assign_counted(rt, ctx, base + 4u, word(rt, source + 4u), previous);
    const std::uint32_t middle = word(rt, source + 8u);
    const std::uint32_t previous_last = word(rt, base + 0xCu);
    ctx.gpr[2] = middle;
    ctx.gpr[3] = previous_last;
    put(rt, base + 8u, middle);
    assign_counted(rt, ctx, base + 0xCu, word(rt, source + 0xCu), previous_last);
}

// func_08B6C020(x = f12): the library square root on single floats, bit by bit. |x| above 0x7F7FFFFF (infinity, NaN): f0 = f12 = x + x * x; +-0: f0 = x; a negative x: f0 = f12 = (x - x) / (x - x), the PSP's default NaN. Otherwise the biased exponent (a denormal is shifted up first, counting the shifts) and the 24-bit mantissa give 25 rounds of the restoring square root; a remainder left over rounds the result up when its last bit is set; the result bits are (root >> 1) + 0x3F000000 + (half exponent << 23).
void func_08B6C020(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    const float x = ctx.fpr[12];
    const std::uint32_t bits = std::bit_cast<std::uint32_t>(x);
    const std::uint32_t magnitude = bits & 0x7FFFFFFFu;
    const auto finish_float = [&](float value) {
        ctx.fpr[12] = value;
        ctx.fpr[0] = value;
    };
    if (magnitude > 0x7F7FFFFFu) {
        float sum = x + psp_mul(x, x);
        if (std::isnan(sum) && !std::isnan(x)) sum = std::bit_cast<float>(0x7FC00000u);
        return finish_float(sum);
    }
    if (magnitude == 0u) {
        ctx.fpr[0] = x;
        return;
    }
    if (static_cast<std::int32_t>(bits) < 0) return finish_float(std::bit_cast<float>(0x7FC00000u));
    std::int32_t exponent = static_cast<std::int32_t>(bits) >> 23;
    std::uint32_t mantissa = bits;
    if (magnitude <= 0x7FFFFFu) {
        std::uint32_t shifts = 0u;
        if ((bits & 0x800000u) == 0u) {
            do {
                mantissa <<= 1;
                ++shifts;
            } while ((mantissa & 0x800000u) == 0u);
        }
        exponent = exponent - static_cast<std::int32_t>(shifts) + 1;
    }
    std::uint32_t scaled = (mantissa & 0x7FFFFFu) | 0x800000u;
    const std::int32_t unbiased = exponent - 0x7F;
    if ((unbiased & 1) != 0) scaled <<= 1;
    std::uint32_t remainder = scaled << 1;
    const std::int32_t half = unbiased >> 1;
    std::uint32_t root = 0u, accumulated = 0u, bit = 0x1000000u;
    for (unsigned rounds = 0x19u; rounds != 0u; --rounds) {
        const std::uint32_t trial = root + bit;
        if (!(static_cast<std::int32_t>(remainder) < static_cast<std::int32_t>(trial))) {
            remainder -= trial;
            accumulated += bit;
            root = trial + bit;
        }
        remainder <<= 1;
        bit >>= 1;
    }
    if (remainder != 0u) accumulated += accumulated & 1u;
    const std::uint32_t result = (static_cast<std::uint32_t>(static_cast<std::int32_t>(accumulated) >> 1)) + 0x3F000000u + (static_cast<std::uint32_t>(half) << 23);
    ctx.gpr[2] = result;
    finish_float(std::bit_cast<float>(result));
}

// func_08B020EC(object = a0): walks a chain of 6-byte records. s3 = object + 0x98, s2 = object + 0x88. The first record: object halfword +0x98 (signed) is an index into the table whose base is the word at *(object[0xA0]) (6 bytes a record; a negative index means none). The walk keeps a 16-byte state in its frame at sp + 0x10 (the garbage word at sp + 0 is copied to its first word, object[0xA0] to the second, the record address to the third, 0 to the fourth). For each record r (until the state's third word is zero): halfword r[2] with bit 14 set and bit 15 clear follows r's halfword +4 (signed; negative ends the walk) to the next record, *(state word 1) + 6 * index; any other record calls func_08C47DFC(sp + 0x20, s3, state), copies the 16 bytes it wrote at sp + 0x20 over the state, stores the low half of object halfword +0x88 at r[4], and appends the record's index ((r - **(object[0x90])) / 6 as a signed halfword) to the ring at s2: halfword s2[2] too when object halfword +0x88 is -1, halfword s2[0], and the count word s2[4] goes up by one. v0 is the register the last step left (the count, the loaded word of the follow path, the halfword of a negative follow or object[0xA0]).
// Branches the routes do not take (checked against the asm by hand): records with bit 14 set and bit 15 clear (following the link halfword, negative or not).
void func_08B020EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50);
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t s3 = object + 0x98u, s2 = object + 0x88u;
    const std::uint32_t index = sext16(half_u(rt, object + 0x98u));
    std::uint32_t record = 0u;
    if (signed_of(index) >= 0) record = word(rt, word(rt, object + 0xA0u)) + ((index << 3) - (index << 1));
    const std::uint32_t table_word = word(rt, object + 0xA0u);
    ctx.gpr[2] = table_word;
    const std::uint32_t garbage = word(rt, frame.at(0u));
    put(rt, frame.at(0x14u), table_word);
    put(rt, frame.at(0x10u), garbage);
    put(rt, frame.at(0x18u), record);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(8u), record);
    put(rt, frame.at(4u), table_word);
    put(rt, frame.at(0x1Cu), 0u);
    while (true) {
        const std::uint32_t current = word(rt, frame.at(0x18u));
        ctx.gpr[4] = frame.at(0x20u);
        ctx.gpr[5] = s3;
        ctx.gpr[6] = frame.at(0x10u);
        if (current == 0u) break;
        const std::uint32_t flags = half_u(rt, current + 2u);
        if (((flags >> 14) & 1u) != 0u && signed_of(sext16(flags)) >= 0) {
            put(rt, frame.at(0x1Cu), current);
            const std::uint32_t next = sext16(half_u(rt, current + 4u));
            ctx.gpr[2] = next;
            if (signed_of(next) >= 0) {
                const std::uint32_t scaled = (next << 3) - (next << 1);
                const std::uint32_t base_pointer = word(rt, frame.at(0x14u));
                const std::uint32_t base = word(rt, base_pointer);
                ctx.gpr[3] = base_pointer;
                ctx.gpr[2] = base;
                put(rt, frame.at(0x18u), base + scaled);
            } else {
                put(rt, frame.at(0x18u), 0u);
            }
            continue;
        }
        (void)call_set(rt, ctx, 0x08C47DFCu, {frame.at(0x20u), s3, frame.at(0x10u)});
        put(rt, frame.at(0x10u), word(rt, frame.at(0x20u)));
        put(rt, frame.at(0x14u), word(rt, frame.at(0x24u)));
        put(rt, frame.at(0x18u), word(rt, frame.at(0x28u)));
        put(rt, frame.at(0x1Cu), word(rt, frame.at(0x2Cu)));
        put16(rt, current + 4u, sext16(half_u(rt, object + 0x88u)));
        const std::uint32_t table = word(rt, word(rt, object + 0x90u));
        const std::uint32_t slot = sext16(half_u(rt, object + 0x88u));
        const std::uint32_t quotient = static_cast<std::uint32_t>(static_cast<std::int32_t>(current - table) >> 1) * 0xAAAAAAABu;
        const std::uint32_t ring_index = sext16(quotient);
        ctx.gpr[3] = 0xFFFFFFFFu;
        if (slot == 0xFFFFFFFFu) put16(rt, s2 + 2u, ring_index);
        const std::uint32_t count = word(rt, s2 + 4u);
        put16(rt, s2, ring_index);
        ctx.gpr[2] = count + 1u;
        ctx.gpr[5] = ring_index;
        put(rt, s2 + 4u, count + 1u);
    }
}

// func_08C3EEE0(first = a0, last = a1, depth = a2): the quick-sort loop of the library's introsort over 8-byte elements keyed by the float at +4. Fewer than 17 elements: return (v0 = last - first, the byte distance). Depth zero or the iteration count reaching the depth: the tail jump func_08C3EE20(first, last, last). Otherwise the pivot is the median of the first, middle ((count / 2) * 8 bytes in) and last element by float key (comparisons as c.lt.s, so a NaN key picks per the asm's order), copied to the frame at sp + 0..7; func_08C3EA44(first, last, &pivot) splits at s0; func_08C3EEE0(s0, last, depth - 1) sorts the upper part (the recursion) and the loop goes on with last = s0 (v0 = s0 - first at the end of every round).
// Branches the routes do not take (checked against the asm by hand): the whole loop: every compared case had at most 16 elements, so the pivot choice, the split calls, the recursion and both tail jumps were read from the asm only.
void func_08C3EEE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30);
    const std::uint32_t first = ctx.gpr[4], limit = ctx.gpr[6];
    std::uint32_t last = ctx.gpr[5];
    ctx.gpr[2] = last - first;
    std::int32_t count = signed_of(last - first) >> 3;
    if (count < 0x11) return;
    std::uint32_t depth = limit, iteration = 0u;
    if (limit == 0u) return tail_call(ctx, 0x08C3EE20u, {first, last, last});
    while (true) {
        const std::uint32_t half = static_cast<std::uint32_t>(signed_of(static_cast<std::uint32_t>(count) + (static_cast<std::uint32_t>(count) >> 31)) >> 1);
        const std::uint32_t middle = first + (half << 3);
        const std::uint32_t end = last - 8u;
        const float f1 = float_at(rt, first + 4u), f0 = float_at(rt, middle + 4u), f2 = float_at(rt, end + 4u);
        --depth;
        std::uint32_t pivot;
        if (f1 < f0) {
            if (f0 < f2) pivot = middle;
            else if (f1 < f2) pivot = end;
            else pivot = first;
        } else {
            if (f1 < f2) pivot = first;
            else if (f0 < f2) pivot = end;
            else pivot = middle;
        }
        put(rt, frame.at(4u), word(rt, pivot + 4u));
        ++iteration;
        put(rt, frame.at(0u), word(rt, pivot));
        const std::uint32_t split = call_set(rt, ctx, 0x08C3EA44u, {first, last, frame.at(0u)});
        (void)call_set(rt, ctx, 0x08C3EEE0u, {split, last, depth});
        ctx.gpr[2] = split - first;
        count = signed_of(split - first) >> 3;
        last = split;
        if (count < 0x11) return;
        if (limit == iteration) return tail_call(ctx, 0x08C3EE20u, {first, last, last});
    }
}

// func_08BDBB44(first = a0, last = a1, depth = a2): the same introsort loop for 8-byte elements keyed by a 64-bit value (low word at +0 unsigned, high word at +4 signed): fewer than 0x88 bytes: return 1; depth zero or iteration count == depth: the tail jump func_08BDBA80(first, last, last) (v0 = 0); the pivot (median of the first, middle and last by key) goes by value (low, high) to func_08BDB7D4(first, last, low, high), whose answer s0 bounds the next round; func_08BDBB44(s0, last, depth - 1) sorts the upper part. v0 = 1 when the next round is small.
// Branches the routes do not take (checked against the asm by hand): the whole loop: every compared case was under 0x88 bytes, so the pivot choice, the split calls, the recursion and both tail jumps were read from the asm only.
void func_08BDBB44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], limit = ctx.gpr[6];
    std::uint32_t last = ctx.gpr[5];
    std::uint32_t bytes = last - first;
    ctx.gpr[3] = bytes;
    if (signed_of(bytes) < 0x88) {
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[2] = 0u;
    std::uint32_t depth = limit, iteration = 0u;
    if (limit == 0u) return tail_call(ctx, 0x08BDBA80u, {first, last, last});
    const auto less = [&](std::uint32_t a, std::uint32_t b) {
        const std::int32_t a_high = signed_of(word(rt, a + 4u)), b_high = signed_of(word(rt, b + 4u));
        return a_high < b_high || (a_high == b_high && word(rt, a) < word(rt, b));
    };
    while (true) {
        const std::int32_t count = signed_of(bytes) >> 3;
        const std::uint32_t half = static_cast<std::uint32_t>(signed_of(static_cast<std::uint32_t>(count) + (static_cast<std::uint32_t>(count) >> 31)) >> 1);
        const std::uint32_t middle = first + (half << 3);
        const std::uint32_t end = last - 8u;
        --depth;
        std::uint32_t pivot;
        if (less(first, middle)) {
            if (less(middle, end)) pivot = middle;
            else if (less(first, end)) pivot = end;
            else pivot = first;
        } else {
            if (less(first, end)) pivot = first;
            else if (less(middle, end)) pivot = end;
            else pivot = middle;
        }
        const std::uint32_t pivot_low = word(rt, pivot), pivot_high = word(rt, pivot + 4u);
        const std::uint32_t split = call_set(rt, ctx, 0x08BDB7D4u, {first, last, pivot_low, pivot_high});
        (void)call_set(rt, ctx, 0x08BDBB44u, {split, last, depth});
        bytes = split - first;
        last = split;
        ++iteration;
        const bool small = signed_of(bytes) < 0x88;
        ctx.gpr[2] = small ? 1u : 0u;
        ctx.gpr[3] = bytes;
        if (small) return;
        if (iteration == limit) return tail_call(ctx, 0x08BDBA80u, {first, last, last});
    }
}

// func_08BB1D9C(first = a0, last = a1, key = a2): a find over the pointer array [first, last): returns the address of the first element p for which func_088B5D6C(*(*p + 0xC), key) answers non-zero, or `last`. Unrolled by four (blocks of 16 bytes), then the 1-3 elements left over (a remainder count of 0 or above 3 after the blocks returns `last`).
// Branches the routes do not take (checked against the asm by hand): every range with one or more elements (the compared ranges were empty): the four-element blocks and the 1-3 leftover tests.
void func_08BB1D9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[4], last = ctx.gpr[5], key = ctx.gpr[6];
    const auto found_at = [&](std::uint32_t slot) { return call_set(rt, ctx, 0x088B5D6Cu, {word(rt, word(rt, slot) + 0xCu), key}) != 0u; };
    const auto result = [&](std::uint32_t value) { ctx.gpr[2] = value; };
    std::uint32_t current = first;
    const std::int32_t blocks = signed_of(last - first) >> 4;
    std::int32_t remaining = signed_of(last - first) >> 2;
    if (blocks > 0) {
        for (std::int32_t done = 1;; ++done) {
            for (std::uint32_t step = 0u; step < 16u; step += 4u) {
                if (found_at(current + step)) return result(current + step);
            }
            current += 16u;
            if (blocks == done) break;
        }
        remaining = signed_of(last - current) >> 2;
    }
    if (remaining == 3) {
        if (found_at(current)) return result(current);
        current += 4u;
    }
    if (remaining == 3 || remaining == 2) {
        if (found_at(current)) return result(current);
        current += 4u;
    }
    if (remaining == 3 || remaining == 2 || remaining == 1) {
        if (found_at(current)) return result(current);
    }
    result(last);
}

// func_08AE9CD8(a0 = group, a1, a2 = slot, a3 = key, t0, t1, t2, t3): fills an entry of the 56-byte table (the pointer word at 0x08D40A3C). s2 = 0x08D40AC4 + ((group * 32 + slot) * 16) is a 16-byte header; s1 = t0, forced to 0 when t3 != 0 or a3 < 0. With s1 != 0 the entry index is func_08AE991C(s2, key, s1), else func_08AE9798(s2, key); a negative index returns -1. The header's word +0xC takes entry[0] + 1, or header[0] when header[4] is below that. When the entry has bit 0 of byte +4 set and a function pointer at +0x10 it is first released through that function (entry[0], entry[0x14], the flag "index of a replacement >= 0"; with s1 != 0 the replacement is searched with func_08AE9798(s2, key) and the entry address recomputed from its index when it is not negative). Then byte +4 |= 1, words +0x28 / +0x2C / +0x30 / +0x34 = 0, halfword +6 = key, +8 = s1, +0x18 = group, halfword +0x20 = a1, +0x22 = t3, words +0xC / +0x10 / +0x14 = 0; v0 = entry[0].
// Branches the routes do not take (checked against the asm by hand): s1 != 0 (t0 non-zero with t3 zero and key >= 0: the func_08AE991C search), a negative index (-1), entries with bit 0 set and a release function (the call and the replacement entry).
void func_08AE9CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t group = ctx.gpr[4], second = ctx.gpr[5], slot = ctx.gpr[6], key = ctx.gpr[7];
    const std::uint32_t t0 = ctx.gpr[8], t1 = ctx.gpr[9], t2 = ctx.gpr[10], t3 = ctx.gpr[11];
    const std::uint32_t header = ((group << 5) + slot) * 16u + 0x08D40AC4u;
    std::uint32_t s1 = t0;
    if (t3 != 0u) s1 = 0u;
    if (signed_of(key) < 0) s1 = 0u;
    ctx.gpr[8] = t2;
    ctx.gpr[7] = t1;
    std::uint32_t index;
    if (s1 != 0u) {
        index = call_set(rt, ctx, 0x08AE991Cu, {header, key, s1});
    } else {
        index = call_set(rt, ctx, 0x08AE9798u, {header, key});
    }
    ctx.gpr[5] = index;
    if (signed_of(index) < 0) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const auto entry_of = [&](std::uint32_t number) { return word(rt, 0x08D40A38u + 4u) + (number * 8u - number) * 8u; };
    std::uint32_t entry = entry_of(index);
    const std::uint32_t id = word(rt, entry);
    const std::uint32_t shared = word(rt, header + 4u);
    ctx.gpr[3] = id + 1u;
    put(rt, header + 0xCu, signed_of(shared) < signed_of(id + 1u) ? word(rt, header) : id + 1u);
    std::uint32_t flags = byte_u(rt, entry + 4u);
    std::uint32_t merged = flags | 1u;
    if ((flags & 1u) != 0u) {
        std::uint32_t function = word(rt, entry + 0x10u);
        if (function != 0u) {
            std::int32_t replacement = -1;
            if (s1 != 0u) {
                replacement = signed_of(call_set(rt, ctx, 0x08AE9798u, {header, key}));
                function = word(rt, entry + 0x10u);
            }
            ctx.gpr[2] = function;
            (void)call_set(rt, ctx, function, {word(rt, entry), word(rt, entry + 0x14u), replacement >= 0 ? 1u : 0u});
            if (replacement >= 0) entry = entry_of(static_cast<std::uint32_t>(replacement));
            flags = byte_u(rt, entry + 4u);
            merged = flags | 1u;
        }
    }
    put8(rt, entry + 4u, merged);
    put(rt, entry + 0x28u, 0u);
    put(rt, entry + 0x2Cu, 0u);
    put(rt, entry + 0x30u, 0u);
    put(rt, entry + 0x34u, 0u);
    put16(rt, entry + 6u, key);
    put(rt, entry + 8u, s1);
    put(rt, entry + 0x18u, group);
    put16(rt, entry + 0x20u, second);
    put16(rt, entry + 0x22u, t3);
    put(rt, entry + 0xCu, 0u);
    put(rt, entry + 0x10u, 0u);
    put(rt, entry + 0x14u, 0u);
    ctx.gpr[2] = word(rt, entry);
}

// func_089408D0(owner = a0, record = a1, a2, a3, t0, t1, t2, t3): s3 = owner[8], fp = owner[0], frame word 0xC = owner[0xC]. With t2 == 0: func_08945ECC() (a0 untouched) then func_089582F8(its answer); otherwise func_08945E80(); either way s3[1] = 0x7F. func_0894052C(owner, t1); s3[0] = t0, s3[2] = a3, s3[3] = a2, s3[0xE] = 0 (the delay slot of the next jal), func_08944484(owner), func_08940328(owner, t1, record). Then the two bytes at record + 0x279 + 192 * (record[0x1FF] and record[0x1FF] + 3) and record[0x1F5] are saved, func_0894D050(fp) is asked and, when it answers 2, record[0x1F5] = record[0x1FF] and the two 0x279 bytes take the bytes at +0x283 of their entries; func_089426C4(owner[0xC], record, s3[3] (signed byte), 0, t3 & 0xFF, fp + 0xE0, t2) -> s1; func_08940290(owner, t1); func_0894D050(fp) again and, when it answers 2, the three saved bytes go back; func_08940218(owner, t1, 1). v0 = s1.
// Branches the routes do not take (checked against the asm by hand): t2 != 0 (func_08945E80 instead of func_08945ECC / func_089582F8) and the two restores after func_0894D050 answers 2.
void func_089408D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s0 = ctx.gpr[4], s2 = ctx.gpr[5], s4 = ctx.gpr[6], s5 = ctx.gpr[7];
    const std::uint32_t t0 = ctx.gpr[8], s7 = ctx.gpr[9], s6 = ctx.gpr[10];
    const std::uint32_t flag = ctx.gpr[11] & 0xFFu;
    const std::uint32_t s3 = word(rt, s0 + 8u), fp = word(rt, s0);
    const std::uint32_t owner_c = word(rt, s0 + 0xCu);
    ctx.gpr[11] = flag;
    ctx.gpr[5] = s6;
    if (s6 != 0u) {
        (void)call_keep(rt, ctx, 0x08945E80u, {s0});
    } else {
        const std::uint32_t answer = call_set(rt, ctx, 0x08945ECCu, {s0});
        (void)call_set(rt, ctx, 0x089582F8u, {answer});
    }
    put8(rt, s3 + 1u, 0x7Fu);
    (void)call_set(rt, ctx, 0x0894052Cu, {s0, s7});
    put8(rt, s3, t0);
    put8(rt, s3 + 2u, s5);
    put8(rt, s3 + 3u, s4);
    put8(rt, s3 + 0xEu, 0u);
    (void)call_set(rt, ctx, 0x08944484u, {s0});
    (void)call_set(rt, ctx, 0x08940328u, {s0, s7, s2});
    const std::uint32_t s1_index = byte_u(rt, s2 + 0x1FFu);
    const std::uint32_t saved_a = byte_u(rt, s2 + 0x1F5u);
    const std::uint32_t entry_b = s1_index * 192u + s2;
    const std::uint32_t entry_c = (s1_index + 3u) * 192u + s2;
    const std::uint32_t saved_b = byte_u(rt, entry_b + 0x279u);
    const std::uint32_t saved_c = byte_u(rt, entry_c + 0x279u);
    ctx.gpr[6] = s1_index << 6;
    ctx.gpr[5] = (s1_index + 3u) * 192u;
    const std::uint32_t first = call_keep(rt, ctx, 0x0894D050u, {fp});
    ctx.gpr[4] = owner_c;
    if (first == 2u) {
        put8(rt, s2 + 0x1F5u, s1_index);
        put8(rt, entry_b + 0x279u, byte_u(rt, entry_b + 0x283u));
        put8(rt, entry_c + 0x279u, byte_u(rt, entry_c + 0x283u));
    }
    ctx.gpr[7] = 0u;
    ctx.gpr[8] = flag;
    ctx.gpr[9] = fp + 0xE0u;
    ctx.gpr[10] = s6;
    const std::uint32_t result = call_set(rt, ctx, 0x089426C4u, {owner_c, s2, sext8(byte_u(rt, s3 + 3u)), 0u, flag, fp + 0xE0u, s6});
    (void)call_set(rt, ctx, 0x08940290u, {s0, s7});
    const std::uint32_t second = call_set(rt, ctx, 0x0894D050u, {fp});
    ctx.gpr[4] = s0;
    ctx.gpr[5] = s7;
    ctx.gpr[6] = 1u;
    if (second == 2u) {
        put8(rt, s2 + 0x1F5u, saved_a);
        put8(rt, entry_b + 0x279u, saved_b);
        put8(rt, entry_c + 0x279u, saved_c);
    }
    (void)call_set(rt, ctx, 0x08940218u, {s0, s7, 1u});
    ctx.gpr[2] = result;
}

// func_08B51D60(state = a0): a token loop over the tokenizer func_08B51F38(state[0xC20]). Not state[0x14] == 1: return 1. Each token (a0 = state[0xC20] at its turn) has a flag byte +0xC and a signed byte +4; s2 follows the position (tokenizer's word +0x20 minus one) while the previous round was a token (s0), and a '<' (0x3C) token reports the text between s2 and the position to the callback state[0xC] (state[0x10], s2, length) when the depth counter s3 is positive and s2 differs. The next token then decides: none: return 2; '!' (0x21): func_08B51774 (zero: return 4); '?' (0x3F): func_08B51754 (zero: return 3); any other: func_08B51BF8 (1: s3 + 1, 2: s3 - 1, 0: return 5) and func_08B518F4. v0 = the returned code (also a0); 0 when the tokenizer is exhausted.
// Branches the routes do not take (checked against the asm by hand): state[0x14] != 1 (return 1), the "<" callback report, the no-token returns (2), the "!" and "?" arms (returns 4 and 3) and a func_08B51BF8 answer of 0 (return 5).
void func_08B51D60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = ctx.gpr[4];
    const auto finish = [&](std::uint32_t value) {
        ctx.gpr[4] = value;
        ctx.gpr[2] = value;
    };
    if (word(rt, state + 0x14u) != 1u) return finish(1u);
    std::uint32_t s0 = 1u, s2 = 0u;
    std::int32_t s3 = 0;
    while (true) {
        const std::uint32_t token = call_set(rt, ctx, 0x08B51F38u, {word(rt, state + 0xC20u)});
        ctx.gpr[5] = token;
        if (token == 0u) return finish(0u);
        std::uint32_t a0 = word(rt, state + 0xC20u);
        const std::uint32_t last = word(rt, a0 + 0x20u) - 1u;
        std::uint32_t kind = 0u;
        if (s0 != 0u) s2 = last;
        if (byte_u(rt, token + 0xCu) != 0u) kind = sext8(byte_u(rt, token + 4u));
        s0 = 0u;
        if (kind != 0x3Cu) continue;
        if (s3 > 0 && s2 != last) {
            const std::uint32_t callback = word(rt, state + 0xCu);
            if (callback != 0u) {
                ctx.gpr[2] = callback;
                (void)call_set(rt, ctx, callback, {word(rt, state + 0x10u), s2, last - s2});
                a0 = word(rt, state + 0xC20u);
            }
        }
        const std::uint32_t next = call_set(rt, ctx, 0x08B51F38u, {a0});
        if (next == 0u) return finish(2u);
        const std::uint32_t next_kind = byte_u(rt, next + 0xCu) != 0u ? sext8(byte_u(rt, next + 4u)) : 0u;
        if (next_kind == 0x21u) {
            if (call_set(rt, ctx, 0x08B51774u, {state}) == 0u) return finish(4u);
        } else if (next_kind == 0x3Fu) {
            if (call_set(rt, ctx, 0x08B51754u, {state}) == 0u) return finish(3u);
        } else {
            const std::uint32_t code = call_set(rt, ctx, 0x08B51BF8u, {state});
            if (code == 1u) {
                ++s3;
            } else if (code == 2u) {
                --s3;
            } else if (code == 0u) {
                return finish(5u);
            }
            (void)call_set(rt, ctx, 0x08B518F4u, {state});
        }
        s0 = 1u;
    }
}

// func_08A03FFC(a0, key = a1, count = a2, items = a3): four floats start at 1.0; for each of the first `count` (up to four) pointers items[i], virtual slot 0x54 of that object (a0 = it) answers the float (a null pointer first reports through func_08A5BB7C(0x08CA8D38) and then reads through it anyway, as the original). func_08A03E58(sp, key) fills a pointer s0 at sp + 0 (null: func_08A5BB7C(0x08CA8D40)); the four floats go to the frame at sp + 0x10, -1 at sp + 0x20, func_089641C4(s0 + 0x20, sp + 0x10) copies them (skipped when the two addresses are equal), s0[0x10] = the word at sp + 0x20, func_08A5B9EC(sp) ends it. v0 = its answer.
// Branches the routes do not take (checked against the asm by hand): a null item pointer (the func_08A5BB7C reports), a null frame pointer and the equal-address case of the copy.
void func_08A03FFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60);
    const std::uint32_t key = ctx.gpr[5], count = ctx.gpr[6], items = ctx.gpr[7];
    float values[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    for (std::uint32_t i = 0u; i < 4u; ++i) {
        if (signed_of(count) < static_cast<std::int32_t>(i + 1u)) continue;
        const std::uint32_t object = word(rt, items + 4u * i);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8D38u});
        values[i] = call_virtual_full(rt, ctx, object, 0x54u, {object}).f0;
    }
    ctx.gpr[5] = key;
    (void)call_set(rt, ctx, 0x08A03E58u, {frame.at(0u), key});
    std::uint32_t target = word(rt, frame.at(0u));
    if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8D40u});
    ctx.gpr[2] = 0xFFFFFFFFu;
    target += 0x20u;
    put_float(rt, frame.at(0x10u), values[0]);
    put_float(rt, frame.at(0x14u), values[1]);
    put_float(rt, frame.at(0x18u), values[2]);
    put_float(rt, frame.at(0x1Cu), values[3]);
    put(rt, frame.at(0x20u), 0xFFFFFFFFu);
    if (target != frame.at(0x10u)) {  // beq s0, a1 at 0x08A04168 skips the call and the store both
        (void)call_set(rt, ctx, 0x089641C4u, {target, frame.at(0x10u)});
        put(rt, target + 0x10u, word(rt, frame.at(0x20u)));
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08ADC4E0(owner = a0, key = a1, out1 = a2, out2 = a3, out3 = t0): looks a name up in a sorted table. An empty key (null or a zero first byte) returns 0 at once. func_08ADC1CC(owner, key) must answer non-zero and func_08ADB278(owner, 0x08C90EEC) must give a block s3 whose table pointer s3[0x3C] is not null; then out1 = -1 and out2 / out3 = 0 (each only when not null) and a binary search over s3[0x40] entries of 8 bytes (the name for entry i is the string at s3[0x14] + the word at entry(i + 1) + 4, compared with func_08B35FD8) finds the key; on a hit the entry's type nibble (the top four bits of the word at +8 of the entry, else of the entry itself when that nibble is 0 and the neighbouring entries carry the same name, or of the entry two further on) picks the answer: types 3-6 store 0 / 0x10 / 0x11 / 0x12 in out1 and the halfword at +0 in out2; types 1 and 2 store the type in out1, the low 12 bits of the signed halfword at +2 in out2 and the halfword at +0 in out3; the flag word becomes 1. func_08ADC230(owner) always runs after the lookup; v0 = the flag.
// Branches the routes do not take (checked against the asm by hand): everything after a hit: the neighbour-entry checks, the type nibble switch (types 1-6 with their out1 / out2 / out3 stores), the empty-key return and the failure returns before the search.
void func_08ADC4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[4], key = ctx.gpr[5], out1 = ctx.gpr[6], out2 = ctx.gpr[7], out3 = ctx.gpr[8];
    std::uint32_t flag = 0u;
    if (key == 0u || byte_u(rt, key) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const auto finish = [&]() {
        (void)call_set(rt, ctx, 0x08ADC230u, {owner});
        ctx.gpr[2] = flag;
    };
    if (call_set(rt, ctx, 0x08ADC1CCu, {owner, key}) == 0u) return finish();
    const std::uint32_t block = call_set(rt, ctx, 0x08ADB278u, {owner, 0x08C90EECu});
    if (block == 0u) return finish();
    if (word(rt, block + 0x3Cu) == 0u) return finish();
    if (out1 != 0u) put(rt, out1, 0xFFFFFFFFu);
    if (out2 != 0u) put(rt, out2, 0u);
    if (out3 != 0u) put(rt, out3, 0u);
    std::int32_t high = signed_of(word(rt, block + 0x40u)) - 1;
    std::int32_t low = 0;
    if (high < 0) return finish();
    const auto compare = [&](std::uint32_t entry) { return signed_of(call_set(rt, ctx, 0x08B35FD8u, {word(rt, block + 0x14u) + word(rt, entry + 4u), key})); };
    std::uint32_t middle = 0u, scaled = 0u;
    while (true) {
        middle = static_cast<std::uint32_t>(low) + static_cast<std::uint32_t>((high - low) >> 1);
        scaled = middle << 3;
        const std::uint32_t entry = word(rt, block + 0x3Cu) + scaled;
        const std::int32_t order = compare(entry + 8u);
        if (order > 0) {
            high = static_cast<std::int32_t>(middle) - 1;
        } else {
            low = static_cast<std::int32_t>(middle) + 1;
            if (order == 0) break;
        }
        if (high < low) return finish();
    }
    const std::uint32_t entry = word(rt, block + 0x3Cu) + scaled;
    std::uint32_t cursor = entry + 8u;
    std::uint32_t kind = word(rt, cursor) >> 28;
    if (kind == 0u) {
        bool chosen = false;
        if (signed_of(middle) > 0) {
            cursor = word(rt, block + 0x3Cu) + scaled;
            if (compare(cursor) == 0) {
                kind = word(rt, cursor) >> 28;
                chosen = true;
            }
        }
        if (!chosen) {
            if (!(signed_of(middle) < signed_of(word(rt, block + 0x40u)) - 1)) return finish();
            cursor = word(rt, block + 0x3Cu) + scaled + 0x10u;
            if (compare(cursor) != 0) return finish();
            kind = word(rt, cursor) >> 28;
        }
    }
    if (kind >= 7u) return finish();
    switch (kind) {
    case 1u:
    case 2u:
        if (out1 != 0u) put(rt, out1, kind);
        if (out2 != 0u) put(rt, out2, sext16(half_u(rt, cursor + 2u)) & 0xFFFu);
        if (out3 != 0u) put(rt, out3, half_u(rt, cursor));
        flag = 1u;
        break;
    case 3u:
    case 4u:
    case 5u:
    case 6u:
        if (out1 != 0u) put(rt, out1, kind == 3u ? 0u : 0xCu + kind);
        if (out2 != 0u) put(rt, out2, half_u(rt, cursor));
        flag = 1u;
        break;
    default:
        break;
    }
    finish();
}

// func_0890673C(holder = a0, source = a1): holder[0] is a block h, source[0] a table whose pointer at +0x48 is t: func_08BD3400(h + 0x1C, t[4]); func_089099B8(h + 0x28, t[0x28]); h[0x3C] = 3, h[0x44] = the largest float, (h + 0x1C)[0x24] |= 0x20000; func_08BD31D4(h + 0x48, t[4]) and (h + 0x48)[0x14] |= 0x20000; func_08BD31D4(h + 0x64, t[0xC]); func_08BD31D4(h + 0x80, t[8]); func_08BD3400(h + 0x9C, t[0x10]); func_089099B8(h + 0xA8, t[0x2C]); func_08BD31D4(h + 0xC8, t[0x14]); func_08BD3400(h + 0xE4, t[4]); func_089099B8(h + 0xF0, func_08BD2D68(t[0x34], t[0x28])); func_08BD3400(h + 0x110, t[0x10]); func_089099B8(h + 0x11C, func_08BD2D68(t[0x3C], t[0x2C])); then h[0x14] = source[0] and the counted pointer h[0x18] takes source[4]. Every object pointer is re-read before each call, as the original.
void func_0890673C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[4], source = ctx.gpr[5];
    const auto table = [&]() { return word(rt, word(rt, source) + 0x48u); };
    const auto block = [&]() { return word(rt, holder); };
    constexpr std::uint32_t kFlag = 0x20000u;
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x08BD3400u, {block() + 0x1Cu, word(rt, t + 4u)});
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x089099B8u, {block() + 0x28u, word(rt, t + 0x28u)});
    }
    put(rt, block() + 0x3Cu, 3u);
    put(rt, block() + 0x44u, 0x7F7FFFFFu);
    {
        const std::uint32_t base = block() + 0x1Cu;
        put(rt, base + 0x24u, word(rt, base + 0x24u) | kFlag);
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x08BD31D4u, {block() + 0x48u, word(rt, t + 4u)});
    }
    {
        const std::uint32_t base = block() + 0x48u;
        put(rt, base + 0x14u, word(rt, base + 0x14u) | kFlag);
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x08BD31D4u, {block() + 0x64u, word(rt, t + 0xCu)});
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x08BD31D4u, {block() + 0x80u, word(rt, t + 8u)});
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x08BD3400u, {block() + 0x9Cu, word(rt, t + 0x10u)});
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x089099B8u, {block() + 0xA8u, word(rt, t + 0x2Cu)});
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x08BD31D4u, {block() + 0xC8u, word(rt, t + 0x14u)});
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x08BD3400u, {block() + 0xE4u, word(rt, t + 4u)});
    }
    {
        const std::uint32_t t = table();
        const std::uint32_t target = block() + 0xF0u;
        const std::uint32_t value = call_set(rt, ctx, 0x08BD2D68u, {word(rt, t + 0x34u), word(rt, t + 0x28u)});
        (void)call_set(rt, ctx, 0x089099B8u, {target, value});
    }
    {
        const std::uint32_t t = table();
        (void)call_set(rt, ctx, 0x08BD3400u, {block() + 0x110u, word(rt, t + 0x10u)});
    }
    {
        const std::uint32_t t = table();
        const std::uint32_t target = block() + 0x11Cu;
        const std::uint32_t value = call_set(rt, ctx, 0x08BD2D68u, {word(rt, t + 0x3Cu), word(rt, t + 0x2Cu)});
        (void)call_set(rt, ctx, 0x089099B8u, {target, value});
    }
    const std::uint32_t base = block();
    const std::uint32_t previous = word(rt, base + 0x18u);
    const std::uint32_t object = word(rt, source);
    ctx.gpr[2] = base;
    ctx.gpr[3] = object;
    ctx.gpr[4] = previous;
    put(rt, base + 0x14u, object);
    assign_counted(rt, ctx, base + 0x18u, word(rt, source + 4u), previous);
}

// func_08951304(a0, a1, a2 = out, a3): a frame of 0x240 bytes holds two scratch objects (sp + 0 initialised by func_08958324, sp + 0x20 and sp + 0x110): func_08958324(sp); func_0894C460(sp + 0x110, a0, 0, 0); func_0895077C(a0, sp + 0x20, sp + 0x110, 0); func_0894CA04(sp + 0x110, a2, a3 or sp when a3 is zero); func_089511BC(sp + 0x20, sp + 0x110, a1); then the float at *a2 gets func_08947A30(sp + 0x20) added (f0) and the sum is stored back; v0 = the sum truncated toward zero.
// Branches the routes do not take (checked against the asm by hand): a3 != 0 (the caller's own pointer instead of the frame).
void func_08951304(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x240);
    const std::uint32_t s4 = ctx.gpr[4], s5 = ctx.gpr[5], s3 = ctx.gpr[6];
    const std::uint32_t s0 = ctx.gpr[7] != 0u ? ctx.gpr[7] : frame.at(0u);
    const std::uint32_t s2 = frame.at(0x20u), s1 = frame.at(0x110u);
    (void)call_set(rt, ctx, 0x08958324u, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x0894C460u, {s1, s4, 0u, 0u});
    (void)call_set(rt, ctx, 0x0895077Cu, {s4, s2, s1, 0u});
    (void)call_set(rt, ctx, 0x0894CA04u, {s1, s3, s0});
    (void)call_set(rt, ctx, 0x089511BCu, {s2, s1, s5});
    float sum = float_at(rt, s3);
    const GuestResult added = call_setf(rt, ctx, 0x08947A30u, {s2}, {});
    sum = sum + added.f0;
    put_float(rt, s3, sum);
    ctx.gpr[2] = ctx.fpu_float_to_word(sum, 1u);
}

// func_089594D8(): converts byte tables of the static block at 0x08CDE1xx into float tables around 0x08CF35xx. For i below 6: dst[0x08CF3588 + 4i] = (float)byte[0x08CDE218 + i + 8], dst[0x08CF3570 + 4i] = (float)(byte[0x08CDE218 + i] * 10). Then with b = byte[0x08CDE1FC]: word 0x08CF3550 = 6 * b; float 0x08CF354C = (100 - byte[0x08CDE1FD]) / (float)(600 * b) when 6 * b > 0, else 0. The remaining bytes of the block at 0x08CDE1F4 become floats: +0xB / 100 -> 0x08CDE22C; signed +0x1C / +0x1D / +0x1E -> 0x08CF3548 / 0x08CF3544 / 0x08CF3540 (unscaled); +0x10 -> 0x08CF3568, +0x11 -> 0x08CF355C, +0x20 -> 0x08CF3558, +0x21 -> 0x08CF3554, +0x22 -> 0x08CF3560, +0x23 -> 0x08CF356C and signed +0x1F -> 0x08CF3564, each times 0.1f. v0 = the lui of the last store's address pair.
// Branches the routes do not take (checked against the asm by hand): a zero count byte at 0x08CDE1FC (six <= 0: no ratio).
void func_089594D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto as_float = [](std::uint32_t integer) { return static_cast<float>(static_cast<std::int32_t>(integer)); };
    for (std::uint32_t i = 0u; i < 6u; ++i) {
        const std::uint32_t low = byte_u(rt, 0x08CDE218u + i), high = byte_u(rt, 0x08CDE218u + i + 8u);
        put_float(rt, 0x08CF3588u + 4u * i, as_float(high));
        put_float(rt, 0x08CF3570u + 4u * i, as_float(low * 10u));
    }
    const std::uint32_t base = 0x08CDE1F4u;
    const std::uint32_t count = byte_u(rt, base + 8u);
    const std::uint32_t six = count * 8u - count * 2u;
    put(rt, 0x08CF3550u, six);
    put(rt, 0x08CF354Cu, 0u);
    if (signed_of(six) > 0) {
        const std::uint32_t scaled = count * 600u;
        const std::uint32_t remaining = 100u - byte_u(rt, base + 9u);
        put_float(rt, 0x08CF354Cu, as_float(remaining) / as_float(scaled));
    }
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);
    const float hundred = 100.0f;
    const float f5 = as_float(sext8(byte_u(rt, base + 0xBu))) / hundred;
    const float f11 = as_float(sext8(byte_u(rt, base + 0x1Fu)));
    const float f4 = as_float(byte_u(rt, base + 0x10u));
    const float f6 = as_float(byte_u(rt, base + 0x11u));
    const float f9 = as_float(byte_u(rt, base + 0x20u));
    const float f7 = as_float(byte_u(rt, base + 0x21u));
    const float f8 = as_float(byte_u(rt, base + 0x22u));
    const float f10 = as_float(byte_u(rt, base + 0x23u));
    const float f12 = as_float(sext8(byte_u(rt, base + 0x1Cu)));
    const float f13 = as_float(sext8(byte_u(rt, base + 0x1Du)));
    const float f1 = as_float(sext8(byte_u(rt, base + 0x1Eu)));
    put_float(rt, 0x08CDE22Cu, f5);
    put_float(rt, 0x08CF3548u, f12);
    put_float(rt, 0x08CF3544u, f13);
    put_float(rt, 0x08CF3540u, f1);
    put_float(rt, 0x08CF3568u, f4 * tenth);
    put_float(rt, 0x08CF355Cu, f6 * tenth);
    put_float(rt, 0x08CF3558u, f9 * tenth);
    put_float(rt, 0x08CF3554u, f7 * tenth);
    put_float(rt, 0x08CF3560u, f8 * tenth);
    put_float(rt, 0x08CF356Cu, f10 * tenth);
    put_float(rt, 0x08CF3564u, f11 * tenth);
    ctx.gpr[2] = lui_of(0x08CF3564u);
}

// func_0895871C(): the same kind of byte-to-float conversion for the block at 0x08CDE1D8: float 0x08CF34D0 = byte[0] * 0.1f; with n = byte[3] > 0: for i below n, float 0x08CF34D4 + 4i = (float)byte[0x08CDE1DC + i] * the float 0x3E8E38E4 and float 0x08CF34F4 + 4i = (float)byte[0x08CDE1E4 + i] / (float)byte[1]; then func_08946734(0x08CF3514, n, 0x08CF34D4, 0x08CF34F4, 0, 0) and the floats at 0x08CF34D0 + 0x50..0x6C come from the bytes at 0x08CDE1D8 + 0x14.. (signed 0x15 / 0x19 divided by 100.0f; +0x14 and +0x16 times 0x3E8E38E4; +0x17, +0x18, +0x1A and +0x1B times the float 0x3AE4C388). v0 = 0x08CF34D0.
// Branches the routes do not take (checked against the asm by hand): a zero count byte at 0x08CDE1DB (no table loop).
void func_0895871C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto as_float = [](std::uint32_t integer) { return static_cast<float>(static_cast<std::int32_t>(integer)); };
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);
    const float ninth = std::bit_cast<float>(0x3E8E38E4u);
    const float small = std::bit_cast<float>(0x3AE4C388u);
    const std::uint32_t base = 0x08CDE1D8u;
    put_float(rt, 0x08CF34D0u, as_float(byte_u(rt, base)) * tenth);
    const std::uint32_t count = byte_u(rt, base + 3u);
    const float divisor = as_float(byte_u(rt, base + 1u));
    if (signed_of(count) > 0) {
        for (std::uint32_t i = 0u; i < count; ++i) {
            const std::uint32_t a = byte_u(rt, 0x08CDE1E4u + i), b = byte_u(rt, 0x08CDE1E4u + i - 8u);
            put_float(rt, 0x08CF34D4u + 4u * i, as_float(b) * ninth);
            put_float(rt, 0x08CF34F4u + 4u * i, as_float(a) / divisor);
        }
    }
    ctx.gpr[5] = count;
    (void)call_set(rt, ctx, 0x08946734u, {0x08CF3514u, count, 0x08CF34D4u, 0x08CF34F4u, 0u, 0u});
    const float f9 = as_float(sext8(byte_u(rt, base + 0x15u))) / 100.0f;
    const float f10 = as_float(sext8(byte_u(rt, base + 0x19u))) / 100.0f;
    const float f3 = as_float(byte_u(rt, base + 0x14u));
    const float f4 = as_float(byte_u(rt, base + 0x18u));
    const float f5 = as_float(byte_u(rt, base + 0x16u));
    const float f6 = as_float(byte_u(rt, base + 0x1Au));
    const float f7 = as_float(byte_u(rt, base + 0x17u));
    const float f2 = as_float(byte_u(rt, base + 0x1Bu));
    const std::uint32_t out = 0x08CF34D0u;
    put_float(rt, out + 0x6Cu, f2 * small);
    put_float(rt, out + 0x50u, f3 * ninth);
    put_float(rt, out + 0x54u, f4 * ninth);
    put_float(rt, out + 0x58u, f9);
    put_float(rt, out + 0x5Cu, f10);
    put_float(rt, out + 0x60u, f5 * small);
    put_float(rt, out + 0x64u, f6 * small);
    put_float(rt, out + 0x68u, f7 * small);
    ctx.gpr[2] = out;
}

// func_0895EEF8(object = a0, record = a1, a2, a3): fills a float block at object from the bytes / halfwords of `record`: object[4] = byte1 * 0.01f * (pi/180)f(0x3C8EFA35), [8] = byte2 * 100.0f, [0xC] = (float)byte3, [0x1C] = 0, [0x14] = (float)half4 * 1000.0f, [0x20] = (float)a3, [0x18] = (float)half6 * 1000.0f; func_08946734(object + 0x24, 2, object + 0x14, object + 0x1C, 0, 0); [0x10] = 1.0f / (float)a3, [0x30] = 0, [0x34] = (float)(a3 + a2), [0x38] = byte8 / 100.0f, [0x3C] = byte9 / 100.0f; func_08946734(object + 0x40, 2, object + 0x30, object + 0x38, 0, 0); [0x4C] = byteA * (pi/180)f / 100.0f, [0x50] = byteB * (pi/180)f / 100.0f and the tail jump func_08946734(object + 0x54, 2, object + 0x30, object + 0x4C, 0, 0).
void func_0895EEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto as_float = [](std::uint32_t integer) { return static_cast<float>(static_cast<std::int32_t>(integer)); };
    const std::uint32_t object = ctx.gpr[4], record = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    const float thousand = std::bit_cast<float>(0x447A0000u), degrees = std::bit_cast<float>(0x3C8EFA35u), hundred = 100.0f;
    const float f22 = as_float(a3);
    ctx.gpr[8] = 0u;
    ctx.gpr[9] = 0u;
    put_float(rt, object + 4u, psp_mul(psp_mul(as_float(byte_u(rt, record + 1u)), std::bit_cast<float>(0x3C23D70Au)), degrees));
    put_float(rt, object + 8u, psp_mul(as_float(byte_u(rt, record + 2u)), hundred));
    put_float(rt, object + 0xCu, as_float(byte_u(rt, record + 3u)));
    put(rt, object + 0x1Cu, 0u);
    put_float(rt, object + 0x14u, psp_mul(as_float(sext16(half_u(rt, record + 4u))), thousand));
    put_float(rt, object + 0x20u, f22);
    put_float(rt, object + 0x18u, psp_mul(as_float(sext16(half_u(rt, record + 6u))), thousand));
    (void)call_set(rt, ctx, 0x08946734u, {object + 0x24u, 2u, object + 0x14u, object + 0x1Cu, 0u, 0u});
    put(rt, object + 0x30u, 0u);
    put_float(rt, object + 0x34u, as_float(a3 + a2));
    put_float(rt, object + 0x10u, 1.0f / f22);
    put_float(rt, object + 0x38u, as_float(byte_u(rt, record + 8u)) / hundred);
    put_float(rt, object + 0x3Cu, as_float(byte_u(rt, record + 9u)) / hundred);
    (void)call_set(rt, ctx, 0x08946734u, {object + 0x40u, 2u, object + 0x30u, object + 0x38u, 0u, 0u});
    put_float(rt, object + 0x4Cu, psp_mul(as_float(byte_u(rt, record + 0xAu)), degrees) / hundred);
    put_float(rt, object + 0x50u, psp_mul(as_float(byte_u(rt, record + 0xBu)), degrees) / hundred);
    tail_call(ctx, 0x08946734u, {object + 0x54u, 2u, object + 0x30u, object + 0x4Cu, 0u, 0u});
}

// func_089572C4(object = a0): s0 = object[8] (a state block), s2 = object[0xC] (a table block), s3 = object[0] + 0x37C. func_0895549C(object); dot = s0[0x6CC] * s0[0x34] + s0[0x6D0] * s0[0x38] + s0[0x6D4] * s0[0x3C], x = s0[0x6C4]; s0[0x680] = s0[0x67C]; f22 = 1 - s0[0x67C]; f21 = func_08B4A430(dot, x) (float arguments f12 / f13); f20 = x * x * (s3[4] + (1 - s3[4]) * f22) * s2[0x1400], times func_08954AB8(object) when byte s2[0x52] is 8, negated when x >= 0 -> s0[0x684]. g = s3[8] + f22 * (1 - s3[8]); with f21 made positive: above 0.17435 the factor is 0, between 0.0869 and 0.17435 the linear ramp (0.17435 - f21) / 0.0869, else 1; s0[0x688 + 4i] = g * x * x * factor * s2[0x1404 + 4i] for i = 0, 1; tail jump func_089582AC(object).
// Branches the routes do not take (checked against the asm by hand): byte s2[0x52] == 8 (the extra factor from func_08954AB8) and a negative x (the negation was applied in every compared case).
void func_089572C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    const std::uint32_t s0 = word(rt, object + 8u), s2 = word(rt, object + 0xCu), s3 = word(rt, object) + 0x37Cu;
    const float one = 1.0f;
    (void)call_set(rt, ctx, 0x0895549Cu, {object});
    float f12 = psp_mul(float_at(rt, s0 + 0x6CCu), float_at(rt, s0 + 0x34u));
    float f1 = psp_mul(float_at(rt, s0 + 0x6D0u), float_at(rt, s0 + 0x38u));
    float f2 = float_at(rt, s0 + 0x6D4u);
    const float f4 = float_at(rt, s0 + 0x3Cu);
    const float f13 = float_at(rt, s0 + 0x6C4u);
    const float f3 = float_at(rt, s0 + 0x67Cu);
    f12 = f12 + f1;
    f2 = psp_mul(f2, f4);
    put_float(rt, s0 + 0x680u, f3);
    const float f24 = psp_mul(f13, f13);
    f12 = f12 + f2;
    const float f22 = one - f3;
    float f21 = call_setf(rt, ctx, 0x08B4A430u, {}, {f12, f13}).f0;
    f1 = float_at(rt, s3 + 4u);
    f2 = float_at(rt, s2 + 0x1400u);
    float f0 = one - f1;
    const std::uint32_t kind = byte_u(rt, s2 + 0x52u);
    f0 = psp_mul(f0, f22);
    f1 = f1 + f0;
    f1 = psp_mul(f24, f1);
    float f20 = psp_mul(f1, f2);
    if (kind == 8u) {
        f0 = call_setf(rt, ctx, 0x08954AB8u, {object}, {}).f0;
        f20 = psp_mul(f20, f0);
    }
    f0 = float_at(rt, s0 + 0x6C4u);
    f2 = 0.0f;
    if (f2 <= f0) f20 = -f20;
    put_float(rt, s0 + 0x684u, f20);
    const bool negative = f21 < f2;
    f1 = float_at(rt, s3 + 8u);
    f0 = one - f1;
    f0 = psp_mul(f22, f0);
    f1 = f1 + f0;
    if (negative) f21 = -f21;
    f0 = std::bit_cast<float>(0x3E32B8C2u);
    if (f0 < f21) {
        f1 = f2;
    } else {
        f2 = std::bit_cast<float>(0x3DB2B8C2u);
        if (f2 < f21) {
            f0 = f0 - f21;
            f0 = f0 / f2;
            f1 = psp_mul(f1, f0);
        }
    }
    f12 = psp_mul(f1, f24);
    for (std::uint32_t i = 0u; i < 2u; ++i) put_float(rt, s0 + 0x688u + 4u * i, psp_mul(f12, float_at(rt, s2 + 0x1404u + 4u * i)));
    ctx.gpr[4] = object;
    tail_call(ctx, 0x089582ACu, {object});
}

// func_08BDABCC(first = a0, last = a1, depth = a2): the introsort loop for 8-byte elements whose first word points at a string (keys compared with func_08B35FD8). Fewer than 0x88 bytes: return 1. Depth zero, or the iteration count reaching the depth after a round: the tail jump func_08BDAA80(first, last, last, a3) (v0 = 0); a3 is the caller's s0 when the depth is zero at entry and the last split (s0) otherwise. Each round the pivot (median of the first, middle ((count / 2) * 8 bytes in) and last element by key, the same order of tests as the float version) is copied to the frame at sp + 0..7 and func_08BDA6D8(first, last, &pivot, last key) splits at s0; func_08BDABCC(s0, last, depth - 1, s0) sorts the upper part and the loop goes on with last = s0.
// Branches the routes do not take (checked against the asm by hand): the whole loop: every compared case was under 0x88 bytes, so the strcmp-based pivot choice, the split calls, the recursion and both tail jumps (a3 = the caller's s0 when the depth is zero at entry) were read from the asm only.
void func_08BDABCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50);
    const std::uint32_t first = ctx.gpr[4], limit = ctx.gpr[6];
    std::uint32_t last = ctx.gpr[5];
    std::uint32_t bytes = last - first;
    ctx.gpr[3] = bytes;
    if (signed_of(bytes) < 0x88) {
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[2] = 0u;
    if (limit == 0u) return tail_call(ctx, 0x08BDAA80u, {first, last, last, ctx.gpr[16]});
    std::uint32_t depth = limit, iteration = 0u;
    put(rt, frame.at(0x14u), 0u);
    while (true) {
        const std::int32_t count = signed_of(bytes) >> 3;
        const std::uint32_t half = static_cast<std::uint32_t>(signed_of(static_cast<std::uint32_t>(count) + (static_cast<std::uint32_t>(count) >> 31)) >> 1);
        const std::uint32_t middle = first + (half << 3);
        const std::uint32_t end = last - 8u;
        const std::uint32_t first_key = word(rt, first), middle_key = word(rt, middle);
        put(rt, frame.at(0x10u), end);
        const auto compare = [&](std::uint32_t a, std::uint32_t b) { return signed_of(call_set(rt, ctx, 0x08B35FD8u, {a, b})); };
        const std::int32_t first_middle = compare(first_key, middle_key);
        --depth;
        std::uint32_t pivot = first;
        std::uint32_t last_key;
        if (first_middle < 0) {
            last_key = word(rt, last - 8u);
            if (compare(middle_key, last_key) < 0) {
                pivot = middle;
            } else if (compare(first_key, last_key) >= 0) {
                pivot = first;
            } else {
                pivot = word(rt, frame.at(0x10u));
            }
        } else {
            last_key = word(rt, last - 8u);
            if (compare(first_key, last_key) < 0) {
                pivot = first;
            } else if (compare(middle_key, last_key) < 0) {
                pivot = word(rt, frame.at(0x10u));
            } else {
                pivot = middle;
            }
        }
        put(rt, frame.at(4u), word(rt, pivot + 4u));
        put(rt, frame.at(0u), word(rt, pivot));
        ++iteration;
        put(rt, frame.at(0x14u), iteration);
        const std::uint32_t split = call_set(rt, ctx, 0x08BDA6D8u, {first, last, frame.at(0u), last_key});
        (void)call_set(rt, ctx, 0x08BDABCCu, {split, last, depth, split});
        bytes = split - first;
        last = split;
        const bool small = signed_of(bytes) < 0x88;
        ctx.gpr[2] = small ? 1u : 0u;
        ctx.gpr[3] = bytes;
        if (small) return;
        if (word(rt, frame.at(0x14u)) == limit) return tail_call(ctx, 0x08BDAA80u, {first, last, last, split});
    }
}

// func_088A0DD4(index = a0, flag = a1, value = a2, a3, t0, t1): picks a string by `index` (0-10; anything else gives a2 = 0) and tail-jumps to func_08B3767C(a3, t0, string, value) (t1 in t0's slot... the original moves a3 = t1 and a2 = the string pointer, a0 = a3, a1 = t0, a3 = value). Indices 8 and 9 also load t0 = 0x72 when `flag` is non-zero and 0x66 when it is zero. The jump table is at 0x08C89CB0 (entries 0..10 -> 0x08CA08DC, E8, F8, 0908, 091C, 092C, 0938, 0948, 0958, 096C, 08CC). Nothing is stored.
// Branches the routes do not take (checked against the asm by hand): indices 8 and 9 (t0 = 0x72 / 0x66 by `flag`) and indices of 11 and above.
void func_088A0DD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)rt;
    static constexpr std::uint32_t kStrings[11] = {0x08CA08DCu, 0x08CA08E8u, 0x08CA08F8u, 0x08CA0908u, 0x08CA091Cu, 0x08CA092Cu,
                                                    0x08CA0938u, 0x08CA0948u, 0x08CA0958u, 0x08CA096Cu, 0x08CA08CCu};
    const std::uint32_t index = ctx.gpr[4], flag = ctx.gpr[5], value = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8], t1 = ctx.gpr[9];
    std::uint32_t text = 0u, fifth = t0;
    ctx.gpr[9] = value;
    if (index < 11u) {
        text = kStrings[index];
        ctx.gpr[2] = lui_of(text);
        if (index == 8u || index == 9u) {
            ctx.gpr[3] = flag != 0u ? 0x72u : 0x66u;
            fifth = ctx.gpr[3];
        }
    }
    (void)t1;
    ctx.gpr[6] = text;
    tail_call(ctx, 0x08B3767Cu, {a3, t0, text, value, fifth});
}

// func_088B4F3C(object = a0, message = a1): an update of a state block from a message. object words +4 .. +0x30 (at +4, +8, +0xC, +0x10, +0x18, +0x1C, +0x20, +0x28, +0x2C, +0x30) must all be non-zero or v0 = 0; message[4] (0-7, else 0) then picks: 1: byte s0[0x94] = message byte 8; 2: bit 2 of half s0[2] = (message byte 8 != 0); when object[0xC][3] (signed byte) is 2 the bit 0 follows func_088DF13C(object[0x10], object[0x14])[0x18] (cleared when the message byte is zero) - always v0 = 0; 3: when object[0xC][3] is 2, s0[8] = -(message[0xC] == 0 && message[8] <= 0x36EE80 ? message[8] * 3 : 0); v0 = 1; 4: byte s0[0x97] |= message byte 8; 5: bit 4 of s0[2] set, and bit 5 + byte s0[0xA6] = message[8] when it is not negative (bit 5 cleared otherwise); 6: byte s0[0xA7] = min(message[8], 0x50) or -1; 7: bit 6 set and bit 7 follows message byte 8. v0 = 1 for the cases that store, 0 else; s0 = object[0x28].
// Branches the routes do not take (checked against the asm by hand): a null state field, a kind of 8 or above, kind 0, kinds 3-7 and kind 2 with a non-zero byte (the func_088DF13C lookup).
void func_088B4F3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], message = ctx.gpr[5];
    const auto none = [&]() { ctx.gpr[6] = 0u; ctx.gpr[2] = 0u; };
    static constexpr std::uint32_t kOffsets[] = {4u, 8u, 0xCu, 0x10u, 0x18u, 0x1Cu, 0x20u, 0x28u, 0x2Cu, 0x30u};
    std::uint32_t s0 = 0u;
    for (const std::uint32_t offset : kOffsets) {
        const std::uint32_t pointer = word(rt, object + offset);
        if (offset == 0x28u) s0 = pointer;
        if (pointer == 0u) return none();
    }
    const std::uint32_t kind = word(rt, message + 4u);
    if (kind >= 8u) return none();
    ctx.gpr[6] = object + 4u;
    ctx.gpr[4] = object + 8u;
    const auto done = [&](std::uint32_t result) { ctx.gpr[6] = result; ctx.gpr[2] = result; };
    switch (kind) {
    case 0u:
        return none();
    case 1u:
        put8(rt, s0 + 0x94u, byte_u(rt, message + 8u));
        return done(1u);
    case 2u: {
        const std::uint32_t enable = byte_u(rt, message + 8u);
        std::uint32_t bits = half_u(rt, s0 + 2u);
        bits = enable != 0u ? (bits | 4u) : (bits & ~4u);
        put16(rt, s0 + 2u, bits);
        if (sext8(byte_u(rt, word(rt, object + 0xCu) + 3u)) != 2u) return none();
        if (enable == 0u) {
            put16(rt, s0 + 2u, bits & ~1u);
            return none();
        }
        const std::uint32_t found = call_set(rt, ctx, 0x088DF13Cu, {word(rt, object + 0x10u), word(rt, object + 0x14u)});
        if (word(rt, found + 0x18u) != 0u) return none();
        put16(rt, s0 + 2u, half_u(rt, s0 + 2u) | 1u);
        return none();
    }
    case 3u: {
        const std::uint32_t amount = word(rt, message + 8u), high = word(rt, message + 0xCu);
        if (sext8(byte_u(rt, word(rt, object + 0xCu) + 3u)) != 2u) return done(1u);
        std::uint32_t result = 0u;
        if (high == 0u && !(amount > 0x36EE80u)) result = amount * 3u;
        put(rt, s0 + 8u, 0u - result);
        return done(1u);
    }
    case 4u:
        put8(rt, s0 + 0x97u, byte_u(rt, message + 8u) | byte_u(rt, s0 + 0x97u));
        return done(1u);
    case 5u: {
        std::uint32_t bits = half_u(rt, s0 + 2u) | 0x10u;
        const std::uint32_t amount = word(rt, message + 8u);
        put16(rt, s0 + 2u, bits);
        if (signed_of(amount) < 0) {
            put16(rt, s0 + 2u, bits & ~0x20u);
        } else {
            put16(rt, s0 + 2u, bits | 0x20u);
            put8(rt, s0 + 0xA6u, amount);
        }
        return done(1u);
    }
    case 6u: {
        const std::int32_t amount = signed_of(word(rt, message + 8u));
        put8(rt, s0 + 0xA7u, amount < 0 ? 0xFFu : static_cast<std::uint32_t>(std::min(amount, 0x50)));
        return done(1u);
    }
    default: {
        std::uint32_t bits = half_u(rt, s0 + 2u) | 0x40u;
        const std::uint32_t enable = byte_u(rt, message + 8u);
        put16(rt, s0 + 2u, bits);
        put16(rt, s0 + 2u, enable != 0u ? (bits | 0x80u) : (bits & ~0x80u));
        return done(1u);
    }
    }
}

// func_088D1BD0(object = a0, vector = a1, a2, source = a3, t0, f12 = step): vector[1] += 3.0f; r = func_088FBA1C(source, vector, sp + 0xC) (the frame words at +0xC / +0x10 start as 0 / -1); when r >= 0 object[0] and object[1] = r (0.0f when r is not above zero), g = func_088FAFD8(source), and when func_088FB100(source, 0, f12 = object[0], f13 = g) is above zero the four words +0x10 .. +0x1C are 1. With step != 0: n = trunc(step / func_088FB028(source)) + (step > 0) goes to the four words, otherwise nothing (step == 0 leaves them); with byte t0 == 1 the four words are zeroed. Then v = func_088FB1B4(source, f12 = object[0]) goes to object[0x30] and [0x2C], func_088D201C(a2) to [0x70] and the four words +0x184 .. +0x190 become 0x481907FF (v0).
// Branches the routes do not take (checked against the asm by hand): step != 0 (the func_088FB028 division and the count) and mode == 1 (the four words zeroed); r < 0 (the object[0] block skipped).
void func_088D1BD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50);
    const std::uint32_t object = ctx.gpr[4], vector = ctx.gpr[5], a2 = ctx.gpr[6], source = ctx.gpr[7];
    const std::uint32_t mode = ctx.gpr[8] & 0xFFu;
    const float step = ctx.fpr[12];
    put_float(rt, vector + 4u, float_at(rt, vector + 4u) + std::bit_cast<float>(0x40400000u));
    put(rt, frame.at(0x10u), 0xFFFFFFFFu);
    put(rt, frame.at(0xCu), 0u);
    const float r = call_setf(rt, ctx, 0x088FBA1Cu, {source, vector, frame.at(0xCu)}, {}).f0;
    if (0.0f <= r) {
        const float clipped = (0.0f < r) ? r : 0.0f;
        put_float(rt, object, clipped);
        put_float(rt, object + 4u, clipped);
        const float f20 = float_at(rt, object);
        const float g = call_setf(rt, ctx, 0x088FAFD8u, {source}, {}).f0;
        const float h = call_setf(rt, ctx, 0x088FB100u, {source, 0u}, {f20, g}).f0;
        ctx.gpr[2] = 1u;
        if (0.0f < h) {
            put(rt, object + 0x1Cu, 1u);
            put(rt, object + 0x10u, 1u);
            put(rt, object + 0x14u, 1u);
            put(rt, object + 0x18u, 1u);
        }
    }
    ctx.gpr[2] = 1u;
    if (!(step == 0.0f)) {
        const float scale = call_setf(rt, ctx, 0x088FB028u, {source}, {}).f0;
        const float quotient = step / scale;
        std::uint32_t count = ctx.fpu_float_to_word(quotient, 1u);
        ctx.gpr[2] = count;
        if (0.0f < step) ++count;
        put(rt, object + 0x10u, count);
        put(rt, object + 0x1Cu, count);
        put(rt, object + 0x18u, count);
        put(rt, object + 0x14u, count);
        ctx.gpr[2] = 1u;
    }
    if (mode == 1u) {
        put(rt, object + 0x1Cu, 0u);
        put(rt, object + 0x18u, 0u);
        put(rt, object + 0x14u, 0u);
        put(rt, object + 0x10u, 0u);
    }
    const GuestResult made = call_setf(rt, ctx, 0x088FB1B4u, {source}, {float_at(rt, object)});
    put(rt, object + 0x30u, made.v0);
    put(rt, object + 0x2Cu, made.v0);
    const std::uint32_t third = call_set(rt, ctx, 0x088D201Cu, {a2});
    put(rt, object + 0x70u, third);
    for (std::uint32_t i = 0u; i < 4u; ++i) put(rt, object + 0x184u + 4u * i, 0x481907FFu);
    ctx.gpr[2] = 0x481907FFu;
}

// func_0895F7C0(group = a0): n = group[8] entries, reached through func_0894CF40(group, i) (an entry is "present" when its word +8 is not zero), func_0894CFB8(group, i) (a block whose float at +0x67C is a weight) and func_0894CFF8(group, i). With n > 0 every present entry's weight is set to 0; when func_0894D040(group) answers non-zero (and n > 0) every present entry i is paired with the later present entries j: when func_08955774 answers zero for both blocks, their byte +0xC bit 4 agree and the distance sqrt((d0 * d0 + d1 * d1) + d2 * d2) of the float triples at +0x10 of their words +8 is at most 200.0f, func_0895F4EC(block, other) is called for each block whose word +8 has a signed byte +3 other than 4. Then every present entry's weight is clamped down to 1.0f. v0 is the last callee's.
// Branches the routes do not take (checked against the asm by hand): count <= 0 and the gate answering zero, pairs that pass the three skip tests (the distance, the func_0895F4EC calls) and weights above 1.0.
void func_0895F7C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t group = ctx.gpr[4];
    const std::int32_t count = signed_of(word(rt, group + 8u));
    const auto entry = [&](std::uint32_t i) { return call_set(rt, ctx, 0x0894CF40u, {group, i}); };
    const auto weight_block = [&](std::uint32_t i) { return call_set(rt, ctx, 0x0894CFB8u, {group, i}); };
    const auto pair_block = [&](std::uint32_t i) { return call_set(rt, ctx, 0x0894CFF8u, {group, i}); };
    const auto clamp_weights = [&]() {
        std::uint32_t index = 0u;
        do {
            const std::uint32_t present = word(rt, entry(index) + 8u);
            const std::uint32_t old = index++;
            if (present != 0u) {
                const std::uint32_t block = weight_block(old);
                if (1.0f < float_at(rt, block + 0x67Cu)) put_float(rt, block + 0x67Cu, 1.0f);
            }
        } while (index != static_cast<std::uint32_t>(count));
    };
    if (count > 0) {
        std::uint32_t index = 0u;
        do {
            const std::uint32_t present = word(rt, entry(index) + 8u);
            const std::uint32_t old = index++;
            if (present != 0u) put(rt, weight_block(old) + 0x67Cu, 0u);
        } while (index != static_cast<std::uint32_t>(count));
    }
    if (call_set(rt, ctx, 0x0894D040u, {group}) == 0u) return;
    if (count <= 0) return;
    std::int32_t outer = 0;
    while (outer < count) {
        if (word(rt, entry(static_cast<std::uint32_t>(outer)) + 8u) == 0u) {
            ++outer;
            continue;
        }
        const std::uint32_t first = pair_block(static_cast<std::uint32_t>(outer));
        const std::int32_t next = outer + 1;
        if (!(next < count)) {
            outer = next;
            continue;
        }
        const std::uint32_t remaining = static_cast<std::uint32_t>(count - next);
        std::uint32_t inner = static_cast<std::uint32_t>(next);
        do {
            if (word(rt, entry(inner) + 8u) == 0u) {
                ++inner;
                continue;
            }
            const std::uint32_t second = pair_block(inner);
            const std::uint32_t s7 = word(rt, first + 8u), s4 = word(rt, second + 8u);
            ++inner;
            if (call_set(rt, ctx, 0x08955774u, {first}) != 0u) continue;
            if (call_set(rt, ctx, 0x08955774u, {second}) != 0u) continue;
            if ((byte_u(rt, s7 + 0xCu) & 0x10u) != (byte_u(rt, s4 + 0xCu) & 0x10u)) continue;
            float delta[3];
            for (std::uint32_t k = 0u; k < 3u; ++k) delta[k] = float_at(rt, s4 + 0x10u + 4u * k) - float_at(rt, s7 + 0x10u + 4u * k);
            const float squared = (psp_mul(delta[0], delta[0]) + psp_mul(delta[1], delta[1])) + psp_mul(delta[2], delta[2]);
            const float distance = call_setf(rt, ctx, 0x08B49D7Cu, {}, {squared}).f0;
            if (std::bit_cast<float>(0x43480000u) < distance) continue;
            if (sext8(byte_u(rt, word(rt, first + 8u) + 3u)) != 4u) (void)call_set(rt, ctx, 0x0895F4ECu, {first, second});
            if (sext8(byte_u(rt, word(rt, second + 8u) + 3u)) != 4u) (void)call_set(rt, ctx, 0x0895F4ECu, {second, first});
        } while (inner != static_cast<std::uint32_t>(next) + remaining);
        outer = next;
    }
    clamp_weights();
}

// func_08A77940(a0, key = a1, mode = a2, items = a3): `mode` 1, 2 or 3 picks among three ways to combine virtual answers of up to three objects items[0..2] (each loaded from guest memory; a null one first reports through func_08A5BB7C(0x08CB0E38) and is then used anyway, as the original). func_08A76ABC(holder, key) fills a pointer s1 at the frame (null: the same report), whose vtable slot 0xE0 (s4) is called last as s4(s1, ...): mode 1: (s1, 0, items[0].slot_0xDC(0), items[0].slot_0xD8()); mode 2: (s1, items[0].slot_0x3C(), items[1].slot_0xDC(0), items[1].slot_0xD8()); mode 3: (s1, items[0].slot_0x3C(), items[1].slot_0xDC(0), items[2].slot_0x3C()). func_08A5B9EC(holder) ends each. Other modes return 3 in v0.
// Branches the routes do not take (checked against the asm by hand): modes 2 and 3 and any other mode (v0 = 3), null item pointers (the reports).
void func_08A77940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30);
    const std::uint32_t key = ctx.gpr[5], mode = ctx.gpr[6], items = ctx.gpr[7];
    const auto check = [&](std::uint32_t pointer) {
        if (pointer == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0E38u});
    };
    ctx.gpr[2] = 1u;
    if (mode == 1u) {
        const std::uint32_t holder = frame.at(8u);
        (void)call_set(rt, ctx, 0x08A76ABCu, {holder, key});
        const std::uint32_t s1 = word(rt, holder);
        check(s1);
        const std::uint32_t s0 = word(rt, items);
        const std::uint32_t s4 = word(rt, word(rt, s1) + 0xE0u);
        check(s0);
        const std::uint32_t s3 = call_virtual(rt, ctx, s0, 0xDCu, {s0, 0u});
        const std::uint32_t again = word(rt, items);
        check(again);
        const std::uint32_t a3 = call_virtual(rt, ctx, again, 0xD8u, {again});
        (void)call_set(rt, ctx, s4, {s1, 0u, s3, a3});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 2u;
    if (mode == 2u) {
        const std::uint32_t first = word(rt, items);
        check(first);
        const std::uint32_t s5 = call_virtual(rt, ctx, first, 0x3Cu, {first});
        const std::uint32_t holder = frame.at(4u);
        (void)call_set(rt, ctx, 0x08A76ABCu, {holder, key});
        const std::uint32_t s1 = word(rt, holder);
        check(s1);
        const std::uint32_t second = word(rt, items + 4u);
        const std::uint32_t s4 = word(rt, word(rt, s1) + 0xE0u);
        check(second);
        const std::uint32_t s3 = call_virtual(rt, ctx, second, 0xDCu, {second, 0u});
        const std::uint32_t again = word(rt, items + 4u);
        check(again);
        const std::uint32_t a3 = call_virtual(rt, ctx, again, 0xD8u, {again});
        (void)call_set(rt, ctx, s4, {s1, s5, s3, a3});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
        return;
    }
    ctx.gpr[2] = 3u;
    if (mode != 3u) return;
    const std::uint32_t first = word(rt, items);
    check(first);
    const std::uint32_t s4 = call_virtual(rt, ctx, first, 0x3Cu, {first});
    const std::uint32_t third = word(rt, items + 8u);
    check(third);
    const std::uint32_t s3 = call_virtual(rt, ctx, third, 0x3Cu, {third});
    const std::uint32_t holder = frame.at(0u);
    (void)call_set(rt, ctx, 0x08A76ABCu, {holder, key});
    const std::uint32_t s1 = word(rt, holder);
    check(s1);
    const std::uint32_t second = word(rt, items + 4u);
    const std::uint32_t s2 = word(rt, word(rt, s1) + 0xE0u);
    check(second);
    const std::uint32_t a2 = call_virtual(rt, ctx, second, 0xDCu, {second, 0u});
    (void)call_set(rt, ctx, s2, {s1, s4, a2, s3});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
}

// func_089101F0(view = a0, records = a1, count = a2): ranks `count` 20-byte records. With count > 0: f22 = func_08AB8990(view, 0, 0), a 3-float vector copied from func_08AB8940(view, 0) to frame +4, and, when func_08AB8718(view) >= 2, the same pair with index 1 (vector to frame +0x10, f23 = the float). For each record: its 3 floats are rotated by func_08B4AA68 (twice, frame vectors +0x10 and +4), a = max(sqrt(func_08B4A720(v)), 1) and b = max(sqrt(func_08B4A720(v')), 1), key = (f22 / a + f23 / b) squared-and-scaled with the record's float at +0xC (k * (k * w) with k = f22 / a + f23 / b); an 8-byte element (index, key bits) is stored in a stack array; func_08BDBB44(array, end, 2 * floor(log2(count))) sorts it and func_08BDB970(array, end) finishes it; then walking the sorted array from the last element to the first, the record named by the element's low byte gets byte +0x10 = its rank (0, 1, 2, ...) and byte +0x11 = the signed byte of the table at 0x08C8A5F4 (3 from the fifth rank on). v0 = the last record address.
// Branches the routes do not take (checked against the asm by hand): count <= 0 or 1, the second vector (func_08AB8718 >= 2) and ranks above 3.
void func_089101F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame fixed(ctx, 0xA0);
    const std::uint32_t fp = ctx.gpr[29];
    const std::uint32_t view = ctx.gpr[4], records = ctx.gpr[5], count = ctx.gpr[6];
    put(rt, fp + 0x44u, records);
    if (signed_of(count) <= 0) return;
    float f23 = 0.0f;
    put(rt, fp + 0x10u, 0u);
    put_float(rt, fp + 0x14u, f23);
    put_float(rt, fp + 0x18u, f23);
    std::uint32_t vector = call_set(rt, ctx, 0x08AB8940u, {view, 0u});
    put(rt, fp + 4u, word(rt, vector));
    put(rt, fp + 8u, word(rt, vector + 4u));
    put(rt, fp + 0xCu, word(rt, vector + 8u));
    const float f22 = call_setf(rt, ctx, 0x08AB8990u, {view, 0u, 0u}, {}).f0;
    if (signed_of(call_set(rt, ctx, 0x08AB8718u, {view})) >= 2) {
        vector = call_set(rt, ctx, 0x08AB8940u, {view, 1u});
        put(rt, fp + 0x10u, word(rt, vector));
        put(rt, fp + 0x14u, word(rt, vector + 4u));
        put(rt, fp + 0x18u, word(rt, vector + 8u));
        f23 = call_setf(rt, ctx, 0x08AB8990u, {view, 1u, 0u}, {}).f0;
    }
    put(rt, fp + 0x40u, count * 8u);
    GuestFrame array(ctx, (count * 8u + 0x26u) & ~0xFu);
    const std::uint32_t base = array.at(0u);
    std::uint32_t source = records, slot = base;
    std::uint32_t low = 0u, high = 0u;
    for (std::uint32_t done = 1u;; ++done) {
        put(rt, fp + 0x1Cu, word(rt, source));
        put(rt, fp + 0x20u, word(rt, source + 4u));
        put(rt, fp + 0x24u, word(rt, source + 8u));
        put(rt, fp, 0u);
        (void)call_set(rt, ctx, 0x08B4AA68u, {fp + 0x28u, fp + 0x1Cu, fp + 0x10u});
        (void)call_set(rt, ctx, 0x08B4AA68u, {fp + 0x1Cu, fp + 0x1Cu, fp + 4u});
        float f20 = std::sqrt(call_setf(rt, ctx, 0x08B4A720u, {fp + 0x1Cu}, {}).f0);
        if (f20 < 1.0f) f20 = 1.0f;
        float f0 = std::sqrt(call_setf(rt, ctx, 0x08B4A720u, {fp + 0x28u}, {}).f0);
        const float f2 = f22 / f20;
        const std::uint32_t next_low = low + 1u;
        const std::uint32_t carry = next_low < low ? 1u : 0u;
        const std::uint32_t next_high = carry + high;
        if (f0 < 1.0f) f0 = 1.0f;
        f0 = f23 / f0;
        const float weight = float_at(rt, source + 0xCu);
        source += 0x14u;
        f0 = f2 + f0;
        const float f1 = psp_mul(f0, weight);
        f0 = psp_mul(f0, f1);
        put_float(rt, fp, f0);
        const std::uint32_t bits = word(rt, fp);
        put(rt, slot, low | 0u);
        put(rt, slot + 4u, high | bits);
        low = next_low;
        high = next_high;
        if (count == done) break;
        slot += 8u;
    }
    const std::uint32_t end = base + count * 8u;
    if (base != end) {
        const std::int32_t elements = signed_of(end - base) >> 3;
        std::uint32_t depth = 0u;
        if (elements != 1) {
            std::int32_t shifted = elements;
            do {
                shifted >>= 1;
                ++depth;
            } while (shifted != 1);
            depth <<= 1;
        }
        (void)call_set(rt, ctx, 0x08BDBB44u, {base, end, depth});
        (void)call_set(rt, ctx, 0x08BDB970u, {base, end});
    }
    std::uint32_t cursor = base + (count - 1u) * 8u;
    std::uint32_t table = 0x08C8A5F4u;
    for (std::uint32_t rank = 0u;;) {
        const std::uint32_t element = byte_u(rt, cursor);
        cursor -= 8u;
        const std::uint32_t record = element * 20u + records;
        ctx.gpr[2] = record;
        put8(rt, record + 0x10u, rank);
        std::uint32_t value = 3u;
        if (signed_of(rank) < 4) value = sext8(byte_u(rt, table));
        put8(rt, record + 0x11u, value);
        ctx.gpr[3] = value;
        ++rank;
        ++table;
        if (count == rank) break;
    }
}

// func_08B00E90(object = a0, list = a1): object halfword +0xEC (signed) is a cursor into list's table (the table pointer is list[0x44], 8-byte entries; a negative cursor returns at once with v0 = that cursor). The frame (0xB0 bytes, the uninitialised words at +0x20 / +0x24 / +0x40 / +0x50 are read exactly where the original reads them) holds the iterator pair the original builds: halfword +0x60 = the cursor, +0x62 = -1, word +0x88 = the entry address (table[0] + cursor * 8, or 0), +0x84 = list + 0x44. While the entry s0 = frame[0x88] is not null: an entry whose halfword +0 has bit 0 clear and whose record (list[0] + halfword(entry + 2) * 240) has bit 2 of halfword +0xE2 clear in `object[0xE2] & record[0xE2]` calls func_08C46854(sp + 0x90, sp + 0x60, sp + 0x80), takes the 16 bytes it wrote at sp + 0x90 as the new iterator at sp + 0x80 and calls func_08B016F4(list, entry); every other entry follows its signed halfword +6 (negative: end) to table[0] + 8 * it. At the end object halfword +0xEC = frame halfword +0x60. v0 = that halfword (sign-extended).
void func_08B00E90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0);
    const std::uint32_t object = ctx.gpr[4], list = ctx.gpr[5];
    const std::uint32_t cursor = sext16(half_u(rt, object + 0xECu));
    ctx.gpr[2] = cursor;
    if (signed_of(cursor) < 0) return;
    const std::uint32_t table = word(rt, list + 0x44u);
    const std::uint32_t a0 = word(rt, frame.at(0x50u)), a1 = word(rt, frame.at(0x20u)), a2 = word(rt, frame.at(0x24u));
    const std::uint32_t pointer = list + 0x44u;
    const std::uint32_t entry_address = table + (cursor << 3);
    put(rt, frame.at(0x10u), a0);
    put(rt, frame.at(0u), a1);
    put(rt, frame.at(4u), a2);
    put(rt, frame.at(0x68u), pointer);
    put(rt, frame.at(0x5Cu), 0u);
    put(rt, frame.at(0x58u), entry_address);
    put(rt, frame.at(0x54u), pointer);
    put(rt, frame.at(0x30u), a0);
    put(rt, frame.at(0x34u), pointer);
    put(rt, frame.at(0x38u), entry_address);
    put(rt, frame.at(0x3Cu), 0u);
    put(rt, frame.at(0x80u), a0);
    put(rt, frame.at(0x84u), pointer);
    put(rt, frame.at(0x88u), entry_address);
    put(rt, frame.at(0x8Cu), 0u);
    put(rt, frame.at(0x2Cu), 0u);
    put(rt, frame.at(0x28u), 0u);
    put(rt, frame.at(0x70u), a1);
    put(rt, frame.at(0x74u), a2);
    put(rt, frame.at(0x78u), 0u);
    put(rt, frame.at(0x7Cu), 0u);
    put(rt, frame.at(0x14u), pointer);
    put(rt, frame.at(0x18u), entry_address);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0x64u), 0u);
    std::uint32_t signed_cursor = 0xFFFFFFFFu;
    if (entry_address != 0u) signed_cursor = sext16(static_cast<std::uint32_t>(signed_of(entry_address - table) >> 3));
    put16(rt, frame.at(0x62u), 0xFFFFu);
    put16(rt, frame.at(0x60u), signed_cursor);
    std::uint32_t current = 0u;
    const std::uint32_t pointer_again = word(rt, frame.at(0x68u));
    if (signed_of(signed_cursor) >= 0) current = word(rt, pointer_again) + (signed_cursor << 3);
    const std::uint32_t uninitialised = word(rt, frame.at(0x40u));
    put(rt, frame.at(0x84u), pointer_again);
    put(rt, frame.at(0x80u), uninitialised);
    put(rt, frame.at(0x88u), current);
    put(rt, frame.at(0x4Cu), 0u);
    put(rt, frame.at(0x48u), current);
    put(rt, frame.at(0x44u), pointer_again);
    put(rt, frame.at(0x8Cu), 0u);
    std::uint32_t s0 = word(rt, frame.at(0x88u));
    while (s0 != 0u) {
        if ((half_u(rt, s0) & 1u) == 0u) {
            const std::uint32_t record = sext16(half_u(rt, s0 + 2u)) * 240u + word(rt, list);
            const std::uint32_t masked = half_u(rt, record + 0xE2u) & half_u(rt, object + 0xE2u) & 4u;
            if (masked == 0u) {
                (void)call_set(rt, ctx, 0x08C46854u, {frame.at(0x90u), frame.at(0x60u), frame.at(0x80u)});
                put(rt, frame.at(0x80u), word(rt, frame.at(0x90u)));
                put(rt, frame.at(0x84u), word(rt, frame.at(0x94u)));
                put(rt, frame.at(0x88u), word(rt, frame.at(0x98u)));
                put(rt, frame.at(0x8Cu), word(rt, frame.at(0x9Cu)));
                (void)call_set(rt, ctx, 0x08B016F4u, {list, s0});
                s0 = word(rt, frame.at(0x88u));
                continue;
            }
        }
        put(rt, frame.at(0x8Cu), s0);
        const std::uint32_t link = sext16(half_u(rt, s0 + 6u));
        const std::uint32_t base_pointer = word(rt, frame.at(0x84u));
        if (signed_of(link) < 0) {
            put(rt, frame.at(0x88u), 0u);
        } else {
            put(rt, frame.at(0x88u), word(rt, base_pointer) + (link << 3));
        }
        s0 = word(rt, frame.at(0x88u));
    }
    const std::uint32_t final_cursor = sext16(half_u(rt, frame.at(0x60u)));
    ctx.gpr[2] = final_cursor;
    put16(rt, object + 0xECu, final_cursor);
}

// func_08A92588(map = a0, out = a1, key = a2): a lookup-or-create on a tree with two sentinel keys (the words 0x08CF6F08 and 0x08CF6F14). Key word == the first sentinel: v1 = map[0x38] when that block and its word +4 are non-zero (else 0) goes to the frame (sp + 0xC), func_08A924F0(sp + 0x10, sp + 0xC), func_08A91A38(sp + 0x14, key, sp + 0x10), func_08A5BAD8(out, *(sp + 0x14)) unless out == sp + 0x14, and func_08A5B9EC for both frame objects. Otherwise func_08A7D708() runs; out[0] != 0 returns; a key equal to the second sentinel (v0 = the lui of that address pair), a null map[0x38] block (the same v0) or one whose word +4 is zero (v0 = that zero) return; else the node s2 = map[0x38] is searched with func_08A7BCC0(s2): when its first word differs from key[0], virtual slot 0x10C of s2 (s2, out, key) is called, otherwise the same three-call construction as the first case runs on the frame (sp + 0 = s2, sp + 4, sp + 8); both end with func_08A7BEBC(map, key, out). v0 = the last callee's.
// Branches the routes do not take (checked against the asm by hand): the first-sentinel arm (key == the word at 0x08CF6F08), the second-sentinel / null-node / empty-node returns and the equal-key construction arm; every compared case called slot 0x10C.
void func_08A92588(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40);
    const std::uint32_t map = ctx.gpr[4], out = ctx.gpr[5], key = ctx.gpr[6];
    const std::uint32_t sentinel = word(rt, 0x08CF6F08u);
    const std::uint32_t key_word = word(rt, key);
    ctx.gpr[7] = sentinel;
    ctx.gpr[3] = key_word;
    const auto build = [&](std::uint32_t output, std::uint32_t temporary, std::uint32_t seed_slot) {
        (void)call_set(rt, ctx, 0x08A924F0u, {temporary, seed_slot});
        (void)call_set(rt, ctx, 0x08A91A38u, {output, key, temporary});
        if (out != output) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, output)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {output});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    };
    if (key_word == sentinel) {
        const std::uint32_t block = word(rt, map + 0x38u);
        std::uint32_t chosen = 0u;
        if (block != 0u && word(rt, block + 4u) != 0u) chosen = block;
        put(rt, frame.at(0xCu), chosen);
        build(frame.at(0x14u), frame.at(0x10u), frame.at(0xCu));
        return;
    }
    (void)call_set(rt, ctx, 0x08A7D708u, {});
    const std::uint32_t existing = word(rt, out);
    ctx.gpr[2] = existing;
    if (existing != 0u) return;
    ctx.gpr[2] = lui_of(0x08CF6F14u);
    const std::uint32_t second_sentinel = word(rt, 0x08CF6F14u);
    if (word(rt, key) == second_sentinel) return;
    const std::uint32_t node = word(rt, map + 0x38u);
    if (node == 0u) return;
    ctx.gpr[2] = word(rt, node + 4u);
    if (ctx.gpr[2] == 0u) return;
    const std::uint32_t found = call_set(rt, ctx, 0x08A7BCC0u, {node});
    if (word(rt, key) != word(rt, found)) {
        const std::uint32_t function = word(rt, word(rt, node) + 0x10Cu);
        ctx.gpr[2] = function;
        (void)call_set(rt, ctx, function, {node, out, key});
    } else {
        put(rt, frame.at(0u), node);
        build(frame.at(8u), frame.at(4u), frame.at(0u));
    }
    (void)call_set(rt, ctx, 0x08A7BEBCu, {map, key, out});
}

// func_08B31AB4(object = a0): v0 = 1 when a status read says so: the virtual slot 0x64 of the object (a0 = it) answering non-zero reads the status word with func_08C7AF30(sp) (v0 = (status == 4) when the call is not negative), otherwise slot 0x60 answering non-zero reads it with func_08C7AEF0(sp) (v0 = (status == 1)); everything else gives 0.
// Branches the routes do not take (checked against the asm by hand): the slot-0x64 arm (status == 4) and a negative import answer.
void func_08B31AB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20);
    const std::uint32_t object = ctx.gpr[4];
    if (call_virtual(rt, ctx, object, 0x64u, {object}) != 0u) {
        ctx.gpr[4] = object;
        if (signed_of(call_set(rt, ctx, 0x08C7AF30u, {frame.at(0u)})) < 0) {
            ctx.gpr[2] = 0u;
            return;
        }
        ctx.gpr[2] = word(rt, frame.at(0u)) == 4u ? 1u : 0u;
        return;
    }
    ctx.gpr[4] = object;
    if (call_virtual(rt, ctx, object, 0x60u, {object}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (signed_of(call_set(rt, ctx, 0x08C7AEF0u, {frame.at(0u)})) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = word(rt, frame.at(0u)) == 1u ? 1u : 0u;
}

// func_0880BBF4(object = a0, flag = a1): func_08B344E0(object); func_088693C4(object + 0x30, a1 & 0xFF); then the tail jump func_08B3457C(object) (a0 = object).
void func_0880BBF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], flag = ctx.gpr[5] & 0xFFu;
    (void)call_set(rt, ctx, 0x08B344E0u, {object});
    (void)call_set(rt, ctx, 0x088693C4u, {object + 0x30u, flag});
    ctx.gpr[4] = object;
    tail_call(ctx, 0x08B3457Cu, {object});
}

// func_08B35418(): v0 = func_08C7A948() - 0x2800, or 0 when that value is below 0x2800 (unsigned).
void func_08B35418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t now = call_set(rt, ctx, 0x08C7A948u, {});
    ctx.gpr[3] = now - 0x2800u;
    ctx.gpr[2] = now < 0x2800u ? 0u : now - 0x2800u;
}

// func_08AEB850(handle = a0): s1 = func_08AD84B4(4, 1) (a block of one word); with s1 non-null the import func_08C7A998(handle, 0, 1, 1, 0) answers v0, stored into s1[0]; a negative answer (or a null block) releases the block with func_08AEB8C8(s1) and returns 0; otherwise v0 = v1 = s1.
void func_08AEB850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    const std::uint32_t block = call_set(rt, ctx, 0x08AD84B4u, {4u, 1u});
    ctx.gpr[4] = handle;
    ctx.gpr[5] = 0u;
    ctx.gpr[6] = 1u;
    ctx.gpr[7] = 1u;
    ctx.gpr[8] = 0u;
    if (block != 0u) {
        const std::uint32_t answer = call_set(rt, ctx, 0x08C7A998u, {handle, 0u, 1u, 1u, 0u});
        put(rt, block, answer);
        ctx.gpr[3] = block;
        if (signed_of(answer) >= 0) {
            ctx.gpr[2] = block;
            return;
        }
    }
    (void)call_set(rt, ctx, 0x08AEB8C8u, {block});
    ctx.gpr[3] = 0u;
    ctx.gpr[2] = 0u;
}

// func_08B2DBF0(value = a0): the import func_08C7AF70(x, v, y) with the pair (x, y) chosen by v = value: v >= 0x14E: (0x14D, 0x14D, 0xA6); v <= 0: (0xBE, 1, 0x5F); v >= 0x10B: (0x14D, v, 0xA6); v >= 0xDF: (0x10A, v, 0x85); v >= 0xBF: (0xDE, v, 0x6F); else (0xBE, v, 0x5F). v0 = 1 when it answers zero, else 0.
void func_08B2DBF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t value = ctx.gpr[4];
    std::uint32_t x, y;
    if (!(signed_of(value) < 0x14E)) {
        value = 0x14Du;
        x = 0x14Du;
        y = 0xA6u;
    } else if (!(signed_of(value) > 0)) {
        value = 1u;
        x = 0xBEu;
        y = 0x5Fu;
    } else if (!(signed_of(value) < 0x10B)) {
        x = 0x14Du;
        y = 0xA6u;
    } else if (!(signed_of(value) < 0xDF)) {
        x = 0x10Au;
        y = 0x85u;
    } else if (!(signed_of(value) < 0xBF)) {
        x = 0xDEu;
        y = 0x6Fu;
    } else {
        x = 0xBEu;
        y = 0x5Fu;
    }
    ctx.gpr[2] = call_set(rt, ctx, 0x08C7AF70u, {x, value, y}) == 0u ? 1u : 0u;
}

// func_0897BB68(object = a0): object byte +0x88 = 1; func_08977260(object); func_089B17B8(its answer); func_089FD934() -> a1; func_0897BB18(object, a1); func_0897851C(object). Then the bit range [start, end) of a bitmap is cleared: (word pointer = object[0x74], bit = object[0x78]) up to (word pointer = object[0x7C], bit = object[0x80]); each bit is cleared in its word (the word is rewritten for every bit, as the original) and the position moves to bit 0 of the next word after bit 31. v0 = 1 << (the final bit).
void func_0897BB68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    put8(rt, object + 0x88u, 1u);
    const std::uint32_t first = call_set(rt, ctx, 0x08977260u, {object});
    const std::uint32_t second = call_set(rt, ctx, 0x089B17B8u, {first});
    const std::uint32_t third = call_set(rt, ctx, 0x089FD934u, {});
    (void)second;
    (void)call_set(rt, ctx, 0x0897BB18u, {object, third});
    (void)call_set(rt, ctx, 0x0897851Cu, {object});
    std::uint32_t word_pointer = word(rt, object + 0x74u);
    std::uint32_t bit = word(rt, object + 0x78u);
    const std::uint32_t end_pointer = word(rt, object + 0x7Cu), end_bit = word(rt, object + 0x80u);
    while (true) {
        const std::uint32_t mask = 1u << (bit & 31u);
        ctx.gpr[2] = mask;
        ctx.gpr[3] = ~mask;
        if (end_pointer == word_pointer && end_bit == bit) break;
        const std::uint32_t cleared = word(rt, word_pointer) & ~mask;
        ctx.gpr[2] = cleared;
        put(rt, word_pointer, cleared);
        ++bit;
        ctx.gpr[3] = 0x20u;
        if (bit == 0x20u) {
            word_pointer += 4u;
            bit = 0u;
        }
    }
    ctx.gpr[4] = bit;
    ctx.gpr[5] = word_pointer;
}

// func_0896264C(object = a0, value = a1): func_08B34888(object + 0x40) (the lock); func_08BE4120(object + 8, value); n = func_08BE3B38(object + 8, 0x80); when n equals object[0x3C], func_08B34CA0(object + 0x40); then the tail jump func_08B34B24(object + 0x40) (the unlock, a0 = object + 0x40). v1 = object[0x3C].
// Branches the routes do not take (checked against the asm by hand): the count differing from object[0x3C] (func_08B34CA0 skipped).
void func_0896264C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4], value = ctx.gpr[5];
    const std::uint32_t lock = object + 0x40u, inner = object + 8u;
    (void)call_set(rt, ctx, 0x08B34888u, {lock});
    (void)call_set(rt, ctx, 0x08BE4120u, {inner, value});
    const std::uint32_t count = call_set(rt, ctx, 0x08BE3B38u, {inner, 0x80u});
    const std::uint32_t expected = word(rt, object + 0x3Cu);
    ctx.gpr[3] = expected;
    ctx.gpr[4] = lock;
    if (count == expected) (void)call_set(rt, ctx, 0x08B34CA0u, {lock});
    ctx.gpr[4] = lock;
    tail_call(ctx, 0x08B34B24u, {lock});
}

// The two singleton-string getters: a holder fetched with func_088248E4 (null reports through func_08A5BB7C(0x08C99B4C) and is read anyway), field +0x14 of it, the holder released with func_08A5B9EC, then `resolver(field)` and func_08A97A6C(sp, answer) fill a frame string that func_08A5BAD8(out, *(sp)) copies into `out` unless out is that frame address, and func_08A5B9EC(sp) ends it.
inline void fetch_singleton_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t resolver) {
    GuestFrame frame(ctx, 0x20);
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t holder = frame.at(4u);
    (void)call_set(rt, ctx, 0x088248E4u, {holder});
    std::uint32_t object = word(rt, holder);
    if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99B4Cu});
    const std::uint32_t field = word(rt, object + 0x14u);
    (void)call_set(rt, ctx, 0x08A5B9ECu, {holder});
    const std::uint32_t answer = call_set(rt, ctx, resolver, {field});
    (void)call_set(rt, ctx, 0x08A97A6Cu, {frame.at(0u), answer});
    if (out != frame.at(0u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
// func_08825AE8(out = a0): the resolver is func_0885D010.
void func_08825AE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fetch_singleton_string(rt, ctx, 0x0885D010u); }
// func_08825B74(out = a0): the resolver is func_0885BFE0.
void func_08825B74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { fetch_singleton_string(rt, ctx, 0x0885BFE0u); }

// func_08B098A8(handle = a0): while the word 0x08D4744C is non-zero: func_08B2EFDC(-1, -1); when the word is still non-zero func_08B09824(handle), and again. v0 = 0 on every path (the word that ended the loop).
void func_08B098A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    ctx.gpr[2] = word(rt, 0x08D4744Cu);
    if (ctx.gpr[2] != 0u) {
        while (true) {
            (void)call_set(rt, ctx, 0x08B2EFDCu, {0xFFFFFFFFu, 0xFFFFFFFFu});
            ctx.gpr[2] = word(rt, 0x08D4744Cu);
            if (ctx.gpr[2] == 0u) break;
            (void)call_set(rt, ctx, 0x08B09824u, {handle});
            ctx.gpr[2] = word(rt, 0x08D4744Cu);
            if (ctx.gpr[2] == 0u) break;
        }
    }
}

// func_08BC6550(): a function-local static built once: the guard byte at 0x08D76288 non-zero returns (v0 = the lui of the address pair); func_08B6441C(guard) answering zero returns 0 (another thread builds it); else s1 = func_08B9E208(), s0 = func_08BBD034(), v = func_08BBD42C(); s0[0x30] = s1, s0[8] = 0x08CA0ED8, s0[0x2C] = v; func_08B18DFC(func_08BBD034()) and the tail jump func_08B64494(guard) (the release).
// Branches the routes do not take (checked against the asm by hand): the contended guard (func_08B6441C answering zero).
void func_08BC6550(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kGuard = 0x08D76288u;
    ctx.gpr[2] = lui_of(kGuard);
    if (sext8(byte_u(rt, kGuard)) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {kGuard}) == 0u) return;
    const std::uint32_t s1 = call_set(rt, ctx, 0x08B9E208u, {});
    const std::uint32_t s0 = call_set(rt, ctx, 0x08BBD034u, {});
    const std::uint32_t made = call_set(rt, ctx, 0x08BBD42Cu, {});
    put(rt, s0 + 0x30u, s1);
    put(rt, s0 + 8u, 0x08CA0ED8u);
    put(rt, s0 + 0x2Cu, made);
    const std::uint32_t again = call_set(rt, ctx, 0x08BBD034u, {});
    (void)call_set(rt, ctx, 0x08B18DFCu, {again});
    tail_call(ctx, 0x08B64494u, {kGuard});
}

// func_08C0A8A0(vector = a0, position = a1, value = a2): a vector of words: insert *value before `position`. Room left (finish != end of storage): a non-null finish first receives the last element (finish[0] = finish[-1]), vector[4] = finish + 4, func_08B79BAC(finish - bytes, position, bytes) with bytes = ((finish - 4) - position) rounded down to words moves the tail up, position[0] = the value (read before the move). No room: the new capacity is 1 element (empty), twice the size, or the maximum when that overflows (a size of 0x3FFFFFFF reports through func_08B5ADDC(0x08CA7CD8) first); func_08A5BC20(capacity * 4, 4) allocates, func_08C0A870 copies the head [start, position) (the answer is where the new element goes: it gets *value when not null) and the tail [position, finish) behind it, func_08C0A860(start, finish, sp) ends the old range, func_08A5BC54(start, capacity bytes, 4) frees it when not null, and vector = (new start, new finish, new start + capacity bytes). v0 = the last callee's, or the new end of storage.
// Branches the routes do not take (checked against the asm by hand): room left in the vector (the in-place insert and the memmove), a non-empty vector growing (doubling) and the free of the old storage; every compared case grew an empty vector.
void func_08C0A8A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30);
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    std::uint32_t finish = word(rt, vector + 4u);
    const std::uint32_t storage_end = word(rt, vector + 8u);
    if (finish != storage_end) {
        std::uint32_t top = finish;
        if (finish != 0u) {
            put(rt, finish, word(rt, finish - 4u));
            finish = word(rt, vector + 4u);
            top = finish;
        }
        const std::uint32_t bytes = ((top - 4u) - position) & ~3u;
        put(rt, vector + 4u, finish + 4u);
        const std::uint32_t moved = word(rt, value);
        (void)call_set(rt, ctx, 0x08B79BACu, {top - bytes, position, bytes});
        put(rt, position, moved);
        return;
    }
    const std::int32_t size = signed_of(finish - word(rt, vector)) >> 2;
    if (size == 0x3FFFFFFF) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CA7CD8u});
    std::uint32_t capacity_bytes;
    if (size == 0) {
        capacity_bytes = 4u;
    } else {
        const std::uint32_t doubled = static_cast<std::uint32_t>(size) << 1;
        capacity_bytes = doubled < static_cast<std::uint32_t>(size) ? 0xFFFFFFFCu : doubled << 2;
    }
    const std::uint32_t fresh = call_set(rt, ctx, 0x08A5BC20u, {capacity_bytes, 4u});
    const std::uint32_t end_of_head = call_set(rt, ctx, 0x08C0A870u, {word(rt, vector), position, fresh, frame.at(2u)});
    if (end_of_head != 0u) put(rt, end_of_head, word(rt, value));
    const std::uint32_t new_finish = call_set(rt, ctx, 0x08C0A870u, {position, word(rt, vector + 4u), end_of_head + 4u, frame.at(1u)});
    (void)call_set(rt, ctx, 0x08C0A860u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_start = word(rt, vector);
    const std::uint32_t old_bytes = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - old_start) >> 2) << 2;
    if (old_start != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_start, old_bytes, 4u});
    const std::uint32_t new_end = fresh + capacity_bytes;
    ctx.gpr[2] = new_end;
    put(rt, vector + 8u, new_end);
    put(rt, vector + 4u, new_finish);
    put(rt, vector, fresh);
}


// func_0885E940(object = a0): the start-up chain of one engine object: func_088D6E9C(object[0][0x5AC]); then func_088CBFD4() -> func_088CBE6C(it), func_088CA548() -> func_088C9780(it), func_08916474() -> func_08916194(it), func_088C7F64() -> func_088C78B4(it) (each no-argument call leaves a0 as the previous call left it), func_08B1C47C() and the tail jump func_08866FA8(object[0]).
void func_0885E940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[4];
    (void)call_set(rt, ctx, 0x088D6E9Cu, {word(rt, word(rt, object) + 0x5ACu)});
    std::uint32_t value = call_set(rt, ctx, 0x088CBFD4u, {});
    value = call_set(rt, ctx, 0x088CBE6Cu, {value});
    value = call_set(rt, ctx, 0x088CA548u, {});
    value = call_set(rt, ctx, 0x088C9780u, {value});
    value = call_set(rt, ctx, 0x08916474u, {});
    value = call_set(rt, ctx, 0x08916194u, {value});
    value = call_set(rt, ctx, 0x088C7F64u, {});
    value = call_set(rt, ctx, 0x088C78B4u, {value});
    (void)call_set(rt, ctx, 0x08B1C47Cu, {});
    tail_call(ctx, 0x08866FA8u, {word(rt, object)});
}

// func_08820C0C(a0, count = a1, source = a2): count below 2 returns (v0 = 1). Otherwise func_089AB370(sp, source), func_08820A54(*(sp)), func_08969244(sp + 4, source + 4), func_08820B68(*(sp + 4)), func_08B0ABC0(func_08B0979C(), 0x08820DF8, 0) and the two frame objects are ended with func_08A5B9EC (sp + 4, then sp).
// Branches the routes do not take (checked against the asm by hand): count < 2 (the early return).
void func_08820C0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20);
    const std::uint32_t count = ctx.gpr[5], source = ctx.gpr[6];
    ctx.gpr[2] = signed_of(count) < 2 ? 1u : 0u;
    ctx.gpr[5] = source;
    if (signed_of(count) < 2) return;
    (void)call_set(rt, ctx, 0x089AB370u, {frame.at(0u), source});
    (void)call_set(rt, ctx, 0x08820A54u, {word(rt, frame.at(0u))});
    (void)call_set(rt, ctx, 0x08969244u, {frame.at(4u), source + 4u});
    (void)call_set(rt, ctx, 0x08820B68u, {word(rt, frame.at(4u))});
    const std::uint32_t made = call_set(rt, ctx, 0x08B0979Cu, {});
    (void)call_set(rt, ctx, 0x08B0ABC0u, {made, 0x08820DF8u, 0u});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08B7771C(size = a0): a bump allocator over a block the first call obtains (the words 0x08D72D00 base, 0x08D72CFC cursor, 0x08D72D04 end, 0x08D72D08 the block id). With no base yet: the id comes from the import func_08C7A968(2, 0x08C96BCC, 3, 0x10000, 0x1000), is stored, and when positive func_08C7A970(id) gives the start, so base = cursor = start and end = start + 0x10000 (the other arm of the original's first test is a null weak symbol and never taken). Then v0 = the old cursor and cursor += size, or -1 when there is no base, the new cursor falls below the base or goes past the end.
void func_08B7771C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t size = ctx.gpr[4];
    if (word(rt, 0x08D72D00u) == 0u) {
        constexpr std::uint32_t kBytes = 0x10000u;
        const std::uint32_t id = call_set(rt, ctx, 0x08C7A968u, {2u, 0x08C96BCCu, 3u, kBytes, 0x1000u});
        put(rt, 0x08D72D08u, id);
        if (signed_of(id) > 0) {
            const std::uint32_t start = call_set(rt, ctx, 0x08C7A970u, {id});
            put(rt, 0x08D72D04u, start + kBytes);
            put(rt, 0x08D72CFCu, start);
            put(rt, 0x08D72D00u, start);
        }
    }
    const std::uint32_t base = word(rt, 0x08D72D00u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (base == 0u) return;
    const std::uint32_t cursor = word(rt, 0x08D72CFCu);
    const std::uint32_t next = cursor + size;
    if (next < base) return;
    if (word(rt, 0x08D72D04u) < next) return;
    ctx.gpr[2] = cursor;
    put(rt, 0x08D72CFCu, next);
}

// ---- end of functions ----

}  // namespace
}  // namespace hot_batch_102
}  // namespace gt::game

#define HOT102(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_102::name, gt::ReturnKind::kind)
#define HOT102N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_102::name, gt::ReturnKind::kind)

// HOT102REGS
HOT102(0x08B207A4u, func_08B207A4, Word);
HOT102(0x08891C54u, func_08891C54, Word);
HOT102(0x08C4CB24u, func_08C4CB24, Word);
HOT102(0x08C4CADCu, func_08C4CADC, Word);
// HOT102(0x0888879Cu, func_0888879C, Word);  // HOT102: writes memory but no compared case changed it (18 sampled cases and a dense race run of 3,000 cases, all equal, none with the first block counted down: every holder[4] was empty)
HOT102(0x08873EA0u, func_08873EA0, Word);
// HOT102(0x08BD74BCu, func_08BD74BC, Word);  // HOT102: compared 0 in every run: all 10 cases not comparable (the member function it calls reaches a blocking import)
HOT102(0x089D5690u, func_089D5690, Word);
HOT102(0x089281E4u, func_089281E4, Word);
HOT102(0x0890778Cu, func_0890778C, Word);
HOT102(0x089191DCu, func_089191DC, Word);
HOT102(0x088BBA00u, func_088BBA00, Word);
HOT102(0x088AAAE8u, func_088AAAE8, Word);
HOT102(0x088AAC74u, func_088AAC74, Word);
// HOT102(0x08B6C020u, func_08B6C020, Float);  // HOT102: compared 0 in every run: the routes never call it (calls 0 on all five)
HOT102(0x08B020ECu, func_08B020EC, Word);
HOT102(0x08C3EEE0u, func_08C3EEE0, Word);
HOT102(0x08BDBB44u, func_08BDBB44, Word);
HOT102(0x08BB1D9Cu, func_08BB1D9C, Word);
HOT102(0x08AE9CD8u, func_08AE9CD8, Word);
HOT102(0x089408D0u, func_089408D0, Word);
HOT102(0x08B51D60u, func_08B51D60, Word);
HOT102(0x08A03FFCu, func_08A03FFC, Word);
HOT102(0x08ADC4E0u, func_08ADC4E0, Word);
HOT102(0x0890673Cu, func_0890673C, Word);
HOT102(0x08951304u, func_08951304, Word);
HOT102(0x089594D8u, func_089594D8, Word);
HOT102(0x0895871Cu, func_0895871C, Word);
HOT102(0x0895EEF8u, func_0895EEF8, Word);
HOT102(0x089572C4u, func_089572C4, Word);
HOT102(0x08BDABCCu, func_08BDABCC, Word);
HOT102(0x088A0DD4u, func_088A0DD4, Word);
HOT102(0x088B4F3Cu, func_088B4F3C, Word);
HOT102(0x088D1BD0u, func_088D1BD0, Word);
HOT102(0x0895F7C0u, func_0895F7C0, Word);
HOT102(0x08A77940u, func_08A77940, Word);
HOT102(0x089101F0u, func_089101F0, Word);
// HOT102(0x08B00E90u, func_08B00E90, Word);  // HOT102: writes memory but no compared case changed it (20 sampled cases and a dense race run of 3,000 cases, all equal: the cursor halfword it stores was already there and the iterator loop never ran)
HOT102(0x08A92588u, func_08A92588, Word);
HOT102(0x08B31AB4u, func_08B31AB4, Word);
HOT102(0x0880BBF4u, func_0880BBF4, Word);
// HOT102(0x08B35418u, func_08B35418, Word);  // HOT102: compared 0 in every run: all 5 cases not comparable (the import func_08C7A948 is not recordable)
// HOT102(0x08AEB850u, func_08AEB850, Word);  // HOT102: compared 0 in every run: all 10 cases not comparable (the import func_08C7A998 is not recordable)
// HOT102(0x08B2DBF0u, func_08B2DBF0, Word);  // HOT102: compared 0 in every run: all 5 cases not comparable (the import func_08C7AF70 is not recordable)
// HOT102(0x0897BB68u, func_0897BB68, Word);  // HOT102: compared 0 in every run: all 20 cases not comparable (its callees reach sce* imports that block or switch threads)
HOT102(0x0896264Cu, func_0896264C, Word);
// HOT102(0x08825AE8u, func_08825AE8, Word);  // HOT102: compared 0 in every run: its one case is not comparable (func_0885D010 reaches an sce* import that blocks)
HOT102(0x08825B74u, func_08825B74, Word);
// HOT102(0x08B098A8u, func_08B098A8, Word);  // HOT102: compared 0 in every run: all 5 cases not comparable (its callees reach sce* imports that block or switch threads)
HOT102(0x08BC6550u, func_08BC6550, Word);
HOT102(0x08C0A8A0u, func_08C0A8A0, Word);
// HOT102(0x0885E940u, func_0885E940, Word);  // HOT102: compared 0 in every run: its one case is not comparable (the start-up chain reaches an sce* import that blocks)
HOT102(0x08820C0Cu, func_08820C0C, Word);
// HOT102(0x08B7771Cu, func_08B7771C, Word);  // HOT102: compared 0 in every run: all 5 cases not comparable (the allocator reaches a partition-memory import that is not recordable)
