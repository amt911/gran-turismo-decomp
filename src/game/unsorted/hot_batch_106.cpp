// Hot batch 106: more of GT's graphics library (0x08B40000-0x08B70000), written by hand from `tools.disasm`; role words
// describe access shapes, none is a game name. VFPU bodies are one include/gt/vfpu.hpp helper per instruction, the
// mnemonic next to it (batch 105 explains why).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_106 {
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
inline std::uint32_t half_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load16(address); }
inline void put16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }
// The value `lui` leaves for an address built by a relocated lui/addiu (or lui/load/store) pair: the high half, rounded up when the low half is negative.
inline constexpr std::uint32_t lui_of(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
inline std::int32_t signed_of(std::uint32_t value) { return static_cast<std::int32_t>(value); }
inline std::uint32_t byte_u(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load8(address); }
inline void put8(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline std::uint32_t sext8(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(value))); }
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee does not take stays as the caller had it.
inline std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// A guest call with integer and float argument registers set like the original sets them before the jal (a0-a3, t0-t3 and f12-f15): v0 / v1 / f0 of the callee stay in the context.
inline GuestResult call_setf(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call the way the compiler issues it: v1 = the vtable (object[0]), v0 = the slot's function, then jalr v0. A callee that returns without setting v0 hands the function's address on to the caller's code, so both registers are set like the original sets them.
inline GuestResult call_virtual_full(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                     std::initializer_list<std::uint32_t> arguments) {
    const std::uint32_t table = word(rt, object);
    const std::uint32_t function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments, {});
}
inline std::uint32_t call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t offset,
                                  std::initializer_list<std::uint32_t> arguments) {
    return call_virtual_full(rt, ctx, object, offset, arguments).v0;
}




// The reference-count steps of the shared-object holder: `ll/sc` on a word of guest memory (single-threaded here, the host never preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful `sc`); true when the old value was 1 (the count reached zero).
inline bool count_down(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address) {
    const std::uint32_t old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}

// One shared object of func_08B8FE44's elements: its use count (+4) goes down; when that was the last use the virtual slot 8 runs, then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    if (count_down(rt, ctx, object + 4u)) {
        (void)call_virtual_full(rt, ctx, object, 8u, {object});
        if (count_down(rt, ctx, object + 8u)) (void)call_virtual_full(rt, ctx, object, 0xCu, {object});
    }
}

// The end of a destructor that releases its last shared object with a tail jump: `jr t9` to the object's virtual slot 0xC with a0 = the object (v0 = its vtable, t9 = the slot's function), after the frame is gone.
inline void tail_virtual_slot_c(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object) {
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[2] = vtable;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {object});
}

// ---- functions ----





// func_08B565F8(a0): sets bit 1 of the state block's flag word (+0x6C) and tail-calls func_08B56530(0x08D6D0E0, a0).
// (The function list runs it on; the code after the jump belongs to the next functions.)
void func_08B565F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = kGuState;
    ctx.gpr[2] = word(rt, kGuFlags) | 2u;
    ctx.gpr[5] = ctx.gpr[4];
    put(rt, kGuFlags, ctx.gpr[2]);
    tail_call(ctx, 0x08B56530u, {kGuCommandBlock});
}

// func_08B53898(prim, uv_matrix, count, uvs): takes count 16-byte vertices from the top of the state block's vertex area
// (+0x60 moves down), each one's texture coordinates are (u, v) of `uvs` (8 bytes each) transformed by two rows of
// uv_matrix (12-byte stride: vdot.t with C100 and C010, z = the float at +0x2C4, the last lane 0 / 1), written with
// svl / svr; then the GE commands BASE, VADDR, VTYPE 0x19C and PRIM(prim & 7, count) go to the display list. count 0
// does nothing.
void func_08B53898(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    const std::uint32_t prim = ctx.gpr[4], uv_matrix = ctx.gpr[5], count = ctx.gpr[6];
    std::uint32_t uvs = ctx.gpr[7];
    ctx.gpr[9] = count;
    ctx.gpr[6] = prim;  // the beqz's delay slot
    if (count == 0u) return;
    ctx.gpr[8] = lui_of(kGuState);
    ctx.gpr[2] = kGuState;
    ctx.gpr[4] = word(rt, kGuState + 0x60u);
    ctx.gpr[3] = count << 4;
    const std::uint32_t vertices = ctx.gpr[4] - ctx.gpr[3];
    ctx.gpr[10] = vertices;
    put(rt, kGuState + 0x60u, vertices);
    v::unary(ctx, 0u, 0x01u, 0x00u, 4u);              // vmov.q C010, C000
    v::load_scalar(rt, ctx, 0x02u, kGuState + 0x2C4u); // lv.s S020
    v::load_scalar(rt, ctx, 0x00u, uv_matrix);
    v::load_scalar(rt, ctx, 0x20u, uv_matrix + 0xCu);
    v::load_scalar(rt, ctx, 0x40u, uv_matrix + 0x18u);
    v::vector_init(ctx, 0x62u, 1u, false);             // vzero.s S023
    v::load_scalar(rt, ctx, 0x01u, uv_matrix + 4u);
    v::vector_init(ctx, 0x43u, 1u, true);              // vone.s S032
    v::load_scalar(rt, ctx, 0x21u, uv_matrix + 0x10u);
    v::load_scalar(rt, ctx, 0x41u, uv_matrix + 0x1Cu);
    v::unary(ctx, 0u, 0x04u, 0x00u, 4u);              // vmov.q C100, C000 (the blez's delay slot)
    if (static_cast<std::int32_t>(count) > 0) {
        ctx.gpr[2] = vertices;
        ctx.gpr[3] = 0u;
        do {
            v::unary(ctx, 0u, 0x00u, 0x03u, 4u);      // vmov.q C000, C030
            v::load_scalar(rt, ctx, 0x00u, uvs);
            v::load_scalar(rt, ctx, 0x20u, uvs + 4u);
            ctx.gpr[3] += 1u;
            uvs += 8u;
            v::unary(ctx, 0u, 0x03u, 0x00u, 4u);      // vmov.q C030, C000
            v::dot(ctx, 0x22u, 0x04u, 0x03u, 3u);     // vdot.t S021, C100, C030
            v::dot(ctx, 0x42u, 0x01u, 0x03u, 3u);     // vdot.t S022, C010, C030
            v::store_quad_left(rt, ctx, 0x02u, ctx.gpr[2] + 0xCu);
            v::store_quad_right(rt, ctx, 0x02u, ctx.gpr[2]);
            ctx.gpr[2] += 0x10u;  // the bne's delay slot
        } while (count != ctx.gpr[3]);
    }
    ctx.gpr[7] = uvs;
    ctx.gpr[8] = kGuState;
    const std::uint32_t list = word(rt, kGuListPointer);
    ctx.gpr[7] = list;
    ctx.gpr[4] = 0x10000000u | ((vertices >> 24) & 0xFFu) << 16;
    ctx.gpr[5] = 0x01000000u | (vertices & 0x00FFFFFFu);
    ctx.gpr[6] = 0x12000000u | 0x19Cu;
    ctx.gpr[3] = 0x04000000u | (prim & 7u) << 16 | (count & 0xFFFFu);
    ctx.gpr[2] = list + 0x10u;
    put(rt, list, ctx.gpr[4]);
    put(rt, list + 4u, ctx.gpr[5]);
    put(rt, list + 8u, ctx.gpr[6]);
    put(rt, list + 0xCu, ctx.gpr[3]);
    put(rt, kGuListPointer, ctx.gpr[2]);
}

// func_08B54D28(cache, blob) -> the address the blob's GE commands are to be called at. blob: +0 data, +4 relocation
// table (8-byte entries), +8 size, +0xC relocation count (half), +0x10 its cache entry. cache: +4 enabled, +8 a stamp.
// With the cache off or a stamp below 2 the blob is used in place (+0x10 cleared). A valid entry (its +0 is this blob)
// gets the stamp and gives its copy (+4); a stale one is cleared. While the word at 0x08D6CDE8 is below 500, a new entry
// comes from func_08B54984(cache, blob, size): the data is copied there 64 bytes at a time through VFPU C010 / C000
// (lv.q / sv.q; the `cache 0x18` before each block has no host effect), then each relocation entry whose first byte is 0
// rewrites two words of the copy (at the entry's word 0 >> 8) as BASE (0x10, high byte) and VADDR (0x01, low 24 bits) of
// copy + the entry's word 1. Anything else returns the data in place.
void func_08B54D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    const std::uint32_t cache = ctx.gpr[4], blob = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t data = word(rt, blob);
    const std::uint32_t size = word(rt, blob + 8u);
    const auto done = [&]() { ctx.gpr[2] = data; };
    ctx.gpr[2] = word(rt, cache + 4u);
    if (ctx.gpr[2] == 0u) {
        put(rt, blob + 0x10u, 0u);
        return done();
    }
    ctx.gpr[4] = word(rt, cache + 8u);
    ctx.gpr[2] = ctx.gpr[4] < 2u ? 1u : 0u;
    if (ctx.gpr[2] != 0u) {
        put(rt, blob + 0x10u, 0u);
        return done();
    }
    ctx.gpr[3] = word(rt, blob + 0x10u);  // the beql's delay slot (taken)
    if (ctx.gpr[3] != 0u) {
        ctx.gpr[2] = word(rt, ctx.gpr[3]);
        if (ctx.gpr[2] == blob) {
            data = word(rt, ctx.gpr[3] + 4u);
            put(rt, ctx.gpr[3] + 0xCu, ctx.gpr[4]);
            return done();
        }
        put(rt, blob + 0x10u, 0u);  // the bnel's delay slot (taken)
    }
    ctx.gpr[3] = lui_of(0x08D6CDE8u);
    ctx.gpr[2] = word(rt, 0x08D6CDE8u) < 500u ? 1u : 0u;
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = data;
        return;
    }
    const std::uint32_t entry = call_set(rt, ctx, 0x08B54984u, {cache, blob, size});
    ctx.gpr[9] = entry;
    if (entry == 0u) return done();
    ctx.gpr[2] = word(rt, cache + 8u);
    const std::uint32_t copy = word(rt, entry + 4u);
    ctx.gpr[10] = copy;
    put(rt, entry + 0xCu, ctx.gpr[2]);
    std::uint32_t to = copy, from = data, left = size;
    put(rt, blob + 0x10u, entry);  // the blez's delay slot
    if (static_cast<std::int32_t>(left) > 0) {
        do {
            v::load_quad(rt, ctx, 0x01u, from);
            left -= 0x40u;
            v::store_quad(rt, ctx, 0x01u, to);
            v::load_quad(rt, ctx, 0x00u, from + 0x10u);
            v::store_quad(rt, ctx, 0x00u, to + 0x10u);
            v::load_quad(rt, ctx, 0x01u, from + 0x20u);
            v::store_quad(rt, ctx, 0x01u, to + 0x20u);
            v::load_quad(rt, ctx, 0x00u, from + 0x30u);
            from += 0x40u;
            v::store_quad(rt, ctx, 0x00u, to + 0x30u);
            to += 0x40u;  // the bgtz's delay slot
        } while (static_cast<std::int32_t>(left) > 0);
    }
    ctx.gpr[2] = from;
    ctx.gpr[6] = left;
    std::uint32_t relocations = half_u(rt, blob + 0xCu);
    const std::uint32_t table = word(rt, blob + 4u);
    ctx.gpr[3] = table;
    if (relocations != 0u) {
        std::uint32_t at = table, index = 0u;
        do {
            ctx.gpr[2] = byte_u(rt, at);
            index += 1u;
            ctx.gpr[8] = 0x10000000u;
            ctx.gpr[6] = 0x01000000u;
            if (ctx.gpr[2] == 0u) {
                const std::uint32_t target = word(rt, at + 4u) + copy;
                const std::uint32_t place = (word(rt, at) >> 8) + copy;
                ctx.gpr[6] = 0x01000000u | (target & 0x00FFFFFFu);
                ctx.gpr[8] = 0x10000000u | ((target >> 24) & 0xFFu) << 16;
                ctx.gpr[3] = place;
                put(rt, place + 4u, ctx.gpr[6]);
                put(rt, place, ctx.gpr[8]);
                relocations = half_u(rt, blob + 0xCu);
            }
            ctx.gpr[2] = static_cast<std::int32_t>(index) < static_cast<std::int32_t>(relocations & 0xFFFFu) ? 1u : 0u;
            at += 8u;  // the bnez's delay slot
        } while (ctx.gpr[2] != 0u);
        ctx.gpr[5] = at;
        ctx.gpr[7] = index;
    }
    ctx.gpr[4] = relocations;
    data = word(rt, entry + 4u);
    done();
}

// func_08B55218(f12..f19): saves VFPU M400 in a frame; C400 = (f12..f15 as C010) x (the vector at the state block's
// +0x170, prefix X, Y, X, Y) + (+0x180, same prefix); func_08B59118(state, 0xBFD) runs with (f16..f19) turned into
// 16.16 integers (vf2in 16, in the jal's delay slot); both results are packed to halfwords (vi2s), four of their words
// go to a1 / a2 / a3 / t0 (mfv), M400 is restored and it tail-calls func_08B552EC.
void func_08B55218(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    namespace v = gt::vfpu;
    using B = v::Binary;
    {
        GuestFrame frame(ctx, 0x50u);
        ctx.gpr[10] = std::bit_cast<std::uint32_t>(ctx.fpr[12]);
        ctx.gpr[11] = std::bit_cast<std::uint32_t>(ctx.fpr[13]);
        ctx.gpr[12] = std::bit_cast<std::uint32_t>(ctx.fpr[14]);
        ctx.gpr[13] = std::bit_cast<std::uint32_t>(ctx.fpr[15]);
        v::move_to(ctx, ctx.gpr[10], 0x01u);
        v::move_to(ctx, ctx.gpr[11], 0x21u);
        v::move_to(ctx, ctx.gpr[12], 0x41u);
        v::move_to(ctx, ctx.gpr[13], 0x61u);
        ctx.gpr[6] = std::bit_cast<std::uint32_t>(ctx.fpr[16]);
        v::store_quad(rt, ctx, 0x13u, frame.at(0x40u));   // sv.q C430
        ctx.gpr[7] = std::bit_cast<std::uint32_t>(ctx.fpr[17]);
        ctx.gpr[8] = std::bit_cast<std::uint32_t>(ctx.fpr[18]);
        v::store_quad(rt, ctx, 0x12u, frame.at(0x30u));   // sv.q C420
        ctx.gpr[9] = std::bit_cast<std::uint32_t>(ctx.fpr[19]);
        v::store_quad(rt, ctx, 0x11u, frame.at(0x20u));   // sv.q C410
        v::store_quad(rt, ctx, 0x10u, frame.at(0x10u));   // sv.q C400
        v::load_quad(rt, ctx, 0x10u, kGuState + 0x170u);  // lv.q C400
        v::load_quad(rt, ctx, 0x00u, kGuState + 0x180u);  // lv.q C000
        v::prefix(ctx, 1u, 0x44u);                         // vpfxt X, Y, X, Y
        v::binary(ctx, B::Mul, 0x10u, 0x01u, 0x10u, 4u);   // vmul.q C400, C010, C400
        v::move_to(ctx, ctx.gpr[6], 0x01u);
        v::move_to(ctx, ctx.gpr[7], 0x21u);
        v::move_to(ctx, ctx.gpr[8], 0x41u);
        v::move_to(ctx, ctx.gpr[9], 0x61u);
        v::prefix(ctx, 1u, 0x44u);                         // vpfxt X, Y, X, Y
        v::binary(ctx, B::Add, 0x10u, 0x10u, 0x00u, 4u);   // vadd.q C400, C400, C000
        v::float_to_int(ctx, 16u, 0x11u, 0x01u, 16u, 4u);  // vf2in.q C410, C010, 16 (the jal's delay slot)
        (void)call_set(rt, ctx, 0x08B59118u, {kGuState, 0xBFDu});
        ctx.gpr[4] = kGuState;
        v::pack_ints(ctx, 3u, 0x11u, 0x11u, 4u);           // vi2s.q C410, C410
        v::float_to_int(ctx, 16u, 0x10u, 0x10u, 16u, 4u);  // vf2in.q C400, C400, 16
        v::unary(ctx, 0u, 0x13u, 0x11u, 4u);               // vmov.q C430, C410
        ctx.gpr[7] = ctx.vfpu_scalar_bits(0x11u);          // mfv a3, S410
        ctx.gpr[8] = ctx.vfpu_scalar_bits(0x33u);          // mfv t0, S431
        v::pack_ints(ctx, 3u, 0x10u, 0x10u, 4u);           // vi2s.q C400, C400
        v::load_quad(rt, ctx, 0x13u, frame.at(0x40u));
        v::load_quad(rt, ctx, 0x11u, frame.at(0x20u));
        v::unary(ctx, 0u, 0x12u, 0x10u, 4u);               // vmov.q C420, C400
        ctx.gpr[5] = ctx.vfpu_scalar_bits(0x10u);          // mfv a1, S400
        v::load_quad(rt, ctx, 0x10u, frame.at(0x10u));
        ctx.gpr[6] = ctx.vfpu_scalar_bits(0x32u);          // mfv a2, S421
        v::load_quad(rt, ctx, 0x12u, frame.at(0x30u));
    }
    tail_call(ctx, 0x08B552ECu, {});
}

}  // namespace
}  // namespace hot_batch_106
}  // namespace gt::game

#define HOT106(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_106::name, gt::ReturnKind::kind)

// HOT106REGS
HOT106(0x08B565F8u, func_08B565F8, Word);
HOT106(0x08B53898u, func_08B53898, Word);
HOT106(0x08B54D28u, func_08B54D28, Word);
HOT106(0x08B55218u, func_08B55218, Word);
