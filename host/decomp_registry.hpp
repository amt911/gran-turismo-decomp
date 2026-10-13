// Registry of decompiled functions. A function in src/ registers itself with
//
//     GT_DECOMP(0x08B36018u, gt::libc::memset, gt::ReturnKind::Word);
//
// (runtime address, the native implementation, what it returns). gt_native then runs it according
// to --decomp off|on|diff (host/diff_harness.cpp).
#pragma once

#include "diff_compare.hpp"
#include "psprecomp/runtime.hpp"

#include <cstdint>
#include <initializer_list>
#include <vector>

namespace gt {

// Same shape as a recompiled guest function: arguments come in ctx (a0-a3, t0-t3), results go back
// into ctx (v0...) and guest memory. The harness handles returning to the caller (ctx.pc = ra),
// unless the function ends with a tail call (tail_call below): then the guest code at the target
// runs next and returns to the caller itself — no native frame stays behind, so the target may block.
using DecompiledFn = void (*)(psprecomp::Runtime &, psprecomp::AllegrexContext &);

// Ends a decompiled function with a jump to the guest function at `address` (the original's `j` /
// `jr $t9` to another function): arguments go in a0-a3, t0-t3, ra is left alone. Call it last.
inline void tail_call(psprecomp::AllegrexContext &ctx, std::uint32_t address,
                      std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.pc = address;
}

struct DecompiledFunction {
    std::uint32_t address;
    const char *name;
    DecompiledFn fn;
    ReturnKind returns;
    // Runs decompiled inside other decompiled functions' cases too (host/diff_harness.cpp), so it is verified
    // through its callers. Only for functions whose whole return contract is right — every register a caller
    // reads after the call (v0 of a "Void" one included) — or a caller's case mismatches for nothing it did.
    bool nested = false;
};

std::vector<DecompiledFunction> &decompiled_functions();

struct DecompRegistrar {
    DecompRegistrar(std::uint32_t address, const char *name, DecompiledFn fn, ReturnKind returns, bool nested = false) {
        decompiled_functions().push_back({address, name, fn, returns, nested});
    }
};

} // namespace gt

#define GT_DECOMP_CAT_(a, b) a##b
#define GT_DECOMP_CAT(a, b) GT_DECOMP_CAT_(a, b)
#define GT_DECOMP(address, fn, returns) \
    static const ::gt::DecompRegistrar GT_DECOMP_CAT(gt_decomp_registrar_, __LINE__){(address), #fn, &fn, (returns)}
// The same, with its return contract audited: it also runs decompiled when nested in another case (see `nested`).
#define GT_DECOMP_NESTED(address, fn, returns) \
    static const ::gt::DecompRegistrar GT_DECOMP_CAT(gt_decomp_registrar_, __LINE__){(address), #fn, &fn, (returns), true}
