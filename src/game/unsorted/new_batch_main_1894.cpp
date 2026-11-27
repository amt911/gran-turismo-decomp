// New batch (main session 1894): functions whose earlier drafts mismatched the original in the gate, rewritten from
// tools.disasm as structured C. Each one carries the gate's finding next to it. Draft batch: not yet verified by the
// differential gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1894 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// The batch files' convention: a guest call whose result becomes v0.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A call with the float arguments in $f12.. like the original's; v0 is the answer (the float results are not read back).
u32 call_f(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
           std::initializer_list<float> float_arguments) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments, float_arguments).v0;
    return ctx.gpr[2];
}
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// 0x0881A708 (a0 = self, a1 = argument): block = allocate(0x1C, 4); 0x0881A5B4(block, argument); local = block;
// 0x08A76AB4(self, &local). The delay slot of the constructor's jal captures the allocator's answer in s0, so the local
// and the sink get the block, never the constructor's v0 (the earlier draft stored the constructor's v0, which is the
// object's vtable: the gate saw a vtable at self+0 and the sink's counters inside .data).
void func_0881A708(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x1Cu, 4u});
    call(rt, ctx, 0x0881A5B4u, {block, argument});
    put(rt, frame.at(0u), block);
    call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08920154 (a0 = self, a1..a3 and t0 = the constructor arguments): 0x08925FE4(self, a1, a2, a3); self+0xC = t0,
// self+0 = vtable 0x08CC5E80, +0x8 = a3, +0x10 = a2. v0 leaves as the vtable constant the asm loads after the call
// (the earlier draft answered the base constructor's v0, 0x08CC6110).
void func_08920154(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08925FE4u, {self, a1, a2, a3});
    put(rt, self + 0xCu, t0);
    put(rt, self, 0x08CC5E80u);
    put(rt, self + 0x8u, a3);
    put(rt, self + 0x10u, a2);
    ctx.gpr[2] = 0x08CC5E80u;
}

// 0x08A3346C (a0 = self): 0x08A76ACC(self, a1, a2, a3); vtable 0x08CD1900; clears +0x17, +0x19..+0x1B, +0x18, the
// halves +0x14 / +0x16 / +0x18 / +0x1A and the word +0x1C, in the asm's order. v0 = the vtable (the earlier draft left
// the base constructor's 0x08CD3C58).
void func_08A3346C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08A76ACCu, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    // The asm keeps self+0x14 in a0 and self+0x18 in v1 and addresses the bytes / halves off those two.
    const u32 low = self + 0x14u;
    const u32 high = self + 0x18u;
    put(rt, self, 0x08CD1900u);
    put_byte(rt, low + 3u, 0u);
    put_byte(rt, high + 3u, 0u);
    put_byte(rt, high + 1u, 0u);
    put_byte(rt, high + 2u, 0u);
    put_byte(rt, self + 0x18u, 0u);
    put_half(rt, self + 0x14u, 0u);
    put(rt, self + 0x1Cu, 0u);
    put_half(rt, low + 6u, 0u);
    put_half(rt, low + 2u, 0u);
    put_half(rt, low + 4u, 0u);
    ctx.gpr[2] = 0x08CD1900u;
}

// 0x08A4449C (a0 = self, a1 / a2 = words, a3 = a byte): 0x08A76ACC(self, a1, a2, a3); self+0x1C = a3 & 0xFF,
// self+0 = vtable 0x08CBE340, +0x14 = a1, +0x18 = a2. v0 = the vtable (the earlier draft left the base's 0x08CD3C58).
void func_08A4449C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A76ACCu, {self, a1, a2, a3});
    put_byte(rt, self + 0x1Cu, a3 & 0xFFu);
    put(rt, self, 0x08CBE340u);
    put(rt, self + 0x14u, a1);
    put(rt, self + 0x18u, a2);
    ctx.gpr[2] = 0x08CBE340u;
}

// 0x0884C52C (a0 = self, a1..a3 and t0 = the constructor arguments): 0x08A76ACC(self, a1, a2, a3); self+0 = vtable
// 0x08CBD348, +0x18 = a2, +0x1C = a3, +0x20 = t0 (half), +0x22 = 0 (half). v0 = the vtable (the earlier draft left the
// base constructor's 0x08CD3C58).
void func_0884C52C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    const u32 a3 = ctx.gpr[7];
    const u32 t0 = ctx.gpr[8];
    GuestFrame frame(ctx, 0x20u);
    call(rt, ctx, 0x08A76ACCu, {self, a1, a2, a3});
    put(rt, self, 0x08CBD348u);
    put(rt, self + 0x18u, a2);
    put(rt, self + 0x1Cu, a3);
    put_half(rt, self + 0x20u, t0);
    put_half(rt, self + 0x22u, 0u);
    ctx.gpr[2] = 0x08CBD348u;
}

// 0x08A87E8C (a0 = self, f12 = float): block = allocate(0x18, 4); 0x08A87708(block, f12); local = block;
// 0x08A76AB4(self, &local). As in 0x0881A708 the delay slot of the constructor's jal captures the allocator's answer in
// s0, so the local is the block, not the constructor's v0 (0x08CBBF20, the new object's vtable: the gate saw that vtable
// at self+0 and a counter bumped inside .rodata).
void func_08A87E8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float value = ctx.fpr[12];
    GuestFrame frame(ctx, 0x30u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call_f(rt, ctx, 0x08A87708u, {block}, {value});
    put(rt, frame.at(0u), block);
    call(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// 0x08A4EE1C (a0 = object, a1 = a two-word kind descriptor, a2 = a flag byte): the per-frame update of the state row at
// +0x90. The kind at info+0 picks the path; each path moves the row by a step taken from the object's own fields, calls
// the row helpers (0x08A4CEDC / 0x08A4CF24 move it and answer a status, 0x08A4E6F4 resets, 0x08A4EE08 sets its flag) and,
// on the common path, appends one 16-byte entry to the ring at +0x98 (count at +0x9C, step at +0x20/+0x24, interpolation
// factors f21/f22) before asking 0x08A4CEDC once more. v0 = the final mode test on the word at +0x58, 0 on every earlier
// return (the asm ends every one of them with `a0 = 0`).
// The 0x08A4F214 re-centre block runs when the descriptor's kind is an ASCII digit and the mode has no 0x800 bit, or when
// it is not a digit and the mode has no 0x400 bit.
// The ring is reset (0x08A4E6F4) only when the count is already at its 0x100 limit: `slti v0, v1, 0x100` + `bnel v0, zero`
// skips the reset while the count is below it. The earlier draft had that test inverted, so it reset a live ring and
// wrote the entry past a re-based one (the gate saw the descriptor and the half of the entry at 0x09EDD0E4 differ).
void func_08A4EE1C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 info = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x40u);
    // Loaded in the delay slot of the first branch: every path uses it.
    float f20 = float_at(rt, self + 0x64u);
    const u32 kind = rt.memory().load16(info);

    if (kind == 10u) {
        if (float_at(rt, self + 0x28u) != 0.0f) {
            call(rt, ctx, 0x08A4EE08u, {self, flag});
            ctx.gpr[2] = 0u;
            return;
        }
        // a1 still holds 10 at this call: 0x08A4CF24 takes it as the second argument.
        const float step = (float_at(rt, self + 0x68u) + 14.0f) * float_at(rt, self + 0x24u) * 0.5f;
        call_f(rt, ctx, 0x08A4CF24u, {self, 10u}, {step});
        ctx.gpr[2] = 0u;
        return;
    }

    if (kind == 0x20u) {
        // a0 takes the mode word in this path (lw a0, 0x58(a0)).
        const u32 mode = word(rt, self + 0x58u);
        if ((mode & 0x300u) == 0x200u) {
            const u32 row = rt.memory().load16(self + 0x90u);
            if (row != 0u && row != 10u && float_at(rt, self + 0x28u) == 0.0f) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
        const float base = (mode & 0x400u) != 0u ? 5.6f : 14.0f;
        const u32 status = call_f(rt, ctx, 0x08A4CEDCu, {self, 10u}, {(base + f20) * float_at(rt, self + 0x20u)});
        if (status != 0u) {
            call(rt, ctx, 0x08A4E6F4u, {self, 0u, flag});
        } else {
            call(rt, ctx, 0x08A4EE08u, {self, flag});
        }
        ctx.gpr[2] = 0u;
        return;
    }

    if (kind == 0x3000u) {
        const u32 mode = word(rt, self + 0x58u);
        if ((mode & 0x300u) == 0x200u) {
            const u32 row = rt.memory().load16(self + 0x90u);
            if (row != 0u && row != 10u && float_at(rt, self + 0x28u) == 0.0f) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
        const u32 status = call_f(rt, ctx, 0x08A4CEDCu, {self, 10u}, {(14.0f + f20) * float_at(rt, self + 0x20u)});
        if (status != 0u) {
            call(rt, ctx, 0x08A4E6F4u, {self, 0u, flag});
        } else {
            call(rt, ctx, 0x08A4EE08u, {self, flag});
        }
        ctx.gpr[2] = 0u;
        return;
    }

    const u32 mode = word(rt, self + 0x58u);
    if (kind == 0xB6u || kind == 0xA0u) {
        const float base = (mode & 0x400u) != 0u ? 5.6f : 14.0f;
        const u32 status = call_f(rt, ctx, 0x08A4CEDCu, {self, 10u}, {(base + f20) * float_at(rt, self + 0x20u)});
        if (status == 0u) call(rt, ctx, 0x08A4E6F4u, {self, 1u, flag});
        ctx.gpr[2] = 0u;
        return;
    }

    // Every other kind: three lookups on the table at 0x08CDF4A8 compare the row at +0x90 and the descriptor against the
    // entries of 0x08CDF4F0 (+0x7C) and 0x08CDF56C; on a match the code resets the row (0x08A4E6F4) or clears +0xA0.
    const u32 row = self + 0x90u;
    bool reset = false;
    if ((mode & 0x300u) == 0x200u && rt.memory().load16(row) != 0u) {
        if (call(rt, ctx, 0x08C1CCD0u, {0x08CDF4A8u, 0x08CDF4F0u, row}) == 0u &&
            rt.memory().load16(row + 4u) != 0u) {
            // 0x08A4F0CC: the descriptor against the first entry, the row against the second.
            if (call(rt, ctx, 0x08C1CCD0u, {0x08CDF4A8u, 0x08CDF4F0u, info}) != 0u) {
                reset = true;
            } else if (rt.memory().load16(info + 4u) == 0u) {
                reset = true;
            }
        } else {
            // 0x08A4F07C: a second descriptor lookup; a non-zero half at +4 clears +0xA0 instead.
            if (call(rt, ctx, 0x08C1CCD0u, {0x08CDF4A8u, 0x08CDF4F0u, info}) != 0u) {
                if (call(rt, ctx, 0x08C1CCD0u, {0x08CDF4F0u, 0x08CDF56Cu, row}) != 0u) reset = true;
            } else if (rt.memory().load16(info + 4u) != 0u) {
                put(rt, self + 0xA0u, 0u);
            } else if (call(rt, ctx, 0x08C1CCD0u, {0x08CDF4F0u, 0x08CDF56Cu, row}) != 0u) {
                reset = true;
            }
        }
    }
    if (reset) call(rt, ctx, 0x08A4E6F4u, {self, 0u, flag});
    const u32 half = rt.memory().load16(info + 4u);

    // The step factors: f21 along the row, f22 across it. Kind 2 with a live member at +0xA4 takes them from that member's
    // record (0x08A4DC8C answers a pointer, +6 a signed half); every other case scales the word the first lookup filled at
    // sp+4 by 14 / (float)(s32)word, with the record's signed half at +4 (0.015625 = 1/64).
    float f21 = 0.0f;
    float f22 = 0.0f;
    float f1 = 0.0f;
    float f5 = 0.0f;
    if (half == 2u && word(rt, self + 0xA4u) != 0u) {
        f22 = 13.6f;
        f21 = 0.7f;
        const u32 record = call(rt, ctx, 0x08A4DC8Cu, {word(rt, self + 0xA4u), rt.memory().load16(info)});
        f5 = static_cast<float>(signed_half(rt, record + 6u)) * 0.7f;
        f1 = f5;
    } else {
        const u32 member = word(rt, self + 0x78u);
        call(rt, ctx, 0x08A50634u, {member, info, frame.at(0u), frame.at(4u)});
        const u32 record = call(rt, ctx, 0x08A50288u, {member, info});
        const float divisor = 14.0f / static_cast<float>(static_cast<s32>(word(rt, frame.at(4u))));
        const float scaled = static_cast<float>(signed_half(rt, record + 4u)) * 0.015625f;
        f21 = divisor * static_cast<float>(signed_byte(rt, record));
        f5 = divisor * scaled;
        f22 = divisor * static_cast<float>(signed_byte(rt, record + 1u));
        f1 = divisor * static_cast<float>(signed_byte(rt, record + 2u));
    }

    // The row's mode word again; the digit test asks whether info+0 holds an ASCII digit.
    const u32 state = word(rt, self + 0x58u);
    const bool high = (state & 0x400u) != 0u;
    const bool top = (state & 0x800u) != 0u;
    const bool digit = rt.memory().load16(info) - 0x30u < 10u;
    float f4;
    if (digit ? !top : !high) {
        f21 = (14.0f - f1) * 0.5f;
        f4 = 14.0f + f20;
    } else {
        f4 = f5 + f20;
    }
    const float scale = float_at(rt, self + 0x20u);
    const float limit_low = (float_at(rt, self + 0x50u) - float_at(rt, self + 0x48u)) - scale * 14.0f;
    f20 = f4 * scale;
    const float limit_high = f20 + float_at(rt, self + 0x28u);
    if (limit_low <= limit_high && (state & 0x300u) == 0x200u) {
        if (call(rt, ctx, 0x08C1CCD0u, {0x08CDF4A8u, 0x08CDF4F0u, info}) != 0u) {
            call(rt, ctx, 0x08A4EE08u, {self, flag});
        } else {
            put(rt, frame.at(4u), word(rt, self + 0x70u));
            if (call(rt, ctx, 0x08C1CD74u, {0x08CDF4F0u, 0x08CDF56Cu, frame.at(4u)}) != 0u) {
                call(rt, ctx, 0x08A4E6F4u, {self, 1u, flag});
            }
        }
    }

    u32 count = word(rt, self + 0x9Cu);
    if (static_cast<s32>(count) >= 0x100) {
        call(rt, ctx, 0x08A4E6F4u, {self, 0u, flag});
        count = word(rt, self + 0x9Cu);
    }
    const u32 entry = word(rt, self + 0x98u) + (count << 4);
    put(rt, self + 0x9Cu, count + 1u);
    put_float(rt, entry, float_at(rt, self + 0x28u) + f21 * float_at(rt, self + 0x20u));
    put_float(rt, entry + 4u, float_at(rt, self + 0x2Cu) - f22 * float_at(rt, self + 0x24u));
    put(rt, entry + 8u, word(rt, info));
    put_half(rt, entry + 0xCu, rt.memory().load16(info + 4u));
    if (call_f(rt, ctx, 0x08A4CEDCu, {self}, {f20}) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08A4E6F4u, {self, 1u, flag});
    ctx.gpr[3] = 0x200u;  // v1 = the mode mask the last test compares with
    ctx.gpr[2] = (word(rt, self + 0x58u) & 0x300u) != 0x200u ? 1u : 0u;
}

} // namespace
} // namespace new_batch_main_1894
} // namespace gt::game

// Registration: one line per drafted address.
#define NEW1894(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1894::name, gt::ReturnKind::kind)
NEW1894(0x0881A708u, func_0881A708, Word);
NEW1894(0x08920154u, func_08920154, Word);
NEW1894(0x08A3346Cu, func_08A3346C, Word);
NEW1894(0x08A4449Cu, func_08A4449C, Word);
NEW1894(0x08A87E8Cu, func_08A87E8C, Word);
NEW1894(0x0884C52Cu, func_0884C52C, Word);
NEW1894(0x08A4EE1Cu, func_08A4EE1C, Word);
