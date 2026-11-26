// New batch 38 (main session): draft, unverified. Structured C from `tools.disasm`.
// Each body is written by hand from the disassembly of the original function: named locals, guest memory through the
// batch helpers, `G[n]`-free (the context's registers only where the call convention needs them).
// Before registering anything: the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_38 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// The bit pattern of an FPU constant the original materialises with lui/ori: exact, never a decimal literal.
constexpr float f32(u32 bits) { return std::bit_cast<float>(bits); }

// A guest call; afterwards v0 holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// A guest call with float arguments in $f12..; returns v0 and leaves v0/v1/f0 as the callee left them.
u32 call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// --- 0x08A06FC0 ---
// With a0 == 1 and a1 == 0xFFFF: registers seventeen name/constant table entries (a word pair copied out of each
// source pair into a stack slot, then one four-argument call per entry). v0 is the constant the guard compared
// on the two early exits, else the last call's result.
void func_08A06FC0(Runtime &rt, AllegrexContext &ctx) {
    if (ctx.gpr[4] != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (ctx.gpr[5] != 0xFFFFu) {
        ctx.gpr[2] = 0xFFFFu;
        return;
    }
    GuestFrame frame(ctx, 0xA0u);
    call(rt, ctx, 0x08B2095Cu, {0x08CF5F5Cu, 0x08C8DDACu, 3u});
    call(rt, ctx, 0x08A92114u, {0x08CF5E30u, 0x08CA8F54u, 0x1B0u, 0x08A06E20u, 0x08A09268u, 0x08A06E44u});
    call(rt, ctx, 0x0897D534u, {0x08CF5E20u, 0x08CA8F54u, 0x08A09510u, 0x08A08F38u});

    struct Entry {
        u32 source;  // the word pair copied to the stack slot
        u32 slot;    // frame offset of that slot (two words)
        u32 name;    // a0 of the registration call
        u32 key;     // a1 of the registration call
    };
    static constexpr Entry kEntries[] = {
        {0x08C8DE50u, 0x80u, 0x08CF5F4Cu, 0x08CA8F60u}, {0x08C8DE48u, 0x78u, 0x08CF5F3Cu, 0x08CA8F70u},
        {0x08C8DE40u, 0x70u, 0x08CF5F2Cu, 0x08CA8F7Cu}, {0x08C8DE38u, 0x68u, 0x08CF5F1Cu, 0x08CA8F88u},
        {0x08C8DE30u, 0x60u, 0x08CF5F0Cu, 0x08CA8F98u}, {0x08C8DE28u, 0x58u, 0x08CF5EFCu, 0x08CA8FA0u},
        {0x08C8DE20u, 0x50u, 0x08CF5EECu, 0x08CA8FA8u}, {0x08C8DE18u, 0x48u, 0x08CF5EDCu, 0x08CA8FB0u},
        {0x08C8DE10u, 0x40u, 0x08CF5ECCu, 0x08CA8FC0u}, {0x08C8DE08u, 0x38u, 0x08CF5EBCu, 0x08CA8FD0u},
        {0x08C8DE00u, 0x30u, 0x08CF5EACu, 0x08CA8FE8u}, {0x08C8DDF8u, 0x28u, 0x08CF5E9Cu, 0x08CA9004u},
        {0x08C8DDF0u, 0x20u, 0x08CF5E8Cu, 0x08CA9014u}, {0x08C8DDE8u, 0x18u, 0x08CF5E7Cu, 0x08CA902Cu},
        {0x08C8DDE0u, 0x10u, 0x08CF5E6Cu, 0x08CA9044u}, {0x08C8DDD8u, 0x08u, 0x08CF5E5Cu, 0x08CA9060u},
        {0x08C8DDD0u, 0x00u, 0x08CF5E4Cu, 0x08CA9078u},
    };
    for (const Entry &entry : kEntries) {
        put(rt, frame.at(entry.slot), word(rt, entry.source));
        put(rt, frame.at(entry.slot + 4u), word(rt, entry.source + 4u));
        call(rt, ctx, 0x08C12044u, {entry.name, entry.key, frame.at(entry.slot), 0x08CF5E20u});
    }
}

// --- 0x0886ECD4 ---
// A camera/scene command dispatcher: a0 is the scene object, a1 an object whose first word leads to a callback,
// a2 the command text. The command is reduced to a four-character tag (0x08AC2690) and dispatched. v0 is the
// first call's result when it is nonzero; otherwise the tag constant the failing comparison left in v0, or the
// last handler's call result.

// DCOP / DCOT / CMOF: token 0 into one stack buffer, its payload into [sp] through 0x08AC2910, token 1 into a
// second buffer, its payload into [sp + 0x3C] through 0x08AC2A8C, then the scene, the payload byte and the
// second payload to `target`. v0 = target's result.
void dispatch_token_pair(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self, u32 command,
                         u32 first_offset, u32 second_offset, u32 target) {
    const u32 first = frame.at(first_offset);
    call(rt, ctx, 0x08AC2730u, {first, command, 0u});
    call(rt, ctx, 0x08AC2910u, {first, frame.at(0u), 0u});
    call(rt, ctx, 0x08AC26DCu, {first});
    const u32 second = frame.at(second_offset);
    call(rt, ctx, 0x08AC2730u, {second, command, 1u});
    call(rt, ctx, 0x08AC2A8Cu, {second, frame.at(0x3Cu), 0u});
    call(rt, ctx, 0x08AC26DCu, {second});
    call(rt, ctx, target,
         {word(rt, self + 0x70u), rt.memory().load8(frame.at(0u)), word(rt, frame.at(0x3Cu))});
}

// POSX / POSY / POSZ: the same two-token read into two stack slots; the second token's payload pointer (v0 of
// 0x08AC2900) goes to 0x0886CBE8 with the scene part.
void dispatch_position(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self, u32 command,
                       u32 first_offset, u32 second_offset) {
    const u32 first = frame.at(first_offset);
    call(rt, ctx, 0x08AC2730u, {first, command, 0u});
    call(rt, ctx, 0x08AC28E8u, {first});  // a float the original drops
    const u32 second = frame.at(second_offset);
    call(rt, ctx, 0x08AC26DCu, {first});
    call(rt, ctx, 0x08AC2730u, {second, command, 1u});
    const u32 payload = call(rt, ctx, 0x08AC2900u, {second});
    call(rt, ctx, 0x08AC26DCu, {second});
    call(rt, ctx, 0x0886CBE8u, {word(rt, self + 0x1Cu), frame.at(0x40u), payload});
}

// DCIM: with the object part present, the callback at word(word(a1) + 0xC) runs with the command's token length,
// then the scene's byte +0x7C is cleared, a bitfield in the stack word at +0x40 is set to 2 and the pair goes on
// to 0x0886F854 / 0x0886F894. v0 is the tag when the second object part is null, else 0x0886F894's result.
void dispatch_image(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self, u32 arg, u32 command,
                    u32 part2, u32 part5) {
    if (part5 == 0u) {
        ctx.gpr[2] = 0x4443494Du;
        return;
    }
    if (part2 != 0u) {
        const u32 callback = word(rt, word(rt, arg) + 0xCu);  // read in the 0x08AC26BC call's delay slot
        const u32 length = call(rt, ctx, 0x08AC26BCu, {command});
        const u32 ignored = call_floats(rt, ctx, callback, {arg, length}, {});
        (void)ignored;
        const u32 scene = word(rt, self + 0x70u);
        ctx.fpr[12] = float_at(rt, scene + 0x80u);
        ctx.fpr[13] = ctx.fpr[0];
        ctx.fpr[14] = float_at(rt, scene + 0x84u);
        call_floats(rt, ctx, 0x088725E4u, {part2, rt.memory().load8(scene + 0x7Cu)},
                    {ctx.fpr[12], ctx.fpr[13], ctx.fpr[14]});
    }
    const u32 scene = word(rt, self + 0x70u);
    rt.memory().store8(scene + 0x7Cu, 0u);
    const u32 flags = word(rt, frame.at(0x40u));
    put(rt, frame.at(0x40u), (flags & ~0xE0u) | (2u << 5));
    call(rt, ctx, 0x0886F854u, {frame.at(0x40u), arg, command});
    call(rt, ctx, 0x0886F894u, {part5, frame.at(0x40u)});
}


// --- 0x088BFA4C ---
// A per-frame tick: a0 is the object, a1 a byte flag. It counts down the halfword at +0xC of the sub-object at
// a0+0x24, dispatches on its +0x10 mode byte, then drives three float thresholds (+0x20, +0x28, +0x2C) from
// +0x44 / +0x60 and the game's frame data, clamping +0x28 into [0, 1]. v0 is what the original leaves: the mode
// constant on the early returns, the countdown-derived halfword sign on the "no further work" path, or 6 / the
// +0x52 byte; otherwise the function ends with a tail call to 0x088BF978.
void func_088BFA4C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];              // s2
    const u32 flag = ctx.gpr[5] & 0xFFu;      // a1, byte-tested several times
    const u32 s3 = self + 4u;
    {
        GuestFrame frame(ctx, 0x30u);
        const u32 s0 = word(rt, self + 0x24u);
        u32 v0 = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(s0 + 0xCu))));
        if (static_cast<std::int32_t>(v0) > 0) {
            v0 -= 1u;
            rt.memory().store16(s0 + 0xCu, static_cast<std::uint16_t>(v0));
        }
        const u32 a3 = rt.memory().load8(s0 + 0x10u);
        u32 s1 = 0xFFFFFFFFu;
        const u32 mode = a3 & 0xFFu;
        if (flag == 0u) s1 = 1u;
        if (((mode - 8u) & 0xFFu) < 2u) {  // 8 or 9: nothing to do, v0 = the sltiu result
            ctx.gpr[2] = 1u;
            return;
        }
        if (mode == 4u) {  // v0 = 3, set in the comparison's delay slot
            ctx.gpr[2] = 3u;
            return;
        }
        std::uint16_t a2 = rt.memory().load16(s0 + 0xCu);
        if (mode == 3u) {
            // The fast-forward test: only some of the +0x48 flag bits run the countdown/slowing block.
            const u32 flags = rt.memory().load16(s0 + 0x48u);
            bool run;
            if ((flags & 0x18u) != 0u && (flags & 0x6u) != 0u) {
                run = true;
            } else {
                run = (flags & 0x30u) != 0u && (flags & 0xCu) != 0u;
            }
            if (run) {
                const float limit = f32(0x41055556u);
                if (float_at(rt, word(rt, s3 + 4u) + 0x6C4u) < limit) {  // 4(s3)
                    a2 = rt.memory().load16(s0 + 0xCu);
                    s1 = 0u;
                    const u32 next = static_cast<u32>(a2) + 1u;
                    a2 = static_cast<std::uint16_t>(next);
                    rt.memory().store16(s0 + 0xCu, static_cast<std::uint16_t>(next));
                } else {
                    a2 = rt.memory().load16(s0 + 0xCu);
                    const float scaled = float_at(rt, s0 + 0x28u) * 0.5f;
                    s1 = 0u;
                    put_float(rt, s0 + 0x28u, scaled);
                }
            } else {
                s1 = 0u;
            }
        }
        // 088BFB54: the mode's bit in a 6-bit set, then the sign of the countdown halfword.
        v0 = rt.memory().load8(s0 + 0x6Eu) < 0x3Cu ? 1u : 0u;
        const u32 sub = rt.memory().load8(s0 + 0x96u);
        if (v0 == 0u) s1 = 0u;
        if (sub < 6u) {
            const u32 bit = 1u << sub;
            if ((bit & 0x26u) != 0u) {
                s1 = 0u;
            } else if ((bit & 0x18u) != 0u) {
                const u32 masked = bit & 0x18u;
                if ((masked & 0xFFu) < 2u) s1 = 0xFFFFFFFFu;
            }
        }
        v0 = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(a2)));  // seh v0, a2

        // 088BFBA8: three entries converge here (sign of the halfword, or the flag byte being zero).
        float f20 = float_at(rt, s0 + 0x44u);
        bool to_fc8c = false;
        if (static_cast<std::int32_t>(v0) > 0 || flag != 0u) {  // 088BFBB0: beql a1, zero goes straight to 088BFCB8
            if (f32(0x3F666666u) < f20) {  // 0.9 < f20
                const float f21 = float_at(rt, s0 + 0x60u);  // the call's delay slot
                const std::int8_t signed_flag = static_cast<std::int8_t>(rt.memory().load8(s0 + 0x51u));
                const u32 found = call(
                    rt, ctx, 0x088B29FCu,
                    {word(rt, self), static_cast<u32>(static_cast<std::int32_t>(signed_flag))});
                float f2 = 0.0f;
                bool close = true;
                if (found != 0u) {
                    const u32 inner = word(rt, found + 0xCu);
                    f2 = float_at(rt, inner + 0x6B4u) - float_at(rt, word(rt, s3 + 4u) + 0x6B4u);  // 4(s3)
                    close = !(f2 < f32(0xC09CCCCDu));  // -4.9
                }
                if (close) {
                    // 088BFC68
                    if (f32(0x3E99999Au) < f21) {  // 0.3 < f21
                        a2 = rt.memory().load16(s0 + 0xCu);
                        v0 = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int16_t>(a2)));
                        to_fc8c = true;
                    }
                } else {
                    const float factor = float_at(rt, word(rt, s3) + 0x13ACu) * -2.0f;
                    float raised = f2 * factor;
                    if (1.0f < raised) raised = 1.0f;
                    if (float_at(rt, s0 + 0x2Cu) < raised) put_float(rt, s0 + 0x2Cu, raised);
                    f20 = 1.0f;  // 088BFE3C
                }
            } else {
                to_fc8c = true;  // 088BFBCC: the 0.9 test failed
            }
        }
        if (to_fc8c && v0 == 0u) {  // 088BFC8C: a non-zero halfword keeps f20
            f20 -= 0.5f;
            if (f20 < 0.0f) f20 = 0.0f;
        }

        // 088BFCB8
        const float target = float_at(rt, s0 + 0x20u);  // f1
        float f0;
        if (0.0f < target) {
            f0 = 1.0f - f20;
            s1 = 0u;
        } else if (static_cast<std::int32_t>(s1) > 0) {
            // 088BFCE0
            const u32 step = call(rt, ctx, 0x088C4A08u, {self});
            if (step != 0u) {
                f0 = static_cast<float>(0x64u - step) * f32(0x3C23D70Au) * f32(0x3ECCCCCDu);
            } else {
                f0 = f32(0x3ECCCCCDu);  // 0.3
            }
            if (f0 < f20) {
                const u32 mode_byte = rt.memory().load8(s0 + 0x10u);
                const u32 reached = call(rt, ctx, 0x088BBA00u, {mode_byte, self});
                if (static_cast<std::int32_t>(reached) >= 0) {
                    call(rt, ctx, 0x088BF978u, {reached, 0u, self});
                    rt.memory().store16(s0 + 0xCu, 0xB4u);
                    f0 = 1.0f;
                } else {
                    f0 = 1.0f - f20;
                }
            } else {
                f0 = 1.0f - f20;
            }
        } else {
            f0 = 1.0f - f20;  // 088BFD74
        }

        // 088BFDA0: +0x28 = clamp(+0x28 * f0, 0, 1)
        float scaled_28 = float_at(rt, s0 + 0x28u) * f0;
        put_float(rt, s0 + 0x28u, scaled_28);
        if (scaled_28 < 0.0f) {
            scaled_28 = 0.0f;
            put_float(rt, s0 + 0x28u, 0.0f);
        }
        if (1.0f < scaled_28) put_float(rt, s0 + 0x28u, 1.0f);

        if (s1 == 0u) {  // 088BFDDC
            ctx.gpr[2] = v0;
            return;
        }
        if ((rt.memory().load16(s0 + 0x48u) & 0x3Fu) != 0u) {
            ctx.gpr[2] = 6u;  // the delay slot's constant
            return;
        }
        if (rt.memory().load8(s0 + 0x10u) == 6u) {
            ctx.gpr[2] = 6u;
            return;
        }
        const u32 byte52 = rt.memory().load8(s0 + 0x52u);
        if (byte52 != 0u) {
            ctx.gpr[2] = byte52;
            return;
        }
        ctx.gpr[2] = byte52;  // 0 at the jump: the callee starts with this v0
    }
    // 088BFE34: the frame is popped in the jump's delay slot.
    tail_call(ctx, 0x088BF978u, {6u, 0u, self});
}

} // namespace
} // namespace new_batch_main_38
} // namespace gt::game

#define NEW38(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_38::name, gt::ReturnKind::kind)
NEW38(0x08A06FC0u, func_08A06FC0, Word);
NEW38(0x088BFA4Cu, func_088BFA4C, Word);
