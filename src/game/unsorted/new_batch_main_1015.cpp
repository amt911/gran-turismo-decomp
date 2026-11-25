// New batch (main-session batch 1015): functions drafted from tools.disasm that a previous draft left mismatching or skipped.
// Each body is structured C written from the asm: delay slots before the call/branch, forward and backward branches
// rendered directly, v0 as the original leaves it on every exit path.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1015 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call that leaves the callee's v0 / v1 / f0 in the context like the original's jal does.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
         std::initializer_list<float> floats = {}) {
    return call_full(rt, ctx, target, arguments, floats).v0;
}

s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
float f32(Runtime &rt, u32 a) { return float_at(rt, a); }
void f32_put(Runtime &rt, u32 a, float v) { put_float(rt, a, v); }

} // namespace

// 0x088F381C (a0 = object, a1 = transform record): renders one model node: builds its local matrix from the
// record's position and rotation, culls it against four planes, then emits the two vertex-batch groups.
void func_088F381C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x160u);
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    const u32 fp = frame.at(0x68u);
    const u32 s1 = frame.at(0x8u);
    const u32 s4 = word(rt, self + 4u) + (word(rt, arg + 0xCu) << 6);
    put(rt, frame.at(0x11Cu), call(rt, ctx, 0x088F3084u, {self, arg}));
    call_full(rt, ctx, 0x08B580F0u, {frame.at(0u), frame.at(4u)}, {f32(rt, s4 + 0x14u)});
    const float f20 = f32(rt, s4 + 8u) + f32(rt, s4 + 8u);

    const float argx = f32(rt, arg + 0u);
    const float argy = f32(rt, arg + 4u);
    const float argz = f32(rt, arg + 8u);
    const float scale = f32(rt, s4 + 8u);
    const float t1 = psp_mul(scale, f32(rt, frame.at(0u)));
    const float t2 = psp_mul(scale, f32(rt, frame.at(4u)));
    f32_put(rt, frame.at(0u), t1);
    f32_put(rt, frame.at(0x94u), argz);
    f32_put(rt, frame.at(0x70u), argz);
    f32_put(rt, frame.at(4u), t2);
    const float sum = t1 + t2;
    const float diff = t2 - t1;
    const float f9 = sum + argx;
    const float f6 = sum + argy;
    const float f7 = argx - sum;
    const float f10 = argy - diff;
    const float e1 = diff + argx;
    const float e8 = diff + argy;
    const float e4 = argx - diff;
    const float e3 = argy - sum;
    f32_put(rt, frame.at(0x68u), e1);
    f32_put(rt, frame.at(0x80u), e4);
    f32_put(rt, frame.at(0x6Cu), f6);
    f32_put(rt, frame.at(0x74u), f7);
    f32_put(rt, frame.at(0x78u), e8);
    f32_put(rt, frame.at(0x84u), e3);
    f32_put(rt, frame.at(0x8Cu), f9);
    f32_put(rt, frame.at(0x90u), f10);
    f32_put(rt, frame.at(0x7Cu), argz);
    call(rt, ctx, 0x08B569A0u);
    f32_put(rt, frame.at(0x88u), argz);
    call(rt, ctx, 0x08B568DCu, {word(rt, self + 0x1B0u)});
    call(rt, ctx, 0x08B5A7F0u, {s1, s4 + 0x30u});
    call(rt, ctx, 0x08B56940u);

    put(rt, self + 0x1ACu, s1);
    put(rt, self + 0x1A4u, 0x08C8A220u + ((rt.memory().load8(s4 + 2u) >> 6) << 5));
    put(rt, self + 0x1A8u, fp);
    const u32 s2 = rt.memory().load8(s4 + 2u) >> 6;
    u32 acc = 0u;
    if (!(f32(rt, frame.at(0x14u)) < 0.0f)) {
        u32 bit = 1u;
        u32 idx = 0u;
        do {
            const u32 off = idx << 2;
            const u32 at = fp + off;
            const float m1 = psp_mul(f32(rt, at), f32(rt, frame.at(8u)));
            idx += 3u;
            const float m2 = psp_mul(f32(rt, at + 4u), f32(rt, frame.at(0xCu)));
            const float m3 = psp_mul(f32(rt, at + 8u), f32(rt, frame.at(0x10u)));
            const u32 slot = frame.at(0x98u) + off;
            const float total = ((m1 + m2) + m3) + f32(rt, frame.at(0x14u));
            if (total < 0.0f) {
                call(rt, ctx, 0x088F32A4u, {self, slot, at});
                acc |= bit;
            }
            const bool more = idx != 0xCu;
            bit <<= 1;
            if (!more) break;
        } while (true);
    }
    if (acc == 0u || acc == 0xFu) {
        call_full(rt, ctx, 0x088F3254u, {self, fp, word(rt, frame.at(0x11Cu)), s2});
        return;
    }
    {
        {
            const u32 bits = acc << 4;
            put(rt, frame.at(0x118u), bits);
            put(rt, frame.at(0x114u), 0xFFFFFFFFu);
            put(rt, frame.at(0x110u), 0xFFFFFFFFu);
            u32 vec = frame.at(0x98u);
            u32 ptr = frame.at(0xA4u);
            u32 mask = 0x33u;
            u32 lane = 0x10u;
            u32 i = 0u;
            do {
                const u32 hit = bits & mask;
                if (hit != 0u && hit != (mask & 0xF0u)) {
                    if (hit == lane) {
                        const float v = call_full(rt, ctx, 0x088F3304u, {self, frame.at(0x2Cu), i}).f0;
                        call_full(rt, ctx, 0x088F3400u, {self, frame.at(0x2Cu), vec},
                                  {psp_mul(f20, v)});
                        put(rt, frame.at(0x110u), i);
                    } else {
                        const float v = call_full(rt, ctx, 0x088F3304u, {self, frame.at(0x18u), i}).f0;
                        call_full(rt, ctx, 0x088F3400u, {self, frame.at(0x18u), ptr},
                                  {psp_mul(f20, 1.0f - v)});
                        put(rt, frame.at(0x114u), i);
                    }
                }
                i += 1u;
                mask <<= 1;
                lane <<= 1;
                vec += 0xCu;
                ptr += 0xCu;
            } while (i != 4u);
        }
    }
    const u32 batch_a = call(rt, ctx, 0x088F3484u, {self, word(rt, frame.at(0x114u)), word(rt, frame.at(0x110u)),
                                                     frame.at(0xC8u), frame.at(0x40u), frame.at(0x18u),
                                                     frame.at(0x2Cu), frame.at(0x98u)});
    const u32 kept = call(rt, ctx, 0x088F3564u, {self, batch_a, frame.at(0xC8u), frame.at(0x40u)});
    call_full(rt, ctx, 0x088F2F90u, {self, frame.at(0xC8u), frame.at(0x40u), word(rt, frame.at(0x11Cu)),
                                     word(rt, s4 + 4u), kept});
    const u32 batch_b = call(rt, ctx, 0x088F3484u, {self, word(rt, frame.at(0x110u)), word(rt, frame.at(0x114u)),
                                                     frame.at(0xC8u), frame.at(0x40u), frame.at(0x2Cu),
                                                     frame.at(0x18u), fp});
    const u32 kept_b = call(rt, ctx, 0x088F3564u, {self, batch_b, frame.at(0xC8u), frame.at(0x40u)});
    call_full(rt, ctx, 0x088F2F90u, {self, frame.at(0xC8u), frame.at(0x40u), word(rt, frame.at(0x11Cu)),
                                     word(rt, frame.at(0x11Cu)), kept_b});
}

// 0x08B137E0 (a0 = object, a1 = an id, a2 = a byte): looks the id up in the object's 8-byte entry table at +0x2D4
// (count at +0x4D4); a hit bubbles the entry towards the front by its ordering bytes and returns the object the
// table's arena hands back for its first word, a miss allocates a slot block, inserts the entry in order and
// dispatches the fill by the mode word at +0x208.

// 0x08862438 (a0 = object, a1 = signed slot index, a2 = record): builds the two temporary sub-objects in its frame,
// fills slot a1 of the object's 0x50-byte table (base +0x4120) from the record, copies the record's source slot
// when asked, and queues the draws through the object at +0x5AC; tears the temporaries down either way.

// 0x0888C750 (a0 = object, a1 = out list, a2 = id half array, a3 = count, t0 = a flag byte, t1 = a flag byte):
// with the object's +4 handle live, walks the ids: a first pass resets each found entry, then every id is mapped
// through +4's lookup, ranked, and emitted into the object at +0x18's list.

// 0x0889A5BC (a0 = object, a1 = source): stores the source on the object, asks it for two vectors, builds the
// object's matrix offset at +0x460, then fills and combines the 0x20-byte elements of the array at +0x8E0.

// 0x0888619C (a0 = object, a1 = source, a2 = argument, a3 = argument): the object's per-frame update: clears the
// two state bits, fills the 0x50-byte slot the sub-object maps to from the source, refreshes the two cached
// vectors and emits the three draw ranges the mode word selects.

} // namespace new_batch_main_1015
} // namespace gt::game

#define NEW1015(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1015::name, gt::ReturnKind::kind)
NEW1015(0x088F381Cu, func_088F381C, Word);
