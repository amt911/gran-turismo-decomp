// Hot batch 341: the graphics library's matrix-stack helpers (a state block with a table of matrix pointers at +0xD00, the
// stack index at +0xD10, dirty-flag words at +0xD18 / +0xD1C and the words they are merged from at +0xD20 / +0xD24), written
// as structured C from `tools.disasm` and verified by synthetic cases. The VFPU is touched only through include/gt/vfpu.hpp,
// operation by operation, because the harness compares the register file.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_341 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
namespace v = gt::vfpu;

constexpr u32 kSlotTable = 0xD00u;   // pointers to the matrices of the stack (and, at +4 / +8 / +0xC, to three more)
constexpr u32 kStackIndex = 0xD10u;
constexpr u32 kPendingLow = 0xD18u;
constexpr u32 kPendingHigh = 0xD1Cu;
constexpr u32 kMergeLow = 0xD20u;
constexpr u32 kMergeHigh = 0xD24u;

// A guest call; afterwards v0 / v1 hold its results, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The matrix the stack index points at: *(block + 0xD00 + index * 4).
u32 current_matrix(Runtime &rt, u32 block) { return word(rt, block + (word(rt, block + kStackIndex) << 2) + kSlotTable); }

// lv.q C<m>00 .. C<m>30 from four consecutive quads.
void load_matrix(Runtime &rt, AllegrexContext &ctx, unsigned matrix, u32 address) {
    for (unsigned column = 0u; column < 4u; ++column) v::load_column(rt, ctx, matrix, column, address + 16u * column);
}
// sv.q C<m>00 .. C<m>30 to four consecutive quads.
void store_columns(Runtime &rt, AllegrexContext &ctx, unsigned matrix, u32 address) {
    for (unsigned column = 0u; column < 4u; ++column) v::store_quad(rt, ctx, matrix * 4u + column, address + 16u * column);
}
// sv.q R<m>00 .. R<m>30 (the rows of matrix m, i.e. its transpose) to four consecutive quads.
void store_rows(Runtime &rt, AllegrexContext &ctx, unsigned matrix, u32 address) {
    for (unsigned row = 0u; row < 4u; ++row) v::store_quad(rt, ctx, 0x20u + matrix * 4u + row, address + 16u * row);
}

// Merges the pending words (+0xD18 / +0xD1C) with the ones at +0xD20 / +0xD24 and stores the result back; v0 / v1 keep them.
void merge_pending(Runtime &rt, AllegrexContext &ctx, u32 block) {
    const u32 low = word(rt, block + kPendingLow) | word(rt, block + kMergeLow);
    const u32 high = word(rt, block + kPendingHigh) | word(rt, block + kMergeHigh);
    put(rt, block + kPendingLow, low);
    put(rt, block + kPendingHigh, high);
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

// When `bit` of the pending word +0xD1C is set: clears it, multiplies the matrices *(block + left) x *(block + right) (as
// vmmul M000, E200, M100) into block + destination. v0 is block + destination either way.
void product_when_pending(Runtime &rt, AllegrexContext &ctx, u32 bit, u32 destination, u32 left, u32 right) {
    const u32 block = ctx.gpr[4];
    const u32 flags = word(rt, block + kPendingHigh);
    ctx.gpr[2] = block + destination;
    ctx.gpr[3] = flags & bit;
    if ((flags & bit) == 0u) return;
    const u32 left_matrix = word(rt, block + left);
    const u32 right_matrix = word(rt, block + right);
    put(rt, block + kPendingHigh, flags & ~bit);
    load_matrix(rt, ctx, 2u, left_matrix);
    load_matrix(rt, ctx, 1u, right_matrix);
    v::matrix_multiply(ctx, 0u, 0x28u, 4u, 4u);
    store_columns(rt, ctx, 0u, block + destination);
}

// 0x08B563E0: transposes the current matrix in place and merges the pending words.
void func_08B563E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    const u32 matrix = current_matrix(rt, block);
    load_matrix(rt, ctx, 0u, matrix);
    merge_pending(rt, ctx, block);
    store_rows(rt, ctx, 0u, matrix);
}

// 0x08B564C8: overwrites the current matrix with the identity (vmidt M000) and merges the pending words; v0 is the matrix.
void func_08B564C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    const u32 matrix = current_matrix(rt, block);
    v::matrix_init(ctx, 0u, 4u, 3u);
    merge_pending(rt, ctx, block);
    ctx.gpr[2] = matrix;
    store_columns(rt, ctx, 0u, matrix);
}

// 0x08B56CF4: when bit 8 is set, matrix at +0x780 = *(+0xD04) x the matrix at +0x700 (vmmul M000, E200, M100).
void func_08B56CF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    const u32 flags = word(rt, block + kPendingHigh);
    ctx.gpr[2] = block + 0x780u;
    ctx.gpr[3] = flags & 0x100u;
    if ((flags & 0x100u) == 0u) return;
    const u32 left_matrix = word(rt, block + 0xD04u);
    load_matrix(rt, ctx, 1u, block + 0x700u);
    put(rt, block + kPendingHigh, flags & ~0x100u);
    load_matrix(rt, ctx, 2u, left_matrix);
    v::matrix_multiply(ctx, 0u, 0x28u, 4u, 4u);
    store_columns(rt, ctx, 0u, block + 0x780u);
}

void func_08B56B10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { product_when_pending(rt, ctx, 0x1000u, 0x880u, 0xD04u, 0xD0Cu); }
void func_08B56C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { product_when_pending(rt, ctx, 0x10u, 0x680u, 0xD04u, 0xD00u); }
void func_08B56C94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { product_when_pending(rt, ctx, 0x20u, 0x6C0u, 0xD08u, 0xD00u); }
void func_08B56DC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { product_when_pending(rt, ctx, 0x800u, 0x840u, 0xD0Cu, 0xD00u); }

// 0x08B56F34: when bit 7 is set, matrix at +0x740 = (0x08B56EC0(block, 6) matrix) x *(+0xD00); v0 is block + 0x740 either way.
void func_08B56F34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[5] = 6u;
    const u32 flags = word(rt, block + kPendingHigh);
    if ((flags & 0x80u) != 0u) {
        put(rt, block + kPendingHigh, flags & ~0x80u);
        const u32 left_matrix = call(rt, ctx, 0x08B56EC0u, {block, 6u});
        load_matrix(rt, ctx, 2u, left_matrix);
        load_matrix(rt, ctx, 1u, word(rt, block + kSlotTable));
        v::matrix_multiply(ctx, 0u, 0x28u, 4u, 4u);
        store_columns(rt, ctx, 0u, block + 0x740u);
    }
    ctx.gpr[2] = block + 0x740u;
}

// 0x08B56300: transforms (f12, f13, f14, 1) by the current matrix's transpose (vhtfm4 C010, E100, C000) and stores it as the
// matrix's fourth column (+0x30); merges the pending words; v0 ends as the bits of f14, v1 as the merged high word.
void func_08B56300(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    const u32 matrix = current_matrix(rt, block);
    v::move_to(ctx, std::bit_cast<u32>(ctx.fpr[12]), 0x00u);
    load_matrix(rt, ctx, 1u, matrix);
    v::move_to(ctx, std::bit_cast<u32>(ctx.fpr[13]), 0x20u);
    v::move_to(ctx, std::bit_cast<u32>(ctx.fpr[14]), 0x40u);
    merge_pending(rt, ctx, block);
    ctx.gpr[2] = std::bit_cast<u32>(ctx.fpr[14]);   // mfc1 v0, f14 after the merge: v0 ends as the third float's bits
    v::transform(ctx, 1u, 0x24u, 0u, 4u, 3u);
    v::store_quad(rt, ctx, 1u, matrix + 0x30u);
}

// 0x08B56A88: when bit 9 is set, 0x08B55E30 builds a matrix in the frame from (0, f1, f2, 0, f3, f4) = the floats at +0xD30 /
// +0xD34 / +0xD28 / +0xD2C; its fourth column gains the third column times *(+0xD38), the third column becomes zero and the
// result is stored at +0x7C0. v0 is block + 0x7C0 either way.
void func_08B56A88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    ctx.fpr[12] = 0.0f;
    ctx.fpr[15] = 0.0f;
    ctx.gpr[4] = frame.at(0u);
    const u32 flags = word(rt, block + kPendingHigh);
    if ((flags & 0x200u) != 0u) {
        ctx.fpr[13] = float_at(rt, block + 0xD30u);
        ctx.fpr[14] = float_at(rt, block + 0xD34u);
        ctx.fpr[16] = float_at(rt, block + 0xD28u);
        ctx.fpr[17] = float_at(rt, block + 0xD2Cu);
        put(rt, block + kPendingHigh, flags & ~0x200u);
        call(rt, ctx, 0x08B55E30u, {frame.at(0u)});
        load_matrix(rt, ctx, 0u, frame.at(0u));
        v::load_scalar(rt, ctx, 4u, block + 0xD38u);
        v::scale(ctx, 4u, 2u, 4u, 4u);
        v::vector_init(ctx, 2u, 4u, false);
        v::binary(ctx, v::Binary::Add, 3u, 3u, 4u, 4u);
        store_columns(rt, ctx, 0u, block + 0x7C0u);
    }
    ctx.gpr[2] = block + 0x7C0u;
}

// 0x08B56B70: initialises the state block: the pointer table (+0xD00 the block, +0xD04 / +0xD08 / +0xD0C the matrices at +0x400 /
// +0x500 / +0x580), sixteen words cleared from +0xD3C, the pending and merge words, the stack index, 0x08B56A10 on +0xD28 / +0xD20,
// and the identity in the three matrices at +0x8C0, +0x700 and +0xA80 and in the one at +0x730.
void func_08B56B70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    put(rt, block + 0xD04u, block + 0x400u);
    put(rt, block + 0xD08u, block + 0x500u);
    put(rt, block + 0xD0Cu, block + 0x580u);
    put(rt, block + kSlotTable, block);
    for (u32 index = 0u; index != 16u; ++index) put(rt, block + 0xD3Cu + 4u * index, 0u);
    put(rt, block + kPendingLow, 0u);
    put(rt, block + kPendingHigh, 0u);
    put(rt, block + kStackIndex, 0u);
    rt.memory().store16(block + 0xD20u, 0xCB1u);
    put(rt, block + 0xD24u, 0x0CB10CB0u);
    rt.memory().store16(block + 0xD22u, 0xCB1u);
    ctx.gpr[5] = block + 0xD20u;
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = 0u;
    call(rt, ctx, 0x08B56A10u, {block + 0xD28u, block + 0xD20u, 0u, 0u});
    v::matrix_init(ctx, 0u, 4u, 3u);
    for (unsigned column = 0u; column < 4u; ++column) {
        v::store_quad(rt, ctx, column, block + 0x8C0u + 16u * column);
        v::store_quad(rt, ctx, column, block + 0x700u + 16u * column);
        v::store_quad(rt, ctx, column, block + 0xA80u + 16u * column);
    }
}

// 0x08B4E700: copies the matrix at a1 to a0 transposed.
void func_08B4E700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    load_matrix(rt, ctx, 0u, source);
    store_rows(rt, ctx, 0u, target);
}

// 0x08B5A6F0: a1 -> (x, y, z, 1) transformed by the matrix 0x08B56EC0 gives for the shared block (transposed, vtfm4 E100) and
// written to the unaligned buffer a0.
void func_08B5A6F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 point = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const u32 matrix = call(rt, ctx, 0x08B56EC0u, {kGuCommandBlock, word(rt, 0x08D6DDF0u)});
    load_matrix(rt, ctx, 1u, matrix);
    v::load_scalar(rt, ctx, 0x00u, point);
    v::load_scalar(rt, ctx, 0x20u, point + 4u);
    v::load_scalar(rt, ctx, 0x40u, point + 8u);
    v::vector_init(ctx, 0x60u, 1u, true);
    v::transform(ctx, 1u, 0x24u, 0u, 4u, 4u);
    v::store_quad_left(rt, ctx, 1u, target + 0xCu);
    v::store_quad_right(rt, ctx, 1u, target);
}

// 0x08B5A558 (w = 1) and 0x08B5A5E0 (w = 0): the same for a point a2 and the matrix 0x08B56FAC gives for (a0 = matrix, t0 != 0), with the
// plain matrix (vtfm4 M100) when a3 is non-zero and the transpose (E100, after the plain one has run) otherwise.
void transform_point(Runtime &rt, AllegrexContext &ctx, bool homogeneous_one) {
    const u32 matrix_source = ctx.gpr[4];
    const u32 target = ctx.gpr[5];
    const u32 point = ctx.gpr[6];
    const u32 plain = ctx.gpr[7] & 0xFFu;
    const u32 flag = ctx.gpr[8] & 0xFFu;
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[6] = flag != 0u ? 1u : 0u;
    const u32 matrix = call(rt, ctx, 0x08B56FACu, {kGuCommandBlock, matrix_source, flag != 0u ? 1u : 0u});
    load_matrix(rt, ctx, 1u, matrix);
    v::load_scalar(rt, ctx, 0x00u, point);
    v::load_scalar(rt, ctx, 0x20u, point + 4u);
    v::load_scalar(rt, ctx, 0x40u, point + 8u);
    v::vector_init(ctx, 0x60u, 1u, homogeneous_one);
    v::transform(ctx, 1u, 0x04u, 0u, 4u, 4u);
    if (plain == 0u) v::transform(ctx, 1u, 0x24u, 0u, 4u, 4u);
    v::store_quad_left(rt, ctx, 1u, target + 0xCu);
    v::store_quad_right(rt, ctx, 1u, target);
}
void func_08B5A558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { transform_point(rt, ctx, true); }
void func_08B5A5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { transform_point(rt, ctx, false); }

} // namespace
} // namespace hot_batch_341
} // namespace gt::game

#define HOT341(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_341::name, gt::ReturnKind::kind)
HOT341(0x08B563E0u, func_08B563E0, Word);
HOT341(0x08B564C8u, func_08B564C8, Word);
HOT341(0x08B56CF4u, func_08B56CF4, Word);
HOT341(0x08B56B10u, func_08B56B10, Word);
HOT341(0x08B56C34u, func_08B56C34, Word);
HOT341(0x08B56C94u, func_08B56C94, Word);
HOT341(0x08B56DC0u, func_08B56DC0, Word);
HOT341(0x08B56F34u, func_08B56F34, Word);
HOT341(0x08B56300u, func_08B56300, Word);
HOT341(0x08B56A88u, func_08B56A88, Word);
HOT341(0x08B56B70u, func_08B56B70, Word);
HOT341(0x08B4E700u, func_08B4E700, Word);
HOT341(0x08B5A6F0u, func_08B5A6F0, Word);
HOT341(0x08B5A558u, func_08B5A558, Word);
HOT341(0x08B5A5E0u, func_08B5A5E0, Word);
