// Hot batch 94: reached functions of 121-170 instructions left by batch 93 (the probe `tools.decomp_gate probe b93p`, 192 reached; batch 93 took 44, 150 remained) plus the candidates batch 93 had read and left (func_08C0E3B8, func_08C4BB10, func_08BF1768, func_08B22694). (main includes batch 93, 6,370 functions.) Before any function was read its address was searched for in src/ (main and the open PR branches: only #223, a GPU change, is open).
//
// How the functions were found: `tools.shapes --skeleton` over the 150 reached, not-done functions gives 149 clusters, all single functions except the pair batch 93 already wrote and left unregistered (args_to_vector), so this batch has no skeleton family to start from. They were read one by one with `tools.disasm`, the pending ones with the fewest FPU, jump-table and import-stub calls first; reading found four families: a vector assignment of 0x30- and 0x28-byte records (vector_assign: func_08BECF40, func_08BF4CF0, its row = shift, inverse of 3 or 5, element size, last copied offset,
// the three callees and the allocator pair), a stream parser that fills 2 or 4 floats (parse_floats: func_089765C0, func_089B31F0, a sibling of batch 93's scan_block) and the three fill-inserts batch 93 left (func_08C4BB10, func_08BF1768, func_08C0E3B8), which reuse batch 93's fill_insert_grow with a row each (copied here: the two files cannot share a helper in an anonymous namespace). The other functions are constructors (func_0897B108, func_088D671C, func_08BACF7C, func_0886CE44, func_088A4D18, func_08BD5870, func_088A0BD8), by-value struct returns (func_08B32B5C, func_088D2320, func_0881B0D8),
// string-slot natives (func_089F37AC, func_08A2982C, func_089E9F90, func_089AF6A0, func_08A18270, func_08C4DA5C), vector insertions (func_08C16984, func_08C3D298), assignments (func_08C0CECC, func_089DDA80) and similar. The role words in the comments (holder, string, vector, record, cell, queue, slot) describe the shape of the accesses; none is a name from the game.
//
// Registered (32): see the HOT94 lines at the end; written but not registered (3, each commented HOT94 line says why): func_088FE08C (its one reached call stops on `kernel isolation (LwMutex contention)`: not comparable, compared 0), func_08A1F5D4 (every case stops on the import stub sceLibFont::0xA834319D, compared 0) and func_0888C2B8 (compared once, on the options route of the coverage run, then not reached in the next runs: its sabotage was not detected, so it is not registered).
// Left (read, not written): the float-heavy and jump-table functions (func_08B71720, func_08ABE53C, func_08AEB4CC, func_088616A8 / func_08860890, func_08B8F080, func_08B0B8CC, func_088B4F3C, func_08AFFA70, func_088DC974, func_08905300, func_089230A4), the map, allocator and lock functions (func_08A7A3D4, func_08A535A4, func_08A7F5FC, func_08BB6964, func_08AACAF8, func_089C8C70, func_08A13BE0 (a worker loop)) and the script natives with several modes (func_08C23158, func_089BACAC, func_08A14D6C, func_08BD4270).
//
// Gates (fast gate, five routes uncapped): every registered function compared at least once with 0 mismatches and at least one compared case that changed memory; `tools.decomp_gate sabotage` pins 32 of 32 (func_0888C2B8 was the one that was not and is unregistered); `tools.contract_audit` finds nothing new (the two registrations of batch 90 whose callers read v1); `tools.family_members --check` reads no row of this file; `tools.literal_check` finds 0 literals missing in the 35 functions with 7 `--allow` entries
// (0x3F and 0x7F of func_089DDA80 are the widths of its `ins` / `ext` bit fields, 0xED of func_08B32B5C and func_08B22694 is the 0xE8 + 4 + 1 bytes of their unrolled value copy, 0xAAAAAAAB / 0xCCCCCCCD of func_08BECF40 / func_08BF4CF0 are `lui` / `ori` pairs on two registers, 0x3FFFFFFF of func_08C3D298 likewise); each one was checked against the asm by hand and they are on paths every compared case takes. The offsets of the big constructors follow the asm's base registers (v1 = this + 0x15C, a2 = this + 0x94 + 0x50 ...) so the tool reads them.
// The helper and template bodies, which the tool skips, were checked by hand: copy_blocks (16-byte blocks), release_cell (D_08D74FD0 = 0x08D74FD0, +4 / +0xC, the two callees), clear_vector_words (8, 4, 0xC offsets, func_08C02614 / func_08A5BC54), fill_insert_grow (limit 0xFFFFFFFF >> shift, the overflow size -(1 << shift), allocators 08B35DA0 / 08B35840(0x10, bytes) / 08A5BC20(bytes, 4)), vector_assign (the rows 4 / 0xAAAAAAAB / 0x30 / 0x2C and 3 / 0xCCCCCCCD / 0x28 / 0x24, tags sp + 0..3), parse_floats (frame 0x50 / 0x60, 0x08D74FDC, 0x7D, sp + 0x14 / 0x18 / 0x8 / 0x4) and copy_fields.
// Findings of the first gate: func_08A1F328 faulted on every case (`Guest memory access outside PSP RAM/EDRAM at 0x13990043`): the first read used a disassembly with every `lw $sN, off($sp)` hidden to skip the epilogues, and `lw $s0, 4($sp)` at 0x08A1F3E0 is a real load (the object func_08A1EC18 stored); a scan of all 35 functions for loads of callee-saved registers from sp whose offset is no save slot found no other (FINDINGS). The other 34 passed the first time.
// Branches the compared cases do not take are listed in each function's comment (from `tools.decomp_gate coverage`, then read against the asm by hand). The weakest proofs: the room-left halves of func_08C4BB10, func_08BF1768, func_08C0E3B8 and func_08C3D298 (only the reallocating half ran), the copy paths of vector_assign (only empty sources ran), the halfword copy of func_088D671C's second block and the aligned third block, the early exit of func_08933C5C and the types of func_08C4DA5C other than the one that ran.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12): `call_set` writes them to ctx like the original and the ones it leaves alone keep the caller's value, so a callee that reads a register this function never passes sees what an earlier call_set left. The callee-saved registers are never written here (the original restores them before it returns or tail-calls), so there is no SavedRegs; $f20 / $f21 are saved and restored by SavedFloat where they are used. The delay slot of a `jal` / `jalr` runs BEFORE the call,
// a `bnel` / `beql` / `bgtzl` / `blezl` delay slot only when the branch is taken, and the delay slot of a non-likely branch on both paths; every "the delay slot of ..." comment below names the instruction right after the branch it says. v0 (and v1 / f0 where a caller could read them) is set as the original leaves it on every path; the dead stack (spills the original makes and nothing reads, the intermediate copies through sp) is not reproduced, the harness ignores it; the frames that callees read are.
//
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_94 {
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

inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A `lw v0, slot(vtable); jalr v0` call: the callee starts with v0 = its own address (call_indirect's rule).
inline std::uint32_t call_set_v0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    ctx.gpr[2] = address;
    return call_set(rt, ctx, address, arguments);
}
// lwl / lwr pair at `address` + 3 / + 0: the little-endian word that starts at an unaligned address; swl / swr at + 3 / + 0 store one.
inline std::uint32_t word_unaligned(psprecomp::Runtime &rt, std::uint32_t address) {
    return static_cast<std::uint32_t>(rt.memory().load8(address)) | static_cast<std::uint32_t>(rt.memory().load8(address + 1u)) << 8 |
           static_cast<std::uint32_t>(rt.memory().load8(address + 2u)) << 16 | static_cast<std::uint32_t>(rt.memory().load8(address + 3u)) << 24;
}
inline void put_unaligned(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    for (unsigned i = 0u; i < 4u; ++i) rt.memory().store8(address + i, static_cast<std::uint8_t>(value >> (8u * i)));
}
// A word load / store that is lw / sw at an aligned address and an lwl+lwr / swl+swr pair at another one (the same bytes either way).
inline std::uint32_t load_word(psprecomp::Runtime &rt, std::uint32_t address) { return (address & 3u) != 0u ? word_unaligned(rt, address) : word(rt, address); }
inline void store_word(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) {
    if ((address & 3u) != 0u) put_unaligned(rt, address, value); else put(rt, address, value);
}
// The unrolled copy of `bytes` (a multiple of 16) the original makes: each block of 16 bytes is read whole (four words) and then written, word-wise when both addresses are aligned and with lwl/lwr/swl/swr otherwise (the same bytes).
inline void copy_blocks(psprecomp::Runtime &rt, std::uint32_t dest, std::uint32_t source, std::uint32_t bytes) {
    for (std::uint32_t offset = 0u; offset != bytes; offset += 16u) {
        const std::uint32_t w0 = load_word(rt, source + offset), w1 = load_word(rt, source + offset + 4u), w2 = load_word(rt, source + offset + 8u), w3 = load_word(rt, source + offset + 12u);
        store_word(rt, dest + offset, w0);
        store_word(rt, dest + offset + 4u, w1);
        store_word(rt, dest + offset + 8u, w2);
        store_word(rt, dest + offset + 12u, w3);
    }
}
// A virtual call: the slot at `offset` of the object's vtable (the word at object[0]) with the object as a0 and the remaining arguments after it.
inline std::uint32_t slot_of(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t offset) { return word(rt, word(rt, object) + offset); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------

// A byte copy (the halfword / word loops of the original move the same bytes in blocks; used for frame temporaries only).
inline void copy_bytes(psprecomp::Runtime &rt, std::uint32_t dest, std::uint32_t source, std::uint32_t bytes) {
    for (std::uint32_t i = 0u; i != bytes; ++i) put8(rt, dest + i, byte_u(rt, source + i));
}

// The ll / sc increment of a reference counter (one thread: a plain load and store).
inline void increment_counter(psprecomp::Runtime &rt, std::uint32_t address) { put(rt, address, word(rt, address) + 1u); }

// The release of a string cell whose data pointer is `data`: nothing happens when data - 0xC is D_08D74FD0 (the shared empty cell), otherwise func_08B60804(data - 4, -1) and, when its result is not positive, func_08B8542C(data - 0xC, flag). Leaves a0 / a1 like the original.
inline void release_cell(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t data, std::uint32_t flag) {
    ctx.gpr[5] = 0xFFFFFFFFu;
    ctx.gpr[4] = data - 4u;  // the delay slot of the beq
    if (data - 0xCu == 0x08D74FD0u) return;
    const std::uint32_t references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    ctx.gpr[4] = data - 0xCu;
    ctx.gpr[5] = flag;  // the delay slot of the bgtz
    if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, flag});
}

// clear_vector_words(vector, copy, slot): the vector {begin, end, capacity} at `vector` is cleared (the three words zero) and its old contents handed on: func_08C02614(begin, end, slot), then a non-null block is freed with func_08A5BC54(begin, ((capacity - begin) >> 2) * 4, 4). The old words are kept at frame[`copy`..+8] like the original does.
inline void clear_vector_words(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t vector, std::uint32_t copy, std::uint32_t slot) {
    const std::uint32_t begin = word(rt, vector), end = word(rt, vector + 4u), capacity = word(rt, vector + 8u);
    put(rt, vector, 0u);
    put(rt, vector + 4u, 0u);
    put(rt, vector + 8u, 0u);
    put(rt, frame.at(copy), begin);
    put(rt, frame.at(copy + 8u), capacity);
    put(rt, frame.at(copy + 4u), end);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08C02614u, {begin, end, slot});
    const std::uint32_t block = word(rt, frame.at(copy));
    const std::uint32_t words = static_cast<std::uint32_t>(signed_of(word(rt, frame.at(copy + 8u)) - block) >> 2);
    ctx.gpr[2] = words;
    if (block != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {block, words << 2, 4u});
}

// A floating-point register as it was at entry (the original saves the callee-saved $f20 / $f21 in its frame and restores them before it returns).
class SavedFloat {
public:
    SavedFloat(psprecomp::AllegrexContext &ctx, unsigned index) : ctx_(ctx), index_(index), saved_(ctx.fpr[index]) {}
    ~SavedFloat() { ctx_.fpr[index_] = saved_; }
    SavedFloat(const SavedFloat &) = delete;
    SavedFloat &operator=(const SavedFloat &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    unsigned index_;
    float saved_;
};

// func_088D2320(source, dest): the packed copy of a record: dest[0] = source byte 0, then the words source[4] and source[8] at dest + 1 and dest + 5, 0xC0 bytes from source + 0xC, 0x40 from source + 0xCC (the 0x40 from source + 0x10C are skipped), 0xC0 from source + 0x14C, three words from source + 0x20C, one from source + 0x218, three from source + 0x220 and four from source + 0x304, each run packed right behind the previous one
// (the destination offsets 9, 0xC9, 0x109, 0x1C9, 0x1D5, 0x1D9, 0x1E5 are the asm's t1, a3, t3, v1, a3, a1, a0). The copies are word-wise when both addresses are aligned and lwl/lwr/swl/swr otherwise (the same bytes). v0 = t3 + 0xEC (the end of what was written), v1 = source[0x304].
void func_088D2320(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t t4 = ctx.gpr[4], a1 = ctx.gpr[5];
    put8(rt, a1, byte_u(rt, t4));
    const std::uint32_t v0a = a1 + 1u, t1 = a1 + 9u, t0 = t4 + 0xCu, v1a = a1 + 5u;
    store_word(rt, v0a, word(rt, t4 + 4u));
    store_word(rt, v1a, word(rt, t4 + 8u));
    copy_blocks(rt, t1, t0, 0xC0u);
    const std::uint32_t a3 = t1 + 0xC0u;
    copy_blocks(rt, a3, t4 + 0xCCu, 0x40u);
    const std::uint32_t t3 = a3 + 0x40u;
    copy_blocks(rt, t3, t4 + 0x14Cu, 0xC0u);
    const std::uint32_t t0b = t4 + 0x20Cu;
    const std::uint32_t v0 = word(rt, t4 + 0x20Cu), a2 = word(rt, t0b + 4u), t1b = word(rt, t0b + 8u);
    const std::uint32_t v1 = t3 + 0xC0u;
    store_word(rt, v1, v0);
    store_word(rt, v1 + 4u, a2);
    store_word(rt, v1 + 8u, t1b);
    const std::uint32_t a3b = t3 + 0xCCu, t2 = t4 + 0x220u, a1b = t3 + 0xD0u;
    store_word(rt, a3b, word(rt, t4 + 0x218u));
    const std::uint32_t v1c = word(rt, t4 + 0x220u), a3c = word(rt, t2 + 8u), a2c = word(rt, t2 + 4u);
    store_word(rt, a1b, v1c);
    store_word(rt, a1b + 4u, a2c);
    store_word(rt, a1b + 8u, a3c);
    const std::uint32_t t0c = t4 + 0x304u, a0 = t3 + 0xDCu;
    const std::uint32_t v1d = word(rt, t4 + 0x304u), a1d = word(rt, t0c + 4u), a3d = word(rt, t0c + 0xCu), a2d = word(rt, t0c + 8u);
    store_word(rt, a0, v1d);
    store_word(rt, a0 + 4u, a1d);
    store_word(rt, a0 + 8u, a2d);
    store_word(rt, a0 + 0xCu, a3d);
    ctx.gpr[2] = t3 + 0xECu;
    ctx.gpr[3] = v1d;
}

// func_0881B0D8(self, dest): the packed copy of a record with a table: 0x30 bytes of self, then func_08B79A88(dest + 0x30, self[0x30], n * 34) with n = self[0], then 0x190 bytes from self + 0x34 and 0x190 from self + 0x1C4 right behind the table (dest + 0x30 + n * 34), and for the 30 eight-byte entries from self + 0x354
// func_088A6F74(entry, cursor) with the cursor starting behind the second block and each call's answer the next cursor. v0 = the last answer.
void func_0881B0D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], dest = ctx.gpr[5];
    copy_blocks(rt, dest, self, 0x30u);
    const std::uint32_t count = word(rt, self);
    (void)call_set(rt, ctx, 0x08B79A88u, {dest + 0x30u, word(rt, self + 0x30u), (count << 1) + (count << 5)});
    std::uint32_t cursor = dest + 0x30u;
    const std::uint32_t after_table = word(rt, self);
    cursor += (after_table << 1) + (after_table << 5);
    copy_blocks(rt, cursor, self + 0x34u, 0x190u);
    cursor += 0x190u;
    copy_blocks(rt, cursor, self + 0x1C4u, 0x190u);
    std::uint32_t next = cursor + 0x190u;
    for (std::uint32_t i = 0u, entry = self + 0x354u; i != 0x1Eu; ++i, entry += 8u) next = call_set(rt, ctx, 0x088A6F74u, {entry, next});
    ctx.gpr[2] = next;
}

// func_08B32B5C(dest, object): the answer of a virtual call is returned by value. With object[4] == 0, func_08B215B8(dest, 0, 0, 0, 0) builds the default value. Otherwise o = object[4]: the virtual slot 8 of o (a0 = a 0xED-byte buffer at sp + 0x1E0, a1 = o) fills the buffer, the 0xED bytes are copied to sp + 0xEE (halfword-wise: the address is 2 mod 4), func_08B22E94(sp + 0x2D0, sp + 0xEE, object[8]) converts them
// into a second buffer and its 0xED bytes become *dest (the copy through sp is dead stack). On both paths the halfword at dest + 2 becomes 0x48 when it was zero and 0 otherwise (movn). v0 = dest.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (inner == 0u) {`; `put16(rt, dest + 2u, half_u(rt, dest + 2u) != 0u ? 0u : 0x48u);`
void func_08B32B5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t dest = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x3D0u);
    const std::uint32_t inner = word(rt, object + 4u);
    if (inner == 0u) {
        (void)call_set(rt, ctx, 0x08B215B8u, {dest, 0u, 0u, 0u, 0u});
    } else {
        const std::uint32_t buffer = frame.at(0x1E0u);
        (void)call_set_v0(rt, ctx, slot_of(rt, inner, 8u), {buffer, inner});
        copy_bytes(rt, frame.at(0xEEu), buffer, 0xEDu);
        (void)call_set(rt, ctx, 0x08B22E94u, {frame.at(0x2D0u), frame.at(0xEEu), word(rt, object + 8u)});
        copy_bytes(rt, dest, frame.at(0x2D0u), 0xEDu);
    }
    put16(rt, dest + 2u, half_u(rt, dest + 2u) != 0u ? 0u : 0x48u);
    ctx.gpr[2] = dest;
}

// func_0897B108(self, owner): a constructor. func_08A76ACC(self); self[0] = D_08CBB740 (0x08CBB740), self[0x14] = owner and the list head self[0x18] / self[0x1C] point at self + 0x18; func_08B28DCC(self + 0x20), func_08B28D34(self + 0x48); the fields at 0x74-0x90 are zeroed; 0x58 bytes from D_08C8BEAC go to self + 0x94 and 0x58 from D_08C8BE54 to self + 0xEC; words and halfwords from D_08C8BE40-D_08C8BE50 fill 0x14A-0x158;
// the defaults 10 (0x168), 300 (0x16C), 1.0f (0x170), 20 (halfword 0x174), 6 (halfword 0x176), 0x00FFFFFF (0x17C); two more list heads at self + 0x184 and self + 0x18C; four func_08A76AA0 members at self + 0x19C..0x1A8; the floats 1.0f at 0x1BC, 0x1C4, 0x1D0 and 0x1D4, self[0x1F0] = -1 and the other fields up to 0x204 zero. The offsets follow the asm's base registers (v1 = self + 0x74, 0x15C, 0x1E0; a2 = self + 0x94 + 0x50, t2 = self + 0xEC + 0x50, a3 = self + 0x184, t0 = self + 0x18C). v0 = -1, v1 = self + 0x1E0, f0 = 1.0f.
void func_0897B108(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s1 = ctx.gpr[4], owner = ctx.gpr[5];
    (void)call_set(rt, ctx, 0x08A76ACCu, {s1});
    const std::uint32_t one = 0x3F800000u;
    const std::uint32_t v1_18 = s1 + 0x18u;
    put(rt, s1, 0x08CBB740u);
    put(rt, s1 + 0x18u, v1_18);
    put(rt, s1 + 0x14u, owner);
    put(rt, v1_18 + 4u, v1_18);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B28DCCu, {s1 + 0x20u});
    (void)call_set(rt, ctx, 0x08B28D34u, {s1 + 0x48u});
    const std::uint32_t v1_74 = s1 + 0x74u;
    put(rt, v1_74 + 4u, 0u);
    put(rt, s1 + 0x74u, 0u);
    put(rt, s1 + 0x7Cu, 0u);
    put(rt, s1 + 0x80u, 0u);
    put(rt, v1_74 + 0x10u, 0u);
    put8(rt, s1 + 0x88u, 0u);
    put(rt, s1 + 0x8Cu, 0u);
    put(rt, s1 + 0x90u, 0u);
    std::uint32_t a3 = 0x08C8BEACu;
    copy_blocks(rt, s1 + 0x94u, a3, 0x50u);
    a3 += 0x50u;
    const std::uint32_t a2 = s1 + 0x94u + 0x50u;
    const std::uint32_t tail_4 = word(rt, a3 + 4u), tail_0 = word(rt, a3);
    put(rt, a2 + 4u, tail_4);
    put(rt, a2, tail_0);
    a3 = 0x08C8BE54u;
    copy_blocks(rt, s1 + 0xECu, a3, 0x50u);
    a3 += 0x50u;
    const std::uint32_t t2 = s1 + 0xECu + 0x50u;
    const std::uint32_t t1 = word(rt, 0x08C8BE48u + 4u), a0_50 = word(rt, a3 + 4u), a1_50 = word(rt, a3), a2_48 = word(rt, 0x08C8BE48u), v1_50 = half_u(rt, 0x08C8BE50u);
    put(rt, t2 + 4u, a0_50);
    put(rt, t2, a1_50);
    put16(rt, s1 + 0x14Au, v1_50);
    put16(rt, s1 + 0x154u, half_u(rt, 0x08C8BE44u));
    put(rt, s1 + 0x14Cu, a2_48);
    put(rt, s1 + 0x168u, 10u);
    put(rt, s1 + 0x158u, word(rt, 0x08C8BE40u));
    put16(rt, s1 + 0x174u, 0x14u);
    put(rt, s1 + 0x16Cu, 0x12Cu);
    put(rt, s1 + 0x150u, t1);
    put(rt, s1 + 0x170u, one);
    put16(rt, s1 + 0x176u, 6u);
    put(rt, s1 + 0x17Cu, 0x00FFFFFFu);
    put(rt, s1 + 0x144u, 0u);
    put8(rt, s1 + 0x148u, 0u);
    put8(rt, s1 + 0x149u, 0u);
    put16(rt, s1 + 0x156u, 0u);
    put(rt, s1 + 0x15Cu, 0u);
    put8(rt, s1 + 0x160u, 0u);
    put(rt, s1 + 0x164u, 0u);
    put(rt, s1 + 0x178u, 0u);
    put(rt, s1 + 0x180u, 0u);
    const std::uint32_t a3_184 = s1 + 0x184u, t0_18c = s1 + 0x18Cu;
    put(rt, s1 + 0x184u, a3_184);
    put(rt, a3_184 + 4u, a3_184);
    put(rt, s1 + 0x18Cu, t0_18c);
    put(rt, t0_18c + 4u, t0_18c);
    put(rt, s1 + 0x194u, 0u);
    put8(rt, s1 + 0x198u, 0u);
    for (std::uint32_t s0 = s1 + 0x19Cu, s2 = 4u; s2 != 0u; --s2, s0 += 4u) (void)call_set(rt, ctx, 0x08A76AA0u, {s0});
    const std::uint32_t v1_1e0 = s1 + 0x1E0u;
    put(rt, s1 + 0x1D4u, one);
    put(rt, s1 + 0x1ACu, 0u);
    put(rt, s1 + 0x1B0u, 0u);
    put8(rt, s1 + 0x1B4u, 0u);
    put8(rt, s1 + 0x1B5u, 0u);
    put(rt, s1 + 0x1B8u, 0u);
    put(rt, s1 + 0x1BCu, one);
    put(rt, s1 + 0x1C0u, 0u);
    put(rt, s1 + 0x1C4u, one);
    put(rt, s1 + 0x1C8u, 0u);
    put(rt, s1 + 0x1CCu, 0u);
    put(rt, s1 + 0x1D0u, one);
    put(rt, s1 + 0x1E0u, 0u);
    put(rt, v1_1e0 + 0x10u, 0xFFFFFFFFu);
    put(rt, v1_1e0 + 4u, 0u);
    put(rt, v1_1e0 + 8u, 0u);
    put(rt, v1_1e0 + 0xCu, 0u);
    put8(rt, s1 + 0x204u, 0u);
    put8(rt, s1 + 0x200u, 0u);
    put8(rt, s1 + 0x201u, 0u);
    put8(rt, s1 + 0x202u, 0u);
    put8(rt, s1 + 0x203u, 0u);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = v1_1e0;
    ctx.fpr[0] = 1.0f;
}

// func_088D671C(self, source): a constructor. self[0] = a new block func_08B62344(0x118) (func_088EF524(block + 0x82) follows it); self[4] = the shared object at D_08D75FE0, whose first word is set to D_08CC36F8 (0x08CC36F8) the first time (guard byte at D_08D75FD8: func_08B6441C acquires, func_08B64494 releases); then the 0x116 bytes of *source (its first word is a pointer)
// are copied into the block in three runs (0x60 bytes, 0x22 from +0x60, 0x94 from +0x82), word-wise or halfword-wise like the asm (a2 | a3 aligned or not). v0 / v1 are the last loads of the copy: the two halfwords at +0x114 / +0x112, or (both addresses +0x82 aligned) the word at +0x112 and the word at +0x106.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (static_cast<std::int32_t>(sext8(byte_u(rt, guard))) == 0) {`; `if (call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {`; `if (((a2 | a3) & 3u) != 0u) {`; `for (; a2 != t0; a2 += 8u, a3 += 8u) {`; `for (; a2 != t0b; a2 += 0x10u, a3 += 0x10u) {`
void func_088D671C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {0x118u});
    (void)call_set(rt, ctx, 0x088EF524u, {block + 0x82u});
    put(rt, self, block);
    const std::uint32_t guard = 0x08D75FD8u, shared = 0x08D75FE0u;
    if (static_cast<std::int32_t>(sext8(byte_u(rt, guard))) == 0) {
        if (call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
            put(rt, shared, 0x08CC36F8u);
            (void)call_set(rt, ctx, 0x08B64494u, {guard});
        }
    }
    std::uint32_t a2 = word(rt, source), a3 = word(rt, self);
    put(rt, self + 4u, shared);
    copy_blocks(rt, a3, a2, 0x60u);
    const std::uint32_t a0 = word(rt, source), v1 = word(rt, self);
    a2 = a0 + 0x60u;
    a3 = v1 + 0x60u;
    const std::uint32_t t0 = a0 + 0x80u;
    if (((a2 | a3) & 3u) != 0u) {
        for (; a2 != t0; a2 += 8u, a3 += 8u) {
            const std::uint32_t h0 = half_u(rt, a2), h1 = half_u(rt, a2 + 2u), h2 = half_u(rt, a2 + 4u), h3 = half_u(rt, a2 + 6u);
            put16(rt, a3, h0);
            put16(rt, a3 + 2u, h1);
            put16(rt, a3 + 4u, h2);
            put16(rt, a3 + 6u, h3);
        }
    } else {
        for (; a2 != t0; a2 += 0x10u, a3 += 0x10u) {
            const std::uint32_t w0 = word(rt, a2), w1 = word(rt, a2 + 4u), w2 = word(rt, a2 + 8u), w3 = word(rt, a2 + 0xCu);
            put(rt, a3, w0);
            put(rt, a3 + 4u, w1);
            put(rt, a3 + 8u, w2);
            put(rt, a3 + 0xCu, w3);
        }
    }
    put16(rt, a3, half_u(rt, a2));
    const std::uint32_t a1 = word(rt, source), v1b = word(rt, self);
    a2 = a1 + 0x82u;
    a3 = v1b + 0x82u;
    const std::uint32_t t0b = a1 + 0x112u;
    if (((a2 | a3) & 3u) != 0u) {
        for (; a2 != t0b; a2 += 8u, a3 += 8u) {
            const std::uint32_t h0 = half_u(rt, a2), h1 = half_u(rt, a2 + 2u), h2 = half_u(rt, a2 + 4u), h3 = half_u(rt, a2 + 6u);
            put16(rt, a3, h0);
            put16(rt, a3 + 2u, h1);
            put16(rt, a3 + 4u, h2);
            put16(rt, a3 + 6u, h3);
        }
        const std::uint32_t v0 = half_u(rt, a2 + 2u), v1c = half_u(rt, a2);
        put16(rt, a3 + 2u, v0);
        put16(rt, a3, v1c);
        ctx.gpr[2] = v0;
        ctx.gpr[3] = v1c;
    } else {
        std::uint32_t v1d = 0u;
        for (; a2 != t0b; a2 += 0x10u, a3 += 0x10u) {
            const std::uint32_t w0 = word(rt, a2), w1 = word(rt, a2 + 4u), w2 = word(rt, a2 + 8u), w3 = word(rt, a2 + 0xCu);
            put(rt, a3, w0);
            put(rt, a3 + 4u, w1);
            put(rt, a3 + 8u, w2);
            put(rt, a3 + 0xCu, w3);
            v1d = w1;
        }
        const std::uint32_t v0 = word(rt, a2);
        put(rt, a3, v0);
        ctx.gpr[2] = v0;
        ctx.gpr[3] = v1d;
    }
}

// func_088FE08C(out, a1, a2, a3): func_08B0CE58(sp + 0x80) and func_088FDF98(out) first. With a2 != 0, func_08B10170(sp + 0xC0, a1, a2, a3, 1) builds a 0x3C-byte record, otherwise func_08B10204(sp + 0x100, a1, 1) does and out[4] = its word at +0x20; the record (the copies through sp + 0x40 / sp are dead stack) lands at sp + 0x80. out[0] = 0 when the words at +0x10 and +0x14 of the record are both zero, else the word at +0x20;
// a non-zero out[0] is passed to func_088FDFE0. v0 = out[0].
[[maybe_unused]] void func_088FE08C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x160u);
    const std::uint32_t record = frame.at(0x80u);
    (void)call_set(rt, ctx, 0x08B0CE58u, {record});
    (void)call_set(rt, ctx, 0x088FDF98u, {out});
    if (a2 != 0u) {
        const std::uint32_t built = frame.at(0xC0u);
        (void)call_set(rt, ctx, 0x08B10170u, {built, a1, a2, a3, 1u});
        copy_blocks(rt, record, built, 0x30u);
        for (std::uint32_t i = 0u; i != 12u; i += 4u) put(rt, record + 0x30u + i, word(rt, built + 0x30u + i));
    } else {
        const std::uint32_t built = frame.at(0x100u);
        (void)call_set(rt, ctx, 0x08B10204u, {built, a1, 1u});
        copy_blocks(rt, record, built, 0x30u);
        for (std::uint32_t i = 0u; i != 12u; i += 4u) put(rt, record + 0x30u + i, word(rt, built + 0x30u + i));
        put(rt, out + 4u, word(rt, frame.at(0xA0u)));
    }
    if ((word(rt, frame.at(0x90u)) | word(rt, frame.at(0x94u))) == 0u) put(rt, out, 0u);
    else put(rt, out, word(rt, frame.at(0xA0u)));
    ctx.gpr[2] = word(rt, out);
    if (ctx.gpr[2] != 0u) {
        (void)call_set(rt, ctx, 0x088FDFE0u, {ctx.gpr[2]});
        ctx.gpr[2] = word(rt, out);
    }
}

// func_08BACF7C(self, first, second): a constructor from two counted pointers {object, counter holder, word, counter holder}. self[0] / self[4] = first[0] / first[4] (a non-zero first[4] gets its counter at +4 incremented), self[8] = first[8], self[0xC] = first[0xC] (counted the same way), self[0x10..0x1C] the same from `second` (the asm's a0 = self + 0x10); self[0x20] / self[0x24] = 0; func_08BACAD4 initialises self + 0x28, self + 0x38 and self + 0x48 (s0 += 0x10) and self + 0x60 (self[0x58] / self[0x5C] = 0 first); self[0x70] and self[0x74] = 0 and four 0xD4-byte records from self + 0x78 get -1 at +0xCC and zero bytes at +0xD0..0xD2;
// self[0x3C8] / self[0x3CC] = 0, self[0x8E0..0x8E8] = 0 (v0 = self + 0x8E0), self[0xAFC] = 0x3C888889 (a float), self[0xB00..0xB09] = 0; func_0889B088(self + 0xB0C, self), func_0889966C(self + 0xB14, self), self[0xB20] = D_08CC2318 (0x08CC2318) with the record at v1 = self + 0xB20 (+4 = self, +8 = 0, +0xC = 1) and func_08BACB08(self + 0xB30, self) (s1 + 0xC = 2); self[0xB30] = D_08CC22F8 (0x08CC22F8), func_08BACB08(self + 0xB40, self);
// the vtable words 0x08CC2338 (0xB40), 0x08CC2358 (0xB4C), 0x08CC2318 (0xB58), 0x08CC2378 (0xB68), 0x08CC2398 (0xB74) with the back pointer self at +4 and a zero byte at +8 of each record (a2 = self + 0xB4C, a1 = 0xB58 (+0xC = 0), a3 = 0xB68, t0 = 0xB74), self[0xB80..0xB88] = 0. Ends with a tail jump to func_08B4E648(self + 0x8A0, ...) (it stores an identity matrix); v0 = 0x08CC2378 and v1 = 0x08CC2398 stay as the original leaves them.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (counted_a != 0u) increment_counter(rt, counted_a + 4u);`; `if (counted_d != 0u) increment_counter(rt, counted_d + 4u);`
void func_08BACF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s2 = ctx.gpr[4], first = ctx.gpr[5], second = ctx.gpr[6];
    put(rt, s2, word(rt, first));
    const std::uint32_t counted_a = word(rt, first + 4u);
    put(rt, s2 + 4u, counted_a);
    if (counted_a != 0u) increment_counter(rt, counted_a + 4u);
    put(rt, s2 + 8u, word(rt, first + 8u));
    const std::uint32_t counted_b = word(rt, first + 0xCu);
    put(rt, s2 + 0xCu, counted_b);
    if (counted_b != 0u) increment_counter(rt, counted_b + 4u);
    const std::uint32_t a0 = s2 + 0x10u;
    put(rt, s2 + 0x10u, word(rt, second));
    const std::uint32_t counted_c = word(rt, second + 4u);
    put(rt, s2 + 0x14u, counted_c);
    if (counted_c != 0u) increment_counter(rt, counted_c + 4u);
    put(rt, a0 + 8u, word(rt, second + 8u));
    const std::uint32_t counted_d = word(rt, second + 0xCu);
    put(rt, a0 + 0xCu, counted_d);
    if (counted_d != 0u) increment_counter(rt, counted_d + 4u);
    put(rt, s2 + 0x20u, 0u);
    put(rt, s2 + 0x24u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BACAD4u, {s2 + 0x28u});
    std::uint32_t s0 = s2 + 0x38u;
    for (std::uint32_t s1 = 1u; s1 != 0xFFFFFFFFu; --s1, s0 += 0x10u) (void)call_set(rt, ctx, 0x08BACAD4u, {s0});
    put(rt, s2 + 0x58u, 0u);
    put(rt, s2 + 0x5Cu, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BACAD4u, {s2 + 0x60u});
    put8(rt, s2 + 0x70u, 0u);
    put(rt, s2 + 0x74u, 0u);
    for (std::uint32_t record = s2 + 0x78u, left = 4u; left != 0u; --left, record += 0xD4u) {
        put8(rt, record + 0xD2u, 0u);
        put(rt, record + 0xCCu, 0xFFFFFFFFu);
        put8(rt, record + 0xD0u, 0u);
        put8(rt, record + 0xD1u, 0u);
    }
    const std::uint32_t v0_8e0 = s2 + 0x8E0u;
    put8(rt, s2 + 0x3C8u, 0u);
    put(rt, s2 + 0x3CCu, 0u);
    put(rt, v0_8e0 + 8u, 0u);
    put(rt, v0_8e0 + 4u, 0u);
    put(rt, s2 + 0x8E0u, 0u);
    put(rt, s2 + 0xAFCu, 0x3C888889u);
    put8(rt, s2 + 0x8ECu, 0u);
    put(rt, s2 + 0xB00u, 0u);
    put(rt, s2 + 0xB04u, 0u);
    put8(rt, s2 + 0xB08u, 0u);
    put8(rt, s2 + 0xB09u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0889B088u, {s2 + 0xB0Cu, s2});
    (void)call_set(rt, ctx, 0x0889966Cu, {s2 + 0xB14u, s2});
    const std::uint32_t v1_b20 = s2 + 0xB20u, s1_b30 = s2 + 0xB30u;
    put(rt, v1_b20 + 0xCu, 1u);
    put(rt, v1_b20 + 4u, s2);
    put8(rt, v1_b20 + 8u, 0u);
    put(rt, s2 + 0xB20u, 0x08CC2318u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BACB08u, {s1_b30, s2});
    put(rt, s1_b30 + 0xCu, 2u);
    put(rt, s2 + 0xB30u, 0x08CC22F8u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BACB08u, {s2 + 0xB40u, s2});
    const std::uint32_t a2 = s2 + 0xB4Cu, a1 = s2 + 0xB58u, a3 = s2 + 0xB68u, t0 = s2 + 0xB74u;
    put(rt, s2 + 0xB40u, 0x08CC2338u);
    put(rt, s2 + 0xB4Cu, 0x08CC2358u);
    put8(rt, a2 + 8u, 0u);
    put(rt, a2 + 4u, s2);
    put8(rt, a1 + 8u, 0u);
    put(rt, a1 + 0xCu, 0u);
    put(rt, a1 + 4u, s2);
    put8(rt, a3 + 8u, 0u);
    put(rt, a3 + 4u, s2);
    put8(rt, t0 + 8u, 0u);
    put(rt, t0 + 4u, s2);
    put(rt, s2 + 0xB58u, 0x08CC2318u);
    put(rt, s2 + 0xB68u, 0x08CC2378u);
    put(rt, s2 + 0xB74u, 0x08CC2398u);
    put(rt, s2 + 0xB80u, 0u);
    put(rt, s2 + 0xB84u, 0u);
    put(rt, s2 + 0xB88u, 0u);
    ctx.gpr[2] = 0x08CC2378u;
    ctx.gpr[3] = 0x08CC2398u;
    tail_call(ctx, 0x08B4E648u, {s2 + 0x8A0u, a1, a2, a3, t0});
}

// copy_fields(dest, source, count, element, last): the element copy of the vector assignments below: `count` elements of `element` bytes, of which only the words at +4..+last are copied (the word at +0 stays).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `for (std::uint32_t i = 0u; i != count; ++i, dest += element, source += element) {`; `for (std::uint32_t offset = 4u; offset <= last; offset += 4u) put(rt, dest + offset, word(rt, source`
inline void copy_fields(psprecomp::Runtime &rt, std::uint32_t dest, std::uint32_t source, std::uint32_t count, std::uint32_t element, std::uint32_t last) {
    for (std::uint32_t i = 0u; i != count; ++i, dest += element, source += element) {
        for (std::uint32_t offset = 4u; offset <= last; offset += 4u) put(rt, dest + offset, word(rt, source + offset));
    }
}

// The assignment of a vector {begin, end, capacity} of `element`-byte records (func_08BECF40: 0x30 bytes, func_08BF4CF0: 0x28). v0 = self. Nothing happens for self == other. With count = the element count of `other` (the byte length >> shift times the inverse of 3 or 5 mod 2^32) and room = the capacity of self in elements:
// room < count: block = func_08B35840(0x10, bytes) / func_08A5BC20(bytes, 4) (`pooled` = the second; bytes = (length >> shift) << shift), `construct`(other.begin, other.end, block, sp) copies into it, `destroy_old`(self.begin, self.end, sp + 3) destroys the old elements, a non-null old block is freed (func_08B35254(block) / func_08A5BC54(block, ((capacity - block) >> shift) << shift, 4)), self = {block, block + bytes, block + bytes}.
// Otherwise size = the element count of self: size >= count copies `count` elements field-wise (words +4..+last) over the old ones and destroys the rest (`destroy_range`(begin + count * element, end, sp + 2)); size < count copies `size` elements field-wise and `construct`(other.begin + (self length & ~(2^shift - 1)), other.end, self.end, sp + 1) constructs the remaining ones. In the last two cases self.end = self.begin + count * element; in the first self.end = block + bytes.
struct VectorAssignRow {
    unsigned shift;
    std::uint32_t inverse, element, last, construct, destroy_old, destroy_range;
    bool pooled;
};
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (other != self) {`; `const std::uint32_t block = Row.pooled ? call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u}) : call_set(rt, `; `if (Row.pooled) {`; `if (old_block != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_block, ctx.gpr[2] << Row.shift, 4u});`; `} else if (old_block != 0u) {`; `if (!(size < count)) {`; `if (signed_of(count) > 0) {`; `if (signed_of(size) > 0) {`
template <VectorAssignRow Row>
void vector_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (other != self) {
        const std::uint32_t begin = word(rt, self), capacity = word(rt, self + 8u);
        const std::uint32_t room = (static_cast<std::uint32_t>(signed_of(capacity - begin) >> Row.shift)) * Row.inverse;
        std::uint32_t other_end = word(rt, other + 4u), other_begin = word(rt, other);
        const std::uint32_t units = static_cast<std::uint32_t>(signed_of(other_end - other_begin) >> Row.shift);
        const std::uint32_t count = units * Row.inverse;
        std::uint32_t used;  // a0 / a1 at the shared end: the byte length of the new contents
        if (room < count) {
            const std::uint32_t bytes = units << Row.shift;
            const std::uint32_t block = Row.pooled ? call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u}) : call_set(rt, ctx, 0x08B35840u, {0x10u, bytes});
            (void)call_set(rt, ctx, Row.construct, {other_begin, other_end, block, frame.at(0u)});
            (void)call_set(rt, ctx, Row.destroy_old, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
            const std::uint32_t old_block = word(rt, self);
            if (Row.pooled) {
                ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - old_block) >> Row.shift);
                if (old_block != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_block, ctx.gpr[2] << Row.shift, 4u});
            } else if (old_block != 0u) {
                (void)call_set(rt, ctx, 0x08B35254u, {old_block});
            }
            put(rt, self + 8u, block + bytes);
            put(rt, self, block);  // the delay slot of the j
            used = bytes;
        } else {
            const std::uint32_t end = word(rt, self + 4u);
            const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> Row.shift) * Row.inverse;
            if (!(size < count)) {
                if (signed_of(count) > 0) {
                    copy_fields(rt, begin, other_begin, count, Row.element, Row.last);
                    (void)call_set(rt, ctx, Row.destroy_range, {begin + count * Row.element, word(rt, self + 4u), frame.at(2u)});
                } else {
                    (void)call_set(rt, ctx, Row.destroy_range, {begin, end, frame.at(2u)});
                }
            } else {
                if (signed_of(size) > 0) {
                    copy_fields(rt, begin, other_begin, size, Row.element, Row.last);
                    other_begin = word(rt, other);
                    other_end = word(rt, other + 4u);
                }
                const std::uint32_t this_begin = word(rt, self), this_end = word(rt, self + 4u);
                const std::uint32_t done_bytes = (this_end - this_begin) & ~((1u << Row.shift) - 1u);
                (void)call_set(rt, ctx, Row.construct, {other_begin + done_bytes, other_end, this_end, frame.at(1u)});
            }
            used = count * Row.element;
        }
        put(rt, self + 4u, word(rt, self) + used);
    }
    ctx.gpr[2] = self;
}

void func_08BECF40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_assign<VectorAssignRow{4u, 0xAAAAAAABu, 0x30u, 0x2Cu, 0x08BECEACu, 0x08BE5BC0u, 0x08BE58F0u, false}>(rt, ctx);
}
void func_08BF4CF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    vector_assign<VectorAssignRow{3u, 0xCCCCCCCDu, 0x28u, 0x24u, 0x08BF4C70u, 0x08BE5C0Cu, 0x08BE593Cu, true}>(rt, ctx);
}

// copy16(dest, source): four words read whole, then written.
inline void copy16(psprecomp::Runtime &rt, std::uint32_t dest, std::uint32_t source) {
    const std::uint32_t w0 = word(rt, source), w1 = word(rt, source + 4u), w2 = word(rt, source + 8u), w3 = word(rt, source + 0xCu);
    put(rt, dest, w0);
    put(rt, dest + 4u, w1);
    put(rt, dest + 8u, w2);
    put(rt, dest + 0xCu, w3);
}

// func_08C16984(self, position, value): the insertion of one 16-byte element (four floats) before `position` in a vector {begin, end, capacity}. With room (end != capacity): a non-null end gets a copy of the last element, the end grows by 0x10, `value` is copied to sp + 4, the elements from `position` up to the old last one are moved up one place from the back
// (v elements, copy_backward) and position takes the copy (unless position is the temporary itself). v0 = sp + 4. Without room: size = (end - begin) >> 4 (0x0FFFFFFF calls func_08B5ADDC(D_08CA9B68)), bytes = 0x10 for an empty vector, size * 0x20 otherwise (0xFFFFFFF0 when the doubling overflowed), block = func_08A5BC20(bytes, 4),
// e = func_08C16084(begin, position, block, sp + 2), a non-null e gets the value, last = func_08C16084(position, end, e + 0x10, sp + 1), func_08C16074(begin, end, sp) destroys the old elements, a non-null old block is freed with func_08A5BC54(begin, ((capacity - begin) >> 4) << 4, 4) and the vector becomes {block, last, block + bytes}; v0 = block + bytes.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (end == word(rt, self + 8u)) {`; `if (size == 0x0FFFFFFFu) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CA9B68u});`; `bytes = doubled < size ? 0xFFFFFFF0u : doubled << 4;`; `if (e != 0u) copy16(rt, e, value);`; `if (end != 0u) copy16(rt, end, end - 0x10u);`; `if (signed_of(moved) > 0) {`; `for (std::uint32_t done = 0u; done != moved; ++done, source -= 0x10u, dest -= 0x10u) copy16(rt, dest`; `if (position != temp) copy16(rt, position, temp);`
void func_08C16984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t end = word(rt, self + 4u);
    if (end == word(rt, self + 8u)) {
        const std::uint32_t begin = word(rt, self);
        const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 4);
        if (size == 0x0FFFFFFFu) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CA9B68u});
        std::uint32_t bytes = 0x10u;
        if (size != 0u) {
            const std::uint32_t doubled = size << 1;
            bytes = doubled < size ? 0xFFFFFFF0u : doubled << 4;
        }
        const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        const std::uint32_t e = call_set(rt, ctx, 0x08C16084u, {word(rt, self), position, block, frame.at(2u)});
        if (e != 0u) copy16(rt, e, value);
        const std::uint32_t last = call_set(rt, ctx, 0x08C16084u, {position, word(rt, self + 4u), e + 0x10u, frame.at(1u)});
        (void)call_set(rt, ctx, 0x08C16074u, {word(rt, self), word(rt, self + 4u), frame.at(0u)});
        const std::uint32_t old_begin = word(rt, self);
        const std::uint32_t units = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - old_begin) >> 4);
        ctx.gpr[2] = units;
        if (old_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_begin, units << 4, 4u});
        ctx.gpr[2] = block + bytes;
        put(rt, self + 8u, block + bytes);
        put(rt, self + 4u, last);
        put(rt, self, block);
        return;
    }
    if (end != 0u) copy16(rt, end, end - 0x10u);
    const std::uint32_t top = word(rt, self + 4u);
    put(rt, self + 4u, top + 0x10u);
    const std::uint32_t last_old = top - 0x10u;
    const std::uint32_t temp = frame.at(4u);
    copy16(rt, temp, value);
    const std::uint32_t moved = static_cast<std::uint32_t>(signed_of(last_old - position) >> 4);
    if (signed_of(moved) > 0) {
        // copy_backward: the guard `a1 != a2` of the original compares two addresses 0x10 apart, never equal.
        std::uint32_t source = top - 0x20u, dest = last_old;
        for (std::uint32_t done = 0u; done != moved; ++done, source -= 0x10u, dest -= 0x10u) copy16(rt, dest, source);
    }
    ctx.gpr[2] = temp;
    if (position != temp) copy16(rt, position, temp);
}

// copy_element_0x40(dest, source): one 0x40-byte element of func_08C0CECC: the words at +4..+0x1C are read, then written, then func_089FB570(dest + 0x20, source + 0x20) and func_089FB570(dest + 0x30, source + 0x30) copy the two members behind them (the word at +0 stays).
inline void copy_element_0x40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t dest, std::uint32_t source) {
    const std::uint32_t w8 = word(rt, source + 8u), wc = word(rt, source + 0xCu), w10 = word(rt, source + 0x10u), w14 = word(rt, source + 0x14u), w18 = word(rt, source + 0x18u), w1c = word(rt, source + 0x1Cu), w4 = word(rt, source + 4u);
    put(rt, dest + 8u, w8);
    put(rt, dest + 0xCu, wc);
    put(rt, dest + 0x10u, w10);
    put(rt, dest + 0x14u, w14);
    put(rt, dest + 0x18u, w18);
    put(rt, dest + 0x1Cu, w1c);
    put(rt, dest + 4u, w4);  // the delay slot of the first jal
    (void)call_set(rt, ctx, 0x089FB570u, {dest + 0x20u, source + 0x20u});
    (void)call_set(rt, ctx, 0x089FB570u, {dest + 0x30u, source + 0x30u});
}

// func_08C0CECC(self, other): the assignment of a vector {begin, end, capacity} of 0x40-byte elements, built like vector_assign (func_08BECF40) but with the element count = byte length >> 6 and copy_element_0x40 as the element copy: other == self does nothing. room < count: block = func_08A5BC20(count << 6, 4), func_08C0CE08(other.begin, other.end, block, sp) copies into it, func_08BE5B74(self.begin, self.end, sp + 3) destroys the old ones,
// a non-null old block is freed with func_08A5BC54(begin, ((capacity - begin) >> 6) << 6, 4) and self = {block, block + bytes, block + bytes}. Otherwise size = (end - begin) >> 6: size >= count copies `count` elements and destroys the rest with func_08BE5988(begin + count * 0x40, end, sp + 2); size < count copies `size` and func_08C0CE08(other.begin + (self length & ~63), other.end, self.end, sp + 1) constructs the remaining ones.
// self.end = self.begin + count * 0x40 in the last two cases. v0 = self.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (other != self) {`; `if (old_block != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_block, units << 6, 4u});`; `if (!(size < count)) {`; `if (signed_of(count) > 0) {`; `for (std::uint32_t i = 0u; i != count; ++i, dest += 0x40u, other_begin += 0x40u) copy_element_0x40(r`; `(void)call_set(rt, ctx, 0x08BE5988u, {dest, signed_of(count) > 0 ? word(rt, self + 4u) : end, frame.`; `if (signed_of(size) > 0) {`; `for (std::uint32_t i = 0u; i != size; ++i, dest += 0x40u, other_begin += 0x40u) copy_element_0x40(rt`
void func_08C0CECC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    if (other != self) {
        std::uint32_t other_end = word(rt, other + 4u), other_begin = word(rt, other);
        const std::uint32_t begin = word(rt, self);
        std::uint32_t count = static_cast<std::uint32_t>(signed_of(other_end - other_begin) >> 6);
        const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - begin) >> 6);
        std::uint32_t used;
        if (room < count) {
            const std::uint32_t bytes = count << 6;
            const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
            (void)call_set(rt, ctx, 0x08C0CE08u, {other_begin, other_end, block, frame.at(0u)});
            (void)call_set(rt, ctx, 0x08BE5B74u, {word(rt, self), word(rt, self + 4u), frame.at(3u)});
            const std::uint32_t old_block = word(rt, self);
            const std::uint32_t units = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - old_block) >> 6);
            ctx.gpr[2] = units;
            if (old_block != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_block, units << 6, 4u});
            put(rt, self + 8u, block + bytes);
            put(rt, self, block);  // the delay slot of the j
            used = bytes;
        } else {
            const std::uint32_t end = word(rt, self + 4u);
            const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 6);
            if (!(size < count)) {
                std::uint32_t dest = begin;
                if (signed_of(count) > 0) {
                    for (std::uint32_t i = 0u; i != count; ++i, dest += 0x40u, other_begin += 0x40u) copy_element_0x40(rt, ctx, dest, other_begin);
                }
                (void)call_set(rt, ctx, 0x08BE5988u, {dest, signed_of(count) > 0 ? word(rt, self + 4u) : end, frame.at(2u)});
            } else {
                std::uint32_t dest = begin;
                if (signed_of(size) > 0) {
                    for (std::uint32_t i = 0u; i != size; ++i, dest += 0x40u, other_begin += 0x40u) copy_element_0x40(rt, ctx, dest, other_begin);
                    other_begin = word(rt, other);
                    other_end = word(rt, other + 4u);
                }
                const std::uint32_t this_end = word(rt, self + 4u);
                const std::uint32_t done_bytes = (this_end - word(rt, self)) & ~63u;
                (void)call_set(rt, ctx, 0x08C0CE08u, {other_begin + done_bytes, other_end, this_end, frame.at(1u)});
            }
            used = count << 6;
        }
        put(rt, self + 4u, word(rt, self) + used);
    }
    ctx.gpr[2] = self;
}

// func_089DDA80(self, other): the copy assignment of an object. func_0896E9F4(self, other) first; a null `other` ends there (v0 = that answer). Otherwise self[0xE8] = other[0xE8]; for the two blocks of three vectors at 0xEC, 0x104 (func_08C07E24(self + k, other + k) when they differ) and 0x11C (func_08C070FC), twice with a step of 0xC; func_08B855E0(self + 0x134, other + 0x134);
// the flag bits 0-6 of self[0x140] come from other[0x140] and bits 7-12 are cleared; self[0x144] = other[0x144]; the members at 0x150, 0x170 and 0x190 are assigned with func_089641C4(self + k, other + k) when they differ, each followed by self[k + 0x10] = other[k + 0x10]; self[0x1B0] / self[0x1B4] = other's, self[0x1B8] and self[0x1BC] = 0. v0 = the last call's answer, or other[0x144] when no call ran after it.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (other == 0u) return;`; `if (a != a_other) (void)call_set(rt, ctx, 0x08C07E24u, {a, a_other});`; `if (b != b_other) (void)call_set(rt, ctx, 0x08C07E24u, {b, b_other});`; `if (c != c_other) (void)call_set(rt, ctx, 0x08C070FCu, {c, c_other});`; `if (member != member_other) {`; `if (self + offset != other + offset) {`
void func_089DDA80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    (void)call_set(rt, ctx, 0x0896E9F4u, {self, other});
    if (other == 0u) return;
    ctx.gpr[2] = word(rt, other + 0xE8u);
    put(rt, self + 0xE8u, ctx.gpr[2]);
    std::uint32_t a = self + 0xECu, a_other = other + 0xECu, b = self + 0x104u, b_other = other + 0x104u, c = self + 0x11Cu, c_other = other + 0x11Cu;
    for (unsigned i = 0u; i != 2u; ++i, a += 0xCu, a_other += 0xCu, b += 0xCu, b_other += 0xCu, c += 0xCu, c_other += 0xCu) {
        if (a != a_other) (void)call_set(rt, ctx, 0x08C07E24u, {a, a_other});
        if (b != b_other) (void)call_set(rt, ctx, 0x08C07E24u, {b, b_other});
        if (c != c_other) (void)call_set(rt, ctx, 0x08C070FCu, {c, c_other});
    }
    (void)call_set(rt, ctx, 0x08B855E0u, {self + 0x134u, other + 0x134u});
    std::uint32_t flags = (word(rt, self + 0x140u) & ~0x7Fu) | (word(rt, other + 0x140u) & 0x7Fu);
    put(rt, self + 0x140u, flags);
    ctx.gpr[2] = (word(rt, other + 0x140u) >> 6) & 1u;
    std::uint32_t member = self + 0x150u, member_other = other + 0x150u;
    if (member != member_other) {
        (void)call_set(rt, ctx, 0x089641C4u, {member, member_other});
        put(rt, member + 0x10u, word(rt, member_other + 0x10u));
        flags = word(rt, self + 0x140u);
    }
    flags &= ~(0x3Fu << 7);
    put(rt, self + 0x140u, flags);
    ctx.gpr[2] = word(rt, other + 0x144u);
    put(rt, self + 0x144u, ctx.gpr[2]);
    if (member != member_other) {
        (void)call_set(rt, ctx, 0x089641C4u, {member, member_other});
        put(rt, member + 0x10u, word(rt, member_other + 0x10u));
    }
    for (const std::uint32_t offset : {0x170u, 0x190u}) {
        if (self + offset != other + offset) {
            (void)call_set(rt, ctx, 0x089641C4u, {self + offset, other + offset});
            put(rt, self + offset + 0x10u, word(rt, other + offset + 0x10u));
        }
    }
    const std::uint32_t f1 = word(rt, other + 0x1B4u), f0 = word(rt, other + 0x1B0u);
    put(rt, self + 0x1B8u, 0u);
    put(rt, self + 0x1B0u, f0);
    put(rt, self + 0x1B4u, f1);
    put(rt, self + 0x1BCu, 0u);
    ctx.fpr[0] = std::bit_cast<float>(f0);
    ctx.fpr[1] = std::bit_cast<float>(f1);
}

// The reallocating half shared by the fill-insert functions (copied from batch 93's fill_insert_grow, where it serves nine of them; here func_08C4BB10, func_08BF1768 and func_08C0E3B8): the vector {begin, end, capacity} of elements of (1 << shift) bytes receives `count` copies of *value before `position`.
// size = (end - begin) >> shift; a size over `limit` - count calls func_08B5ADDC(message) first; bytes = (size + max(size, count)) << shift (-(1 << shift) on overflow of the sum); the block comes from func_08A5BC20(bytes, 4) (the old block is freed with func_08A5BC54(begin, ((capacity - begin) >> shift) << shift, 4)) or, with
// `plain`, from func_08B35DA0(bytes) (nothing is allocated when the sum is 0; the old block is freed with func_08B35254(begin)). Then e = copy_block(begin, position, block, sp + 3), fill_block(e, count, value, sp + 2), last = copy_block(position, end', e + (count << shift), sp + 1), destroy_range(begin, end', sp), the old block is freed and the vector becomes {block, last, block + bytes}; v0 = block + bytes.
struct GrowRow {
    std::uint32_t copy_block, fill_block, destroy_range, message, limit;
    unsigned shift;
    bool plain;
    std::uint32_t align = 0u;  // non-zero: the block comes from func_08B35840(align, bytes) (no zero check) and is freed like the plain one
};
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (g.limit - size < count) (void)call_set(rt, ctx, 0x08B5ADDCu, {g.message});`; `const std::uint32_t larger = size < count ? count : size;`; `if (g.align != 0u)`; `if (!overflow) bytes = sum << g.shift;`; `} else if (!g.plain)`; `if (!overflow) bytes = sum << g.shift;`; `if (!overflow)`; `if (sum == 0u)`; `if (allocate)`; `if (!g.plain && g.align == 0u)`; `if (old_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_begin, static_cast<std::uint32_t>(signed_of(cap`; `if (old_begin != 0u) (void)call_set(rt, ctx, 0x08B35254u, {old_begin});`
inline void fill_insert_grow(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const GuestFrame &frame, std::uint32_t vector, std::uint32_t position, std::uint32_t count, std::uint32_t value, const GrowRow &g) {
    const std::uint32_t end = word(rt, vector + 4u);
    std::uint32_t begin = word(rt, vector);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> g.shift);
    put(rt, frame.at(0xCu), size);
    if (g.limit - size < count) (void)call_set(rt, ctx, 0x08B5ADDCu, {g.message});
    const std::uint32_t larger = size < count ? count : size;
    const std::uint32_t sum = size + larger;
    const bool overflow = sum < size;
    std::uint32_t bytes = 0u - (1u << g.shift);
    std::uint32_t block = 0u;
    if (g.align != 0u) {
        if (!overflow) bytes = sum << g.shift;
        block = call_set(rt, ctx, 0x08B35840u, {g.align, bytes});
        begin = word(rt, vector);
    } else if (!g.plain) {
        if (!overflow) bytes = sum << g.shift;
        block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    } else {
        bool allocate = true;
        if (!overflow) {
            bytes = 0u;
            if (sum == 0u) {
                allocate = false;
            } else {
                bytes = sum << g.shift;
            }
        }
        if (allocate) {
            block = call_set(rt, ctx, 0x08B35DA0u, {bytes});
            begin = word(rt, vector);
        }
    }
    const std::uint32_t first = call_set(rt, ctx, g.copy_block, {begin, position, block, frame.at(3u)});
    (void)call_set(rt, ctx, g.fill_block, {first, count, value, frame.at(2u)});
    const std::uint32_t last = call_set(rt, ctx, g.copy_block, {position, word(rt, vector + 4u), (count << g.shift) + first, frame.at(1u)});
    (void)call_set(rt, ctx, g.destroy_range, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
    const std::uint32_t old_begin = word(rt, vector), capacity = word(rt, vector + 8u);
    if (!g.plain && g.align == 0u) {
        ctx.gpr[4] = old_begin;
        ctx.gpr[2] = static_cast<std::uint32_t>(signed_of(capacity - old_begin) >> g.shift);
        if (old_begin != 0u) (void)call_set(rt, ctx, 0x08A5BC54u, {old_begin, static_cast<std::uint32_t>(signed_of(capacity - old_begin) >> g.shift) << g.shift, 4u});
    } else {
        ctx.gpr[2] = block + bytes;  // the delay slot of the beqz
        if (old_begin != 0u) (void)call_set(rt, ctx, 0x08B35254u, {old_begin});
    }
    ctx.gpr[2] = block + bytes;
    put(rt, vector + 8u, block + bytes);
    put(rt, vector + 4u, last);
    put(rt, vector, block);
}
// func_089F37AC(self, other): eight string slots self[0x24C..0x268] each take a fresh func_089E4C14(&t, D_08CF6ED8) value (func_08A5BAD8(slot, *t) unless the slot is the temporary; the temporaries are sp + 0x1C, 0x18, ... 0, each destroyed with func_08A5B9EC); then func_089E1F4C(self, other); a non-null `other` hands over its eight strings at 0x22C..0x248 (func_08B855E0(self + k, other + k)) and the floats at 0x26C / 0x270.
// v0 = the last call's answer.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (slot != temp) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp)});`; `if (other == 0u) return;`
void func_089F37AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    for (std::uint32_t i = 0u; i != 8u; ++i) {
        const std::uint32_t temp = frame.at(0x1Cu - 4u * i), slot = self + 0x24Cu + 4u * i;
        (void)call_set(rt, ctx, 0x089E4C14u, {temp, 0x08CF6ED8u});
        ctx.gpr[5] = word(rt, temp);  // the delay slot of the beq
        if (slot != temp) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temp)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temp});
    }
    (void)call_set(rt, ctx, 0x089E1F4Cu, {self, other});
    if (other == 0u) return;
    for (std::uint32_t offset = 0x22Cu; offset != 0x24Cu; offset += 4u) (void)call_set(rt, ctx, 0x08B855E0u, {self + offset, other + offset});
    const std::uint32_t f0 = word(rt, other + 0x270u), f1 = word(rt, other + 0x26Cu);
    put(rt, self + 0x270u, f0);
    put(rt, self + 0x26Cu, f1);
    ctx.fpr[0] = std::bit_cast<float>(f0);
    ctx.fpr[1] = std::bit_cast<float>(f1);
}

// func_08A2982C(self): a script class registers its methods. A string from D_08CABA2C (func_08A69B3C(sp, ..)) is made; a 0x38-byte block (func_08A5BC20(0x38, 4)) is built by func_08A7E914(block, sp) and stored at sp[4], func_08A76AB4(self, sp + 4) takes it and func_08A69D68(sp) releases the string; then for each of nine (name, handler) pairs
// func_08B86FB0(self[0], name, handler) runs (a zero self[0] first calls func_08A5BB7C(D_08CABA44), which returns at once). v0 = self.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CABA44u});`
void func_08A2982C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0u), 0x08CABA2Cu});
    const std::uint32_t block = call_set(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    (void)call_set(rt, ctx, 0x08A7E914u, {block, frame.at(0u)});
    put(rt, frame.at(4u), block);  // the delay slot of the next jal
    (void)call_set(rt, ctx, 0x08A76AB4u, {self, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    struct Pair {
        std::uint32_t name, handler;
    };
    static constexpr Pair kPairs[9] = {{0x08CABA4Cu, 0x08A29A4Cu}, {0x08CABA64u, 0x08A29A94u}, {0x08CABA6Cu, 0x08A29AD8u}, {0x08CABA74u, 0x08A29730u}, {0x08CABA8Cu, 0x08A29FD8u},
                                       {0x08CABAA4u, 0x08A29C9Cu}, {0x08CABAB4u, 0x08A29BECu}, {0x08CABAC4u, 0x08A29780u}, {0x08CABAE0u, 0x08A29B1Cu}};
    for (const Pair &pair : kPairs) {
        const std::uint32_t target = word(rt, self);
        ctx.gpr[5] = 0x004A0000u;  // the delay slot of the bnez
        if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CABA44u});
        (void)call_set(rt, ctx, 0x08B86FB0u, {target, pair.name, pair.handler});
    }
    ctx.gpr[2] = self;
}

// func_089E9F90(self, a1): func_08994B6C(self); self[0x1A8] = self[0x19C]; func_089E8408(self, a1, 0, 0); the vectors at self + 0x168 and self + 0x174 are cleared and released (func_08C02614 destroys their elements, func_08A5BC54 frees the blocks); two temporaries made by func_08969188(&t, D_08CF6ED8) go to func_089E5E90(self, a1, &t) and func_089E6218(self, a1, &t) and are destroyed (func_08A5B9EC);
// then the string slots self[0x184], self[0x188] and self[0x18C] each take a fresh func_08A76AAC(&t, D_08CF6ED8) value (func_08A5BAD8(slot, *t)) and the temporary is destroyed; finally a string made by func_08BEEF60(&t, self + 0x1E0) (starting as the shared empty cell + 0xC) is released. v0 = the last call's result.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (self + slot != frame.at(temp)) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + slot, word(rt, frame`
void func_089E9F90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], extra = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    (void)call_set(rt, ctx, 0x08994B6Cu, {self});
    put(rt, self + 0x1A8u, word(rt, self + 0x19Cu));
    (void)call_set(rt, ctx, 0x089E8408u, {self, extra, 0u, 0u});
    clear_vector_words(rt, ctx, frame, self + 0x168u, 0x28u, frame.at(2u));
    clear_vector_words(rt, ctx, frame, self + 0x174u, 0x1Cu, frame.at(1u));
    const std::uint32_t first = frame.at(0x18u), second = frame.at(0x14u);
    (void)call_set(rt, ctx, 0x08969188u, {first, 0x08CF6ED8u});
    (void)call_set(rt, ctx, 0x089E5E90u, {self, extra, first});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {first});
    (void)call_set(rt, ctx, 0x08969188u, {second, 0x08CF6ED8u});
    (void)call_set(rt, ctx, 0x089E6218u, {self, extra, second});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {second});
    for (const auto [slot, temp] : {std::pair<std::uint32_t, std::uint32_t>{0x184u, 0x10u}, {0x188u, 0xCu}, {0x18Cu, 8u}}) {
        (void)call_set(rt, ctx, 0x08A76AACu, {frame.at(temp), 0x08CF6ED8u});
        ctx.gpr[5] = word(rt, frame.at(temp));  // the delay slot of the beq
        if (self + slot != frame.at(temp)) (void)call_set(rt, ctx, 0x08A5BAD8u, {self + slot, word(rt, frame.at(temp))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(temp)});
    }
    put(rt, frame.at(4u), 0x08D74FD0u + 0xCu);  // the shared empty cell + 0xC, the delay slot of the jal
    (void)call_set(rt, ctx, 0x08BEEF60u, {frame.at(4u), self + 0x1E0u});
    release_cell(rt, ctx, word(rt, frame.at(4u)), frame.at(0u));
}

// func_08BD5870(self, source): a constructor from a counted pointer. self[0] = D_08CC5488 (0x08CC5488); self[4] / self[8] = source[0] / source[4] (a non-zero source[4] gets its counter at +4 incremented), self[0xC] / self[0x10] = source[8] / source[0xC] (counted the same way), self[0x14] / self[0x18] = 0; then the sub-objects of self are built from five-word values made by func_089099F4(&v, 0)
// (each value copied through a second five-word temporary): func_08BD3F30(self + 0x1C, 0, -1, sp + 7, &copy, 0), func_08BD3650(self + 0x48 / 0x64 / 0x80, 0, -1, sp + 6 / 5 / 4, -1, 0), func_08BD3F30(self + 0x9C, ..., sp + 3, ...), func_08BD3650(self + 0xC8, ..., sp + 2), func_08BD3F30(self + 0xE4, ..., sp + 1, ...) and func_08BD3F30(self + 0x110, ..., sp, ...);
// self[0x158] (byte) and the seven words self[0x13C..0x154] = 0. v0 = the last call's answer.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (counted_a != 0u) increment_counter(rt, counted_a + 4u);`; `if (counted_b != 0u) increment_counter(rt, counted_b + 4u);`
void func_08BD5870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], source = ctx.gpr[5];
    GuestFrame frame(ctx, 0xD0u);
    put(rt, self, 0x08CC5488u);
    put(rt, self + 4u, word(rt, source));
    const std::uint32_t counted_a = word(rt, source + 4u);
    put(rt, self + 8u, counted_a);
    if (counted_a != 0u) increment_counter(rt, counted_a + 4u);
    put(rt, self + 0xCu, word(rt, source + 8u));
    const std::uint32_t counted_b = word(rt, source + 0xCu);
    put(rt, self + 0x10u, counted_b);
    if (counted_b != 0u) increment_counter(rt, counted_b + 4u);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x18u, 0u);  // the delay slot of the jal
    // One of the six five-word values: the callee fills `value`, the original copies it word by word to `copy` and passes both a3 (a dead-stack address) and the copy.
    const auto build = [&](std::uint32_t value, std::uint32_t copy, std::uint32_t target, std::uint32_t tag) {
        (void)call_set(rt, ctx, 0x089099F4u, {frame.at(value), 0u});
        const std::uint32_t w0 = word(rt, frame.at(value)), w1 = word(rt, frame.at(value + 4u)), w2 = word(rt, frame.at(value + 8u)), w3 = word(rt, frame.at(value + 0xCu)), w4 = word(rt, frame.at(value + 0x10u));
        put(rt, frame.at(copy), w0);
        put(rt, frame.at(copy + 4u), w1);
        put(rt, frame.at(copy + 8u), w2);
        put(rt, frame.at(copy + 0xCu), w3);
        put(rt, frame.at(copy + 0x10u), w4);
        (void)call_set(rt, ctx, 0x08BD3F30u, {target, 0u, 0xFFFFFFFFu, frame.at(tag), frame.at(copy), 0u});
    };
    build(0x94u, 0x44u, self + 0x1Cu, 7u);
    (void)call_set(rt, ctx, 0x08BD3650u, {self + 0x48u, 0u, 0xFFFFFFFFu, frame.at(6u), 0xFFFFFFFFu, 0u});
    (void)call_set(rt, ctx, 0x08BD3650u, {self + 0x64u, 0u, 0xFFFFFFFFu, frame.at(5u), 0xFFFFFFFFu, 0u});
    (void)call_set(rt, ctx, 0x08BD3650u, {self + 0x80u, 0u, 0xFFFFFFFFu, frame.at(4u), 0xFFFFFFFFu, 0u});
    build(0x80u, 0x30u, self + 0x9Cu, 3u);
    (void)call_set(rt, ctx, 0x08BD3650u, {self + 0xC8u, 0u, 0xFFFFFFFFu, frame.at(2u), 0xFFFFFFFFu, 0u});
    build(0x6Cu, 0x1Cu, self + 0xE4u, 1u);
    build(0x58u, 8u, self + 0x110u, 0u);
    put8(rt, self + 0x158u, 0u);
    for (std::uint32_t offset = 0x13Cu; offset != 0x158u; offset += 4u) put(rt, self + offset, 0u);
}

// func_08C3D298(self, position, count, value): the insertion of `count` copies of the word *value before `position` into a vector of words {begin, end, capacity}. Nothing happens for count == 0 (v0 untouched). With room (capacity - end >= count words) the value is copied to sp + 4 and, with tail = (end - position) >> 2 words behind `position`: count < tail moves the last `count` words up
// (func_08B79BAC(end, end - 4 * count, 4 * count)), the overlap (end - 4 * count - position, low two bits cleared) up to end with func_08B79BAC(end - bytes, position, bytes) and fills [position, position + 4 * count) with func_08C3D0A4; otherwise func_08C3CF94(end, count - tail, &copy, sp + 1) fills past the end, the tail moves to end + 4 * (count - tail) and [position, end) is filled. The end moves in step.
// Without room: size = (end - begin) >> 2 (more than 0x3FFFFFFF - count calls func_08B5ADDC(D_08CB4930)), n = size + max(size, count) (0x3FFFFFFF on overflow; above 0x3FFFFFFF func_08B5A9C8() runs), block = func_08B62344(4 * n), the head is copied, func_08C3D09C(block + head, count, value, sp) fills, the tail is copied behind it, the old block is freed with func_08B64654 and the vector becomes {block, last, block + 4 * n}; v0 = block + 4 * n.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (count == 0u) return;`; `if (!(room < count)) {`; `if (count < tail) {`; `if (0x3FFFFFFFu - size < count) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4930u});`; `const std::uint32_t larger = size < count ? count : size;`; `if (total < size) total = 0x3FFFFFFFu;`; `if (0x3FFFFFFFu < total) (void)call_set(rt, ctx, 0x08B5A9C8u, {});`; `if (old_block != 0u) (void)call_set(rt, ctx, 0x08B64654u, {old_block});`
void func_08C3D298(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], position = ctx.gpr[5], count = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x10u), count);  // the delay slot of the beqz
    if (count == 0u) return;
    const std::uint32_t end = word(rt, self + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, self + 8u) - end) >> 2);
    if (!(room < count)) {
        const std::uint32_t copy = frame.at(4u);
        put(rt, copy, word(rt, value));
        const std::uint32_t tail_bytes = end - position;
        const std::uint32_t tail = static_cast<std::uint32_t>(signed_of(tail_bytes) >> 2);
        if (count < tail) {
            const std::uint32_t step = count << 2;
            (void)call_set(rt, ctx, 0x08B79BACu, {end, end - step, step});
            put(rt, self + 4u, word(rt, self + 4u) + step);  // the delay slot of the jal
            const std::uint32_t overlap = (end - step - position) & ~3u;
            (void)call_set(rt, ctx, 0x08B79BACu, {end - overlap, position, overlap});
            (void)call_set(rt, ctx, 0x08C3D0A4u, {position, position + step, copy});
        } else {
            (void)call_set(rt, ctx, 0x08C3CF94u, {end, count - tail, copy, frame.at(1u)});
            const std::uint32_t grown = word(rt, self + 4u) + ((count - tail) << 2);
            put(rt, self + 4u, grown);
            (void)call_set(rt, ctx, 0x08B79BACu, {grown, position, tail_bytes});
            put(rt, self + 4u, word(rt, self + 4u) + (tail << 2));
            (void)call_set(rt, ctx, 0x08C3D0A4u, {position, end, copy});
        }
        return;
    }
    const std::uint32_t begin = word(rt, self);
    const std::uint32_t size = static_cast<std::uint32_t>(signed_of(end - begin) >> 2);
    put(rt, frame.at(4u), size);
    if (0x3FFFFFFFu - size < count) (void)call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4930u});
    const std::uint32_t larger = size < count ? count : size;
    std::uint32_t total = size + larger;
    if (total < size) total = 0x3FFFFFFFu;  // the movn on the overflow
    const std::uint32_t bytes = total << 2;
    if (0x3FFFFFFFu < total) (void)call_set(rt, ctx, 0x08B5A9C8u, {});
    const std::uint32_t block = call_set(rt, ctx, 0x08B62344u, {bytes});
    std::uint32_t head = position - word(rt, self);
    (void)call_set(rt, ctx, 0x08B79BACu, {block, word(rt, self), head});
    std::uint32_t cursor = block + head;
    (void)call_set(rt, ctx, 0x08C3D09Cu, {cursor, word(rt, frame.at(0x10u)), value, frame.at(0u)});
    const std::uint32_t current_end = word(rt, self + 4u);
    cursor += word(rt, frame.at(0x10u)) << 2;
    const std::uint32_t rest = current_end - position;
    (void)call_set(rt, ctx, 0x08B79BACu, {cursor, position, rest});
    const std::uint32_t old_block = word(rt, self);
    cursor += rest;  // the delay slot of the beqz
    if (old_block != 0u) (void)call_set(rt, ctx, 0x08B64654u, {old_block});
    ctx.gpr[2] = block + bytes;
    put(rt, self + 8u, block + bytes);
    put(rt, self + 4u, cursor);
    put(rt, self, block);
}

// func_08A18270(self, target, a2, a3, t0, t1): four optional float cells are written through func_0896A9C4(target, ...) after the matching test func_08966FB0 / func_08966FEC / func_08967028 / func_08967064(target) answers non-zero; each call passes the float (copied to sp[0] first) in one of the four pointer arguments (a1, a2, a3, t0), the others zero.
// Which cell feeds which test depends on *(*self): zero maps (a2, a3, t0, t1) to the tests in order, otherwise (a3, a2, t1, t0). v0 = the last call's answer (the mask test or the setter).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (call_set(rt, ctx, kTests[i], {target}) == 0u) continue;`
void func_08A18270(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], target = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7], t0 = ctx.gpr[8], t1 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x30u);
    static constexpr std::uint32_t kTests[4] = {0x08966FB0u, 0x08966FECu, 0x08967028u, 0x08967064u};
    const std::uint32_t cells_zero[4] = {a2, a3, t0, t1}, cells_other[4] = {a3, a2, t1, t0};
    const bool plain = word(rt, word(rt, self)) == 0u;
    const std::uint32_t *cells = plain ? cells_zero : cells_other;
    for (unsigned i = 0u; i != 4u; ++i) {
        if (cells[i] == 0u) continue;
        if (call_set(rt, ctx, kTests[i], {target}) == 0u) continue;
        const std::uint32_t bits = word(rt, cells[i]);
        ctx.fpr[0] = std::bit_cast<float>(bits);
        put(rt, frame.at(0u), bits);  // the delay slot of the jal
        std::uint32_t arguments[4] = {0u, 0u, 0u, 0u};
        arguments[i] = frame.at(0u);
        (void)call_set(rt, ctx, 0x0896A9C4u, {target, arguments[0], arguments[1], arguments[2], arguments[3]});
    }
}

// func_08C4DA5C(self, value): a typed cell {type, pointer} receives `value`: the first of twelve type ids (each answered by a function: func_08B9DDB0 float, func_08C4D18C 64-bit through func_08B71CE0(value), func_08BC473C / func_08BC48B4 byte, func_08BC45C4 / func_08BC444C halfword, func_08BC42D4 / func_08B9E8D4 word, func_08BC415C / func_08BC3FE4 sign-extended 64-bit,
// func_08B9CEBC / func_08BC4A2C boolean byte) that equals self[0] while self[4] is non-null stores it there. v0 = 1 when a store happened, else 0.
void func_08C4DA5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    enum Kind { kFloat, kWide, kByte, kHalf, kWord, kSigned, kBool };
    struct Entry {
        std::uint32_t id;
        Kind kind;
    };
    static constexpr Entry kEntries[12] = {{0x08B9DDB0u, kFloat}, {0x08C4D18Cu, kWide}, {0x08BC473Cu, kByte}, {0x08BC48B4u, kByte}, {0x08BC45C4u, kHalf}, {0x08BC444Cu, kHalf},
                                           {0x08BC42D4u, kWord}, {0x08B9E8D4u, kWord}, {0x08BC415Cu, kSigned}, {0x08BC3FE4u, kSigned}, {0x08B9CEBCu, kBool}, {0x08BC4A2Cu, kBool}};
    for (const Entry &entry : kEntries) {
        const std::uint32_t current = word(rt, self);  // the delay slot of the jal
        const std::uint32_t id = call_set(rt, ctx, entry.id, {});
        const std::uint32_t cell = word(rt, self + 4u);
        if (current != id || cell == 0u) continue;
        ctx.gpr[5] = 1u;
        switch (entry.kind) {
            case kFloat:
                ctx.fpr[1] = std::bit_cast<float>(value);
                ctx.fpr[0] = static_cast<float>(signed_of(value));
                put_float(rt, cell, ctx.fpr[0]);
                break;
            case kWide: {
                (void)call_set(rt, ctx, 0x08B71CE0u, {value});
                put(rt, cell, ctx.gpr[2]);
                put(rt, cell + 4u, ctx.gpr[3]);
                break;
            }
            case kByte: put8(rt, cell, value); break;
            case kHalf: put16(rt, cell, value); break;
            case kWord: put(rt, cell, value); break;
            case kSigned:
                ctx.gpr[3] = static_cast<std::uint32_t>(signed_of(value) >> 31);
                put(rt, cell, value);
                put(rt, cell + 4u, ctx.gpr[3]);
                break;
            case kBool:
                ctx.gpr[2] = value != 0u ? 1u : 0u;
                put8(rt, cell, ctx.gpr[2]);
                break;
        }
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[5] = 0u;
    ctx.gpr[2] = 0u;
}

// func_0886CE44(self): a constructor that builds its object on the heap. self[0] = D_08CBF748 (0x08CBF748); s1 = func_08B62344(0x338) is initialised by func_08B93A70(s1); s1[0xC], [0x28], [0x2C], [0x30] (byte), [0x34], [0x44] = 0 around func_0887D080(s1 + 0x38) and func_0890B2F4(s1 + 0x60); the fields 0x68-0x84 are zeroed and func_0887DE44(s1 + 0x88, self) runs; s1[0xF8] takes D_08CC6838 and then D_08CC6850 (0x08CC6838 / 0x08CC6850)
// around the zeroed fields 0xEC-0x108 and func_08B28DCC(s1 + 0x110); the float block at 0x138-0x17C (1.0f at 0x148, 0x160, 0x164, the pair copied to 0x16C-0x178 and the low three bits of s1[0x17C] cleared) and sixteen zero words from 0x184; the two lists at 0x1C4 / 0x1DC / 0x1F4 (self-pointing heads); memset(s1 + 0x20C, 0, 0x100) and memset(s1 + 0x30C, 0, 0x20) with 14 bytes from D_08C9F754 (0x08C9F754) copied to s1 + 0x30C;
// zero words at 0x10-0x18 and 0x1C-0x24, two entries of (0x48, 0x50 = 0, 0x58 = s1 + 0x60), three halfwords at 0x330-0x334 and self[4] = s1. v0 = s1 + 0x330, v1 = 2, f0 = 1.0f.
void func_0886CE44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08CBF748u);
    const std::uint32_t s1 = call_set(rt, ctx, 0x08B62344u, {0x338u});
    (void)call_set(rt, ctx, 0x08B93A70u, {s1});
    const std::uint32_t s3 = s1 + 0x60u;
    put(rt, s1 + 0xCu, 0u);
    put(rt, s1 + 0x28u, 0u);
    put(rt, s1 + 0x2Cu, 0u);
    put8(rt, s1 + 0x30u, 0u);
    put(rt, s1 + 0x34u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0887D080u, {s1 + 0x38u});
    put(rt, s1 + 0x44u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0890B2F4u, {s3});
    put8(rt, s1 + 0x68u, 0u);
    put(rt, s1 + 0x6Cu, 0u);
    put(rt, s1 + 0x70u, 0u);
    put(rt, s1 + 0x74u, 0u);
    put(rt, s1 + 0x78u, 0u);
    put(rt, s1 + 0x7Cu, 0u);
    put(rt, s1 + 0x80u, 0u);
    put(rt, s1 + 0x84u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x0887DE44u, {s1 + 0x88u, self});
    put(rt, s1 + 0xF8u, 0x08CC6838u);
    put(rt, s1 + 0xECu, 0u);
    put(rt, s1 + 0xF0u, 0u);
    put(rt, s1 + 0xF4u, 0u);
    put(rt, s1 + 0xFCu, 0u);
    put(rt, s1 + 0x100u, 0u);
    put(rt, s1 + 0x104u, 0u);
    put(rt, s1 + 0x108u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B28DCCu, {s1 + 0x110u});
    const std::uint32_t one = 0x3F800000u;
    put(rt, s1 + 0x150u, 0u);
    const std::uint32_t v1 = s1 + 0x15Cu;
    put(rt, s1 + 0xF8u, 0x08CC6850u);
    put(rt, s1 + 0x138u, 0u);
    put(rt, s1 + 0x13Cu, 0u);
    put(rt, s1 + 0x144u, 0u);
    put(rt, s1 + 0x148u, one);
    put8(rt, s1 + 0x14Cu, 0u);
    put(rt, s1 + 0x154u, 0u);
    put(rt, s1 + 0x158u, 0u);
    put(rt, s1 + 0x15Cu, 0u);
    std::uint32_t cleared = word(rt, v1 + 0x20u) & ~7u;
    put(rt, v1 + 8u, one);
    put(rt, v1 + 0xCu, 0u);
    const std::uint32_t pair_a = word(rt, v1 + 8u), pair_b = word(rt, v1 + 0xCu);
    put(rt, v1 + 0x20u, cleared);
    put(rt, v1 + 0x10u, pair_a);
    put(rt, v1 + 0x14u, pair_b);
    put(rt, v1 + 4u, one);
    put(rt, v1 + 0x18u, pair_a);
    put(rt, v1 + 0x1Cu, pair_b);
    put(rt, s1 + 0x180u, 0u);
    for (std::uint32_t i = 0u, a2 = s1 + 0x184u; i != 0x10u; ++i, a2 += 4u) put(rt, a2, 0u);
    const std::uint32_t list_a = s1 + 0x1DCu, list_c = s1 + 0x1F4u, list_b = s1 + 0x1C4u;
    const std::uint32_t t0 = s1 + 0x1C8u, t1 = s1 + 0x1E0u, t2 = s1 + 0x1F8u;
    put(rt, list_b + 0x14u, 0u);
    const std::uint32_t s0 = s1 + 0x30Cu;
    put(rt, list_b + 0x10u, t0);
    put(rt, list_b + 0xCu, t0);
    put(rt, list_a + 0x10u, t1);
    put(rt, list_a + 0xCu, t1);
    put(rt, list_b + 4u, 0u);
    put(rt, list_b + 8u, 0u);
    put(rt, list_a + 4u, 0u);
    put(rt, list_a + 0x14u, 0u);
    put(rt, list_a + 8u, 0u);
    put(rt, list_c + 0x10u, t2);
    put(rt, list_c + 4u, 0u);
    put(rt, list_c + 0x14u, 0u);
    put(rt, list_c + 8u, 0u);
    put(rt, list_c + 0xCu, t2);
    put(rt, s1 + 0x32Cu, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B36018u, {s1 + 0x20Cu, 0u, 0x100u});
    (void)call_set(rt, ctx, 0x08B36018u, {s0, 0u, 0x20u});
    const std::uint32_t text = 0x08C9F754u;
    const std::uint32_t w0 = word_unaligned(rt, text), w1 = word_unaligned(rt, text + 4u), w2 = word_unaligned(rt, text + 8u);
    put_unaligned(rt, s0, w0);
    put_unaligned(rt, s0 + 4u, w1);
    put_unaligned(rt, s0 + 8u, w2);
    put8(rt, s0 + 0xCu, byte_u(rt, text + 0xCu));
    put8(rt, s0 + 0xDu, byte_u(rt, text + 0xDu));
    for (std::uint32_t a3 = s1, i = 0u; i != 3u; ++i, a3 += 4u) {
        put(rt, a3 + 0x10u, 0u);
        put(rt, a3 + 0x1Cu, 0u);
    }
    for (std::uint32_t v0 = s1, i = 0u; i != 2u; ++i, v0 += 4u) {
        put(rt, v0 + 0x48u, 0u);
        put(rt, v0 + 0x50u, 0u);
        put(rt, v0 + 0x58u, s3);
    }
    const std::uint32_t tail = s1 + 0x330u;
    put16(rt, s1 + 0x330u, 0u);
    put16(rt, tail + 4u, 0u);
    put16(rt, tail + 2u, 0u);
    put(rt, self + 4u, s1);
    ctx.gpr[2] = tail;
    ctx.gpr[3] = 2u;
    ctx.fpr[0] = 1.0f;
}

// func_088A4D18(self): a constructor. self[0] = D_08CC29A8 (0x08CC29A8), self[4] = 0, func_088A79AC(self + 8, 0), self[0x10] = D_08CC2AC8, self[0x14] = 1375.0f (0x44AF0000), self[0x18] = 0, self[0x20] = D_08CC2CB8 and the block at self + 0x20 (b): b[4] = D_08CC2DDC, b[8] = self, b[0x18] = 1, b[0x30] = 1.0f and zeroed fields; four 0x18-byte entries from self + 0x5C ({0, 1.0f, 0, byte 0}),
// b[0xA0] = 0x12345678, four 0x1C-byte entries from b + 0xA4 (D_08CC2DF8 / D_08CC2E78 vtable words and zeros), four 0xC-byte entries from b + 0x114 (D_08CC2E48), the fields 0x1A0-0x1BE of b, func_088AA504(b, 0), self[0x268] / [0x269], func_088AB254(self + 0x270), the block at self + 0x318 (D_08CC2C30, D_08CC2C4C, -1 halfword / words), func_08961058(self + 0x41C, self + 0x34C, self + 0x40C, 4),
// an identity matrix at sp (func_08B4E648(sp)) handed to func_088A42BC(self, sp), the fields 0x2D8-0x2F4 and memset(self + 0x1E8, 0, 0x80). v0 = the memset's answer.
void func_088A4D18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s2 = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    put(rt, s2, 0x08CC29A8u);
    put(rt, s2 + 4u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088A79ACu, {s2 + 8u, 0u});
    const std::uint32_t one = 0x3F800000u;
    put(rt, s2 + 0x10u, 0x08CC2AC8u);
    put(rt, s2 + 0x20u, 0x08CC2CB8u);
    const std::uint32_t a2 = s2 + 0x20u;
    put(rt, s2 + 0x14u, 0x44AF0000u);
    put(rt, s2 + 0x18u, 0u);
    put(rt, a2 + 4u, 0x08CC2DDCu);
    put8(rt, a2 + 0x18u, 1u);
    put(rt, a2 + 8u, s2);
    put(rt, a2 + 0x10u, 0u);
    put(rt, a2 + 0x1Cu, 0u);
    put(rt, a2 + 0x30u, one);
    put(rt, a2 + 0x34u, 0u);
    put8(rt, a2 + 0x38u, 0u);
    put8(rt, a2 + 0x39u, 0u);
    for (std::uint32_t a0 = s2 + 0x5Cu, i = 0u; i != 4u; ++i, a0 += 0x18u) {
        put(rt, a0, 0u);
        put(rt, a0 + 4u, one);
        put(rt, a0 + 8u, 0u);
        put8(rt, a0 + 0xCu, 0u);
    }
    put(rt, a2 + 0xA0u, 0x12345678u);
    for (std::uint32_t a0 = a2 + 0xA4u, i = 0u; i != 4u; ++i, a0 += 0x1Cu) {
        const std::uint32_t v0 = a0 + 0xCu;
        put(rt, a0, 0x08CC2DF8u);
        put(rt, a0 + 4u, 0u);
        put(rt, a0 + 8u, 0u);
        put(rt, a0 + 0xCu, 0x08CC2E78u);
        put(rt, v0 + 0xCu, 0u);
        put(rt, v0 + 4u, 0u);
        put(rt, v0 + 8u, 0u);
    }
    for (std::uint32_t v1 = a2 + 0x114u, i = 0u; i != 4u; ++i, v1 += 0xCu) {
        put(rt, v1, 0x08CC2E48u);
        put(rt, v1 + 4u, 0u);
        put(rt, v1 + 8u, 0u);
    }
    put(rt, a2 + 0x1A0u, 0u);
    put(rt, a2 + 0x1A4u, 0u);
    put(rt, a2 + 0x1A8u, 0u);
    put(rt, a2 + 0x1ACu, 0u);
    put8(rt, a2 + 0x1B0u, 0u);
    put(rt, a2 + 0x1B4u, 0u);
    put(rt, a2 + 0x1B8u, 0u);
    put16(rt, a2 + 0x1BCu, 0u);
    put16(rt, a2 + 0x1BEu, 0u);
    put(rt, a2 + 0x20u, 0u);
    put(rt, a2 + 0x24u, 0u);
    put(rt, a2 + 0x28u, 0u);
    put(rt, a2 + 0x2Cu, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088AA504u, {a2, 0u});
    put8(rt, s2 + 0x269u, 1u);
    put(rt, s2 + 0x1E0u, 0u);
    put(rt, s2 + 0x1E4u, 0u);
    put8(rt, s2 + 0x268u, 0u);
    put(rt, s2 + 0x26Cu, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088AB254u, {s2 + 0x270u});
    put(rt, s2 + 0x318u, 0x08CC2C30u);
    const std::uint32_t v1 = s2 + 0x318u, t0 = s2 + 0x32Cu, t1 = s2 + 0x338u;
    put8(rt, s2 + 0x2F8u, 1u);
    put(rt, s2 + 0x2FCu, 0u);
    put(rt, s2 + 0x310u, 0u);
    put(rt, v1 + 4u, 0x08CC2C4Cu);
    put16(rt, v1 + 0xCu, 0xFFFFFFFFu);
    put(rt, v1 + 8u, 0u);
    put(rt, v1 + 0x10u, 0u);
    put(rt, t0 + 8u, 0u);
    put(rt, t0 + 4u, 0u);
    put(rt, s2 + 0x32Cu, 0u);
    put(rt, s2 + 0x338u, 0u);
    put(rt, t1 + 8u, 0u);
    put(rt, t1 + 4u, 0u);
    put(rt, v1 + 0x2Cu, 0xFFFFFFFFu);
    put(rt, v1 + 0x30u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08961058u, {s2 + 0x41Cu, s2 + 0x34Cu, s2 + 0x40Cu, 4u});
    (void)call_set(rt, ctx, 0x08B4E648u, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x088A42BCu, {s2, frame.at(0u)});
    const std::uint32_t v0 = s2 + 0x2D8u, v1b = s2 + 0x2E8u;
    put(rt, s2 + 0x2D8u, 0u);
    put(rt, v0 + 0xCu, 0u);
    put(rt, v0 + 4u, 0u);
    put(rt, v0 + 8u, 0u);
    put(rt, s2 + 0x2E8u, 0u);
    put(rt, v1b + 0xCu, 0u);
    put(rt, v1b + 4u, 0u);
    put(rt, v1b + 8u, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B36018u, {s2 + 0x1E8u, 0u, 0x80u});
}

// parse_floats<Count, Literal, Defaults>(this, stream): func_089765C0 (2 floats, Literal 0x08CA2E50, Defaults 0x08C8BD9C) and func_089B31F0 (4 floats, Literal 0x08CA5618, Defaults 0x08C8C8F8). A copy of the string at this + 4 (func_08B88F44) is compared with the literal string (the common length, then the length difference when the bytes agree; both strings are released with release_cell).
// When they differ nothing else happens. Otherwise the cell sp[0x14] is set to the shared empty cell + 0xC (0x08D74FDC) and func_08987A74(stream, &cell) runs once (the answer is dropped); the Count floats at sp + 0x18 start as the words at Defaults; then, while func_08987A74(stream, &cell) answers neither -1 nor 0x7D, the float func_08B71EC8(func_08B77BF4(cell string)) is stored at the cursor, and when the cursor reaches the end func_0898682C(stream, 1) runs.
// An answer of -1 also calls func_0898682C(stream, 1); an answer of 0x7D does not. The floats are then stored to this[0xC][0..Count) and the cell is released. v0 = the last call's answer, or this[0xC] when none followed it.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `GuestFrame frame(ctx, Count == 2u ? 0x50u : 0x60u);`; `if (answer != 0u) difference = answer;`; `if (difference != 0u) return;`; `if (item == 0xFFFFFFFFu) {`; `if (item == 0x7Du) break;`
template <unsigned Count, std::uint32_t Literal, std::uint32_t Defaults>
void parse_floats(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], stream = ctx.gpr[5];
    GuestFrame frame(ctx, Count == 2u ? 0x50u : 0x60u);
    const std::uint32_t text = frame.at(8u), literal = frame.at(4u), cell = frame.at(0x14u);
    (void)call_set(rt, ctx, 0x08B88F44u, {text, self + 4u});
    (void)call_set(rt, ctx, 0x08B85EB8u, {literal, Literal, frame.at(3u)});
    const std::uint32_t text_data = word(rt, text), literal_data = word(rt, literal);
    const std::uint32_t text_length = word(rt, text_data - 0xCu), literal_length = word(rt, literal_data - 0xCu);
    put(rt, frame.at(0xCu), text_length);
    put(rt, frame.at(0x10u), literal_length);
    std::uint32_t difference = text_length - literal_length;
    const std::uint32_t answer = call_set(rt, ctx, 0x08B36170u, {text_data, literal_data, std::min(text_length, literal_length)});
    if (answer != 0u) difference = answer;  // the movn after the jal
    release_cell(rt, ctx, literal_data, frame.at(2u));
    release_cell(rt, ctx, word(rt, text), frame.at(1u));
    if (difference != 0u) return;
    put(rt, cell, 0x08D74FDCu);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08987A74u, {stream, cell});
    for (std::uint32_t i = 0u; i != Count; ++i) put(rt, frame.at(0x18u + 4u * i), word(rt, Defaults + 4u * i));
    std::uint32_t cursor = frame.at(0x18u);
    const std::uint32_t end = frame.at(0x18u + 4u * Count);
    for (;;) {
        const std::uint32_t item = call_set(rt, ctx, 0x08987A74u, {stream, cell});
        if (item == 0xFFFFFFFFu) {
            (void)call_set(rt, ctx, 0x0898682Cu, {stream, 1u});
            break;
        }
        ctx.fpr[0] = float_at(rt, frame.at(0x18u));  // the delay slot of the beq
        if (item == 0x7Du) break;
        (void)call_set(rt, ctx, 0x08B77BF4u, {word(rt, cell)});
        const std::uint32_t low = ctx.gpr[2], high = ctx.gpr[3];
        (void)call_set(rt, ctx, 0x08B71EC8u, {low, high});
        put_float(rt, cursor, ctx.fpr[0]);
        cursor += 4u;
        if (cursor == end) {
            (void)call_set(rt, ctx, 0x0898682Cu, {stream, 1u});
            break;
        }
    }
    ctx.fpr[0] = float_at(rt, frame.at(0x18u));
    for (std::uint32_t i = 0u; i != Count; ++i) {
        ctx.gpr[2] = word(rt, self + 0xCu);
        put(rt, ctx.gpr[2] + 4u * i, word(rt, frame.at(0x18u + 4u * i)));
        ctx.fpr[0] = float_at(rt, frame.at(0x18u + 4u * i));
    }
    release_cell(rt, ctx, word(rt, cell), frame.at(0u));
}

void func_089765C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { parse_floats<2u, 0x08CA2E50u, 0x08C8BD9Cu>(rt, ctx); }
void func_089B31F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { parse_floats<4u, 0x08CA5618u, 0x08C8C8F8u>(rt, ctx); }

// func_08C4BB10(vector, position, count, value): the insertion of `count` copies of the 4-byte element *value (read with lwl / lwr) before `position`, for a vector {begin, end, capacity} of elements that the original moves halfword-wise. Nothing happens for count == 0 (v0 untouched). With room (capacity - end >= count elements) the value is copied to sp + 8 and, with
// tail = (end - position) >> 2: count < tail moves the last `count` elements up with func_08C4AD60(end - 4 * count, end, end, sp + 6), grows the end, copies the elements between `position` and the old end up by `count` places from the back (two halfwords each) and fills [position, position + 4 * count) with func_08C4B014; otherwise func_08C4ADF0(end, count - tail, &temp, sp + 5) fills past the end,
// the end grows by (count - tail) * 4, func_08C4B040(position, end, new end, sp + 4) copies the tail, the end grows by tail * 4 and func_08C4B014(position, end, &temp) fills. v0 = the last call's answer. Without room fill_insert_grow does it (plain allocator func_08B35DA0, limit 0x3FFFFFFF, message D_08CB4AFC).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (count == 0u) return;`; `if (room < count) {`; `const std::uint32_t temp = frame.at(8u);`; `if (count < tail) {`; `if (signed_of(moves) > 0) {`; `for (std::uint32_t i = 0u; i != moves; ++i) {`
void func_08C4BB10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], count = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x10u), count);  // the delay slot of the beqz
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 2);
    if (room < count) {
        const GrowRow grow{0x08C4B23Cu, 0x08C4B20Cu, 0x08C498D4u, 0x08CB4AFCu, 0x3FFFFFFFu, 2u, true};
        fill_insert_grow(rt, ctx, frame, vector, position, count, value, grow);
        return;
    }
    const std::uint32_t temp = frame.at(8u);
    put(rt, temp, word_unaligned(rt, value));  // the delay slot of the beqz
    const std::uint32_t tail = static_cast<std::uint32_t>(signed_of(end - position) >> 2);
    if (count < tail) {
        const std::uint32_t step = count << 2;
        (void)call_set(rt, ctx, 0x08C4AD60u, {end - step, end, end, frame.at(6u)});
        const std::uint32_t source_end = end - step;
        const std::uint32_t moves = static_cast<std::uint32_t>(signed_of(source_end - position) >> 2);
        put(rt, vector + 4u, word(rt, vector + 4u) + step);  // the delay slot of the blez
        if (signed_of(moves) > 0) {
            std::uint32_t source = source_end, dest = end;
            for (std::uint32_t i = 0u; i != moves; ++i) {
                source -= 4u;
                const std::uint32_t low = half_u(rt, source), high = half_u(rt, source + 2u);
                dest -= 4u;
                put16(rt, dest, low);
                put16(rt, dest + 2u, high);
            }
        }
        (void)call_set(rt, ctx, 0x08C4B014u, {position, position + step, temp});
    } else {
        (void)call_set(rt, ctx, 0x08C4ADF0u, {end, count - tail, temp, frame.at(5u)});
        const std::uint32_t grown = word(rt, vector + 4u) + ((count - tail) << 2);
        put(rt, vector + 4u, grown);
        (void)call_set(rt, ctx, 0x08C4B040u, {position, end, grown, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (tail << 2));
        (void)call_set(rt, ctx, 0x08C4B014u, {position, end, temp});
    }
}

// func_08BF1768(vector, position, count, value): func_08C4BB10 for 8-byte elements (two floats copied with lwc1 / swc1, the temporary at sp + 0xC, the count at sp + 0x20): move_tail func_08BF0C9C, fill_uninit func_08BF0E04, copy_uninit func_08BF0DCC, fill func_08BF0DA0, the element move loop copies two words per element.
// Without room fill_insert_grow runs with the aligned allocator func_08B35840(0x10, bytes), limit 0x1FFFFFFF, message D_08CA43C8, copy func_08BF114C, fill func_08BF111C and destroy func_08BF0BF0.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (count == 0u) return;`; `if (room < count) {`; `const std::uint32_t temp = frame.at(0xCu);`; `if (count < tail) {`; `if (signed_of(moves) > 0) {`; `for (std::uint32_t i = 0u; i != moves; ++i, source -= 8u, dest -= 8u) {`
void func_08BF1768(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], count = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    put(rt, frame.at(0x20u), count);  // the delay slot of the beqz
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 3);
    if (room < count) {
        const GrowRow grow{0x08BF114Cu, 0x08BF111Cu, 0x08BF0BF0u, 0x08CA43C8u, 0x1FFFFFFFu, 3u, false, 0x10u};
        fill_insert_grow(rt, ctx, frame, vector, position, count, value, grow);
        return;
    }
    const std::uint32_t temp = frame.at(0xCu);
    const std::uint32_t value_high = word(rt, value + 4u), value_low = word(rt, value);
    put(rt, temp, value_low);
    put(rt, temp + 4u, value_high);  // the delay slot of the beqz
    const std::uint32_t tail = static_cast<std::uint32_t>(signed_of(end - position) >> 3);
    if (count < tail) {
        const std::uint32_t step = count << 3;
        (void)call_set(rt, ctx, 0x08BF0C9Cu, {end - step, end, end, frame.at(6u)});
        const std::uint32_t source_end = end - step;
        const std::uint32_t moves = static_cast<std::uint32_t>(signed_of(source_end - position) >> 3);
        put(rt, vector + 4u, word(rt, vector + 4u) + step);  // the delay slot of the blez
        if (signed_of(moves) > 0) {
            std::uint32_t source = source_end - 8u, dest = end - 8u;
            for (std::uint32_t i = 0u; i != moves; ++i, source -= 8u, dest -= 8u) {
                const std::uint32_t low = word(rt, source);
                put(rt, dest, low);
                const std::uint32_t high = word(rt, source + 4u);
                put(rt, dest + 4u, high);
            }
        }
        (void)call_set(rt, ctx, 0x08BF0DA0u, {position, position + step, temp});
    } else {
        (void)call_set(rt, ctx, 0x08BF0E04u, {end, count - tail, temp, frame.at(5u)});
        const std::uint32_t grown = word(rt, vector + 4u) + ((count - tail) << 3);
        put(rt, vector + 4u, grown);
        (void)call_set(rt, ctx, 0x08BF0DCCu, {position, end, grown, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (tail << 3));
        (void)call_set(rt, ctx, 0x08BF0DA0u, {position, end, temp});
    }
}

// func_08C0E3B8(vector, position, count, value): func_08C4BB10 for 32-byte elements: the temporary at sp + 0x10 is built by func_0896418C(&temp, value) and takes the word value[0x10] at sp + 0x20; the count is at sp + 0x30; move_tail func_08C0D9E0, fill_uninit func_08BE6028, copy_uninit func_08C0D968, fill func_08C0DB4C; the element move is func_089641C4(dest, source) (unless they are the same address) plus the word at +0x10.
// Without room fill_insert_grow runs with the aligned allocator func_08B35840(0x10, bytes), limit 0x07FFFFFF, message D_08CA8644, copy func_08C0D880, fill func_08C0D8F8 and destroy func_08BE5888.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (room < count) {`; `const std::uint32_t temp = frame.at(0x10u);`; `if (count < tail) {`; `if (signed_of(moves) > 0) {`; `for (std::uint32_t i = 0u; i != moves; ++i, word_source -= 0x20u, word_dest -= 0x20u) {`; `if (dest != source) {`
void func_08C0E3B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t vector = ctx.gpr[4], position = ctx.gpr[5], count = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    put(rt, frame.at(0x30u), count);  // the delay slot of the beqz
    if (count == 0u) return;
    const std::uint32_t end = word(rt, vector + 4u);
    const std::uint32_t room = static_cast<std::uint32_t>(signed_of(word(rt, vector + 8u) - end) >> 5);
    if (room < count) {
        const GrowRow grow{0x08C0D880u, 0x08C0D8F8u, 0x08BE5888u, 0x08CA8644u, 0x07FFFFFFu, 5u, false, 0x10u};
        fill_insert_grow(rt, ctx, frame, vector, position, count, value, grow);
        return;
    }
    const std::uint32_t temp = frame.at(0x10u);
    (void)call_set(rt, ctx, 0x0896418Cu, {temp, value});
    const std::uint32_t s0 = word(rt, vector + 4u);
    put(rt, frame.at(0x20u), word(rt, value + 0x10u));
    const std::uint32_t tail = static_cast<std::uint32_t>(signed_of(s0 - position) >> 5);
    if (count < tail) {
        const std::uint32_t step = count << 5;
        (void)call_set(rt, ctx, 0x08C0D9E0u, {s0 - step, s0, s0, frame.at(6u)});
        const std::uint32_t source_end = s0 - step;
        const std::uint32_t moves = static_cast<std::uint32_t>(signed_of(source_end - position) >> 5);
        put(rt, vector + 4u, word(rt, vector + 4u) + step);  // the delay slot of the blez
        if (signed_of(moves) > 0) {
            std::uint32_t word_dest = s0 - 0x10u, word_source = source_end - 0x10u, dest = s0, source = source_end;
            for (std::uint32_t i = 0u; i != moves; ++i, word_source -= 0x20u, word_dest -= 0x20u) {
                dest -= 0x20u;
                source -= 0x20u;
                if (dest != source) {
                    (void)call_set(rt, ctx, 0x089641C4u, {dest, source});
                    put(rt, word_dest, word(rt, word_source));
                }
            }
        }
        (void)call_set(rt, ctx, 0x08C0DB4Cu, {position, position + step, temp});
    } else {
        (void)call_set(rt, ctx, 0x08BE6028u, {s0, count - tail, temp, frame.at(5u)});
        const std::uint32_t grown = word(rt, vector + 4u) + ((count - tail) << 5);
        put(rt, vector + 4u, grown);
        (void)call_set(rt, ctx, 0x08C0D968u, {position, s0, grown, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (tail << 5));
        (void)call_set(rt, ctx, 0x08C0DB4Cu, {position, s0, temp});
    }
}

// func_08933C5C(self): s2 = self[0x48], s3 = self[0x4C]; func_089368DC(func_089363FC(), s2, s3) answers 0 -> v0 = 0 and nothing else. Otherwise func_08936584(func_089363FC(), s2, s3, sp) builds a record at sp and func_089324EC(self, a, b, c) (a byte-returning conversion) maps byte triples of it to the colour bytes self[0x131] / [0x132] / [0x13A]-[0x147] (the result of each call is stored in the delay slot of the next
// one, i.e. before it); plain copies: self[0x138] = sp[0x35], [0x139] = sp[0x38], [0x13C] = sp[0x46], [0x13D] = sp[0x47], halfwords self[0x164] = sp[0x12] and self[0x166] = sp[0x18]; with sp[0x7D] == 0 the halfword triples at sp[0..4] and sp[6..0xA] give self[0x134] and self[0x136]; with sp[0x7B] == 0 two more byte triples (sp[0x40..] and sp[0x43..]) give self[0x13A] / self[0x13B]. v0 = 1.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (usable == 0u) {`; `if (b(0x7Du) == 0u) {`; `if (b(0x7Bu) == 0u) {`
void func_08933C5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s1 = ctx.gpr[4];
    GuestFrame frame(ctx, 0xC0u);
    const std::uint32_t s2 = word(rt, s1 + 0x48u);
    const std::uint32_t s3 = word(rt, s1 + 0x4Cu);  // the delay slot of the jal: read before the call runs
    const std::uint32_t configuration = call_set(rt, ctx, 0x089363FCu, {s1});
    const std::uint32_t usable = call_set(rt, ctx, 0x089368DCu, {configuration, ctx.gpr[5], s2, s3});
    if (usable == 0u) {
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t second = call_set(rt, ctx, 0x089363FCu, {});
    (void)call_set(rt, ctx, 0x08936584u, {second, ctx.gpr[5], s2, s3, frame.at(0u)});
    const auto b = [&](std::uint32_t offset) { return byte_u(rt, frame.at(offset)); };
    const auto convert = [&](std::uint32_t one, std::uint32_t two, std::uint32_t three) { return call_set(rt, ctx, 0x089324ECu, {s1, one, two, three}); };
    const std::uint32_t v1 = b(0x38u), v0 = b(0x35u);
    put8(rt, s1 + 0x139u, v1);
    put8(rt, s1 + 0x138u, v0);  // the delay slot of the jal
    std::uint32_t result = convert(b(0x1Bu), b(0x1Du), b(0x1Cu));
    put8(rt, s1 + 0x131u, result);  // the delay slot of the next jal
    result = convert(b(0x1Eu), b(0x20u), b(0x1Fu));
    const std::uint32_t t0 = b(0x47u), v1b = b(0x46u), s0 = b(0x54u), a2 = b(0x57u);
    put8(rt, s1 + 0x13Du, t0);
    put8(rt, s1 + 0x13Cu, v1b);
    put8(rt, s1 + 0x132u, result);  // the delay slot of the next jal
    result = convert(1u, a2, s0);
    put8(rt, s1 + 0x13Eu, result);
    result = convert(1u, b(0x5Au), s0);
    put8(rt, s1 + 0x13Fu, result);
    const std::uint32_t s0b = b(0x5Bu);
    result = convert(1u, b(0x5Eu), s0b);
    put8(rt, s1 + 0x140u, result);
    result = convert(1u, b(0x61u), s0b);
    const std::uint32_t half_12 = half_u(rt, frame.at(0x12u)), s0c = b(0x62u);
    put16(rt, s1 + 0x164u, half_12);
    put8(rt, s1 + 0x141u, result);
    result = convert(1u, b(0x65u), s0c);
    put8(rt, s1 + 0x142u, result);
    result = convert(1u, b(0x68u), s0c);
    const std::uint32_t s0d = b(0x69u);
    put8(rt, s1 + 0x143u, result);
    result = convert(1u, b(0x6Cu), s0d);
    put8(rt, s1 + 0x144u, result);
    result = convert(1u, b(0x6Fu), s0d);
    put8(rt, s1 + 0x145u, result);
    result = convert(1u, b(0x75u), b(0x72u));
    put8(rt, s1 + 0x146u, result);
    result = convert(1u, b(0x79u), b(0x76u));
    put8(rt, s1 + 0x147u, result);
    const std::uint32_t half_18 = half_u(rt, frame.at(0x18u));
    put16(rt, s1 + 0x166u, half_18);  // the delay slot of the bnez
    if (b(0x7Du) == 0u) {
        result = convert(sext16(half_u(rt, frame.at(0u))), sext16(half_u(rt, frame.at(4u))), sext16(half_u(rt, frame.at(2u))));
        const std::uint32_t first = result;
        const std::uint32_t a1 = sext16(half_u(rt, frame.at(6u))), a2b = sext16(half_u(rt, frame.at(0xAu))), a3 = sext16(half_u(rt, frame.at(8u)));
        put16(rt, s1 + 0x134u, first);  // the delay slot of the second jal
        result = call_set(rt, ctx, 0x089324ECu, {s1, a1, a2b, a3});
        put16(rt, s1 + 0x136u, result);
    }
    ctx.gpr[3] = 1u;
    if (b(0x7Bu) == 0u) {
        result = convert(b(0x40u), b(0x42u), b(0x41u));
        const std::uint32_t a1 = b(0x43u), a2c = b(0x45u), a3 = b(0x44u);
        put8(rt, s1 + 0x13Au, result);  // the delay slot of the second jal
        result = call_set(rt, ctx, 0x089324ECu, {s1, a1, a2c, a3});
        ctx.gpr[3] = 1u;
        put8(rt, s1 + 0x13Bu, result);
    }
    ctx.gpr[2] = 1u;
}

// func_088A0BD8(self, a1, a2, f12): a constructor. With x = f12: self[0] = D_08CC2590 (0x08CC2590), self[4] = D_08CC26B4, self[8] = a1 (a2 when a1 is zero), self[0xC] = a2 (a1 when a2 is zero), self[0x10] = 1.0f - x, self[0x14] = x; four 0x1C-byte entries from self + 0x18 (+0 = D_08CC2C68, +0x14 = D_08CC4110, +0x18 = 0) and four 0x14-byte entries from self + 0x88 (D_08CC3630); then, four times (i = 0..3), func_088A8B14(sp + 0x14,
// self[8] + 0x170 + 0xB8 * i + 8, self[0xC] + 0x170 + 0xB8 * i + 8, x) builds a record whose fields go to the i-th entry of the first list (+4, +8, +0xC, +0x10, +0x18) and the i-th entry of the second list gets the two addresses self[8 / 0xC] + 0x450 + 0x24 * i + 8 (the first, or the second when the first is zero; the second when non-zero) with 1.0f - x and x. v0 = 4, v1 = the last second address, f0 = the last record's float at +0x24 of sp.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (a1 == 0u) a3 = a2;`; `if (a2 == 0u) a2 = a1;`; `if (v1 == 0u) a0 = v0;`; `if (v0 != 0u) v1 = v0;`
void func_088A0BD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s2 = ctx.gpr[4];
    std::uint32_t a1 = ctx.gpr[5], a2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    SavedFloat save20(ctx, 20u), save21(ctx, 21u);
    const float x = ctx.fpr[12];
    const float one_minus_x = 1.0f - x;
    ctx.fpr[0] = 1.0f;
    ctx.fpr[21] = one_minus_x;
    ctx.fpr[20] = x;
    std::uint32_t a3 = a1;
    if (a1 == 0u) a3 = a2;  // movz a3, a2, a1
    if (a2 == 0u) a2 = a1;  // movz a2, a1, a2
    put(rt, s2, 0x08CC2590u);
    put(rt, s2 + 4u, 0x08CC26B4u);
    put(rt, s2 + 0xCu, a2);
    put(rt, s2 + 8u, a3);
    put_float(rt, s2 + 0x10u, one_minus_x);
    put_float(rt, s2 + 0x14u, x);
    for (std::uint32_t v0 = s2 + 0x18u, i = 0u; i != 4u; ++i, v0 += 0x1Cu) {
        put(rt, v0, 0x08CC2C68u);
        put(rt, v0 + 0x14u, 0x08CC4110u);
        put(rt, v0 + 0x18u, 0u);
    }
    for (std::uint32_t v1 = s2 + 0x88u, i = 0u; i != 4u; ++i, v1 += 0x14u) put(rt, v1, 0x08CC3630u);
    const std::uint32_t fp = frame.at(0x14u), s7 = frame.at(0x28u), s6 = frame.at(0u);
    std::uint32_t s4 = 0x450u, s1 = s2 + 0x88u, s3 = 0x170u, s0 = s2 + 0x18u;
    for (std::uint32_t s5 = 0u; s5 != 4u;) {
        const std::uint32_t first = word(rt, s2 + 8u) + s3 + 8u, second = word(rt, s2 + 0xCu) + s3 + 8u;
        ctx.fpr[12] = x;
        ctx.gpr[5] = first;
        ctx.gpr[6] = second;
        ctx.gpr[4] = fp;  // the delay slot of the jal
        (void)call_keep(rt, ctx, 0x088A8B14u, {fp, first, second}, {x});
        const std::uint32_t record_4 = word(rt, frame.at(0x18u)), record_8 = word(rt, frame.at(0x1Cu)), record_c = word(rt, frame.at(0x20u)), record_10 = word(rt, frame.at(0x24u)), record_18 = word(rt, frame.at(0x2Cu));
        ++s5;
        put(rt, s0 + 4u, record_4);
        put(rt, s0 + 0xCu, record_c);
        s3 += 0xB8u;
        put(rt, s0 + 8u, record_8);
        put(rt, s0 + 0x10u, record_10);
        put(rt, s0 + 0x18u, record_18);
        s0 += 0x1Cu;
        std::uint32_t v1 = word(rt, s2 + 8u), v0 = word(rt, s2 + 0xCu);
        put(rt, s7, 0x08CC2760u);
        v1 = v1 + s4 + 8u;
        v0 = v0 + s4 + 8u;
        std::uint32_t a0 = v1;
        if (v1 == 0u) a0 = v0;  // movz a0, v0, v1
        if (v0 != 0u) v1 = v0;  // movn v1, v0, v0
        put(rt, s1 + 4u, a0);
        s4 += 0x24u;
        put(rt, s1 + 8u, v1);
        put(rt, fp, 0x08CBF428u);
        put(rt, s6, 0x08CC2790u);
        put_float(rt, s1 + 0xCu, one_minus_x);
        put_float(rt, s1 + 0x10u, x);
        s1 += 0x14u;
        put(rt, frame.at(4u), a0);
        put(rt, frame.at(8u), v1);
        put_float(rt, frame.at(0xCu), one_minus_x);
        put_float(rt, frame.at(0x10u), x);  // the delay slot of the bne
        ctx.fpr[0] = float_at(rt, frame.at(0x24u));
        ctx.gpr[2] = 4u;
        ctx.gpr[3] = v1;
    }
}

// func_089AF6A0(self): func_089C9C34(&a) fills the smart pointer a = sp[0x14] (a null one calls func_08A5BB7C(D_08CA518C), which returns at once); a string t = D_08CA51DC (func_08B85EB8(&t, ..)) is handed with a to func_089C5D04(&r, a, &t), self[0xB8] takes r (func_08A5BAD8(self + 0xB8, r) unless it is the temporary) and r and t are released; func_089A5CF0(&b) fills b = sp[0x18];
// self[0xB8] (null: func_08A5BB7C(D_08CA50D4)) gets func_08A7BFD0(self[0xB8], &b, name D_08CA51EC) and self[0xBC] = b; func_089E2350(&c) fills c = sp[0x1C]; b (null: func_08A5BB7C(D_08CA5170)) gets func_08A7BFD0(b, &c, name D_08CA51F8); self[0xC0] = c, or b[0xF4] when c is zero (with b null the original still reads the word at 0xF4); func_089AF5E8(self, 1), func_08BFA998(self + 0x12C, 10) and c, b, a are destroyed (func_08A5B9EC).
// v0 = the last call's result.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (s3 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA518Cu});`; `if (s4 != s1) (void)call_set(rt, ctx, 0x08A5BAD8u, {s4, word(rt, frame.at(0x10u))});`; `if (owner == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50D4u});`; `if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5170u});`; `if (ctx.gpr[2] != 0u) {`; `if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5170u});`
void func_089AF6A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t s5 = frame.at(0x14u), s4 = self + 0xB8u;
    (void)call_set(rt, ctx, 0x089C9C34u, {s5});
    const std::uint32_t s3 = word(rt, frame.at(0x14u));
    if (s3 == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA518Cu});
    const std::uint32_t s0 = frame.at(0xCu), s1 = frame.at(0x10u);
    (void)call_set(rt, ctx, 0x08B85EB8u, {s0, 0x08CA51DCu, frame.at(1u)});
    (void)call_set(rt, ctx, 0x089C5D04u, {s1, s3, s0});
    ctx.gpr[5] = word(rt, frame.at(0x10u));  // the delay slot of the beq
    if (s4 != s1) (void)call_set(rt, ctx, 0x08A5BAD8u, {s4, word(rt, frame.at(0x10u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {s1});
    release_cell(rt, ctx, word(rt, frame.at(0xCu)), frame.at(0u));
    const std::uint32_t b = frame.at(0x18u);
    (void)call_set(rt, ctx, 0x089A5CF0u, {b});
    const std::uint32_t owner = word(rt, self + 0xB8u);
    if (owner == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50D4u});
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(8u), 0x08CA51ECu});
    (void)call_set(rt, ctx, 0x08A7BFD0u, {owner, b, frame.at(8u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(8u)});
    const std::uint32_t c = frame.at(0x1Cu);
    put(rt, self + 0xBCu, word(rt, b));  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x089E2350u, {c});
    const std::uint32_t target = word(rt, b);
    if (target == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5170u});
    (void)call_set(rt, ctx, 0x08A69B3Cu, {frame.at(4u), 0x08CA51F8u});
    (void)call_set(rt, ctx, 0x08A7BFD0u, {target, c, frame.at(4u)});
    (void)call_set(rt, ctx, 0x08A69D68u, {frame.at(4u)});
    ctx.gpr[2] = word(rt, c);
    if (ctx.gpr[2] != 0u) {
        put(rt, self + 0xC0u, ctx.gpr[2]);  // the delay slot of the bnel
    } else {
        const std::uint32_t holder = word(rt, b);
        if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5170u});
        ctx.gpr[2] = word(rt, holder + 0xF4u);
        put(rt, self + 0xC0u, ctx.gpr[2]);
    }
    (void)call_set(rt, ctx, 0x089AF5E8u, {self, 1u});
    (void)call_set(rt, ctx, 0x08BFA998u, {self + 0x12Cu, 0xAu});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {c});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {b});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {s5});
}

// func_0888C2B8(self, key): s3 = func_08893D14(self[0x10]) records; for each index i, r = func_08893D38(self[0x10], i) gives a record whose three floats at +0, +4, +8 are copied to sp[0..8], func_08893B80(self[0x10], (int8)r[0xC]) and func_088DAF90(self[4], ..) give an index and, when it is not negative, func_088E0984(self[4], index, sp, r + 0x10, 0, 0) runs. Then self[0x1C] == 2 calls func_088928F4(self[0x14]), == 1 func_08892084(self[0x14]);
// when func_088D4DB8(key) answers non-zero, the byte 1 is stored at *(self[4][0x618]) and func_088D5A04(self[4] + 4, 0) and func_088D5AB8(self[4] + 4, 1) run. With n = func_088D9F80(self[4]): when self[0x34] is non-zero its byte at +0x1C34 becomes 1, its halfword at +0x1C32 = func_088DA160(self[4]) and, for each i < n, its halfword at +0x1558 + 2 * i is 0 when func_088D62FC(key) answers non-zero, else func_088DA1C8(self[4], i);
// then (whether or not self[0x34] was non-zero, for n > 0) every i < n gets a mode s0 = func_088D6358(key) (2 stays 2 when func_088D62FC(key) != 1, otherwise 1, or 2 when func_088DB2C8(self[4], i) answers non-zero), 0xC when func_088D4DB8(key) is non-zero, and func_088D31B0(self[4][0x620], i, s0) and func_088D3020(self[4][0x620], i, s0) run. Last: func_089178D0(self[8]) and func_088936A4(self[0x18], &sp[0x10]) with sp[0x10] = 4. v0 = the last call's answer.
// Both loops over n run only for n > 0 (blez at 0x0888C404 / 0x0888C460 jump to the final calls).
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (signed_of(s3) > 0) {`; `do {`; `if (signed_of(r2) >= 0) (void)call_set(rt, ctx, 0x088E0984u, {word(rt, s1 + 4u), r2, frame.at(0u), s`; `} while (s3 != s2);`; `if (mode == 2u) (void)call_set(rt, ctx, 0x088928F4u, {word(rt, s1 + 0x14u)});`; `else if (mode == 1u) (void)call_set(rt, ctx, 0x08892084u, {word(rt, s1 + 0x14u)});`; `if (call_set(rt, ctx, 0x088D4DB8u, {s4}) != 0u) {`; `if (v1 != 0u) {`; `if (answer != 0u) {`; `if (s0 == 2u) {`; `if (answer != 1u) {`; `if (call_set(rt, ctx, 0x088DB2C8u, {word(rt, s1 + 4u), s2}) != 0u) s0 = 2u;`; `if (answer != 0u) s0 = 0xCu;`
[[maybe_unused]] void func_0888C2B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s1 = ctx.gpr[4], s4 = ctx.gpr[5];
    GuestFrame frame(ctx, 0xD0u);
    std::uint32_t s3 = call_set(rt, ctx, 0x08893D14u, {word(rt, s1 + 0x10u)});
    if (signed_of(s3) > 0) {
        std::uint32_t s2 = 0u;
        do {
            const std::uint32_t index = s2;
            ++s2;  // the delay slot of the jal
            const std::uint32_t s0 = call_set(rt, ctx, 0x08893D38u, {word(rt, s1 + 0x10u), index});
            put(rt, frame.at(0u), 0u);
            put(rt, frame.at(4u), 0u);
            put(rt, frame.at(8u), 0u);
            put(rt, frame.at(0u), word(rt, s0));
            put(rt, frame.at(4u), word(rt, s0 + 4u));
            put(rt, frame.at(8u), word(rt, s0 + 8u));
            const std::uint32_t a1 = sext8(byte_u(rt, s0 + 0xCu));  // the delay slot of the jal
            const std::uint32_t r1 = call_set(rt, ctx, 0x08893B80u, {word(rt, s1 + 0x10u), a1});
            const std::uint32_t r2 = call_set(rt, ctx, 0x088DAF90u, {word(rt, s1 + 4u), r1});
            if (signed_of(r2) >= 0) (void)call_set(rt, ctx, 0x088E0984u, {word(rt, s1 + 4u), r2, frame.at(0u), s0 + 0x10u, 0u, 0u});
        } while (s3 != s2);
    }
    const std::uint32_t mode = word(rt, s1 + 0x1Cu);
    if (mode == 2u) (void)call_set(rt, ctx, 0x088928F4u, {word(rt, s1 + 0x14u)});
    else if (mode == 1u) (void)call_set(rt, ctx, 0x08892084u, {word(rt, s1 + 0x14u)});
    if (call_set(rt, ctx, 0x088D4DB8u, {s4}) != 0u) {
        const std::uint32_t v1 = word(rt, s1 + 4u);
        put8(rt, word(rt, v1 + 0x618u), 1u);
        (void)call_set(rt, ctx, 0x088D5A04u, {word(rt, s1 + 4u) + 4u, 0u});
        (void)call_set(rt, ctx, 0x088D5AB8u, {word(rt, s1 + 4u) + 4u, 1u});
    }
    s3 = call_set(rt, ctx, 0x088D9F80u, {word(rt, s1 + 4u)});
    const std::uint32_t v1 = word(rt, s1 + 0x34u);
    if (v1 != 0u) {
        put8(rt, v1 + 0x1C34u, 1u);
        const std::uint32_t s0 = word(rt, s1 + 0x34u);  // the delay slot of the jal
        const std::uint32_t v0 = call_set(rt, ctx, 0x088DA160u, {word(rt, s1 + 4u)});
        put16(rt, s0 + 0x1C32u, v0);  // the delay slot of the blez
        if (signed_of(s3) > 0) {
            for (std::uint32_t s2 = 0u; s2 != s3; ++s2) {
                const std::uint32_t answer = call_set(rt, ctx, 0x088D62FCu, {s4});
                ctx.gpr[5] = s2;
                if (answer != 0u) {
                    put16(rt, (s2 << 1) + word(rt, s1 + 0x34u) + 0x1558u, 0u);
                } else {
                    const std::uint32_t slot = call_set(rt, ctx, 0x088DA1C8u, {word(rt, s1 + 4u), s2});
                    const std::uint32_t holder = word(rt, s1 + 0x34u);  // the delay slot of the jal
                    put16(rt, (s2 << 1) + holder + 0x1558u, slot);
                }
            }
        }
    }
    if (signed_of(s3) > 0) {
        for (std::uint32_t s2 = 0u; s2 != s3;) {
            std::uint32_t s0 = call_set(rt, ctx, 0x088D6358u, {s4});
            if (s0 == 2u) {
                const std::uint32_t answer = call_set(rt, ctx, 0x088D62FCu, {s4});
                ctx.gpr[5] = s2;  // the delay slot of the bne
                if (answer != 1u) {
                    s0 = 2u;
                } else {
                    s0 = 1u;  // the delay slot of the jal
                    if (call_set(rt, ctx, 0x088DB2C8u, {word(rt, s1 + 4u), s2}) != 0u) s0 = 2u;
                }
            }
            const std::uint32_t answer = call_set(rt, ctx, 0x088D4DB8u, {s4});
            const std::uint32_t v1b = word(rt, s1 + 4u);
            if (answer != 0u) s0 = 0xCu;  // movn s0, a0, v0
            (void)call_set(rt, ctx, 0x088D31B0u, {word(rt, v1b + 0x620u), s2, s0});
            const std::uint32_t index = s2;
            ++s2;  // the delay slot of the jal
            (void)call_set(rt, ctx, 0x088D3020u, {word(rt, word(rt, s1 + 4u) + 0x620u), index, s0});
        }
    }
    (void)call_set(rt, ctx, 0x089178D0u, {word(rt, s1 + 8u)});
    put(rt, frame.at(0x10u), 4u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088936A4u, {word(rt, s1 + 0x18u), frame.at(0x10u)});
}

// func_08AE6620(config): initialises the pool table at D_08D40978. memset(table, 0, 0xC0); func_08AD8960(config, name, table + off, size) registers four entries (D_08C91930 at +0x28 / 0x20, D_08C91940 at +0x40 / 0x40, D_08C91950 at +0x58 / 0x40, D_08C91960 at +0xBC / 0); when table[0xBC] is non-zero and bit 2 of the word at D_08CDFB3C is set,
// func_08AD8464(D_08C91970) runs; func_08AE65FC is called for the sixteen 6-byte records from D_08D409D4; then three pools are allocated with func_08AD84B4(count * size, 1) (table[0x28] * 160, table[0x40] * 0xB8, table[0x58] * 40 bytes) and each one is linked with func_08AD8C18(list head, block, count, size) (heads at table + 0x14, 0x2C, 0x44; table[4] / [0x14] / [0x2C] / [0x44] point at themselves, [0xC] / [0x1C] / [0x34] / [0x4C] = -1, [0x10] / [0x20] / [0x38] / [0x50] = 0); a failed allocation frees the earlier blocks (func_08AD85C8 of table[0x54], [0x3C], [0x24] when non-zero) and answers 0. On success table[0] = 1 and v0 = 1.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (word(rt, s0 + 0xBCu) != 0u && (word(rt, 0x08CDFB3Cu) & 4u) != 0u) (void)call_set(rt, ctx, 0x08AD`; `for (const std::uint32_t slot : {0x54u, 0x3Cu, 0x24u}) {`; `if (block != 0u) (void)call_set(rt, ctx, 0x08AD85C8u, {block});`; `if (block == 0u) return fail();`
void func_08AE6620(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s1 = ctx.gpr[4];
    const std::uint32_t s0 = 0x08D40978u;
    (void)call_set(rt, ctx, 0x08B36018u, {s0, 0u, 0xC0u});
    (void)call_set(rt, ctx, 0x08AD8960u, {s1, 0x08C91930u, s0 + 0x28u, 0x20u});
    (void)call_set(rt, ctx, 0x08AD8960u, {s1, 0x08C91940u, s0 + 0x40u, 0x40u});
    (void)call_set(rt, ctx, 0x08AD8960u, {s1, 0x08C91950u, s0 + 0x58u, 0x40u});
    (void)call_set(rt, ctx, 0x08AD8960u, {s1, 0x08C91960u, s0 + 0xBCu, 0u});
    if (word(rt, s0 + 0xBCu) != 0u && (word(rt, 0x08CDFB3Cu) & 4u) != 0u) (void)call_set(rt, ctx, 0x08AD8464u, {0x08C91970u});
    std::uint32_t record = 0x08D409D4u;
    for (std::int32_t left = 0xF; left >= 0; --left, record += 6u) (void)call_set(rt, ctx, 0x08AE65FCu, {record});
    const auto fail = [&]() {
        for (const std::uint32_t slot : {0x54u, 0x3Cu, 0x24u}) {
            const std::uint32_t block = word(rt, s0 + slot);
            if (block != 0u) (void)call_set(rt, ctx, 0x08AD85C8u, {block});
        }
        ctx.gpr[3] = 0u;
        ctx.gpr[2] = 0u;
    };
    const std::uint32_t minus_one = 0xFFFFFFFFu;
    std::uint32_t count = word(rt, s0 + 0x28u);
    std::uint32_t block = call_set(rt, ctx, 0x08AD84B4u, {((count << 2) + count) << 5, 1u});
    put(rt, s0 + 0x24u, block);  // the delay slot of the beqz
    if (block == 0u) return fail();
    std::uint32_t v1 = s0 + 0x14u, v0 = s0 + 4u;
    put(rt, s0 + 4u, v0);
    put(rt, v0 + 0xCu, 0u);
    put(rt, v0 + 4u, v0);
    put(rt, v0 + 8u, minus_one);
    put(rt, s0 + 0x14u, v1);
    put(rt, v1 + 4u, v1);
    put(rt, v1 + 8u, minus_one);
    put(rt, v1 + 0xCu, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08AD8C18u, {v1, block, word(rt, s0 + 0x28u), 0xA0u});
    count = word(rt, s0 + 0x40u);
    block = call_set(rt, ctx, 0x08AD84B4u, {count * 0xB8u, 1u});
    put(rt, s0 + 0x3Cu, block);  // the delay slot of the beqz
    if (block == 0u) return fail();
    v0 = s0 + 0x2Cu;
    put(rt, s0 + 0x2Cu, v0);
    put(rt, v0 + 4u, v0);
    put(rt, v0 + 8u, minus_one);
    put(rt, v0 + 0xCu, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08AD8C18u, {v0, block, word(rt, s0 + 0x40u), 0xB8u});
    count = word(rt, s0 + 0x58u);
    block = call_set(rt, ctx, 0x08AD84B4u, {((count << 2) + count) << 3, 1u});
    put(rt, s0 + 0x54u, block);  // the delay slot of the beqz
    if (block == 0u) return fail();
    v0 = s0 + 0x44u;
    put(rt, s0 + 0x44u, v0);
    put(rt, v0 + 8u, minus_one);
    put(rt, v0 + 4u, v0);
    put(rt, v0 + 0xCu, 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08AD8C18u, {v0, block, word(rt, s0 + 0x58u), 0x28u});
    put(rt, s0, 1u);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = 1u;
}

// func_08B22694(self): the push of one value into a ring buffer, under the LwMutex at self + 4 (func_08B344E0 locks, func_08B3457C unlocks). With the buffer q = self + 0x40: a full queue (q[0x18] == q[0xC]) is grown first (func_08B24A60(q, 1)); the slot s3 = q[0x10] + q[4] * q[0x14] (the mtlo / madd pair: lo = q[0x10], lo += q[4] * q[0x14]) receives the value: with self[0x2C] non-zero its virtual slot 8
// (a0 = a 0xED-byte buffer at sp + 0xF0, a1 = self[0x2C]) builds it and the 0xED bytes are copied to s3 (through sp: dead stack); otherwise func_08B215B8(s3, 0, 0, 0, 0) builds the default value. Then func_08B21C74(s3), func_08B24A8C(q, 1), and when self[0x64] is zero: self[0x5C] = s3 when self[0x60] is D_08D68B7C (0x08D68B7C), else the old self[0x60]; self[0x60] = s3 and self[0x68] = 1. v0 = the unlock call's answer.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (source != 0u) {`; `if (word(rt, s2 + 0x64u) == 0u) {`
void func_08B22694(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s2 = ctx.gpr[4];
    GuestFrame frame(ctx, 0x200u);
    const std::uint32_t s4 = s2 + 4u, s1 = s2 + 0x40u;
    (void)call_set(rt, ctx, 0x08B344E0u, {s4});
    if (word(rt, s1 + 0x18u) == word(rt, s1 + 0xCu)) (void)call_set(rt, ctx, 0x08B24A60u, {s1, 1u});
    const std::uint32_t lo = word(rt, s1 + 0x10u);
    const std::uint32_t s3 = lo + word(rt, s1 + 4u) * word(rt, s1 + 0x14u);  // mtlo / madd / mflo: the low word
    const std::uint32_t source = word(rt, s2 + 0x2Cu);
    if (source != 0u) {
        const std::uint32_t buffer = frame.at(0xF0u);
        (void)call_set(rt, ctx, slot_of(rt, source, 8u), {buffer, source});
        copy_bytes(rt, s3, buffer, 0xEDu);
    } else {
        (void)call_set(rt, ctx, 0x08B215B8u, {s3, 0u, 0u, 0u, 0u});
    }
    (void)call_set(rt, ctx, 0x08B21C74u, {s3});
    (void)call_set(rt, ctx, 0x08B24A8Cu, {s1, 1u});
    if (word(rt, s2 + 0x64u) == 0u) {
        std::uint32_t v0 = word(rt, s2 + 0x60u);
        const std::uint32_t v1 = v0 ^ 0x08D68B7Cu;
        if (v1 == 0u) v0 = s3;  // movz v0, s3, v1
        put(rt, s2 + 0x68u, 1u);
        put(rt, s2 + 0x5Cu, v0);
        put(rt, s2 + 0x60u, s3);
    }
    (void)call_set(rt, ctx, 0x08B3457Cu, {s4});
}

// func_088D1858(self, a1, a2, a3, t0, t1): (a1 = a vector of four floats, a2 = s6, a3 = s4, t0 = s5, t1 = s3.) self[0x1AC] = self[0x19C], self[4] = self[0], self[0x10] = self[0x14], the 16 bytes at self + 0x19C go to self + 0x1AC (+4, +8, +0xC too); the bytes self[0x24], [0x34], [0x5A], [0x5B] and the words [0x28], [0x5C] are zeroed; the vector *a1 becomes self[0x19C..0x1A8] and a copy at sp + 0x10 that func_08B4A8D4(sp + 0x10, sp + 0x10, self + 0x1AC) rewrites in place,
// then it is copied to self[0x1BC..0x1C8]; *a1 is also copied to sp[0..0xC]. Then func_088D0B0C(self, sp, s4), func_088D0A14(self, s4), func_088D0A60(self, s4), func_088D12AC(self, s5, s3, s4), func_088D0AB4(self) (non-zero: func_088DB0AC(s5, s3)), func_088D13E0(self, s3, s5, s4), func_088D0D78(self, s3, s5), func_088D0DC4(self, s3, s5, s6, s4) and func_088D0CCC(self, s3, s5) run;
// with self[0x27] == 1 or self[0x25] == 1 the function returns (v0 = 1), otherwise func_088D10E4(self, s6, s5, s3, s7), func_088D1034(self, s6, s4, s7) (s7 = self[0x2C] read early) and func_088D0AD8(self) end it. v0 = the last call's answer.
// Branches the routes do not take (from `tools.decomp_gate coverage` of the fast gate, read against the asm by hand, branch targets and delay slots included): `if (call_set(rt, ctx, 0x088D0AB4u, {s2}) != 0u) {`; `if (ctx.gpr[2] == 1u) return;`
void func_088D1858(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s2 = ctx.gpr[4], s0 = ctx.gpr[5], s6 = ctx.gpr[6], s4 = ctx.gpr[7], s5 = ctx.gpr[8], s3 = ctx.gpr[9];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t v1 = s2 + 0x19Cu, v0 = s2 + 0x1ACu, s1 = s2 + 0x1BCu;
    const std::uint32_t f0_19c = word(rt, s2 + 0x19Cu), f1_0 = word(rt, s2), a0_14 = word(rt, s2 + 0x14u);
    put(rt, s2 + 0x1ACu, f0_19c);
    put(rt, s2 + 0x10u, a0_14);
    put(rt, s2 + 4u, f1_0);
    put(rt, v0 + 4u, word(rt, v1 + 4u));
    put(rt, v0 + 8u, word(rt, v1 + 8u));
    put(rt, v0 + 0xCu, word(rt, v1 + 0xCu));
    put8(rt, s2 + 0x24u, 0u);
    put8(rt, s2 + 0x34u, 0u);
    put(rt, s2 + 0x28u, 0u);
    put8(rt, s2 + 0x5Au, 0u);
    put8(rt, s2 + 0x5Bu, 0u);
    put(rt, s2 + 0x5Cu, 0u);
    const std::uint32_t s7 = word(rt, s2 + 0x2Cu);
    put(rt, s2 + 0x19Cu, word(rt, s0));
    put(rt, v1 + 4u, word(rt, s0 + 4u));
    put(rt, frame.at(0x14u), word(rt, s0 + 4u));
    put(rt, v1 + 8u, word(rt, s0 + 8u));
    put(rt, frame.at(0x18u), word(rt, s0 + 8u));
    put(rt, v1 + 0xCu, word(rt, s0 + 0xCu));
    put(rt, frame.at(0x1Cu), word(rt, s0 + 0xCu));
    put(rt, frame.at(0x10u), word(rt, s2 + 0x19Cu));  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08B4A8D4u, {frame.at(0x10u), frame.at(0x10u), v0});
    const std::uint32_t f4 = word(rt, frame.at(0x18u)), f2 = word(rt, frame.at(0x14u)), f1 = word(rt, frame.at(0x1Cu)), f0 = word(rt, frame.at(0x10u));
    put(rt, s2 + 0x1BCu, f0);
    put(rt, s1 + 8u, f4);
    put(rt, s1 + 0xCu, f1);
    put(rt, s1 + 4u, f2);
    const std::uint32_t g3 = word(rt, s0 + 0xCu), g0 = word(rt, s0), g1 = word(rt, s0 + 4u), g2 = word(rt, s0 + 8u);
    put(rt, frame.at(0u), g0);
    put(rt, frame.at(4u), g1);
    put(rt, frame.at(8u), g2);
    put(rt, frame.at(0xCu), g3);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x088D0B0Cu, {s2, frame.at(0u), s4});
    (void)call_set(rt, ctx, 0x088D0A14u, {s2, s4});
    (void)call_set(rt, ctx, 0x088D0A60u, {s2, s4});
    (void)call_set(rt, ctx, 0x088D12ACu, {s2, s5, s3, s4});
    ctx.gpr[7] = s4;  // the delay slot of the beqz
    if (call_set(rt, ctx, 0x088D0AB4u, {s2}) != 0u) {
        (void)call_set(rt, ctx, 0x088DB0ACu, {s5, s3});
        ctx.gpr[7] = s4;
    }
    (void)call_set(rt, ctx, 0x088D13E0u, {s2, s3, s5, s4});
    (void)call_set(rt, ctx, 0x088D0D78u, {s2, s3, s5});
    (void)call_set(rt, ctx, 0x088D0DC4u, {s2, s3, s5, s6, s4});
    (void)call_set(rt, ctx, 0x088D0CCCu, {s2, s3, s5});
    ctx.gpr[2] = byte_u(rt, s2 + 0x27u);
    ctx.gpr[3] = 1u;
    if (ctx.gpr[2] == 1u) return;
    ctx.gpr[2] = byte_u(rt, s2 + 0x25u);
    if (ctx.gpr[2] == 1u) return;
    (void)call_set(rt, ctx, 0x088D10E4u, {s2, s6, s5, s3, s7});
    (void)call_set(rt, ctx, 0x088D1034u, {s2, s6, s4, s7});
    (void)call_set(rt, ctx, 0x088D0AD8u, {s2});
}

// func_08A1F5D4(out, count, args): a script native that builds a string cell from one or two object arguments (the objects are args[0] and args[1]; a null one first calls func_08A5BB7C(D_08CAA480), which returns at once, and is then read anyway). With count == 1: the virtual slot 0xC of args[0] fills the cell sp[0xC] (a0 = &cell, a1 = the object), t = func_08A4D2D4(D_08CF68E8, cell, 0) and func_08A97A6C(&r, t) fills the cell r = sp[8]; out takes it (func_08A5BAD8(out, r) unless out is the temporary)
// and r is destroyed (func_08A5B9EC); the cell sp[0xC] is released (release_cell with flag sp + 2). With count == 2 the same runs with the second object's slot 0xC filling the cell sp[0x10], func_08A4D2D4(D_08CF68E8, sp[0xC], sp[0x10]) and r = sp[4]; the cells sp[0x10] and sp[0xC] are released (flags sp + 1 and sp). Any other count leaves v0 = 2. v0 = the last call's answer.
[[maybe_unused]] void func_08A1F5D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], count = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = 1u;
    const auto object = [&](std::uint32_t index) {
        const std::uint32_t value = word(rt, args + 4u * index);
        if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
        return value;
    };
    if (count == 1u) {
        const std::uint32_t first = object(0u);
        const std::uint32_t slot = word(rt, word(rt, first) + 0xCu);
        const std::uint32_t result = frame.at(8u);
        (void)call_set(rt, ctx, slot, {frame.at(0xCu), first});
        const std::uint32_t found = call_set(rt, ctx, 0x08A4D2D4u, {word(rt, 0x08CF68E8u), word(rt, frame.at(0xCu)), 0u});
        (void)call_set(rt, ctx, 0x08A97A6Cu, {result, found});
        ctx.gpr[5] = word(rt, frame.at(8u));
        if (out != result) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(8u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {result});
        release_cell(rt, ctx, word(rt, frame.at(0xCu)), frame.at(2u));
        return;
    }
    ctx.gpr[2] = 2u;
    if (count != 2u) return;
    const std::uint32_t first = object(0u);
    (void)call_set(rt, ctx, word(rt, word(rt, first) + 0xCu), {frame.at(0xCu), first});
    const std::uint32_t second = object(1u);
    const std::uint32_t slot = word(rt, word(rt, second) + 0xCu);
    const std::uint32_t result = frame.at(4u);  // the delay slot of the jalr
    (void)call_set(rt, ctx, slot, {frame.at(0x10u), second});
    const std::uint32_t found = call_set(rt, ctx, 0x08A4D2D4u, {word(rt, 0x08CF68E8u), word(rt, frame.at(0xCu)), word(rt, frame.at(0x10u))});
    (void)call_set(rt, ctx, 0x08A97A6Cu, {result, found});
    ctx.gpr[5] = word(rt, frame.at(4u));
    if (out != result) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(4u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {result});
    release_cell(rt, ctx, word(rt, frame.at(0x10u)), frame.at(1u));
    release_cell(rt, ctx, word(rt, frame.at(0xCu)), frame.at(0u));
}

// func_08A1F328(out, count, args): a script native, nothing happens for count <= 0 (v0 untouched). With o = args[0] (a null one first calls func_08A5BB7C(D_08CAA480), then is read anyway): s2 = the virtual slot 0xDC of o (a1 = 0) and v = its slot 0xD8; func_08A1EC18(s2, v, &sp[4], &sp[8], &sp[0xC], &sp[0x10]) fills four words and o' = sp[4] replaces o; func_08AF771C(o', sp[0xC]) and func_08AF7B0C(o', -1) run;
// two strings are made with func_08A76AAC(&t, D_08CF6ED8) at sp + 0x14 and sp + 0x18 and filled by func_08A44D9C(sp + 0x20, o', sp[8], 0) and func_08A44D9C(sp + 0x20, sp[0xC], sp[0x10], 0) (each handed over with func_08A5BAD8(t, sp[0x20]) unless the same cell, and the temporary sp + 0x20 destroyed); the map cell sp + 0x1C is made by func_08A6BA44, func_08A6BBE8(&m, D_08CAA624) and func_08A6BBE8(&m, D_08CAA62C) give two slots that take
// the strings (func_08A5BAD8(slot, string) unless the slot is the string), func_08A76ABC(&r, &m) fills r = sp[0] which out takes (unless it is the temporary); r, m and the two strings are destroyed (func_08A5B9EC). v0 = the last call's answer.
void func_08A1F328(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t s5 = ctx.gpr[4], count = ctx.gpr[5], args = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    if (signed_of(count) <= 0) return;
    const auto object = [&]() {
        const std::uint32_t value = word(rt, args);
        if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
        return value;
    };
    std::uint32_t s0 = object();
    const std::uint32_t slot_dc = word(rt, word(rt, s0) + 0xDCu);
    ctx.gpr[5] = 0u;  // the delay slot of the jalr
    const std::uint32_t s2 = call_set(rt, ctx, slot_dc, {s0, 0u});
    s0 = object();
    const std::uint32_t slot_d8 = word(rt, word(rt, s0) + 0xD8u);
    const std::uint32_t s3 = frame.at(0x14u), s1 = frame.at(0x20u);
    const std::uint32_t v = call_set(rt, ctx, slot_d8, {s0});
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0x10u), 0u);  // the delay slot of the jal
    (void)call_set(rt, ctx, 0x08A1EC18u, {s2, v, frame.at(4u), frame.at(8u), frame.at(0xCu), frame.at(0x10u)});
    s0 = word(rt, frame.at(4u));  // the first word func_08A1EC18 stored: the object the next calls work on
    (void)call_set(rt, ctx, 0x08AF771Cu, {s0, word(rt, frame.at(0xCu))});
    (void)call_set(rt, ctx, 0x08AF7B0Cu, {s0, 0xFFFFFFFFu});
    (void)call_set(rt, ctx, 0x08A76AACu, {s3, 0x08CF6ED8u});
    (void)call_set(rt, ctx, 0x08A44D9Cu, {s1, s0, word(rt, frame.at(8u)), 0u});
    ctx.gpr[5] = word(rt, frame.at(0x20u));  // the delay slot of the beq
    if (s3 != s1) (void)call_set(rt, ctx, 0x08A5BAD8u, {s3, word(rt, frame.at(0x20u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {s1});
    const std::uint32_t second = frame.at(0x18u);
    (void)call_set(rt, ctx, 0x08A76AACu, {second, 0x08CF6ED8u});
    (void)call_set(rt, ctx, 0x08A44D9Cu, {s1, word(rt, frame.at(0xCu)), word(rt, frame.at(0x10u)), 0u});
    if (second != s1) (void)call_set(rt, ctx, 0x08A5BAD8u, {second, word(rt, frame.at(0x20u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {s1});
    const std::uint32_t cell = frame.at(0x1Cu);
    (void)call_set(rt, ctx, 0x08A6BA44u, {cell});
    std::uint32_t slot = call_set(rt, ctx, 0x08A6BBE8u, {cell, 0x08CAA624u});
    ctx.gpr[5] = word(rt, frame.at(0x14u));  // the delay slot of the beq
    if (slot != s3) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x14u))});
    slot = call_set(rt, ctx, 0x08A6BBE8u, {cell, 0x08CAA62Cu});
    ctx.gpr[5] = word(rt, frame.at(0x18u));  // the delay slot of the beq
    if (slot != second) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x18u))});
    (void)call_set(rt, ctx, 0x08A76ABCu, {frame.at(0u), cell});
    ctx.gpr[5] = word(rt, frame.at(0u));  // the delay slot of the beq
    if (s5 != frame.at(0u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {s5, word(rt, frame.at(0u))});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {cell});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {second});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {s3});
}

}  // namespace
}  // namespace hot_batch_94
}  // namespace gt::game

#define HOT94(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_94::name, gt::ReturnKind::kind)
#define HOT94N(address, name, kind) GT_DECOMP_NESTED(address, gt::game::hot_batch_94::name, gt::ReturnKind::kind)
HOT94(0x088D2320u, func_088D2320, Word);
HOT94(0x0881B0D8u, func_0881B0D8, Word);
HOT94(0x08B32B5Cu, func_08B32B5C, Word);
HOT94(0x0897B108u, func_0897B108, Word);
HOT94(0x088D671Cu, func_088D671C, Word);
// Not registered (its only reached call stops on kernel isolation (LwMutex contention): not comparable, compared 0 on all five routes): HOT94(0x088FE08Cu, func_088FE08C, Word);
HOT94(0x08BACF7Cu, func_08BACF7C, Word);
HOT94(0x08BECF40u, func_08BECF40, Word);
HOT94(0x08BF4CF0u, func_08BF4CF0, Word);
HOT94(0x08C16984u, func_08C16984, Word);
HOT94(0x08C0CECCu, func_08C0CECC, Word);
HOT94(0x089DDA80u, func_089DDA80, Word);
HOT94(0x089F37ACu, func_089F37AC, Word);
HOT94(0x08A2982Cu, func_08A2982C, Word);
HOT94(0x089E9F90u, func_089E9F90, Word);
HOT94(0x08BD5870u, func_08BD5870, Word);
HOT94(0x08C3D298u, func_08C3D298, Word);
HOT94(0x08A18270u, func_08A18270, Word);
HOT94(0x08C4DA5Cu, func_08C4DA5C, Word);
HOT94(0x0886CE44u, func_0886CE44, Word);
HOT94(0x088A4D18u, func_088A4D18, Word);
HOT94(0x089765C0u, func_089765C0, Word);
HOT94(0x089B31F0u, func_089B31F0, Word);
HOT94(0x08C4BB10u, func_08C4BB10, Word);
HOT94(0x08BF1768u, func_08BF1768, Word);
HOT94(0x08C0E3B8u, func_08C0E3B8, Word);
HOT94(0x08933C5Cu, func_08933C5C, Word);
HOT94(0x088A0BD8u, func_088A0BD8, Word);
HOT94(0x089AF6A0u, func_089AF6A0, Word);
// Not registered (compared once on the options route of the coverage run (equal, changed memory) but not reached in the later fast runs, and its sabotage was not detected: its call comes at the end of a route and depends on the wall-clock): HOT94(0x0888C2B8u, func_0888C2B8, Word);
HOT94(0x08AE6620u, func_08AE6620, Word);
HOT94(0x08B22694u, func_08B22694, Word);
HOT94(0x088D1858u, func_088D1858, Word);
// Not registered (every case stops on the import stub 0x08C7ACA0 (sceLibFont::0xA834319D): not comparable, compared 0 on all five routes (24-40 calls each)): HOT94(0x08A1F5D4u, func_08A1F5D4, Word);
HOT94(0x08A1F328u, func_08A1F328, Word);
