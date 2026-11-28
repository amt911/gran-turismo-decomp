// New batch (main session 1052). Drafts from `tools.disasm`, structured C, no gate run (draft-only task).
//
// 0x08AE4BB0 is the SGX sequencer's SESQ message evaluator (`sgxseq.c`, the file name logged by its own default arm):
// a1 is the message object, a0 the interpreter context it hands to the handlers, a2 a nonzero flag. Its inline handler
// pointers sit at +0x34 / +0x38 of the object (not behind a vtable word), +0x48 is a word saved at entry and restored
// by three arms, +0x4C is cleared with it, +0x55 is the "unknown message" flag byte, and +0x10 is the object case 0xF9
// asks through +0x10 -> +8. The opcode comes from the object's fn_34 slot once it is >= 0xA0 (below that the object is
// expanded through fn_38 and re-asked); the table at 0x08C91574 maps opcodes 0xB0..0xFF to the arms below, the inner
// table 0x08C916B4 maps 0xE0..0xE8 to the arithmetic/bit operators (0 = assign, 1..8 = + - * / % & | ^).
//
// The dispatcher's table index is the *opcode* minus 0xB0 (`sltiu v1, v1, 0x50` then `sll v0, v1, 2` at 0x08AE4C54),
// so the switch below is over the opcode itself; an earlier draft switched over `token - 0xB0` and no arm could ever be
// reached from a real opcode.
//
// Every arm ends in the shared epilogue, which returns v1 (`addu v0, v1, zero`): 1 for "handled", 0 for "the handler
// took the message object over" (the arms that store +0x48/+0x4C). No arm falls through into another; the arms the
// table leaves out (0xB4..0xBF, 0xC4..0xCF, 0xD6..0xDF, 0xE9..0xEF, 0xFC..0xFD) run the "unknown SESQ message" arm.
//
// The reads of `func_08AE3DB4(self, default)` are in the order the calls run, not the order the delay slots write the
// saved registers: a `jal`'s delay slot runs before its callee, so the value moved into sN there is the *previous*
// call's result (the traps in docs/agents/decomp-workflow.md). Each evaluation's default is the a1 the arm sets.
//
// `tools.literal_check` reports 37 arm labels (0xB1..0xFF) and the inner operator indices: they are the handler
// table's own data at 0x08C91574 / 0x08C916B4, not an instruction operand of this function, so they belong in its
// --allow list. `tools.sp_loads` lists the five med3 spill reloads of 0x08B7A3BC (below); `tools.arg_reads` and
// `tools.frame_audit` are clean.
//
// 0x08B7A3BC is the libc qsort the PSP carries (Bentley & McIlroy): insertion sort below 7 elements, median-of-three
// for 8..40, median-of-nine above 40, the pa/pb/pc/pd partition with swap_cnt, vecswap, recursion on the smaller half
// and a loop on the larger one. `swaptype` is SWAPINIT: 0 when base and size are 4-aligned and size == 4 (a bare word
// swap), 1 when both are 4-aligned and size is not 4 (whole words), 2 otherwise (bytes). The comparator is a3; the
// function returns nothing (v0 is whatever the last comparator call left), so it is registered Void.
// `tools.sp_loads` lists five `lw s0/s6, 0/4/8(sp)`: those slots are not saved callee-saved registers but the med3
// arguments spilled at 0x08B7A860 (pm - d) and 0x08B7A8A0 (pn - 2d) and the `base + size` copy at 0x08B7A4B0; the C
// keeps all three as locals, so the reloads have nothing to model.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1052 {
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
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// ----------------------------------------------------------------------------------------------------------------
// 0x08AE4BB0: SESQ message dispatch.

// func_08AE3DB4(self, default): evaluates the next value of the message object.
u32 evaluate(Runtime &rt, AllegrexContext &ctx, u32 self, u32 deflt) {
    return call(rt, ctx, 0x08AE3DB4u, {self, deflt});
}
// The shared epilogue: `addu v0, v1, zero` returns v1 in v0.
void finish(AllegrexContext &ctx, u32 value) {
    ctx.gpr[3] = value;
    ctx.gpr[2] = value;
}
// The arms that gave the message object to a handler store the word read from +0x48 at entry back and clear +0x4C.
void keep_message(Runtime &rt, u32 self, u32 saved) {
    put(rt, self + 0x48u, saved);
    put(rt, self + 0x4Cu, 0u);
}
// The PSP's `div` in LO: a zero divisor (impossible here, func_08AE4BA4 replaces it with 1) leaves the dividend, and
// the overflow INT_MIN / -1 leaves INT_MIN as the ALU and the recompiler do.
u32 div_lo(u32 dividend, u32 divisor) {
    if (divisor == 0u) return dividend;
    if (dividend == 0x80000000u && divisor == 0xFFFFFFFFu) return 0x80000000u;
    return static_cast<u32>(static_cast<s32>(dividend) / static_cast<s32>(divisor));
}
// The same in HI: the remainder.
u32 div_hi(u32 dividend, u32 divisor) {
    if (divisor == 0u) return dividend;
    if (dividend == 0x80000000u && divisor == 0xFFFFFFFFu) return 0u;
    return static_cast<u32>(static_cast<s32>(dividend) % static_cast<s32>(divisor));
}

// 0x08AE4BB0 (a0 = interpreter context, a1 = SESQ message object, a2 = flag): dispatch the object's message opcode.

// ----------------------------------------------------------------------------------------------------------------
// 0x08B7A3BC: qsort(base = a0, count = a1, size = a2, compare = a3).

// swapfunc(a, b, n, swaptype): `n / 4` words when swaptype <= 1, `n` bytes otherwise. The loops are do-while loops
// (`bgtz` after the decrement), so a zero count still moves one unit -- unreachable from the arithmetic below.
void swap_range(Runtime &rt, u32 swaptype, u32 x, u32 y, u32 bytes) {
    if (swaptype <= 1u) {
        s32 count = static_cast<s32>(bytes / 4u);
        do {
            const u32 first = word(rt, x);
            const u32 second = word(rt, y);
            put(rt, x, second);
            put(rt, y, first);
            x += 4u;
            y += 4u;
        } while (--count > 0);
    } else {
        s32 count = static_cast<s32>(bytes);
        do {
            const std::uint8_t first = rt.memory().load8(x);
            const std::uint8_t second = rt.memory().load8(y);
            rt.memory().store8(x, second);
            rt.memory().store8(y, first);
            ++x;
            ++y;
        } while (--count > 0);
    }
}
// swap(a, b): one word when swaptype == 0, swapfunc over `size` bytes otherwise.
void swap_one(Runtime &rt, u32 swaptype, u32 x, u32 y, u32 size) {
    if (swaptype == 0u) {
        const u32 first = word(rt, x);
        const u32 second = word(rt, y);
        put(rt, x, second);
        put(rt, y, first);
        return;
    }
    swap_range(rt, swaptype, x, y, size);
}
// vecswap(a, b, n): always swapfunc (no single-word fast path), and nothing for an empty range.
void vec_swap(Runtime &rt, u32 swaptype, u32 x, u32 y, u32 bytes) {
    if (bytes != 0u) swap_range(rt, swaptype, x, y, bytes);
}
// The comparator call leaves its answer in v0 as the original's `jalr` does.
s32 compare(Runtime &rt, AllegrexContext &ctx, u32 function, u32 x, u32 y) {
    ctx.gpr[2] = call_guest(rt, ctx, function, {x, y});
    return static_cast<s32>(ctx.gpr[2]);
}
// med3(a, b, c): the middle of the three, with the comparator calls the original makes (and no more).
u32 median3(Runtime &rt, AllegrexContext &ctx, u32 compare_at, u32 x, u32 y, u32 z) {
    if (compare(rt, ctx, compare_at, x, y) < 0) {
        if (compare(rt, ctx, compare_at, y, z) < 0) return y;
        return compare(rt, ctx, compare_at, x, z) < 0 ? z : x;
    }
    if (compare(rt, ctx, compare_at, y, z) > 0) return y;
    return compare(rt, ctx, compare_at, x, z) < 0 ? x : z;
}
// The insertion sort of the original (its `n < 7` arm and the swap_cnt == 0 arm): [base, base + count * size).
void insertion_sort(Runtime &rt, AllegrexContext &ctx, u32 compare_at, u32 base, u32 count, u32 size, u32 swaptype) {
    const u32 end = base + count * size;
    for (u32 position = base + size; position < end; position += size) {
        u32 previous = position;
        while (previous > base) {
            const u32 before = previous - size;
            if (compare(rt, ctx, compare_at, before, previous) <= 0) break;
            swap_one(rt, swaptype, previous, before, size);
            previous = before;
        }
    }
}

void func_08B7A3BC(Runtime &rt, AllegrexContext &ctx) {
    u32 base = ctx.gpr[4];
    u32 count = ctx.gpr[5];
    const u32 size = ctx.gpr[6];
    const u32 compare_at = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    for (;;) {
        // SWAPINIT. The original re-tests only `size` when it jumps back here (base stays 4-aligned then, `size` times
        // the element count from the aligned base), so this is the same value on both entries.
        const u32 swaptype = ((base & 3u) != 0u || (size & 3u) != 0u) ? 2u : (size == 4u ? 0u : 1u);
        u32 swap_cnt = 0u;
        if (count < 7u) {
            insertion_sort(rt, ctx, compare_at, base, count, size, swaptype);
            return;
        }
        u32 middle = base + (count / 2u) * size;
        if (count >= 8u) {
            u32 low = base;
            u32 high = base + (count - 1u) * size;
            if (count >= 41u) {
                const u32 step = (count / 8u) * size;
                low = median3(rt, ctx, compare_at, low, low + step, low + 2u * step);
                middle = median3(rt, ctx, compare_at, middle - step, middle, middle + step);
                high = median3(rt, ctx, compare_at, high - 2u * step, high - step, high);
            }
            middle = median3(rt, ctx, compare_at, low, middle, high);
        }
        swap_one(rt, swaptype, base, middle, size);
        u32 first_equal = base + size;   // pa
        u32 scan_low = base + size;      // pb
        u32 last_equal = base + (count - 1u) * size;  // pd
        u32 scan_high = last_equal;      // pc
        for (;;) {
            s32 result;
            while (scan_low <= scan_high && (result = compare(rt, ctx, compare_at, scan_low, base)) <= 0) {
                if (result == 0) {
                    swap_cnt = 1u;
                    swap_one(rt, swaptype, first_equal, scan_low, size);
                    first_equal += size;
                }
                scan_low += size;
            }
            while (scan_low <= scan_high && (result = compare(rt, ctx, compare_at, scan_high, base)) >= 0) {
                if (result == 0) {
                    swap_cnt = 1u;
                    swap_one(rt, swaptype, scan_high, last_equal, size);
                    last_equal -= size;
                }
                scan_high -= size;
            }
            if (scan_low > scan_high) break;
            swap_one(rt, swaptype, scan_low, scan_high, size);
            swap_cnt = 1u;
            scan_low += size;
            scan_high -= size;
        }
        const u32 end = base + count * size;
        if (swap_cnt == 0u) {  // no element equalled the pivot: finish with an insertion sort
            insertion_sort(rt, ctx, compare_at, base, count, size, swaptype);
            return;
        }
        u32 span = scan_low - first_equal < first_equal - base ? scan_low - first_equal : first_equal - base;
        vec_swap(rt, swaptype, base, scan_low - span, span);
        span = last_equal - scan_high < end - last_equal - size ? last_equal - scan_high : end - last_equal - size;
        vec_swap(rt, swaptype, scan_low, end - span, span);
        u32 rest = scan_low - first_equal;
        if (rest > size) {
            // The original `jal`s itself; on a case the dispatcher runs whichever version is registered.
            if (size != 0u) (void)call(rt, ctx, 0x08B7A3BCu, {base, rest / size, size, compare_at});
        }
        rest = last_equal - scan_high;
        if (rest > size) {
            // Loop on the larger half instead of recursing; the original divides by `size` (a trap when it is 0).
            base = end - rest;
            count = size != 0u ? rest / size : 0u;
            continue;
        }
        return;
    }
}

} // namespace
} // namespace new_batch_main_1052
} // namespace gt::game

#define NEW1052(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1052::name, gt::ReturnKind::kind)
NEW1052(0x08B7A3BCu, func_08B7A3BC, Void);
