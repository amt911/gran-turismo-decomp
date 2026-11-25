// New batch main-58: 80 emitted functions of 58..82 instructions, written as structured C from `tools.disasm` (libstdc++ stream inserters,
// destructors of containers, virtual-call wrappers). Verified later by synthetic cases; no names (none has evidence). Registered with
// `NEW58(address, name, Word)`; every function keeps `v0` as the asm leaves it.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_58 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A `jalr`: the callee starts with v0 = its own address; v0 / v1 afterwards are the callee's.
u32 vcall(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_indirect(rt, ctx, target, arguments);
}
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }

// The callee-saved registers s0-s7 and fp: the original saves them in its frame and puts them back.
struct Saved {
    AllegrexContext &ctx;
    u32 value[9];
    explicit Saved(AllegrexContext &c) : ctx(c) {
        for (int i = 0; i < 8; ++i) value[i] = c.gpr[16 + i];
        value[8] = c.gpr[30];
    }
    ~Saved() {
        for (int i = 0; i < 8; ++i) ctx.gpr[16 + i] = value[i];
        ctx.gpr[30] = value[8];
    }
};

// ostream-style inserter: a sentry is built at sp (0x08C55FDC); when it is true the stream's buffer object (+0x84 of the virtual base)
// gets a virtual call `slot` with (buffer, sp+0x18, sp+0x10, base, sp+4, value); a non-zero error word in sp+4 is or-ed into the state
// (0x08C54CFC); a null buffer calls 0x08B5A9FC. v0 = the stream.
void stream_insert(Runtime &rt, AllegrexContext &ctx, u32 slot) {
    const u32 stream = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 sentry = frame.at(0x0u);
    ctx.gpr[16] = stream;
    ctx.gpr[17] = value;
    call(rt, ctx, 0x08C55FDCu, {sentry, stream, 0u});
    if (rt.memory().load8(sentry) == 0u) {
        ctx.gpr[2] = stream;
        return;
    }
    put(rt, frame.at(0x4u), 0u);
    const u32 base = stream + word(rt, word(rt, stream) - 0xCu);
    const u32 buffer = word(rt, base + 0x84u);
    if (buffer == 0u) {
        call(rt, ctx, 0x08B5A9FCu, {0u});
        return;
    }
    put(rt, frame.at(0x18u), word(rt, base + 0x78u));
    put(rt, frame.at(0x14u), 0xFFFFFFFFu);
    put(rt, frame.at(0x1Cu), 0xFFFFFFFFu);
    put(rt, frame.at(0x10u), 0u);
    const u32 target = word(rt, word(rt, buffer) + slot);
    vcall(rt, ctx, target, {buffer, frame.at(0x18u), frame.at(0x10u), base, frame.at(0x4u), value});
    put(rt, frame.at(0x8u), ctx.gpr[2]);
    put(rt, frame.at(0xCu), ctx.gpr[3]);
    put(rt, frame.at(0x20u), ctx.gpr[2]);
    put(rt, frame.at(0x24u), ctx.gpr[3]);
    const u32 error = word(rt, frame.at(0x4u));
    ctx.gpr[3] = error;
    if (error != 0u) {
        const u32 state_base = stream + word(rt, word(rt, stream) - 0xCu);
        call(rt, ctx, 0x08C54CFCu, {state_base, word(rt, state_base + 0x14u) | error});
    }
    ctx.gpr[2] = stream;
}

// 0x0880D478 (a0 = object, a1 = argument): builds an object (0x08B62344, size 0x210) and installs it at +0xE8 of a0 (releasing the previous one
// held at +0xEC through its virtual destructor), creates the shared singleton at 0x08D75100 on first use (guard 0x08D750F8), then
// tail-calls 0x0880B5E8(word +0xE8, a1).

// 0x0882F97C (a0 = out, a1, a2 = a selector, a3 = pointer to an object): with a2 == 1 the virtual +0x3C of the object *a3 gives s4 (else -1);
// builds a temporary at sp (0x088297A8), chains 0x088294D0 -> 0x0885EDF8 -> 0x088D7794(.., s4) -> 0x08A97A6C(sp+4, ..), and assigns the
// result into a0 (0x08A5BAD8) unless a0 is sp+4; the two temporaries are destroyed (0x08A5B9EC).

// 0x088EB868 (a0 = object, a1 = id): asks the helper at +0x538 (virtual +0x90) whether it equals a1 and stores that byte at +0x522; when the
// helper's virtual +0xA0 answers non-zero it is given the two halves at +0x558 / +0x55C and the word at +0xA8 (virtuals +0x84, +0x84, +0x58);
// the object's virtual +0x50(a1) result r is looked up: s2 = word +0x38 of r when r != 0; tail call 0x08B09FA0(0x08B0979C(), s2).

// 0x089ECD64 (a0 = this): destructor of a container-like object: stores the vtable 0x08CCFA30, runs 0x089EC798(this), releases the vector at +0x14
// (0x08C0A850 over [+0x14 .. +0x1C), then 0x08A5BC54 with the byte size when it is non-null), the member at +0x10, and two reference-counted
// strings at +8 and +4 (header at pointer - 0xC; count word at -4 via 0x08B60804, freed through 0x08B8542C when it is not above zero).
void func_089ECD64(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 members = self + 0x14u;
    put(rt, self, 0x08CCFA30u);
    call(rt, ctx, 0x089EC798u, {self});
    call(rt, ctx, 0x08C0A850u, {word(rt, self + 0x14u), word(rt, members + 4u), frame.at(0x2u)});
    const u32 begin = word(rt, self + 0x14u);
    const u32 end = word(rt, members + 8u);
    const u32 byte_size = static_cast<u32>(static_cast<s32>(end - begin) >> 2) << 2;
    ctx.gpr[2] = begin;
    if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, byte_size, 4u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x10u});
    const u32 empty = 0x08D74FD0u;
    u32 header = word(rt, self + 8u) - 0xCu;
    ctx.gpr[2] = 0x08D70000u;
    if (header != empty) {
        const u32 count = call(rt, ctx, 0x08B60804u, {header + 8u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x1u)});
    }
    ctx.gpr[2] = word(rt, self + 4u);
    header = ctx.gpr[2] - 0xCu;
    if (header != empty) {
        const u32 count = call(rt, ctx, 0x08B60804u, {header + 8u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
    }
}

// 0x08B33E50 (a0, a1, a2): retry loop around 0x08B33B30 (64-bit result in v0/v1) and 0x08B33D3C; 0x80020130 when the retries disagree.
void func_08B33E50(Runtime &rt, AllegrexContext &ctx) {
    const u32 a0 = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    GuestResult first = call_guest(rt, ctx, 0x08B33B30u, {a0, a1, 0u, 0u, 1u}, {});
    const u32 low = first.v0;
    const u32 high = first.v1;
    u32 result = low;
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
    if (static_cast<s32>(high) < 0) return;
    for (;;) {
        const u32 before = call(rt, ctx, 0x08B2DDE8u);
        const u32 step = call(rt, ctx, 0x08B33D3Cu, {a0, a1, a2});
        result = step;
        const u32 after = call(rt, ctx, 0x08B2DDE8u);
        if (static_cast<s32>(result) < 0) {
            ctx.gpr[2] = result;
            return;
        }
        if (before == after) {
            if (call(rt, ctx, 0x08B2DE00u, {a0}) == 0u) {
                ctx.gpr[2] = result;
                return;
            }
        }
        GuestResult again = call_guest(rt, ctx, 0x08B33B30u, {a0, a1, low, high, 0u}, {});
        ctx.gpr[2] = again.v0;
        ctx.gpr[3] = again.v1;
        if (static_cast<s32>(again.v1) < 0) {
            result = again.v0;
            ctx.gpr[2] = result;
            return;
        }
        if (again.v0 != low || again.v1 != high) {
            result = 0x80020130u;
            ctx.gpr[2] = result;
            return;
        }
    }
}

// 0x08C2A304 (a0 = pointer-to-member holder, a1 = out, a2 = pointer to an object, t0): temporary at sp+4 (0x08A87380(sp+4, t0)); a first
// pointer-to-member call (words +0x14 / +0x18 of a0: bit 0 of +0x18 selects a virtual slot through the vtable) on *a2 gives r; a second one
// (+0x1C / +0x20) on r takes sp+4; the result is assigned (0x08B8C564) into the temporary at sp, copied to a1 unless equal; both are destroyed.

// 0x08C2A444 (a0 = target, a1 = argument, a2 = pair, a3 = pair): allocates a 0x24-byte node (0x08A5BC20), constructs it (0x08A80214) with the
// vtable 0x08CD4300 and the fields +0x14 = a2[0], +0x18 = a2[1], +0x1C = a3[0], +0x20 = a3[1], wraps it in a holder at sp (0x08A5D414 with
// sp+4) and hands it on (0x08A7C5C8(a0, sp+4)).
void func_08C2A444(Runtime &rt, AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 pair_a = ctx.gpr[6];
    const u32 pair_b = ctx.gpr[7];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 holder = frame.at(0x4u);
    const u32 a_second = word(rt, pair_a + 4u);
    const u32 a_first = word(rt, pair_a);
    const u32 b_first = word(rt, pair_b);
    const u32 b_second = word(rt, pair_b + 4u);
    const u32 node = call(rt, ctx, 0x08A5BC20u, {0x24u, 4u});
    call(rt, ctx, 0x08A80214u, {node, argument});
    put(rt, node, 0x08CD4300u);
    put(rt, node + 0x18u, a_second);
    put(rt, node + 0x14u, a_first);
    put(rt, node + 0x20u, b_second);
    put(rt, node + 0x1Cu, b_first);
    put(rt, frame.at(0x0u), node);
    call(rt, ctx, 0x08A5D414u, {holder, frame.at(0x0u), a_first});
    call(rt, ctx, 0x08A7C5C8u, {target, holder});
    call(rt, ctx, 0x08A5B9ECu, {holder});
}

// 0x08867150 (a0 = object, a1 = index, a2 = float[3], a3 = out float[2], t0 = out float or null): transforms (x, y, z, 1) by the two 4x4
// matrices at +0x4390 and +0x4290 of the object (64 bytes each, `index` << 6) through 0x08346774; stores z at *t0 when t0 is set; z < 0 gives
// out = (0, 0) and v0 = 0, otherwise out = (x / w * 0.5 + 0.5, 0.5 - y / w * 0.5) and v0 = 1.

// 0x08875138 (a0 = pointer to an object): releases the five reference-counted holders at +0x834 .. +0x854 of *a0 (weak count at +4, use count at +8;
// the virtual +8 runs when the first count reaches 0, the virtual +0xC when the second does), then 0x08077EB0(+0x814), 0x082CC574(+0x3E0),
// 0x082CC700(+0x10) and a tail call of 0x08360654(*a0). Null *a0: returns with v0 untouched.

// 0x0890F90C (a0 = object, a1 = argument, a2 = holder with a vtable, f12 = scale): the holder's virtuals +0x6C and +0x7C and the object's bytes
// +0 / +1 pick a pair (f1, f2): (10, 0) | (0.5, 1) | (1, 0); it is stored at +0x18 + {0xC, 4, 8}; then 0x082B47B4(a1, f12 = x * x * y with
// x = +0x18, y = +4), 0x082B4BDC(a0 + 8, scale) and a tail call of 0x082B4BDC(a0 + 0x18, scale).
void func_0890F90C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    const float scale = ctx.fpr[12];
    const u32 block = object + 0x18u;
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        float f1;
        float f2;
        const GuestResult first = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x6Cu), {holder}, {scale});
        bool far_pair = false;
        if (first.v0 == 0u) {
            far_pair = true;
        } else {
            const GuestResult second = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x7Cu), {holder}, {});
            if (second.v0 != 0u) {
                far_pair = true;
            } else if (rt.memory().load8(object) != 0u) {
                far_pair = true;
            }
        }
        if (far_pair) {
            f1 = 10.0f;
            f2 = 0.0f;
        } else if (rt.memory().load8(object + 1u) != 0u) {
            f1 = 0.5f;
            f2 = 1.0f;
        } else {
            f1 = 1.0f;
            f2 = 0.0f;
        }
        put_float(rt, block + 0xCu, f1);
        put_float(rt, block + 0x4u, f2);
        put_float(rt, block + 0x8u, f1);
        const float x = float_at(rt, object + 0x18u);
        const float y = float_at(rt, object + 0x4u);
        const float product = psp_mul(x, psp_mul(x, y));
        const GuestResult r = call_guest(rt, ctx, 0x08AB87B4u, {argument}, {product});
        ctx.gpr[2] = r.v0;
        const GuestResult r2 = call_guest(rt, ctx, 0x08AB8BDCu, {object + 8u}, {scale});
        ctx.gpr[2] = r2.v0;
    }
    ctx.fpr[12] = scale;
    tail_call(ctx, 0x08AB8BDCu, {block});
}

// 0x0892A698 (a0 = vector of 16-byte elements {begin, end}, a1 = {count, array of words}): builds a temporary at sp+4 (0x0812668C); when
// count < size the range [begin + count * 16, end) goes to 0x083DBA4C, else 0x083DBB84(a0, end, count - size, tmp) appends; then 0x083D97D8 finishes
// (the temporary's words +4, +8 and the frame), 0x08331254 runs when the word at sp+8 is set, and each of the `count` words of the array is
// copied into element i (0x0812658C(begin + i * 16, array[i])). v0 = 0.

// 0x089E68F8 (a0 = this): destructor: vtables 0x08CC93B0 at +0 and 0x08CC9608 at +0x38; the reference-counted string at +0x1E0 (header at
// pointer - 0xC, count 0x08B60804 at pointer - 4, 0x08B8542C when it is not above zero); members +0x190 / +0x18C / +0x188 / +0x184
// (0x08A5B9EC); two vectors at +0x168 and +0x174 (0x083FE614 over [begin, end), 0x08A5BC54 with the byte size when allocated); member
// +0x15C; then 0x08196FCC(this).
void func_089E68F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 text = word(rt, self + 0x1E0u);
    put(rt, self, 0x08CC93B0u);
    u32 header = text - 0xCu;
    put(rt, self + 0x38u, 0x08CC9608u);
    if (header != 0x08D74FD0u) {
        const u32 count = call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x1u)});
    }
    call(rt, ctx, 0x08A5B9ECu, {self + 0x190u});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x18Cu});
    call(rt, ctx, 0x08A5B9ECu, {self + 0x188u});
    const u32 stop = self + 0x168u;
    call(rt, ctx, 0x08A5B9ECu, {self + 0x184u});
    ctx.gpr[18] = stop;
    u32 cursor = self + 0x180u;
    if (stop != 0u) {
        while (cursor != stop) {
            cursor -= 0xCu;
            call(rt, ctx, 0x08C02614u, {word(rt, cursor), word(rt, cursor + 4u), frame.at(0x0u)});
            const u32 begin = word(rt, cursor);
            const u32 bytes = static_cast<u32>(static_cast<s32>(word(rt, cursor + 8u) - begin) >> 2) << 2;
            ctx.gpr[2] = begin;
            if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
        }
    }
    call(rt, ctx, 0x08A5B9ECu, {self + 0x15Cu});
    call(rt, ctx, 0x0899AFCCu, {self});
}

// 0x08B484CC (a0 = this, a1 = key): virtual +0x14 of this; walks the list headed at *(this + 0x30) (nodes {item, ?, next at +8}): every node whose item's
// first word equals the key (all nodes when key == -1) gets the virtual +0xAC(this, item), the item's word +0x40 and the item itself are released
// through 0x08328E7C(word +0x28, ..), the node is unlinked (0x0831BC48(this + 0x2C, node)) and released; with a key the walk ends at the first match.
// Ends with a tail call of the virtual +0x18.

// 0x08BEC948 (a0 = pointer-to-member holder, a1 = out, a2 = pointer to an object, t0): s3 = 0x0829EA30(t0); a first pointer-to-member call
// (words +0x14 / +0x18 of a0, bit 0 of +0x18 selects the virtual form) on *a2, then a second (+0x1C / +0x20) on its result with a1 = s3, whose
// result points at two floats; they go to sp+4 / sp+8 and 0x083E50D8(sp, sp+4) builds the answer, assigned into a1 (0x08A5BAD8) unless equal.

// 0x08BFAD84 (a0 = object, a1 = value): builds three temporaries (sp+8 and sp+0xC with 0x08272AA0, sp+4 from the value at sp with 0x08272AAC),
// asks the object at +0x1C (virtual +0xCC with sp+0xC, 1, sp+8) and returns the virtual +0x30 of the object the call left at sp+0xC.

// 0x08C219C4 (a0 = vector {begin, end, capacity end}, a1 = count): reserve: more than 0x3FFFFFFF elements calls 0x08356DDC(0x08CB02C0) (a0 is then that
// string, as in the original); when the capacity (+8 - +0) in words is below the count, allocates count words (0x08257C20(count * 4, 4)), moves the
// elements (0x083816F4), destroys the old range (0x083816B0), frees the old block (0x08257C54) and stores {new, new + size, new + count * 4}.
void func_08C219C4(Runtime &rt, AllegrexContext &ctx) {
    u32 vector = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 owner = vector;
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[17] = owner;
    if (0x3FFFFFFFu < count) {
        call(rt, ctx, 0x08B5ADDCu, {0x08CB02C0u});
        vector = 0x08CB02C0u;
    }
    const u32 begin = word(rt, vector);
    const u32 capacity = static_cast<u32>(static_cast<s32>(word(rt, vector + 8u) - begin) >> 2);
    ctx.gpr[18] = begin;
    if (!(capacity < count)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 end = word(rt, vector + 4u);
    const u32 bytes = count << 2;
    const u32 size_words = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
    const u32 fresh = call(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    ctx.gpr[19] = fresh;
    call(rt, ctx, 0x08B856F4u, {begin, end, fresh, frame.at(0x0u)});
    call(rt, ctx, 0x08B856B0u, {word(rt, owner), word(rt, owner + 4u), frame.at(0x1u)});
    const u32 old = word(rt, owner);
    const u32 old_words = static_cast<u32>(static_cast<s32>(word(rt, owner + 8u) - old) >> 2);
    ctx.gpr[2] = old_words;
    if (old != 0u) call(rt, ctx, 0x08A5BC54u, {old, old_words << 2, 4u});
    put(rt, owner + 8u, fresh + bytes);
    put(rt, owner + 4u, fresh + (size_words << 2));
    put(rt, owner, fresh);
    ctx.gpr[2] = fresh + (size_words << 2);
}

// 0x0885D110 (a0 = pointer to a holder {array of 0x188-byte entries, count at +4}, the object at +4 of a0): for every entry whose byte +0x16 is set and
// whose word +0x1C is not -1: 0x0805AC80(*(a0 + 4), word, 0x0805954C(entry)), then for i = 0, 1: 0x0805956C(entry, i) and, when it answers non-zero,
// 0x0805AD58(*(a0 + 4), word, i, answer).

// 0x08995794 (a0 = object): builds a string temporary (0x08381EB8 with 0x08CA4294) and passes it to the virtual +0x108; releases the temporary (the
// reference-counted header at pointer - 0xC), stores the result of 0x08190488 at +0x38 after freeing the old one (0x08257A08 / 0x08257930), and
// registers two callbacks through 0x083838E8 (0x08CA42A4 -> 0x089955D4 and 0x08CA423C -> 0x089954A0, each as getter and setter).
void func_08995794(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 temp = frame.at(0x4u);
    ctx.gpr[18] = self;
    const u32 method = word(rt, word(rt, self) + 0x108u);
    ctx.gpr[17] = method;
    call(rt, ctx, 0x08B85EB8u, {temp, 0x08CA4294u, frame.at(0x1u)});
    vcall(rt, ctx, method, {self, temp});
    const u32 text = word(rt, frame.at(0x4u));
    const u32 header = text - 0xCu;
    if (header != 0x08D74FD0u) {
        const u32 count = call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
    }
    const u32 fresh = call(rt, ctx, 0x08994488u);
    ctx.gpr[16] = fresh;
    if (fresh != 0u) call(rt, ctx, 0x08A5BA08u, {fresh});
    const u32 old = word(rt, self + 0x38u);
    ctx.gpr[2] = old;
    if (old != 0u) call(rt, ctx, 0x08A5B930u, {old});
    put(rt, self + 0x38u, fresh);
    call(rt, ctx, 0x08B878E8u, {self, 0x08CA42A4u, 0x089955D4u, 0x089955D4u});
    call(rt, ctx, 0x08B878E8u, {self, 0x08CA423Cu, 0x089954A0u, 0x089954A0u});
}

// 0x08A32F9C (a0 = object): pops the top of the stack kept at +0x408 (end pointer at +0x40C): when 0x0822E33C(a0) answers non-zero, the element
// before the end (0x0823CC24 -> sp+8; its object +0x44 receives a string built by 0x08381EB8 from the answer, 0x083815E0) is released; the object's bytes
// +0 and +8 and word +4 are cleared and the end pointer moves back one word (the removed word is destroyed with 0x08A5B9EC).
void func_08A32F9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 stack = self + 0x408u;
    const u32 temp_8 = frame.at(0x8u);
    const u32 temp_4 = frame.at(0x4u);
    const u32 answer = call(rt, ctx, 0x08A3233Cu, {self});
    ctx.gpr[16] = answer;
    if (answer != 0u) {
        call(rt, ctx, 0x08A40C24u, {temp_8, word(rt, stack + 4u) - 4u});
        const u32 holder = word(rt, temp_8);
        ctx.gpr[19] = holder;
        if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACC60u});
        call(rt, ctx, 0x08B85EB8u, {temp_4, answer, frame.at(0x1u)});
        call(rt, ctx, 0x08B855E0u, {holder + 0x44u, temp_4});
        const u32 text = word(rt, temp_4);
        const u32 header = text - 0xCu;
        ctx.gpr[16] = header;
        if (header != 0x08D74FD0u) {
            const u32 count = call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
            if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
        }
        call(rt, ctx, 0x08A5B9ECu, {temp_8});
    }
    rt.memory().store8(self, 0u);
    rt.memory().store8(self + 8u, 0u);
    put(rt, self + 4u, 0u);
    const u32 last = word(rt, stack + 4u) - 4u;
    put(rt, stack + 4u, last);
    call(rt, ctx, 0x08A5B9ECu, {last});
}

// 0x08BCFEF8 (a0 = this, a1 = value): takes a reference on the counted object at +0x2C (atomic +1 of its word +4), stores a1 at +0x18, clears byte +0xE
// and word +0x1C, calls the pointer-to-member (+0x30 / +0x34, bit 0 = virtual) on the object at +0x28, clears +0x18, and puts the reference back
// (atomic -1 of +4 -> virtual +8, atomic -1 of +8 -> virtual +0xC). Returns the word +0x1C the call left (v0).

// 0x08BDD458 (a0 = this): destructor of a class with arrays of 0x2C-byte members: stores the vtable 0x08CC5FC0, destroys three members through their virtual
// (the first array ends at +0x2E8, the second at +0x264), stores 0x08CC6130 at +0x10 and eight more offsets, and tail-calls 0x08121F98(this).
void func_08BDD458(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        const u32 end_b = self + 0x264u;
        ctx.gpr[16] = self;
        ctx.gpr[20] = end_b;
        put(rt, self, 0x08CC5FC0u);
        if (end_b != 0u) {
            u32 cursor = self + 0x2E8u;
            if (end_b != cursor) {
                u32 slot = self + 0x2BCu;
                for (u32 left = 3u; left != 0u; --left) {
                    const u32 table = word(rt, slot);
                    cursor -= 0x2Cu;
                    slot -= 0x2Cu;
                    vcall(rt, ctx, word(rt, table), {cursor});
                }
            }
        }
        u32 first_end = self + 0x1E0u;
        ctx.gpr[2] = first_end;
        if (first_end != 0u && first_end != end_b) {
            u32 cursor = end_b;
            u32 slot = self + 0x238u;
            for (u32 left = 3u; left != 0u; --left) {
                const u32 table = word(rt, slot);
                cursor -= 0x2Cu;
                slot -= 0x2Cu;
                vcall(rt, ctx, word(rt, table), {cursor});
            }
        }
        const u32 vtable = 0x08CC6130u;
        ctx.gpr[2] = vtable;
        put(rt, self + 0x10u, vtable);
        put(rt, self + 0x1A0u, vtable);
        put(rt, self + 0x174u, vtable);
        put(rt, self + 0x144u, vtable);
        put(rt, self + 0x118u, vtable);
        put(rt, self + 0xECu, vtable);
        put(rt, self + 0xC0u, vtable);
        put(rt, self + 0x94u, vtable);
        put(rt, self + 0x68u, vtable);
        put(rt, self + 0x3Cu, vtable);
    }
    tail_call(ctx, 0x08925F98u, {self});
}

// 0x08C48EC4 (a0 = list object, a1 = range {w0, w1, w2, w3}, a2 = range/holder): copies a1's four words to sp[0..0xC]; when word +0xC of a2 is zero,
// 0x08444CDC(a0, sp, a2, a3) answers; else when a2's word +8 equals w2 it returns; otherwise 0x08442DAC runs for the node at +0xC of a0 (when set, with
// the 16-bit index (w2 - *(*(a0 + 4))) / 16 * 0xEEEEEEEF) and then, as a tail call, for the one at +8 with the same index form.

// 0x08ACF390 (a0 = object): 0x082F0F24(+0xFC, word +0xF8), s4 = 0x08476A88(), 0x08476A50(0, 0x10), swaps the word at 0x08D00414 with 0x08355A24(word +0x18,
// old, 1), 0x083171E4 / 0x08317198 over +0xB8 (the 64-bit answer goes to +0xF0 / +0xF4), flips the flag word +0xF8 (0x082F0F90), asks the virtual +8 and
// calls 0x08354E84(0) when it answers; 0x0832A314(6, 0) -> 0x0832A0A4, 0x082B578C(word +0xF0 * 1e-6f) and a tail call of 0x08476A50(0, s4).
void func_08ACF390(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 token;
    {
        Saved saved(ctx);
        GuestFrame frame(ctx, 0x20u);
        const u32 flags = self + 0xFCu;
        const u32 timer = self + 0xB8u;
        const u32 slot = 0x08D00414u;
        ctx.gpr[19] = self;
        ctx.gpr[18] = flags;
        ctx.gpr[16] = timer;
        call(rt, ctx, 0x08AF4F24u, {flags, word(rt, self + 0xF8u)});
        token = call(rt, ctx, 0x08C7AA88u);
        ctx.gpr[20] = token;
        call(rt, ctx, 0x08C7AA50u, {0u, 0x10u});
        const u32 old = word(rt, slot);
        const u32 swapped = call(rt, ctx, 0x08B59A24u, {word(rt, self + 0x18u), old, 1u});
        put(rt, slot, swapped);
        call(rt, ctx, 0x08B1B1E4u, {timer});
        const GuestResult pair = call_guest(rt, ctx, 0x08B1B198u, {timer}, {});
        ctx.gpr[2] = pair.v0;
        ctx.gpr[3] = pair.v1;
        const u32 flag = word(rt, self + 0xF8u) ^ 1u;
        put(rt, self + 0xF0u, pair.v0);
        put(rt, self + 0xF4u, pair.v1);
        put(rt, self + 0xF8u, flag);
        call(rt, ctx, 0x08AF4F90u, {flags, flag});
        if (vcall(rt, ctx, word(rt, word(rt, self) + 8u), {self}) != 0u) call(rt, ctx, 0x08B58E84u, {0u});
        call(rt, ctx, 0x08B2E314u, {6u, 0u});
        call(rt, ctx, 0x08B2E0A4u, {ctx.gpr[2]});
        const float scale = static_cast<float>(static_cast<s32>(word(rt, self + 0xF0u)));
        const float micro = std::bit_cast<float>(0x358637BDu);
        const GuestResult r = call_guest(rt, ctx, 0x08AB978Cu, {}, {psp_mul(scale, micro)});
        ctx.gpr[2] = r.v0;
    }
    tail_call(ctx, 0x08C7AA50u, {0u, token});
}

// 0x08B01F0C (a0..a3, t0..t3, three stack words at +0x50..): s0 = a2 == 2 ? 2 : a2 ^ 1. When a3 < t3 the call 0x082FDD1C gets the arguments as they came (stack
// words copied to sp[0..0xC]); otherwise a value built by 0x08442D64(word +0x5C) / 0x083AA364 (stored at sp+0x14 / sp+0x10) goes in sp[0xC] and the arguments
// are re-ordered (a2 = s0, a3 = t3, t0 = word +0x50, t3 = a3, stack = t0, t1, t2). v0 = the call's answer.

// 0x08B952A8 (a0 = string object, a1 = chars, a2 = n): append of n chars (the rep header sits at -0xC: length, -8 capacity, -4 reference count): n == 0 returns
// at once; a length above 0x3FFFFFFC - len calls 0x08356DDC(0x08C9F710); when the capacity is too small or the string is shared, 0x083911FC(a0, len + n) makes
// room (an argument pointing into the old buffer is rebased); one char is stored directly, more go through 0x08375A88; then the new length and a 0 reference
// count are stored and the terminator byte 0x08CBF760 closes the string. v0 = a0.
void func_08B952A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 chars = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = chars;
    ctx.gpr[17] = count;
    ctx.gpr[19] = self;
    if (count == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    u32 rep = word(rt, self);
    const u32 length = word(rt, rep - 0xCu);
    u32 capacity;
    if (0x3FFFFFFCu - length < count) {
        capacity = call(rt, ctx, 0x08B5ADDCu, {0x08C9F710u});
    } else {
        capacity = word(rt, rep - 8u);
    }
    const u32 total = count + length;
    ctx.gpr[18] = total;
    bool in_place = false;
    if (!(capacity < total)) {
        const s32 shared = static_cast<s32>(word(rt, rep - 4u));
        if (shared <= 0) in_place = true;
    }
    if (!in_place) {
        if (chars < rep) {
            call(rt, ctx, 0x08B951FCu, {self, total});
            rep = word(rt, self);
        } else if (!(rep + length < chars)) {
            const u32 offset = chars - rep;
            chars = offset;
            call(rt, ctx, 0x08B951FCu, {self, total});
            rep = word(rt, self);
            chars = rep + offset;
        } else {
            call(rt, ctx, 0x08B951FCu, {self, total});
            rep = word(rt, self);
        }
    }
    const u32 end = rep + word(rt, rep - 0xCu);
    if (count == 1u) {
        rt.memory().store8(end, rt.memory().load8(chars));
    } else {
        call(rt, ctx, 0x08B79A88u, {end, chars, count});
    }
    rep = word(rt, self);
    put(rt, rep - 4u, 0u);
    put(rt, rep - 0xCu, total);
    rt.memory().store8(rep + total, rt.memory().load8(0x08CBF760u));
    ctx.gpr[2] = self;
}

// 0x08BEB8D8, 0x08BF6CB4, 0x08BFC2B4, 0x08C038E4, 0x08C05EC0, 0x08C06220 (identical bodies; a0 = pointer-to-member holder, a1 = pointer to an object, a2 = argument): a first pointer-to-member call (words +0x14 / +0x18, bit 0 = virtual) on
// *a1; a second (+0x24 / +0x28) on its result with a string temporary at sp+4 built by 0x083E5070(sp+4, a2); the temporary's reference-counted header is released.
void member_pair_with_string(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    const u32 pointer = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 first_adjust = word(rt, holder + 0x18u);
    const u32 object = word(rt, pointer);
    const u32 first_function = word(rt, holder + 0x14u);
    const u32 this_a = object + (static_cast<s32>(first_adjust) >> 1);
    u32 first_target = first_function;
    if ((first_adjust & 1u) != 0u) first_target = word(rt, word(rt, this_a) + first_function);
    const u32 r = vcall(rt, ctx, first_target, {this_a, object, first_function, first_adjust});
    const u32 second_adjust = word(rt, holder + 0x28u);
    const u32 receiver = r + (static_cast<s32>(second_adjust) >> 1);
    u32 second_target;
    if ((second_adjust & 1u) != 0u) {
        second_target = word(rt, word(rt, receiver) + word(rt, holder + 0x24u));
    } else {
        second_target = word(rt, holder + 0x24u);
    }
    const u32 temp = frame.at(0x4u);
    call(rt, ctx, 0x08BE9070u, {temp, argument});
    vcall(rt, ctx, second_target, {receiver, temp});
    const u32 text = word(rt, temp);
    const u32 header = text - 0xCu;
    if (header != 0x08D74FD0u) {
        const u32 count = call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
    }
}

// 0x08C01094 (a0 = tree {.., header at +4, count at +0x14}, a1 = force-left flag, a2 = parent node, a3 = key/value pair): allocates a 0x1C-byte node (0x08257C20),
// constructs its string at +0x10 (0x08384F44) and the value at +0x18 (0x081B6C2C, word +0x14 = word +4 of the pair), links it (0x0835B2B8(left, node, parent,
// header)) with left = flag != 0 || parent == header || compare(key, parent key) < 0 (0x08332170 over the shorter length, else the length difference),
// and counts it. v0 = the node.

// 0x0889DEE4 (a0 = object with vtable, a1 = float[3] point, a2 = float[3] margins): virtual +0x14(a0, sp+0x24) and +0x10(a0, sp) give two vectors; the first is
// negated and transformed (0x08346A68(a1, a1, sp)), 0x08347AA8(sp+0x24, a1), then the virtual +0x2C(a0, sp+0xC, sp+0x18) gives centre and half extent:
// v0 = 0 when a component of the point lies below centre - margin or above margin + extent, else 1.

// 0x08978D3C (a0 = object, a1 = key): fills a list at sp (head = sp) through 0x083304E0(a0 + 0x20) / 0x083E9F34(a0 + 0x18, key, sp) / 0x0833057C, then for each
// node of it, unless the bit range at +0x74 / +0x7C is empty, tests the bit (word(+0x7C + ..) index word(+0x7C + 4) - 1, signed, truncating) in the bit
// array and, when it is set, runs the virtual +0x108 of the object at node + 8 (0x08A5BB7C(0x08CA2F7C) when that is null); the list is freed (0x083E9D64).

// 0x088D30B0 (a0 = object): gets two helpers through 0x080CEB34 / 0x080AD330 (the second chained through virtual +8 twice with 0), and from the object found
// (byte +0x1A * 5 when 0x080D2358(*a0 + 4) == 3, else byte +0x24) and 0x080D0FA8 / 0x080D0E34 returns a value: the answer of 0x080D0E34(a0 + 4, -(0x080D0FA8)) when
// non-zero, else the byte value, with 0x50 when that is 0.
void func_088D30B0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 inner = word(rt, self) + 4u;
    ctx.gpr[18] = inner;
    ctx.gpr[16] = self;
    call(rt, ctx, 0x088D2B34u, {self});
    const u32 holder = word(rt, word(rt, self + 4u));
    u32 found = word(rt, holder + 8u);
    ctx.gpr[17] = found;
    u32 current = call(rt, ctx, 0x088B1330u, {self});
    if (current != 0u) {
        current = call(rt, ctx, 0x088B1330u, {self});
        const u32 first = vcall(rt, ctx, word(rt, word(rt, current) + 8u), {current, 0u});
        if (first != 0u) {
            current = call(rt, ctx, 0x088B1330u, {self});
            found = vcall(rt, ctx, word(rt, word(rt, current) + 8u), {current, 0u});
            ctx.gpr[17] = found;
        }
    }
    u32 value = 0u;
    ctx.gpr[16] = 0u;
    if (found != 0u) {
        if (call(rt, ctx, 0x088D6358u, {inner}) == 3u) {
            value = rt.memory().load8(found + 0x1Au) * 5u;
        } else {
            value = rt.memory().load8(found + 0x24u);
        }
        ctx.gpr[16] = value;
    }
    u32 argument = 0u;
    if (static_cast<s32>(call(rt, ctx, 0x088D4FA8u, {inner})) >= 0) {
        const u32 again = call(rt, ctx, 0x088D4FA8u, {inner});
        argument = 0u - again;
    }
    const u32 answer = call(rt, ctx, 0x088D4E34u, {inner, argument});
    if (answer == 0u) {
        ctx.gpr[2] = 0x50u;
        if (value == 0u) value = 0x50u;
    } else {
        value = answer;
    }
    ctx.gpr[16] = value;
    ctx.gpr[2] = value;
}

// 0x088DFA68 (a0 = object, a1 = key, a2 = flag byte): 0x080D81F0(a0, a1) must answer non-zero (else v0 = 0); stores -1 at *(+0x130), 0x080E04BC(+0x640, key)
// must too; then 0x080E0924(+0x640) -> 0x080D1108(+4, ..), 0x080DB790(a0); with an object at +0x148 the virtual +0x10 result goes to 0x080E03D0(+0x640, ., n)
// (n = word +0x7C of the key when positive, else -1) and its virtual +0xC(1) runs; byte +0x5B5 = 0, 0x080D8784(a0, 1), v0 = 0x080D832C(a0, flag, 0).
void func_088DFA68(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    u32 key = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = flag;
    ctx.gpr[17] = key;
    ctx.gpr[16] = self;
    if (call(rt, ctx, 0x088DC1F0u, {self, key}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    rt.memory().store8(word(rt, self + 0x130u), 0xFFu);
    if (call(rt, ctx, 0x088E44BCu, {word(rt, self + 0x640u), key}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 made = call(rt, ctx, 0x088E4924u, {word(rt, self + 0x640u)});
    call(rt, ctx, 0x088D5108u, {self + 4u, made});
    call(rt, ctx, 0x088DF790u, {self});
    const u32 holder = word(rt, self + 0x148u);
    if (holder != 0u) {
        if (key == 0u) {
            key = 0xFFFFFFFFu;
        } else {
            key = word(rt, key + 0x7Cu);
            if (!(static_cast<s32>(key) > 0)) key = 0xFFFFFFFFu;
        }
        ctx.gpr[17] = key;
        const u32 produced = vcall(rt, ctx, word(rt, word(rt, holder) + 0x10u), {holder});
        call(rt, ctx, 0x088E43D0u, {word(rt, self + 0x640u), produced, key});
        const u32 target = word(rt, self + 0x148u);
        vcall(rt, ctx, word(rt, word(rt, target) + 0xCu), {target, 1u});
    }
    rt.memory().store8(self + 0x5B5u, 0u);
    call(rt, ctx, 0x088DC784u, {self, 1u});
    call(rt, ctx, 0x088DC32Cu, {self, flag, 0u});
}

// 0x0897AAB4, 0x089AEF20 (a2 = selector, a3 = pair of object pointers): a2 != 2 returns 2; otherwise a holder is made at sp+0xC (0x081C5C34; the
// error call 0x08A5BB7C(holder_error) when it is null), each object of the pair (null: 0x08A5BB7C(object_error)) writes a string temporary through
// its virtual +0xC (sp+8 for the first, sp+4 for the second), 0x081C4C70(holder, sp+8, sp+4) combines them and both temporaries are released
// (reference-counted headers, 0x0835C804 / 0x0838142C), then the holder is (0x08A5B9EC). The two files differ only in the two error strings.
void object_pair_to_strings(Runtime &rt, AllegrexContext &ctx, u32 holder_error, u32 object_error) {
    const u32 selector = ctx.gpr[6];
    const u32 pair = ctx.gpr[7];
    Saved saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[2] = 2u;
    ctx.gpr[17] = pair;
    if (selector != 2u) return;
    const u32 holder_slot = frame.at(0xCu);
    const u32 first_text = frame.at(0x8u);
    const u32 second_text = frame.at(0x4u);
    call(rt, ctx, 0x089C9C34u, {holder_slot});
    const u32 holder = word(rt, holder_slot);
    ctx.gpr[18] = holder;
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {holder_error});
    u32 object = word(rt, pair);
    ctx.gpr[16] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {object_error});
    vcall(rt, ctx, word(rt, word(rt, object) + 0xCu), {first_text, object});
    object = word(rt, pair + 4u);
    ctx.gpr[17] = object;
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {object_error});
    vcall(rt, ctx, word(rt, word(rt, object) + 0xCu), {second_text, object});
    call(rt, ctx, 0x089C8C70u, {holder, first_text, second_text});
    const u32 empty = 0x08D74FD0u;
    u32 text = word(rt, second_text);
    u32 header = text - 0xCu;
    bool released = false;
    if (header != empty) {
        const u32 count = call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x1u)});
        released = true;
    }
    (void)released;
    text = word(rt, first_text);
    header = text - 0xCu;
    ctx.gpr[16] = header;
    if (empty != header) {
        const u32 count = call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call(rt, ctx, 0x08B8542Cu, {header, frame.at(0x0u)});
    }
    call(rt, ctx, 0x08A5B9ECu, {holder_slot});
}
void func_0897AAB4(Runtime &rt, AllegrexContext &ctx) { object_pair_to_strings(rt, ctx, 0x08CA3268u, 0x08CA2F84u); }
void func_089AEF20(Runtime &rt, AllegrexContext &ctx) { object_pair_to_strings(rt, ctx, 0x08CA518Cu, 0x08CA50BCu); }

// 0x08C4636C (a0 = out float[9], a1 = float[9]): inverse of a 3x3 matrix (m0..m8 row-major): the determinant m0(m4 m8 - m5 m7) ... computed in the original's
// operation order; a determinant of exactly 0 leaves the output untouched, else each cofactor is multiplied by 1 / det.
void func_08C4636C(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 in = ctx.gpr[5];
    const float m6 = float_at(rt, in + 0x18u);
    const float m3 = float_at(rt, in + 0xCu);
    const float m4 = float_at(rt, in + 0x10u);
    const float neg_m6 = -m6;
    const float m7 = float_at(rt, in + 0x1Cu);
    const float f18 = psp_mul(m3, m7);
    float f21 = psp_mul(m4, neg_m6);
    const float m2 = float_at(rt, in + 0x8u);
    const float m1 = float_at(rt, in + 0x4u);
    const float m5 = float_at(rt, in + 0x14u);
    float f1 = psp_mul(m2, f21);
    float f17 = psp_mul(m1, m6);
    float f0 = psp_mul(m2, f18);
    const float m0 = float_at(rt, in + 0x0u);
    float f11 = psp_mul(m1, m3);
    float f9 = psp_mul(m5, f17);
    const float f15 = psp_mul(m0, m7);
    f1 = f1 + f0;
    const float m8 = float_at(rt, in + 0x20u);
    float f16 = psp_mul(m0, m4);
    float f6 = psp_mul(m5, f15);
    f1 = f1 + f9;
    f11 = psp_mul(m8, f11);
    f9 = psp_mul(m2, m7);
    const float f14 = psp_mul(m4, m2);
    f1 = f1 - f6;
    const float f19 = psp_mul(m0, m5);
    f6 = psp_mul(m1, m8);
    float f13 = psp_mul(m2, neg_m6);
    const float f7 = psp_mul(m0, m8);
    float f2 = psp_mul(m2, m3);
    float f10 = psp_mul(m4, m8);
    f1 = f1 - f11;
    const float f20 = psp_mul(m3, m1);
    float f4 = psp_mul(m3, m8);
    f0 = psp_mul(m8, f16);
    float f8 = psp_mul(m7, m5);
    float f12 = psp_mul(m5, m6);
    float f3 = psp_mul(m1, m5);
    f1 = f1 + f0;
    f10 = f10 - f8;
    f12 = f12 - f4;
    f21 = f21 + f18;
    f9 = f9 - f6;
    f13 = f13 + f7;
    f17 = f17 - f15;
    f8 = f3 - f14;
    const float f11b = f2 - f19;
    f16 = f16 - f20;
    if (f1 == 0.0f) return;
    const float inverse = 1.0f / f1;
    put_float(rt, out + 0x20u, psp_mul(inverse, f16));
    put_float(rt, out + 0x0u, psp_mul(inverse, f10));
    put_float(rt, out + 0xCu, psp_mul(inverse, f12));
    put_float(rt, out + 0x18u, psp_mul(inverse, f21));
    put_float(rt, out + 0x4u, psp_mul(inverse, f9));
    put_float(rt, out + 0x10u, psp_mul(inverse, f13));
    put_float(rt, out + 0x1Cu, psp_mul(inverse, f17));
    put_float(rt, out + 0x8u, psp_mul(inverse, f8));
    put_float(rt, out + 0x14u, psp_mul(inverse, f11b));
}

} // namespace
} // namespace new_batch_main_58
} // namespace gt::game

#define NEW58(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_58::name, gt::ReturnKind::kind)
NEW58(0x089ECD64u, func_089ECD64, Word);
NEW58(0x08B33E50u, func_08B33E50, Word);
NEW58(0x08C2A444u, func_08C2A444, Word);
NEW58(0x0890F90Cu, func_0890F90C, Word);
NEW58(0x089E68F8u, func_089E68F8, Word);
NEW58(0x08C219C4u, func_08C219C4, Word);
NEW58(0x08995794u, func_08995794, Word);
NEW58(0x08A32F9Cu, func_08A32F9C, Word);
NEW58(0x08BDD458u, func_08BDD458, Word);
NEW58(0x08ACF390u, func_08ACF390, Word);
NEW58(0x08B952A8u, func_08B952A8, Word);
NEW58(0x088D30B0u, func_088D30B0, Word);
NEW58(0x088DFA68u, func_088DFA68, Word);
NEW58(0x0897AAB4u, func_0897AAB4, Word);
NEW58(0x089AEF20u, func_089AEF20, Word);
NEW58(0x08C4636Cu, func_08C4636C, Word);
