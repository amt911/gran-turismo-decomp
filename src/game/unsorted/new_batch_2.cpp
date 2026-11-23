// New batch 2: leaf functions the routes never reach, written as structured C from `tools.disasm` and verified by synthetic
// cases: the C library's strtol / strtoul (the reentrant forms: a0 = the errno cell) and a type-size switch.
// `v0` ends as the asm leaves it. No names beyond the C library's: the shape (whitespace table at 0x08C96BD9, the 0x / 0 prefixes,
// the cutoff by division, errno 0x22 = ERANGE) is strtol's.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace new_batch_2 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u64 = std::uint64_t;
using s64 = std::int64_t;

constexpr u32 kCharacterTypes = 0x08C96BD9u;  // byte table indexed by the (signed) character: 1 upper, 2 lower, 4 digit, 8 space

// The reentrant strtol (signed) / strtoul (unsigned): a0 = the errno cell, a1 = the text, a2 = where to store the end pointer (or 0),
// a3 = the base (0 chooses from the prefix). Overflow stores 0x22 at the errno cell and returns the limit.
void convert(Runtime &rt, AllegrexContext &ctx, bool is_signed) {
    const u32 errno_cell = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 end_cell = ctx.gpr[6];
    u32 base = ctx.gpr[7];
    const auto character = [&](u32 address) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address)))); };
    const auto kind = [&](u32 c) { return static_cast<u32>(rt.memory().load8(kCharacterTypes + c)); };

    u32 at = text;
    u32 c;
    u32 next;
    for (;;) {
        c = character(at);
        next = at + 1u;
        if ((kind(c) & 8u) == 0u) break;
        at = next;
    }
    bool negative = false;
    if (c == '-') {
        negative = true;
        c = character(next);
        next = at + 2u;
    } else if (c == '+') {
        c = character(next);
        next = at + 2u;
    }
    u32 radix = base;
    if (base == 0u || base == 16u) {
        bool prefixed = false;
        if (c == '0') {
            const u32 d = character(next);
            if (d == 'x' || d == 'X') {
                c = character(next + 1u);
                base = 16u;
                radix = 16u;
                next += 2u;
                prefixed = true;
            }
        }
        if (!prefixed) {
            if (base != 0u) {
                radix = base;
            } else {
                base = (c == '0') ? 8u : 10u;
                radix = base;
            }
        }
    }
    const u32 limit = is_signed ? (negative ? 0x80000000u : 0x7FFFFFFFu) : 0xFFFFFFFFu;
    const u32 cutoff = limit / radix;
    const u32 cutlim = limit % radix;
    ctx.hi = cutlim;
    ctx.lo = cutoff;
    u32 accumulator = 0u;
    s32 any = 0;
    for (;;) {
        const u32 t = kind(c);
        u32 digit;
        if (t & 4u) {
            digit = c - '0';
        } else if (t & 3u) {
            digit = c - ((t & 1u) ? 0x37u : 0x57u);
        } else {
            break;
        }
        if (!(static_cast<s32>(digit) < static_cast<s32>(base))) break;
        if (any >= 0) {
            if (accumulator == cutoff) ctx.lo = digit;  // the `mtlo` in the delay slot of the equality branch
            if (accumulator > cutoff || (accumulator == cutoff && static_cast<s32>(cutlim) < static_cast<s32>(digit))) {
                any = -1;
            } else {
                // mtlo digit; madd accumulator, radix: hi:lo = hi:digit + accumulator * radix
                const u64 before = (static_cast<u64>(ctx.hi) << 32) | digit;
                const u64 sum = before + static_cast<u64>(static_cast<s64>(static_cast<s32>(accumulator)) * static_cast<s64>(static_cast<s32>(radix)));
                ctx.hi = static_cast<u32>(sum >> 32);
                ctx.lo = static_cast<u32>(sum);
                accumulator = ctx.lo;
                any = 1;
            }
        }
        c = character(next);
        ++next;
    }
    u32 result;
    if (any < 0) {
        result = is_signed ? (negative ? 0x80000000u : 0x7FFFFFFFu) : 0xFFFFFFFFu;
        put(rt, errno_cell, 0x22u);
    } else {
        result = negative ? 0u - accumulator : accumulator;
    }
    if (end_cell != 0u) put(rt, end_cell, any != 0 ? next - 1u : text);
    ctx.gpr[2] = result;
}

void func_08B7DD9C(Runtime &rt, AllegrexContext &ctx) { convert(rt, ctx, true); }
void func_08B7E314(Runtime &rt, AllegrexContext &ctx) { convert(rt, ctx, false); }

// 0x08B75C8C (a0 = a type code): the size of a stored value for the low three bits of the code (0 and 3: 4, 2: 2, 4: 8) and 0 for
// the code 0xFF; any other code goes to 0x08B77B04 (the fatal path).
void func_08B75C8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 code = ctx.gpr[4] & 0xFFu;
    if (code == 0xFFu) {
        ctx.gpr[2] = 0u;
        return;
    }
    switch (code & 7u) {
    case 0u:
    case 3u:
        ctx.gpr[2] = 4u;
        return;
    case 2u:
        ctx.gpr[2] = 2u;
        return;
    case 4u:
        ctx.gpr[2] = 8u;
        return;
    default:
        break;
    }
    ctx.gpr[2] = (code & 7u) == 1u ? 3u : 4u;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x08B77B04u, {});
}

} // namespace
} // namespace new_batch_2
} // namespace gt::game

#define NEW2(address, name, kind) GT_DECOMP(address, gt::game::new_batch_2::name, gt::ReturnKind::kind)
NEW2(0x08B7DD9Cu, func_08B7DD9C, Word);
NEW2(0x08B7E314u, func_08B7E314, Word);
NEW2(0x08B75C8Cu, func_08B75C8C, Word);
