// The VFPU as decompiled code sees it while it runs next to recompiled code: the register file lives in
// psprecomp::AllegrexContext (vfpu, vfpu_ctrl), and no calling convention saves it. GT's graphics library leaves vectors
// and matrices there for later calls, so a decompiled function leaves the registers the original leaves (the
// differential harness compares the VFPU; host/diff_compare.cpp). No inline VFPU (decomp-workflow): each helper is one
// VFPU operation with the exact semantics the recompiler gives it, so the two agree bit for bit. The bodies follow
// lcs-recomp's tools/codegen_main.cpp (MIT, Copyright (c) the lcs-recomp authors) and use the context's own accessors
// (read/write_vfpu_vector, read_vfpu_matrix, prefixes), until stage 2 of the no-emulation plan retypes the VFPU state.
//
// Register numbers are the instruction's 7-bit fields: a quad column C<m><c>0 is m * 4 + c, a transposed matrix E<m>00
// is m * 4 + 0x20; a scalar S<m><c><r> is m * 4 + c + r * 0x20.
#pragma once

#include "psprecomp/allegrex_context.hpp"
#include "psprecomp/runtime.hpp"

#include <bit>
#include <cstdint>

namespace gt::vfpu {

// lv.q <vector>, (address): four words from guest memory.
inline void load_quad(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vector,
                      std::uint32_t address) {
    float value[4];
    for (unsigned i = 0u; i < 4u; ++i) value[i] = std::bit_cast<float>(rt.memory().load32(address + 4u * i));
    ctx.write_vfpu_vector(value, vector, 4u);
}

// lv.q C<matrix><column>0, (address).
inline void load_column(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, unsigned matrix, unsigned column,
                        std::uint32_t address) {
    load_quad(rt, ctx, matrix * 4u + column, address);
}

// sv.q <vector>, (address).
inline void store_quad(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vector,
                       std::uint32_t address) {
    float value[4]{};
    ctx.read_vfpu_vector(value, vector, 4u);
    for (unsigned i = 0u; i < 4u; ++i) rt.memory().store32(address + 4u * i, std::bit_cast<std::uint32_t>(value[i]));
}

// svl.q <vector>, (address): words 3..(3 - k) to address, address - 4, ... (k = the address's word in its 16-byte line).
inline void store_quad_left(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vector,
                            std::uint32_t address) {
    const std::uint32_t word = (address >> 2u) & 3u;
    float value[4]{};
    ctx.read_vfpu_vector(value, vector, 4u);
    for (std::uint32_t i = 0u; i <= word; ++i)
        rt.memory().store32(address - 4u * i, std::bit_cast<std::uint32_t>(value[3u - i]));
}

// svr.q <vector>, (address): words 0..(3 - k) to address, address + 4, ...
inline void store_quad_right(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vector,
                             std::uint32_t address) {
    const std::uint32_t word = (address >> 2u) & 3u;
    float value[4]{};
    ctx.read_vfpu_vector(value, vector, 4u);
    for (std::uint32_t i = 0u; i < 4u - word; ++i)
        rt.memory().store32(address + 4u * i, std::bit_cast<std::uint32_t>(value[i]));
}

// lv.s <scalar>, (address) / sv.s <scalar>, (address).
inline void load_scalar(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t scalar,
                        std::uint32_t address) {
    ctx.set_vfpu_scalar_bits(scalar, rt.memory().load32(address));
}
inline void store_scalar(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t scalar,
                         std::uint32_t address) {
    rt.memory().store32(address, ctx.vfpu_scalar_bits(scalar));
}

// mtv <bits>, <scalar>.
inline void move_to(psprecomp::AllegrexContext &ctx, std::uint32_t bits, std::uint32_t scalar) {
    ctx.set_vfpu_scalar_bits(scalar, bits);
}

// vmidt / vmzero / vmone (operation 3 / 6 / 7) on a side x side matrix.
inline void matrix_init(psprecomp::AllegrexContext &ctx, std::uint32_t matrix, std::uint32_t side,
                        std::uint32_t operation) {
    ctx.execute_vfpu_matrix_init(matrix, side, operation);
}

// vtfm<side>.<input> / vhtfm<side>.<input> destination, matrix, vector: matrix x vector, the vector padded to `side`
// lanes with zeros and, when shorter than `side`, a final 1 (homogeneous); the last row takes the source prefixes.
inline void transform(psprecomp::AllegrexContext &ctx, std::uint32_t destination, std::uint32_t matrix_register,
                      std::uint32_t vector, std::uint32_t side, std::uint32_t input_length) {
    float matrix[16]{}, raw[4]{}, target[4]{}, result[4]{};
    ctx.read_vfpu_matrix(matrix, matrix_register, side);
    ctx.read_vfpu_vector(raw, vector, side);
    for (std::uint32_t i = 0u; i < 4u; ++i) target[i] = i < input_length ? raw[i] : 0.0f;
    if (side - 1u >= input_length) target[side - 1u] = 1.0f;
    for (std::uint32_t row = 0u; row + 1u < side; ++row) {
        float sum = 0.0f;
        for (std::uint32_t column = 0u; column < side; ++column) sum += matrix[row * 4u + column] * target[column];
        result[row] = sum;
    }
    float final_row[4]{matrix[(side - 1u) * 4u + 0u], matrix[(side - 1u) * 4u + 1u], matrix[(side - 1u) * 4u + 2u],
                       matrix[(side - 1u) * 4u + 3u]};
    ctx.apply_vfpu_source_prefix(final_row, 4u, 0u);
    ctx.apply_vfpu_source_prefix(target, 4u, 1u);
    for (std::uint32_t column = 0u; column < 4u; ++column) result[side - 1u] += final_row[column] * target[column];
    const std::uint32_t prefix = ctx.vfpu_ctrl[2];
    const std::uint32_t last = side - 1u;
    ctx.vfpu_ctrl[2] = ((prefix & (1u << 8u)) << last) | ((prefix & 3u) << (last * 2u));
    ctx.write_vfpu_vector_with_destination_prefix(result, destination, side);
}

} // namespace gt::vfpu
