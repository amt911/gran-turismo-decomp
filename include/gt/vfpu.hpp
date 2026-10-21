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
#include <cmath>
#include <cstdint>
#include <limits>

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

// vadd / vsub / vmul / vdiv destination, source, target over `length` lanes, with the source and destination prefixes.
enum class Binary { Add, Sub, Mul, Div };
inline void binary(psprecomp::AllegrexContext &ctx, Binary operation, std::uint32_t destination, std::uint32_t source,
                   std::uint32_t target, std::uint32_t length) {
    float s[4]{}, t[4]{}, d[4]{};
    ctx.read_vfpu_vector_with_source_prefix(s, source, length, 0u);
    ctx.read_vfpu_vector_with_source_prefix(t, target, length, 1u);
    for (std::uint32_t i = 0u; i < length; ++i) {
        switch (operation) {
        case Binary::Add: d[i] = s[i] + t[i]; break;
        case Binary::Sub: d[i] = s[i] - t[i]; break;
        case Binary::Mul: d[i] = s[i] * t[i]; break;
        case Binary::Div: d[i] = s[i] / t[i]; break;
        }
    }
    ctx.write_vfpu_vector_with_destination_prefix(d, destination, length);
}

// The one-operand VFPU operations, by their instruction's operation field (vmov 0, vabs 1, vneg 2, vsat0 4, vsat1 5,
// vrcp 16, vrsq 17, vsin 18, vcos 19, vexp2 20, vlog2 21, vsqrt 22, vasin 23, vnrcp 24, vnsin 26, vrexp2 otherwise).
inline void unary(psprecomp::AllegrexContext &ctx, std::uint32_t operation, std::uint32_t destination,
                  std::uint32_t source, std::uint32_t length) {
    float s[4]{}, d[4]{};
    ctx.read_vfpu_vector_with_source_prefix(s, source, length, 0u);
    for (std::uint32_t i = 0u; i < length; ++i) {
        const float x = s[i];
        switch (operation) {
        case 0u: d[i] = x; break;
        case 1u: d[i] = std::fabs(x); break;
        case 2u: d[i] = -x; break;
        case 4u: d[i] = x <= 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); break;
        case 5u: d[i] = x < -1.0f ? -1.0f : (x > 1.0f ? 1.0f : x); break;
        case 16u: d[i] = 1.0f / x; break;
        case 17u: d[i] = 1.0f / std::sqrt(x); break;
        case 18u: d[i] = std::sin(x * 1.57079632679489661923f); break;
        case 19u: d[i] = std::cos(x * 1.57079632679489661923f); break;
        case 20u: d[i] = std::exp2(x); break;
        case 21u: d[i] = std::log2(x); break;
        case 22u: d[i] = std::fabs(std::sqrt(x)); break;
        case 23u: d[i] = std::asin(x) * 0.63661977236758134308f; break;
        case 24u: d[i] = -1.0f / x; break;
        case 26u: d[i] = -std::sin(x * 1.57079632679489661923f); break;
        default: d[i] = 1.0f / std::exp2(x); break;
        }
    }
    ctx.write_vfpu_vector_with_destination_prefix(d, destination, length);
}

// vfim.s destination, half: the half-precision immediate as a float.
inline void load_half(psprecomp::AllegrexContext &ctx, std::uint32_t destination, std::uint16_t half) {
    const std::uint32_t sign = static_cast<std::uint32_t>(half & 0x8000u) << 16u;
    const std::uint32_t exponent = (half >> 10u) & 0x1Fu;
    std::uint32_t mantissa = half & 0x03FFu;
    std::uint32_t bits = 0u;
    if (exponent == 0u) {
        if (mantissa == 0u) {
            bits = sign;
        } else {
            std::uint32_t shift = 0u;
            while ((mantissa & 0x0400u) == 0u) {
                mantissa <<= 1u;
                ++shift;
            }
            mantissa &= 0x03FFu;
            bits = sign | ((113u - shift) << 23u) | (mantissa << 13u);
        }
    } else if (exponent == 31u) {
        bits = sign | 0x7F800000u | (mantissa << 13u);
    } else {
        bits = sign | ((exponent + 112u) << 23u) | (mantissa << 13u);
    }
    const float value[1]{std::bit_cast<float>(bits)};
    ctx.write_vfpu_vector_with_destination_prefix(value, destination, 1u);
}

// vf2in / vf2iz / vf2iu / vf2id (mode 16 / 17 / 18 / 19): float lanes times 2^immediate to integers (bit patterns).
inline void float_to_int(psprecomp::AllegrexContext &ctx, std::uint32_t mode, std::uint32_t destination,
                         std::uint32_t source, std::uint32_t immediate, std::uint32_t length) {
    float s[4]{};
    std::int32_t d[4]{};
    ctx.read_vfpu_vector(s, source, length);
    ctx.apply_vfpu_source_prefix(s, length, 0u);
    const float scale = std::ldexp(1.0f, static_cast<int>(immediate));
    for (std::uint32_t i = 0u; i < length; ++i) {
        if (std::isnan(s[i])) {
            d[i] = std::numeric_limits<std::int32_t>::max();
            continue;
        }
        const double scaled = static_cast<double>(s[i] * scale);
        if (scaled > static_cast<double>(std::numeric_limits<std::int32_t>::max())) {
            d[i] = std::numeric_limits<std::int32_t>::max();
        } else if (scaled <= static_cast<double>(std::numeric_limits<std::int32_t>::min())) {
            d[i] = std::numeric_limits<std::int32_t>::min();
        } else {
            double rounded = 0.0;
            switch (mode) {
            case 16u: rounded = psprecomp::AllegrexContext::round_ties_to_even(scaled); break;
            case 17u: rounded = std::trunc(scaled); break;
            case 18u: rounded = std::ceil(scaled); break;
            default: rounded = std::floor(scaled); break;
            }
            d[i] = static_cast<std::int32_t>(rounded);
        }
    }
    const std::uint32_t prefix = ctx.vfpu_ctrl[2];
    for (std::uint32_t i = 0u; i < length; ++i) {
        if (((prefix >> (8u + i)) & 1u) == 0u)
            ctx.vfpu[psprecomp::AllegrexContext::vfpu_vector_lane_index(destination, length, i)] =
                std::bit_cast<float>(static_cast<std::uint32_t>(d[i]));
    }
    ctx.eat_vfpu_prefixes();
}

// vi2f destination, source, immediate: integer lanes (bit patterns) to floats times 2^-immediate.
inline void int_to_float(psprecomp::AllegrexContext &ctx, std::uint32_t destination, std::uint32_t source,
                         std::uint32_t immediate, std::uint32_t length) {
    float s[4]{}, d[4]{};
    ctx.read_vfpu_vector(s, source, length);
    ctx.apply_vfpu_source_prefix(s, length, 0u);
    const float scale = std::ldexp(1.0f, -static_cast<int>(immediate));
    for (std::uint32_t i = 0u; i < length; ++i)
        d[i] = static_cast<float>(static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(s[i]))) * scale;
    ctx.write_vfpu_vector_with_destination_prefix(d, destination, length);
}

// vi2uc / vi2c / vi2us / vi2s (operation 0 / 1 / 2 / 3): integer lanes packed into bytes or halfwords.
inline void pack_ints(psprecomp::AllegrexContext &ctx, std::uint32_t operation, std::uint32_t destination,
                      std::uint32_t source, std::uint32_t length) {
    const bool to_bytes = operation < 2u;
    const std::uint32_t out_length = to_bytes ? 1u : (length + 1u) / 2u;
    float s[4]{}, d[4]{};
    std::uint32_t w[4]{};
    ctx.read_vfpu_vector(s, source, length);
    ctx.apply_vfpu_source_prefix(s, length, 0u);
    for (std::uint32_t i = 0u; i < length; ++i) {
        const auto x = static_cast<std::int32_t>(std::bit_cast<std::uint32_t>(s[i]));
        std::uint32_t v = 0u;
        switch (operation) {
        case 0u: v = x < 0 ? 0u : (static_cast<std::uint32_t>(x) >> 23u) & 0xFFu; break;
        case 1u: v = (static_cast<std::uint32_t>(x) >> 24u) & 0xFFu; break;
        case 2u: v = x < 0 ? 0u : (static_cast<std::uint32_t>(x) >> 15u) & 0xFFFFu; break;
        default: v = (static_cast<std::uint32_t>(x) >> 16u) & 0xFFFFu; break;
        }
        if (to_bytes) w[0] |= v << (8u * i);
        else w[i / 2u] |= v << (16u * (i % 2u));
    }
    for (std::uint32_t i = 0u; i < out_length; ++i) d[i] = std::bit_cast<float>(w[i]);
    ctx.write_vfpu_vector_with_destination_prefix(d, destination, out_length);
}

// vscl destination, source, scalar.
inline void scale(psprecomp::AllegrexContext &ctx, std::uint32_t destination, std::uint32_t source,
                  std::uint32_t scalar, std::uint32_t length) {
    ctx.execute_vfpu_vscl(destination, source, scalar, length);
}

// vrot destination, angle scalar, immediate (which lanes take sin, cos and zero).
inline void rotation(psprecomp::AllegrexContext &ctx, std::uint32_t destination, std::uint32_t source,
                     std::uint32_t length, std::uint32_t immediate) {
    ctx.execute_vfpu_vrot(destination, source, length, immediate);
}

// vmmul destination, source, target (side x side): destination = source x target as the VFPU lays them out.
inline void matrix_multiply(psprecomp::AllegrexContext &ctx, std::uint32_t destination, std::uint32_t source,
                            std::uint32_t target, std::uint32_t side) {
    float s[16]{}, t[16]{}, d[16]{};
    ctx.read_vfpu_matrix(s, source, side);
    ctx.read_vfpu_matrix(t, target, side);
    for (std::uint32_t a = 0u; a < side; ++a) {
        for (std::uint32_t b = 0u; b < side; ++b) {
            float sum = 0.0f;
            for (std::uint32_t c = 0u; c < side; ++c) sum += s[b * 4u + c] * t[a * 4u + c];
            d[a * 4u + b] = sum;
        }
    }
    ctx.write_vfpu_matrix(d, destination, side);
    ctx.eat_vfpu_prefixes();
}

} // namespace gt::vfpu
