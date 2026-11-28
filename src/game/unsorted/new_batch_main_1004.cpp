// New batch (main-session batch 1004): functions drafted before that mismatched the differential gate (or were dropped
// as too hard), rewritten as structured C from `tools.disasm` against what the gate saw. Draft only, unverified here.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1004 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call: v0 / v1 / f0 come back as the callee left them, and a1 too, because the original's next call reads it
// when the asm does not set a1 itself.
GuestResult call_all(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                     std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.gpr[5] = result.a1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_all(rt, ctx, target, arguments).v0;
}
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats) {
    return call_all(rt, ctx, target, arguments, floats).f0;
}
// `lw v0, slot(vtable); jalr v0`: the callee starts with v0 = its own address.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_all(rt, ctx, target, arguments).v0;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// The callee-saved registers: the original keeps its state in them and restores the caller's before every return.
struct SavedRegs {
    AllegrexContext &ctx;
    u32 saved[8];
    u32 saved_fp;
    explicit SavedRegs(AllegrexContext &c) : ctx(c), saved_fp(c.gpr[30]) {
        for (unsigned i = 0u; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedRegs() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
        ctx.gpr[30] = saved_fp;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;
};

// 0x08C53264 (a0 = a 3x3 matrix, three rows of three floats, a1 = the three-float answer, a2 = the three-float
// right-hand side): copies the matrix TRANSPOSED into the frame (a0's [r][c] lands at frame +0xC + 4*(3c + r)) and the
// side vector at +0..+8, then for each row i compares |row i's entries beyond the diagonal| against |row i's diagonal|,
// swapping whole columns (and side entries) while one is larger. A zero largest magnitude answers 0. Otherwise row
// j (> i) takes the reduction 08B4D650 with -row_i[j]/row_i[i] and the side entry follows it, and the answer is solved
// back from row 2 up, each entry divided by the diagonal. v0 = 1 when it solved.

// 0x08A511E0 (a0 = the object, a1 = the handle the file helpers take): fills the object from a 0xA8-byte descriptor
// the loaders fill in the frame, then allocates the two strips and the two line tables and starts the worker thread
// 0x08A5176C on it. v0 is the last call's answer on every exit.

// 0x08A7A0D8 (a0 = map holder whose tree header sits at +0x10, a1 = key pointer, a2 = value holder pointer): the map's
// insert. A missing tree takes the value only with a non-empty holder word; a missing key likewise. An existing key
// with a null holder word erases the node (08B5F444 hands back what replaces it), a non-null holder word whose +0x28
// check answers gets the value through its +0x64 virtual, otherwise the node's holder takes the value word. Then the
// tree is dropped when its size word is zero. v0 is the size word when it survived, else the last call's answer.

// 0x088C58F0 (a0 = object, a1/a2/a3 = three float outputs, t0/t1/t2 = three word outputs, the two words at the
// caller's stack top, $f12-$f15 = four floats): asks the object's virtual at +8 whether the point is inside, then
// resolves the four corner offsets through 0x088C5834 and passes the two chosen distances through 0x08B4A430
// (blend(value, 0.5 or 1.0)). A corner's own answer picks the pair; the corners beyond +8 use the y offset where the
// first two use x (the original's own ordering). v0 = 1 when the point was hit and the blend ran, 0 otherwise.

// 0x08C78E68 (a0 = a stream buffer object, a1 = the container the lookup index comes from): the buffer's underflow.
// It looks the index up twice in the container (the first only gates, the second takes the element into s3), then
// either grows through the element's +0x14 virtual, answers through it, sets the read area up through the +0x18
// virtuals, or resets the area when no element is left. The element lands at +0x90.
// v0 is that of the last call on every exit, except the +0x90 setup, which answers -1.
void func_08C78E68(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    SavedRegs saved(ctx);
    u32 element = 0u;
    const auto lookup = [&](bool &found) -> u32 {
        const u32 index = call(rt, ctx, 0x08B5C2B0u, {0x08D721C8u});
        const u32 table = word(rt, source);
        found = false;
        if (index >= word(rt, table + 0x08u)) return 0u;
        const u32 value = word(rt, word(rt, table + 0x04u) + index * 4u);
        found = value != 0u;
        return value;
    };
    // 08C78FD8: no stream object left — the area becomes the empty one the stream report gives.
    const auto reset_area = [&] {
        if (call(rt, ctx, 0x08C76B7Cu, {self}) == 0u) {
            put(rt, self + 0x90u, 0u);
            return;
        }
        const u32 stream = word(rt, self + 0x78u);
        put(rt, self + 0x14u, 0u);
        put(rt, self + 0x0Cu, stream);
        put(rt, self + 0x04u, stream);
        put(rt, self + 0x08u, stream);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0x18u, 0u);
        put(rt, self + 0x90u, element);
    };
    // 08C78FC8: the report throws; a zero answer keeps the element, otherwise the area is reset.
    const auto throw_path = [&] {
        if (call(rt, ctx, 0x08B5A9FCu, {}) == 0u) {
            put(rt, self + 0x90u, element);
            return;
        }
        reset_area();
    };
    // 08C79098: asks the object at +0x90 for the bytes it holds, compacts what is left, and sets the read area up.
    const auto fill_area = [&] {
        const u32 base = word(rt, self + 0x94u);
        const u32 object = word(rt, self + 0x90u);
        const u32 read = call_virtual(rt, ctx, word(rt, word(rt, object) + 0x1Cu),
                                      {object, self + 0x60u, base, word(rt, self + 0x9Cu),
                                       word(rt, self + 0x08u) - word(rt, self + 0x04u)});
        const u32 position = base + read;
        put(rt, self + 0x9Cu, position);
        u32 count = word(rt, self + 0xA0u) - position;
        if (count != 0u) {
            call_all(rt, ctx, 0x08B79BACu, {base, position, count});
        } else {
            count = 0u;
        }
        const u32 start = word(rt, self + 0x94u);
        const u32 stream = word(rt, self + 0x78u);
        const u32 w0 = word(rt, self + 0x30u);
        const u32 w1 = word(rt, self + 0x34u);
        const u32 w2 = word(rt, self + 0x38u);
        const u32 w3 = word(rt, self + 0x3Cu);
        const u32 w4 = word(rt, self + 0x40u);
        const u32 w5 = word(rt, self + 0x44u);
        put(rt, self + 0xA0u, start + count);
        put(rt, self + 0x0Cu, stream);
        put(rt, self + 0x60u, w0);
        put(rt, self + 0x64u, w1);
        put(rt, self + 0x68u, w2);
        put(rt, self + 0x6Cu, w3);
        put(rt, self + 0x70u, w4);
        put(rt, self + 0x74u, w5);
        put(rt, self + 0x9Cu, start);
        put(rt, self + 0x04u, stream);
        put(rt, self + 0x08u, stream);
        put(rt, self + 0x14u, 0u);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0x18u, 0u);
        put(rt, self + 0x48u, w0);
        put(rt, self + 0x4Cu, w1);
        put(rt, self + 0x50u, w2);
        put(rt, self + 0x54u, w3);
        put(rt, self + 0x58u, w4);
        put(rt, self + 0x5Cu, w5);
    };
    // 08C79008: the object hands an eight-word descriptor to the virtual at +0x10 of self's own vtable; +0x90 keeps
    // the element unless the descriptor is the empty pair (two -1 words). Every exit answers -1, as the original's.
    const auto setup_area = [&] {
        const u32 own = word(rt, self);
        call_virtual(rt, ctx, word(rt, own + 0x10u),
                     {frame.at(0x20u), self, 0u, 0u, 1u, word(rt, self + 0x2Cu)});
        for (u32 i = 0u; i < 8u; ++i) put(rt, frame.at(i * 4u), word(rt, frame.at(0x20u + i * 4u)));
        const u32 first = word(rt, frame.at(0x20u));
        if (first != 0xFFFFFFFFu || word(rt, frame.at(0x24u)) != first) {
            put(rt, self + 0x90u, element);
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        put(rt, self + 0x90u, 0u);
        ctx.gpr[2] = 0xFFFFFFFFu;
    };
    // 08C78FD0: the element goes to +0x90; a set +0x82 byte resets the area instead.
    const auto no_stream = [&](u32 pending) {
        if (pending == 0u) {
            ctx.gpr[2] = 0u;
            put(rt, self + 0x90u, element);
            return;
        }
        reset_area();
    };

    bool first_found = false;
    lookup(first_found);
    if (!first_found) {
        element = 0u;
    } else {
        bool second_found = false;
        const u32 found = lookup(second_found);
        if (second_found) {
            element = found;
        } else {
            throw_path();
            return;
        }
    }
    if (call(rt, ctx, 0x08B61534u, {self + 0x24u}) == 0u) {
        put(rt, self + 0x90u, element);
        return;
    }
    if (byte_at(rt, self + 0x81u) == 0u && byte_at(rt, self + 0x82u) == 0u) {
        no_stream(0u);
        return;
    }
    const u32 grower = word(rt, self + 0x90u);
    if (grower == 0u) {
        throw_path();
        return;
    }
    if (call_virtual(rt, ctx, word(rt, word(rt, grower) + 0x14u), {grower}) == 0xFFFFFFFFu) {
        put(rt, self + 0x90u, 0u);
        return;
    }
    if (byte_at(rt, self + 0x81u) == 0u) {
        no_stream(byte_at(rt, self + 0x82u));
        return;
    }
    const u32 stream_object = word(rt, self + 0x90u);
    if (stream_object == 0u) {
        throw_path();
        return;
    }
    if (call_virtual(rt, ctx, word(rt, word(rt, stream_object) + 0x18u), {stream_object}) == 0u) {
        fill_area();
        return;
    }
    if (element == 0u) {
        put(rt, self + 0x90u, element);
        return;
    }
    if (call_virtual(rt, ctx, word(rt, word(rt, element) + 0x18u), {element}) == 0u) {
        setup_area();
        return;
    }
    put(rt, self + 0x90u, element);
}

// The intrusive holder the container at +0x44AC keeps: a retain counter at +4 and a shared counter at +8; the object's
// virtuals at +8 and +0xC run when each counter reaches zero. 08C7908..~: `ll`/`sc` count for a plain read-modify-write.
void assign_holder(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 value) {
    if (value == word(rt, slot)) return;
    if (value != 0u) put(rt, value + 4u, word(rt, value + 4u) + 1u);
    const u32 old = word(rt, slot);
    if (old != 0u) {
        const u32 retain = word(rt, old + 4u);
        put(rt, old + 4u, retain - 1u);
        if (retain == 1u) {
            call_virtual(rt, ctx, word(rt, word(rt, old) + 0x08u), {old});
            const u32 shared = word(rt, old + 8u);
            put(rt, old + 8u, shared - 1u);
            if (shared == 1u) call_virtual(rt, ctx, word(rt, word(rt, old) + 0x0Cu), {old});
        }
    }
    put(rt, slot, value);
}

void release_holder(Runtime &rt, AllegrexContext &ctx, u32 value) {
    if (value == 0u) return;
    const u32 retain = word(rt, value + 4u);
    put(rt, value + 4u, retain - 1u);
    if (retain != 1u) return;
    call_virtual(rt, ctx, word(rt, word(rt, value) + 0x08u), {value});
    const u32 shared = word(rt, value + 8u);
    put(rt, value + 8u, shared - 1u);
    if (shared == 1u) call_virtual(rt, ctx, word(rt, word(rt, value) + 0x0Cu), {value});
}

// 0x0886473C (a0 = the owner object): the destruction of the sub-container at +0x4490 / +0x44A0. Its three words are
// moved into a stack temporary and cleared, the temporary is rebuilt through 08B8FE44, the recorded words at +0x44AC /
// +0x44B4 and the two holders at +0x44B0 / +0x44B8 take what it carries (releasing what they held), then the temporary
// and its own two holders are released and the container's tail is torn down through 0x0896275C.

// 0x08A79DD4 (a0 = map holder whose tree sits at +0x10, a1 = the value holder, a2 = the key, a3 = a pointer to the
// object the key maps to): an existing key copies the node's object into the holder — through the object's virtual at
// +0x60 when its +0x28 check answers, by a plain holder assignment otherwise — and a missing key empties the holder;
// then, for an empty holder only, the value the object supplies at +0xCC / +0x60 is inserted under the lower bound
// (making the tree header first when the map has none) and handed to the new node's holder.

} // namespace
} // namespace new_batch_main_1004
} // namespace gt::game

#define NEW1004(address, name, kind) \
    GT_DECOMP(address, gt::game::new_batch_main_1004::name, gt::ReturnKind::kind)
NEW1004(0x08C78E68u, func_08C78E68, Word);
