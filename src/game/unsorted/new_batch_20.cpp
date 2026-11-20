// New batch 20: small functions (76-82 instructions each) that the extra seeds made emittable, written by hand from
// `tools.disasm`, one function per original body. Delay slots are read as "before the branch / call"; a register set in a
// `jal`'s delay slot is set before the call. The callee-saved registers the original keeps values in are set where the
// asm sets them (a callee may read them) and put back at the end; `v0` (or `f0` for the float ones) ends as the asm
// leaves it on every exit path. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_20 {
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
// The same, with `v1` and `f0` too (a callee that never writes `v1` leaves it, and a float callee answers in `f0`).
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A call whose arguments are floats ($f12-...) and whose answer may be in $f0.
GuestResult call_floats(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
s32 signed_byte_at(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
s32 trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<s32>(value);
}

// s0-s7 and fp are callee-saved: the body sets them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_[8] = ctx.gpr[30];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_[8];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[9];
};

// $f20-$f31 are callee-saved the same way.
class SavedFloats {
public:
    explicit SavedFloats(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 12u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFloats() {
        for (unsigned i = 0u; i < 12u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[12];
};

// 0x08927D14 (a0 = this, a1 = a cursor object): 0x089273D8(this) answers a holder whose virtual +0x0C is tested with 1
// and, when that is zero, again with 0x40; a caller with a non-negative word at this + 0x1FB0 then carries on. The
// counter at cursor + 8 moves by one (down for a nonzero answer, up for zero) and is clamped into [0, cursor + 4 - 1];
// 0x08927BE4(this, cursor, 0, answer) runs and, when it answers nonzero, 0x089274C8(this, cursor) too. Three flags are
// cleared, cursor + 0xC receives cursor + 8 and v0 ends as that word (or as what the bailing path held).
void func_08927D14(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 cursor = ctx.gpr[5];
    ctx.gpr[16] = self;
    ctx.gpr[17] = cursor;
    u32 holder = call(rt, ctx, 0x089273D8u, {self});
    u32 answer = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder, 1u});
    u32 state;
    if (answer != 0u) {
        state = word(rt, self + 0x1FB0u);
    } else {
        holder = call(rt, ctx, 0x089273D8u, {self});
        answer = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder, 0x40u});
        if (answer == 0u) return;  // v0 = 0
        state = word(rt, self + 0x1FB0u);
    }
    ctx.gpr[2] = state;
    if (static_cast<s32>(state) < 0) return;
    holder = call(rt, ctx, 0x089273D8u, {self});
    answer = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder, 0x40u});
    u32 counter = word(rt, cursor + 8u);
    counter = answer != 0u ? counter - 1u : counter + 1u;
    put(rt, cursor + 8u, counter);
    const u32 limit = word(rt, cursor + 4u);
    if (static_cast<s32>(counter) >= static_cast<s32>(limit)) {
        put(rt, cursor + 8u, 0u);
    } else if (static_cast<s32>(counter) < 0) {
        put(rt, cursor + 8u, limit - 1u);
    }
    holder = call(rt, ctx, 0x089273D8u, {self});
    answer = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder, 0x40u});
    const u32 advanced = call(rt, ctx, 0x08927BE4u, {self, cursor, 0u, answer});
    if (advanced != 0u) call(rt, ctx, 0x089274C8u, {self, cursor});
    rt.memory().store8(self + 0x1FCDu, 0u);
    put(rt, self + 0x1FC8u, 0u);
    rt.memory().store8(self + 0x1FCCu, 0u);
    const u32 value = word(rt, cursor + 8u);
    put(rt, cursor + 0xCu, value);
    ctx.gpr[2] = value;
}

// 0x089396F8 (a0 = a reader, a1 = a packed bit source, a2 = the byte destination): the top two bits of the byte at a1
// pick the path. Kind 1 ends in a jump to 0x08B79A88(destination, a1 + 1, word(reader's + 4's + 0x18)); kinds 0 and 2
// first ask 0x089396D8(reader, byte & 0x3F) for a value and pass it on the same way. Kind 2 then unpacks that many bits
// of a1 + 1, copying the bytes at a1 + 1 + ceil(count / 8) whose bit is set, and answers 0 (or (count & 7) != 0 when
// count <= 0); any other kind answers 2.
void func_089396F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 reader = ctx.gpr[4];
    const u32 packed = ctx.gpr[5];
    const u32 destination = ctx.gpr[6];
    u32 tail_target = 0u;
    u32 tail_arguments[3] = {0u, 0u, 0u};
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        const u32 count = word(rt, word(rt, reader + 4u) + 0x18u);
        const u32 header = rt.memory().load8(packed);
        const u32 kind = header >> 6;
        const u32 masked = header & 0x3Fu;
        ctx.gpr[18] = packed + 1u;
        ctx.gpr[17] = destination;
        ctx.gpr[16] = count;
        if (kind == 1u) {
            tail_target = 0x08B79A88u;
            tail_arguments[0] = destination;
            tail_arguments[1] = packed + 1u;
            tail_arguments[2] = count;
        } else if (kind == 0u) {
            tail_target = 0x08B79A88u;
            tail_arguments[0] = destination;
            tail_arguments[1] = call(rt, ctx, 0x089396D8u, {reader, masked});
            tail_arguments[2] = count;
        } else if (kind == 2u) {
            const u32 lead = call(rt, ctx, 0x089396D8u, {reader, masked});
            call(rt, ctx, 0x08B79A88u, {destination, lead, count});
            if (static_cast<s32>(count) > 0) {
                u32 source = packed + 1u + (count >> 3) + ((count & 7u) != 0u ? 1u : 0u);
                for (u32 i = 0u; i != count; ++i) {
                    const u32 bit = (rt.memory().load8(packed + 1u + (i >> 3)) >> (i & 7u)) & 1u;
                    if (bit != 0u) {
                        rt.memory().store8(destination + i, rt.memory().load8(source));
                        source += 1u;
                    }
                }
                ctx.gpr[2] = 0u;
            } else {
                ctx.gpr[2] = (count & 7u) != 0u ? 1u : 0u;
            }
            return;
        } else {
            ctx.gpr[2] = 2u;
            return;
        }
    }
    tail_call(ctx, tail_target, {tail_arguments[0], tail_arguments[1], tail_arguments[2]});
}

// 0x08A4F784 (a0 = a descriptor, a1 = a command block, a2 = a 16-bit selector): 0x08A4F5C0(selector, a1, selector, a3)
// answers a word plus a second register; the word goes to a3 and the low halfword of the second to sp + 4.
// 0x08C7ACB0(a1, selector, sp + 0x20, word) formats it, then the descriptor's word + 8 is mtlo'd and the halfword at
// descriptor + 0x12 madd's against the word's high half (<< 3) into the offset. 0x08B36018(offset, 0, count) clears that
// many bytes, 0x08C7ACA8(a1, selector, sp + 8, 0, 0, byte + 4, byte + 5) fills the output area, and four bytes plus two
// halfwords go to word(descriptor + 0xC) + offset. v0 = the halfword read from sp + 0x50.
void func_08A4F784(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x90u);
    const u32 descriptor = ctx.gpr[4];
    const u32 block = ctx.gpr[5];
    const u32 selector = ctx.gpr[6];
    const u32 extra = ctx.gpr[7];
    const u32 masked = selector & 0xFFFFu;
    ctx.gpr[16] = descriptor;
    ctx.gpr[19] = block;
    ctx.gpr[18] = masked;
    const GuestResult first = call_result(rt, ctx, 0x08A4F5C0u, {selector, block, selector, extra});
    rt.memory().store16(frame.at(0x64u), static_cast<std::uint16_t>(first.v1));
    const u32 assembled = first.v0;                          // both halves of v0, re-inserted into a3
    const u32 low = first.v1 & 0xFFFFu;                      // the halfword `lhu sp + 4` reads back
    const u32 high = assembled >> 16;                        // the halfword at sp + 2 of the stored word
    put(rt, frame.at(0u), assembled);
    put(rt, frame.at(0x60u), assembled);
    rt.memory().store16(frame.at(4u), static_cast<std::uint16_t>(low));
    const u32 count = rt.memory().load16(descriptor + 0x12u);
    ctx.lo = word(rt, descriptor + 8u);
    {   // `madd high, count` (signed): hi:lo += high x count
        std::uint64_t accumulator = (static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo;
        accumulator += static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(high)) *
                                                  static_cast<std::int32_t>(count));
        ctx.lo = static_cast<std::uint32_t>(accumulator);
        ctx.hi = static_cast<std::uint32_t>(accumulator >> 32);
    }
    const u32 offset = ctx.lo;
    const u32 byte4 = rt.memory().load8(descriptor + 4u);
    const u32 byte5 = rt.memory().load8(descriptor + 5u);
    rt.memory().store16(frame.at(0x16u), static_cast<std::uint16_t>(byte5));
    put(rt, frame.at(0x1Cu), offset);
    rt.memory().store16(frame.at(0x18u), static_cast<std::uint16_t>((byte4 >> 1) & 0x7Fu));
    rt.memory().store16(frame.at(0x14u), static_cast<std::uint16_t>(byte4));
    put(rt, frame.at(8u), 0u);
    rt.memory().store16(frame.at(0x1Au), 0u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(0x10u), 0u);
    call(rt, ctx, 0x08B36018u, {offset, 0u, count});
    call(rt, ctx, 0x08C7ACA8u, {block, masked, frame.at(8u), 0u, 0u, byte4, byte5});
    const u32 destination = word(rt, descriptor + 0xCu) + (high << 3);
    rt.memory().store8(destination, rt.memory().load8(frame.at(0x28u)));
    rt.memory().store8(destination + 1u, rt.memory().load8(frame.at(0x2Cu)));
    rt.memory().store8(destination + 2u, rt.memory().load8(frame.at(0x20u)));
    rt.memory().store8(destination + 3u, rt.memory().load8(frame.at(0x24u)));
    const u32 answer = word(rt, frame.at(0x50u));
    rt.memory().store16(destination + 6u, static_cast<std::uint16_t>(masked));
    rt.memory().store16(destination + 4u, static_cast<std::uint16_t>(answer));
    ctx.gpr[2] = answer;
}

// 0x08A74B34 (a0 = an object, a1 = a formatter state): builds the string at sp + 4 with 0x08AAB5DC(sp + 4, a1) and
// 0x08AABA8C(sp + 4, 0x08CB0C94). The word at a1 + 0x14 is resolved (a zero one is reported through
// 0x08A5BB7C(0x08CB0CA4) first) to its word + 0x14, which 0x08A7BDB0 turns into text; the pieces are appended with
// 0x08AABA8C, 0x08A5D454(a1) and 0x08AABA8C, and 0x08B88F44(a0, 0x08AAB540(sp + 4)) finishes. The word at sp + 4, unless
// it is the empty-string marker 0x08D74FD0, is released with 0x08B60804(word - 4, -1) and, when that answers <= 0,
// 0x08B8542C(word - 0xC, sp). Answers a0.
void func_08A74B34(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 state = ctx.gpr[5];
    ctx.gpr[20] = self;
    ctx.gpr[18] = state;
    const u32 buffer = frame.at(4u);
    ctx.gpr[19] = buffer;
    call(rt, ctx, 0x08AAB5DCu, {buffer, state});
    const u32 text = call(rt, ctx, 0x08AABA8Cu, {buffer, 0x08CB0C94u});
    const u32 held = word(rt, state + 0x14u);
    if (held == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB0CA4u});
    u32 object = word(rt, held + 0x14u);
    if (object != 0u && word(rt, object + 4u) == 0u) object = 0u;
    const u32 first = call(rt, ctx, 0x08A7BDB0u, {object});
    const u32 second = call(rt, ctx, 0x08AABA8Cu, {text, first});
    const u32 third = call(rt, ctx, 0x08AABA8Cu, {second, 0x08CB0CACu});
    const u32 fourth = call(rt, ctx, 0x08A5D454u, {state});
    const u32 fifth = call(rt, ctx, 0x08AABA8Cu, {third, fourth});
    call(rt, ctx, 0x08AABA8Cu, {fifth, 0x08CB0CB0u});
    const u32 seventh = call(rt, ctx, 0x08AAB540u, {buffer});
    call(rt, ctx, 0x08B88F44u, {self, seventh});
    const u32 head = word(rt, frame.at(4u));
    if (head - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {head - 0xCu, frame.at(0u)});
    }
    ctx.gpr[2] = self;
}

// 0x08ACE7A8 (a0 = this): 0x08C7ADA8(this + 0x1C0) looks a list up (zero ends the function), 0x08C7ADA0(this + 0x1C0,
// answer, answer) reports a code; a negative or zero one finishes with 0x08ACE768(this). The arrays at this + 0x328
// (records of 0xEC from this + 0x334) and at this + 0x32C (records of 0xF0 from this + 0x420) are walked with
// 0x08ACD4F0 and 0x08ACD8B4. v0 ends 0, or the word at this + 0x32C when that is <= 0.
void func_08ACE7A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    bool tail = false;
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[17] = self;
        const u32 found = call(rt, ctx, 0x08C7ADA8u, {self + 0x1C0u});
        if (found != 0u) {
            const u32 code = call(rt, ctx, 0x08C7ADA0u, {self + 0x1C0u, found, found});
            if (static_cast<s32>(code) < 0 || code == 0u) {
                tail = true;
            } else {
                if (static_cast<s32>(word(rt, self + 0x328u)) > 0) {
                    u32 index = 0u;
                    u32 offset = 0x334u;
                    do {
                        call(rt, ctx, 0x08ACD4F0u, {self, self + offset});
                        offset += 0xECu;
                        ++index;
                    } while (index < word(rt, self + 0x328u));
                }
                const u32 count = word(rt, self + 0x32Cu);
                if (static_cast<s32>(count) <= 0) {
                    ctx.gpr[2] = count;
                    return;
                }
                u32 index = 0u;
                u32 offset = 0x420u;
                do {
                    call(rt, ctx, 0x08ACD8B4u, {self, self + offset});
                    offset += 0xF0u;
                    ++index;
                } while (index < word(rt, self + 0x32Cu));
                ctx.gpr[2] = 0u;
            }
        }
    }
    if (tail) tail_call(ctx, 0x08ACE768u, {self});
}

// 0x08AD31E0 (a0 = this): 0x08AD16C0(this) and 0x08AD1098(this + 0x1D0) reset the members; the float 50.0 goes to
// this + 0x260, 1.0 to +0x250..+0x268, the byte 1 to this + 0x27A and zeros to this + 0x26C..+0x279, then
// 0x08AD0FAC(this + 0x27C). 0x08B62344(0xA0) allocates, 0x08AD6AB8(allocation, 0x80, 0x18) sets it up and it goes to
// this + 0x29C with the table 0x08D77268 at + 4 (the first use stores 0x08CD6CF0 there under 0x08B6441C/0x08B64494).
// 0x08C3DECC(this + 0x2A4), 0x08B28DCC(this + 0x40B0) and the float 1.0 at this + 0x240 finish; v0 is the last call's.
void func_08AD31E0(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[17] = self;
    ctx.gpr[19] = self + 0x29Cu;
    call(rt, ctx, 0x08AD16C0u, {self});
    call(rt, ctx, 0x08AD1098u, {self + 0x1D0u});
    put_float(rt, self + 0x260u, 50.0f);
    put_float(rt, self + 0x268u, 1.0f);
    put_float(rt, self + 0x250u, 1.0f);
    put_float(rt, self + 0x254u, 1.0f);
    put_float(rt, self + 0x258u, 1.0f);
    put_float(rt, self + 0x25Cu, 1.0f);
    put_float(rt, self + 0x264u, 1.0f);
    rt.memory().store8(self + 0x27Au, 1u);
    put(rt, self + 0x26Cu, 0u);
    rt.memory().store16(self + 0x270u, 0u);
    rt.memory().store16(self + 0x272u, 0u);
    rt.memory().store16(self + 0x274u, 0u);
    rt.memory().store16(self + 0x276u, 0u);
    rt.memory().store8(self + 0x278u, 0u);
    rt.memory().store8(self + 0x279u, 0u);
    call(rt, ctx, 0x08AD0FACu, {self + 0x27Cu});
    const u32 allocated = call(rt, ctx, 0x08B62344u, {0xA0u});
    ctx.gpr[16] = allocated;
    call(rt, ctx, 0x08AD6AB8u, {allocated, 0x80u, 0x18u});
    put(rt, self + 0x29Cu, allocated);
    const u32 table = 0x08D77268u;
    ctx.gpr[18] = table;
    if (signed_byte_at(rt, 0x08D77260u) == 0) {
        if (call(rt, ctx, 0x08B6441Cu, {0x08D77260u}) != 0u) {
            put(rt, 0x08D77268u, 0x08CD6CF0u);
            call(rt, ctx, 0x08B64494u, {});
        }
    }
    put(rt, self + 0x2A0u, table);
    call(rt, ctx, 0x08C3DECCu, {self + 0x2A4u});
    call(rt, ctx, 0x08B28DCCu, {self + 0x40B0u});
    put_float(rt, self + 0x240u, 1.0f);
}

// 0x08B1A284 (a0 = this): 0x08B2E924(this) then 0x08B1A17C(this), which is the function's real work.
void func_08B1A284(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x10u);
        ctx.gpr[16] = self;
        call(rt, ctx, 0x08B2E924u, {self});
    }
    tail_call(ctx, 0x08B1A17Cu, {self});
}

// 0x08BA8C48 (a0 = this): the vtable 0x08CC1918 is stored, the array ending at this + 0x28 (records of 0xC before
// this + 0x4C) is walked from the end with 0x08BA76B8(record, record + 4, &flag) and each held object released with
// 0x08B35254; then the vtable 0x08CC18E0, the reference counts at node + 4 and node + 8 of the node at this + 0x1C
// (each one released with the virtual at +8 / +0xC when it drops to zero), 0x08B8E5D8(word + 8, word + 0xC, sp),
// 0x08B35254(word(this + 8)) and the vtable 0x08CBF518 finish. v0 = 0x08CBF518.
void func_08BA8C48(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 begin = self + 0x28u;
    ctx.gpr[18] = self;
    put(rt, self, 0x08CC1918u);
    ctx.gpr[19] = frame.at(1u);
    if (begin != 0u) {
        u32 node = self + 0x40u;
        if (self + 0x4Cu != begin) {
            for (;;) {
                call(rt, ctx, 0x08BA76B8u, {word(rt, node), word(rt, node + 4u), frame.at(1u)});
                const u32 held = word(rt, node);
                if (held != 0u) call(rt, ctx, 0x08B35254u, {held});
                if (node == begin) break;
                node -= 0xCu;
            }
        }
    }
    const u32 element = word(rt, self + 0x1Cu);
    put(rt, self, 0x08CC18E0u);
    if (element != 0u) {
        const u32 count = word(rt, element + 4u);
        put(rt, element + 4u, count - 1u);
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, element) + 8u), {element});
            const u32 other = word(rt, element + 8u);
            put(rt, element + 8u, other - 1u);
            if (other == 1u) call_indirect(rt, ctx, word(rt, word(rt, element) + 0xCu), {element});
        }
    }
    call(rt, ctx, 0x08B8E5D8u, {word(rt, self + 8u), word(rt, self + 0xCu), frame.at(0u)});
    const u32 member = word(rt, self + 8u);
    if (member != 0u) call(rt, ctx, 0x08B35254u, {member});
    put(rt, self, 0x08CBF518u);
    ctx.gpr[2] = 0x08CBF518u;
}

// 0x08C2605C (a0 = an owner, a1 = a list node, a2 = a word): 0x08A5BC20(0x18, 4, a2, a3) allocates a copy; its members
// at +0x10 and +0x14 are copied from the node's (+0x10 with 0x08A6972C, +0x14 with 0x08A76ABC), the node's word and the
// given word go to +0 and +4, and the node's +0xC child (if any) is copied with a recursive call at this address. The
// node's +8 chain is walked the same way, each new copy linked through +8 and back through the previous node's +4.
// v0 = the first copy.
u32 clone_nodes(Runtime &rt, AllegrexContext &ctx, u32 address, u32 second_copy) {
    const u32 owner = ctx.gpr[4];
    const u32 node = ctx.gpr[5];
    const u32 word_value = ctx.gpr[6];
    const u32 extra = ctx.gpr[7];
    u32 argument2 = word_value;  // $a2 as the asm leaves it between the calls
    ctx.gpr[20] = owner;
    ctx.gpr[16] = node;
    ctx.gpr[17] = word_value;
    const u32 created = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u, argument2, extra});
    ctx.gpr[19] = created;
    call(rt, ctx, 0x08A6972Cu, {created + 0x10u, node + 0x10u});
    call(rt, ctx, second_copy, {created + 0x14u, node + 0x14u});
    put(rt, created + 0xCu, 0u);
    put(rt, created, word(rt, node));
    put(rt, created + 4u, word_value);
    put(rt, created + 8u, 0u);
    const u32 child = word(rt, node + 0xCu);
    if (child != 0u) {
        argument2 = created;
        put(rt, created + 0xCu, call(rt, ctx, address, {owner, child, created}));
    }
    u32 next = word(rt, node + 8u);
    if (next == 0u) {
        ctx.gpr[2] = created;
        return created;
    }
    u32 previous = created;
    do {
        const u32 copy = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u, argument2, extra});
        ctx.gpr[16] = copy;
        ctx.gpr[18] = previous;
        call(rt, ctx, 0x08A6972Cu, {copy + 0x10u, next + 0x10u});
        call(rt, ctx, second_copy, {copy + 0x14u, next + 0x14u});
        put(rt, copy + 0xCu, 0u);
        const u32 grandchild = word(rt, next + 0xCu);
        put(rt, copy + 4u, previous);
        put(rt, copy + 8u, 0u);
        put(rt, copy, word(rt, next));
        put(rt, previous + 8u, copy);
        argument2 = copy;
        if (grandchild != 0u) put(rt, copy + 0xCu, call(rt, ctx, address, {owner, grandchild, copy}));
        next = word(rt, next + 8u);
        previous = copy;
    } while (next != 0u);
    ctx.gpr[2] = created;
    return created;
}
void func_08C2605C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    clone_nodes(rt, ctx, 0x08C2605Cu, 0x08A76ABCu);
}
void func_08C27F70(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    clone_nodes(rt, ctx, 0x08C27F70u, 0x08A5D424u);
}

// 0x0881BAC0 (a0 = this, a1..a3 and t0 = a key plus a destination index): the 30 records of 8 bytes from this + 0x354
// are scanned: 0x088A5EE0(record) answers two words and 0x088A6164(record) a third; a record whose pair is the pair -1
// ends the search, one whose words are (a2, a3) and whose third is the saved t0 answers 0. A pair of -1 with a negative
// index reports through 0x0881ACC0(this, 1, 1) and uses index 0x1D; an exhausted scan does the same. Then
// 0x088A79AC(sp, 0), 0x088A58A4(sp, a2, a3), 0x088A599C(sp, t0), 0x088A6C18(this + 0x354 + index * 8, sp) and
// 0x088A7548(sp) run and v0 = 1.
void func_0881BAC0(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 fourth = ctx.gpr[8];
    ctx.gpr[30] = self;
    ctx.gpr[23] = 0x1Eu;
    ctx.gpr[22] = 0xFFFFFFFFu;
    ctx.gpr[21] = third;
    ctx.gpr[20] = second;
    ctx.gpr[19] = 0u;
    ctx.gpr[18] = self + 0x354u;
    put(rt, frame.at(0x10u), fourth);
    u32 index = 0u;
    for (;;) {
        const GuestResult pair = call_result(rt, ctx, 0x088A5EE0u, {ctx.gpr[18], first, second, third});
        const u32 left = pair.v0;
        const u32 right = pair.v1;
        ctx.gpr[16] = left;
        ctx.gpr[17] = right;
        const u32 probe = call(rt, ctx, 0x088A6164u, {ctx.gpr[18], first, second, third});
        ctx.gpr[18] += 8u;
        if (left == 0xFFFFFFFFu && right == 0xFFFFFFFFu) {
            if (static_cast<s32>(ctx.gpr[19]) >= 0) {
                index = ctx.gpr[19];
            } else {
                call(rt, ctx, 0x0881ACC0u, {self, 1u, 1u});
                index = 0x1Du;
            }
            break;
        }
        const u32 scanned = ctx.gpr[19] + 1u;
        ctx.gpr[19] = scanned;
        if (left == second && right == third && word(rt, frame.at(0x10u)) == probe) {
            ctx.gpr[2] = 0u;
            return;
        }
        if (scanned != 0x1Eu) continue;
        call(rt, ctx, 0x0881ACC0u, {self, 1u, 1u});
        index = 0x1Du;
        break;
    }
    call(rt, ctx, 0x088A79ACu, {frame.at(0u), 0u});
    call(rt, ctx, 0x088A58A4u, {frame.at(0u), second, third});
    call(rt, ctx, 0x088A599Cu, {frame.at(0u), word(rt, frame.at(0x10u))});
    call(rt, ctx, 0x088A6C18u, {(index << 3) + self + 0x354u, frame.at(0u)});
    call(rt, ctx, 0x088A7548u, {frame.at(0u)});
    ctx.gpr[2] = 1u;
}

// 0x088CFB14 (a0 = this, a1 = an object): 0x088DA788(word(this), word(this + 0x10)) answers a pointer whose float and
// this's +0x144 go to 0x088CFA9C(this, f12, f13) -> f22, then 0x088CF414(this, float(this + 0x140)) -> this + 0x114 and
// 0x088CF4AC(this, f22) writes +0x114 and +0x120. The object's virtual +0x1C -> 0x088CF4F8(this, f0) -> this + 0x118,
// 0x088CF554(this) -> this + 0x11C. The product of this's +0x114, +0x120 and +0x118, scaled by that and by 1000 and
// 1/60, is compared with zero: above it 0x088CF5F4(this, f12, f22, f14) runs, else this + 0xBC is cleared. Answers the
// scaled product (or zero) in f0.
void func_088CFB14(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    ctx.gpr[16] = self;
    ctx.gpr[17] = other;
    const u32 holder = call(rt, ctx, 0x088DA788u, {word(rt, self), word(rt, self + 0x10u)});
    const float f20 = float_at(rt, self + 0x144u);
    const float f22 = call_floats(rt, ctx, 0x088CFA9Cu, {self}, {f20, float_at(rt, holder)}).f0;
    const float first = call_floats(rt, ctx, 0x088CF414u, {self}, {float_at(rt, self + 0x140u)}).f0;
    put_float(rt, self + 0x114u, first);
    const float second = call_floats(rt, ctx, 0x088CF4ACu, {self}, {f22}).f0;
    put_float(rt, self + 0x120u, second);
    const GuestResult virtual_answer = call_indirect(rt, ctx, word(rt, word(rt, other) + 0x1Cu), {other}, {});
    const float third = call_floats(rt, ctx, 0x088CF4F8u, {self}, {virtual_answer.f0}).f0;
    put_float(rt, self + 0x118u, third);
    const float fourth = call_floats(rt, ctx, 0x088CF554u, {self}, {}).f0;
    const float f2 = 0.0f;
    const float f1 = psp_mul(float_at(rt, self + 0x114u), float_at(rt, self + 0x120u));
    put_float(rt, self + 0x11Cu, fourth);
    const float product = psp_mul(fourth, psp_mul(f1, float_at(rt, self + 0x118u)));
    const float scaled = psp_mul(psp_mul(product, 1000.0f), std::bit_cast<float>(0x3C88AB7Cu));
    if (f2 < scaled) {
        const GuestResult again = call_indirect(rt, ctx, word(rt, word(rt, other) + 0x1Cu), {other}, {});
        call_floats(rt, ctx, 0x088CF5F4u, {self}, {product, f22, again.f0});
        ctx.fpr[0] = scaled;
    } else {
        put(rt, self + 0xBCu, 0u);
        ctx.fpr[0] = f2;
    }
}

// 0x088DE740 (a0 = this): 0x088D6FD4(this, 0, 0, -1) starts it, then the loop asks 0x088D9F80(this) for the count while
// the counter is below it: each entry at word(this + 0xC0) + index * 0x2F8 whose byte + 0x2B4 is set and that
// 0x088E85E8 accepts gets this's word + 0x90 and + 0x94 through 0x088D9C24, its +0x2C8 set to 1, the answers at +0x2D0
// and +0x2D4 and the byte cleared, then 0x088D9BBC(this, 0, 0) runs. 0x088E7690(word(this + 0x594)) when present,
// 0x088DC1B8(this, 3), the virtual +0x6C of word(this + 0x148) and the virtual +0x18 of word(this + 0x14C) when present
// finish, then bytes this + 0x21 and this + 0x1E are cleared. v0 is the last virtual call's answer.
void func_088DE740(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[20] = 1u;
    ctx.gpr[19] = 0u;
    ctx.gpr[18] = 0u;
    ctx.gpr[17] = self;
    call(rt, ctx, 0x088D6FD4u, {self, 0u, 0u, 0xFFFFFFFFu});
    for (;;) {
        const u32 count = call(rt, ctx, 0x088D9F80u, {self});
        const bool again = ctx.gpr[18] < count;
        ctx.gpr[18] += 1u;
        if (!again) break;
        const u32 entry = word(rt, self + 0xC0u) + ctx.gpr[19];
        if (rt.memory().load8(entry + 0x2B4u) != 0u) {
            if (call(rt, ctx, 0x088E85E8u, {entry}) != 0u) {
                put(rt, entry + 0x2C8u, 1u);
                const GuestResult answer = call_result(rt, ctx, 0x088D9C24u,
                                                       {self, word(rt, self + 0x90u), word(rt, self + 0x94u)});
                put(rt, entry + 0x2D0u, answer.v0);
                put(rt, entry + 0x2D4u, answer.v1);
                rt.memory().store8(entry + 0x2B4u, 0u);
            }
        }
        ctx.gpr[19] += 0x2F8u;
        call(rt, ctx, 0x088D9BBCu, {self, 0u, 0u});
    }
    const u32 held = word(rt, self + 0x594u);
    if (held != 0u) call(rt, ctx, 0x088E7690u, {held});
    call(rt, ctx, 0x088DC1B8u, {self, 3u});
    const u32 first_object = word(rt, self + 0x148u);
    call_indirect(rt, ctx, word(rt, word(rt, first_object) + 0x6Cu), {first_object});
    const u32 second_object = word(rt, self + 0x14Cu);
    if (second_object != 0u) call_indirect(rt, ctx, word(rt, word(rt, second_object) + 0x18u), {second_object});
    rt.memory().store8(self + 0x21u, 0u);
    rt.memory().store8(self + 0x1Eu, 0u);
}

// 0x088EAC34 (a0 = this, a1 = a flag byte): 0x538's virtual +0x9C decides whether to go on. The 0x234-byte block at sp is
// cleared (0x08B36018), 0x08B08A88/0x08B089F8/0x08B08AC0/0x08B08A70 on word(this + 0xA8) fill the head of it (the word at
// sp, the byte at sp + 4, a 0x20-byte string at sp + 5 through 0x08B7C830 and 0x088D4808, a copy at sp + 0x25 through
// 0x08B79A88), sp + 0x228 gets that word, sp + 0x22C a halfword and sp + 0x230 this. Then the virtual +0x28 of the 0x538
// object's vtable runs with (0x538 object, sp, word(this + 0xA8), word(word(this + 0xA8)+0x28), word(this + 0xA8)'s copy,
// flag); when it answers nonzero the words at this + 0x560..0x574 are cleared. Answers 1 or 0.
void func_088EAC34(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x260u);
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5] & 0xFFu;
    ctx.gpr[20] = self;
    ctx.gpr[21] = flag;
    const u32 manager = word(rt, self + 0x538u);
    const u32 accepted = call_indirect(rt, ctx, word(rt, word(rt, manager) + 0x9Cu), {manager});
    u32 result = 0u;
    if (accepted != 0u) {
        call(rt, ctx, 0x08B36018u, {frame.at(0u), 0u, 0x234u});
        call(rt, ctx, 0x08B08A88u, {word(rt, self + 0xA8u)});
        const u32 first = call(rt, ctx, 0x08B089F8u, {word(rt, self + 0xA8u)});
        put(rt, frame.at(0u), first);
        rt.memory().store8(frame.at(4u), static_cast<std::uint8_t>(first));
        call(rt, ctx, 0x08B7C830u, {frame.at(5u), 0x08C89F2Cu, 0x20u});
        call(rt, ctx, 0x088D4808u, {});
        const u32 second = call(rt, ctx, 0x08B08AC0u, {word(rt, self + 0xA8u)});
        ctx.gpr[18] = second;
        call(rt, ctx, 0x08B79A88u, {frame.at(0x25u), second, second});
        const u32 third = call(rt, ctx, 0x08B08A70u, {word(rt, self + 0xA8u)});
        put(rt, frame.at(0x228u), second);
        ctx.gpr[17] = word(rt, self + 0x538u);
        rt.memory().store16(frame.at(0x22Cu), static_cast<std::uint16_t>(third));
        put(rt, frame.at(0x230u), self);
        const u32 handler_table = word(rt, ctx.gpr[17]);
        ctx.gpr[16] = word(rt, self + 0xA8u);
        const u32 owner = call(rt, ctx, 0x08B08AC0u, {ctx.gpr[16]});
        const u32 handler = word(rt, handler_table + 0x28u);
        ctx.gpr[19] = handler;
        const u32 ok = call_indirect(rt, ctx, handler,
                                     {ctx.gpr[17], frame.at(0u), ctx.gpr[16], owner, second, flag});
        if (ok != 0u) {
            put(rt, self + 0x574u, 0u);
            put(rt, self + 0x560u, 0u);
            put(rt, self + 0x564u, 0u);
            put(rt, self + 0x568u, 0u);
            put(rt, self + 0x570u, 0u);
            result = 1u;
        }
    }
    ctx.gpr[2] = result;
}

// 0x08978248 (a0 = a scratch object, a1 = a scene, a2 = a count, a3 = a four-pointer block, f12 = a value): when a2 is
// below 4 only the destructor of the scratch object at sp runs. Otherwise the four pointers at a3.. are each checked
// (0x08A5BB7C(0x08CA2F94) for the scene, 0x08CA2F84 for a pointer) and their virtual +0x54 read: the four floats go to
// 0x089772E0(the scratch object's word, f12..f15) and then the scratch object is released with 0x08A5B9EC(sp).
void func_08978248(Runtime &rt, AllegrexContext &ctx) {
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 block = ctx.gpr[7];
    ctx.gpr[17] = block;
    const bool short_form = static_cast<s32>(ctx.gpr[6]) < 4;
    call(rt, ctx, 0x08978000u, {frame.at(0u), ctx.gpr[5], ctx.gpr[6], block});
    if (!short_form) {
        const u32 scene = word(rt, frame.at(0u));
        ctx.gpr[18] = scene;
        if (scene == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA2F94u});
        u32 object = word(rt, block);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});
        const GuestResult first_answer = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x54u), {object}, {});
        const float f22 = first_answer.f0;
        object = word(rt, block + 4u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});
        const GuestResult second = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x54u), {object}, {});
        object = word(rt, block + 8u);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});
        const GuestResult third = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x54u), {object}, {});
        object = word(rt, block + 0xCu);
        if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA2F84u});
        const GuestResult fourth = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x54u), {object}, {});
        call_floats(rt, ctx, 0x089772E0u, {scene}, {f22, second.f0, third.f0, fourth.f0});
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x089CDC0C (a0 = this, a1 = an object, a2 = a key): with a key, 0x089BB6C0(sp + 8, key) then 0x08969244(sp + 0xC, sp + 8)
// and the release of sp + 8; the object at sp + 0xC must be there (0x08A5BB7C(0x08CA6984) otherwise) and 0x0896A7B0 takes
// it with 1, 0x089CDA4C(this, object, sp + 0xC) and 0x089CD49C(sp + 4, this) run, the object at sp + 4 is released with
// 0x0896A35C(sp + 4, 0, 0, this + 0x158, this + 0x15C) after its 0x08CA6984 check, and sp + 4 and sp + 0xC are released.
// Without a key 0x08969188(sp, 0x08CF6ED8), 0x089CDA4C(this, object, sp) and the release of sp run. v0 is the last
// 0x08A5B9EC answer.
void func_089CDC0C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    ctx.gpr[18] = object;
    ctx.gpr[17] = self;
    if (key != 0u) {
        call(rt, ctx, 0x089BB6C0u, {frame.at(8u), key});
        call(rt, ctx, 0x08969244u, {frame.at(0xCu), frame.at(8u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        const u32 held = word(rt, frame.at(0xCu));
        if (held == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6984u});
        call(rt, ctx, 0x0896A7B0u, {held, 1u});
        call(rt, ctx, 0x089CDA4Cu, {self, object, frame.at(0xCu)});
        call(rt, ctx, 0x089CD49Cu, {frame.at(4u), self});
        const u32 other = word(rt, frame.at(4u));
        if (other == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6984u});
        call(rt, ctx, 0x0896A35Cu, {other, 0u, 0u, self + 0x158u, self + 0x15Cu});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    } else {
        call(rt, ctx, 0x08969188u, {frame.at(0u), 0x08CF6ED8u});
        call(rt, ctx, 0x089CDA4Cu, {self, object, frame.at(0u)});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    }
}

// 0x08B3E908 (a0 = this): the 0x320 bytes at this are copied to this + 0x320, then the word(this + 0x644) - 1 entries
// after it are ordered: entry i's (word, word + 4) is compared with the entries after it as `sign-extended word i` against
// their 64-bit (low, high) pair, the first one that is not below it is moved to slot i (its low word and sign), and entry
// i goes to that slot. Answers the address (this + 8 * slot) of the last move, or word(this + 0x644) - 1 when that is <= 0.
void func_08B3E908(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    for (u32 source = self, destination = self + 0x320u; source != self + 0x320u;
         source += 0x10u, destination += 0x10u) {
        put(rt, destination, word(rt, source));
        put(rt, destination + 4u, word(rt, source + 4u));
        put(rt, destination + 8u, word(rt, source + 8u));
        put(rt, destination + 0xCu, word(rt, source + 0xCu));
    }
    const u32 count = word(rt, self + 0x644u);
    const u32 limit = count - 1u;
    if (static_cast<s32>(limit) <= 0) {
        ctx.gpr[2] = limit;
        return;
    }
    u32 cursor = 0u;
    for (;;) {
        const u32 slot = self + 0x320u + cursor;
        const u32 low = word(rt, slot);
        const u32 high = word(rt, slot + 4u);
        const u32 following = cursor / 8u + 1u;
        u32 position = cursor / 8u;
        u32 best_low = low;
        u32 best_high = static_cast<u32>(static_cast<s32>(low) >> 31);
        if (following < count) {
            for (u32 scan = following;; ++scan) {
                const u32 address = self + 0x320u + (scan << 3);
                const u32 other_low = word(rt, address);
                const u32 other_high = word(rt, address + 4u);
                bool better;
                if (static_cast<s32>(best_high) < static_cast<s32>(other_high)) {
                    better = true;
                } else if (best_high != other_high) {
                    better = false;
                } else {
                    better = best_low < other_low;
                }
                if (better) {
                    best_high = static_cast<u32>(static_cast<s32>(other_low) >> 31);
                    best_low = other_low;
                    position = scan;
                }
                if (scan + 1u == count) break;
            }
        } else {
            position = cursor / 8u;
            best_low = low;
            best_high = static_cast<u32>(static_cast<s32>(low) >> 31);
        }
        const u32 target = self + (position << 3);
        put(rt, target + 0x320u, low);
        put(rt, target + 0x324u, high);
        put(rt, slot, best_low);
        put(rt, slot + 4u, best_high);
        ctx.gpr[2] = target;
        cursor += 8u;
        if (!(following < limit)) break;
    }
}

// 0x08B66D54 (f12 = a value): 0x08B6A1AC() answers f21 (the caller's registers are passed on). With the mode word at
// 0x08C96698 not -1 and 0x08B6ECB0(f12 = the value) answering 0, 0x08B663A4(f12 = the value) is compared with the float at
// 0x08CB62B4: above it the 0x28-byte request at sp is built (word flags 1, 1, 0, 0, 0, the table 0x08CB5E3C at sp + 4 and
// 0x08B70F28's answer at sp + 8 and sp + 0x10) and 0x08B6E524(sp) decides: with word(sp + 0x20) set, 0x08B77D40 answers a
// slot that gets that word and 0x08B71EC8(word(sp + 0x18), word(sp + 0x1C)) finishes; without it the slot gets 0x21 first.
// A mode of 2 goes straight to the 0x21 path. Answers f0 (f21 on the early exits).
void func_08B66D54(Runtime &rt, AllegrexContext &ctx) {
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x50u);
    const float f20 = ctx.fpr[12];
    const u32 argument0 = ctx.gpr[4];
    const u32 argument1 = ctx.gpr[5];
    const u32 argument2 = ctx.gpr[6];
    const u32 argument3 = ctx.gpr[7];
    float f21 = call_floats(rt, ctx, 0x08B6A1ACu, {argument0, argument1, argument2, argument3}, {f20}).f0;
    const u32 mode = word(rt, 0x08C96698u);
    if (mode == 0xFFFFFFFFu) {
        ctx.fpr[0] = f21;
        return;
    }
    if (call(rt, ctx, 0x08B6ECB0u, {argument0, argument1, argument2, argument3}) != 0u) {
        ctx.fpr[0] = f21;
        return;
    }
    const float probe = call_floats(rt, ctx, 0x08B663A4u, {argument0, argument1, argument2, argument3}, {f20}).f0;
    if (!(float_at(rt, 0x08CB62B4u) < probe)) {
        ctx.fpr[0] = f21;
        return;
    }
    put(rt, frame.at(0u), 1u);
    put(rt, frame.at(4u), 0x08CB5E3Cu);
    put(rt, frame.at(0x20u), 0u);
    const u32 request = call_floats(rt, ctx, 0x08B70F28u, {argument0, argument1, argument2, argument3}, {f20}).v0;
    put(rt, frame.at(8u), request);
    put(rt, frame.at(0x10u), request);
    put(rt, frame.at(0xCu), 1u);
    put(rt, frame.at(0x14u), 1u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    bool skipped = mode == 2u;
    if (!skipped && call(rt, ctx, 0x08B6E524u, {frame.at(0u)}) == 0u) skipped = true;
    if (skipped) {
        const u32 slot = call(rt, ctx, 0x08B77D40u, {});
        put(rt, slot, 0x21u);
    }
    const u32 reported = word(rt, frame.at(0x20u));
    if (reported != 0u) {
        const u32 slot = call(rt, ctx, 0x08B77D40u, {});
        put(rt, slot, reported);
    }
    f21 = call_floats(rt, ctx, 0x08B71EC8u, {word(rt, frame.at(0x18u)), word(rt, frame.at(0x1Cu))}, {}).f0;
    ctx.fpr[0] = f21;
}

// 0x08B73D9C (a0 = a scene, a1 = a holder): a table of 306 bytes from 0x08D72BB4 is walked against the words at a0 and
// word(a1 + 0x74), four bytes at a time; a pair of non-zero, different words goes to 0x08B79A88(left, right, byte). When
// word(a0 + 0x74) is zero the answer is (word(word(a1 + 0x74)) - word(a0 + 0x4CC)) + word(a1 + 0x4E4); when it is not,
// zero. A byte 4 at table + 0x1D instead makes a1's + 0x74 point at a1 + 0x4CC (first case) or reports through
// 0x08B77B04. v0 = a0 (zero) on the plain path.
void func_08B73D9C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 scene = ctx.gpr[4];
    u32 holder = ctx.gpr[5];
    ctx.gpr[21] = scene;
    ctx.gpr[20] = holder;
    ctx.gpr[22] = 0x08D72BB4u;
    if (word(rt, holder + 0x74u) == 0u) {
        if (rt.memory().load8(0x08D72BB4u + 0x1Du) != 4u) {
            call(rt, ctx, 0x08B77B04u, {word(rt, holder + 0x4CCu)});
            return;
        }
        put(rt, frame.at(0u), word(rt, holder + 0x4CCu));
        put(rt, holder + 0x74u, frame.at(0u));
    }
    u32 left = scene;
    u32 right = holder;
    u32 table = 0x08D72BB4u;
    const u32 end = 0x08D72CE6u;
    do {
        const u32 argument1 = word(rt, right);
        const u32 argument0 = word(rt, left);
        if (argument1 != 0u && argument0 != 0u && argument0 != argument1)
            call(rt, ctx, 0x08B79A88u, {argument0, argument1, rt.memory().load8(table)});
        table += 1u;
        left += 4u;
        right += 4u;
    } while (table != end);
    if (word(rt, scene + 0x74u) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (rt.memory().load8(0x08D72BB4u + 0x1Du) != 4u) {
        call(rt, ctx, 0x08B77B04u, {word(rt, holder + 0x74u)});
        return;
    }
    const u32 answer = (word(rt, word(rt, holder + 0x74u)) - word(rt, scene + 0x4CCu)) + word(rt, holder + 0x4E4u);
    ctx.gpr[2] = answer;
}

// 0x08BC97C8, 0x08BD2378 and 0x08BD5108 (a0 = this, the vtable word differs): this's word + 0x10 is released with
// 0x08B34888; while its word + 0x2C is zero, 0x08B34B24 runs, the object at word(holder + 0x38) is released through its
// first virtual with word(holder + 0x34) and both words cleared, then 0x08B28BE0 and 0x08B64654; otherwise the count at
// +0x2C drops, and at zero the pair at +0x34/+0x38 moves out (the second released through its virtual +8 with the first,
// the first through its virtual +0) and 0x08B34C1C runs when the byte at +0x30 is set. The vtable 0x08CBF488 finishes and
// is v0.
void release_holder(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    ctx.gpr[17] = self;
    const u32 holder = word(rt, self + 0x10u);
    ctx.gpr[16] = holder;
    put(rt, self, vtable);
    call(rt, ctx, 0x08B34888u, {holder});
    if (word(rt, holder + 0x2Cu) == 0u) {
        call(rt, ctx, 0x08B34B24u, {holder});
        ctx.gpr[18] = holder + 0x34u;
        const u32 held = word(rt, holder + 0x38u);
        if (held != 0u) {
            call_indirect(rt, ctx, word(rt, word(rt, held)), {held, word(rt, holder + 0x34u)});
            put(rt, holder + 0x38u, 0u);
            put(rt, holder + 0x34u, 0u);
        }
        call(rt, ctx, 0x08B28BE0u, {holder});
        call(rt, ctx, 0x08B64654u, {holder});
    } else {
        u32 first = 0u;
        u32 second = 0u;
        put(rt, holder + 0x2Cu, word(rt, holder + 0x2Cu) - 1u);
        if (word(rt, holder + 0x2Cu) == 0u) {
            const u32 held = word(rt, holder + 0x34u);
            if (held != 0u) {
                second = word(rt, holder + 0x38u);
                first = held;
                put(rt, holder + 0x34u, 0u);
                put(rt, holder + 0x38u, 0u);
            }
            if (rt.memory().load8(holder + 0x30u) != 0u) call(rt, ctx, 0x08B34C1Cu, {holder});
        }
        call(rt, ctx, 0x08B34B24u, {holder});
        if (first != 0u) call_indirect(rt, ctx, word(rt, word(rt, first) + 8u), {first});
        if (second != 0u) call_indirect(rt, ctx, word(rt, word(rt, second)), {second, first});
    }
    put(rt, self, 0x08CBF488u);
    ctx.gpr[2] = 0x08CBF488u;
}
void func_08BC97C8(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    release_holder(rt, ctx, 0x08CC5240u);
}
void func_08BD2378(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    release_holder(rt, ctx, 0x08CC53E0u);
}
void func_08BD5108(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    release_holder(rt, ctx, 0x08CC5570u);
}

// 0x08BE427C (a0 = a list head): each node of the circular list from word(a0) is released. Its child chain at node + 0x18
// (next through +4) is walked: 0x08BBD1B8(node + 0x18, &child) answers a pair kept in the stack block, then the child's
// virtual +4 runs. The pointer at node + 0x10 has its count at +8 dropped and, at zero, is released through its virtual
// +0xC; the word at node + 8, unless it is the empty-string marker 0x08D75298, is released with 0x08B60804(word - 4, -1)
// and, when that answers <= 0, 0x08B93554(word - 0xC, sp); then 0x08B35254(node).
void func_08BE427C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    const u32 head = ctx.gpr[4];
    ctx.gpr[21] = head;
    u32 node = word(rt, head);
    if (node == head) return;
    ctx.gpr[22] = 0x08D75298u;
    ctx.gpr[23] = 1u;
    for (;;) {
        const u32 next = word(rt, node);
        u32 child = word(rt, node + 0x18u);
        const u32 slot = node + 8u;
        ctx.gpr[19] = node;
        ctx.gpr[18] = slot;
        while (child != 0u) {
            const u32 following = word(rt, child + 4u);
            put(rt, frame.at(4u), child);
            const GuestResult pair = call_result(rt, ctx, 0x08BBD1B8u, {slot + 0x10u, frame.at(4u)});
            put(rt, frame.at(8u), pair.v0);
            put(rt, frame.at(0xCu), pair.v0);
            put(rt, frame.at(0x10u), pair.v1);
            put(rt, frame.at(0x20u), pair.v0);
            put(rt, frame.at(0x24u), pair.v1);
            call_indirect(rt, ctx, word(rt, word(rt, child) + 4u), {child});
            ctx.gpr[17] = child;
            child = following;
        }
        const u32 held = word(rt, slot + 8u);
        if (held != 0u) {
            const u32 count = word(rt, held + 8u);
            put(rt, held + 8u, count - 1u);
            if (count == 1u) call_indirect(rt, ctx, word(rt, word(rt, held) + 0xCu), {held});
        }
        const u32 text = word(rt, slot);
        if (text - 0xCu != 0x08D75298u) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B93554u, {text - 0xCu, frame.at(0u)});
        }
        call(rt, ctx, 0x08B35254u, {node});
        if (next == head) break;
        node = next;
    }
}

// 0x088331F4 (a0 = an object, a1 = a name, a2 = a count, a3 = a pointer to it): with a positive count the target's virtual
// +0x54 is read (f20) and 0x088297A8(sp + 8, name) -> the string at sp + 8 (0x08A5BB7C(0x08C9A2F8) when missing) ->
// 0x088294D0 -> 0x0885EDF8, and 1000 * f20 truncated goes to its +0x648 as a float. Without one 0x088297A8(sp + 4, name),
// 0x088294D0, 0x0885EDEC -> (word + 0x648 as an integer) / 1000, 0x08A87EEC(sp, f12) and, when a0 is not sp, 0x08A5BAD8(a0,
// word(sp)). The string at sp + 4 or sp + 8 is released with 0x08A5B9EC either way.
void func_088331F4(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 target = ctx.gpr[7];
    ctx.gpr[18] = self;
    ctx.gpr[17] = name;
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        const u32 holder = word(rt, target);
        if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
        const float f20 = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x54u), {holder}, {}).f0;
        const u32 slot = frame.at(8u);
        call(rt, ctx, 0x088297A8u, {slot, name});
        const u32 text = word(rt, slot);
        if (text == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
        const u32 converted = call(rt, ctx, 0x088294D0u, {text});
        call(rt, ctx, 0x08A5B9ECu, {slot});
        const u32 record = call(rt, ctx, 0x0885EDF8u, {converted});
        put_float(rt, record + 0x648u, static_cast<float>(trunc_w_s(psp_mul(f20, 1000.0f))));
        return;
    }
    const u32 slot = frame.at(4u);
    call(rt, ctx, 0x088297A8u, {slot, name});
    const u32 text = word(rt, slot);
    if (text == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const u32 converted = call(rt, ctx, 0x088294D0u, {text});
    call(rt, ctx, 0x08A5B9ECu, {slot});
    const u32 record = call(rt, ctx, 0x0885EDECu, {converted});
    const float integer = static_cast<float>(static_cast<s32>(std::bit_cast<std::uint32_t>(float_at(rt, record + 0x648u))));
    call_floats(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {integer / 1000.0f});
    if (self != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// 0x088D4014 (a0 = an output record, a1 = a holder, a2 = an index): 0x088D2F9C(sp) and 0x088DB5C4(word(a1)) run, then
// 0x088D9F80(word(a1)) answers a count. With 0x088DB5C4's answer zero, 0x088D3E98(a0) runs and a0 comes back. Otherwise
// the count (1 when it is <= 0) and the index clamped below it scale 0x088FB028's answer, divided by the count, and
// 0x088D390C(word(word(a1) + 0x61C), sp, sp + 0xC, 6) fills the block that the record copies; its byte +0x20 is 1.
void func_088D4014(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x60u);
    const u32 output = ctx.gpr[4];
    const u32 holder = ctx.gpr[5];
    u32 index = ctx.gpr[6];
    ctx.gpr[19] = holder;
    ctx.gpr[18] = output;
    ctx.gpr[17] = index;
    const u32 requested = index;
    call(rt, ctx, 0x088D2F9Cu, {frame.at(0u), holder, index, ctx.gpr[7]});
    const u32 count = call(rt, ctx, 0x088DB5C4u, {word(rt, holder), holder, index, ctx.gpr[7]});
    const u32 total = call(rt, ctx, 0x088D9F80u, {word(rt, holder), holder, index, ctx.gpr[7]});
    if (count == 0u) {
        call(rt, ctx, 0x088D3E98u, {output, holder, index, ctx.gpr[7]});
        ctx.gpr[2] = output;
        return;
    }
    float scale;
    if (static_cast<s32>(total) > 0) {
        scale = static_cast<float>(static_cast<s32>(total));
    } else {
        scale = 1.0f;
    }
    const u32 limit = total - 1u;
    if (static_cast<s32>(index) < static_cast<s32>(total)) index = limit;
    ctx.gpr[17] = index;
    const float ratio =
        call_floats(rt, ctx, 0x088FB028u, {count, holder, requested, 0xFFFFFFFFu}, {}).f0 / scale;
    const float scaled = psp_mul(ratio, static_cast<float>(static_cast<s32>(index)));
    put_float(rt, frame.at(0x18u), scaled);
    call_floats(rt, ctx, 0x088D390Cu, {word(rt, word(rt, holder) + 0x61Cu), frame.at(0u), frame.at(0xCu), 6u},
                {scaled, 0.0f});
    put_float(rt, output, float_at(rt, frame.at(0u)));
    put_float(rt, output + 4u, float_at(rt, frame.at(4u)));
    put_float(rt, output + 8u, float_at(rt, frame.at(8u)));
    put_float(rt, output + 0xCu, float_at(rt, frame.at(0xCu)));
    put_float(rt, output + 0x10u, float_at(rt, frame.at(0x10u)));
    put_float(rt, output + 0x14u, float_at(rt, frame.at(0x14u)));
    put_float(rt, output + 0x18u, float_at(rt, frame.at(0x18u)));
    put(rt, output + 0x1Cu, word(rt, frame.at(0x1Cu)));
    rt.memory().store8(output + 0x20u, 1u);
    rt.memory().store8(frame.at(0x20u), 1u);
    ctx.gpr[2] = output;
}

// 0x08905BE0 (a0 = this): word(this) is replaced by this and the byte distance self - old word(this) is the argument of
// 21 member initialisers (0x08BD2BD8 for +4, +8, +0xC, +0x10, +0x14 and +0x78, 0x08BD2C68 for +0x18, 0x08BD2BF0 for +0x4C,
// 0x08BD2C94 for +0x64 and +0x68, 0x08BD2CB8 for +0x58, 0x08BD2C08 for +0x40, 0x08BD2CDC for +0x28..+0x3C,
// 0x08BD2D00 for +0x74, 0x08BD2C20 for +0x7C, 0x08BD2D24 for +0x80), then 0x08BD2C38(this + 0x84, distance) finishes.
void func_08905BE0(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    const u32 previous = word(rt, self);
    put(rt, self, self);
    ctx.gpr[17] = self - previous;
    const u32 distance = self - previous;
    call(rt, ctx, 0x08BD2BD8u, {self + 4u, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 8u, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 0xCu, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 0x10u, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 0x14u, distance});
    call(rt, ctx, 0x08BD2C68u, {self + 0x18u, distance});
    call(rt, ctx, 0x08BD2BF0u, {self + 0x4Cu, distance});
    call(rt, ctx, 0x08BD2C94u, {self + 0x64u, distance});
    call(rt, ctx, 0x08BD2C94u, {self + 0x68u, distance});
    call(rt, ctx, 0x08BD2CB8u, {self + 0x58u, distance});
    call(rt, ctx, 0x08BD2C08u, {self + 0x40u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x28u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x2Cu, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x30u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x34u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x38u, distance});
    call(rt, ctx, 0x08BD2CDCu, {self + 0x3Cu, distance});
    call(rt, ctx, 0x08BD2D00u, {self + 0x74u, distance});
    call(rt, ctx, 0x08BD2BD8u, {self + 0x78u, distance});
    call(rt, ctx, 0x08BD2C20u, {self + 0x7Cu, distance});
    call(rt, ctx, 0x08BD2D24u, {self + 0x80u, distance});
    tail_call(ctx, 0x08BD2C38u, {self + 0x84u, distance});
}

// 0x0892A454 (a0 = a vector, a1 = a record, a2 = unused): 0x0892A424(sp + 4, a1) prepares, the record's index word(a1) is
// kept and compared (unsigned) with (word(a0 + 4) - word(a0)) / 28: below it 0x08BDEE7C(a0, word(a0) + index * 28,
// word(a0 + 4)) runs, otherwise 0x08BDF434(a0, index - count, sp + 4). Then 0x08BDD7C8(word(sp + 0x14), word(sp + 0x18),
// sp + 1), the release of word(sp + 0x14), 0x08BDD7B8(word(sp + 8), word(sp + 0xC), sp) and the release of word(sp + 8);
// the record's own table (word(a1)) is walked with 0x0892A37C(word(a0) + i * 0x1C, word(word(a1 + 4) + i * 4)).
void func_0892A454(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    const u32 record = ctx.gpr[5];
    ctx.gpr[19] = self;
    ctx.gpr[18] = record;
    ctx.gpr[17] = frame.at(4u);
    const u32 index = word(rt, record);
    call(rt, ctx, 0x0892A424u, {frame.at(4u), record, ctx.gpr[6], ctx.gpr[7]});
    const u32 count = (static_cast<u32>(static_cast<s32>(word(rt, self + 4u) - word(rt, self)) >> 2) * 0xB6DB6DB7u);
    if (index < count) {
        call(rt, ctx, 0x08BDEE7Cu, {self, word(rt, self) + index * 28u, word(rt, self + 4u)});
    } else {
        call(rt, ctx, 0x08BDF434u, {self, index - count, frame.at(4u)});
    }
    call(rt, ctx, 0x08BDD7C8u, {word(rt, frame.at(0x14u)), word(rt, frame.at(0x18u)), frame.at(1u)});
    if (word(rt, frame.at(0x14u)) != 0u) call(rt, ctx, 0x08B35254u, {word(rt, frame.at(0x14u))});
    call(rt, ctx, 0x08BDD7B8u, {word(rt, frame.at(8u)), word(rt, frame.at(0xCu)), frame.at(0u)});
    if (word(rt, frame.at(8u)) != 0u) call(rt, ctx, 0x08B35254u, {word(rt, frame.at(8u))});
    u32 remaining = word(rt, record);
    if (remaining != 0u) {
        u32 position = 0u;
        u32 target = 0u;
        do {
            call(rt, ctx, 0x0892A37Cu, {word(rt, self) + target, word(rt, word(rt, record + 4u) + position * 4u)});
            position += 1u;
            remaining = word(rt, record);
            target += 0x1Cu;
        } while (position < remaining);
    }
}

// 0x0898B0E0 (a0 = an output name, a1 = a holder): 0x089896CC(sp + 0xC, a1) answers the record at sp + 0xC (0x08CA39F0 is
// reported when it is missing), whose floats +0x1EC and +0x1F0 are kept. 0x08A98C1C(sp + 8, 2) formats one, the output name
// is released (0x08A5BAD8 when it is not sp + 8) and 0x08A5B9EC(sp + 8) runs. word(a0) begins a second string: 0x08A87EEC
// (sp + 4, f20) then the virtual at word(word(a0)+0x88) with (a0, 0, sp + 4), released; the same with f21 into sp and the
// virtual with (a0, 1, sp), then sp, sp + 8 and sp + 0xC are released.
void func_0898B0E0(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 output = ctx.gpr[4];
    ctx.gpr[18] = output;
    ctx.gpr[20] = frame.at(0xCu);
    ctx.gpr[19] = frame.at(4u);
    ctx.gpr[17] = frame.at(8u);
    call(rt, ctx, 0x089896CCu, {frame.at(0xCu), ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    const u32 record = word(rt, frame.at(0xCu));
    if (record == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39F0u});
    const float f21 = float_at(rt, record + 0x1F0u);
    const float f20 = float_at(rt, record + 0x1ECu);
    call(rt, ctx, 0x08A98C1Cu, {frame.at(8u), 2u, ctx.gpr[6], ctx.gpr[7]});
    if (output != frame.at(8u)) call(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(8u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    const u32 holder = word(rt, output);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39E8u});
    const u32 handler = word(rt, word(rt, holder) + 0x88u);
    ctx.gpr[16] = handler;
    call_floats(rt, ctx, 0x08A87EECu, {frame.at(4u)}, {f20});
    call_indirect(rt, ctx, handler, {holder, 0u, frame.at(4u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    const u32 holder2 = word(rt, output);
    if (holder2 == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA39E8u});
    const u32 handler2 = word(rt, word(rt, holder2) + 0x88u);
    ctx.gpr[16] = handler2;
    call_floats(rt, ctx, 0x08A87EECu, {frame.at(0u)}, {f21});
    call_indirect(rt, ctx, handler2, {holder2, 1u, frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
}

// 0x0899000C (a0 = this): bit 14 of word(this + 0xE0) is taken out and kept, the words at +0x220 and +0x224 cleared, the
// block at +0x4A8..+0x4C0 zeroed (the word +0x4B4 is 1), the bytes at +0x1F4 and +0x166 set to 1, +0x168 and
// +0x16C cleared, this + 0x120 gets 1 and 0x40000 is ORed into +0xE0. Then the 0x80 slots from this + 0x228 are each
// formatted with 0x08B855E0 and their string released (0x08B60804(word - 4, -1) and 0x08B8542C(word - 0xC, sp) when it
// answers <= 0, the empty-string marker 0x08D74FD0 skipped). Bit 14 comes back at the end. v0 = the word it stores there.
void func_0899000C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[23] = self;
    ctx.gpr[19] = 0x80u;
    ctx.gpr[18] = 0u;
    ctx.gpr[17] = self + 0x228u;
    ctx.gpr[20] = frame.at(4u);
    const u32 flags = word(rt, self + 0xE0u);
    const u32 saved_bit = (flags >> 14) & 1u;
    ctx.gpr[30] = saved_bit;
    put(rt, self + 0xE0u, flags & ~(1u << 14));
    ctx.gpr[22] = 0x08D74FD0u;
    ctx.gpr[21] = 0x08D74FDCu;
    put(rt, self + 0x220u, 0u);
    put(rt, self + 0x224u, 0u);
    put(rt, self + 0x4A8u, 0u);
    put(rt, self + 0x4ACu, 0u);
    put(rt, self + 0x4B0u, 0u);
    put(rt, self + 0x4B4u, 1u);
    put(rt, self + 0x4B8u, 0u);
    put(rt, self + 0x4BCu, 0u);
    rt.memory().store8(self + 0x4C0u, 0u);
    rt.memory().store8(self + 0x1F4u, 1u);
    rt.memory().store8(self + 0x168u, 0u);
    put(rt, self + 0x16Cu, 0u);
    rt.memory().store8(self + 0x166u, 1u);
    put(rt, self + 0x120u, 1u);
    put(rt, self + 0xE0u, word(rt, self + 0xE0u) | 0x00040000u);
    u32 index = 0u;
    u32 slot = self + 0x228u;
    for (;;) {
        put(rt, frame.at(4u), 0x08D74FDCu);
        call(rt, ctx, 0x08B855E0u, {slot, frame.at(4u)});
        ++index;
        const u32 head = word(rt, frame.at(4u));
        slot += 4u;
        if (head - 0xCu != 0x08D74FD0u) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B8542Cu, {head - 0xCu, frame.at(0u)});
        }
        if (index == 0x80u) break;
    }
    const u32 final_flags = (word(rt, self + 0xE0u) & ~(1u << 14)) | (saved_bit << 14);
    put(rt, self + 0xE0u, final_flags);
    ctx.gpr[2] = final_flags;
}

// 0x08996CBC (a0 = this): word(this + 0x20) is cleared and 0x08996A18(sp + 0x18, this, f12 = 0) runs. Then 8 times
// (f = 0.125, 0.25, ..., 1.0) 0x08996A18(sp + 0x20, this, f) gives a 3-float vector that 0x08B4AC30(sp + 0x10,
// sp + 0x10, sp + 0x18) and 0x08B4A708(sp + 0x10) reduce to a length; it accumulates into this + 0x20 as f20 + sqrt. A last
// 0x08996A18(sp, this, 1.0) vector goes the same way and is added. f0 = the total.
void func_08996CBC(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    SavedFloats floats(ctx);
    GuestFrame frame(ctx, 0x60u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[17] = self;
    ctx.gpr[19] = frame.at(0x18u);
    ctx.gpr[20] = frame.at(0x20u);
    ctx.gpr[16] = frame.at(0x10u);
    ctx.gpr[18] = 7u;
    float f21 = 0.0f;
    const float step = std::bit_cast<float>(0x3E000000u);
    put_float(rt, self + 0x20u, 0.0f);
    call_floats(rt, ctx, 0x08996A18u, {frame.at(0x18u), self}, {0.0f});
    for (;;) {
        f21 += step;
        call_floats(rt, ctx, 0x08996A18u, {frame.at(0x20u), self}, {f21});
        put_float(rt, frame.at(0x10u), float_at(rt, frame.at(0x20u)));
        put_float(rt, frame.at(0x14u), float_at(rt, frame.at(0x24u)));
        const float accumulated = float_at(rt, self + 0x20u);
        call_floats(rt, ctx, 0x08B4AC30u, {frame.at(0x10u), frame.at(0x10u), frame.at(0x18u)}, {});
        const float length = call_floats(rt, ctx, 0x08B4A708u, {frame.at(0x10u)}, {}).f0;
        put_float(rt, frame.at(0x18u), float_at(rt, frame.at(0x20u)));
        put_float(rt, frame.at(0x1Cu), float_at(rt, frame.at(0x24u)));
        const float total = accumulated + std::sqrt(length);
        --ctx.gpr[18];
        put_float(rt, self + 0x20u, total);
        if (ctx.gpr[18] == 0u) break;
    }
    call_floats(rt, ctx, 0x08996A18u, {frame.at(0u), self}, {1.0f});
    put_float(rt, frame.at(8u), float_at(rt, frame.at(0u)));
    put_float(rt, frame.at(0xCu), float_at(rt, frame.at(4u)));
    call_floats(rt, ctx, 0x08B4AC30u, {frame.at(8u), frame.at(8u), frame.at(0x18u)}, {});
    const float last = call_floats(rt, ctx, 0x08B4A708u, {frame.at(8u)}, {}).f0;
    const float answer = float_at(rt, self + 0x20u) + std::sqrt(last);
    put_float(rt, self + 0x20u, answer);
    ctx.fpr[0] = answer;
}

// 0x08A3676C (a0 = a cursor object, a1 = a name, a2 = a count, a3 = a pointer): 0x08A354EC(sp + 0xC, a1). With a positive
// count the pointer's virtual +0xC(sp + 0x10, pointer) runs, 0x08A354EC(sp + 8, name), 0x08A35160(word(sp + 8),
// word(sp + 0x10)), the release of sp + 8 and the release of word(sp + 0x10) unless it is 0x08D74FD0 (0x08B60804 + maybe
// 0x08B8542C). With a count of zero 0x08A354EC(sp + 4, name), 0x08A35160(word(sp + 4), 0x08CAD024) and the release of
// sp + 4. The cursor object is released with 0x08A5B9EC either way.
void func_08A3676C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 cursor = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    ctx.gpr[19] = frame.at(0xCu);
    ctx.gpr[18] = name;
    call(rt, ctx, 0x08A354ECu, {frame.at(0xCu), name, ctx.gpr[6], ctx.gpr[7]});
    if (static_cast<s32>(count) > 0) {
        const u32 pointer = word(rt, ctx.gpr[7]);
        if (pointer == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACEB8u});
        call_indirect(rt, ctx, word(rt, word(rt, pointer) + 0xCu), {frame.at(0x10u), pointer});
        call(rt, ctx, 0x08A354ECu, {frame.at(8u), name, ctx.gpr[6], ctx.gpr[7]});
        const u32 text = word(rt, frame.at(8u));
        if (text == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACF04u});
        call(rt, ctx, 0x08A35160u, {text, word(rt, frame.at(0x10u))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
        const u32 head = word(rt, frame.at(0x10u));
        if (head - 0xCu != 0x08D74FD0u) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B8542Cu, {head - 0xCu, frame.at(0u)});
        }
    } else {
        call(rt, ctx, 0x08A354ECu, {frame.at(4u), name, ctx.gpr[6], ctx.gpr[7]});
        const u32 text = word(rt, frame.at(4u));
        if (text == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CACF04u});
        call(rt, ctx, 0x08A35160u, {text, 0x08CAD024u});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
}

// 0x08A8A034 (a0 = a cursor object, a1 = a name, a2 = a selector, a3 = a pointer): with selector 1 the pointer's virtual
// +0xC(sp + 4, pointer) runs, 0x08B855E0(sp + 8, sp + 4), the release of word(sp + 4) unless it is 0x08D74FD0 (0x08B60804
// + maybe 0x08B8542C(sp + 1)), then 0x08A89AA0(sp + 0xC, name) (0x08CB1B60 when it is missing), 0x08A89F54(word(sp + 0xC),
// sp + 8), and the release of sp + 0xC and of word(sp + 8) (0x08B60804 + maybe 0x08B8542C); without the selector
// 0x08A89AA0(sp + 0xC, name) and 0x08A89F54 go straight through. The cursor is not released here.
void func_08A8A034(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 name = ctx.gpr[5];
    ctx.gpr[19] = name;
    ctx.gpr[20] = frame.at(8u);
    ctx.gpr[18] = 0x08D74FD0u;
    if (ctx.gpr[6] == 1u) {
        const u32 pointer = word(rt, ctx.gpr[7]);
        if (pointer == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1AD4u});
        call_indirect(rt, ctx, word(rt, word(rt, pointer) + 0xCu), {frame.at(4u), pointer});
        call(rt, ctx, 0x08B855E0u, {frame.at(8u), frame.at(4u)});
        const u32 head = word(rt, frame.at(4u));
        if (head - 0xCu != 0x08D74FD0u) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B8542Cu, {head - 0xCu, frame.at(1u)});
        }
    }
    call(rt, ctx, 0x08A89AA0u, {frame.at(0xCu), name, ctx.gpr[6], ctx.gpr[7]});
    const u32 holder = word(rt, frame.at(0xCu));
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CB1B60u});
    call(rt, ctx, 0x08A89F54u, {holder, frame.at(8u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    const u32 head = word(rt, frame.at(8u));
    if (head - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {head - 0xCu, frame.at(0u)});
    }
}

// 0x08ACECFC (a0 = this): the vtable 0x08CD6B28 is stored; 0x08B2EE10(this + 4, 2, 0x300) and 0x08B2EE10(this + 0x18, 2,
// 0x303) build two tables, 0x08B2EDD4(this + 0x2C, 0x08CB49D4) a third, 0x08B0CC04(this + 0x40) runs, the bytes at
// this + 0x160 and +0x162 are cleared, 0x08B28DCC(this + 0x164), word(this + 0x244) = this + 0x240, the words at +0x300,
// +0x30C, +0x314, +0x31C, +0x328, +0x32C and +0x330 are cleared, 0x08B28D34 on +0x334, +0x360, +0x38C, +0x448 and +0x474,
// 0x08B28DCC on +0x3B8, +0x420 and +0x4A0, word(this + 0x510) = 4, +0x514 = -1, +0x518 = 0, 0x08B2EEB8 on +4, +0x18 and
// +0x2C, 0x08C7AD70() and word(this + 0x240) = 0.
void func_08ACECFC(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[16] = self;
    ctx.gpr[19] = self + 4u;
    ctx.gpr[18] = self + 0x18u;
    ctx.gpr[17] = self + 0x2Cu;
    put(rt, self, 0x08CD6B28u);
    call(rt, ctx, 0x08B2EE10u, {self + 4u, 2u, 0x300u});
    call(rt, ctx, 0x08B2EE10u, {self + 0x18u, 2u, 0x303u});
    call(rt, ctx, 0x08B2EDD4u, {self + 0x2Cu, 0x08CB49D4u});
    call(rt, ctx, 0x08B0CC04u, {self + 0x40u});
    rt.memory().store8(self + 0x160u, 0u);
    rt.memory().store8(self + 0x162u, 0u);
    call(rt, ctx, 0x08B28DCCu, {self + 0x164u});
    put(rt, self + 0x244u, self + 0x240u);
    put(rt, self + 0x300u, 0u);
    put(rt, self + 0x30Cu, 0u);
    put(rt, self + 0x314u, 0u);
    put(rt, self + 0x31Cu, 0u);
    put(rt, self + 0x328u, 0u);
    put(rt, self + 0x32Cu, 0u);
    put(rt, self + 0x330u, 0u);
    call(rt, ctx, 0x08B28D34u, {self + 0x334u});
    call(rt, ctx, 0x08B28D34u, {self + 0x360u});
    call(rt, ctx, 0x08B28D34u, {self + 0x38Cu});
    call(rt, ctx, 0x08B28DCCu, {self + 0x3B8u});
    call(rt, ctx, 0x08B28DCCu, {self + 0x420u});
    call(rt, ctx, 0x08B28D34u, {self + 0x448u});
    call(rt, ctx, 0x08B28D34u, {self + 0x474u});
    call(rt, ctx, 0x08B28DCCu, {self + 0x4A0u});
    put(rt, self + 0x510u, 4u);
    put(rt, self + 0x514u, 0xFFFFFFFFu);
    put(rt, self + 0x518u, 0u);
    call(rt, ctx, 0x08B2EEB8u, {self + 4u});
    call(rt, ctx, 0x08B2EEB8u, {self + 0x18u});
    call(rt, ctx, 0x08B2EEB8u, {self + 0x2Cu});
    call(rt, ctx, 0x08C7AD70u, {});
    put(rt, self + 0x240u, 0u);
}

// 0x08B03CE4 and 0x08B047D0 (a0 = this, a1/a2 and a3 = a record, t0 = a vtable holder): the signed halfword at a3 + 4 is
// the row count; a row is 84 bytes from word(this + 0x60) and the slot is word(this) + t2 * 240 (t2 being one of a1/a2).
// The walk starts at the row and follows the signed halfword at row + 0x52 as the next row index: a row whose byte +0x50
// is below 3 calls the virtual +8 of word(t0) with (t0, slot, call_second, float +0x34, float +0x24, float +0x2C) first.
// The 16 words at sp (0x0..0x3C) are filled with (word(sp + 0x10), this + 0x60, word(this + 0x60) + index * 84, 0).
void traverse_rows(Runtime &rt, AllegrexContext &ctx, u32 call_second, u32 stride, u32 first_offset, u32 second_offset,
                   u32 frame_base) {
    const u32 self = ctx.gpr[4];
    const u32 holder = ctx.gpr[8];
    ctx.gpr[9] = self;
    ctx.gpr[10] = stride;
    ctx.gpr[20] = call_second;
    ctx.gpr[19] = holder;
    const s32 rows = signed_half_at(rt, ctx.gpr[7] + 4u);
    if (rows < 0) return;
    const u32 slot = word(rt, self) + ctx.gpr[10] * 240u;
    const u32 base = word(rt, self + 0x60u);
    const u32 table = base + static_cast<u32>(rows) * 84u;
    ctx.gpr[17] = self + 0x60u;
    ctx.gpr[18] = slot;
    const u32 spare = word(rt, frame_base + 0x10u);
    put(rt, frame_base + 0x34u, ctx.gpr[17]);
    put(rt, frame_base + 0x38u, base);
    put(rt, frame_base + 0x3Cu, 0u);
    put(rt, frame_base + 0x24u, ctx.gpr[17]);
    put(rt, frame_base + 0x28u, base);
    put(rt, frame_base + 0x2Cu, 0u);
    put(rt, frame_base + 0x14u, ctx.gpr[17]);
    put(rt, frame_base + 0x18u, base);
    put(rt, frame_base + 0x1Cu, 0u);
    put(rt, frame_base + 0x4u, ctx.gpr[17]);
    put(rt, frame_base + 0x8u, base);
    put(rt, frame_base + 0xCu, 0u);
    put(rt, frame_base + 0x30u, spare);
    put(rt, frame_base + 0x20u, spare);
    put(rt, frame_base + 0u, spare);
    u32 row = table;
    for (;;) {
        if (row == 0u) return;
        if (rt.memory().load8(row + 0x50u) < 3u) {
            call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u),
                          {holder, slot, call_second, row + first_offset, row + second_offset},
                          {float_at(rt, row + 0x34u), float_at(rt, row + 0x24u), float_at(rt, row + 0x2Cu)});
        }
        const s32 next = signed_half_at(rt, row + 0x52u);
        if (next < 0) return;
        row = word(rt, self + 0x60u) + static_cast<u32>(next) * 84u;
    }
}
void func_08B03CE4(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    traverse_rows(rt, ctx, ctx.gpr[6], ctx.gpr[5], 0x18u, 0xCu, frame.at(0u));
}
void func_08B047D0(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    traverse_rows(rt, ctx, ctx.gpr[5], ctx.gpr[6], 0xCu, 0x18u, frame.at(0u));
}

// 0x08B136A8 (a0 = a 0x40-byte output record, a1 = a name, a2 and a3 kept in the record): a 0x10-byte request at sp + 0x58
// (buffer sp + 0x64, size 0x80, count 0) goes through 0x08B1262C and 0x08B23A70(0x08CB4BD4) and 0x08C7A848(sp) reads it;
// a negative answer answers 0. Otherwise the record is cleared (0x08B36018) and filled with word(sp + 8), word(sp + 0x10),
// +0x14, +0x18, +0x1C, +0x24 = word(sp + 0xC) and +0x30..+0x3C, then a2 and a3. A 60-byte hash loop over the table
// 0x08CD7EC8 (word(table + 4 * (byte ^ (hash >> 24))), hash = that ^ (hash << 8)) ends with the byte-swapped hash at
// the record's +0x3C. Answers 1, or 0 when the read failed.
void func_08B136A8(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x100u);
    const u32 record = ctx.gpr[4];
    ctx.gpr[16] = record;
    const u32 first = ctx.gpr[6];
    const u32 second = ctx.gpr[7];
    put(rt, frame.at(0x58u), frame.at(0x64u));
    put(rt, frame.at(0x5Cu), 0x80u);
    put(rt, frame.at(0x60u), 0u);
    rt.memory().store8(frame.at(0x64u), 0u);
    const u32 prepared = call(rt, ctx, 0x08B1262Cu, {frame.at(0x58u), ctx.gpr[5]});
    call(rt, ctx, 0x08B23A70u, {prepared, 0x08CB4BD4u});
    if (static_cast<s32>(call(rt, ctx, 0x08C7A848u, {word(rt, frame.at(0x58u)), frame.at(0u)})) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08B36018u, {record, 0u, 0x40u});
    put(rt, record + 0x20u, word(rt, frame.at(8u)));
    put(rt, record, word(rt, frame.at(0x10u)));
    put(rt, record + 4u, word(rt, frame.at(0x14u)));
    put(rt, record + 0x24u, word(rt, frame.at(0xCu)));
    put(rt, record + 8u, word(rt, frame.at(0x18u)));
    put(rt, record + 0xCu, word(rt, frame.at(0x1Cu)));
    put(rt, record + 0x10u, word(rt, frame.at(0x30u)));
    put(rt, record + 0x14u, word(rt, frame.at(0x34u)));
    put(rt, record + 0x18u, word(rt, frame.at(0x38u)));
    put(rt, record + 0x1Cu, word(rt, frame.at(0x3Cu)));
    put(rt, record + 0x28u, first);
    put(rt, record + 0x2Cu, second);
    u32 hash = 0u;
    for (u32 i = 0u; i < 0x3Cu; ++i)
        hash = word(rt, 0x08CD7EC8u + 4u * ((rt.memory().load8(record + i) ^ (hash >> 24)) & 0xFFu)) ^ (hash << 8);
    put(rt, record + 0x3Cu, __builtin_bswap32(hash));
    ctx.gpr[2] = 1u;
}

// 0x08BA7990 (a0 = this): the vtable 0x08CC1918 is stored, the records of 0xC below this + 0x4C are walked from the end
// with 0x08BA76B8(record, record + 4, &flag) and each held object released with 0x08B35254; then the vtable 0x08CC18E0,
// the counts at node + 4 and node + 8 of word(this + 0x1C) (each virtual +8 / +0xC run when one drops to zero),
// 0x08B8E5D8(word + 8, word + 0xC, sp), 0x08B35254(word(this + 8)), the vtable 0x08CBF518 and 0x08B64654(this).
void func_08BA7990(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    const u32 begin = self + 0x28u;
    ctx.gpr[18] = self;
    put(rt, self, 0x08CC1918u);
    ctx.gpr[19] = frame.at(1u);
    if (begin != 0u) {
        u32 node = self + 0x40u;
        if (self + 0x4Cu != begin) {
            for (;;) {
                call(rt, ctx, 0x08BA76B8u, {word(rt, node), word(rt, node + 4u), frame.at(1u)});
                const u32 held = word(rt, node);
                if (held != 0u) call(rt, ctx, 0x08B35254u, {held});
                if (node == begin) break;
                node -= 0xCu;
            }
        }
    }
    const u32 element = word(rt, self + 0x1Cu);
    put(rt, self, 0x08CC18E0u);
    if (element != 0u) {
        const u32 count = word(rt, element + 4u);
        put(rt, element + 4u, count - 1u);
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, element) + 8u), {element});
            const u32 other = word(rt, element + 8u);
            put(rt, element + 8u, other - 1u);
            if (other == 1u) call_indirect(rt, ctx, word(rt, word(rt, element) + 0xCu), {element});
        }
    }
    call(rt, ctx, 0x08B8E5D8u, {word(rt, self + 8u), word(rt, self + 0xCu), frame.at(0u)});
    const u32 member = word(rt, self + 8u);
    if (member != 0u) call(rt, ctx, 0x08B35254u, {member});
    put(rt, self, 0x08CBF518u);
    call(rt, ctx, 0x08B64654u, {self});
}

// 0x08BC207C (a0 = this): the 0xC word is the holder. When it has word +0x110, 0x08905A94(holder, 0) runs; then
// 0x08BC0750(word + 0x118, word + 0x11C, sp) and the release of word + 0x118; the counts at word + 0x114's +4 and +8 drop
// (the virtuals +8 and +0xC run at zero). Then this's word + 0x10, +0x6C is locked and 0x08B34888 runs, 0x08B2CE7C(record + 8,
// holder), 0x08B2D1A8(record + 8), and when its answer differs from word(record + 0x68) 0x08B34CA0(record + 0x6C);
// 0x08B34B24(record + 0x6C) and 0x08BBEA18(word(this + 0x10) + 0x98, record) finish.
void func_08BC207C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 holder = word(rt, ctx.gpr[4] + 0xCu);
    ctx.gpr[18] = holder;
    ctx.gpr[20] = ctx.gpr[4] + 0x10u;
    if (word(rt, holder + 0x110u) != 0u) call(rt, ctx, 0x08905A94u, {holder, 0u});
    call(rt, ctx, 0x08BC0750u, {word(rt, holder + 0x118u), word(rt, holder + 0x11Cu), frame.at(0u)});
    if (word(rt, holder + 0x118u) != 0u) call(rt, ctx, 0x08B35254u, {word(rt, holder + 0x118u)});
    const u32 object = word(rt, holder + 0x114u);
    if (object != 0u) {
        const u32 count = word(rt, object + 4u);
        put(rt, object + 4u, count - 1u);
        if (count == 1u) {
            call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
            const u32 other = word(rt, object + 8u);
            put(rt, object + 8u, other - 1u);
            if (other == 1u) call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
        }
    }
    const u32 record = word(rt, ctx.gpr[20]);
    ctx.gpr[19] = record + 0x6Cu;
    call(rt, ctx, 0x08B34888u, {record + 0x6Cu});
    ctx.gpr[17] = record + 8u;
    call(rt, ctx, 0x08B2CE7Cu, {record + 8u, holder});
    if (call(rt, ctx, 0x08B2D1A8u, {record + 8u}) != word(rt, record + 0x68u))
        call(rt, ctx, 0x08B34CA0u, {record + 0x6Cu});
    call(rt, ctx, 0x08B34B24u, {record + 0x6Cu});
    call(rt, ctx, 0x08BBEA18u, {word(rt, ctx.gpr[20]) + 0x98u, record});
}

// 0x08C2E3D8 (a0 = a member-pointer holder, a1 = an output string, a2 = an argument block, a3 and t0 = more): the
// pointer-to-member at holder + 0x18 is resolved (a virtual when bit 0 is set, else a plain offset into word(a2)) and
// called with 0x08AA27A4(sp + 0xC, t0) as its argument; the one at holder + 0x20 is resolved the same way and called with
// the answer. Then 0x08C2DC0C(sp + 8, sp + 4) converts, the output string is replaced (0x08A5BAD8), sp + 8 and sp + 4 are
// released and the string at sp + 0xC is released (0x08B60804 + maybe 0x08B8542C).
void func_08C2E3D8(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 holder = ctx.gpr[4];
    const u32 output = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    ctx.gpr[19] = frame.at(0xCu);
    ctx.gpr[18] = output;
    ctx.gpr[17] = holder;
    ctx.gpr[16] = argument;
    call(rt, ctx, 0x08AA27A4u, {frame.at(0xCu), ctx.gpr[8], ctx.gpr[6], ctx.gpr[7]});
    const u32 block = word(rt, argument);
    const u32 first_step = word(rt, holder + 0x18u);
    u32 first_target;
    if ((first_step & 1u) != 0u) {
        first_target = word(rt, word(rt, block + (first_step >> 1)) + word(rt, holder + 0x14u));
    } else {
        first_target = first_step >> 1;
    }
    const u32 answer = call_indirect(rt, ctx, first_target, {block + (first_step >> 1), word(rt, holder + 0x14u)});
    const u32 second_step = word(rt, holder + 0x20u);
    u32 second_target;
    if ((second_step & 1u) != 0u) {
        second_target = word(rt, word(rt, answer + (second_step >> 1)) + word(rt, holder + 0x1Cu));
    } else {
        second_target = word(rt, holder + 0x1Cu);
    }
    call_indirect(rt, ctx, second_target, {frame.at(4u), answer + (second_step >> 1), frame.at(0xCu)});
    call(rt, ctx, 0x08C2DC0Cu, {frame.at(8u), frame.at(4u)});
    if (output != frame.at(8u)) call(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(8u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    const u32 text = word(rt, frame.at(0xCu));
    if (text - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {text - 0xCu, frame.at(0u)});
    }
}

// 0x08836FD4 (a0 = an output string, a1 = a name, a2 = a count, a3 = a pointer, t0 = a second output): with a positive
// count the pointer's virtual +0xC(sp + 0xC, pointer) runs (0x08C9B798 when it is missing), 0x08B20A54(word(sp + 0xC),
// 0x08CE2BCC, -1) writes to t0 and word(sp + 0xC) is released (the 0x08D74FD0 marker skipped, 0x08B60804, maybe
// 0x08B8542C(sp + 1)). Without a count 0x08B20968(0x08CE2BCC, word(t0)) and 0x08B85EB8(sp + 4, answer, sp + 2) and
// 0x08A6D2D4(sp + 8, sp + 4) produce the string at sp + 8, which replaces a0 (0x08A5BAD8), and sp + 8 and word(sp + 4) are
// released.
void func_08836FD4(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 output = ctx.gpr[4];
    const u32 second_output = ctx.gpr[8];
    ctx.gpr[18] = output;
    ctx.gpr[17] = second_output;
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        const u32 pointer = word(rt, ctx.gpr[7]);
        if (pointer == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9B798u});
        call_indirect(rt, ctx, word(rt, word(rt, pointer) + 0xCu), {frame.at(0xCu), pointer});
        put(rt, second_output, call(rt, ctx, 0x08B20A54u, {word(rt, frame.at(0xCu)), 0x08CE2BCCu, 0xFFFFFFFFu}));
        const u32 head = word(rt, frame.at(0xCu));
        if (head - 0xCu != 0x08D74FD0u) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu})) <= 0)
                call(rt, ctx, 0x08B8542Cu, {head - 0xCu, frame.at(1u)});
        }
        return;
    }
    const u32 converted = call(rt, ctx, 0x08B20968u, {0x08CE2BCCu, word(rt, second_output)});
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), converted, frame.at(2u)});
    call(rt, ctx, 0x08A6D2D4u, {frame.at(8u), frame.at(4u)});
    if (output != frame.at(8u)) call(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(8u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    const u32 head = word(rt, frame.at(4u));
    if (head - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {head - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {head - 0xCu, frame.at(0u)});
    }
}

// 0x088EC42C (a0 = this, a1 = a parameter): a 0x40-byte block at sp + 0x28 is cleared and this's virtual +0x18(this, 0)
// answers a pair kept at sp + 0x48 / +0x4C with the byte at sp + 0x32 set to 0xFF. 0x08B0979C(), 0x08B09974() ->
// 0x08B7C830(sp + 0x33, answer, 0x11), 0x08B09F98(sp) and 0x08B09530(sp) prepare the scene, then the virtual +0x68 of
// word(this + 0x538) runs with (that object, the halfword at this + 0x584, 0x08CA0AC0, 0, sp + 0x28, 0). Its answer is
// s3, the virtual +0x90 answers the index, and the virtual +0x70(s3) the value stored at this + 0x598 + index * 4 and
// s3 at + 0x5A8 under the lock at this + 0xEC. 0x08B09290(sp) releases the block.
void func_088EC42C(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x90u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[17] = self;
    ctx.gpr[18] = frame.at(0x28u);
    call(rt, ctx, 0x08B36018u, {frame.at(0x28u), 0u, 0x40u});
    const GuestResult prepared = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x18u), {self, 0u}, {});
    put(rt, frame.at(0x48u), prepared.v0);
    put(rt, frame.at(0x4Cu), prepared.v1);
    rt.memory().store8(frame.at(0x32u), 0xFFu);
    call(rt, ctx, 0x08B0979Cu, {});
    const u32 text = call(rt, ctx, 0x08B09974u, {});
    call(rt, ctx, 0x08B7C830u, {frame.at(0x33u), text, 0x11u});
    call(rt, ctx, 0x08B09F98u, {frame.at(0u)});
    put(rt, frame.at(0x60u), call(rt, ctx, 0x08B09530u, {frame.at(0u)}));
    const u32 manager = word(rt, self + 0x538u);
    const u32 produced = call_indirect(rt, ctx, word(rt, word(rt, manager) + 0x68u),
                                       {manager, static_cast<u32>(signed_half_at(rt, self + 0x584u)), 0x08CA0AC0u, 0u,
                                        frame.at(0x28u), 0u});
    ctx.gpr[19] = produced;
    const u32 manager2 = word(rt, self + 0x538u);
    const u32 index = call_indirect(rt, ctx, word(rt, word(rt, manager2) + 0x90u), {manager2});
    const u32 manager3 = word(rt, self + 0x538u);
    ctx.gpr[16] = self + (index << 2);
    const u32 value = call_indirect(rt, ctx, word(rt, word(rt, manager3) + 0x70u), {manager3, produced});
    ctx.gpr[18] = value;
    call(rt, ctx, 0x08B344E0u, {self + 0xECu});
    put(rt, ctx.gpr[16] + 0x5A8u, value);
    put(rt, ctx.gpr[16] + 0x598u, produced);
    call(rt, ctx, 0x08B3457Cu, {self + 0xECu});
    call(rt, ctx, 0x08B09290u, {frame.at(0u)});
}

// 0x089B4614 (a0 = an output string, a1 = an object, a2 = a container): 0x08A7BDB0(object) answers a name that
// 0x08B85EB8(sp + 8, name, sp + 3) converts and 0x08B88F44(sp + 0xC, sp + 8) copies; 0x08BEED1C(sp + 0xC, 0x08CA573C, 2)
// appends the separator. 0x089BD5D4(container) + 0x130 is copied to sp + 4 and 0x08B89124(a0, sp + 0xC, sp + 4) joins
// them. sp + 4, sp + 0xC and sp + 8 are released (0x08B60804 + maybe 0x08B8542C per block). Answers a0.
void func_089B4614(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 output = ctx.gpr[4];
    const u32 container = ctx.gpr[6];
    ctx.gpr[19] = output;
    ctx.gpr[18] = container;
    ctx.gpr[17] = frame.at(0xCu);
    ctx.gpr[16] = frame.at(8u);
    const u32 name = call(rt, ctx, 0x08A7BDB0u, {ctx.gpr[5]});
    call(rt, ctx, 0x08B85EB8u, {frame.at(8u), name, frame.at(3u)});
    call(rt, ctx, 0x08B88F44u, {frame.at(0xCu), frame.at(8u)});
    call(rt, ctx, 0x08BEED1Cu, {frame.at(0xCu), 0x08CA573Cu, 2u});
    const u32 tail = call(rt, ctx, 0x089BD5D4u, {container});
    call(rt, ctx, 0x08B88F44u, {frame.at(4u), tail + 0x130u});
    call(rt, ctx, 0x08B89124u, {output, frame.at(0xCu), frame.at(4u)});
    const u32 third = word(rt, frame.at(4u));
    if (third - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {third - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {third - 0xCu, frame.at(2u)});
    }
    const u32 first = word(rt, frame.at(0xCu));
    if (first - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {first - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {first - 0xCu, frame.at(1u)});
    }
    const u32 second = word(rt, frame.at(8u));
    if (second - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {second - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {second - 0xCu, frame.at(0u)});
    }
    ctx.gpr[2] = output;
}

// 0x089CB2B4 (a0 = this): the vtables 0x08CC9FD8 and 0x08CCA230 (at +0x38) are stored and the four containers at
// +0x150, +0x144, +0x138 and +0x12C are released: 0x08C01F00 (the first two) or 0x08BEEA84 (the last two) on the block's
// begin and end, then 0x08A5BC54 on the block when its begin is set (the byte distance for the first two, the word count
// times four for the last two). 0x08A0DD40(this) finishes.
void func_089CB2B4(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[17] = self;
    ctx.gpr[20] = self + 0x12Cu;
    ctx.gpr[19] = self + 0x138u;
    ctx.gpr[18] = self + 0x144u;
    ctx.gpr[16] = self + 0x150u;
    put(rt, self, 0x08CC9FD8u);
    put(rt, self + 0x38u, 0x08CCA230u);
    call(rt, ctx, 0x08C01F00u, {word(rt, self + 0x150u), word(rt, self + 0x154u), frame.at(3u)});
    const u32 first = word(rt, self + 0x150u);
    if (first != 0u)
        call(rt, ctx, 0x08A5BC54u, {first, word(rt, self + 0x158u) - first, 4u});
    call(rt, ctx, 0x08C01F00u, {word(rt, self + 0x144u), word(rt, self + 0x148u), frame.at(2u)});
    const u32 second = word(rt, self + 0x144u);
    if (second != 0u)
        call(rt, ctx, 0x08A5BC54u, {second, word(rt, self + 0x14Cu) - second, 4u});
    call(rt, ctx, 0x08BEEA84u, {word(rt, self + 0x138u), word(rt, self + 0x13Cu), frame.at(1u)});
    const u32 third = word(rt, self + 0x138u);
    if (third != 0u) {
        const u32 count = (static_cast<u32>(static_cast<s32>(word(rt, self + 0x140u) - third) >> 2) << 2);
        call(rt, ctx, 0x08A5BC54u, {third, count, 4u});
    }
    call(rt, ctx, 0x08BEEA84u, {word(rt, self + 0x12Cu), word(rt, self + 0x130u), frame.at(0u)});
    const u32 fourth = word(rt, self + 0x12Cu);
    if (fourth != 0u) {
        const u32 count = (static_cast<u32>(static_cast<s32>(word(rt, self + 0x134u) - fourth) >> 2) << 2);
        call(rt, ctx, 0x08A5BC54u, {fourth, count, 4u});
    }
    call(rt, ctx, 0x08A0DD40u, {self});
}

// 0x08A39E64 (a0 = this, a1 = a buffer): 0x08B35F60(a1) answers the length. Up to 10 times: the remaining room is
// 0x100 - word(this + 0x20); when the whole buffer fits it is copied into this + 0x14 under the lock at this + 0x124
// (0x08C1B60C) and 0x08B2E2B8(0x4E20) finishes; otherwise 0x08B35DA0(room) takes that much, 0x08B7C830(copy, buffer,
// room) fills it, this + 0x14 gets it under the lock, 0x08B35254(copy) releases it, the buffer advances and the loop asks
// again after 0x08B2E2B8(0x4E20).
void func_08A39E64(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 self = ctx.gpr[4];
    u32 buffer = ctx.gpr[5];
    ctx.gpr[20] = self;
    ctx.gpr[19] = buffer;
    ctx.gpr[21] = self + 0x124u;
    ctx.gpr[23] = 0x100u;
    ctx.gpr[22] = 0xAu;
    u32 pending = call(rt, ctx, 0x08B35F60u, {buffer});
    ctx.gpr[18] = pending;
    for (;;) {
        const u32 room = 0x100u - word(rt, self + 0x20u);
        if (static_cast<s32>(room) > 0) {
            if (room < pending) {
                const u32 copy = call(rt, ctx, 0x08B35DA0u, {room});
                pending -= room;
                ctx.gpr[18] = pending;
                call(rt, ctx, 0x08B7C830u, {copy, buffer, room});
                ctx.gpr[16] = copy;
                call(rt, ctx, 0x08B344E0u, {ctx.gpr[21]});
                call(rt, ctx, 0x08C1B60Cu, {self + 0x14u, copy, room});
                call(rt, ctx, 0x08B3457Cu, {ctx.gpr[21]});
                call(rt, ctx, 0x08B35254u, {copy});
                buffer += room;
                ctx.gpr[19] = buffer;
            } else {
                call(rt, ctx, 0x08B344E0u, {ctx.gpr[21]});
                call(rt, ctx, 0x08C1B60Cu, {self + 0x14u, buffer, pending});
                call(rt, ctx, 0x08B3457Cu, {ctx.gpr[21]});
                tail_call(ctx, 0x08B2E2B8u, {0x4E20u});
                return;
            }
        }
        call(rt, ctx, 0x08B2E2B8u, {0x4E20u});
        ctx.gpr[22] -= 1u;
        if (ctx.gpr[22] == 0u) return;
    }
}

// 0x08A44E1C, 0x08A44F5C and 0x08A95378 (a0 = a record, a1 = an output string, a2 unused, a3 = a pointer): the virtual at
// +0xD8 of a0 and of word(a3) are compared (0x08CADDF4 / 0x08CB211C reported for the second when missing); equal ones run
// the virtual +0xDC(a0, 0) and the virtual +0xDC(pointer, 0), 0x08B36170 copies word(a3)'s text, 0x08A97A6C(sp + 4,
// answer < 1 or answer != 0) writes it to the output and sp + 4 is released; different ones write 0 or 1 through
// 0x08A97A6C(sp, ...). The block is released with 0x08A5B9EC either way.
void compare_records(Runtime &rt, AllegrexContext &ctx, u32 message, bool flag_when_not_less, u32 mismatch_value) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    const u32 record = ctx.gpr[4];
    const u32 output = ctx.gpr[5];
    const u32 pointer_slot = ctx.gpr[7];
    ctx.gpr[20] = output;
    ctx.gpr[19] = pointer_slot;
    ctx.gpr[18] = record;
    const u32 first = call_indirect(rt, ctx, word(rt, word(rt, record) + 0xD8u), {record, output});
    ctx.gpr[17] = first;
    const u32 pointer = word(rt, pointer_slot);
    if (pointer == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 second = call_indirect(rt, ctx, word(rt, word(rt, pointer) + 0xD8u), {pointer, output});
    if (first != second) {
        call(rt, ctx, 0x08A97A6Cu, {frame.at(0u), mismatch_value});
        if (output != frame.at(0u)) call(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(0u))});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        return;
    }
    const u32 left = call_indirect(rt, ctx, word(rt, word(rt, record) + 0xDCu), {record, 0u});
    ctx.gpr[21] = left;
    const u32 pointer2 = word(rt, pointer_slot);
    if (pointer2 == 0u) call(rt, ctx, 0x08A5BB7Cu, {message});
    const u32 right = call_indirect(rt, ctx, word(rt, word(rt, pointer2) + 0xDCu), {pointer2, 0u});
    const u32 compared = call(rt, ctx, 0x08B36170u,
                              {left, right, call_indirect(rt, ctx, word(rt, word(rt, record) + 0xD8u), {record, 0u})});
    if (flag_when_not_less) {
        call(rt, ctx, 0x08A97A6Cu, {frame.at(4u), compared != 0u ? 1u : 0u});
    } else {
        call(rt, ctx, 0x08A97A6Cu, {frame.at(4u), compared < 1u ? 1u : 0u});
    }
    if (output != frame.at(4u)) call(rt, ctx, 0x08A5BAD8u, {output, word(rt, frame.at(4u))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}
void func_08A44E1C(Runtime &rt, AllegrexContext &ctx) { compare_records(rt, ctx, 0x08CADDF4u, false, 0u); }
void func_08A44F5C(Runtime &rt, AllegrexContext &ctx) { compare_records(rt, ctx, 0x08CADDF4u, true, 1u); }
void func_08A95378(Runtime &rt, AllegrexContext &ctx) { compare_records(rt, ctx, 0x08CB211Cu, false, 0u); }

// 0x08A72364 (a0 = this): 0x08B85EB8(sp + 4, 0x08CB0AC8, sp + 1) formats the class name and the virtual +0x108(this, sp + 4)
// runs; the string at sp + 4 is released (0x08B60804 + maybe 0x08B8542C(sp)). 0x08A76A94() answers the new object (released
// with 0x08A5BA08 when nonzero) that replaces word(this + 0x38) (0x08A5B930 on the old one). Then the native registrations
// through 0x08B87108 (0x08CB0AD0 -> 0x08A71F70, 0x08CB0AD8 -> 0x08A7210C, 0x08CB0AE0 -> 0x08A71EC8, 0x08CB0AE8 ->
// 0x08A71F1C) and 0x08B8D51C(0x08CB0AF0, 0x08A724B4, 0).
void func_08A72364(Runtime &rt, AllegrexContext &ctx) {
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    ctx.gpr[18] = self;
    ctx.gpr[16] = frame.at(4u);
    ctx.gpr[17] = word(rt, word(rt, self) + 0x108u);
    call(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CB0AC8u, frame.at(1u)});
    call_indirect(rt, ctx, ctx.gpr[17], {self, frame.at(4u)});
    const u32 text = word(rt, frame.at(4u));
    if (text - 0xCu != 0x08D74FD0u) {
        if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0)
            call(rt, ctx, 0x08B8542Cu, {text - 0xCu, frame.at(0u)});
    }
    const u32 created = call(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = created;
    if (created != 0u) call(rt, ctx, 0x08A5BA08u, {created});
    const u32 previous = word(rt, self + 0x38u);
    if (previous != 0u) call(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, created);
    call(rt, ctx, 0x08B87108u, {self, 0x08CB0AD0u, 0x08A71F70u});
    call(rt, ctx, 0x08B87108u, {self, 0x08CB0AD8u, 0x08A7210Cu});
    call(rt, ctx, 0x08B87108u, {self, 0x08CB0AE0u, 0x08A71EC8u});
    call(rt, ctx, 0x08B87108u, {self, 0x08CB0AE8u, 0x08A71F1Cu});
    call(rt, ctx, 0x08B8D51Cu, {self, 0x08CB0AF0u, 0x08A724B4u, 0u});
}

} // namespace
} // namespace new_batch_20
} // namespace gt::game

#define NEW20(address, name, kind) GT_DECOMP(address, gt::game::new_batch_20::name, gt::ReturnKind::kind)
NEW20(0x08927D14, func_08927D14, Word);
NEW20(0x089396F8, func_089396F8, Word);
NEW20(0x08A4F784, func_08A4F784, Word);
NEW20(0x08A74B34, func_08A74B34, Word);
NEW20(0x08ACE7A8, func_08ACE7A8, Word);
NEW20(0x08AD31E0, func_08AD31E0, Word);
NEW20(0x08B1A284, func_08B1A284, Word);
NEW20(0x08BA8C48, func_08BA8C48, Word);
NEW20(0x08C2605C, func_08C2605C, Word);
NEW20(0x08C27F70, func_08C27F70, Word);
NEW20(0x0881BAC0, func_0881BAC0, Word);
NEW20(0x088CFB14, func_088CFB14, Float);
NEW20(0x088DE740, func_088DE740, Word);
NEW20(0x088EAC34, func_088EAC34, Word);
NEW20(0x08978248, func_08978248, Word);
NEW20(0x089CDC0C, func_089CDC0C, Word);
NEW20(0x08B3E908, func_08B3E908, Word);
NEW20(0x08B66D54, func_08B66D54, Float);
NEW20(0x08B73D9C, func_08B73D9C, Word);
NEW20(0x08BC97C8, func_08BC97C8, Word);
NEW20(0x08BD2378, func_08BD2378, Word);
NEW20(0x08BD5108, func_08BD5108, Word);
NEW20(0x08BE427C, func_08BE427C, Word);
NEW20(0x088331F4, func_088331F4, Word);
NEW20(0x088D4014, func_088D4014, Word);
NEW20(0x08905BE0, func_08905BE0, Word);
NEW20(0x0892A454, func_0892A454, Word);
NEW20(0x0898B0E0, func_0898B0E0, Word);
NEW20(0x0899000C, func_0899000C, Word);
NEW20(0x08996CBC, func_08996CBC, Float);
NEW20(0x08A3676C, func_08A3676C, Word);
NEW20(0x08A8A034, func_08A8A034, Word);
NEW20(0x08ACECFC, func_08ACECFC, Word);
NEW20(0x08B03CE4, func_08B03CE4, Word);
NEW20(0x08B047D0, func_08B047D0, Word);
NEW20(0x08B136A8, func_08B136A8, Word);
NEW20(0x08BA7990, func_08BA7990, Word);
NEW20(0x08BC207C, func_08BC207C, Word);
NEW20(0x08C2E3D8, func_08C2E3D8, Word);
NEW20(0x08836FD4, func_08836FD4, Word);
NEW20(0x088EC42C, func_088EC42C, Word);
NEW20(0x089B4614, func_089B4614, Word);
NEW20(0x089CB2B4, func_089CB2B4, Word);
NEW20(0x08A39E64, func_08A39E64, Word);
NEW20(0x08A44E1C, func_08A44E1C, Word);
NEW20(0x08A44F5C, func_08A44F5C, Word);
NEW20(0x08A95378, func_08A95378, Word);
NEW20(0x08A72364, func_08A72364, Word);
