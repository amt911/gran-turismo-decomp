// ---- from main-session batch 241 (draft b5be033bc21c0daa2b412f4f80a30042ef324618) ----
// New batch main-241 (draft, unverified): functions the gate could not run with useful inputs. Structured C written from
// `tools.disasm`; the two old drafts (0889AD18 from hot_batch_97, 08AC9CC8 from hot_batch_115) were re-read against the
// asm and copied here, the other four are new. Input shapes: /tmp/claude-1000/shapes_main241.txt (for merging into
// config/synth_shapes.txt). Before registering: the four audits (frame_audit, literal_check, sp_loads, arg_reads) and
// the synthetic gate; drop what never compares.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_241 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 / v1 / f0 hold the callee's results, as after the original's `jal`.
// The argument registers are written into the caller context too, as the original sets them before the jal.
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                  std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float value : floats) {
        if (freg <= 15u) ctx.fpr[freg++] = value;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return callf(rt, ctx, target, arguments).v0;
}
// A `jalr` through a vtable slot: the callee starts with v0 = its own address.
GuestResult call_slot(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return callf(rt, ctx, target, arguments);
}
// A call with the argument registers set as the original sets them before the jal (a0-a3, t0-t3, in order).
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return callf(rt, ctx, target, arguments).v0;
}
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats) {
    return callf(rt, ctx, target, arguments, floats);
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 signed_of(u32 value) { return static_cast<s32>(value); }
u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }

// $f20 / $f21 are callee-saved; the originals save and restore them.
struct SavedFloats {
    AllegrexContext &ctx;
    float f20;
    float f21;
    explicit SavedFloats(AllegrexContext &c) : ctx(c), f20(c.fpr[20]), f21(c.fpr[21]) {}
    ~SavedFloats() {
        ctx.fpr[20] = f20;
        ctx.fpr[21] = f21;
    }
};

// 0x0889AD18(self, op): a switch over the command id `op` (0 .. 0x45) on t = self[4][0xB04] (returns 0 when t is null).
// t is an object with a vtable; the groups come from the jump table at 0x08C89B10. The epilogue answers a0, so every
// arm sets the value it returns. The other ids answer 0.
void func_0889AD18(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], op = ctx.gpr[5];
    const u32 a2 = word(rt, self + 4u);
    const u32 t = word(rt, a2 + 0xB04u);
    ctx.gpr[2] = 0u;
    if (t == 0u) return;
    const auto slot = [&](u32 offset) { return word(rt, word(rt, t) + offset); };
    const auto show = [&](float f0) { (void)call_setf(rt, ctx, 0x08B53844u, {}, {f0}); };
    const auto finish = [&](u32 value) { ctx.gpr[2] = value; };
    if (op >= 0x46u) {
        finish(0u);
        return;
    }
    switch (op) {
    case 0: {
        const GuestResult r = call_slot(rt, ctx, slot(0x64u), {t});
        finish((std::bit_cast<float>(0x3DCCCCCDu) < r.f0) ? 1u : 0u);
        return;
    }
    case 1: case 2: case 3: case 4: {
        const GuestResult r = call_slot(rt, ctx, slot(0xA4u), {t, op - 1u});
        show(r.f0);
        finish(0u);
        return;
    }
    case 5: {
        const GuestResult r = call_slot(rt, ctx, slot(0x5Cu), {t});
        show(r.f0);
        finish(0u);
        return;
    }
    case 6: {
        const GuestResult r = call_slot(rt, ctx, slot(0x9Cu), {t, 0u});
        show(r.f0);
        finish(0u);
        return;
    }
    case 42: case 43: case 44: case 45: {
        const GuestResult r = call_slot(rt, ctx, slot(0x9Cu), {t, op - 0x2Au});
        show(r.f0);
        finish(0u);
        return;
    }
    case 7:
        finish(word(rt, a2 + 0x74u));
        return;
    case 8: {
        const GuestResult r = call_slot(rt, ctx, slot(0x60u), {t});
        (void)call_setf(rt, ctx, 0x08B55FB0u, {}, {psp_mul(r.f0, std::bit_cast<float>(0x42652EE0u))});
        finish(0u);
        return;
    }
    case 9:
        (void)call_set(rt, ctx, 0x08B566BCu, {a2 + 0x8A0u});
        finish(0u);
        return;
    case 10:
        finish(0u);
        return;
    case 11: case 12: case 13: case 14: {
        const u32 o = word(rt, a2 + 0x58u);
        const u32 third = o == 0u ? 0u : word(rt, o + 0x10u);
        (void)call_set(rt, ctx, 0x0889AB88u, {t, op - 0xBu, third});
        finish(0u);
        return;
    }
    case 15: case 16: case 17: case 18: {
        const u32 k = op - 0xFu;
        const u32 r = a2 + k * 0xD4u + 0x70u;
        (void)call_set(rt, ctx, 0x08899D84u, {t, k});
        if ((k & 1u) != 0u) (void)call_setf(rt, ctx, 0x08B5606Cu, {}, {std::bit_cast<float>(0x43340000u)});
        put8(rt, r + 8u + 0xD0u, 1u);
        (void)call_set(rt, ctx, 0x08B56A00u, {3u});
        (void)call_set(rt, ctx, 0x08B568DCu, {r + 0x94u});
        (void)call_set(rt, ctx, 0x08B566BCu, {r + 0x14u});
        (void)call_set(rt, ctx, 0x08B56A00u, {0u});
        finish(0u);
        return;
    }
    case 36: case 37: case 38: case 39: {
        const u32 e = a2 + op * 0xD4u - 0x1D58u;
        const u32 lit = byte_u(rt, e + 0xD2u);
        const u32 r = call_set(rt, ctx, 0x08B587ECu, {});
        u32 flag = ((r ^ 2u) == 0u) ? 1u : 0u;
        flag ^= lit;
        put(rt, e + 0xCCu, flag == 0u ? 1u : 2u);
        put8(rt, e + 0xD1u, 1u);
        finish(0u);
        return;
    }
    case 52: case 53: {
        const GuestResult r = call_slot(rt, ctx, slot(op == 0x34u ? 0x68u : 0x6Cu), {t});
        show(r.f0);
        finish(0u);
        return;
    }
    case 56: case 57: {
        const GuestResult r = call_slot(rt, ctx, slot(0x74u), {t, op - 0x38u});
        show(r.f0);
        finish(0u);
        return;
    }
    case 67: case 68: case 69: {
        const u32 s = a2 + op * 0xA4u - 0x21FCu;
        if (byte_u(rt, s + 0x28u) == 0u) {
            finish(0u);
            return;
        }
        (void)call_setf(rt, ctx, 0x08B56128u, {}, {float_at(rt, s + 0x4Cu)});
        (void)call_setf(rt, ctx, 0x08B5606Cu, {}, {float_at(rt, s + 0x50u)});
        const u32 group = word(rt, word(rt, self + 4u) + 0x68u);
        if (group == 0u) {
            finish(0u);
            return;
        }
        (void)call_set(rt, ctx, 0x08AF8590u, {group, s + 0x58u, word(rt, s + 4u)});
        (void)call_set(rt, ctx, 0x08B569A0u, {});
        (void)call_set(rt, ctx, 0x08B568DCu, {s + 0x58u});
        const u32 base = word(rt, word(rt, self + 4u) + 0x68u);
        const u32 entry = word(rt, base + 0x38u) + word(rt, s) * 48u;
        const u32 found = call_set(rt, ctx, 0x08AF8750u, {entry});
        put8(rt, s + 0x54u, found);
        (void)call_set(rt, ctx, 0x08B56940u, {});
        finish(byte_u(rt, s + 0x54u) != 0u ? 0u : 1u);
        return;
    }
    default:
        if (op - 0x14u < 0x10u) (void)call_set(rt, ctx, 0x08B566BCu, {a2 + 0x4A0u + ((op - 0x14u) << 6u)});
        finish(0u);
        return;
    }
}

// 0x088C85E4(self, out, mode; f12 = angle): reads two vectors through 0x088D32E0 (the kind chosen by two query calls),
// builds an orthonormal basis with the float-vector helpers, and transforms the four rows of `out` by it. A null id
// query clears self[8] and answers 0.
void func_088C85E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 mode_arg = ctx.gpr[6];
    const float angle = ctx.fpr[12];
    const u32 handle = word(rt, self + 4u);
    SavedFloats saved(ctx);
    GuestFrame frame(ctx, 0x100u);
    if (call(rt, ctx, 0x088D2C30u, {handle, 0x6u}) == 0u) {
        put8(rt, self + 8u, 0u);
        ctx.gpr[2] = 0u;
        return;
    }
    u32 masked = mode_arg & 0xFFu;
    u32 kind;
    u32 mode;
    if (masked != 0u) {
        if (call(rt, ctx, 0x088D2C30u, {handle, 0x9u}) != 0u) {
            kind = 0x9u;
            mode = 0x8u;
        } else if (call(rt, ctx, 0x088D2C30u, {handle, 0x2u}) != 0u) {
            kind = 0x2u;
            mode = 0x1u;
        } else {
            kind = 0x6u;
            mode = 0x5u;
        }
    } else {
        if (call(rt, ctx, 0x088D2C30u, {handle, 0x8u}) != 0u) {
            kind = 0x8u;
            mode = 0x7u;
        } else if (call(rt, ctx, 0x088D2C30u, {handle, 0x1u}) != 0u) {
            kind = 0x1u;
            mode = 0x0u;
        } else {
            kind = 0x6u;
            mode = 0x5u;
        }
    }
    (void)call_setf(rt, ctx, 0x088D32E0u, {frame.at(0xCu), handle, kind}, {angle});
    const float a0 = float_at(rt, frame.at(0xCu));
    const float a1 = float_at(rt, frame.at(0x10u));
    const float a2 = float_at(rt, frame.at(0x14u));
    put_float(rt, frame.at(0x28u), a0);
    put_float(rt, frame.at(0x2Cu), a1);
    put_float(rt, frame.at(0x30u), a2);
    put_float(rt, frame.at(0x34u), 1.0f);
    (void)call_setf(rt, ctx, 0x088D32E0u, {frame.at(0u), handle, kind}, {angle + 1.0f});
    const float b0 = float_at(rt, frame.at(0u));
    const float b1 = float_at(rt, frame.at(4u));
    const float b2 = float_at(rt, frame.at(8u));
    put_float(rt, frame.at(0x18u), a0);
    put_float(rt, frame.at(0x1Cu), a1);
    put_float(rt, frame.at(0x20u), a2);
    put_float(rt, frame.at(0x24u), 1.0f);
    put_float(rt, frame.at(0x38u), b0);
    put_float(rt, frame.at(0x3Cu), b1);
    put_float(rt, frame.at(0x40u), b2);
    put_float(rt, frame.at(0x44u), 1.0f);
    put_float(rt, frame.at(0xCu), b0);
    put_float(rt, frame.at(0x10u), b1);
    put_float(rt, frame.at(0x14u), b2);
    put_float(rt, frame.at(0x58u), 0.0f);
    put_float(rt, frame.at(0x5Cu), 1.0f);
    put_float(rt, frame.at(0x60u), 0.0f);
    put_float(rt, frame.at(0x64u), 0.0f);
    (void)call_setf(rt, ctx, 0x08B4A8D4u, {frame.at(0x18u), frame.at(0x18u), frame.at(0x38u)}, {});
    const float c0 = float_at(rt, frame.at(0x18u));
    const float c1 = float_at(rt, frame.at(0x1Cu));
    const float c2 = float_at(rt, frame.at(0x20u));
    put_float(rt, frame.at(0x48u), c0);
    put_float(rt, frame.at(0x4Cu), c1);
    put_float(rt, frame.at(0x50u), c2);
    put_float(rt, frame.at(0x54u), 0.0f);
    const GuestResult first = callf(rt, ctx, 0x08B4A744u, {frame.at(0x48u)});
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0x48u)}, {1.0f / std::sqrt(first.f0)});
    put_float(rt, frame.at(0x64u), 0.0f);
    const GuestResult second = callf(rt, ctx, 0x08B4A744u, {frame.at(0x58u)});
    (void)call_setf(rt, ctx, 0x08B4A6A0u, {frame.at(0x58u)}, {1.0f / std::sqrt(second.f0)});
    (void)call_setf(rt, ctx, 0x08B4A82Cu, {frame.at(0x78u), frame.at(0x58u), frame.at(0x48u)}, {});
    put_float(rt, frame.at(0x90u), float_at(rt, frame.at(0x78u)));
    put_float(rt, frame.at(0x94u), float_at(rt, frame.at(0x7Cu)));
    put_float(rt, frame.at(0x98u), float_at(rt, frame.at(0x80u)));
    put_float(rt, frame.at(0x9Cu), 0.0f);
    put_float(rt, frame.at(0xA0u), float_at(rt, frame.at(0x58u)));
    put_float(rt, frame.at(0xA4u), float_at(rt, frame.at(0x5Cu)));
    put_float(rt, frame.at(0xA8u), float_at(rt, frame.at(0x60u)));
    put_float(rt, frame.at(0xACu), float_at(rt, frame.at(0x64u)));
    put_float(rt, frame.at(0xB0u), float_at(rt, frame.at(0x48u)));
    put_float(rt, frame.at(0xB4u), float_at(rt, frame.at(0x4Cu)));
    put_float(rt, frame.at(0xB8u), float_at(rt, frame.at(0x50u)));
    put_float(rt, frame.at(0xBCu), float_at(rt, frame.at(0x54u)));
    put_float(rt, frame.at(0xC0u), float_at(rt, frame.at(0x28u)));
    put_float(rt, frame.at(0xC4u), float_at(rt, frame.at(0x2Cu)));
    put_float(rt, frame.at(0xC8u), float_at(rt, frame.at(0x30u)));
    put_float(rt, frame.at(0xCCu), float_at(rt, frame.at(0x34u)));
    // movn s2, s2 ^ 1, (s3 < 2): a small mode flips the choice taken from the argument.
    if (mode < 2u) masked ^= 1u;
    const float sign = masked != 0u ? 1.5f : -1.5f;
    put_float(rt, out + 0x00u, 0.0f);
    put_float(rt, out + 0x04u, 1.0f);
    put_float(rt, out + 0x08u, 0.0f);
    put_float(rt, out + 0x0Cu, 1.0f);
    put_float(rt, out + 0x10u, sign);
    put_float(rt, out + 0x14u, 1.0f);
    put_float(rt, out + 0x18u, 0.0f);
    put_float(rt, out + 0x1Cu, 1.0f);
    put_float(rt, out + 0x20u, sign);
    put_float(rt, out + 0x24u, 2.5f);
    put_float(rt, out + 0x28u, 0.0f);
    put_float(rt, out + 0x2Cu, 1.0f);
    put_float(rt, out + 0x30u, 0.0f);
    put_float(rt, out + 0x34u, 2.5f);
    put_float(rt, out + 0x38u, 0.0f);
    put_float(rt, out + 0x3Cu, 1.0f);
    (void)call_setf(rt, ctx, 0x08B4A774u, {out, frame.at(0x90u), out}, {});
    (void)call_setf(rt, ctx, 0x08B4A774u, {out + 0x10u, frame.at(0x90u), out + 0x10u}, {});
    (void)call_setf(rt, ctx, 0x08B4A774u, {out + 0x20u, frame.at(0x90u), out + 0x20u}, {});
    (void)call_setf(rt, ctx, 0x08B4A774u, {out + 0x30u, frame.at(0x90u), out + 0x30u}, {});
}

// 0x08B43014(self, a1, a2, a3, t0, t1, t2, t3): finds or takes a slot in the 0x80-entry table at self+0x454 and links a
// new node (the block 08B2CB18 returns for self+0xCC) into the intrusive list. Returns -1 when a query fails or the new
// slot holds a node, else the node index 08B42204 answered.

// 0x08A930E8(self, a1, a2, a3): when the list header a3 points at itself, rebuilds the string keyed object at
// self+0x38: a temporary name from 08A6972C is looked up in a2 (08BFE7D0), the pair is moved or a new element is built,
// and the found/made object is recorded in the list through 08A924F0 / 08A91A38. Otherwise it walks the list a3 and
// rebuilds the object through 08B85EB8, 08B36170 and the virtual at +0x11C. The many stack temporaries are libstdc++
// strings and the list node; only the addresses the callees take matter.

// 0x08AC9CC8(self, name, create): looks the name up (08AC99A4 over the pair 08B1CE38 returns); a hit returns
// entry + 0x28. Without `create` (low byte) 0. Else a new entry (08AC9A68): for the name 0x08CB4948 an object per word
// of the zero-terminated list 08C3CEDC(0x08D00120) is made (08B62344(20) + 08AD5590, named by 08B37640, through
// 08AD5460 / 08AD542C / 08ADC988) into the vector at entry + 0x30, 08AD36AC is told the size sum and 08AC963C(entry,
// object->+0x10, text, size, index, running sum) records each; for any other name one object is made from the name
// (destroyed through its virtual slot 4 and the entry 08AC9240 on failure), pushed on that vector and recorded.
// Success sets entry->+0x28 = self, entry->+0x2C = entry and returns entry + 0x28.
void func_08AC9CC8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 create = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x1040u);
    constexpr u32 kDefaultName = 0x08CB4948u;
    constexpr u32 kFormat = 0x08CB4950u;
    call_set(rt, ctx, 0x08B1CE38u, {name, name});
    const u32 pair_low = ctx.gpr[2];
    const u32 pair_high = ctx.gpr[3];
    const u32 found = call_set(rt, ctx, 0x08AC99A4u, {pair_low, pair_high});
    if (found != 0u) {
        ctx.gpr[2] = found + 0x28u;
        return;
    }
    if (create == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 entry;
    const u32 text = frame.at(0x808u);
    if (call_set(rt, ctx, 0x08B35FD8u, {name, kDefaultName}) == 0u) {
        const u32 owner = call_set(rt, ctx, 0x08AD354Cu, {});
        const u32 label = call_set(rt, ctx, 0x08C1A0C4u, {0x08D00138u});
        call_set(rt, ctx, 0x08B37640u, {frame.at(8u), kFormat, owner, label});
        const u32 list = call_set(rt, ctx, 0x08C3CEDCu, {0x08D00120u});
        if (word(rt, list) == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        u32 count = 0u;
        for (u32 cursor = list;; cursor += 4u) {
            ++count;
            if (word(rt, cursor + 4u) == 0u) break;
        }
        entry = call_set(rt, ctx, 0x08AC9A68u, {kDefaultName});
        const u32 objects = entry + 0x30u;
        put(rt, frame.at(4u), 0u);
        const u32 begin = word(rt, objects);
        const u32 end = word(rt, objects + 4u);
        const u32 size = static_cast<u32>(signed_of(end - begin) >> 2);
        if (count < size) call_set(rt, ctx, 0x08C3D0C8u, {objects, begin + count * 4u, end});
        else call_set(rt, ctx, 0x08C3D298u, {objects, end, count - size, frame.at(4u)});
        u32 total = 0u;
        if (signed_of(count) > 0) {
            u32 cursor = list;
            for (u32 index = 0u; index != count; ++index) {
                const u32 block = call_set(rt, ctx, 0x08B62344u, {0x14u});
                call_set(rt, ctx, 0x08AD5590u, {block});
                put(rt, word(rt, objects) + index * 4u, block);
                call_set(rt, ctx, 0x08B37640u, {frame.at(0x408u), kFormat, frame.at(8u), word(rt, cursor)});
                cursor += 4u;
                if (call_set(rt, ctx, 0x08AD5460u, {block, frame.at(0x408u)}) != 0u) {
                    call_set(rt, ctx, 0x08AD542Cu, {block});
                    total += call_set(rt, ctx, 0x08ADC988u, {word(rt, block + 0x10u), 0u});
                }
            }
        }
        call_set(rt, ctx, 0x08AD36ACu, {entry, total});
        if (signed_of(count) > 0) {
            u32 running = 0u;
            for (u32 index = 0u; index != count; ++index) {
                const u32 object = word(rt, word(rt, objects) + index * 4u);
                const u32 resource = word(rt, object + 0x10u);
                const u32 resource_size = call_set(rt, ctx, 0x08ADC988u, {resource, text});
                call_set(rt, ctx, 0x08AC963Cu, {entry, resource, text, resource_size, index, running});
                running += resource_size;
            }
        }
    } else {
        entry = call_set(rt, ctx, 0x08AC9A68u, {name});
        const u32 block = call_set(rt, ctx, 0x08B62344u, {0x14u});
        call_set(rt, ctx, 0x08AD5590u, {block});
        if (call_set(rt, ctx, 0x08AD5460u, {block, name}) == 0u) {
            if (block != 0u) {
                ctx.gpr[3] = word(rt, block);
                ctx.gpr[2] = word(rt, ctx.gpr[3] + 4u);
                call_set(rt, ctx, ctx.gpr[2], {block});
            }
            call_set(rt, ctx, 0x08AC9240u, {entry});
            ctx.gpr[2] = 0u;
            return;
        }
        call_set(rt, ctx, 0x08AD542Cu, {block});
        const u32 resource = word(rt, block + 0x10u);
        const u32 resource_size = call_set(rt, ctx, 0x08ADC988u, {resource, text});
        call_set(rt, ctx, 0x08AD36ACu, {entry, resource_size});
        put(rt, frame.at(0u), block);
        const u32 objects = entry + 0x30u;
        u32 end = word(rt, objects + 4u);
        if (end == word(rt, objects + 8u)) {
            call_set(rt, ctx, 0x08C3D12Cu, {objects, end, frame.at(0u)});
        } else {
            if (end != 0u) {
                put(rt, end, block);
                end = word(rt, objects + 4u);
            }
            put(rt, objects + 4u, end + 4u);
        }
        call_set(rt, ctx, 0x08AC963Cu, {entry, resource, text, resource_size, 0u, 0u});
    }
    if (entry == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, entry + 0x28u, self);
    put(rt, entry + 0x2Cu, entry);
    ctx.gpr[2] = entry + 0x28u;
}

} // namespace
} // namespace new_batch_main_241
} // namespace gt::game

#define NEWMAIN241(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_241::name, gt::ReturnKind::kind)
NEWMAIN241(0x0889AD18u, func_0889AD18, Word);
NEWMAIN241(0x088C85E4u, func_088C85E4, Word);
NEWMAIN241(0x08AC9CC8u, func_08AC9CC8, Word);

// ---- from main-session batch 242 (draft 364b63d4116a2d5a2ee4381cb06964c2cd8ed6ea) ----
// New batch, main session 242: seven functions the recompiler emitted that no src/ file defines or registers. Written
// by hand from `tools/disasm` as structured C; a draft batch, not verified by the differential gate.
//
// Shapes: two walks over an intrusive list of nodes (0x08A91AEC, 0x089EB244) that hand each element to a virtual; a
// strip builder over a table of 24-byte records (0x088C9C08); a straight-line slice of a 3D transform (0x088ACFD8); the
// same binary subdivision written twice (0x08901DB8 / 0x089041C8, byte-table offset 0x98 vs 0x80 and a different
// delegate); and a UMD read loop (0x08B14188) whose imports are the unresolved `scesupPreAcc` stubs.
//
// The delay slot of a `jal` runs BEFORE the call, so values a call leaves for the next instruction are read after it and
// values stored in its delay slot are the pre-call ones. v0 (and v1 / f0 where a caller could read them) is left as the
// original leaves it on every path: the call helpers below keep v0 / v1 / f0 of the callee, which is what a callee that
// never writes v0 hands on. Guest memory only through the helpers; locals are named and typed as the asm shows them.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_242 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values afterwards, and the argument registers
// the original sets before the `jal` are written to the context, so a callee that reads one the call does not pass sees
// the value the original had.
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                     std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : floats) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_set(rt, ctx, target, arguments).v0;
}
// `lw v1, slot(vtable); jalr v1` with v0 = the slot and v1 = the vtable, as the original leaves them.
GuestResult call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    return call_set(rt, ctx, target, arguments);
}
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }

// Allegrex `trunc.w.s`: toward zero; NaN and values out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}
// The compiler's float -> unsigned-bits conversion: trunc.w.s, plus the 2^31 fixup for values >= 2^31 (0x4F000000).
u32 float_bits_unsigned(float value) {
    constexpr float kTwo31 = 2147483648.0f;
    if (!(kTwo31 <= value)) return static_cast<u32>(trunc_w_s(value));
    return static_cast<u32>(trunc_w_s(value - kTwo31)) | (0x8000u << 16);
}

// -------------------------------------------------------------------------------------------------------------------
// 0x08A91AEC (a0 = object, a1 = handed to every visitor): walks word(a0 + 0x14)'s chain, asks each node's object about
// the object's own word +0x14 and either dispatches it through vtable slot 0x28 or looks the pair up in a map and runs
// the mapped visitor's slot 0x28. The chain is advanced with 0x08B5F100 until it hands back the node at sp + 0x34.

// -------------------------------------------------------------------------------------------------------------------
// 0x089EB244 (a0 = object, a1 = handed to the callbacks): takes the lock at 0x08CF575C, reads the two indices at +0x19C
// / +0x1A0 (publishing the first, or -1 when they are equal), and for index 0x19C calls 0x089E7700 first and otherwise a
// run of 0x089E75AC / 0x089E7020 / 0x089E7520 / 0x089E7340. When +0x188 (index 0x19C) or +0x18C (index 0x1A0) is set it
// builds a temporary list on the frame and runs the object's slot 0xCC over it (two or three entries, a3 = the list).

// -------------------------------------------------------------------------------------------------------------------
// 0x088C9C08 (a0 = the row container, a1 = handed to 0x08AF26F0): for every row (word +0x28 of them) and both bands
// (+0x1F8 apart) builds 20 quads over the 24-byte records at +0xAB0 + row * 0x3F0, each quad two 0x30-byte vertices
// with the packed colour (alpha 0 on the first vertex of band 0, 204 otherwise).

// -------------------------------------------------------------------------------------------------------------------
// 0x088ACFD8 (a0 = the transform object, a1 = two floats out, a2 = a matrix, a3 = another, t0 = a third): the two-sided
// slice through the plane the object's +0x42C / +0x430 scales define. Straight line, no branches.
void func_088ACFD8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 matrix_a = ctx.gpr[6];
    const u32 matrix_b = ctx.gpr[7];
    const u32 matrix_c = ctx.gpr[8];
    GuestFrame frame(ctx, 0xF0u);
    const float bend = float_at(rt, 0x08CDD10Cu);
    constexpr float kOne = 1.0f;
    constexpr float kNegativeOne = -1.0f;
    const u32 band_c = self + 0x3F0u;
    (void)call_set(rt, ctx, 0x08B4D080u, {frame.at(0x88u), matrix_b});   // a1 = a3: the matrix it copies from
    const u32 slot_a = word(rt, self + 0x3CCu);
    (void)call_set(rt, ctx, 0x08B4AD90u, {frame.at(0x40u), matrix_a, self + 0x294u + slot_a * 40u});
    (void)call_set(rt, ctx, 0x088AB868u, {frame.at(0x50u), matrix_a, band_c});
    put_float(rt, frame.at(0x30u), float_at(rt, frame.at(0x50u)));
    put_float(rt, frame.at(0x34u), float_at(rt, frame.at(0x54u)));
    (void)call_set(rt, ctx, 0x08B4A560u, {frame.at(0x30u)}, {float_at(rt, self + 0x48u)});
    put_float(rt, frame.at(0x38u), float_at(rt, self + 0x3Cu));
    put_float(rt, frame.at(0x3Cu), float_at(rt, self + 0x40u));
    (void)call_set(rt, ctx, 0x08B4AC30u, {frame.at(0x38u), frame.at(0x38u), frame.at(0x30u)});
    const u32 slot_b = word(rt, self + 0x3D0u);
    const float edge_a = float_at(rt, frame.at(0x38u));
    const float edge_b = float_at(rt, frame.at(0x3Cu));
    (void)call_set(rt, ctx, 0x088AB868u, {frame.at(0x58u), matrix_b, matrix_c + 0x284u + slot_b * 40u});
    put_float(rt, frame.at(0x20u), float_at(rt, frame.at(0x58u)));
    put_float(rt, frame.at(0x24u), float_at(rt, frame.at(0x5Cu)));
    (void)call_set(rt, ctx, 0x08B4A560u, {frame.at(0x20u)}, {float_at(rt, matrix_c + 0x48u)});
    put_float(rt, frame.at(0x28u), float_at(rt, matrix_c + 0x3Cu));
    put_float(rt, frame.at(0x2Cu), float_at(rt, matrix_c + 0x40u));
    (void)call_set(rt, ctx, 0x08B4AC30u, {frame.at(0x28u), frame.at(0x28u), frame.at(0x20u)});
    put_float(rt, frame.at(0x48u), float_at(rt, frame.at(0x28u)));
    put_float(rt, frame.at(0x4Cu), float_at(rt, frame.at(0x2Cu)));
    const u32 slot_c = word(rt, self + 0x3CCu);
    const float scaled_a = call_set(rt, ctx, 0x08B4ABECu, {band_c, self + 0x294u + slot_c * 40u}).f0;
    const float weight_a = scaled_a * (kNegativeOne / float_at(rt, self + 0x430u));
    (void)call_set(rt, ctx, 0x088AB868u, {frame.at(0x60u), matrix_a, band_c});
    (void)call_set(rt, ctx, 0x08B4AD90u, {frame.at(0x68u), frame.at(0x88u), frame.at(0x40u)});
    const u32 slot_d = word(rt, self + 0x3D0u);
    const float scaled_b = call_set(rt, ctx, 0x08B4ABECu, {matrix_c + 0x284u + slot_d * 40u, frame.at(0x68u)}).f0;
    const float weight_b = scaled_b * (kNegativeOne / float_at(rt, matrix_c + 0x430u));
    const u32 slot_e = word(rt, self + 0x3D0u);
    (void)call_set(rt, ctx, 0x088AB868u, {frame.at(0x70u), matrix_b, matrix_c + 0x284u + slot_e * 40u});
    put_float(rt, frame.at(0x10u), float_at(rt, frame.at(0x60u)));
    put_float(rt, frame.at(0x14u), float_at(rt, frame.at(0x64u)));
    (void)call_set(rt, ctx, 0x08B4A560u, {frame.at(0x10u)}, {weight_a});
    put_float(rt, frame.at(8u), float_at(rt, frame.at(0x70u)));
    put_float(rt, frame.at(0xCu), float_at(rt, frame.at(0x74u)));
    (void)call_set(rt, ctx, 0x08B4A560u, {frame.at(8u)}, {weight_b});
    put_float(rt, frame.at(0x18u), float_at(rt, frame.at(0x10u)));
    put_float(rt, frame.at(0x1Cu), float_at(rt, frame.at(0x14u)));
    (void)call_set(rt, ctx, 0x08B4AC0Cu, {frame.at(0x18u), frame.at(0x18u), frame.at(8u)});
    const float sum = kOne / float_at(rt, self + 0x42Cu) + kOne / float_at(rt, matrix_c + 0x42Cu);
    put_float(rt, frame.at(0x7Cu), float_at(rt, frame.at(0x1Cu)));
    put_float(rt, frame.at(0x78u), float_at(rt, frame.at(0x18u)));
    const float scaled = call_set(rt, ctx, 0x08B4ABCCu, {frame.at(0x78u), frame.at(0x40u)}).f0;
    put_float(rt, frame.at(0x80u), edge_a);
    put_float(rt, frame.at(0x84u), edge_b);
    const float total = sum + scaled;
    (void)call_set(rt, ctx, 0x08B4AC30u, {frame.at(0x80u), frame.at(0x80u), frame.at(0x48u)});
    float ratio = 0.0f;
    if (!(total == 0.0f)) {
        const float along = call_set(rt, ctx, 0x08B4ABCCu, {frame.at(0x80u), frame.at(0x40u)}).f0;
        ratio = ((-(bend + kOne)) * along) / total;
    }
    put_float(rt, frame.at(0u), float_at(rt, frame.at(0x40u)));
    put_float(rt, frame.at(4u), float_at(rt, frame.at(0x44u)));
    (void)call_set(rt, ctx, 0x08B4A560u, {frame.at(0u)}, {ratio});
    put_float(rt, out + 4u, float_at(rt, frame.at(4u)));
    put_float(rt, out, float_at(rt, frame.at(0u)));
    ctx.fpr[0] = ratio;
}

// -------------------------------------------------------------------------------------------------------------------
// 0x08901DB8 / 0x089041C8: one body, the twins differ only in the byte table's offset in the object (0x98 vs 0x80) and
// the function the negative-level path hands the job to. a0 = object, a1 = passed through, a2 = the node (six floats:
// two control points at lane k and k + 3), a3 = the first point array, t0 = the second, t1 = the table index, t2 = the
// level, t3 = the node lane. Returns 1 when a child reports a hit, 0 otherwise.
s32 spine_step(Runtime &rt, AllegrexContext &ctx, u32 address, u32 table_offset, u32 delegate) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 node = ctx.gpr[6];
    const u32 out_a = ctx.gpr[7];
    const u32 out_b = ctx.gpr[8];
    const u32 index = ctx.gpr[9];
    const s32 level = static_cast<s32>(ctx.gpr[10]);
    const u32 lane = ctx.gpr[11];
    GuestFrame frame(ctx, 0x70u);
    const auto answer = [&](s32 value) {
        ctx.gpr[2] = static_cast<u32>(value);
        return value;
    };
    const auto recurse = [&](u32 child_node, u32 a3, u32 t0, u32 table_index, s32 child_level, u32 child_lane) {
        return static_cast<s32>(call_set(rt, ctx, address,
                                          {self, argument, child_node, a3, t0, table_index,
                                           static_cast<u32>(child_level), child_lane})
                                    .v0);
    };
    if (level < 0) {
        return answer(static_cast<s32>(call_set(rt, ctx, delegate,
                                                 {self, argument, node, out_b, lane, index,
                                                  static_cast<u32>(level), lane})
                                           .v0));
    }
    const u32 byte = rt.memory().load8(word(rt, self + table_offset) + index);
    const unsigned k = byte >> 6;
    const float weight = (static_cast<float>(byte & 0x3Fu) + 0.5f) * 0.015625f;
    const float height = psp_mul(float_at(rt, node + 4u * k), 1.0f - weight) +
                         psp_mul(weight, float_at(rt, node + 0xCu + 4u * k));
    const float to_a = float_at(rt, out_a + 4u * k) - height;
    const float to_b = float_at(rt, out_b + 4u * k) - height;
    const u32 child_lane = lane * 2u;
    const u32 copy = frame.at(0xCu);
    const u32 split = frame.at(0u);
    const u32 child_index = index + (1u << (static_cast<unsigned>(level) & 31u));
    if (psp_mul(to_a, to_b) < 0.0f) {
        // The segment crosses the surface: split it at the crossing point and pin the node's lane to the height.
        // The two point arrays are read before the node copy is written (the original loads them first).
        const float span = float_at(rt, out_b + 4u * k) - float_at(rt, out_a + 4u * k);
        const float along = psp_mul(height - float_at(rt, out_a + 4u * k), 1.0f / span);
        const float rest = psp_mul(to_b, 1.0f / span);
        const float point_a0 = float_at(rt, out_a);
        const float point_a1 = float_at(rt, out_a + 4u);
        const float point_a2 = float_at(rt, out_a + 8u);
        const float point_b0 = float_at(rt, out_b);
        const float point_b1 = float_at(rt, out_b + 4u);
        const float point_b2 = float_at(rt, out_b + 8u);
        for (u32 i = 0; i < 6u; ++i) put_float(rt, copy + 4u * i, float_at(rt, node + 4u * i));
        put_float(rt, split + 8u, psp_mul(rest, point_a2) + psp_mul(along, point_b2));
        put_float(rt, split, psp_mul(rest, point_a0) + psp_mul(along, point_b0));
        put_float(rt, split + 4u, psp_mul(rest, point_a1) + psp_mul(along, point_b1));
        put_float(rt, split + 4u * k, height);  // the asm's `swc1 f20, 0(s6)` with s6 = sp + 4k: the split's own lane, exact
        if (to_a < 0.0f) {
            put_float(rt, copy + 0xCu + 4u * k, height);
            if (recurse(copy, out_a, split, index + 1u, level - 1, child_lane) != 0) return answer(1);
            put_float(rt, copy + 0xCu + 4u * k, float_at(rt, node + 0xCu + 4u * k));
            put_float(rt, copy + 4u * k, height);
            if (recurse(copy, split, out_b, child_index, level - 1, child_lane + 1u) != 0) return answer(1);
            return answer(0);
        }
        put_float(rt, copy + 4u * k, height);
        if (recurse(copy, out_a, split, child_index, level - 1, child_lane + 1u) != 0) return answer(1);
        put_float(rt, copy + 0xCu + 4u * k, height);
        put_float(rt, copy + 4u * k, float_at(rt, node + 4u * k));
        if (recurse(copy, split, out_b, index + 1u, level - 1, child_lane) != 0) return answer(1);
        return answer(0);
    }
    // No crossing: one child over a copy of the node with one lane pinned to the height.
    for (u32 i = 0; i < 6u; ++i) put_float(rt, copy + 4u * i, float_at(rt, node + 4u * i));
    if (to_a < 0.0f) {
        put_float(rt, copy + 0xCu + 4u * k, height);
        if (recurse(copy, out_a, out_b, index + 1u, level - 1, child_lane) != 0) return answer(1);
    } else {
        put_float(rt, copy + 4u * k, height);
        if (recurse(copy, out_a, out_b, child_index, level - 1, child_lane + 1u) != 0) return answer(1);
    }
    return answer(0);
}

// -------------------------------------------------------------------------------------------------------------------
// 0x08B14188 (a0 = the reader object): a UMD read loop. Takes the handle at 0x08CB4BA0, seeks to +0x254, then reads
// +0x20000-byte chunks through 0x08B33D3C while the 64-bit byte counter lasts, advancing the offset at +0x274 and the
// state at +0x278 (0 = running, 1 = +0x27C asked to stop, 2 = a step failed). The imports are the unresolved
// `scesupPreAcc` stubs at 0x08C7B208 / 0x08C7B210 / 0x08C7B218 / 0x08C7B228 / 0x08C7B230 / 0x08C7B238.

} // namespace
} // namespace new_batch_main_242
} // namespace gt::game

#define NEW242(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_242::name, gt::ReturnKind::kind)
NEW242(0x088ACFD8u, func_088ACFD8, Float);

// ---- from main-session batch 243 (draft 0ccc3211a6ec2c4de5d5a4b75d80f2c6ff299e29) ----
// New batch main 243 (main session): functions called by code already in src/ but never written by hand,
// so no route could compare them with useful inputs. Written as structured C from `tools/disasm`; this is
// a draft batch, unverified until the differential gate runs it.
//
// Skipped: 0x08C677D4 (the printf-style formatter: it lowers `sp` by a run-time amount for the argument
// buffer and its return path hands the stack pointer back through $fp, so its frame cannot be written as
// a plain GuestFrame without modelling that alloca; left for a dedicated pass).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_243 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0 / v1 / f0 as the callee left them (a callee that never sets v0 hands the
// previous one on to the code after it).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments ($f12-f$f15).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A guest call with the argument registers set like the original (a0-a3, t0-t3) and the float registers
// $f12-$f15; v0 / v1 / f0 stay as the callee left them.
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keepf(rt, ctx, target, arguments, float_arguments);
}

u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
u32 load16(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// The compiler's virtual calls; the register that holds the function pointer differs between them and a
// callee that never writes v0 hands the caller whatever the original left there.
GuestResult call_slot_fn(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments);
}
GuestResult call_slot_table(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[2] = table;  // `lw v0, 0(object); lw v1, off(v0); jalr v1`
    ctx.gpr[3] = function;
    return call_keep(rt, ctx, function, arguments);
}

// --- 0x08B7394C ---
// A command parser: memset the 0x9C8-byte output, take the record 08B7721C(index - 1, self + 0x4D8), walk
// its text with the varint reader 08B727A4 / 08B727D8, run the L / R / P command loop (08B72704 resolves a
// command byte, 08B72830 applies it), then the final 08B73410 pass with a mode-dependent cursor from
// self (+4, +8, +0x10) or record + 8. Returns 0, or 3 / 5 on the early outs.
void func_08B7394C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    call(rt, ctx, 0x08B36018u, {out, 0u, 0x9C8u});  // memset(out, 0, 0x9C8)
    const u32 index = word(rt, self + 0x4D0u);
    put(rt, self + 0x4E4u, 0u);
    put(rt, self + 0x4D4u, 0u);  // the beqz's delay slot runs on both paths
    if (index == 0u) {
        ctx.gpr[2] = 5u;
        return;
    }
    const u32 record = call(rt, ctx, 0x08B7721C, {index - 1u, self + 0x4D8u});
    if (record == 0u) {
        ctx.gpr[2] = 5u;
        return;
    }
    const u32 base = (record + 4u) - word(rt, record + 4u);  // s5
    put(rt, out + 0x9ACu, word(rt, self + 0x4E0u));
    u32 cursor = base + 9u;
    u32 scan = cursor + call(rt, ctx, 0x08B35F60u, {cursor});  // strlen(cursor)
    if (byte_u(rt, base + 9u) == 0x65u && byte_u(rt, cursor + 1u) == 0x68u) {  // "eh": a word follows the text
        put(rt, out + 0x9C4u, word(rt, scan + 1u));
        cursor = base + 0xBu;
        scan += 5u;
    } else {
        scan += 1u;
    }
    // Two varints after the text: one at out+0x9B8, then the 0x9B4 one.
    const u32 after_first = call(rt, ctx, 0x08B727A4, {scan, out + 0x9B8u});
    const u32 second = call(rt, ctx, 0x08B727D8, {after_first, out + 0x9B4u});
    u32 s1;
    u32 s7 = 0u;
    if (byte_u(rt, base + 8u) == 1u) {
        // The word at out+0x9BC is the byte 08B727D8 points at; s1 walks that block.
        put(rt, out + 0x9BCu, byte_u(rt, second));
        put8(rt, out + 0x9C1u, 0xFFu);
        s1 = second + 1u;
    } else {
        s1 = call(rt, ctx, 0x08B727A4, {second, out + 0x9BCu});
        put8(rt, out + 0x9C1u, 0xFFu);
    }
    // The command loop: 'z' (first character only) reads a varint and sets s7; L / R store one byte; P
    // resolves and applies a command; anything else ends it with s1 = s7.
    u32 ch = byte_u(rt, cursor);
    bool allow_z = true;
    for (;;) {
        if (allow_z && ch == 0x7Au) {  // 'z'
            s1 = call(rt, ctx, 0x08B727A4, {s1, frame.at(0u)});
            const u32 amount = word(rt, frame.at(0u));
            put8(rt, out + 0x9C2u, 1u);
            cursor += 1u;
            ch = byte_u(rt, cursor);
            s7 = s1 + amount;
            allow_z = false;
            continue;
        }
        allow_z = false;
        if (ch == 0u) break;
        if (ch == 0x4Cu) {  // 'L'
            put8(rt, out + 0x9C1u, byte_u(rt, s1));
            cursor += 1u;
            s1 += 1u;
            ch = byte_u(rt, cursor);
            continue;
        }
        if (ch == 0x52u) {  // 'R'
            put8(rt, out + 0x9C0u, byte_u(rt, s1));
            cursor += 1u;
            s1 += 1u;
            ch = byte_u(rt, cursor);
            continue;
        }
        if (ch != 0x50u) {  // anything but 'P' ends the loop with s1 = s7
            s1 = s7;
            break;
        }
        const u32 code = byte_u(rt, s1);
        cursor += 1u;
        const u32 handler = call(rt, ctx, 0x08B72704, {code});
        s1 = call(rt, ctx, 0x08B72830, {code, handler, s1 + 1u, out + 0x9B0u});
        ch = byte_u(rt, cursor);
        if (ch == 0u) break;
    }
    if (s7 == 0u) {
        ctx.gpr[2] = 3u;
        return;
    }
    s1 = s7;
    // 08B73410 picks the record up; then the mode byte at out+0x9C0 chooses the cursor base and the
    // length of the block that follows.
    call(rt, ctx, 0x08B73410, {s1, word(rt, base) + base + 4u, self, out});
    const u32 mode = byte_u(rt, out + 0x9C0u);
    u32 size;
    u32 cursor_base;
    if (mode == 0xFFu) {
        size = 0u;
        cursor_base = record + 8u;
    } else if ((mode & 7u) == 2u) {
        size = 4u;
        cursor_base = self;
    } else if ((mode & 7u) == 0u || (mode & 7u) == 3u) {
        size = 8u;
        cursor_base = self;
    } else if ((mode & 7u) == 4u) {
        size = 0x10u;
        cursor_base = self;
    } else {
        // 0x08B77B04 traps (it calls the `break` stub 08B35530 and has no return): the original never
        // reaches the code the compiler laid out after it.
        call(rt, ctx, 0x08B77B04u);
        return;
    }
    u32 s1b = cursor_base + size;
    u32 s2b = 0u;
    if (byte_u(rt, out + 0x9C2u) != 0u) {
        s1b = call(rt, ctx, 0x08B727A4, {s1b, frame.at(0u)});
        s2b = s1b + word(rt, frame.at(0u));
    }
    const u32 selector = byte_u(rt, out + 0x9C1u);
    if (selector != 0xFFu) {
        const u32 handler = call(rt, ctx, 0x08B72704, {selector, self});
        s1b = call(rt, ctx, 0x08B72830, {selector, handler, s1b, self + 0x4D4u});
    }
    call(rt, ctx, 0x08B73410, {s2b != 0u ? s2b : s1b, word(rt, record) + record + 4u, self, out});
    ctx.gpr[2] = 0u;
}

// --- 0x08BED390 ---
// vector<T>::insert at a position; T is 0x30 bytes (word 0 = vtable, words +4 / +8, floats +0xC..+0x2C).
// When the vector is full it grows (0x08B35840 allocates, 0x08BE64BC moves the two ranges, 0x08BE58F0
// destroys the old ones); otherwise the last element is copied into the free slot, the tail is shifted up
// one slot (fields +4..+0x2C, the destination's vtable word left alone), the stack temporary built from
// a2 is copied into *position and destroyed by 0x089D6684.
void func_08BED390(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    u32 end = word(rt, self + 4u);
    if (end == word(rt, self + 8u)) {
        // Grow: count = (end - begin) / 0x30 through the compiler's multiply; the doubled count is
        // clamped by the asm's overflow test (which leaves 0xFFFFFFF0 as the byte count).
        const s32 current = static_cast<s32>(
            (static_cast<u32>(static_cast<s32>(end - word(rt, self)) >> 4)) * 0xAAAAAAABu);
        s32 count = current;
        if (count == 0x05555555) count = static_cast<s32>(call(rt, ctx, 0x08B5ADDCu, {0x08CA2DA8u}));
        u32 bytes;
        if (count == 0) {
            bytes = 0x30u;  // a0 = 1, a0 * 0x30
        } else {
            const u32 grown = static_cast<u32>(count) << 1;
            if (grown < static_cast<u32>(count)) {
                bytes = 0xFFFFFFF0u;
            } else {
                bytes = (grown << 6) - (grown << 4);
            }
        }
        const u32 block = call(rt, ctx, 0x08B35840u, {0x10u, bytes});
        const u32 first = call(rt, ctx, 0x08BE64BCu, {word(rt, self), position, block, frame.at(2u)});
        if (first != 0u) {
            const u32 w4 = word(rt, value + 4u);
            const u32 w8 = word(rt, value + 8u);
            const float f0 = float_at(rt, value + 0xCu);
            const float f1 = float_at(rt, value + 0x10u);
            const float f2 = float_at(rt, value + 0x14u);
            const float f3 = float_at(rt, value + 0x18u);
            const float f4 = float_at(rt, value + 0x1Cu);
            const float f5 = float_at(rt, value + 0x20u);
            const float f6 = float_at(rt, value + 0x24u);
            const float f7 = float_at(rt, value + 0x28u);
            const float f8 = float_at(rt, value + 0x2Cu);
            put(rt, first + 4u, w4);
            put(rt, first + 8u, w8);
            put_float(rt, first + 0xCu, f0);
            put_float(rt, first + 0x10u, f1);
            put_float(rt, first + 0x14u, f2);
            put_float(rt, first + 0x18u, f3);
            put_float(rt, first + 0x1Cu, f4);
            put(rt, first, 0x08CCDE00u);
            put_float(rt, first + 0x20u, f5);
            put_float(rt, first + 0x24u, f6);
            put_float(rt, first + 0x28u, f7);
            put_float(rt, first + 0x2Cu, f8);
        }
        const u32 old_end = word(rt, self + 4u);
        const u32 second = call(rt, ctx, 0x08BE64BCu, {position, old_end, first + 0x30u, frame.at(1u)});
        call(rt, ctx, 0x08BE58F0u, {word(rt, self), word(rt, self + 4u), frame.at(0u)});
        const u32 old_begin = word(rt, self);
        if (old_begin != 0u) call(rt, ctx, 0x08B35254u, {old_begin});
        put(rt, self + 8u, block + bytes);
        put(rt, self + 4u, second);
        put(rt, self, block);
        ctx.gpr[2] = block + bytes;
        return;
    }
    if (end != 0u) {
        // Copy the last element into the new slot [end].
        const u32 last = end - 0x30u;
        const u32 w4 = word(rt, last + 4u);
        const u32 w8 = word(rt, last + 8u);
        const float f0 = float_at(rt, last + 0xCu);
        const float f1 = float_at(rt, last + 0x10u);
        const float f2 = float_at(rt, last + 0x14u);
        const float f3 = float_at(rt, last + 0x18u);
        const float f4 = float_at(rt, last + 0x1Cu);
        const float f5 = float_at(rt, last + 0x20u);
        const float f6 = float_at(rt, last + 0x24u);
        const float f7 = float_at(rt, last + 0x28u);
        const float f8 = float_at(rt, last + 0x2Cu);
        put(rt, end + 4u, w4);
        put(rt, end + 8u, w8);
        put_float(rt, end + 0xCu, f0);
        put_float(rt, end + 0x10u, f1);
        put_float(rt, end + 0x14u, f2);
        put_float(rt, end + 0x18u, f3);
        put_float(rt, end + 0x1Cu, f4);
        put(rt, end, 0x08CCDE00u);
        put_float(rt, end + 0x20u, f5);
        put_float(rt, end + 0x24u, f6);
        put_float(rt, end + 0x28u, f7);
        put_float(rt, end + 0x2Cu, f8);
        end = word(rt, self + 4u);
    }
    // Number of elements to shift: ((end - 0x30) - position) / 0x30, the low word of the product (the
    // byte distance is a multiple of 0x30 and the compiler uses the multiply's low half).
    const s32 shift_count = static_cast<s32>(
        (static_cast<u32>(static_cast<s32>((end - 0x30u) - position) >> 4)) * 0xAAAAAAABu);
    put(rt, self + 4u, end + 0x30u);
    // The temporary element at frame+4, built from a2 (vtable word included).
    const u32 vw4 = word(rt, value + 4u);
    const u32 vw8 = word(rt, value + 8u);
    const float vf0 = float_at(rt, value + 0xCu);
    const float vf1 = float_at(rt, value + 0x10u);
    const float vf2 = float_at(rt, value + 0x14u);
    const float vf3 = float_at(rt, value + 0x18u);
    const float vf4 = float_at(rt, value + 0x1Cu);
    const float vf5 = float_at(rt, value + 0x20u);
    const float vf6 = float_at(rt, value + 0x24u);
    const float vf7 = float_at(rt, value + 0x28u);
    const float vf8 = float_at(rt, value + 0x2Cu);
    put(rt, frame.at(4u), 0x08CCDE00u);
    put(rt, frame.at(8u), vw4);
    put(rt, frame.at(0xCu), vw8);
    put_float(rt, frame.at(0x10u), vf0);
    put_float(rt, frame.at(0x14u), vf1);
    put_float(rt, frame.at(0x18u), vf2);
    put_float(rt, frame.at(0x1Cu), vf3);
    put_float(rt, frame.at(0x20u), vf4);
    put_float(rt, frame.at(0x24u), vf5);
    put_float(rt, frame.at(0x28u), vf6);
    put_float(rt, frame.at(0x2Cu), vf7);
    put_float(rt, frame.at(0x30u), vf8);
    if (shift_count > 0) {
        u32 src = end - 4u;
        u32 dst = end + 0x2Cu;
        s32 moved = 0;
        do {
            src -= 0x30u;
            dst -= 0x30u;
            const u32 s = src - 0x2Cu;
            const u32 d = dst - 0x2Cu;
            const u32 w4 = word(rt, s + 4u);
            const u32 w8 = word(rt, s + 8u);
            const float f0 = float_at(rt, s + 0xCu);
            const float f1 = float_at(rt, s + 0x10u);
            const float f2 = float_at(rt, s + 0x14u);
            const float f3 = float_at(rt, s + 0x18u);
            const float f4 = float_at(rt, s + 0x1Cu);
            const float f5 = float_at(rt, s + 0x20u);
            const float f6 = float_at(rt, s + 0x24u);
            const float f7 = float_at(rt, s + 0x28u);
            const float f8 = float_at(rt, s + 0x2Cu);
            put(rt, d + 4u, w4);
            put(rt, d + 8u, w8);
            put_float(rt, d + 0xCu, f0);
            put_float(rt, d + 0x10u, f1);
            put_float(rt, d + 0x14u, f2);
            put_float(rt, d + 0x18u, f3);
            put_float(rt, d + 0x1Cu, f4);
            put_float(rt, d + 0x20u, f5);
            put_float(rt, d + 0x24u, f6);
            put_float(rt, d + 0x28u, f7);
            put_float(rt, d + 0x2Cu, f8);
            ++moved;
        } while (moved != shift_count);
    }
    // Copy the temporary into *position (the vtable word is left alone) and destroy the temporary.
    put(rt, position + 4u, word(rt, frame.at(8u)));
    put(rt, position + 8u, word(rt, frame.at(0xCu)));
    put_float(rt, position + 0xCu, float_at(rt, frame.at(0x10u)));
    put_float(rt, position + 0x10u, float_at(rt, frame.at(0x14u)));
    put_float(rt, position + 0x14u, float_at(rt, frame.at(0x18u)));
    put_float(rt, position + 0x18u, float_at(rt, frame.at(0x1Cu)));
    put_float(rt, position + 0x1Cu, float_at(rt, frame.at(0x20u)));
    put_float(rt, position + 0x20u, float_at(rt, frame.at(0x24u)));
    put_float(rt, position + 0x24u, float_at(rt, frame.at(0x28u)));
    put_float(rt, position + 0x28u, float_at(rt, frame.at(0x2Cu)));
    put_float(rt, position + 0x2Cu, float_at(rt, frame.at(0x30u)));
    put(rt, frame.at(4u), 0x08CCDE00u);
    ctx.gpr[2] = call(rt, ctx, 0x089D6684u, {frame.at(4u)});
}

// --- 0x089AC2A8 ---
// Draws a quad or a six-vertex fan: the strip kind comes from 089FA8A0(object); with 4 the six points
// (midpoint + the four corners and the closing corner) go through 089A900C / 089A9064 and the virtual at
// slot 0x10C gets a1 = 5, a2 = 6; otherwise the four corners are built, the virtual at 0x108 runs and the
// virtual at 0x10C gets a1 = 4, a2 = 4. Only points normalized by (f12, f13), which must both be > 0.
void func_089AC2A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 points = ctx.gpr[5];
    const u32 object = ctx.gpr[6];
    const u32 arrays = ctx.gpr[7];
    const float width = ctx.fpr[12];
    const float height = ctx.fpr[13];
    if (width <= 0.0f) return;
    if (height <= 0.0f) return;
    GuestFrame frame(ctx, 0x140u);
    const s32 kind = static_cast<s32>(call(rt, ctx, 0x089FA8A0u, {object}));
    if (kind == 4) {
        const u32 a = call(rt, ctx, 0x089F9B54u, {object, 0u});
        call(rt, ctx, 0x0896418Cu, {frame.at(0x60u), a});
        const u32 b = call(rt, ctx, 0x089F9B54u, {object, 1u});
        call(rt, ctx, 0x0896418Cu, {frame.at(0x70u), b});
        const u32 c = call(rt, ctx, 0x089F9B54u, {object, 2u});
        call(rt, ctx, 0x0896418Cu, {frame.at(0x80u), c});
        const u32 d = call(rt, ctx, 0x089F9B54u, {object, 3u});
        call(rt, ctx, 0x0896418Cu, {frame.at(0x90u), d});
        const float x = float_at(rt, points);
        const float y = float_at(rt, points + 4u);
        const float x2 = float_at(rt, points + 8u);
        const float y2 = float_at(rt, points + 0xCu);
        const float dx = x2 - x;
        const float dy = y2 - y;
        put_float(rt, frame.at(0u), x + dx * 0.5f);
        put_float(rt, frame.at(4u), y + dy * 0.5f);
        put_float(rt, frame.at(8u), x);
        put_float(rt, frame.at(0xCu), y);
        put_float(rt, frame.at(0x10u), x2);
        put_float(rt, frame.at(0x14u), y);
        put_float(rt, frame.at(0x18u), x2);
        put_float(rt, frame.at(0x1Cu), y2);
        put_float(rt, frame.at(0x20u), x);
        put_float(rt, frame.at(0x24u), y2);
        put_float(rt, frame.at(0x28u), x);
        put_float(rt, frame.at(0x2Cu), y);
        for (u32 i = 0u; i < 6u; ++i) {
            const u32 block = frame.at(0xA0u + 0x10u * i);
            put_float(rt, block, 1.0f);
            put_float(rt, block + 4u, 1.0f);
            put_float(rt, block + 8u, 1.0f);
            put_float(rt, block + 0xCu, 1.0f);
        }
        if (arrays != 0u) {
            for (u32 i = 0u; i < 6u; ++i) {
                const float px = float_at(rt, frame.at(8u * i));
                const float py = float_at(rt, frame.at(8u * i + 4u));
                call_setf(rt, ctx, 0x089A900Cu, {frame.at(0x30u + 8u * i), arrays}, {px / width, py / height});
            }
        }
        for (u32 i = 0u; i < 6u; ++i) {
            if (0.0f < dx && 0.0f < dy) {
                const float px = float_at(rt, frame.at(8u * i));
                const float py = float_at(rt, frame.at(8u * i + 4u));
                call_setf(rt, ctx, 0x089A9064u, {frame.at(0xA0u + 0x10u * i), frame.at(0x60u)},
                          {(px - x) / dx, (py - y) / dy});
            }
        }
        const u32 table = word(rt, self);
        const u32 slot = word(rt, table + 0x10Cu);
        ctx.gpr[3] = table;
        ctx.gpr[2] = 0x00490000u;  // the `lui v0, 0x49` the asm leaves at the call
        call_keep(rt, ctx, slot, {self, 5u, 6u, frame.at(0u), arrays != 0u ? frame.at(0x30u) : 0u, frame.at(0xA0u)});
        return;
    }
    const float x = float_at(rt, points);
    const float y = float_at(rt, points + 4u);
    const float x2 = float_at(rt, points + 8u);
    const float y2 = float_at(rt, points + 0xCu);
    put_float(rt, frame.at(0x30u), x);
    put_float(rt, frame.at(0x34u), y);
    put_float(rt, frame.at(0x38u), x2);
    put_float(rt, frame.at(0x3Cu), y);
    put_float(rt, frame.at(0x40u), x);
    put_float(rt, frame.at(0x44u), y2);
    put_float(rt, frame.at(0x48u), x2);
    put_float(rt, frame.at(0x4Cu), y2);
    if (arrays != 0u) {
        for (u32 i = 0u; i < 4u; ++i) {
            const float px = float_at(rt, frame.at(0x30u + 8u * i));
            const float py = float_at(rt, frame.at(0x34u + 8u * i));
            call_setf(rt, ctx, 0x089A900Cu, {frame.at(8u * i), arrays}, {px / width, py / height});
        }
    }
    const u32 slot108 = word(rt, word(rt, self) + 0x108u);
    const u32 first = call(rt, ctx, 0x089F9B54u, {object, 0u});
    ctx.gpr[2] = first;  // v0 at the jalr is 089F9B54's result, as the asm leaves it
    call_keep(rt, ctx, slot108, {self, first});
    const u32 table = word(rt, self);
    const u32 slot10c = word(rt, table + 0x10Cu);
    ctx.gpr[3] = table;
    ctx.gpr[2] = 0x00490000u;
    call_keep(rt, ctx, slot10c, {self, 4u, 4u, frame.at(0x30u), arrays != 0u ? frame.at(0u) : 0u, 0u});
}

// --- 0x08AC8110 ---
// The stream driver's run loop: set the reader up (08B0C99C / 08B0C650), start the two callbacks
// (08AC79B0 over 08B2E314/08B2E134), then pump 08AC7508 / 08AC7488 / 08AC7384 until 0x566 clears or the
// 0x564 flag ends it. The teardown releases 0x570, 0x5BC and the stream; the final block releases 0x574
// and 0x598 and sets 0x569.

// --- 0x088C2A9C ---
// The per-frame state machine of a track/camera object: self+4 holds two words and self+0x24 the state
// object (its +2 halfword is the flag word, +0xA5 the state). The state picks one of the ten handlers,
// which run the object's virtuals (slots 0x14 / 0x48 / 0x4C) and the 088BF978 callback; everything ends
// by clearing flag bits 4 and 6, storing the state back and returning the new flags.

} // namespace
} // namespace new_batch_main_243
} // namespace gt::game

#define MAIN243(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_243::name, gt::ReturnKind::kind)
MAIN243(0x08B7394Cu, func_08B7394C, Word);
MAIN243(0x08BED390u, func_08BED390, Word);
MAIN243(0x089AC2A8u, func_089AC2A8, Word);

// ---- from main-session batch 244 (draft f9f2e9d77205a2bb7970508fcce05791c97974b2) ----
// New batch 244 (main session): functions the recompiler emitted that no src/ file defines or registers,
// written by hand from `tools/disasm` as structured C. A draft batch: nothing here is verified yet, and the
// input shapes its synthetic cases need were written next to the batch (one line per address), not committed.
//
// The helpers below cover the shapes this batch repeats: a guest call that leaves v0/v1/f0 as the callee left
// them (several paths test the *integer* result of a float helper), the virtual call the compiler writes, and
// the "unexpected null" check (a null jumps over a call to func_08A5BB7C with the message, then the code goes on
// with the null).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_244 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 / v1 / f0 hold what the callee returned, as after the `jal`.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with the argument registers set like the original sets them before the `jal` (a0-a3, t0-t3 in order)
// and the float arguments in f12-f15: what the callee is not given stays as the caller had it.
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                     std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float value : floats) {
        if (freg <= 15u) ctx.fpr[freg++] = value;
    }
    return call_keep(rt, ctx, target, arguments, floats);
}
// A virtual call as the compiler writes it: `lw v1, 0(object); lw v0, slot(v1)` and `jalr v0` with a0 = object
// unless the caller passed another set (the original's v1 / v0 hold the vtable and the target at the call).
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments,
                         std::initializer_list<float> floats = {}) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments, floats);
}
// The null check the original compiles as "branch over a call to the null-message printer"; the code then goes on
// with the null (the printer throws in the release build, this is the debug shape).
u32 checked(Runtime &rt, AllegrexContext &ctx, u32 pointer, u32 message) {
    if (pointer == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
    return pointer;
}
// Destructor of the value-object library the batch uses: func_08A5B9EC(object).
constexpr u32 kDestroy = 0x08A5B9ECu;

// The element count of a begin/end pair as the asm computes it: `subu`, `sra 2`, then compared unsigned.
u32 element_count(u32 begin, u32 end) { return static_cast<u32>(static_cast<s32>(end - begin) >> 2); }

// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the empty
// rep is 0x08D74FD0, the count 4 bytes before the data): unless the rep is the empty one, func_08B60804 steps
// the count by -1 and func_08B8542C(rep, scratch) frees it when the old count was not positive.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 scratch) {
    const u32 text = word(rt, slot);
    const u32 rep = text - 0xCu;
    if (rep == 0x08D74FD0u) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}).v0) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {rep, scratch});
}

// f20, f21 and f22 are callee-saved: the original restores them from its frame on every exit.
struct SavedFloats {
    AllegrexContext &ctx;
    float f20, f21, f22;
    explicit SavedFloats(AllegrexContext &c) : ctx(c), f20(c.fpr[20]), f21(c.fpr[21]), f22(c.fpr[22]) {}
    ~SavedFloats() {
        ctx.fpr[20] = f20;
        ctx.fpr[21] = f21;
        ctx.fpr[22] = f22;
    }
};

// --- batch ---

// 0x08902884 (out = a0, object = a1, source = a2): samples the object's circular track of 40-byte points. The
// point index comes from func_08900F94(object, func_0890103C(object, source->+4)), the two angles
// source->+0 and point->+0x24 drive two func_089010A0 calls whose ratio f20 = first / second scales the
// tangents; the point's +0xC..+0x14 (or +0x18..+0x20 on the non-negative side of source->+0) is combined with
// the next point's into a direction, normalized, scaled by |source->+0|, offset by source->+0 and lifted by 3;
// out is written and returned, and func_08902554(object, sp + 0x90, out) may replace out->+4 with sp + 0x94.
void func_08902884(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    const SavedFloats saved(ctx);
    GuestFrame frame(ctx, 0x100u);

    const float ratio_base = call_set(rt, ctx, 0x0890103Cu, {object}, {float_at(rt, source + 4u)}).f0;
    const u32 slot = call_set(rt, ctx, 0x08900F94u, {object}, {ratio_base}).v0;

    const u32 count = word(rt, object + 0x54u);
    const u32 elements = word(rt, object + 0x58u);
    const u32 current = elements + slot * 40u;
    const u32 next = (slot + 1u == count) ? elements : elements + (slot + 1u) * 40u;

    const float angle = float_at(rt, current + 0x24u);
    const float first = call_set(rt, ctx, 0x089010A0u, {object, 0u}, {ratio_base, angle}).f0;
    const float second = call_set(rt, ctx, 0x089010A0u, {object, 0u}, {float_at(rt, next + 0x24u), angle}).f0;
    const float ratio = first / second;

    // The current and the next point's vectors; the next one is also kept at sp + 0x6C for the last add.
    const u32 vec_current = frame.at(0x78u);
    put_float(rt, vec_current, float_at(rt, current + 0xCu));
    put_float(rt, vec_current + 4u, float_at(rt, current + 0x10u));
    put_float(rt, vec_current + 8u, float_at(rt, current + 0x14u));
    const u32 vec_next = frame.at(0x60u);
    put_float(rt, vec_next, float_at(rt, next + 0xCu));
    put_float(rt, vec_next + 4u, float_at(rt, next + 0x10u));
    put_float(rt, vec_next + 8u, float_at(rt, next + 0x14u));
    call_set(rt, ctx, 0x08B4AA68u, {vec_next, vec_next, vec_current});

    // The difference goes to sp + 0x48 and to sp + 0x6C, is scaled by the ratio and offset by the current point.
    const u32 difference = frame.at(0x48u);
    const u32 difference_copy = frame.at(0x6Cu);
    for (u32 offset = 0u; offset < 12u; offset += 4u) {
        const float value = float_at(rt, vec_next + offset);
        put_float(rt, difference + offset, value);
        put_float(rt, difference_copy + offset, value);
    }
    call_set(rt, ctx, 0x08B4A5E8u, {difference}, {ratio});
    const u32 sum = frame.at(0x54u);
    for (u32 offset = 0u; offset < 12u; offset += 4u) put_float(rt, sum + offset, float_at(rt, difference + offset));
    call_set(rt, ctx, 0x08B4AA34u, {sum, sum, vec_current});
    for (u32 offset = 0u; offset < 12u; offset += 4u) put_float(rt, difference_copy + offset, float_at(rt, sum + offset));

    // source->+0 picks the point's tangent (+0x18..+0x20) or its position (+0x0..+0x8); the next point's copy of
    // the same triple goes to sp + 0x3C.
    const u32 line = frame.at(0x3Cu);
    if (0.0f <= float_at(rt, source)) {
        for (u32 offset = 0u; offset < 12u; offset += 4u) {
            put_float(rt, vec_current + offset, float_at(rt, current + 0x18u + offset));
            put_float(rt, line + offset, float_at(rt, next + 0x18u + offset));
        }
    } else {
        for (u32 offset = 0u; offset < 12u; offset += 4u) {
            put_float(rt, vec_current + offset, float_at(rt, current + offset));
            put_float(rt, line + offset, float_at(rt, next + offset));
        }
    }
    call_set(rt, ctx, 0x08B4AA68u, {line, line, vec_current});

    const u32 offset_scaled = frame.at(0x24u);
    for (u32 offset = 0u; offset < 12u; offset += 4u) put_float(rt, offset_scaled + offset, float_at(rt, line + offset));
    call_set(rt, ctx, 0x08B4A5E8u, {offset_scaled}, {ratio});
    const u32 placed = frame.at(0x30u);
    for (u32 offset = 0u; offset < 12u; offset += 4u) put_float(rt, placed + offset, float_at(rt, offset_scaled + offset));
    call_set(rt, ctx, 0x08B4AA34u, {placed, placed, vec_current});

    // distance from the scaled difference: normalize and scale by |source->+0|.
    const u32 relative = frame.at(0x18u);
    for (u32 offset = 0u; offset < 12u; offset += 4u) put_float(rt, relative + offset, float_at(rt, placed + offset));
    call_set(rt, ctx, 0x08B4AA68u, {relative, relative, difference_copy});
    const u32 direction = frame.at(0x84u);
    for (u32 offset = 0u; offset < 12u; offset += 4u) put_float(rt, direction + offset, float_at(rt, relative + offset));
    const float squared = call_set(rt, ctx, 0x08B4A720u, {direction}).f0;
    call_set(rt, ctx, 0x08B4A5E8u, {direction}, {1.0f / std::sqrt(squared)});

    const u32 final_vector = frame.at(0xCu);
    for (u32 offset = 0u; offset < 12u; offset += 4u) put_float(rt, final_vector + offset, float_at(rt, direction + offset));
    call_set(rt, ctx, 0x08B4A5E8u, {final_vector}, {std::fabs(float_at(rt, source))});

    const u32 base = frame.at(0u);
    for (u32 offset = 0u; offset < 12u; offset += 4u) {
        const float value = float_at(rt, final_vector + offset);
        put_float(rt, base + offset, value);
        put_float(rt, out + offset, value);
    }
    call_set(rt, ctx, 0x08B4AA34u, {base, base, difference_copy});
    const float lifted = float_at(rt, base + 4u) + 3.0f;
    put_float(rt, out, float_at(rt, base));
    put_float(rt, out + 4u, lifted);
    put_float(rt, out + 8u, float_at(rt, base + 8u));
    for (u32 offset = 0u; offset < 12u; offset += 4u) put_float(rt, base + offset, float_at(rt, out + offset));

    put(rt, frame.at(0xC0u), 0u);
    put(rt, frame.at(0xC4u), 0xFFFFFFFFu);
    if (call_set(rt, ctx, 0x08902554u, {object, frame.at(0x90u), out}).v0 != 0u)
        put_float(rt, out + 4u, float_at(rt, frame.at(0x94u)));
    ctx.gpr[2] = out;
}

// 0x0891E5BC (out = a0, target = a1, pose = a2, index = a3): with two objects looked up at `index` and the next
// stride in the pose's list, builds a matrix from their virtual +0x38 / +0x3C vectors, turns it with the +0x20..
// +0x28 floats, prints it with a 4x4 frame at sp + 0x80 and classifies target->+0x218 into target->+0x224 (1/0/3/2
// above 5, above 0, below -5, below 0), then scales by min(target->+0x218 / 30, 1) (unused) and hands the
// state's 0x48-byte table row (0x08CF213C + state * 72) to func_0891E2A8(out, target, row). Returns out; a
// missing object stores 0 first and still reaches the table.

// 0x08A54B50 (out = a0, source = a1, context = a2): builds a value object into `out` (returned). With
// source->+0x58 set: an offset s1 = context->+0x1C (or the +0xC of context->+0x18 when that is zero), the owner
// context->+0x14's virtual slot 0x120 gives a length and func_08A950D0(sp + 0x20, s1, length, 16) a record whose
// slots 0xDC / 0xD8 give two numbers; a 24-byte block (func_08A5BC20) is filled by func_08A56018, wrapped
// (func_08A5BAB8, func_08A52AA8) and func_08A57B54(sp + 0x10, sp + 0x18, sp + 0x1C, first) builds the value,
// taken by func_08A56974 and assigned to out (func_08A76ABC). With the byte clear: for every node of source's
// circular list (from source->+0x44 to the sentinel source + 0x3C, step func_08B5F16C) a 24-byte object
// (func_08A76ACC, vtable 0x08CB6B88, func_08A6972C copying node->+0x10) joins out's value list through
// func_08A57B54 / virtual slots 0xF0 and 0xA0. Temporaries are destroyed on the way out.

// 0x08AB73CC (self = a0, writer = a1): a debug dump of `self`. With self->+0x10 set: func_08AB7054(sp + 0x14, self)
// and func_08AB7004(sp + 0x10, self) produce two strings, printed through func_08A767EC(writer, 0x08CB42B0, sp + 0x14,
// sp + 0x10) and released; when self's vector at +0x14 is non-empty, each element (checked against 0x08CB4274) gets
// its virtual slot 0x18 called with the writer, wrapped in func_08A768CC / func_08A768E0. With the byte clear: a
// vector at sp + 0x18 is filled by func_08AB70A4(self); each element is converted twice (func_08AAE2D4,
// func_08AAE224) into strings that are printed (0x08CB42C4) and released, then self's +0x14 vector is walked the same
// way, then one (func_08A768E0, func_08A767EC 0x08CB42C0) pair is printed per vector element and the vector is
// destroyed. v0 is the last call's result (the vector's word count on the destroy-the-empty-vector path).

// 0x088D13E0 (holder = a0, param1 = a1, context = a2, object = a3): a per-object state machine. scene =
// func_088DCD28(context), count = scene's slot 0x8C, list = context->+0x14C's slot 0x2C; list's slots 0x34 / 0x3C /
// 0x40 fill three words at sp + 0, +4, +8 which func_088FB0C8 turns into angles; scene's slot 0x94 decides `inside`
// (how many indices answer non-zero) and the two func_088FB100 results at f22 / f21. The switch on holder->+0x54
// (table 0x08C89E94, 7 entries: 0x088D1570, 0x088D1720, 0x088D168C, 0x088D16FC, 0x088D1708, 0x088D172C twice;
// anything above 6 takes case 0 with v0 = 0): state 1 -> 2, state 3 -> 4, state 4 -> 2 when func_088D0F38 fails,
// states 5/6 -> 0; case 2 runs func_088D0F38 then func_088FB144 and ends at 5 / 6; case 0 (with `inside` non-zero)
// runs two func_088FB144 checks, and when every index was inside compares the two func_088FB100 angles, then
// func_088D390C(sp + 0xC, sp + 0x18) and func_088D2DE0 measure holder->+0 to set state 1 when the scene's slot 0x3C
// moved by more than 0x3FB2B8C3 and |f22| < |f21|. v0 keeps the register the asm leaves on every exit (the last
// call's result, the constant a case stores, or the jump table's entry address).

// 0x08A7DF34 (result = a0, source = a1): joins one or more values of `source` into `result`. The word pair at
// source->+0 / +4 is a vector of pointers whose element count (difference >> 2) decides: fewer than 2 (the small
// path) builds a temporary with func_08A5D438(sp + 0x14) and func_08A7D710(sp + 0x18, result, sp + 0x14, source)
// and, when the temporary at sp + 0x14 exists, hands it (func_08A5D444) to func_08A7DE18 with the value at
// sp + 0x18; a count of exactly 1 uses func_08A7DA34 into sp + 0x14 with the temporary at sp + 0x18 instead.
// Otherwise func_08A759BC(source) gives the count: a vector of that many pointers is allocated
// (func_08A5BC20, page 0x08A696E4), filled by func_08C251D0 and defaulted to 0x08CF6FDC, its elements 1..n-1
// built from source 0..n-2 by func_08A759E0 / func_08A6A23C; func_08A76744(sp + 0x28) and func_08A76AA0(sp + 0x14)
// wrap it, func_08A7DBB0 writes `result`, func_08A76AB4(sp + 0x18, 0x08CF6ED8) and func_08A7C28C decide whether the
// last value (func_08A759E0(sp + 0xC)) equals the global at 0x08CF6FA0 (func_08A7D24C) or must be built again from
// source and assigned to it (func_08A7DE18), then every temporary and the vector are released and v0 is the last
// call's result (the vector's word count when its begin pointer is null).
void func_08A7DF34(Runtime &rt, AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);

    const u32 temporary = frame.at(0x14u);
    const u32 other = frame.at(0x18u);
    const s32 elements = (static_cast<s32>(word(rt, source + 4u)) - static_cast<s32>(word(rt, source))) >> 2;
    if (static_cast<u32>(elements) < 2u) {
        call_set(rt, ctx, 0x08A5D438u, {temporary});
        call_set(rt, ctx, 0x08A7D710u, {other, result, temporary, source});
        u32 first = word(rt, temporary);
        const u32 second = word(rt, other);
        if (first != 0u) {
            if (second == 0u) {
                call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1360u});
                first = word(rt, temporary);
            }
            const u32 value = call_set(rt, ctx, 0x08A5D444u, {checked(rt, ctx, first, 0x08CB1358u)}).v0;
            call_set(rt, ctx, 0x08A7DE18u, {second, value});
        }
        call_set(rt, ctx, kDestroy, {other});
        call_set(rt, ctx, kDestroy, {temporary});
        return;
    }

    const u32 size = call_set(rt, ctx, 0x08A759BCu, {source}).v0;
    const u32 bytes = size * 4u;
    if (size == 1u) {
        // The one-element shape: the same join with the temporaries in the other order.
        call_set(rt, ctx, 0x08A5D438u, {other});
        call_set(rt, ctx, 0x08A7DA34u, {temporary, result, other, source});
        u32 first = word(rt, other);
        const u32 second = word(rt, temporary);
        if (first != 0u) {
            if (second == 0u) {
                call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1360u});
                first = word(rt, other);
            }
            const u32 value = call_set(rt, ctx, 0x08A5D444u, {checked(rt, ctx, first, 0x08CB1358u)}).v0;
            call_set(rt, ctx, 0x08A7DE18u, {second, value});
        }
        call_set(rt, ctx, kDestroy, {temporary});
        call_set(rt, ctx, kDestroy, {other});
        return;
    }

    const u32 scratch = frame.at(4u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x20u), 0u);
    put(rt, frame.at(0x24u), 0u);
    const u32 block = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u}).v0;
    put(rt, frame.at(0x24u), block + bytes);
    put(rt, frame.at(0x1Cu), block);
    put(rt, frame.at(0x20u), block);
    call_set(rt, ctx, 0x08A696E4u, {scratch});
    call_set(rt, ctx, 0x08C251D0u, {block, size, scratch, frame.at(1u)});
    call_set(rt, ctx, 0x08A69D68u, {scratch});
    call_set(rt, ctx, 0x08A6A23Cu, {block, 0x08CF6FDCu});
    put(rt, frame.at(0x20u), block + bytes);

    for (u32 index = 0u; index + 1u < size; ++index) {
        const u32 element = word(rt, frame.at(0x1Cu)) + (index + 1u) * 4u;
        call_set(rt, ctx, 0x08A759E0u, {frame.at(0x10u), source, index});
        call_set(rt, ctx, 0x08A6A23Cu, {element, frame.at(0x10u)});
        call_set(rt, ctx, 0x08A69D68u, {frame.at(0x10u)});
    }

    const u32 range = frame.at(0x28u);
    call_set(rt, ctx, 0x08A76744u, {range, frame.at(0x1Cu)});
    call_set(rt, ctx, 0x08A76AA0u, {temporary});
    call_set(rt, ctx, 0x08A7DBB0u, {result, temporary, range});
    call_set(rt, ctx, 0x08A76AB4u, {other, 0x08CF6ED8u});
    if (call_set(rt, ctx, 0x08A7C28Cu, {other, temporary}).v0 != 0u) {
        call_set(rt, ctx, 0x08A759E0u, {frame.at(0xCu), source, size - 1u});
        const u32 shared = word(rt, 0x08CF6FA0u);
        const u32 built = word(rt, frame.at(0xCu));
        call_set(rt, ctx, 0x08A69D68u, {frame.at(0xCu)});
        if (built != shared) {
            if (shared == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB1360u});
            call_set(rt, ctx, 0x08A759E0u, {frame.at(8u), source, size - 1u});
            call_set(rt, ctx, 0x08A7DE18u, {shared, frame.at(8u)});
            call_set(rt, ctx, 0x08A69D68u, {frame.at(8u)});
        } else {
            call_set(rt, ctx, 0x08A7D24Cu, {checked(rt, ctx, word(rt, frame.at(0x18u)), 0x08CB1360u)});
        }
    }

    call_set(rt, ctx, kDestroy, {other});
    call_set(rt, ctx, kDestroy, {temporary});
    call_set(rt, ctx, 0x08A75A88u, {range});
    call_set(rt, ctx, 0x08C24F04u, {word(rt, frame.at(0x1Cu)), word(rt, frame.at(0x20u)), frame.at(0u)});

    const u32 begin = word(rt, frame.at(0x1Cu));
    const u32 count = element_count(begin, word(rt, frame.at(0x24u)));
    ctx.gpr[2] = count;
    if (begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {begin, count << 2, 4u});
}

} // namespace
} // namespace new_batch_main_244
} // namespace gt::game

#define NEW244(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_244::name, gt::ReturnKind::kind)
NEW244(0x08902884u, func_08902884, Word);
NEW244(0x08A7DF34u, func_08A7DF34, Word);

// ---- from main-session batch 245 (draft 81702d9a95e3b0e09eeeb9498d64ec50e8a48c6c) ----
// New batch, main session 245: functions the recompiler emitted that no src/ file defines or registers. Written by
// hand from `tools/disasm` as structured C; a draft batch, not yet verified by the differential gate. The helpers below
// cover the shapes it repeats: a guest call that leaves v0/v1/f0 as the callee leaves them, the byte readers, and the
// frame guard for the callee-saved floats.
//
// Input shapes (the gate's synthetic generator needs them; see /tmp/claude-1000/shapes_main245.txt): 0x089419D8 writes
// a 0x60-byte section and reads a 0x18-byte record, 0x08AA6E38 reads a cursor word and walks the byte string it points
// at, 0x08B137E0 walks the record array at +0x2D4 of a large section (its record count at +0x4D4 is past the 1024-byte
// shape cap, so most synthetic inputs end in the same fault as the original and are simply not comparable).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_245 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s16 = std::int16_t;
using s8 = std::int8_t;
using GuestResult = gt::GuestResult;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 as the callee left them (a callee that never writes v0 hands the caller's value on).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments in $f12-$f19.
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lbu` / `lhu` / `lh` / `seb` widen a loaded field the way the instruction does.
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s16 half_s(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<s8>(value))); }

// $f20 and $f22 are callee-saved: the body sets them where the original does and the caller's values come back at exit.
class SavedFloats {
public:
    SavedFloats(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx), count_(0u) {
        for (const unsigned reg : registers) {
            saved_[count_] = ctx.fpr[reg];
            indexes_[count_] = reg;
            ++count_;
        }
    }
    ~SavedFloats() {
        for (unsigned i = 0u; i < count_; ++i) ctx_.fpr[indexes_[i]] = saved_[i];
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[8]{};
    unsigned indexes_[8]{};
    unsigned count_;
};

// The flag word the two vector writers check (0x08CDE508 + 0x14D); nonzero stores the vectors straight.
constexpr u32 kVectorFlagBase = 0x08CDE508u;

// 0x089419D8 (a0 = the section to fill, a1 = the packed record, a2 = a percentage byte, a3 = the mode, t0 = a config
// object, f12 = a scale): fills the section's twelve parameters. In modes 1 and 3 the first value is the halfword at
// +0x12 through 0x0895EDD8 (fourth and fifth float arguments), otherwise the halfword at +0x10 times 980, scaled by
// the byte's percentage twice. 0x089416B8 turns the byte into the second scale. Modes 1 and 2 read two more halves as
// spline weights (0x0895EE04); every other mode reads four bytes (0x089416D4). Each pair of vectors then goes through
// 0x08941750, and with the flag at 0x08CDE655 set it is stored into the section as is, otherwise 0x08941784 writes it
// (a0-a2 point at the three x/y/z words, a3 is the config object, t0 the axis table).

// 0x08AA6E38 (a0 = a flag byte: zero takes the signed form, anything else the unsigned one; a1 = the format
// character; a2 = the reader's cursor word; a3 = a word holding the destination object): reads the value the character
// asks for out of the byte stream the cursor points at (B/b one byte, s two, i/f four, l/d eight, big-endian across the
// bytes), formats it into a temporary string object on the frame and appends that to the destination's vector at
// +0x14. Every formatter is picked by the character (0x08AB19B0 unsigned byte, 0x08A960C0 signed byte, 0x08AB022C
// unsigned half, 0x08A7334C signed half, 0x08A5FA8C int, 0x08A5DEE8 64-bit, 0x08A85B1C unsigned, 0x08A87EEC float,
// 0x08A8B098 another 64-bit); the temporary is released after the append. With the append room exhausted the vector
// grows through 0x08B8A03C, else the write pointer advances.
void func_08AA6E38(Runtime &rt, AllegrexContext &ctx) {
    const u32 flags = ctx.gpr[4] & 0xFFu;
    const u32 format = ctx.gpr[5];
    const u32 cursor_holder = ctx.gpr[6];
    const u32 destination_holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x50u);
    const u32 key = format - 0x42u;
    // Both dispatch tables hold one row per character from 'B' to 's', and both cases of each letter land on the same
    // handler, so the letter is the character folded to lower case.
    const u32 letter = format | 0x20u;
    u32 low = 0u;
    u32 high = 0u;
    if (key < 0x32u) {
        u32 width = 0u;
        switch (letter) {
        case 'b':  // 'B' / 'b'
            width = 1u;
            break;
        case 's':  // 'S' / 's'
            width = 2u;
            break;
        case 'i':  // 'I' / 'i'
        case 'f':  // 'F' / 'f'
            width = 4u;
            break;
        case 'l':  // 'L' / 'l'
        case 'd':  // 'D' / 'd'
            width = 8u;
            break;
        default:
            break;
        }
        if (width != 0u) {
            // The stream is big-endian for every width the table reaches (the little-endian half of the loop is dead:
            // the shift always lands on a byte boundary, and the `srli` side of the split is zero for every one).
            const u32 cursor = word(rt, cursor_holder);
            for (u32 index = 0u; index < width; ++index) {
                const u32 value = byte_u(rt, cursor + index);
                const unsigned position = 8u * (width - 1u - index);
                if (position >= 32u) high |= value << (position - 32u);
                else low |= value << position;
                put(rt, cursor_holder, cursor + index + 1u);
            }
        }
    }
    // The accumulator string at sp+0x28; its construction leaves a1 where the next calls see it.
    const u32 accumulator = frame.at(0x28u);
    const u32 leftover = call_keep(rt, ctx, 0x08A76AA0u, {accumulator}).a1;
    bool have_temporary = false;
    u32 temporary = 0u;
    if (key < 0x32u) {
        switch (letter) {
        case 'b': {  // one byte: unsigned when the flag is set, signed otherwise
            temporary = flags != 0u ? frame.at(0x24u) : frame.at(0x20u);
            if (flags != 0u) call_keep(rt, ctx, 0x08AB19B0u, {temporary, low & 0xFFu});
            else call_keep(rt, ctx, 0x08A960C0u, {temporary, sext8(low)});
            have_temporary = true;
            break;
        }
        case 's': {  // halfword
            temporary = flags != 0u ? frame.at(0x1Cu) : frame.at(0x18u);
            if (flags != 0u) call_keep(rt, ctx, 0x08AB022Cu, {temporary, low & 0xFFFFu});
            else call_keep(rt, ctx, 0x08A7334Cu, {temporary, static_cast<u32>(static_cast<s32>(static_cast<s16>(low)))});
            have_temporary = true;
            break;
        }
        case 'i': {  // int
            temporary = flags != 0u ? frame.at(0x14u) : frame.at(0x10u);
            if (flags != 0u) call_keep(rt, ctx, 0x08A5FA8Cu, {temporary, low});
            else call_keep(rt, ctx, 0x08A85B1Cu, {temporary, low});
            have_temporary = true;
            break;
        }
        case 'l': {  // 64-bit
            temporary = flags != 0u ? frame.at(0xCu) : frame.at(0x8u);
            if (flags != 0u) call_keep(rt, ctx, 0x08A5DEE8u, {temporary, leftover, low, high});
            else call_keep(rt, ctx, 0x08A85B1Cu, {temporary, leftover, low, high});
            have_temporary = true;
            break;
        }
        case 'f': {  // float from the low word
            temporary = frame.at(0x4u);
            ctx.fpr[12] = std::bit_cast<float>(low);
            call_keepf(rt, ctx, 0x08A87EECu, {temporary}, {ctx.fpr[12]});
            have_temporary = true;
            break;
        }
        case 'd': {  // 64-bit through 0x08A8B098 (which overwrites a1 before reading it)
            temporary = frame.at(0x0u);
            call_keep(rt, ctx, 0x08A8B098u, {temporary, leftover, low, high});
            have_temporary = true;
            break;
        }
        default:
            break;
        }
    }
    if (have_temporary) {
        // The asm compares the accumulator's address with the temporary's before appending; two frame slots never
        // match, so the append always runs.
        if (accumulator != temporary) call_keep(rt, ctx, 0x08A5BAD8u, {accumulator, word(rt, temporary)});
        call_keep(rt, ctx, 0x08A5B9ECu, {temporary});
    }
    // The destination's vector at +0x14: an empty write pointer appends a fresh element, a full one grows (0x08B8A03C),
    // anything else appends at the write pointer and advances it by one word. *destination == 0 runs the (empty) null
    // check function and then writes through the null-derived address, as the asm does.
    u32 container = word(rt, destination_holder);
    if (container == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB2F98u});
    const u32 vector = container + 0x14u;
    u32 write = word(rt, vector + 4u);
    const u32 limit = word(rt, vector + 8u);
    if (write == limit) {
        call_keep(rt, ctx, 0x08B8A03Cu, {vector, write, accumulator});
    } else {
        if (write != 0u) call_keep(rt, ctx, 0x08A76ABCu, {write, accumulator});
        write = word(rt, vector + 4u) + 4u;
        put(rt, vector + 4u, write);
    }
    call_keep(rt, ctx, 0x08A5B9ECu, {accumulator});
}

// 0x08B137E0 (a0 = the section, a1 = a key, a2 = a class byte): finds the key among the 8-byte records at +0x2D4
// (count at +0x4D4, the key being the halfword at record+4) by bubbling the matching record into order (the class
// bytes at record+6/+7 through 0x08B1205C decide); when it is absent, allocates the record's buffer for the key's
// range (the halfword pair at word(+0x248)[2*key] / [2*key+1], 64 bytes each) through 0x08B2BFDC, dropping the last
// record while that fails, finds the insertion point by the class bytes and stores the record. The buffer's answer
// from 0x08B2C1C0 is returned, and 0x08B2C1C0's argument (the record) is left at +0x2D0.

// 0x08908A14 (a0 = the output object, a1 = a path object whose word at +0x98 is the node list, a2 = the control
// array of six floats: four values at 0,4,8,0xC and their weights at 0xC,0x10,0x14, i.e. four (value, rate) pairs).
// 0x08908260 fills sp+0/sp+4 with two parameters (the path object at +0x20, the control array, the two outputs).
// The frame holds a table of 24-byte records from sp+8, each {node, parent level, step, value[3]}: levels 0 and 1
// are seeded with the two interpolations of the control array's first and last pair, then the loop walks the node
// list. A node's low two bits pick which of the four stencils it uses (0 ends the branch and asks 0x0890835C
// whether to accept it, storing the node's offset at a0+0x34); the stencil is evaluated (the node's value at +4
// against the four values and weights), the result pushed as a new level, and the walk either follows the node's
// +8 link or backtracks to the record's saved node and parent level. The return is 1 when a branch was accepted and
// 0 when the list is exhausted.
void func_08908A14(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 path = ctx.gpr[5];
    const u32 control = ctx.gpr[6];
    SavedFloats saved_floats(ctx, {20u});
    GuestFrame frame(ctx, 0x4F0u);
    call_keepf(rt, ctx, 0x08908260u, {path + 0x20u, control, frame.at(0x0u), frame.at(0x4u)}, {});
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    // The four control values and their four weights.
    const float value0 = float_at(rt, control + 0x0u);
    const float value1 = float_at(rt, control + 0x4u);
    const float value2 = float_at(rt, control + 0x8u);
    const float weight0 = float_at(rt, control + 0xCu);
    const float weight1 = float_at(rt, control + 0x10u);
    const float weight2 = float_at(rt, control + 0x14u);
    const float first = float_at(rt, frame.at(0x0u));
    const float second = float_at(rt, frame.at(0x4u));
    // Levels 0 and 1: the control array's two end pairs interpolated by the two parameters. A negative first
    // parameter leaves level 0 at zero with the values themselves.
    if (0.0f <= first) {
        put_float(rt, frame.at(0x10u), first);
        put_float(rt, frame.at(0x14u), value0 + psp_mul(first, weight0));
        put_float(rt, frame.at(0x18u), value1 + psp_mul(first, weight1));
        put_float(rt, frame.at(0x1Cu), value2 + psp_mul(first, weight2));
    } else {
        put_float(rt, frame.at(0x10u), 0.0f);
        put_float(rt, frame.at(0x14u), value0);
        put_float(rt, frame.at(0x18u), value1);
        put_float(rt, frame.at(0x1Cu), value2);
    }
    put(rt, frame.at(0x20u), 0u);  // the node of level 1
    put_float(rt, frame.at(0x28u), second);
    put_float(rt, frame.at(0x2Cu), value0 + psp_mul(second, weight0));
    put_float(rt, frame.at(0x30u), value1 + psp_mul(second, weight1));
    put_float(rt, frame.at(0x34u), value2 + psp_mul(second, weight2));
    u32 node = word(rt, path + 0x98u);
    if (node == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.fpr[20] = 1.0f;
    // Record `level` starts at sp+8+24*level: +0 the node to resume at, +4 the parent level, +8 the step, +0xC..+0x14
    // the three stencil values.
    const auto record = [&](u32 level) { return frame.at(0x8u) + 24u * level; };
    const auto record_value = [&](u32 level, u32 index) { return float_at(rt, record(level) + 0xCu + 4u * index); };
    u32 level = 1u;
    u32 parent = 0u;
    // Backtrack: pop this level's record, the walk resumes at its saved node one level up.
    const auto backtrack = [&]() {
        const u32 saved_node = word(rt, record(level));
        const u32 saved_level = word(rt, record(level) + 4u);
        parent = level;
        level = saved_level;
        node = saved_node;
    };
    for (;;) {
        const u32 flags = word(rt, node);
        const u32 count = flags & 3u;
        const u32 index = count - 1u;
        if (count == 0u) {
            // The end of a branch: 0x0890835C gets the object, the path, the control array, the node's data and the
            // two levels' steps; a nonzero answer accepts it and stores the node's offset.
            call_keepf(rt, ctx, 0x0890835Cu, {self, path, control, node + 0x8u, flags >> 2},
                       {float_at(rt, record(parent) + 0x8u), float_at(rt, record(level) + 0x8u)});
            if (ctx.gpr[2] != 0u) {
                put(rt, self + 0x34u, node - path);
                ctx.gpr[2] = 1u;
                return;
            }
            backtrack();
            if (node == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
            continue;
        }
        const float node_value = float_at(rt, node + 0x4u);
        const float difference = node_value - record_value(parent, index);
        u32 resume = 0u;
        u32 next = 0u;
        if (0.0f <= difference) {
            if (record_value(level, index) <= node_value) {
                // The value is within this level's span: follow the node's +8 link, or give the branch up.
                if ((flags & 0x4u) == 0u) {
                    backtrack();
                    if (node == 0u) {
                        ctx.gpr[2] = 0u;
                        return;
                    }
                    continue;
                }
                node += 0x8u;
                continue;
            }
            if (difference == 0.0f) {
                if ((flags >> 3) == 0u) {
                    backtrack();
                    if (node == 0u) {
                        ctx.gpr[2] = 0u;
                        return;
                    }
                    continue;
                }
                node += flags >> 3;
                continue;
            }
            // The value lies before this level's span: resume at the node's far link, follow its +8 link.
            resume = (flags >> 3) != 0u ? node + (flags >> 3) : 0u;
            next = (flags & 0x4u) != 0u ? node + 0x8u : 0u;
        } else {
            if (node_value < record_value(level, index)) {
                if ((flags >> 3) == 0u) {
                    backtrack();
                    if (node == 0u) {
                        ctx.gpr[2] = 0u;
                        return;
                    }
                    continue;
                }
                node += flags >> 3;
                continue;
            }
            // The value lies past this level's span: resume at the node's +8 link, follow its far link.
            resume = (flags & 0x4u) != 0u ? node + 0x8u : 0u;
            next = (flags >> 3) != 0u ? node + (flags >> 3) : 0u;
        }
        // The stencil: the node's value against the control array's four (value, weight) pairs, re-centred on the
        // current value, pushed as a new level (skipping a level the parent just used).
        const float span = float_at(rt, control + 0xCu + 4u * index);
        const float delta = node_value - float_at(rt, control + 4u * index);
        const u32 previous_index = count < 3u ? count : count - 3u;
        const u32 following_index = (count + 1u) < 3u ? count + 1u : count - 2u;
        const float previous_value =
            float_at(rt, control + 4u * previous_index) +
            psp_mul(delta, float_at(rt, control + 0xCu + 4u * previous_index));
        const float following_value =
            float_at(rt, control + 4u * following_index) +
            psp_mul(delta, float_at(rt, control + 0xCu + 4u * following_index));
        const u32 new_level = parent == level + 1u ? level + 2u : level + 1u;
        const u32 target = record(new_level);
        put_float(rt, target + 0xCu + 4u * index, node_value);
        put_float(rt, target + 0xCu + 4u * previous_index, previous_value);
        put_float(rt, target + 0xCu + 4u * following_index, following_value);
        put_float(rt, target + 0x8u, psp_mul(ctx.fpr[20] / span, delta));
        put(rt, target + 0x4u, level);
        put(rt, target, resume);
        level = new_level;
        node = next;
        if (node == 0u) {
            backtrack();
            if (node == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
    }
}
} // namespace
} // namespace new_batch_main_245
} // namespace gt::game

#define NEW245(address, name) GT_DECOMP(address, gt::game::new_batch_main_245::name, gt::ReturnKind::Word)
NEW245(0x08AA6E38u, func_08AA6E38);
NEW245(0x08908A14u, func_08908A14);

// ---- from main-session batch 246 (draft a15b63db932a975a7a00467e9fe44e07725566fe) ----
// New batch 246 (main session): the six long functions (234-237 instructions) listed for it, all written and registered.
// The gate never had input shapes for them, so they are drafted here together with the shapes for the synthetic generator
// (/tmp/claude-1000/shapes_main246.txt, evidence per line; the verifier merges them into config/synth_shapes.txt).
// Written from `tools.disasm` one function at a time; the comments describe access shapes, not game names.
//
//   * 0x08A192D4 / 0x08A7B8C8 — one child-walk body, two constant sets (the tree node at a0->+0x14, visitor a1).
//   * 0x08A15800 — the ordering test between two 2-row blocks in `self`'s space (0/1 answer).
//   * 0x0889A5BC — fills the 32-byte skin entries at self + 0x8E0 from the painter's list, with a quaternion product.
//   * 0x08862438 — builds the 0x50-byte record at self + 0x4120 + 0x50 * index and hands the indices to func_08860C80.
//   * 0x08BBAA74 — the structured rewrite of the emitter-style draft in hot_batch_115 (see its own comment).
//
// Audits: frame_audit 0 wrong, literal_check 0 missing, arg_reads 0. sp_loads lists 7 `lw $s0, off($sp)` at offsets s0 was
// never saved to (08A192D4 0x10/0x10/4, 08A7B8C8 0xC/0xC/0x18, 08BBAA74 0x24): every one is a real frame read and is
// modelled (`word(rt, frame.at(off))`), not a register restore.
//
// Conventions (as in hot_batch_102): every call_guest sets the argument registers the original sets before the `jal`
// (a0-a3, t0-t3, f12-f15) and keeps v0/v1/f0 as the callee leaves them. A `jal`'s delay slot runs BEFORE the call, a
// `beql`/`bnel`/`bc1tl`/`bc1fl`/`bgtzl` delay slot only when the branch is taken, and the delay slot of a non-likely
// branch on both paths. Callee-saved registers are never written (the original restores them), so there is no SavedRegs.
// v0 is set as the original leaves it on every exit, `lui` leftovers included.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_246 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose v0 / v1 / f0 stay in the context like the original's (a callee that sets no v0 hands the previous
// one on to the code after it, so ctx starts it with whatever the original's v0 was at the `jal`).
inline GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                               std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// The argument registers as the original sets them before the jal (a0-a3, t0-t3 in order); what the callee does not take
// keeps the caller's value.
inline void set_arguments(AllegrexContext &ctx, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
}
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    set_arguments(ctx, arguments);
    return call_result(rt, ctx, address, arguments).v0;
}
// The same with f12-f15 as well.
inline GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments) {
    set_arguments(ctx, arguments);
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_result(rt, ctx, address, arguments, float_arguments);
}
// A `lw v1, 0(a0); lw v0, slot(v1); jalr v0` virtual call: v1 = the vtable, v0 = the slot's function before the call.
inline GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    set_arguments(ctx, arguments);
    return call_result(rt, ctx, function, arguments);
}
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
inline s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
// A word at any alignment (`lwl` + `lwr`): the guest is little endian.
inline u32 unaligned_word(Runtime &rt, u32 address) {
    return rt.memory().load8(address) | rt.memory().load8(address + 1u) << 8u | rt.memory().load8(address + 2u) << 16u |
           static_cast<u32>(rt.memory().load8(address + 3u)) << 24u;
}
inline void put_unaligned_word(Runtime &rt, u32 address, u32 value) {
    for (unsigned index = 0u; index < 4u; ++index) put8(rt, address + index, value >> (8u * index));
}

// ---------------------------------------------------------------------------------------------------------------------
// The child-walk pair 0x08A192D4 / 0x08A7B8C8: one body, two constant sets (the assertion strings the null checks
// report and the separator of the path builder).

struct ChildWalk {
    u32 container_null;  // reported when this->+0x14 (the node the walk belongs to) is null
    u32 child_null;      // reported when a walked node's +0x14 attribute is null
    u32 output_null;     // reported when the lookup func_08A7C0E0 answers no object
    u32 wrapper_null;    // reported when the wrapper func_08A7B544 answers no object
    u32 separator;       // the separator func_08A7F8E8 joins the path with
    // The frame slots the original keeps its temporaries in (the two functions allocate the same 0x90 frame but do not
    // assign the slots alike), so each twin keeps its own layout.
    u32 path1;      // the walked node's path (released at the very end)
    u32 held;       // func_08A7C0E0's output object
    u32 path2;      // the child's path
    u32 output;     // func_08A8CE2C's output object
    u32 cursor;     // the converted child path func_08A5D444 / func_08A9032C take
    u32 alternate;  // func_08A7B544's wrapper output
    u32 key;        // the word handed to func_08A7C0E0
};

// The libstdc++ COW string release the tail of the function ends with: a pointer at `slot`, the 12-byte header in front
// of it, the static empty string 0x08D74FD0 short-circuit (v0 = that address, v1 = the `lui` half), else the refcount
// step func_08B60804(pointer - 4, -1) and, when it does not answer > 0, func_08B8542C(pointer - 0xC, scratch).
inline void release_string(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 scratch) {
    const u32 pointer = word(rt, slot);
    const u32 block = pointer - 0xCu;
    if (block == 0x08D74FD0u) {
        ctx.gpr[2] = 0x08D74FD0u;
        ctx.gpr[3] = 0x000A0000u;
        return;
    }
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) > 0) return;
    (void)call_set(rt, ctx, 0x08B8542Cu, {block, scratch});
}

void walk_children(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, const ChildWalk &k) {
    const u32 self = ctx.gpr[4];
    const u32 visitor = ctx.gpr[5];
    u32 node = word(rt, self + 0x14u);
    if (node == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {k.container_null});
    // The walked node's own path, handed to the visitor's "begin" call; the converted temporary is destroyed at once.
    (void)call_set(rt, ctx, 0x08A7F8E8u, {frame.at(k.path1), node, 0u, k.separator});
    (void)call_set(rt, ctx, 0x08A76724u, {frame.at(0x34u), frame.at(k.path1)});
    (void)call_set(rt, ctx, 0x08A8DB8Cu, {visitor, frame.at(0x34u)});
    (void)call_set(rt, ctx, 0x08A75A88u, {frame.at(0x34u)});
    // The child list: a sentinel node in the frame (word 0 = next, +4 = previous, +8 / +0xC = the sentinel itself for the
    // iterator's end), filled by func_08A7D1F8 through the handle at frame +0x40.
    node = word(rt, self + 0x14u);
    const u32 sentinel = frame.at(0x44u);
    put(rt, frame.at(0x54u), 0u);
    put(rt, sentinel, 0u);
    put(rt, frame.at(0x48u), 0u);
    put(rt, frame.at(0x4Cu), sentinel);
    put(rt, frame.at(0x50u), sentinel);
    if (node == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {k.container_null});
    (void)call_set(rt, ctx, 0x08A7D1F8u, {node, frame.at(0x40u)});
    u32 current = word(rt, frame.at(0x4Cu));
    while (current != sentinel) {
        const u32 attribute = word(rt, current + 0x14u);
        if (attribute == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {k.child_null});
        // The node's owner: the object at attribute + 0x10, real only when its own +4 word is set.
        const u32 holder = word(rt, attribute + 0x10u);
        if (holder != 0u && word(rt, holder + 4u) != 0u) {
            if (holder == word(rt, self + 0x14u)) {
                // One of the walked node's own: handed straight to the visitor's slot 0x28 (a0 = the child).
                (void)call_virtual(rt, ctx, attribute, 0x28u, {attribute, visitor});
            } else {
                put(rt, frame.at(k.key), holder);
                (void)call_set(rt, ctx, 0x08A7C0E0u, {frame.at(k.held), frame.at(k.key)});
                const u32 output = word(rt, frame.at(k.held));
                if (output == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {k.output_null});
                // The lookup's object gets its own path, then func_08A8CE2C applies the visitor to it.
                (void)call_set(rt, ctx, 0x08A7F8E8u, {frame.at(k.path2), output, 0u, k.separator});
                (void)call_set(rt, ctx, 0x08A76724u, {frame.at(0x28u), frame.at(k.path2)});
                (void)call_set(rt, ctx, 0x08A8CE2Cu, {frame.at(k.output), visitor, frame.at(0x28u)});
                (void)call_set(rt, ctx, 0x08A75A88u, {frame.at(0x28u)});
                if (word(rt, frame.at(k.output)) == 0u) {
                    // The direct application answered nothing: wrap the object (func_08A7BCC0 + func_08A7B544) and call
                    // its slot 0x28, then destroy the wrapper.
                    const u32 again = word(rt, frame.at(k.held));
                    if (again == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {k.output_null});
                    const u32 converted = call_set(rt, ctx, 0x08A7BCC0u, {again});
                    (void)call_set(rt, ctx, 0x08A7B544u, {frame.at(k.alternate), converted, frame.at(k.held)});
                    const u32 wrapped = word(rt, frame.at(k.alternate));
                    if (wrapped == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {k.wrapper_null});
                    (void)call_virtual(rt, ctx, wrapped, 0x28u, {wrapped, visitor});
                    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(k.alternate)});
                }
                // The child's path goes through func_08A76724, then func_08A5D444 + func_08A9032C; the temporaries and
                // the string are released.
                (void)call_set(rt, ctx, 0x08A76724u, {frame.at(k.cursor), frame.at(k.path2)});
                const u32 child = word(rt, current + 0x14u);
                if (child == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {k.child_null});
                const u32 converted_child = call_set(rt, ctx, 0x08A5D444u, {child});
                (void)call_set(rt, ctx, 0x08A9032Cu, {visitor, frame.at(k.cursor), converted_child, current + 0x10u});
                (void)call_set(rt, ctx, 0x08A75A88u, {frame.at(k.cursor)});
                (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(k.output)});
                release_string(rt, ctx, frame.at(k.path2), frame.at(1u));
                (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(k.held)});
            }
        }
        current = call_set(rt, ctx, 0x08B5F100u, {current});
    }
    (void)call_set(rt, ctx, 0x08A8C1C4u, {visitor});
    (void)call_set(rt, ctx, 0x08C00860u, {frame.at(0x40u), word(rt, frame.at(0x48u))});
    release_string(rt, ctx, frame.at(k.path1), frame.at(0u));
}

// 0x08A192D4 — the walk of the children of a0->+0x14 for the visitor a1.
// 0x08A7B8C8 — the same body with its own constants (and its own frame slots).

// ---------------------------------------------------------------------------------------------------------------------
// 0x08A15800(self, first, second, mode): an ordering test between two 2-row blocks (first + 8 / + 0x18 and
// second + 8 / + 0x18, four floats each) in the space of `self`. The 16 floats are stored in the frame at +0x10 and every
// even one gets `mode != 0 ? 10000.0f : 0.0f` added (the odd ones are added to 0.0f), then func_08A155B8(self, row0, row1,
// out0, out1) tests each block (skipped when the block's first word equals self->+0x34); its two floats per block land at
// frame +0 / +4 and +8 / +0xC. The answer is 1 when `first` comes first:
//   * both blocks tested: word +4 compared (signed, the order flipped when mode is set), then the two test floats, then
//     the distances of the blocks' midpoints from the midpoint of self->+0x18 / self->+0x20, then the blocks' first words
//     (unsigned);
//   * only the second tested: 1; only the first: 0;
//   * neither tested: with self->+0x11 set and mode clear, the same call again with mode 1 (which forces the mode compare);
//     otherwise the block whose first word equals self->+0x34 answers 1, else the first words (unsigned).
void func_08A15800(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 mode = ctx.gpr[7] & 0xFFu;
    // The 4x4 block at frame +0x10: the first block's rows (+8 and +0x18), then the second's.
    float block[16];
    for (unsigned index = 0u; index < 4u; ++index) block[index] = float_at(rt, first + 8u + 4u * index);
    for (unsigned index = 0u; index < 4u; ++index) block[4u + index] = float_at(rt, first + 0x18u + 4u * index);
    for (unsigned index = 0u; index < 4u; ++index) block[8u + index] = float_at(rt, second + 8u + 4u * index);
    for (unsigned index = 0u; index < 4u; ++index) block[12u + index] = float_at(rt, second + 0x18u + 4u * index);
    const float expand = mode != 0u ? std::bit_cast<float>(0x461C4000u) : 0.0f;
    for (unsigned index = 0u; index < 16u; ++index) {
        // `add.s f, f17, f` on the even words and `add.s f, f, f0(0.0)` on the odd ones.
        put_float(rt, frame.at(0x10u + 4u * index), index % 2u == 0u ? expand + block[index] : block[index] + 0.0f);
    }
    const u32 self_key = word(rt, self + 0x34u);
    u32 first_tested = 0u;
    if (word(rt, first) != self_key) {
        first_tested = call_set(rt, ctx, 0x08A155B8u, {self, frame.at(0x10u), frame.at(0x20u), frame.at(0u), frame.at(4u)}) != 0u ? 1u : 0u;
    }
    u32 second_tested = 0u;
    if (word(rt, second) != self_key) {
        second_tested = call_set(rt, ctx, 0x08A155B8u, {self, frame.at(0x30u), frame.at(0x40u), frame.at(8u), frame.at(0xCu)}) != 0u ? 1u : 0u;
    }
    const auto answer = [&](u32 value) {
        ctx.gpr[3] = value;
        ctx.gpr[2] = value;
    };
    if (first_tested == 0u) {
        if (second_tested != 0u) return answer(0u);
        if (rt.memory().load8(self + 0x11u) == 0u || mode != 0u) {
            const u32 first_word = word(rt, first);
            if (first_word == self_key) return answer(1u);
            if (self_key != word(rt, second)) return answer(first_word < word(rt, second) ? 1u : 0u);
            return answer(0u);
        }
        return answer(call_set(rt, ctx, 0x08A15800u, {self, first, second, 1u}));
    }
    if (second_tested == 0u) return answer(1u);
    const u32 first_key = word(rt, first + 4u);
    const u32 second_key = word(rt, second + 4u);
    if (mode == 0u) {
        if (signed_of(second_key) < signed_of(first_key)) return answer(1u);
        if (signed_of(first_key) < signed_of(second_key)) return answer(0u);
    } else {
        if (signed_of(first_key) < signed_of(second_key)) return answer(1u);
        if (signed_of(second_key) < signed_of(first_key)) return answer(0u);
    }
    float left = float_at(rt, frame.at(4u));
    float right = float_at(rt, frame.at(0xCu));
    if (left < right) return answer(1u);
    if (right < left) return answer(0u);
    left = float_at(rt, frame.at(0u));
    right = float_at(rt, frame.at(8u));
    if (left < right) return answer(1u);
    if (right < left) return answer(0u);
    // The midpoints of both blocks' frames (+0x14..+0x1C and +0x34..+0x3C) against self's (+0x18, +0x20); |x| < |y| answers 1.
    const float first_low = float_at(rt, frame.at(0x14u));
    const float self_high = float_at(rt, self + 0x20u);
    const float self_low = float_at(rt, self + 0x18u);
    const float first_high = float_at(rt, frame.at(0x1Cu));
    const float second_low = float_at(rt, frame.at(0x34u));
    const float second_high = float_at(rt, frame.at(0x3Cu));
    const float middle = (self_low + self_high) * 0.5f;
    float first_span = (first_low + first_high) * 0.5f - middle;
    if (first_span < 0.0f) first_span = -first_span;
    float second_span = (second_low + second_high) * 0.5f - middle;
    if (second_span < 0.0f) second_span = -second_span;
    if (first_span < second_span) return answer(1u);
    if (second_span < first_span) return answer(0u);
    return answer(word(rt, first) < word(rt, second) ? 1u : 0u);
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0889A5BC(self, painter): fills the skin list at self + 0x8E0 from the painter's entries.
//
// The painter's slot 0x10 fills the frame at +0x28 (three floats) and slot 0x14 the frame at +0x4C; func_08B4B114 then
// builds a transform from the first three floats into self + 0x460. self + 0xB08 (a byte) is cleared and self + 0xB04
// takes the painter. The painter's slot 0x100 is the count limit; the two maybe-null sub-objects at self + 0x20 (its +8)
// and at self + 0x30 / self + 0x68 (their +0x68) are negotiated with func_08AF8FF0 when func_08AF923C answers more than
// the limit. The painter's slot 0xF4 gives the entry limit; the list (self + 0x8E0 begin, +0x8E4 end) holds 32-byte
// entries, and min(count, limit) of them are updated: the painter's slot 0xF8(painter, index, entry, entry + 0x10) must
// answer non-zero (the answer is stored as the byte at entry + 0x1C); then func_08BAC1D8(entry) gives the destination
// quaternion, func_08BAC1E0(entry + 0x10) a handle, func_08BAC270(handle, frame + 0x34), func_08BAC25C(frame + 0x5C,
// frame + 0x18), func_08BAC428(frame + 0x40, the two words at the second pair's v1), func_08BAC284(handle, the pair's v0,
// frame + 0x40) and func_08BAC248(frame + 0x5C, destination) give two quaternions whose product (a x b, in x / y / z / w
// order) is written to the destination. With a positive entry count the loop runs that many times, and func_08AF61A4
// (self + 0x20's +8 or 0, begin, begin, 0, 0) closes it; v0 is its answer (the limit when no entry was updated).
void quaternion_product(Runtime &rt, u32 destination, u32 left, u32 right) {
    const float left_x = float_at(rt, left);
    const float left_y = float_at(rt, left + 4u);
    const float left_z = float_at(rt, left + 8u);
    const float left_w = float_at(rt, left + 0xCu);
    const float right_x = float_at(rt, right);
    const float right_y = float_at(rt, right + 4u);
    const float right_z = float_at(rt, right + 8u);
    const float right_w = float_at(rt, right + 0xCu);
    // The asm's order: every product first, then the sums (x = f2, y = f1, z = f0, w = f3).
    const float w0 = psp_mul(left_w, right_w);
    const float x0 = psp_mul(left_x, right_x);
    const float y0 = psp_mul(left_z, right_y);
    float x1 = psp_mul(left_z, right_x);
    const float x2 = psp_mul(left_x, right_z);
    float z1 = psp_mul(left_x, right_y);
    const float z2 = psp_mul(left_y, right_x);
    float y1 = psp_mul(left_y, right_z);
    x1 = x1 - x2;
    const float x3 = psp_mul(left_x, right_w);
    const float y2 = psp_mul(left_y, right_y);
    y1 = y1 - y0;
    const float y3 = psp_mul(left_y, right_w);
    const float w1 = w0 - x0;
    const float z3 = psp_mul(left_z, right_w);
    z1 = z1 - z2;
    const float x4 = psp_mul(left_w, right_x);
    const float y4 = psp_mul(left_w, right_y);
    const float w = w1 - y2;
    const float y = (y1 + x3) + x4;
    const float x = (x1 + y3) + y4;
    const float z0 = z1 + z3;
    const float z5 = psp_mul(left_z, right_z);
    const float z6 = psp_mul(left_w, right_z);
    const float z = z0 + z6;
    const float result_w = w - z5;
    put_float(rt, destination + 0u, y);
    put_float(rt, destination + 0xCu, result_w);
    put_float(rt, destination + 4u, x);
    put_float(rt, destination + 8u, z);
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08862438(self, index, source): builds the 0x50-byte record at self + 0x4120 + 0x50 * index from `source` and its
// neighbours, then hands the touched indices to func_08860C80.
//
// Frame: an array object constructed by func_088A79AC(frame, 0) (destroyed by func_088A7548 at the end) with two 0x30-byte
// items at frame +0x48 / +0x78 (func_08897B8C constructs one, func_088978B4 destroys it, in reverse order at the end).
// func_08866510(source + 8, source + 0xC, frame + 0x10, frame, frame + 0x48) fills the record's fields: the half at
// frame +0x10, the signed byte at +0x1B, the words at +0x14 / +0x20 / +0x24 / +0x28, and the 16 bytes at +0x32 and the
// four at +0x2C the record receives. A negative index skips straight to the destruction.
//
// The record: byte +0x17 = 1, +0x18 = (source[0] != 0), +0x19 / +0x1A / +0x1B / +0x1C = 0, +0x44 = source[8],
// +0x48 = source[0xC], +0x1E = the half from the frame, +0x24 = the signed byte, +0x38 / +0x3C / +0x40 = the three words,
// +0x28 = source[0x14], a 0x10-byte copy from frame +0x32 to +6 and a 4-byte one from frame +0x2C to +0, +0x16 = 0 and
// the bytes +4 / +5 = 0xFF. With +0x18 clear, the node at word(word(self + 0x3F50)) + 0x188 * (+0x1E) is told
// func_0885D508 and its byte +0x15 = (the frame word at +0x14 != 0). Then func_088DAA40(self + 0x5AC, index, +0x40),
// func_0885DDD8(self, index, +0x38, +0x3C) answers an object whose +0xC = the record's +0x40, and:
//   * with the frame word at +0x14 set: func_0888A9B8(self + 0x608 + 0x1C38 * word(self + 0x3E78), index, the record + 6,
//     -1, -1, 0, 0);
//   * with it clear and source[0x18] set: func_08860C80(self, index, frame, -1) and straight to the destruction;
//   * with it clear and source[0x18] clear: the record's block (+6, 0x10 bytes and +0..3, +0x16) is copied to the record
//     of source[0x10] when that index is not negative, and then func_08860C80(self, word(frame + 8 + 4 * k), frame,
//     k == 0 ? -1 : index) is called for k in [0, 2) — one call, or two when the copy happened.
// Finally func_088D4D9C(self + 0x5AC + 4) non-zero runs func_088DC784(self + 0x5AC, 1).

// ---------------------------------------------------------------------------------------------------------------------
// 0x08BBAA74(self, actor): returns 0 when self->+8 (the world object) is null. Else graphics set-up calls
// (func_08B56790(1, frame + 0x3C, 0, 0), func_08B55140(0x65, 0xD, frame + 0xC)) and, with pos = func_08AF26F0(actor),
// dir = func_08AF2724(actor) and the world's +0x68 value through func_08AF8904(world, value), the scale 0x08CE9ECC squared
// is the limit base. For each of the count elements of self->+0xC (virtual slot 8; slot 0xC fills a half at frame +0x1C
// and a point pointer at frame +0x24, and takes (the object, index, frame + 0x1C)) the record
// world->+0xA4[world->+0x30[index].half(+0x2C)] is tested: the squared distance from pos must not exceed
// scale^2 x record->+0x20^2 and the dot product with dir must not be below -2 x record->+0x20. A passing element builds a
// transform (func_08B4B114 with the point, a plane height from the plane at frame +0x28, func_08AF52E4's distance; when
// positive func_08B5894C(-0.02 x d), func_08B569A0, func_08AF5394, func_08B566BC x2, func_08B56364(-record +0x24 / +0x28 /
// +0x2C), func_08AF9E88(world, index * 2 + 1, value, 0), func_08B56940, func_08B5894C(0)) and always func_08B569A0,
// func_08B566BC, func_08B56364 (the negated record fields), func_08AF9E88(world, index * 2, value, func_08AF2954(actor))
// and func_08B56940. Last func_08AF8434(world).
//
// Structured rewrite of the emitter-style draft in hot_batch_115 (its HOT115 line is still commented out: with the
// batch-329 set the synthetic gate faults the decompiled run on the frame word at +0x20, which this function reads and
// never writes — a scene needs to fill it, FINDINGS "A callee can read a stale stack slot").

} // namespace
} // namespace new_batch_main_246
} // namespace gt::game

#define NEW246(address, name) GT_DECOMP(address, gt::game::new_batch_main_246::name, gt::ReturnKind::Word)
NEW246(0x08A15800u, func_08A15800);

