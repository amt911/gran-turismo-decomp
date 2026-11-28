// New batch main 143 (main session): functions emitted by the recompiler that no src/ file defines or
// registers. Written by hand from `tools.disasm` as structured C; a draft batch, not yet verified by the
// differential gate. The functions are large float / container routines; each is commented with what it does.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <vector>

namespace gt::game {
namespace new_batch_main_143 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call with float arguments (f12...); returns the callee's f0.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                 std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
u32 byte_at(Runtime &rt, u32 a) { return rt.memory().load8(a); }
float bits(u32 value) { return std::bit_cast<float>(value); }
// The FPU's int-to-float of an unsigned byte read from memory.
// i * 280: the original builds it as (i * 40) * 8 - i * 40 with i * 40 = (i << 3) + (i << 5).
u32 stride_280(u32 i) {
    const u32 times_40 = (i << 3) + (i << 5);
    return (times_40 << 3) - times_40;
}
float byte_float(Runtime &rt, u32 a) { return static_cast<float>(static_cast<s32>(byte_at(rt, a))); }

// 0x0894F6C8 (a0 = object): reads the state object (+8) and the other object (+0xC), asks 0x0895B744 for two
// outputs (sp+0, sp+4), derives them from the state's floats (a steering-like pair clamped to 0..1, smoothed
// below), then integrates both through the 0x550/0x554 and 0x558/0x55C pairs and calls the finishers.
void func_0894F6C8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 sp = ctx.gpr[29];
    const u32 self = ctx.gpr[4];
    const u32 state = word(rt, self + 8u);
    const u32 other = word(rt, self + 0xCu);
    constexpr u32 kTable = 0x08CDE508u;
    put(rt, sp, 0u);
    put(rt, sp + 4u, 0u);
    const s32 mode = signed_byte(rt, state + 3u);
    const bool flag = mode == 2 || byte_at(rt, state + 0xEu) != 0u;
    do {
        if (float_at(rt, state + 0x6C4u) <= 0.0f) break;
        float sum = 0.0f;
        for (u32 i = 0; i < 4u; ++i) {
            const u32 entry = call(rt, ctx, 0x08955300u, {self, i});
            const u32 kind = byte_at(rt, entry + 0xE9u);
            const u32 slot = call(rt, ctx, 0x0894268Cu, {other, i, kind});
            sum = sum + float_at(rt, slot);
        }
        if (call(rt, ctx, 0x0895B744u, {self, sp, sp + 4u}) != 0u) break;
        bool clamp_path;
        if (!flag) clamp_path = byte_at(rt, kTable + 0x1ACu) != 0u || byte_at(rt, other + 0x270u) == 0xBu;
        else clamp_path = byte_at(rt, kTable + 0x1ADu) != 0u;
        if (clamp_path) {
            const float value = float_at(rt, state + 0x564u);
            if (1.0f < value) {
                const float rest = value - 1.0f;
                put_float(rt, sp + 4u, rest);
                if (1.0f < rest) put_float(rt, sp + 4u, 1.0f);
            } else {
                put_float(rt, sp, 1.0f - value);
            }
            break;
        }
        float f12 = float_at(rt, state + 0x520u);
        float f22 = float_at(rt, state + 0x6F8u);
        float f24 = float_at(rt, state + 0x6E0u);
        if (0.0f <= psp_mul(f12, f22)) {
            if (f12 < 0.0f) {
                f12 = -f12;
                f22 = -f22;
                f24 = -f24;
            }
            float f20 = float_at(rt, state + 0x6C4u);
            const float scale = call_float(rt, ctx, 0x08B49CC8u, {}, {f12});
            f20 = psp_mul(f20, scale);
            f20 = f20 / float_at(rt, other + 0x18u);
            if (0.0f < f20) {
                float f1 = float_at(rt, state + 0x6B8u);
                if (f1 < 0.0f) f1 = -f1;
                f1 = f1 / sum;
                if (bits(0x3E4CCCCDu) < f1) {
                    if (0.0f < psp_mul(f22, f24)) f22 = f22 + psp_mul(f24, 0.5f);
                    if (f22 < f20) put_float(rt, sp, 1.0f - f22 / f20);
                }
            }
            if (!flag) {
                if (call(rt, ctx, 0x0895541Cu, {self}) != 0u && byte_at(rt, kTable + 0x180u) != 0u) {
                    const float scaled = psp_mul(float_at(rt, sp), float_at(rt, 0x08CF2F8Cu));
                    put_float(rt, sp, scaled);
                    if (1.0f < scaled) put_float(rt, sp, 1.0f);
                }
            }
        }
        const float f0 = float_at(rt, state + 0x6C4u);
        float f12b = float_at(rt, state + 0x6C8u);
        f12b = psp_mul(f12b, f12b);
        const float radius = call_float(rt, ctx, 0x08B49D7Cu, {}, {psp_mul(f0, f0) + f12b});
        if (radius < bits(0x40DE38E4u)) break;
        if (flag) {
            float f1 = float_at(rt, state + 0x6F8u);
            float f2 = float_at(rt, state + 0x6B8u);
            f1 = psp_mul(radius, f1);
            const float product = psp_mul(f1, f2);
            if (0.0f <= product) break;
            if (f1 < 0.0f) f1 = -f1;
            if (f2 < 0.0f) f2 = -f2;
            const float diff = f1 - f2;
            f1 = 0.0f;
            if (!(diff < 0.0f)) f1 = diff + diff;
            put_float(rt, sp + 4u, f1 / sum);
            break;
        }
        for (u32 i = 2; i < 4u; ++i) {
            const u32 entry = call(rt, ctx, 0x08955300u, {self, i});
            float f0b = float_at(rt, entry + 0x24u);
            if (f0b < 0.0f) f0b = -f0b;
            if (float_at(rt, sp + 4u) < f0b) put_float(rt, sp + 4u, f0b);
        }
        float f7 = psp_mul(radius, bits(0x3C9374BCu));
        if (1.0f < f7) f7 = 1.0f;
        const float degrees = bits(0x3C8EFA35u);
        float f5 = float_at(rt, sp + 4u);
        float low = psp_mul(byte_float(rt, kTable + 0x129u), degrees);
        float high = psp_mul(byte_float(rt, kTable + 0x12Au), degrees);
        high = high - low;
        high = psp_mul(f7, high);
        const float f6 = low + high;
        if (!(f6 <= f5)) break;
        f5 = f5 - f6;
        const float tenth = bits(0x3DCCCCCDu);
        float low2 = psp_mul(byte_float(rt, kTable + 0x12Bu), tenth);
        float high2 = psp_mul(byte_float(rt, kTable + 0x12Cu), tenth);
        const float half_pi = bits(0x3FC90FDBu) - f6;
        high2 = high2 - low2;
        high2 = psp_mul(f7, high2);
        low2 = low2 + high2;
        low2 = psp_mul(low2, f5);
        low2 = low2 / half_pi;
        put_float(rt, sp + 4u, low2);
        const u32 busy = call(rt, ctx, 0x0895541Cu, {self});
        float f1 = float_at(rt, sp + 4u);
        if (busy != 0u && byte_at(rt, kTable + 0x181u) != 0u) {
            f1 = psp_mul(f1, float_at(rt, 0x08CF2F88u));
            put_float(rt, sp + 4u, f1);
        }
        if (1.0f < f1) put_float(rt, sp + 4u, 1.0f);
    } while (false);
    // The end section: smooth the outputs into the 0x550 / 0x558 pairs.
    const float step = call_float(rt, ctx, 0x0895549Cu, {self}, {});
    if (byte_at(rt, kTable + 0x182u) != 0u) {
        const float position = float_at(rt, state + 0x550u);
        float velocity = float_at(rt, state + 0x554u);
        float delta = float_at(rt, sp) - position;
        delta = psp_mul(delta, bits(0x43FA0000u));
        delta = delta + psp_mul(velocity, bits(0xC1A00000u));
        delta = psp_mul(step, delta);
        velocity = velocity + delta;
        put_float(rt, state + 0x554u, velocity);
        put_float(rt, sp, position + psp_mul(step, velocity));
    }
    if (byte_at(rt, kTable + 0x183u) != 0u) {
        const float position = float_at(rt, state + 0x558u);
        float velocity = float_at(rt, state + 0x55Cu);
        float delta = float_at(rt, sp + 4u) - position;
        delta = psp_mul(delta, bits(0x43FA0000u));
        delta = delta + psp_mul(velocity, bits(0xC1A00000u));
        delta = psp_mul(step, delta);
        velocity = velocity + delta;
        put_float(rt, state + 0x55Cu, velocity);
        put_float(rt, sp + 4u, position + psp_mul(step, velocity));
    }
    put(rt, state + 0x550u, word(rt, sp));
    rt.memory().store8(state + 0x731u, 0u);
    put(rt, state + 0x558u, word(rt, sp + 4u));
    rt.memory().store8(state + 0x730u, 0u);
    call(rt, ctx, 0x0894FCA8u, {self});
    const s32 tail_mode = signed_byte(rt, state + 3u);
    bool finish_b = false; // true: 0x0894F63C, false: 0x0894F5D0
    if (tail_mode == 0) {
        finish_b = byte_at(rt, state + 0xEu) != 0u && byte_at(rt, state + 0xDu) == 0u;
    } else if (tail_mode == 2) {
        finish_b = true;
    } else {
        ctx.gpr[2] = 2u;
        return;
    }
    if (finish_b) {
        call(rt, ctx, 0x0894F63Cu, {self});
        rt.memory().store8(state + 0x730u, 0u);
        rt.memory().store8(state + 0x731u, 0u);
    } else {
        call(rt, ctx, 0x0894F5D0u, {self});
    }
}


constexpr u32 kEmptyRep = 0x08D74FD0u; // the shared empty string representation (refcounted strings: rep = data - 0xC)

// The assertion calls the file's null checks make (they come back, the code goes on with the null).
void assert_a(Runtime &rt, AllegrexContext &ctx) { call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D54u}); }
void assert_b(Runtime &rt, AllegrexContext &ctx) { call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D60u}); }

// Releases a refcounted string whose data pointer is `data`: not the empty representation, and the 0x08B60804
// (atomic add) of -1 on the count at data-4 gives <= 0, then 0x08B8542C destroys the representation (a1 = `slot`, the
// stack address of the allocator argument). v0 is left as the last call left it; the empty case leaves it untouched.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 slot) {
    const u32 rep = data - 0xCu;
    if (rep == kEmptyRep) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {rep, slot});
}

// 0x08A8FA4C (a0 = object, a1 = key, a2 = code): for the codes whose bit is in 0x77 (and a flag byte in the object at +0x70)
// finds the entry for the key in the tree at +0x20 and, when it is not there or differs, updates it; then looks the key up
// in the tree at +0x50 and, by the found entry's kind (jump table 0x08C8F73C) or by the code, records a change entry.
void func_08A8FA4C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x90u);
    const u32 sp = ctx.gpr[29];
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 code = ctx.gpr[6];
    if (code == 7u) {
        call(rt, ctx, 0x08C2BE98u, {word(rt, self + 0x50u)});
        return;
    }
    const u32 holder = word(rt, self + 0x70u);
    if (byte_at(rt, holder + 8u) != 0u && code < 7u && ((1u << code) & 0x77u) != 0u) {
        u32 s0 = word(rt, self + 0x20u);
        if (s0 == 0u) assert_a(rt, ctx);
        const u32 found = call(rt, ctx, 0x08C2B010u, {s0 + 0x24u, key});
        s0 = word(rt, self + 0x20u);
        if (s0 == 0u) assert_a(rt, ctx);
        if (found != s0 + 0x28u) {
            const u32 iterator = sp + 0xCu;
            call(rt, ctx, 0x08AAE214u, {iterator, found + 0x14u});
            s0 = word(rt, sp + 0xCu);
            u32 s1 = s0;
            if (s0 == 0u) {
                assert_b(rt, ctx);
                s0 = word(rt, sp + 0xCu);
            }
            if (byte_at(rt, s1 + 0x20u) == 0u) {
                s1 = s0;
                if (s0 == 0u) {
                    assert_b(rt, ctx);
                    s0 = word(rt, sp + 0xCu);
                }
                if (code != word(rt, s1 + 0x14u)) {
                    u32 a1 = s0;
                    if (s0 == 0u) {
                        assert_b(rt, ctx);
                        a1 = s0;
                    }
                    call(rt, ctx, 0x08AAECB0u, {sp + 0x24u, a1});
                    s0 = word(rt, sp + 0xCu);
                    if (s0 == 0u) {
                        assert_b(rt, ctx);
                        s0 = word(rt, sp + 0xCu);
                        if (s0 == 0u) assert_b(rt, ctx);
                    }
                    call(rt, ctx, 0x08B88F44u, {sp + 0x10u, s0 + 0x18u});
                    s0 = word(rt, sp + 0xCu);
                    if (s0 == 0u) assert_b(rt, ctx);
                    release_string(rt, ctx, word(rt, sp + 0x10u), sp + 8u);
                    release_string(rt, ctx, word(rt, sp + 0x24u), sp + 7u);
                    s0 = word(rt, sp + 0xCu);
                }
                if (s0 == 0u) assert_b(rt, ctx);
                rt.memory().store8(s0 + 0x20u, 1u);
                s1 = word(rt, sp + 0xCu);
                if (s1 == 0u) assert_b(rt, ctx);
                s0 = word(rt, self + 0x78u);
                call(rt, ctx, 0x08B855E0u, {s1 + 0x18u, self + 0x74u});
                put(rt, s1 + 0x1Cu, s0);
            } else {
                u32 a1 = s0;
                if (s0 == 0u) {
                    assert_b(rt, ctx);
                    a1 = s0;
                }
                call(rt, ctx, 0x08AAECB0u, {sp + 0x10u, a1});
                s0 = word(rt, sp + 0xCu);
                if (s0 == 0u) {
                    assert_b(rt, ctx);
                    s0 = word(rt, sp + 0xCu);
                    if (s0 == 0u) assert_b(rt, ctx);
                }
                call(rt, ctx, 0x08B88F44u, {sp + 0x24u, s0 + 0x18u});
                if (word(rt, sp + 0xCu) == 0u) assert_b(rt, ctx);
                release_string(rt, ctx, word(rt, sp + 0x24u), sp + 6u);
                release_string(rt, ctx, word(rt, sp + 0x10u), sp + 5u);
            }
            call(rt, ctx, 0x08A5B9ECu, {iterator});
        }
    }
    const u32 node = call(rt, ctx, 0x08C2AFBCu, {word(rt, self + 0x50u), key});
    if (word(rt, self + 0x50u) + 4u == node) {
        // Not found: build a change record (code 3 from 0x08A8C424, the others from 0x08A8C384) and insert it.
        const bool is_three = code == 3u;
        const u32 made = call(rt, ctx, is_three ? 0x08A8C424u : 0x08A8C384u, {self});
        u32 owner;
        u32 first;
        u32 second;
        u32 wide;
        u32 temp;
        if (is_three) {
            const u32 tree = word(rt, self + 0x78u);
            put(rt, sp + 0x24u, made);
            put(rt, sp + 0x28u, code);
            call(rt, ctx, 0x08B88F44u, {sp + 0x2Cu, self + 0x74u});
            put(rt, sp + 0x30u, tree);
            owner = word(rt, self + 0x50u);
            call(rt, ctx, 0x08A6972Cu, {sp + 0x48u, key});
            first = sp + 0x4Cu;
            second = sp + 0x50u;
            wide = sp + 0x54u;
            temp = sp + 0x48u;
            put(rt, first, word(rt, sp + 0x24u));
            const u32 v1 = word(rt, sp + 0x28u);
            call(rt, ctx, 0x08B88F44u, {wide, sp + 0x2Cu});
            put(rt, second, v1);
            const u32 tail = word(rt, sp + 0x30u);
            put(rt, sp + 0x58u, tail);
            const GuestResult inserted = call_guest(rt, ctx, 0x08C2BC8Cu, {owner, temp}, {});
            ctx.gpr[2] = inserted.v0;
            ctx.gpr[3] = inserted.v1;
            put(rt, sp + 0x64u, inserted.v1);
            const u32 data = word(rt, wide);
            put(rt, sp + 0x1Cu, inserted.v0);
            rt.memory().store8(sp + 0x20u, static_cast<std::uint8_t>(byte_at(rt, sp + 0x64u)));
            put(rt, sp + 0x60u, inserted.v0);
            release_string(rt, ctx, data, sp + 4u);
            call(rt, ctx, 0x08A69D68u, {temp});
            release_string(rt, ctx, word(rt, sp + 0x2Cu), sp + 3u);
        } else {
            const u32 tree = word(rt, self + 0x78u);
            put(rt, sp + 0x24u, made);
            put(rt, sp + 0x28u, code);
            call(rt, ctx, 0x08B88F44u, {sp + 0x2Cu, self + 0x74u});
            const u32 table_owner = word(rt, word(rt, self + 0x58u) + 8u);
            put(rt, sp + 0x30u, tree);
            call(rt, ctx, 0x08A6972Cu, {sp + 0x34u, key});
            const u32 v0 = word(rt, sp + 0x24u);
            const u32 v1 = word(rt, sp + 0x28u);
            put(rt, sp + 0x38u, v0);
            call(rt, ctx, 0x08B88F44u, {sp + 0x40u, sp + 0x2Cu});
            put(rt, sp + 0x3Cu, v1);
            const u32 tail = word(rt, sp + 0x30u);
            put(rt, sp + 0x44u, tail);
            const GuestResult inserted = call_guest(rt, ctx, 0x08C2BC8Cu, {table_owner, sp + 0x34u}, {});
            ctx.gpr[2] = inserted.v0;
            ctx.gpr[3] = inserted.v1;
            put(rt, sp + 0x64u, inserted.v1);
            const u32 data = word(rt, sp + 0x40u);
            put(rt, sp + 0x14u, inserted.v0);
            rt.memory().store8(sp + 0x18u, static_cast<std::uint8_t>(byte_at(rt, sp + 0x64u)));
            put(rt, sp + 0x60u, inserted.v0);
            release_string(rt, ctx, data, sp + 2u);
            call(rt, ctx, 0x08A69D68u, {sp + 0x34u});
            release_string(rt, ctx, word(rt, sp + 0x2Cu), sp + 1u);
        }
        return;
    }
    const u32 kind = word(rt, node + 0x18u);
    if (kind >= 9u) {
        ctx.gpr[2] = 0u;
        return;
    }
    // The jump table at 0x08C8F73C: kinds 7 and 8 leave, 1 and 5 test the code, the others fall to the common release.
    if (kind == 7u || kind == 8u) {
        ctx.gpr[2] = 0x08C8F73Cu + kind * 4u;
        return;
    }
    if (kind == 5u) {
        if (code == 5u) {
            ctx.gpr[2] = 0u;
            return;
        }
    } else if (kind == 1u) {
        if (code == 5u || code == 1u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    call(rt, ctx, 0x08B88F44u, {sp + 0x24u, node + 0x1Cu});
    ctx.gpr[2] = kEmptyRep;
    release_string(rt, ctx, word(rt, sp + 0x24u), sp);
}


// A function-local static float: when the guard byte is clear and 0x08B6441C (guard acquire) says to initialise, the
// value is stored and 0x08B64494 (guard release) called.
void init_static_float(Runtime &rt, AllegrexContext &ctx, u32 guard, u32 value_address, u32 value_bits) {
    if (signed_byte(rt, guard) != 0) return;
    if (call(rt, ctx, 0x08B6441Cu, {guard}) == 0u) return;
    put(rt, value_address, value_bits);
    call(rt, ctx, 0x08B64494u, {guard});
}

// 0x088BE248 (a0 = object, a1 = input, a2 = float out; f0 result): a clamped steering-like value from the wheel set
// (object +4 -> pair of objects at +0/+4), the input's floats (+0, +4, +8, +0xC) and local-static limits; the result is
// clamped to +-the float at +0x78 of the first object of the pair. Stores a 0..1 ratio through a2 when it saturates.
void func_088BE248(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 sp = ctx.gpr[29];
    const u32 self = ctx.gpr[4];
    const u32 input = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    init_static_float(rt, ctx, 0x08CE3070u, 0x08CE30B8u, 0x3F490FDBu);
    init_static_float(rt, ctx, 0x08CE3068u, 0x08CE30B4u, 0x3F060A92u);
    const u32 pair = self + 4u;
    const u32 s5 = word(rt, self + 0x24u);
    const float lean = float_at(rt, word(rt, pair + 4u) + 0x6C4u);
    s32 sign = 1;
    float f2 = 0.0f;
    bool skip_all = false; // |lean| small: the result is the clamp of 0
    if (bits(0x3F0E38E4u) < lean) {
        sign = 1;
    } else if (!(lean < bits(0xBF0E38E4u))) {
        skip_all = true;
    } else {
        sign = -1;
    }
    if (!skip_all) {
        if (word(rt, input + 0xCu) == 1u) {
            float f22 = 0.0f;
            u32 s2 = 0;
            for (u32 i = 0; i < 4u; ++i) {
                if (i >= 2u) continue;
                const u32 a1 = word(rt, pair + 4u);
                const u32 kind = byte_at(rt, a1 + stride_280(i) + 0x159u);
                const u32 r = call(rt, ctx, 0x0894268Cu, {word(rt, pair), i, kind});
                ++s2;
                f22 = f22 + float_at(rt, r + 0xB8u);
            }
            if (s2 != 0u) f22 = f22 / static_cast<float>(static_cast<s32>(s2));
            const u32 a3 = word(rt, pair + 4u);
            float sum = 0.0f;
            u32 counted = 0;
            for (u32 i = 0; i < 4u; ++i) {
                if (i >= 2u) continue;
                sum = sum + float_at(rt, a3 + stride_280(i) + 0x98u);
                ++counted;
            }
            float f24 = 0.0f;
            if (counted != 0u) f24 = sum / static_cast<float>(static_cast<s32>(counted)) + f24;
            float f20 = 0.0f;
            if (byte_at(rt, s5 + 0xFu) != 0u || byte_at(rt, s5 + 0x6Du) != 0u) {
                const float steer = float_at(rt, input + 8u);
                f20 = f22;
                if (!(bits(0x3FB2B8C2u) < steer)) {
                    if (steer < bits(0xBFB2B8C2u)) f20 = -f22;
                    else f20 = 0.0f;
                }
            }
            if (f20 != 0.0f) {
                f2 = f20 + f24;
            } else {
                float f1 = float_at(rt, a3 + 0x6D0u);
                float f12 = float_at(rt, a3 + 0x6CCu);
                float fx = float_at(rt, input);
                f1 = psp_mul(f1, f1);
                f12 = psp_mul(f12, f12);
                float fy = float_at(rt, input + 4u);
                fx = psp_mul(fx, fx);
                fy = psp_mul(fy, fy);
                f12 = f12 + f1;
                const float f23 = fx + fy;
                const float f21 = call_float(rt, ctx, 0x08B49D7Cu, {}, {f12});
                const u32 a_now = word(rt, pair + 4u);
                float sa; // the f0 register after the rotation
                if (f21 < bits(0x3FB1C71Du)) {
                    call_float(rt, ctx, 0x08B498ACu, {sp + 8u, sp + 0xCu}, {float_at(rt, a_now + 0x704u)});
                    float c0 = float_at(rt, sp + 8u);
                    const float c1 = float_at(rt, sp + 0xCu);
                    c0 = -c0;
                    put_float(rt, sp + 4u, c1);
                    put_float(rt, sp, c0);
                    sa = c0;
                } else {
                    const float g12 = -float_at(rt, a_now + 0x6CCu);
                    const float g13 = float_at(rt, a_now + 0x6D0u);
                    const float angle = call_float(rt, ctx, 0x08B4A430u, {}, {g12, g13});
                    call_float(rt, ctx, 0x08B498ACu, {sp, sp + 4u}, {angle});
                    sa = -float_at(rt, sp);
                    put_float(rt, sp, sa);
                }
                float f3 = psp_mul(float_at(rt, input + 4u), sa);
                float f2a = float_at(rt, input);
                const u32 a1 = word(rt, pair + 4u);
                float f1b = psp_mul(f21, bits(0xC0000000u));
                f2a = psp_mul(f2a, float_at(rt, sp + 4u));
                const float f7 = 0.0f;
                const float roll = float_at(rt, a1 + 0x6C4u);
                const float f6 = f2a - f3;
                f1b = psp_mul(f6, f1b);
                float f2b = f1b / f23;
                if (roll < f7) f2b = -f2b;
                const u32 v0 = word(rt, self + 0x14u);
                const u32 a0 = word(rt, self + 0x18u);
                const float f5 = float_at(rt, a1 + 0x6F8u);
                float f4 = float_at(rt, a0 + 0x14u);
                if (byte_at(rt, v0 + 2u) != 0u) {
                    float f0 = float_at(rt, a1 + 0x6B0u);
                    f0 = f0 - f22;
                    f0 = -f0;
                    float f3b = f0 / f22;
                    bool second = true; // fall into the 0x088BE60C check
                    if (1.0f < f3b) {
                        f3b = 1.0f;
                        f4 = psp_mul(f4, f3b);
                        second = false;
                    } else if (f3b < -1.0f) {
                        f3b = -1.0f;
                    } else if (!(f3b < f7)) {
                        f4 = psp_mul(f4, f3b);
                        second = false;
                    }
                    if (second) {
                        const float f0b = psp_mul(f6, float_at(rt, s5 + 0x24u));
                        if (!(0.0f <= f0b)) {
                            f4 = psp_mul(f4, f3b);
                        } else {
                            f3b = 1.0f;
                            f4 = psp_mul(f4, f3b);
                        }
                    }
                }
                f2 = f20 + f24; // the value the paths that leave early use
                if (0.0f < f2b) {
                    if (f5 < f2b) {
                        float f0 = f2b - f5;
                        f0 = psp_mul(f4, f0);
                        f20 = f20 + f0;
                        f2 = f20 + f24;
                        if (f22 < f20) {
                            put_float(rt, out, f20 / f22);
                            const u32 kind = call(rt, ctx, 0x088B1A0Cu, {word(rt, self)});
                            if (!(kind - 2u < 2u)) f20 = f22;
                            f2 = f20 + f24;
                        }
                    }
                } else if (f2b < 0.0f && f2b < f5) {
                    float f0 = f5 - f2b;
                    const float f21b = -f22;
                    f0 = psp_mul(f4, f0);
                    f20 = f20 - f0;
                    f2 = f20 + f24;
                    if (f20 < f21b) {
                        put_float(rt, out, (-f20) / f22);
                        const u32 kind = call(rt, ctx, 0x088B1A0Cu, {word(rt, self)});
                        if (!(kind - 2u < 2u)) f20 = f21b;
                        f2 = f20 + f24;
                    }
                }
            }
            const float limit = float_at(rt, 0x08CE30B8u);
            if (limit < f2) {
                f2 = limit;
            } else {
                const float neg = -limit;
                if (f2 < neg) f2 = neg;
            }
        } else {
            init_static_float(rt, ctx, 0x08CE3060u, 0x08CE30B0u, 0x3EB2B8C2u);
            const float f1 = float_at(rt, 0x08CE30B0u);
            f2 = float_at(rt, input + 8u);
            if (f2 < -f1) {
                f2 = -float_at(rt, 0x08CE30B4u);
            } else if (f1 < f2) {
                f2 = float_at(rt, 0x08CE30B4u);
            } else {
                const float ratio = float_at(rt, 0x08CE30B4u) / f1;
                f2 = psp_mul(f2, ratio);
            }
        }
        if (sign == -1) f2 = -f2;
    }
    const float limit = float_at(rt, word(rt, pair) + 0x78u);
    float result = limit;
    if (!(limit < f2)) {
        result = -limit;
        if (!(f2 < result)) result = f2;
    }
    ctx.fpr[0] = result;
}


// Byte copy of `count` bytes (the original's word / lwl-lwr loops; the blocks never overlap).
void copy_block(Runtime &rt, u32 destination, u32 source, u32 count) {
    for (u32 i = 0; i < count; ++i) rt.memory().store8(destination + i, static_cast<std::uint8_t>(rt.memory().load8(source + i)));
}

// 0x088ED758 (a0 = id, a1 = message, a2 = data, a3 = length, t0 = object): asks the object at +0x538 (virtual +0x90)
// for its id; when it is the given id only the messages in 0x3128 are handled; the messages 0..14 (jump table 0x08C89F30)
// copy a record into the object, call one of the object's callbacks (+0x4E0, +0x4F4) or lock / unlock the +0x1C8 section.
void func_088ED758(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 sp = ctx.gpr[29];
    const u32 id = ctx.gpr[4];
    const u32 message = ctx.gpr[5];
    const u32 data = ctx.gpr[6];
    const u32 length = ctx.gpr[7];
    const u32 self = ctx.gpr[8];
    const u32 owner = word(rt, self + 0x538u);
    const u32 owner_id = call(rt, ctx, word(rt, word(rt, owner) + 0x90u), {owner});
    if (owner_id == id) {
        if (!(message < 0xEu) || ((1u << message) & 0x3128u) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    } else if (!(message < 0xFu)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 section = self + 0x1C8u;
    switch (message) {
    case 0: {
        call(rt, ctx, 0x08B79A88u, {word(rt, self + 0x120u), data, length});
        put(rt, self + 0x124u, length >> 1);
        return;
    }
    case 3: {
        put(rt, self + 0x208u, word(rt, data));
        put(rt, self + 0x20Cu, word(rt, data + 4u));
        rt.memory().store8(self + 0x527u, 1u);
        rt.memory().store8(self + 0x52Cu, 0u);
        put(rt, self + 0x200u, static_cast<u32>(signed_half(rt, data + 8u)));
        if (signed_half(rt, data + 0xAu) != 0) {
            for (u32 i = 0; i < 4u; ++i) rt.memory().store16(self + 0x458u + i * 2u, rt.memory().load16(data + 0xCu + i * 2u));
        } else {
            rt.memory().store16(self + 0x458u, static_cast<std::uint16_t>(0xFFFFFFFFu));
        }
        for (u32 i = 0; i < 4u; ++i) rt.memory().store8(self + 0x4ACu + i, rt.memory().load8(data + 0x14u + i));
        rt.memory().store8(self + 0x52Bu, 1u);
        ctx.gpr[2] = 1u;
        return;
    }
    case 12: {
        rt.memory().store8(self + 0x527u, 1u);
        rt.memory().store8(self + 0x52Cu, 1u);
        put(rt, self + 0x500u, word(rt, data));
        put(rt, self + 0x504u, word(rt, data + 4u));
        for (u32 i = 0; i < 4u; ++i) rt.memory().store8(self + 0x4ACu + i, rt.memory().load8(data + 8u + i));
        rt.memory().store8(self + 0x52Bu, 1u);
        ctx.gpr[2] = 1u;
        return;
    }
    case 1: {
        copy_block(rt, sp, data, 0x50u);
        call(rt, ctx, 0x08B344E0u, {section});
        const u32 low = word(rt, sp + 8u);
        const u32 high = word(rt, sp + 0xCu);
        bool copy = true;
        if (signed_half(rt, self + 0x178u) != -1) {
            const u32 held = self + 0x178u;
            const u32 held_low = word(rt, held + 8u);
            const u32 held_high = word(rt, held + 0xCu);
            if (held_low == low && held_high == high) {
                copy = false;
            } else {
                const u32 flag = byte_at(rt, self + 0x52Au);
                bool newer = false;
                if (high < held_high) newer = true;
                else if (high == held_high && low < held_low) newer = true;
                if (flag == (newer ? 1u : 0u)) copy = false;
            }
        }
        if (copy) {
            copy_block(rt, self + 0x178u, sp, 0x50u);
            rt.memory().store8(self + 0x529u, 1u);
            rt.memory().store8(self + 0x528u, 0u);
        }
        call(rt, ctx, 0x08B3457Cu, {section});
        return;
    }
    case 2: {
        const u32 index = static_cast<u32>(signed_half(rt, data));
        const u32 slot = self + index * 0x90u + 0x218u;
        copy_block(rt, slot, data, 0x90u);
        rt.memory().store8(slot + 0x89u, 1u);
        ctx.gpr[2] = 1u;
        return;
    }
    case 5: {
        u32 state = 3u;
        if ((word(rt, data) | word(rt, data + 4u)) == 0u) state = signed_byte(rt, data + 0xAu) == 0 ? 2u : 1u;
        call(rt, ctx, 0x08B344E0u, {section});
        u32 stored = state;
        if (state == 1u) {
            const u32 held = word(rt, self + 0x548u);
            if (held == 0xFFFFFFFFu || held != word(rt, self + 0x54Cu)) stored = 0u;
        }
        const u32 index = static_cast<u32>(signed_half(rt, data + 8u));
        rt.memory().store8(index + self + 0x460u, static_cast<std::uint8_t>(stored));
        const u32 row = static_cast<u32>(signed_half(rt, data + 8u)) * 8u + self;
        put(rt, row + 0x468u, word(rt, data));
        put(rt, row + 0x46Cu, word(rt, data + 4u));
        rt.memory().store8(static_cast<u32>(signed_half(rt, data + 8u)) + self + 0x4A8u,
                           static_cast<std::uint8_t>(signed_byte(rt, data + 0xBu)));
        call(rt, ctx, 0x08B3457Cu, {section});
        return;
    }
    case 6: {
        call(rt, ctx, 0x08B344E0u, {section});
        const u32 slot_owner = word(rt, self);
        rt.memory().store8(self + id + 0x210u, 1u);
        const GuestResult made = call_guest(rt, ctx, word(rt, slot_owner + 0x18u), {self, 0u}, {});
        ctx.gpr[2] = made.v0;
        ctx.gpr[3] = made.v1;
        const u32 base = self + 0x4C0u;
        const u32 scaled = static_cast<u32>((static_cast<std::uint64_t>(word(rt, base + 0x10u)) * 0x80808081ull) >> 32) >> 9;
        put(rt, base + 0x18u, made.v0);
        put(rt, base + 0x1Cu, made.v1);
        rt.memory().store8(base + id, static_cast<std::uint8_t>(scaled + 1u));
        put(rt, self + 0x4C8u, 2u);
        call(rt, ctx, 0x08B3457Cu, {section});
        return;
    }
    case 7: {
        const u32 shifted = (byte_at(rt, data + 2u) << 5) + 0x4C0u;
        const u32 record = self + shifted;
        const u32 buffer = word(rt, record + 0xCu);
        if (buffer == 0u) {
            ctx.gpr[2] = shifted;
            return;
        }
        const u32 count = byte_at(rt, data);
        call(rt, ctx, 0x08B79A88u, {buffer + byte_at(rt, data + 1u) * 1020u, data + 4u, count});
        const u32 fill = word(rt, record + 4u) + byte_at(rt, data);
        const u32 limit = word(rt, record + 0x10u);
        put(rt, record + 4u, fill);
        ctx.gpr[2] = fill;
        if (fill < limit) return;
        const u32 state_word = word(rt, record + 8u);
        ctx.gpr[2] = state_word ^ 3u;
        if (ctx.gpr[2] != 0u) return;
        put(rt, record + 8u, 4u); // ll / sc: store 4 when the word was 3 (single thread: the store succeeds)
        ctx.gpr[2] = 1u;
        return;
    }
    case 8: {
        call(rt, ctx, 0x08B344E0u, {section});
        const u32 row = static_cast<u32>(signed_half(rt, data + 0xAu)) * 8u + self;
        put(rt, row + 0x488u, word(rt, data));
        put(rt, row + 0x48Cu, word(rt, data + 4u));
        rt.memory().store8(static_cast<u32>(signed_half(rt, data + 0xAu)) + self + 0x4B0u,
                           static_cast<std::uint8_t>(signed_byte(rt, data + 0xCu)));
        call(rt, ctx, 0x08B3457Cu, {section});
        return;
    }
    case 9: {
        const u32 callback = word(rt, self + 0x4E0u);
        ctx.gpr[2] = byte_at(rt, data + 3u);
        if (callback == 0u) return;
        const u32 a2 = byte_at(rt, data + 3u) != 0u ? 0u : data + 4u;
        call(rt, ctx, callback,
             {static_cast<u32>(signed_byte(rt, data)), static_cast<u32>(signed_byte(rt, data + 1u)), a2, byte_at(rt, data + 2u),
              word(rt, self + 0x4E4u)});
        return;
    }
    case 10: {
        const u32 callback = word(rt, self + 0x4E0u);
        ctx.gpr[2] = callback;
        if (callback == 0u) return;
        call(rt, ctx, callback, {static_cast<u32>(signed_byte(rt, data)), 0xFFFFFFFFu, 0u, 0u, word(rt, self + 0x4E4u)});
        return;
    }
    case 11: {
        const u32 callback = word(rt, self + 0x4F4u);
        if (callback == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        call(rt, ctx, callback, {static_cast<u32>(signed_byte(rt, data + 8u)), data, word(rt, self + 0x4F8u)});
        return;
    }
    case 13: {
        put(rt, sp, 0x21u);
        put(rt, sp + 0x14u, data);
        put(rt, sp + 0x18u, length);
        put(rt, sp + 0x10u, id);
        put(rt, sp + 4u, 0u);
        put(rt, sp + 8u, 0u);
        put(rt, sp + 0x1Cu, 0u);
        put(rt, sp + 0xCu, id);
        call(rt, ctx, 0x08B09538u, {sp});
        return;
    }
    case 14: {
        const u32 value = static_cast<u32>(signed_half(rt, data));
        ctx.gpr[2] = 0xFFFFFFFFu;
        if (value != 0xFFFFFFFFu) put(rt, self + 0x510u, value);
        return;
    }
    default: // 4: the jump table leaves with v0 = the table slot's address
        ctx.gpr[2] = 0x08C89F30u + message * 4u;
        return;
    }
}


// Shared-pointer control blocks (the original's ll / sc loops, single thread): the use count is at +4, the weak count at +8,
// the virtual at +8 disposes the object and the one at +0xC destroys the block.
void add_reference(Runtime &rt, u32 control) {
    if (control != 0u) put(rt, control + 4u, word(rt, control + 4u) + 1u);
}
// Drops one use of a non-null control block; v0 ends as the original leaves it (1 after each counter update, else the callee's).
void release_reference(Runtime &rt, AllegrexContext &ctx, u32 control) {
    const u32 uses = word(rt, control + 4u);
    put(rt, control + 4u, uses - 1u);
    ctx.gpr[2] = 1u;
    if (uses != 1u) return;
    call(rt, ctx, word(rt, word(rt, control) + 8u), {control});
    const u32 weak = word(rt, control + 8u);
    put(rt, control + 8u, weak - 1u);
    ctx.gpr[2] = 1u;
    if (weak != 1u) return;
    call(rt, ctx, word(rt, word(rt, control) + 0xCu), {control});
}
// *slot = control, taking a use of the new block and dropping one of the old (nothing when they are the same block).
void assign_reference(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 control) {
    if (word(rt, slot) == control) return;
    add_reference(rt, control);
    const u32 old = word(rt, slot);
    if (old != 0u) release_reference(rt, ctx, old);
    put(rt, slot, control);
}

constexpr u32 kMaxRecords = (0x0FFFu << 16) | 0xFFFFu; // the vector's max_size in 16-byte records

// 0x08B8F81C (a0 = vector of 16-byte records {raw, shared, raw, shared}, a1 = position, a2 = record): insert one record at
// the position. With room left the tail moves up one record (copy-assign, shifting from the end) and the new record is
// assigned in; when full a new buffer of twice the size is allocated and the ranges copied around the new record.
void func_08B8F81C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 sp = ctx.gpr[29];
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    u32 value = ctx.gpr[6];
    const u32 end = word(rt, vector + 4u);
    if (end == word(rt, vector + 8u)) {
        // Full: grow.
        const u32 begin = word(rt, vector);
        const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 4);
        if (size == kMaxRecords) call(rt, ctx, 0x08B5ADDCu, {0x08C9F5E8u});
        u32 capacity = 1u;
        u32 buffer = 0u;
        bool allocate = true;
        if (size != 0u) {
            capacity = size * 2u;
            if (!(capacity < size)) {
                if (capacity == 0u) allocate = false;
            } else {
                capacity = kMaxRecords;
            }
        }
        u32 bytes = 0u;
        if (allocate) {
            bytes = capacity << 4;
            buffer = call(rt, ctx, 0x08B35DA0u, {bytes});
        }
        u32 first = call(rt, ctx, 0x08B8F7A0u, {word(rt, vector), position, buffer, sp + 2u});
        if (first != 0u) {
            put(rt, first, word(rt, value));
            const u32 shared_a = word(rt, value + 4u);
            put(rt, first + 4u, shared_a);
            add_reference(rt, shared_a);
            put(rt, first + 8u, word(rt, value + 8u));
            const u32 shared_b = word(rt, value + 0xCu);
            put(rt, first + 0xCu, shared_b);
            add_reference(rt, shared_b);
        }
        const u32 tail_end = call(rt, ctx, 0x08B8F7A0u, {position, word(rt, vector + 4u), first + 0x10u, sp + 1u});
        const u32 old_begin = word(rt, vector);
        call(rt, ctx, 0x08B8F680u, {old_begin, word(rt, vector + 4u), sp});
        if (word(rt, vector) != 0u) call(rt, ctx, 0x08B35254u, {word(rt, vector)});
        ctx.gpr[2] = buffer + bytes;
        put(rt, vector + 8u, buffer + bytes);
        put(rt, vector + 4u, tail_end);
        put(rt, vector, buffer);
        return;
    }
    if (end != 0u) {
        const u32 last = end - 0x10u;
        put(rt, end, word(rt, last));
        const u32 shared_a = word(rt, last + 4u);
        put(rt, end + 4u, shared_a);
        add_reference(rt, shared_a);
        put(rt, end + 8u, word(rt, last + 8u));
        const u32 shared_b = word(rt, last + 0xCu);
        put(rt, end + 0xCu, shared_b);
        add_reference(rt, shared_b);
    }
    put(rt, vector + 4u, word(rt, vector + 4u) + 0x10u);
    // The temporary copy of the record (a use taken of each shared part).
    const u32 temp_raw_a = word(rt, value);
    const u32 temp_shared_a = word(rt, value + 4u);
    put(rt, sp + 0x14u, temp_shared_a);
    put(rt, sp + 0x10u, temp_raw_a);
    add_reference(rt, temp_shared_a);
    const u32 temp_shared_b = word(rt, value + 0xCu);
    put(rt, sp + 0x1Cu, temp_shared_b);
    const u32 temp_raw_b = word(rt, value + 8u);
    put(rt, sp + 0x18u, temp_raw_b);
    add_reference(rt, temp_shared_b);
    const u32 new_end = word(rt, vector + 4u);
    u32 source_end = new_end - 0x20u; // s5
    const s32 count = static_cast<s32>(source_end - position) >> 4;
    put(rt, sp + 0x20u, static_cast<u32>(count));
    if (count > 0) {
        u32 destination = new_end - 0x10u;
        u32 source = source_end - 0x10u;
        for (s32 done = 0; done != count; ++done) {
            const u32 raw = word(rt, source);
            destination -= 0x10u;
            put(rt, destination, raw);
            source_end = source;
            assign_reference(rt, ctx, destination + 4u, word(rt, source + 4u));
            put(rt, destination + 8u, word(rt, source + 8u));
            assign_reference(rt, ctx, destination + 0xCu, word(rt, source + 0xCu));
            source = source_end - 0x10u;
        }
    }
    put(rt, position, word(rt, sp + 0x10u));
    assign_reference(rt, ctx, position + 4u, word(rt, sp + 0x14u));
    put(rt, position + 8u, word(rt, sp + 0x18u));
    assign_reference(rt, ctx, position + 0xCu, word(rt, sp + 0x1Cu));
    const u32 shared_b = word(rt, sp + 0x1Cu);
    ctx.gpr[2] = shared_b;
    if (shared_b != 0u) release_reference(rt, ctx, shared_b);
    const u32 shared_a = word(rt, sp + 0x14u);
    if (shared_a != 0u) release_reference(rt, ctx, shared_a);
}


// 0x088AF3C8 (a0 = set, a1 = value): over the set's records (stride 0x440 at +0x10, count at +4, an index table at +8) it finds
// the highest word at +0x3F8 and collects the indices whose entry exists and whose +0x3F8 word equals a1; two rounds of
// pairwise contact tests and per-record updates follow, picking the closest hit each round (the float at +0x3C8); returns
// 1 when a1 < the highest word.
void func_088AF3C8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 threshold = static_cast<s32>(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x80u);
    const s32 count = static_cast<s32>(word(rt, self + 4u));
    s32 highest = -1;
    std::vector<u32> list; // the original's alloca'd index list
    auto record = [&](u32 index) { return word(rt, self + 0x10u) + index * 0x440u; };
    if (count > 0) {
        s32 limit = count;
        u32 offset = 0;
        for (s32 index = 0; index < limit; ++index) {
            const s32 value = static_cast<s32>(word(rt, word(rt, self + 0x10u) + offset + 0x3F8u));
            if (highest < value) highest = value;
            offset += 0x440u;
            const u32 entry = word(rt, word(rt, self + 8u) + static_cast<u32>(index) * 4u);
            if (entry != 0u && threshold == value) {
                list.push_back(static_cast<u32>(index));
                limit = static_cast<s32>(word(rt, self + 4u));
            }
        }
    }
    const u32 listed = static_cast<u32>(list.size());
    if (listed == 0u) {
        ctx.gpr[2] = threshold < highest ? 1u : 0u;
        return;
    }
    for (u32 i = 0; i < listed; ++i) {
        const u32 index = list[i];
        call(rt, ctx, 0x088ABCA8u, {record(index), word(rt, self + 0xCu) + index * 0x40u});
    }
    for (u32 round = 0; round != 2u; ++round) {
        if (byte_at(rt, self + 0x20u) != 0u) {
            // Contact pairs among the listed records, both orders.
            for (u32 i = 0; i + 1u < listed; ++i) {
                const u32 first = record(list[i]);
                for (u32 j = i + 1u; j < listed; ++j) {
                    const u32 second = word(rt, self + 0x10u) + list[j] * 0x440u;
                    if (byte_at(rt, first + 0x43Eu) != 0u && byte_at(rt, second + 0x43Eu) != 0u) {
                        call(rt, ctx, 0x088AC6DCu, {first, second});
                        call(rt, ctx, 0x088AC6DCu, {second, word(rt, self + 0x10u) + list[i] * 0x440u});
                    }
                }
            }
        }
        for (u32 i = 0; i < listed; ++i) {
            const u32 index = list[i];
            call(rt, ctx, 0x088ACD24u, {word(rt, self + 0x10u) + index * 0x440u});
            call(rt, ctx, 0x088AD9E0u, {index * 0x440u + word(rt, self + 0x10u)});
            call(rt, ctx, 0x088AC8F0u, {index * 0x440u + word(rt, self + 0x10u)});
        }
        // Scan the listed records for the first / closest one that passes 0x088AB884.
        float best = bits(0x40000000u);
        bool found = false;
        s32 best_index = -1;
        s32 partner = -1;
        for (u32 i = 0; i < listed; ++i) {
            const u32 index = list[i];
            const u32 shape = word(rt, self + 0xCu) + index * 0x40u;
            const u32 rec = word(rt, self + 0x10u) + index * 0x440u;
            const u32 hit = call(rt, ctx, 0x088AB884u, {rec, word(rt, self + 0x14u), word(rt, shape + 0x30u), word(rt, shape + 0x34u)});
            if (hit != 0u) {
                const float distance = float_at(rt, rec + 0x3C8u);
                if (distance < best) {
                    best = distance;
                    best_index = static_cast<s32>(index);
                    found = true;
                }
            }
        }
        if (byte_at(rt, self + 0x20u) != 0u) {
            for (u32 i = 0; i < listed; ++i) {
                const u32 outer = list[i];
                const u32 rec_outer = record(outer);
                if (byte_at(rt, rec_outer + 0x43Eu) == 0u) continue;
                for (u32 j = 0; j != listed; ++j) {
                    if (i == j) continue;
                    const u32 other = list[j];
                    const u32 rec_other = record(other);
                    if (byte_at(rt, rec_other + 0x43Eu) == 0u) continue;
                    if (call(rt, ctx, 0x088ADD60u, {rec_outer, rec_other}) == 0u) continue;
                    const float distance = float_at(rt, rec_outer + 0x3C8u);
                    if (best <= distance) continue;
                    best = distance;
                    partner = static_cast<s32>(other);
                    best_index = static_cast<s32>(outer);
                    found = true;
                }
            }
        }
        if (!found) break;
        if (best_index >= 0) {
            for (u32 i = 0; i < listed; ++i)
                call_float(rt, ctx, 0x088AC5ACu, {word(rt, self + 0x10u) + list[i] * 0x440u}, {best});
            const u32 rec = record(static_cast<u32>(best_index));
            u32 mode = 1u;
            if (partner < 0) {
                if (byte_at(rt, rec + 0x43Au) != 0u) {
                    best = call_float(rt, ctx, 0x088AE77Cu, {rec}, {});
                    mode = 0u;
                } else if (byte_at(rt, rec + 0x43Bu) == 0u) {
                    best = 0.0f;
                    mode = 0u;
                } else {
                    best = call_float(rt, ctx, 0x088AECC8u, {rec}, {});
                    mode = 0u;
                }
                partner = -1;
            } else {
                best = call_float(rt, ctx, 0x088AE41Cu, {rec, word(rt, self + 0x10u) + static_cast<u32>(partner) * 0x440u}, {});
            }
            const u32 rec_a = word(rt, rec + 0x3FCu);
            if (rec_a != 0u) call_float(rt, ctx, 0x0893B824u, {rec_a, mode, 0u}, {best});
            if (partner >= 0) {
                const u32 rec_b = word(rt, record(static_cast<u32>(partner)) + 0x3FCu);
                if (rec_b != 0u) call_float(rt, ctx, 0x0893B824u, {rec_b, mode, 0u}, {best});
            }
            const u32 callback = word(rt, self + 0x18u);
            if (callback != 0u)
                call_float(rt, ctx, callback, {static_cast<u32>(best_index), static_cast<u32>(partner), word(rt, self + 0x1Cu)}, {best});
        }
    }
    for (u32 i = 0; i < listed; ++i) {
        const u32 index = list[i];
        const u32 shape = word(rt, self + 0xCu) + index * 0x40u;
        call(rt, ctx, 0x088AB6C4u, {word(rt, self + 0x10u) + index * 0x440u, word(rt, self + 0x14u), word(rt, shape + 0x30u), word(rt, shape + 0x34u)});
        call(rt, ctx, 0x088AC10Cu, {index * 0x440u + word(rt, self + 0x10u), word(rt, self + 0xCu) + index * 0x40u});
    }
    ctx.gpr[2] = threshold < highest ? 1u : 0u;
}

} // namespace
} // namespace new_batch_main_143
} // namespace gt::game

#define NEW143(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_143::name, gt::ReturnKind::kind)
NEW143(0x0894F6C8u, func_0894F6C8, Word);
NEW143(0x08A8FA4Cu, func_08A8FA4C, Word);
NEW143(0x088BE248u, func_088BE248, Float);
NEW143(0x088ED758u, func_088ED758, Word);
NEW143(0x08B8F81Cu, func_08B8F81C, Word);
NEW143(0x088AF3C8u, func_088AF3C8, Word);
