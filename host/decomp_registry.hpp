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
#include <vector>

namespace gt {

// Same shape as a recompiled guest function: arguments come in ctx (a0-a3, t0-t3), results go back
// into ctx (v0...) and guest memory. The harness handles returning to the caller (ctx.pc = ra).
using DecompiledFn = void (*)(psprecomp::Runtime &, psprecomp::AllegrexContext &);

struct DecompiledFunction {
    std::uint32_t address;
    const char *name;
    DecompiledFn fn;
    ReturnKind returns;
};

std::vector<DecompiledFunction> &decompiled_functions();

struct DecompRegistrar {
    DecompRegistrar(std::uint32_t address, const char *name, DecompiledFn fn, ReturnKind returns) {
        decompiled_functions().push_back({address, name, fn, returns});
    }
};

} // namespace gt

#define GT_DECOMP_CAT_(a, b) a##b
#define GT_DECOMP_CAT(a, b) GT_DECOMP_CAT_(a, b)
#define GT_DECOMP(address, fn, returns) \
    static const ::gt::DecompRegistrar GT_DECOMP_CAT(gt_decomp_registrar_, __LINE__){(address), #fn, &fn, (returns)}
