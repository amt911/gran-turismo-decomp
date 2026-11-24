// Main-session batch 150. Functions written by hand from `tools.disasm` as structured C (draft: the differential gate
// runs after this batch is handed over). No names: none has evidence.
//
// 0x088B75D8 is the 4x4 matrix routine in front of 0x088B5ED4: it turns the 3x3 part of a2 and then of a1 into the block
// the original builds on its own stack, multiplies each block by the matrix a3 (row-major, the dot products summed in k
// order), writes the first product into eight floats plus a 4 at +0x40 of the object a0, scales two words of the second
// product by a1[0x24] + 0.05f, and hands the object, that block and a1[0x2C] / a1[0x38] (in $f12 / $f13) to 0x088B5ED4.
#include "decomp_registry.hpp"
#include "guest_call.hpp"
#include "gt/game/hot_common.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace new_batch_main_150 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// A 4x4 block of guest floats, read in one go (element (i, j) at 16 * i + 4 * j).
void load_matrix(Runtime &rt, u32 address, float matrix[16]) {
    for (unsigned i = 0; i < 16u; ++i) matrix[i] = float_at(rt, address + 4u * i);
}

// out = a * b for two row-major 4x4 blocks. The original's schedule interleaves the sixteen dot products across its
// registers and spills, but each of them is the same left-to-right chain over k (a mul.s per term, then add.s in k
// order), so computing them one after the other gives the same values.
void multiply(const float a[16], const float b[16], float out[16]) {
    for (unsigned row = 0; row < 4u; ++row) {
        for (unsigned column = 0; column < 4u; ++column) {
            float sum = psp_mul(a[row * 4u], b[column]);
            sum = sum + psp_mul(a[row * 4u + 1u], b[4u + column]);
            sum = sum + psp_mul(a[row * 4u + 2u], b[8u + column]);
            sum = sum + psp_mul(a[row * 4u + 3u], b[12u + column]);
            out[row * 4u + column] = sum;
        }
    }
}

// The block the original lays out at sp+0xC0 before each multiply (0x088B7634..0x088B76B8 and 0x088B7ADC..0x088B7B70):
// m[0x0C..0x20] in the first two row groups with their three cross products in between, then m[0x00..0x08] and a 1.
void cofactor_block(const float m[16], float block[16]) {
    block[0] = m[3];
    block[1] = m[4];
    block[2] = m[5];
    block[3] = 0.0f;
    block[4] = psp_mul(m[4], m[8]) - psp_mul(m[5], m[7]);
    block[5] = psp_mul(m[5], m[6]) - psp_mul(m[8], m[3]);
    block[6] = psp_mul(m[7], m[3]) - psp_mul(m[4], m[6]);
    block[7] = 0.0f;
    block[8] = m[6];
    block[9] = m[7];
    block[10] = m[8];
    block[11] = 0.0f;
    block[12] = m[0];
    block[13] = m[1];
    block[14] = m[2];
    block[15] = 1.0f;
}

// The eight floats and the 4 the original stores into the object a0 (0x088B7A30..0x088B7AD8), built from every second
// element of the first product block against three floats of the first matrix. The three floats are read before any store,
// so an object that overlaps that matrix sees the original's order.
void write_object(Runtime &rt, u32 out, const float product[16], u32 first) {
    const float scale = float_at(rt, first + 0x24u);
    const float cosine = float_at(rt, first + 0x2Cu);
    const float sine = float_at(rt, first + 0x38u);
    const float p0 = product[0];
    const float p2 = product[2];
    const float p8 = product[8];
    const float p10 = product[10];
    const float p12 = product[12];
    const float p14 = product[14];
    put_float(rt, out + 0x00u, (p12 + psp_mul(p0, scale)) + psp_mul(cosine, p8));
    put_float(rt, out + 0x04u, (p14 + psp_mul(p2, scale)) + psp_mul(cosine, p10));
    put_float(rt, out + 0x08u, (p12 + psp_mul(p0, scale)) - psp_mul(sine, p8));
    put_float(rt, out + 0x0Cu, (p14 + psp_mul(p2, scale)) - psp_mul(sine, p10));
    put_float(rt, out + 0x10u, (p12 - psp_mul(p0, scale)) - psp_mul(sine, p8));
    put_float(rt, out + 0x14u, (p14 - psp_mul(p2, scale)) - psp_mul(sine, p10));
    put_float(rt, out + 0x18u, (p12 - psp_mul(p0, scale)) + psp_mul(cosine, p8));
    put_float(rt, out + 0x1Cu, (p14 - psp_mul(p2, scale)) + psp_mul(cosine, p10));
    put(rt, out + 0x40u, 4u);
}

// 0x088B75D8 (a0 = the object written, a1 = the transform read, a2 and a3 = two 4x4 matrices): the two products, the
// object, and 0x088B5ED4(object, second block, $f12 = a1[0x2C], $f13 = a1[0x38]) as the last call — its v0 is the result.
void func_088B75D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 transform = ctx.gpr[5];
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    GuestFrame frame(ctx, 0x1B0u);

    float matrix[16];
    float block[16];
    float product[16];

    load_matrix(rt, first, matrix);
    cofactor_block(matrix, block);
    load_matrix(rt, second, matrix);
    multiply(block, matrix, product);
    write_object(rt, object, product, first);

    load_matrix(rt, transform, matrix);
    cofactor_block(matrix, block);
    load_matrix(rt, second, matrix);
    multiply(block, matrix, product);

    // The second product goes to sp+0x80 as the block 0x088B5ED4 receives.
    for (unsigned i = 0; i < 16u; ++i) put_float(rt, frame.at(0x80u + 4u * i), product[i]);
    const float scale = float_at(rt, transform + 0x24u) + 0.05f;
    put_float(rt, frame.at(0x80u), psp_mul(product[0], scale));
    put_float(rt, frame.at(0x88u), psp_mul(product[2], scale));
    // The original's copy loop leaves the block's last word in v0, so a callee that never writes v0 returns that word.
    ctx.gpr[2] = std::bit_cast<u32>(product[15]);
    ctx.gpr[2] = call_guest(rt, ctx, 0x088B5ED4u, {object, frame.at(0x80u)},
                            {float_at(rt, transform + 0x2Cu), float_at(rt, transform + 0x38u)})
                     .v0;
}

} // namespace
} // namespace new_batch_main_150
} // namespace gt::game

#define MAIN150(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_150::name, gt::ReturnKind::kind)
MAIN150(0x088B75D8u, func_088B75D8, Word);
