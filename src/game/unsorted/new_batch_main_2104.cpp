// New batch main-2104 (functions the extra seeds made emittable). Hand-written structured C from `tools.disasm`, not gated yet.
//   0x089283AC: sets four state flags of an object by the word at +0x1D1C and runs the transition the word at +0x1FB8 selects.
//   0x089344D8: compares a struct against the canonical values two 0x08936584 fills and one 0x089368DC checks; 0 all-equal, 1 otherwise.
//   0x0896301C: builds a counted holder from an {object, block} pair and assigns it to the two pairs out+0 / out+4 and out+8 / out+0xC.
//   0x088F20E4: a float geometry pass over the settings block a1 (a0 = the renderer, f12 = a scale).
//   0x08862C90: a constructor over the object a0 (a1 is a callback table), all through calls and virtuals.
// Every delay slot of a `jal` / `jalr` runs before the call; a `bnel` / `beql` / `bc1fl` / `bc1tl` delay slot only when the branch is taken.
// v0 is set as the asm leaves it on every path. The callee-saved registers are stored where the prologue does and given back on return.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2104 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose v0 / v1 / f0 stay in the context as the original's do (a callee that sets no v0 hands the previous one on).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, address, arguments, {});
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}
// The same with float arguments in f12-f19.
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 19u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original sets them.
GuestResult call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}

float bits_float(u32 value) { return std::bit_cast<float>(value); }

// Where the prologue of a function stores a callee-saved register: `reg` 0-31 is a general register, 32-63 a floating one (32 + its number).
struct SavedSlot {
    unsigned reg;
    u32 offset;
};
// The callee-saved registers (s0-s7, fp, f20-f31) the function keeps values in: the prologue's stores go where the asm puts them (a
// callee can read the stack below its caller's frame) and the caller's values come back on return.
class SavedRegisters {
public:
    SavedRegisters(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots) : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
        fp_ = ctx.gpr[30];
        for (unsigned i = 0; i < 12; ++i) fpr_[i] = ctx.fpr[20 + i];
        for (const SavedSlot &slot : slots) {
            if (slot.reg < 32u) put(rt, frame.at(slot.offset), ctx.gpr[slot.reg]);
            else put_float(rt, frame.at(slot.offset), ctx.fpr[slot.reg - 32u]);
        }
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
        ctx_.gpr[30] = fp_;
        for (unsigned i = 0; i < 12; ++i) ctx_.fpr[20 + i] = fpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

// ---------------------------------------------------------------------------------------------------------------
// 0x089283AC (a0 = the object, a1 handed on to 0x089275B0 / 0x08927EAC / 0x089200CC). 0x08916474 returns the
// state table, 0x089161A0 stores its byte argument a3 at a2 + 0x08CF2064; the four (index, byte) pairs the word at
// +0x1D1C picks are written first, then the word at +0x1FB8 selects the transition.
void set_state_pairs(Runtime &rt, AllegrexContext &ctx, const u32 (&pairs)[4][2]) {
    for (const auto &pair : pairs) {
        const u32 table = call_set(rt, ctx, 0x08916474u, {}).v0;
        call_set(rt, ctx, 0x089161A0u, {table, 0u, pair[0], pair[1]});
    }
}
void func_089283AC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];  // s4
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {17u, 0x4u}, {18u, 0x8u}, {19u, 0xCu}, {20u, 0x10u}, {31u, 0x14u}});
    ctx.gpr[2] = 0xAu;
    if (word(rt, self + 0x1FB8u) == 0xAu) {
        const u32 state = word(rt, self + 0x1D1Cu);
        if (state == 1u || state == 2u) {
            set_state_pairs(rt, ctx, {{1u, 1u}, {0u, 0u}, {2u, 0u}, {3u, 0u}});
        } else if (state == 3u) {
            set_state_pairs(rt, ctx, {{1u, 0u}, {0u, 0u}, {2u, 1u}, {3u, 0u}});
        } else if (state == 4u) {
            set_state_pairs(rt, ctx, {{1u, 0u}, {0u, 0u}, {2u, 0u}, {3u, 1u}});
        } else {
            set_state_pairs(rt, ctx, {{1u, 0u}, {0u, 1u}, {2u, 0u}, {3u, 0u}});
        }
    } else {
        set_state_pairs(rt, ctx, {{0u, 0u}, {1u, 0u}, {2u, 0u}, {3u, 0u}});
    }
    const u32 phase = word(rt, self + 0x1FB8u);
    if (phase == 0xBu) {
        if (call_set(rt, ctx, 0x089273B4u, {self}).v0 != 0u) return;  // v0 = busy
        const u32 object = call_set(rt, ctx, 0x089273D8u, {self}).v0;
        if (call_virtual_at(rt, ctx, object, 0xCu, {object, 0x20u}).v0 == 0u) return;  // v0 = 0
        tail_call(ctx, 0x089281E4u, {self, 3u});
        return;
    }
    if (phase != 0u && phase != 3u) {
        ctx.gpr[2] = 0xBu;
        return;
    }
    if (call_set(rt, ctx, 0x089273B4u, {self}).v0 != 0u) {
        if (phase != 0u && word(rt, self + 0x1FCu) == 0x22u) {
            ctx.gpr[2] = 0x22u;
            return;
        }
        tail_call(ctx, 0x08927D14u, {self, self + 0x2184u});
        return;
    }
    const u32 previous = call_set(rt, ctx, 0x089275B0u, {self, other}).v0;  // s3
    const u32 step = call_set(rt, ctx, 0x08927EACu, {self, other}).v0;      // s0
    u32 mode = 1u;  // s2
    u32 answer = 0u;  // v1
    if (step != 1u) {
        answer = call_set(rt, ctx, 0x08927E4Cu, {self}).v0;
        mode = word(rt, self + 0x2050u);
    }
    const bool same = previous == 1u;  // s3
    if (same || answer == 1u || rt.memory().load8(self + 0x2054u) != step) {
        if (mode == 0u) call_set(rt, ctx, 0x089281E4u, {self, 3u});
        else call_set(rt, ctx, 0x089274C8u, {self, self + 0x2194u});
    }
    ctx.gpr[2] = 1u;
    rt.memory().store8(self + 0x2054u, static_cast<std::uint8_t>(step));
    if (mode != 1u) return;
    const u32 slot = self + 0x2194u;  // s0
    call_set(rt, ctx, 0x08927D14u, {self, slot});
    if (same) {
        put(rt, slot + 8u, word(rt, slot + 0xCu));
        call_set(rt, ctx, 0x089274C8u, {self, slot});
        call_set(rt, ctx, 0x089200CCu, {self, other, word(rt, self + 0x1FB0u)});
    }
    tail_call(ctx, 0x08927BE4u, {self, slot, 0u, 0u});
}

// ---------------------------------------------------------------------------------------------------------------
// 0x089344D8 (a0 = the struct to check): 0x089363FC gives a context, 0x08936584 fills a block of canonical values
// (a2 / a3 are the first two words of the struct, t0 the destination) and 0x089368DC checks the pair; the rest is a
// chain of word compares. v0 = 0 when every field matches, 1 on the first mismatch.
void func_089344D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];  // s0
    GuestFrame frame(ctx, 0x100u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0xF0u}, {31u, 0xF4u}});
    const u32 first = call_set(rt, ctx, 0x089363FCu, {}).v0;
    call_set(rt, ctx, 0x08936584u, {first, ctx.gpr[5], word(rt, self), word(rt, self + 4u), frame.at(0u)});
    if (word(rt, frame.at(0u)) == 0xFFFFFFFFu && word(rt, frame.at(4u)) == 0xFFFFFFFFu) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 second = call_set(rt, ctx, 0x089363FCu, {}).v0;
    if (call_set(rt, ctx, 0x089368DCu, {second, ctx.gpr[5], word(rt, frame.at(0u)), word(rt, frame.at(4u))}).v0 == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 third = call_set(rt, ctx, 0x089363FCu, {}).v0;
    call_set(rt, ctx, 0x08936584u, {third, ctx.gpr[5], word(rt, frame.at(0u)), word(rt, frame.at(4u)), frame.at(0x20u)});
    auto mismatch = [&](u32 self_offset, u32 frame_offset) { return word(rt, self + self_offset) != word(rt, frame.at(frame_offset)); };
    const bool differs = mismatch(0x18u, 0x20u) || mismatch(0x1Cu, 0x24u) || mismatch(0x20u, 0x28u) || mismatch(0x24u, 0x2Cu) ||
                         mismatch(0x28u, 0x48u) || mismatch(0x2Cu, 0x4Cu) || mismatch(0x30u, 0x78u) || mismatch(0x34u, 0x7Cu) ||
                         mismatch(0x38u, 0x68u) || mismatch(0x3Cu, 0x6Cu) || mismatch(0x40u, 0x70u) || mismatch(0x44u, 0x74u) ||
                         mismatch(0x48u, 0x30u) || mismatch(0x4Cu, 0x34u) || mismatch(0x50u, 0xD8u) || mismatch(0x54u, 0xDCu) ||
                         mismatch(0x68u, 0x60u) || mismatch(0x6Cu, 0x64u) || mismatch(0x70u, 0x58u) || mismatch(0x74u, 0x5Cu) ||
                         mismatch(0x78u, 0x50u) || mismatch(0x7Cu, 0x54u) || mismatch(0x80u, 0x90u) || mismatch(0x84u, 0x94u) ||
                         mismatch(0x88u, 0x98u) || mismatch(0x8Cu, 0x9Cu) || mismatch(0x90u, 0xA0u) || mismatch(0x94u, 0xA4u) ||
                         mismatch(0x98u, 0xA8u) || mismatch(0x9Cu, 0xACu) || mismatch(0xA0u, 0x80u) || mismatch(0xA4u, 0x84u) ||
                         mismatch(0xA8u, 0x88u) || mismatch(0xACu, 0x8Cu) || mismatch(0xB0u, 0xC8u) || mismatch(0xB4u, 0xCCu) ||
                         mismatch(0xB8u, 0xC0u) || mismatch(0xBCu, 0xC4u) || mismatch(0xC0u, 0xD0u) || mismatch(0xC4u, 0xD4u) ||
                         mismatch(0xC8u, 0xB8u) || mismatch(0xCCu, 0xBCu) || mismatch(0xD0u, 0xB0u) || mismatch(0xD4u, 0xB4u) ||
                         mismatch(0xD8u, 0x38u) || mismatch(0xDCu, 0x3Cu) || mismatch(0xE0u, 0x40u) || mismatch(0xE4u, 0x44u) ||
                         word(rt, self + 0xF0u) != 0xFFFFFFFFu || word(rt, self + 0xF4u) != 0xFFFFFFFFu ||
                         word(rt, self + 0xF8u) != 0xFFFFFFFFu || word(rt, self + 0xFCu) != 0xFFFFFFFFu ||
                         word(rt, self + 0x100u) != 0xFFFFFFFFu || word(rt, self + 0x104u) != 0xFFFFFFFFu ||
                         rt.memory().load8(self + 0x15Eu) != 0u;
    ctx.gpr[2] = differs ? 1u : 0u;
}

// ---------------------------------------------------------------------------------------------------------------
// The reference-count steps of a shared object: `ll`/`sc` on a word of guest memory (single-threaded here, the host never
// preempts inside one). The decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful
// `sc`); true when the old value was 1.
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}
// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
}
// The `ll`/`sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// 0x0896301C (a0 = out, a1 = the {object, block} pair): byte-identical to func_08963418. It builds two {pointer,
// shared-pointer control block} pairs at out +0/+4 and +8/+0xC from the two words pair->+0 / pair->+4 (both start as 0).
// A non-null pair->+0: func_08B2B4D8(0x80, it) is wrapped by func_08BE43E4 (deleter func_08B35254) into a frame pair, a
// 0xA0-byte object is allocated (func_08B62344) and built (func_08BE45C4), a 0x10-byte control block {vtable 0x08CC6928,
// counts 1 / 1, object} is allocated and func_08B8E380 links it; the frame pair's block is released, out->+0 takes the
// object and out->+4 the control block (use count up, old block released). A non-null pair->+4 does the same with
// func_08B59AC8 / func_08BE43E4 (0x08B59ADC) / func_08BE449C into out +8 / +0xC. The temporaries' blocks are released
// at the end.

// ---------------------------------------------------------------------------------------------------------------
// 0x088F20E4 (a0 = the renderer, a1 = the settings block, f12 = a scale): the point at settings +0/+4/+8 is moved back
// along the direction at +0x1C/+0x20/+0x24 by `scale`; f22 = 1 + byte(+0x42) * 0xBB808081. For each of byte(+0x40)
// slots the phase (halfword +0x46 - 5) / 30 clamped to 0..1 is compared with 0x08BB9130(renderer +0x840, 0, 1); when
// the gate is at or below it, two more 0x08BB9130 reads scale 150 and 0.001 * halfword +0x44 into the two distances
// (f27, f26), and with byte(+0x41) set 0x08B1ED60 / 0x08B1EFF8 give a turn and a radius for 0x08B4C0F8 / 0x08B4BA68 /
// 0x08B4A720 / sqrt / 0x08B4A5E8 / 0x08B4FE74 / 0x08B4BAA8. Both 0x08BB9130 reads with -1 then scale settings
// +0x28..+0x30 and +0x34..+0x3C onto the point, which 0x088F5D20 hands to the renderer's +0x844 with (f27, f26) and
// settings +0xC. v0 = byte(+0x40) when no slot is to be done, 0 after the loop.
void func_088F20E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 renderer = ctx.gpr[4];  // s5
    const u32 settings = ctx.gpr[5];  // s0
    const float scale = ctx.fpr[12];
    GuestFrame frame(ctx, 0xA0u);
    SavedRegisters saved(rt, ctx, frame,
                         {{16u, 0x40u}, {17u, 0x44u}, {18u, 0x48u}, {19u, 0x4Cu}, {20u, 0x50u}, {21u, 0x54u}, {22u, 0x58u},
                          {23u, 0x5Cu}, {31u, 0x60u}, {52u, 0x70u}, {53u, 0x74u}, {54u, 0x78u}, {55u, 0x7Cu},
                          {56u, 0x80u}, {57u, 0x84u}, {58u, 0x88u}, {59u, 0x8Cu}, {60u, 0x90u}, {61u, 0x94u}});
    const u32 direction = frame.at(0xCu);  // s3
    const u32 state = frame.at(0x18u);     // s4
    const u32 zero_block = frame.at(0x24u);  // s6
    const u32 matrix = frame.at(0x30u);      // s7
    const u32 rotator = renderer + 0x840u;   // s1
    const float dx = float_at(rt, settings + 0x1Cu);
    const float dy = float_at(rt, settings + 0x20u);
    const float dz = float_at(rt, settings + 0x24u);
    const float sx = psp_mul(dx, scale);
    const float sy = psp_mul(scale, dy);
    const float sz = psp_mul(scale, dz);
    const float px = float_at(rt, settings) - sx;
    const float py = float_at(rt, settings + 4u) - sy;
    const float pz = float_at(rt, settings + 8u) - sz;
    put_float(rt, frame.at(0xCu), dx);
    put_float(rt, frame.at(4u), py);
    put_float(rt, frame.at(8u), pz);
    put_float(rt, frame.at(0u), px);
    put_float(rt, frame.at(0x10u), dy);
    put_float(rt, frame.at(0x14u), dz);
    const float length = std::sqrt(call_setf(rt, ctx, 0x08B4A720u, {direction}, {}).f0);
    const float f22 = static_cast<float>(rt.memory().load8(settings + 0x42u)) * bits_float(0xBB808081u) + 1.0f;
    call_setf(rt, ctx, 0x08B4A5E8u, {direction}, {1.0f / length});
    u32 count = rt.memory().load8(settings + 0x40u);
    if (static_cast<s32>(count) <= 0) {
        ctx.gpr[2] = count;
        return;
    }
    const float zero = 0.0f;
    u32 index = 0u;
    for (;;) {
        const u32 advance = rt.memory().load16(settings + 0x46u);
        float phase = (static_cast<float>(advance) - 5.0f) / 30.0f;
        if (phase < zero) phase = 0.0f;
        else if (1.0f < phase) phase = 1.0f;
        const float gate = call_setf(rt, ctx, 0x08BB9130u, {rotator}, {0.0f, 1.0f}).f0;
        if (gate <= phase) {
            const float first_read = call_setf(rt, ctx, 0x08BB9130u, {rotator}, {f22, 1.0f}).f0;
            const u32 span = rt.memory().load16(settings + 0x44u);
            const float f27 = psp_mul(first_read, 150.0f) + bits_float(0x44834000u);
            const float scaled = psp_mul(static_cast<float>(span), bits_float(0x3A83126Fu));
            const float second_read = call_setf(rt, ctx, 0x08BB9130u, {rotator}, {f22, 1.0f}).f0;
            const float f26 = psp_mul(scaled, second_read);
            put_float(rt, frame.at(0x18u), dx);
            put_float(rt, frame.at(0x1Cu), dy);
            put_float(rt, frame.at(0x20u), dz);
            if (rt.memory().load8(settings + 0x41u) != 0u) {
                const float turn_read = call_setf(rt, ctx, 0x08BB9130u, {rotator}, {f22, 1.0f}).f0;
                const u32 turn_byte = rt.memory().load8(settings + 0x41u);
                const float f21 = psp_mul(static_cast<float>(turn_byte), bits_float(0x3B808081u));
                put(rt, frame.at(0x24u), 0u);
                put_float(rt, frame.at(0x28u), psp_mul(length, turn_read));
                put(rt, frame.at(0x2Cu), 0u);
                const u32 first_object = call_set(rt, ctx, 0x08B1ED60u, {rotator}).v0;
                const float turn = call_setf(rt, ctx, 0x08B1EFF8u, {first_object}, {}).f0;
                const u32 second_object = call_set(rt, ctx, 0x08B1ED60u, {rotator}).v0;
                const float radius = call_setf(rt, ctx, 0x08B1EFF8u, {second_object}, {}).f0;
                const float angle = psp_mul(f21, psp_mul(turn, bits_float(0x40490FDBu)));
                call_setf(rt, ctx, 0x08B4C0F8u, {matrix}, {angle, psp_mul(radius, bits_float(0x40C90FDBu))});
                call_setf(rt, ctx, 0x08B4BA68u, {matrix, state, zero_block}, {});
                const float turn_length = std::sqrt(call_setf(rt, ctx, 0x08B4A720u, {zero_block}, {}).f0);
                call_setf(rt, ctx, 0x08B4A5E8u, {zero_block}, {1.0f / turn_length});
                call_setf(rt, ctx, 0x08B4FE74u, {matrix, zero_block, direction}, {});
                call_setf(rt, ctx, 0x08B4BAA8u, {matrix, state}, {});
            }
            const float side = call_setf(rt, ctx, 0x08BB9130u, {rotator}, {-1.0f, 1.0f}).f0;
            const float ox = float_at(rt, frame.at(0u)) + psp_mul(side, float_at(rt, settings + 0x28u));
            const float oy = float_at(rt, frame.at(4u)) + psp_mul(side, float_at(rt, settings + 0x2Cu));
            const float oz = float_at(rt, frame.at(8u)) + psp_mul(side, float_at(rt, settings + 0x30u));
            put_float(rt, frame.at(0u), ox);
            put_float(rt, frame.at(4u), oy);
            put_float(rt, frame.at(8u), oz);
            const float forward = call_setf(rt, ctx, 0x08BB9130u, {rotator}, {-1.0f, 1.0f}).f0;
            const float fx = float_at(rt, frame.at(0u)) + psp_mul(forward, float_at(rt, settings + 0x34u));
            const float fy = float_at(rt, frame.at(4u)) + psp_mul(forward, float_at(rt, settings + 0x38u));
            const float fz = float_at(rt, frame.at(8u)) + psp_mul(forward, float_at(rt, settings + 0x3Cu));
            put_float(rt, frame.at(0u), fx);
            put_float(rt, frame.at(4u), fy);
            put_float(rt, frame.at(8u), fz);
            call_setf(rt, ctx, 0x088F5D20u, {renderer + 0x844u, frame.at(0u), state, settings + 0xCu}, {f27, f26});
        }
        count = rt.memory().load8(settings + 0x40u);
        ++index;
        if (!(index < count)) break;
    }
    ctx.gpr[2] = 0u;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x08862C90 (a0 = the object, a1 = a callback table): the object is built in place through calls and virtuals.

} // namespace
} // namespace new_batch_main_2104
} // namespace gt::game

#define NEW2104(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2104::name, gt::ReturnKind::kind)
NEW2104(0x089283ACu, func_089283AC, Word);
NEW2104(0x089344D8u, func_089344D8, Word);
NEW2104(0x088F20E4u, func_088F20E4, Word);
