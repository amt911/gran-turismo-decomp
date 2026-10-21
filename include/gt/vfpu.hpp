// The VFPU as decompiled code sees it while it runs next to recompiled code: the register file lives in
// psprecomp::AllegrexContext::vfpu (matrix * 16 + column * 4 + row), and no calling convention saves it. GT's graphics
// library leaves vectors and matrices there for later calls, so a decompiled function that loads one must leave it too
// (the differential harness compares the VFPU; host/diff_compare.cpp). Portable C++, no inline VFPU (decomp-workflow).
#pragma once

#include "psprecomp/allegrex_context.hpp"
#include "psprecomp/runtime.hpp"

#include <bit>
#include <cstdint>

namespace gt::vfpu {

// lv.q C<matrix><column>0, (address): four words from guest memory into column `column` of matrix `matrix`.
inline void load_column(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, unsigned matrix, unsigned column,
                        std::uint32_t address) {
    for (unsigned row = 0u; row < 4u; ++row)
        ctx.vfpu[matrix * 16u + column * 4u + row] = std::bit_cast<float>(rt.memory().load32(address + 4u * row));
}

} // namespace gt::vfpu
