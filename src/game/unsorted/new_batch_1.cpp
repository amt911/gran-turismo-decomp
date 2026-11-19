// New batch 1: functions that were still recompiled only, written as structured C from `tools.disasm` and verified by
// synthetic cases. These are the bounds-checked string members (the representation keeps its length in the word 12
// bytes before the character data, and a position past the length goes to the throw helper 0x08B5AEDC) and a few small
// callers around them.
// `v0` ends as the asm leaves it on every path: a result, the last call's result, or the value of the compare that chose
// the path (a callee that never writes v0 returns what it was called with, so it is kept current before every call).
// The delay slots are read as "before the branch / call". A throw helper that returns would fall into the next function
// in the original, so a path that ends in one just returns here.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_1 {
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

// The length word of a string representation: 12 bytes before its character data.
u32 length_of(Runtime &rt, u32 data) { return word(rt, data - 12u); }

constexpr u32 kThrowOutOfRange = 0x08B5AEDCu;
constexpr u32 kThrowLengthError = 0x08B5ADDCu;

// `jal 0x08B5AEDC` (the throw helper) with the message in a0, from a frame of 16 bytes; v0 is whatever the compare left.
void throw_out_of_range(Runtime &rt, AllegrexContext &ctx, u32 message, u32 compare_result) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = compare_result;
    call(rt, ctx, kThrowOutOfRange, {message});
}

// The unsigned minimum the asm builds with `sltu` and `movz`.
u32 minimum(u32 a, u32 b) { return b < a ? b : a; }

// 0x08C73530 (a0 = pointer to the string data, a1 = position, a2 = the throw helper's argument if a1 is past the length):
// returns the position, or throws.
void func_08C73530(Runtime &rt, AllegrexContext &ctx) {
    const u32 position = ctx.gpr[5];
    const bool past = length_of(rt, word(rt, ctx.gpr[4])) < position;
    ctx.gpr[4] = ctx.gpr[6];
    if (past) {
        throw_out_of_range(rt, ctx, ctx.gpr[4], 1u);
        return;
    }
    ctx.gpr[2] = position;
}

// 0x08C7359C (a0 = pointer to the string data, a1 = position): the address of the character at the position, or throws.
void func_08C7359C(Runtime &rt, AllegrexContext &ctx) {
    const u32 data = word(rt, ctx.gpr[4]);
    const u32 position = ctx.gpr[5];
    ctx.gpr[4] = data;
    if (position < length_of(rt, data)) {
        ctx.gpr[2] = data + position;
        return;
    }
    throw_out_of_range(rt, ctx, 0x08CB581Cu, 0u);
}

// 0x08C73EFC (a0 = pointer to the string data, a1 = position, a2 = count, a3 = the helper's argument): v0 = 0 when
// 0x3FFFFFFC - (length - position) is not below the count, else the length-error helper (0x08B5ADDC) is called with a3.
void func_08C73EFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 remaining = length_of(rt, word(rt, ctx.gpr[4])) - ctx.gpr[5];
    const bool too_long = 0x3FFFFFFCu - remaining < ctx.gpr[6];
    ctx.gpr[2] = too_long ? 1u : 0u;
    ctx.gpr[4] = ctx.gpr[7];
    if (too_long) {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, kThrowLengthError, {ctx.gpr[7]});
    }
}

// 0x08C75080 (a0 = string, a1 = position, a2 = count, a3, t0 = a signed byte): the count is cut to what the string has past the
// position and the work goes on in 0x08C74FAC (a tail call); a position past the length throws.
void func_08C75080(Runtime &rt, AllegrexContext &ctx) {
    const u32 length = length_of(rt, word(rt, ctx.gpr[4]));
    const u32 position = ctx.gpr[5];
    ctx.gpr[8] = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[8])));
    if (length < position) {
        throw_out_of_range(rt, ctx, 0x08CB58D8u, 1u);
        return;
    }
    const u32 rest = length - position;
    ctx.gpr[2] = ctx.gpr[6] < rest ? 1u : 0u;
    ctx.gpr[6] = minimum(ctx.gpr[6], rest);
    tail_call(ctx, 0x08C74FACu, {ctx.gpr[4], position, ctx.gpr[6], ctx.gpr[7], ctx.gpr[8]});
}

// 0x08C75118 (a0 = string, a1 = position, a2 = a value that moves to a3, a3 = a signed byte that moves to t0): the same
// continuation 0x08C74FAC with a count of 0, or a throw for a position past the length.
void func_08C75118(Runtime &rt, AllegrexContext &ctx) {
    const u32 data = word(rt, ctx.gpr[4]);
    const u32 length = length_of(rt, data);
    const u32 position = ctx.gpr[5];
    const u32 moved = ctx.gpr[6];
    ctx.gpr[9] = moved;
    ctx.gpr[3] = length;
    ctx.gpr[2] = data;
    const bool past = length < position;
    ctx.gpr[6] = past ? 1u : 0u;
    ctx.gpr[8] = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[7])));
    if (past) {
        throw_out_of_range(rt, ctx, 0x08CB58F0u, data);
        return;
    }
    ctx.gpr[6] = 0u;
    ctx.gpr[7] = moved;
    tail_call(ctx, 0x08C74FACu, {ctx.gpr[4], position, 0u, moved, ctx.gpr[8]});
}

// 0x08C74F24 (a0, a1 = pointer to a second string, a2 = position in it, a3 = count): the count is cut to what the second string
// has past the position and 0x08C74DD8 continues with the address of that character (a tail call); a position past the length throws.
void func_08C74F24(Runtime &rt, AllegrexContext &ctx) {
    const u32 position = ctx.gpr[6];
    const u32 count = ctx.gpr[7];
    const u32 data = word(rt, ctx.gpr[5]);
    const u32 length = length_of(rt, data);
    ctx.gpr[8] = position;
    ctx.gpr[5] = data;
    ctx.gpr[3] = length;
    if (length < position) {
        ctx.gpr[6] = count;
        throw_out_of_range(rt, ctx, 0x08CB58A0u, 1u);
        return;
    }
    const u32 rest = length - position;
    ctx.gpr[3] = rest;
    ctx.gpr[2] = count < rest ? 1u : 0u;
    ctx.gpr[6] = minimum(count, rest);
    ctx.gpr[5] = data + position;
    tail_call(ctx, 0x08C74DD8u, {ctx.gpr[4], ctx.gpr[5], ctx.gpr[6], count});
}

// 0x08C76930 (a0 = result, a1 = pointer to the string data, a2 = position, a3): a position past the length throws, else
// 0x08C768C8 does the work and the result is returned in v0.
void func_08C76930(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved_s0 = ctx.gpr[16];
    const u32 result = ctx.gpr[4];
    const u32 data = word(rt, ctx.gpr[5]);
    const u32 length = length_of(rt, data);
    ctx.gpr[2] = data;
    ctx.gpr[3] = length;
    const bool past = length < ctx.gpr[6];
    ctx.gpr[8] = past ? 1u : 0u;
    ctx.gpr[16] = result;
    if (past) {
        throw_out_of_range(rt, ctx, 0x08CB5968u, data);
    } else {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08C768C8u);
        ctx.gpr[2] = result;
    }
    ctx.gpr[16] = saved_s0;
}

// 0x08C75268 (a0 = string, a1 = position, a2 = count): the count is cut to what is left past the position, 0x08C74B68 is called
// with it and a 0 and the string is returned; a position past the length throws.
void func_08C75268(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved_s0 = ctx.gpr[16];
    const u32 string = ctx.gpr[4];
    const u32 length = length_of(rt, word(rt, string));
    const u32 position = ctx.gpr[5];
    ctx.gpr[3] = length;
    ctx.gpr[16] = string;
    if (length < position) {
        throw_out_of_range(rt, ctx, 0x08CB5908u, 1u);
    } else {
        const u32 rest = length - position;
        ctx.gpr[2] = ctx.gpr[6] < rest ? 1u : 0u;
        ctx.gpr[6] = minimum(ctx.gpr[6], rest);
        ctx.gpr[3] = rest;
        ctx.gpr[7] = 0u;
        {
            GuestFrame frame(ctx, 0x10u);
            call(rt, ctx, 0x08C74B68u, {string, position, ctx.gpr[6], 0u});
        }
        ctx.gpr[2] = string;
    }
    ctx.gpr[16] = saved_s0;
}

// The substring members (a0 = where the result goes, a1 = pointer to the source string data, a2 = position, a3 = count): a
// position past the length throws; else the count is cut to what is left and the copy helper gets (data + position,
// data + position + count, a third argument) and its result is stored at a0. The first group keeps the data pointer in t0 and
// passes the frame's address as the third argument; the second keeps it in t1 and passes t0.
void substring_in_frame(Runtime &rt, AllegrexContext &ctx, u32 message, u32 helper) {
    const u32 saved_s0 = ctx.gpr[16];
    const u32 destination = ctx.gpr[4];
    const u32 position = ctx.gpr[6];
    const u32 count = ctx.gpr[7];
    const u32 data = word(rt, ctx.gpr[5]);
    const u32 length = length_of(rt, data);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[8] = data;
    ctx.gpr[3] = length;
    ctx.gpr[16] = destination;
    const bool past = length < position;
    ctx.gpr[2] = past ? 1u : 0u;
    if (past) {
        call(rt, ctx, kThrowOutOfRange, {message});
        // 0x08B9479C goes on after the throw helper; the others end here.
        if (helper != 0x08B946E4u) {
            ctx.gpr[16] = saved_s0;
            return;
        }
    }
    const u32 rest = ctx.gpr[3] - position;
    const u32 cut = minimum(count, rest);
    ctx.gpr[7] = cut;
    const u32 from = ctx.gpr[8] + position;
    const u32 to = position + (ctx.gpr[8] + cut);
    put(rt, destination, call(rt, ctx, helper, {from, to, frame.at(0u)}));
    ctx.gpr[16] = saved_s0;
}

void substring_t1(Runtime &rt, AllegrexContext &ctx, u32 message) {
    const u32 saved_s0 = ctx.gpr[16];
    const u32 destination = ctx.gpr[4];
    const u32 position = ctx.gpr[6];
    const u32 count = ctx.gpr[7];
    const u32 data = word(rt, ctx.gpr[5]);
    const u32 length = length_of(rt, data);
    ctx.gpr[9] = data;
    ctx.gpr[3] = length;
    ctx.gpr[16] = destination;
    const bool past = length < position;
    ctx.gpr[2] = past ? 1u : 0u;
    GuestFrame frame(ctx, 0x10u);
    if (past) {
        call(rt, ctx, kThrowOutOfRange, {message});
        ctx.gpr[16] = saved_s0;
        return;
    }
    const u32 rest = length - position;
    ctx.gpr[2] = rest;
    const u32 cut = minimum(count, rest);
    ctx.gpr[7] = cut;
    put(rt, destination, call(rt, ctx, 0x08C76750u, {data + position, position + (data + cut), ctx.gpr[8]}));
    ctx.gpr[16] = saved_s0;
}

void func_08B9479C(Runtime &rt, AllegrexContext &ctx) { substring_in_frame(rt, ctx, 0x08C9F6F4u, 0x08B946E4u); }
void func_08C76860(Runtime &rt, AllegrexContext &ctx) { substring_in_frame(rt, ctx, 0x08CB594Cu, 0x08C76750u); }
void func_08C768C8(Runtime &rt, AllegrexContext &ctx) { substring_in_frame(rt, ctx, 0x08CB594Cu, 0x08C76750u); }
void func_08C76978(Runtime &rt, AllegrexContext &ctx) { substring_t1(rt, ctx, 0x08CB594Cu); }

// 0x08C7570C (a0 = pointer to the string data, a1 = position): the address of the character at the position; when the
// representation's word at -4 is not negative (shared) 0x08C75524 is called first (it may reallocate), and the data pointer is
// read again afterwards. A position past the length throws.
void func_08C7570C(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved_s0 = ctx.gpr[16];
    const u32 saved_s1 = ctx.gpr[17];
    const u32 string = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    ctx.gpr[17] = string;
    ctx.gpr[16] = position;
    const u32 data = word(rt, string);
    ctx.gpr[3] = data;
    if (!(position < length_of(rt, data))) {
        ctx.gpr[2] = 0u;
        throw_out_of_range(rt, ctx, 0x08CB581Cu, 0u);
    } else {
        ctx.gpr[2] = data + position;
        if (static_cast<s32>(word(rt, data - 4u)) >= 0) {
            {
                GuestFrame frame(ctx, 0x10u);
                call(rt, ctx, 0x08C75524u);
            }
            ctx.gpr[3] = word(rt, string);
            ctx.gpr[2] = ctx.gpr[3] + position;
        } else {
            ctx.gpr[2] = data + position;
        }
    }
    ctx.gpr[16] = saved_s0;
    ctx.gpr[17] = saved_s1;
}

// The stream state setters (a0 = the object, a1 = the bits): the exception mask is the word at +0x10, the state the word at +0x14.
// 0x08C54D8C ORs the bits into the state and calls 0x08B6250C when any of them is in the mask (v0 = the new state).
void func_08C54D8C(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 bits = ctx.gpr[5];
    const u32 state = word(rt, object + 0x14u) | bits;
    ctx.gpr[3] = bits & word(rt, object + 0x10u);
    put(rt, object + 0x14u, state);
    ctx.gpr[2] = state;
    if (ctx.gpr[3] != 0u) {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B6250Cu);
    }
}

// 0x08C54CFC: stores the bits at +0x14 (with bit 0 set when the word at +0x78 is 0) and throws (0x08B5AA30) when any of them is
// in the mask at +0x10; v0 = that intersection.
void func_08C54CFC(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 bits = ctx.gpr[5];
    put(rt, object + 0x14u, word(rt, object + 0x78u) == 0u ? (bits | 1u) : bits);
    ctx.gpr[3] = word(rt, object + 0x14u);
    ctx.gpr[2] = word(rt, object + 0x10u) & ctx.gpr[3];
    if (ctx.gpr[2] != 0u) {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, 0x08B5AA30u, {0x08CB57D0u});
    }
}

// 0x08C742D8 (a0 = pointer to the string data, a1 = destination, a2 = count, a3 = position): the copy member. A position past
// the length throws; the count is cut to what is left past the position; 0 copies nothing, 1 copies one byte and any other
// count goes to the block copy 0x08B79A88. v0 = the count copied.
void func_08C742D8(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved_s0 = ctx.gpr[16];
    const u32 destination = ctx.gpr[5];
    const u32 position = ctx.gpr[7];
    const u32 data = word(rt, ctx.gpr[4]);
    const u32 length = length_of(rt, data);
    ctx.gpr[8] = destination;
    ctx.gpr[4] = data;
    ctx.gpr[3] = length;
    ctx.gpr[16] = ctx.gpr[6];
    const bool past = length < position;
    ctx.gpr[2] = past ? 1u : 0u;
    GuestFrame frame(ctx, 0x10u);
    if (past) {
        call(rt, ctx, kThrowOutOfRange, {0x08CB5848u});
        ctx.gpr[16] = saved_s0;
        return;
    }
    const u32 rest = length - position;
    const u32 count = minimum(ctx.gpr[6], rest);
    ctx.gpr[16] = count;
    ctx.gpr[3] = rest;
    ctx.gpr[2] = 1u;
    if (count == 1u) {
        ctx.gpr[5] = data + position;
        ctx.gpr[2] = rt.memory().load8(data + position);
        rt.memory().store8(destination, static_cast<std::uint8_t>(ctx.gpr[2]));
    } else if (count != 0u) {
        call(rt, ctx, 0x08B79A88u, {destination, data + position, count});
    }
    ctx.gpr[2] = count;
    ctx.gpr[16] = saved_s0;
}

// 0x08C741BC (a0 = pointer to the string data, a1 = position, a2 = count, a3 = the other characters, t0 = their number): the
// compare member. A position past the length throws; else 0x08B36170 compares min(count, left, number) characters and
// a zero answer gives the difference of the two lengths (the cut count minus the number) instead.
void func_08C741BC(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved_s0 = ctx.gpr[16];
    const u32 saved_s1 = ctx.gpr[17];
    const u32 position = ctx.gpr[5];
    const u32 number = ctx.gpr[8];
    const u32 data = word(rt, ctx.gpr[4]);
    const u32 length = length_of(rt, data);
    GuestFrame frame(ctx, 0x20u);
    put(rt, frame.at(0u), ctx.gpr[6]);
    put(rt, frame.at(4u), number);
    ctx.gpr[4] = data;
    ctx.gpr[3] = length;
    const bool past = length < position;
    ctx.gpr[2] = past ? 1u : 0u;
    if (past) {
        call(rt, ctx, kThrowOutOfRange, {0x08CB5830u});
        return;
    }
    const u32 rest = length - position;
    ctx.gpr[2] = ctx.gpr[6] < rest ? 1u : 0u;
    const u32 cut = minimum(ctx.gpr[6], rest);
    ctx.gpr[16] = cut;
    ctx.gpr[17] = number;
    put(rt, frame.at(0u), cut);
    const u32 compared = minimum(cut, number);
    ctx.gpr[3] = number < cut ? 1u : 0u;
    const u32 answer = call(rt, ctx, 0x08B36170u, {data + position, ctx.gpr[7], compared});
    ctx.gpr[16] = cut - number;
    ctx.gpr[2] = answer == 0u ? cut - number : answer;
    ctx.gpr[16] = saved_s0;
    ctx.gpr[17] = saved_s1;
}

// 0x08C73FE0 (a0 = pointer to the string data, a1 = position, a2 = count, a3 = pointer to the other string data): the compare member
// against another string: as 0x08C741BC with the other length read from the other string.
void func_08C73FE0(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved_s0 = ctx.gpr[16];
    const u32 saved_s1 = ctx.gpr[17];
    const u32 position = ctx.gpr[5];
    const u32 data = word(rt, ctx.gpr[4]);
    const u32 length = length_of(rt, data);
    GuestFrame frame(ctx, 0x30u);
    put(rt, frame.at(0x10u), ctx.gpr[6]);
    ctx.gpr[9] = data;
    ctx.gpr[3] = length;
    ctx.gpr[8] = position;
    const bool past = length < position;
    ctx.gpr[2] = past ? 1u : 0u;
    if (past) {
        call(rt, ctx, kThrowOutOfRange, {0x08CB5830u});
        return;
    }
    const u32 other = word(rt, ctx.gpr[7]);
    const u32 other_length = length_of(rt, other);
    const u32 rest = length - position;
    ctx.gpr[2] = ctx.gpr[6] < rest ? 1u : 0u;
    const u32 cut = minimum(ctx.gpr[6], rest);
    ctx.gpr[17] = other_length;
    ctx.gpr[16] = cut;
    ctx.gpr[5] = other;
    ctx.gpr[3] = rest;
    put(rt, frame.at(0x10u), cut);
    put(rt, frame.at(0u), other_length);
    const u32 compared = other_length < cut ? other_length : cut;
    ctx.gpr[4] = data + position;
    const u32 answer = call(rt, ctx, 0x08B36170u, {data + position, other, compared});
    ctx.gpr[16] = cut - other_length;
    ctx.gpr[2] = answer == 0u ? cut - other_length : answer;
    ctx.gpr[16] = saved_s0;
    ctx.gpr[17] = saved_s1;
}

// 0x08C75A68 (a0 = pointer to the string data, a1 = the new length, a2 = a character): the resize member. A length above
// 0x3FFFFFFC throws the length error (0x08B5ADDC); a longer one continues in 0x08C7598C with the difference and the character
// (sign-extended from a byte); an equal one returns 0; a shorter one continues in 0x08C74B68 with the difference and a 0.
// (The out-of-range throw the asm ends with is reached by no path.)
void func_08C75A68(Runtime &rt, AllegrexContext &ctx) {
    const u32 new_length = ctx.gpr[5];
    const u32 data = word(rt, ctx.gpr[4]);
    const u32 filler = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[6])));
    const u32 length = length_of(rt, data);
    ctx.gpr[3] = data;
    ctx.gpr[8] = filler;
    ctx.gpr[6] = length;
    const bool too_long = 0x3FFFFFFCu < new_length;
    ctx.gpr[2] = too_long ? 1u : 0u;
    if (too_long) {
        GuestFrame frame(ctx, 0x10u);
        call(rt, ctx, kThrowLengthError, {0x08CB5934u});
        return;
    }
    if (length < new_length) {
        ctx.gpr[2] = 1u;
        tail_call(ctx, 0x08C7598Cu, {ctx.gpr[4], new_length - length, filler});
        return;
    }
    ctx.gpr[2] = new_length < length ? 1u : 0u;
    if (!(new_length < length)) return;
    ctx.gpr[2] = 0u;
    tail_call(ctx, 0x08C74B68u, {ctx.gpr[4], new_length, length - new_length, 0u});
}

// 0x08C74110 (a0 = pointer to the string data, a1 = position, a2 = count, a3 = a C string): the compare member against a C
// string: its length comes from 0x08B35F60, the rest as 0x08C741BC.
void func_08C74110(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved[5] = {ctx.gpr[16], ctx.gpr[17], ctx.gpr[18], ctx.gpr[19], ctx.gpr[20]};
    const u32 position = ctx.gpr[5];
    const u32 text = ctx.gpr[7];
    const u32 data = word(rt, ctx.gpr[4]);
    const u32 length = length_of(rt, data);
    GuestFrame frame(ctx, 0x40u);
    put(rt, frame.at(0x10u), ctx.gpr[6]);
    ctx.gpr[19] = text;
    ctx.gpr[20] = data;
    ctx.gpr[3] = length;
    ctx.gpr[18] = position;
    const bool past = length < position;
    ctx.gpr[2] = past ? 1u : 0u;
    if (past) {
        call(rt, ctx, kThrowOutOfRange, {0x08CB5830u});
        return;
    }
    const u32 rest = length - position;
    ctx.gpr[2] = ctx.gpr[6] < rest ? 1u : 0u;
    const u32 cut = minimum(ctx.gpr[6], rest);
    ctx.gpr[16] = cut;
    put(rt, frame.at(0x10u), cut);
    const u32 text_length = call(rt, ctx, 0x08B35F60u, {text});
    ctx.gpr[17] = text_length;
    put(rt, frame.at(0u), text_length);
    ctx.gpr[3] = text_length < cut ? 1u : 0u;
    const u32 answer = call(rt, ctx, 0x08B36170u, {data + position, text, minimum(cut, text_length)});
    ctx.gpr[16] = cut - text_length;
    ctx.gpr[2] = answer == 0u ? cut - text_length : answer;
    for (unsigned i = 0u; i < 5u; ++i) ctx.gpr[16u + i] = saved[i];
}

// The string constructors from a range of characters [a0, a1) (0x08C7488C and 0x08C76750 are the same code): an empty range
// answers the shared empty representation's data (0x08D77498 + 12); a null start with a non-null end throws the logic error
// (0x08B5AADC); else a representation is allocated for the length (0x08C74398(length, 0)), the characters copied (a byte, or
// 0x08B79A88), the length and a zero reference count stored and the terminator written after the data. v0 = the data pointer.
void string_from_range(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved[4] = {ctx.gpr[16], ctx.gpr[17], ctx.gpr[18], ctx.gpr[19]};
    const u32 first = ctx.gpr[4];
    const u32 last = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = first;
    if (first == last) {
        ctx.gpr[19] = 0x08D77498u + 0xCu;
        ctx.gpr[2] = ctx.gpr[19];
    } else {
        if (first == 0u && last != 0u) {
            call(rt, ctx, 0x08B5AADCu, {0x08CB5874u});
            return;
        }
        const u32 length = last - first;
        ctx.gpr[18] = length;
        const u32 representation = call(rt, ctx, 0x08C74398u, {length, 0u});
        const u32 data = representation + 0xCu;
        ctx.gpr[17] = representation;
        ctx.gpr[19] = data;
        ctx.gpr[2] = 1u;
        if (length == 1u) {
            rt.memory().store8(data, rt.memory().load8(first));
        } else {
            ctx.gpr[5] = first;
            call(rt, ctx, 0x08B79A88u, {data, first, length});
        }
        const u32 terminator = rt.memory().load8(0x08CDBE28u);
        ctx.gpr[4] = terminator;
        put(rt, representation, length);
        put(rt, representation + 8u, 0u);
        rt.memory().store8(data + length, static_cast<std::uint8_t>(terminator));
        ctx.gpr[2] = data;
    }
    for (unsigned i = 0u; i < 4u; ++i) ctx.gpr[16u + i] = saved[i];
}
void func_08C7488C(Runtime &rt, AllegrexContext &ctx) { string_from_range(rt, ctx); }
void func_08C76750(Runtime &rt, AllegrexContext &ctx) { string_from_range(rt, ctx); }

// 0x08C7598C (a0 = the string, a1 = a count, a2 = a character): appends the character count times. Zero changes nothing; a result
// longer than 0x3FFFFFFC throws the length error (0x08B5ADDC); a representation that is too small or shared goes through 0x08C75840
// first; the characters are written with a byte store (count 1) or 0x08B36018, then the length is stored, the reference count
// zeroed and the terminator written. v0 = the string.
void func_08C7598C(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved[4] = {ctx.gpr[16], ctx.gpr[17], ctx.gpr[18], ctx.gpr[19]};
    const u32 string = ctx.gpr[4];
    const u32 count = ctx.gpr[5];
    const u32 character = static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(ctx.gpr[6])));
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = character;
    ctx.gpr[18] = string;
    ctx.gpr[16] = count;
    if (count != 0u) {
        u32 data = word(rt, string);
        const u32 length = length_of(rt, data);
        const u32 room = 0x3FFFFFFCu - length;
        ctx.gpr[7] = data;
        ctx.gpr[5] = length;
        ctx.gpr[2] = room < count ? 1u : 0u;
        const u32 new_length = count + length;
        ctx.gpr[17] = new_length;
        if (room < count) {
            call(rt, ctx, kThrowLengthError, {0x08CB591Cu});
            return;
        }
        u32 end_offset;
        if (word(rt, data - 8u) < new_length || static_cast<s32>(word(rt, data - 4u)) > 0) {
            call(rt, ctx, 0x08C75840u, {string, new_length});
            data = word(rt, string);
            end_offset = word(rt, data - 12u);
        } else {
            end_offset = length;
        }
        ctx.gpr[2] = 1u;
        ctx.gpr[5] = character;
        if (count == 1u) {
            rt.memory().store8(data + end_offset, static_cast<std::uint8_t>(character));
        } else {
            call(rt, ctx, 0x08B36018u, {data + end_offset, character, count});
        }
        data = word(rt, string);
        put(rt, data - 4u, 0u);
        put(rt, data - 12u, new_length);
        ctx.gpr[2] = rt.memory().load8(0x08CDBE28u);
        rt.memory().store8(data + new_length, static_cast<std::uint8_t>(ctx.gpr[2]));
    }
    ctx.gpr[2] = string;
    for (unsigned i = 0u; i < 4u; ++i) ctx.gpr[16u + i] = saved[i];
}

// 0x08C75C94 (a0 = the string, a1 = pointer to another string's data, a2 = a position in it, a3 = a count): appends a part of the
// other string. A position past its length throws (0x08B5AEDC); the count is cut to what is left; zero returns the string; a
// representation that is too small or shared goes through 0x08C75840 first (the other string's data pointer is read again after
// it); the characters are copied (a byte, or 0x08B79A88), the length stored, the reference count zeroed and the terminator written.
void func_08C75C94(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved[5] = {ctx.gpr[16], ctx.gpr[17], ctx.gpr[18], ctx.gpr[19], ctx.gpr[20]};
    const u32 string = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    const u32 position = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[20] = other;
    ctx.gpr[19] = string;
    ctx.gpr[17] = position;
    u32 source = word(rt, other);
    const u32 other_length = length_of(rt, source);
    ctx.gpr[3] = other_length;
    ctx.gpr[5] = source;
    ctx.gpr[16] = ctx.gpr[7];
    const bool past = other_length < position;
    ctx.gpr[2] = past ? 1u : 0u;
    if (past) {
        call(rt, ctx, kThrowOutOfRange, {0x08CB591Cu});
        return;
    }
    const u32 rest = other_length - position;
    const u32 count = minimum(ctx.gpr[7], rest);
    ctx.gpr[16] = count;
    ctx.gpr[3] = rest;
    ctx.gpr[2] = string;
    if (count != 0u) {
        u32 data = word(rt, string);
        u32 length = length_of(rt, data);
        const u32 new_length = count + length;
        ctx.gpr[18] = new_length;
        ctx.gpr[7] = data;
        if (word(rt, data - 8u) < new_length || static_cast<s32>(word(rt, data - 4u)) > 0) {
            call(rt, ctx, 0x08C75840u, {string, new_length});
            source = word(rt, other);
            data = word(rt, string);
            length = length_of(rt, data);
        }
        ctx.gpr[2] = 1u;
        if (count == 1u) {
            rt.memory().store8(data + length, rt.memory().load8(source + position));
        } else {
            call(rt, ctx, 0x08B79A88u, {data + length, source + position, count});
        }
        data = word(rt, string);
        put(rt, data - 4u, 0u);
        put(rt, data - 12u, new_length);
        ctx.gpr[2] = rt.memory().load8(0x08CDBE28u);
        rt.memory().store8(data + new_length, static_cast<std::uint8_t>(ctx.gpr[2]));
        ctx.gpr[2] = string;
    }
    for (unsigned i = 0u; i < 5u; ++i) ctx.gpr[16u + i] = saved[i];
}

// 0x08C75AF8 (a0 = the string, a1 = a pointer to characters, a2 = their number): appends characters that may be inside the string
// itself. Zero changes nothing; a result longer than 0x3FFFFFFC throws the length error (0x08B5ADDC); a representation that is too small
// or shared is reallocated with 0x08C75840, and a source that lies inside the old data is rebased onto the new data; the characters
// are copied (a byte, or 0x08B79A88), then the length is stored, the reference count zeroed and the terminator written. v0 = the string.
void func_08C75AF8(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved[4] = {ctx.gpr[16], ctx.gpr[17], ctx.gpr[18], ctx.gpr[19]};
    const u32 string = ctx.gpr[4];
    const u32 number = ctx.gpr[6];
    u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[19] = string;
    ctx.gpr[17] = number;
    ctx.gpr[16] = source;
    if (number != 0u) {
        u32 data = word(rt, string);
        const u32 length = length_of(rt, data);
        const u32 new_length = number + length;
        ctx.gpr[18] = new_length;
        ctx.gpr[3] = data;
        ctx.gpr[5] = length;
        ctx.gpr[2] = 0x3FFFFFFCu - length < number ? 1u : 0u;
        if (0x3FFFFFFCu - length < number) {
            call(rt, ctx, kThrowLengthError, {0x08CB591Cu});
            return;
        }
        const bool grow = word(rt, data - 8u) < new_length || static_cast<s32>(word(rt, data - 4u)) > 0;
        if (grow) {
            if (source < data || data + length < source) {
                call(rt, ctx, 0x08C75840u, {string, new_length});
            } else {
                const u32 offset = source - data;
                call(rt, ctx, 0x08C75840u, {string, new_length});
                source = word(rt, string) + offset;
                ctx.gpr[16] = source;
            }
            data = word(rt, string);
        }
        ctx.gpr[2] = word(rt, data - 12u);
        if (number == 1u) {
            rt.memory().store8(data + ctx.gpr[2], rt.memory().load8(source));
        } else {
            call(rt, ctx, 0x08B79A88u, {data + ctx.gpr[2], source, number});
        }
        data = word(rt, string);
        put(rt, data - 4u, 0u);
        put(rt, data - 12u, new_length);
        ctx.gpr[2] = rt.memory().load8(0x08CDBE28u);
        rt.memory().store8(data + new_length, static_cast<std::uint8_t>(ctx.gpr[2]));
    }
    ctx.gpr[2] = string;
    for (unsigned i = 0u; i < 4u; ++i) ctx.gpr[16u + i] = saved[i];
}

// 0x08C4F32C (a0 = a vector of 16-byte elements: begin, end, end of storage; a1 = the capacity wanted): the reserve member. More
// than 0x0FFFFFFF elements throws the length error (0x08B5ADDC); a vector that is already big enough is left alone; else new
// storage is allocated (0x08B35DA0), the elements are moved over (0x08C4F268) and destroyed in the old storage (0x08C4F218), the
// old storage is freed (0x08B35254) and the three pointers are set. v0 = the new end pointer after a reallocation, else 0.
void func_08C4F32C(Runtime &rt, AllegrexContext &ctx) {
    const u32 saved[6] = {ctx.gpr[16], ctx.gpr[17], ctx.gpr[18], ctx.gpr[19], ctx.gpr[20], ctx.gpr[21]};
    const u32 vector = ctx.gpr[4];
    const u32 wanted = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[16] = vector;
    const bool too_many = 0x0FFFFFFFu < wanted;
    ctx.gpr[2] = too_many ? 1u : 0u;
    if (too_many) call(rt, ctx, kThrowLengthError, {0x08CB538Cu});
    const u32 begin = word(rt, vector);
    ctx.gpr[17] = begin;
    const u32 capacity = static_cast<u32>((static_cast<s32>(word(rt, vector + 8u) - begin)) >> 4);
    ctx.gpr[2] = capacity < wanted ? 1u : 0u;
    ctx.gpr[19] = 0u;
    if (capacity < wanted) {
        const u32 end = word(rt, vector + 4u);
        const u32 size = static_cast<u32>((static_cast<s32>(end - begin)) >> 4);
        u32 storage = 0u;
        u32 bytes = 0u;
        ctx.gpr[18] = end;
        ctx.gpr[20] = 0u;
        ctx.gpr[21] = size;
        if (wanted != 0u) {
            bytes = wanted << 4;
            ctx.gpr[20] = bytes;
            storage = call(rt, ctx, 0x08B35DA0u, {bytes});
            ctx.gpr[19] = storage;
        }
        call(rt, ctx, 0x08C4F268u, {begin, end, storage, frame.at(0u)});
        call(rt, ctx, 0x08C4F218u, {word(rt, vector), word(rt, vector + 4u), frame.at(1u)});
        const u32 old_storage = word(rt, vector);
        ctx.gpr[4] = old_storage;
        ctx.gpr[2] = size << 4;
        if (old_storage != 0u) {
            call(rt, ctx, 0x08B35254u);
            ctx.gpr[2] = size << 4;
        }
        put(rt, vector + 8u, storage + bytes);
        ctx.gpr[2] = storage + (size << 4);
        put(rt, vector + 4u, ctx.gpr[2]);
        put(rt, vector, storage);
    }
    for (unsigned i = 0u; i < 6u; ++i) ctx.gpr[16u + i] = saved[i];
}

} // namespace
} // namespace new_batch_1
} // namespace gt::game

#define NEW1(address, name, kind) GT_DECOMP(address, gt::game::new_batch_1::name, gt::ReturnKind::kind)
NEW1(0x08C73530u, func_08C73530, Word);
NEW1(0x08C7359Cu, func_08C7359C, Word);
NEW1(0x08C73EFCu, func_08C73EFC, Word);
NEW1(0x08C75080u, func_08C75080, Word);
NEW1(0x08C75118u, func_08C75118, Word);
NEW1(0x08C74F24u, func_08C74F24, Word);
NEW1(0x08C76930u, func_08C76930, Word);
NEW1(0x08C75268u, func_08C75268, Word);
NEW1(0x08B9479Cu, func_08B9479C, Word);
NEW1(0x08C76860u, func_08C76860, Word);
NEW1(0x08C768C8u, func_08C768C8, Word);
NEW1(0x08C76978u, func_08C76978, Word);
NEW1(0x08C7570Cu, func_08C7570C, Word);
NEW1(0x08C54D8Cu, func_08C54D8C, Word);
NEW1(0x08C54CFCu, func_08C54CFC, Word);
NEW1(0x08C742D8u, func_08C742D8, Word);
NEW1(0x08C741BCu, func_08C741BC, Word);
NEW1(0x08C73FE0u, func_08C73FE0, Word);
NEW1(0x08C75A68u, func_08C75A68, Word);
NEW1(0x08C74110u, func_08C74110, Word);
NEW1(0x08C7488Cu, func_08C7488C, Word);
NEW1(0x08C76750u, func_08C76750, Word);
NEW1(0x08C7598Cu, func_08C7598C, Word);
NEW1(0x08C75C94u, func_08C75C94, Word);
NEW1(0x08C75AF8u, func_08C75AF8, Word);
NEW1(0x08C4F32Cu, func_08C4F32C, Word);
