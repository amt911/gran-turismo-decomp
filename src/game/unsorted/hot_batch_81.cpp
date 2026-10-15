// Hot batch 81: reached functions that no earlier batch has written, picked from the call probe of batch 78 (every function of up to 70 instructions not yet
// decompiled, run on the five routes with --decomp off; batch 80 = PR #201 excluded with --exclude-ref origin/decomp/hot-batch-80, and no address that #201 writes, registers
// or leaves unregistered is taken here). Before reading any function its address was searched for in src/ and in the branch of batch 80. The families that remained
// (`tools.family_members --list --limit 0 --any-template --min-reached 1`) were almost all written by batch 80 already (its unregistered ones are left alone); the one new
// member found is func_08BE6154, a member of batch 80's "new holder from a temporary" shape (the extra argument from a3, a copy of that body here). Everything else is a
// single: the reached functions without a sibling of the same shape, ordered by calls on the five routes, each read by hand word by word with its branch targets and delay slots
// (the branch targets taken from the disassembler's resolved column, never counted by hand: func_08B54FB8 was first read one word off).
//
//  - float math of the libm: sin / cos kernels (func_08B499B8, func_08B49CC8, func_08B6DEF8), the range-reduced wrappers (func_08B66604, func_08B662C4), log (func_08B847A8), exp
//    (func_08B84954), pow (func_08B84A2C), atan2 (func_08B4A430), with the result bits compared in f0 (ReturnKind::Float); each operation keeps the asm's operand order (the PSP
//    multiply is psp_mul) and the conversions go through the context's own `fpu_float_to_word_ct`;
//  - matrix and quaternion builders (func_08B4B114, func_08C46184, func_08B4C804, func_08B4CBDC, func_08B4E360), the Euler angle extractors (func_08C52868, func_08C527C0);
//  - graphics library state (func_08B54FB8 with 15.8 million calls, func_08B58D08, func_08B58F84, func_08B5547C);
//  Evidence for the descriptive words (sin, cos, log, exp, pow, atan2, strcasecmp, strchr) is the behaviour each comment
//  derives from the asm (range reduction by 2 pi, the polynomial, the ctype table, ...); the symbols keep their func_XXXXXXXX
//  names and nothing goes into config/symbol_addrs.txt until a caller or string confirms them.
//  - strings and libc (func_08B864EC reserve, func_08BEFC38 / func_08B86674 replace and resize, func_08B7B8C4 strcasecmp, func_08B7BA50 strchr);
//  - for_each over lists and arrays with a pointer to member, serializers over virtual getters, holder / pointer-to-member chains and guarded statics.
//
// Every call_guest / tail_call passes the argument registers the original sets before the jal (a0-a3, t0-t3, f12-f14): the ones it leaves alone keep the caller's value, which call_guest
// copies from ctx. Every call goes through `call_keep` (the callee's v0, v1 and f0 stay in the registers like the original's; a callee that sets no v0 hands the previous one on).
//
// Written but not registered (the commented HOT81 lines say why): func_088AA074, func_088FA224 and func_08AF2D20 — no compared case changed memory, so the gate cannot see their stores.

#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>

namespace gt::game {
namespace hot_batch_81 {
namespace {

using namespace gt::game::hot;

constexpr std::uint32_t kReleaseLocal = 0x08A5B9ECu; // destroys a temporary holder
constexpr std::uint32_t kAllocate = 0x08A5BC20u;     // (size, alignment): a new block
constexpr std::uint32_t kStringEmpty = 0x08D74FD0u;  // the shared empty representation of the 1-byte-character string (its characters follow at +0xC)

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next instruction sees, and a callee that sets
// no v0 hands the previous one on to the code after it (the caller's own v0 is what call_guest starts the callee with).
inline GuestResult call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments,
                             std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
inline std::uint32_t call_keep(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address, std::initializer_list<std::uint32_t> arguments) {
    return call_keep(rt, ctx, address, arguments, {}).v0;
}

inline std::int32_t sra1(std::uint32_t value) { return static_cast<std::int32_t>(value) >> 1; }
inline float fbits(std::uint32_t bits) { return std::bit_cast<float>(bits); }
inline std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
// abs.s: clears the sign bit.
inline float fabs_bits(float value) { return std::bit_cast<float>(std::bit_cast<std::uint32_t>(value) & 0x7FFFFFFFu); }
// cvt.s.w: the word read as a signed integer.
inline float cvt_sw(std::uint32_t word_bits) { return static_cast<float>(static_cast<std::int32_t>(word_bits)); }
inline std::int8_t byte_at(psprecomp::Runtime &rt, std::uint32_t address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Graphics state and holders
// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08B54FB8(state): the graphics library's enable / disable table. A state number maps to a bit of the library's state block (kGuState, the .bss block
// of hot_common.hpp): 2 -> bit 7, 0x8002 -> bit 10, 0x64..0x6A -> bits 0..6, 0x6C / 0x6D -> bits 8 / 9, 0x6F..0x75 -> bits 11..17: the tail call
// func_08B54F54(kGuState, 1 << bit) sets it. 0x77 and 0x78 clear a byte (+0x148 / +0x240) and set a flag of the flag word (+0x6C: 0x8 / 0x200) themselves.
// Every other state leaves without touching anything. v0 / v1 are what the instructions leave on each path (the delay slots of the tests set them: see the comments below).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the states 0 and 1 (the `bnez` to the return; its delay slot already made v0 = (index < 7) = 0), the states
// 3..0x63 and 0x6B (the `beqz` to the same return, v0 = 0), 0x6E and 0x76 (the `bnez` not taken: return with v0 = 0, v1 = state - 0x64), 0x77 (the `beq` to the byte store and flag update; v0 = the new flags,
// v1 = kGuState) and every state above 0x78 but 0x8002 (the `beq` not taken: return with v0 = 0x8002 and v1 = 0xA from the delay slots). The compared cases take 2, 0x64..0x6A, 0x6C, 0x6D, 0x6F..0x75, 0x78 and 0x8002.
void func_08B54FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = ctx.gpr[4];
    const auto leave = [&](std::uint32_t v0, std::uint32_t v1) {
        ctx.gpr[2] = v0;
        ctx.gpr[3] = v1;
    };
    const auto set_flag = [&](std::uint32_t v0, std::uint32_t shift) {
        leave(v0, shift);
        tail_call(ctx, 0x08B54F54u, {kGuState, 1u << (shift & 31u)});
    };
    const auto set_state_flag = [&](std::uint32_t byte_offset, std::uint32_t flag) {
        const std::uint32_t flags = word(rt, kGuFlags);
        rt.memory().store8(kGuState + byte_offset, 0u);
        put(rt, kGuFlags, flags | flag);
        leave(flags | flag, kGuState);
    };
    if (state < 0x6Eu) {
        const std::uint32_t index = state - 0x64u;  // v1, set in the delay slot of the 0x6C test
        if (state >= 0x6Cu) {
            set_flag(0u, index);
        } else if (state == 2u) {
            set_flag(0u, 7u);
        } else if (state < 2u) {
            leave(0u, index);  // the delay slot of the `bnez` already replaced v0 = (state < 2) with (index < 7), which is 0 for these
        } else if (index < 7u) {
            set_flag(1u, index);
        } else {
            leave(0u, index);
        }
        return;
    }
    if (state == 0x77u) {
        set_state_flag(0x148u, 0x8u);
    } else if (state < 0x78u) {
        const std::uint32_t index = state - 0x64u;
        if (state - 0x6Fu < 7u) set_flag(1u, index);
        else leave(0u, index);
    } else if (state == 0x78u) {
        set_state_flag(0x240u, 0x200u);
    } else if (state == 0x8002u) {
        set_flag(0x8002u, 0xAu);
    } else {
        leave(0x8002u, 0xAu);
    }
}

// func_08B864EC(this, wanted): basic_string::reserve for the 1-byte-character string (the data pointer at this[0] has its representation {length, capacity, refcount} at
// data - 0xC). When the capacity already is `wanted` and the string is not shared (refcount <= 0) nothing happens (v0 = the refcount, v1 = the representation). Otherwise the wanted size
// is raised to the length, func_08B854DC(rep, &frame, wanted - length) makes a private copy of that capacity, the old representation loses a reference (func_08B60804(data - 4, -1)
// unless it is the shared empty one, freed with func_08B8542C(rep, &frame) when the count is not positive) and this[0] = the new data.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the capacity already being `wanted` (the `bne` falls through: with a count
// of 0 or less the `blez` ends the function with v0 = the count and v1 = the representation) and a wanted size below the current length (`movn` raises it to the length).
void func_08B864EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t wanted = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t chars = word(rt, self);
    const std::uint32_t rep = chars - 0xCu;
    ctx.gpr[3] = rep;
    const std::uint32_t capacity = word(rt, rep + 4u);
    ctx.gpr[2] = capacity;
    if (capacity == wanted) {
        const std::uint32_t count = word(rt, rep + 8u);
        ctx.gpr[2] = count;
        if (static_cast<std::int32_t>(count) <= 0) return;
    }
    const std::uint32_t length = word(rt, chars - 0xCu);
    if (wanted < length) wanted = length;
    const std::uint32_t fresh = call_keep(rt, ctx, 0x08B854DCu, {rep, frame.at(0u), wanted - length, wanted});
    const std::uint32_t old_chars = word(rt, self);
    const std::uint32_t old_rep = old_chars - 0xCu;
    ctx.gpr[2] = kStringEmpty;
    ctx.gpr[3] = old_chars;
    if (old_rep != kStringEmpty) {
        const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {old_chars - 4u, 0xFFFFFFFFu});
        if (static_cast<std::int32_t>(count) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {old_rep, frame.at(0u)});
    }
    put(rt, self, fresh);
}

// func_08B59508(this): the halfword this[0xFF8] holds three request bits; the low three bits are cleared first (the store comes before the calls), then bit 0 runs
// func_08B59210(this, 0x3A, this[0xFE0]), bit 1 func_08B59194(this, 0x3E, this[0xFE4]) and bit 2 the tail call func_08B59210(this, 0x40, this[0xFE8]).
// v0 = (flags & 4) | 0 and v1 = 0 as the instructions leave them.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): bit 0 clear (the `beqz` skips the first call); bits 1 and 2 are taken and not taken.
void func_08B59508(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        const std::uint32_t block = self + 0x2E0u;
        const std::uint32_t flags = rt.memory().load16(block + 0xD18u);
        rt.memory().store16(block + 0xD18u, static_cast<std::uint16_t>(flags & ~7u));
        if ((flags & 1u) != 0u) (void)call_keep(rt, ctx, 0x08B59210u, {self, 0x3Au, word(rt, block + 0xD00u)});
        if ((flags & 2u) != 0u) (void)call_keep(rt, ctx, 0x08B59194u, {self, 0x3Eu, word(rt, block + 0xD04u)});
        ctx.gpr[2] = flags & 4u;
        ctx.gpr[3] = 0u;
        if ((flags & 4u) == 0u) return;
    }
    tail_call(ctx, 0x08B59210u, {self, 0x40u, word(rt, self + 0x2E0u + 0xD08u)});
}

// func_08C436F4(this, a1, index, a3, t0): entry `index` of the table at this[0xC] (stride 0x30; the count is the halfword this[4]) with a non-null handle at +0x18 (stored to the
// frame): s = a3, or this[0x1A] when a3 is 0. The entry is used when its byte +0x13 is 0, or s[10] has bit 0 set, or (bit 1 of s[10]: func_08B542AC(handle, entry[0x14])
// / else func_08B543B4(handle, entry[0x14])) answers non-zero; then func_08C426A0(this, a1, index, s, &handle, t0) runs until it answers 0 and the result is 1.
// Every other path answers 0 (index out of range, no handle, or the check refuses).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an index at or above the count and a null handle (both end with v0 = 0; the handle is stored to the frame in the delay slot before the test), the owner taken from this[0x1A] (a3 == 0), and every case with a non-zero byte at entry + 0x13: bit 0 of the owner's word at +0x28 skips the checks, bit 1 picks func_08B542AC (else func_08B543B4) called with the byte (v1 holds the `lbu`, not the handle) and entry[0x14], and a zero answer ends with v0 = 0. The compared cases all have that byte 0.
void func_08C436F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], second = ctx.gpr[5], index = ctx.gpr[6], third = ctx.gpr[7], fifth = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    if (!(index < rt.memory().load16(self + 0x10u))) {
        ctx.gpr[2] = 0u;
        return;
    }
    const std::uint32_t entry = word(rt, self + 0x30u) + index * 0x30u;
    const std::uint32_t handle = word(rt, entry + 0x18u);
    put(rt, frame.at(0u), handle);
    if (handle == 0u) {
        ctx.gpr[2] = 0u;
        ctx.gpr[3] = 0u;
        return;
    }
    const std::uint32_t owner = third == 0u ? word(rt, self + 0x68u) : third;
    const std::uint32_t kind = rt.memory().load8(entry + 0x13u);  // v1 now holds this byte, not the handle: the checks below are called with it
    ctx.gpr[3] = kind;
    if (kind != 0u) {
        const std::uint32_t flags = word(rt, owner + 0x28u);
        if ((flags & 1u) == 0u) {
            const std::uint32_t extra = word(rt, entry + 0x14u);
            std::uint32_t answer;
            if ((flags & 2u) != 0u) {
                answer = call_keep(rt, ctx, 0x08B542ACu, {kind, extra});
            } else {
                answer = call_keep(rt, ctx, 0x08B543B4u, {kind, extra});
            }
            if (answer == 0u) {
                ctx.gpr[2] = 0u;
                return;
            }
        }
    }
    while (call_keep(rt, ctx, 0x08C426A0u, {self, second, index, owner, frame.at(0u), fifth}) != 0u) {
    }
    ctx.gpr[2] = 1u;
}

// func_089E0E68(this, a1): three optional children at this[0x210], this[0x220] and this[0x218]: the first one must answer its virtual slot 0x28 (else the function ends with
// v0 = 0); then func_089E0D60(this, a1, second) when the second exists and answers slot 0x28; func_089E0B3C(this, a1, first) always; and the tail call
// func_089E0DE4(this, a1, third) when the third exists and answers slot 0x28.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a missing first child (v0 untouched, v1 = 0), a first child answering 0 on slot 0x28 (v0 = 0), and the second and third children (the `beqz` lands on the next step / the epilogue; func_089E0D60 after the second's slot-0x28 check, the tail call func_089E0DE4 after the third's). Every compared case has only the first child.
void func_089E0E68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        const std::uint32_t first = word(rt, self + 0x210u);
        ctx.gpr[3] = first;
        if (first == 0u) return;
        if (call_keep(rt, ctx, word(rt, word(rt, first) + 0x28u), {first}) == 0u) return;
        const std::uint32_t second = word(rt, self + 0x220u);
        ctx.gpr[3] = second;
        if (second != 0u) {
            if (call_keep(rt, ctx, word(rt, word(rt, second) + 0x28u), {second}) != 0u)
                (void)call_keep(rt, ctx, 0x089E0D60u, {self, other, word(rt, self + 0x220u)});
        }
        (void)call_keep(rt, ctx, 0x089E0B3Cu, {self, other, word(rt, self + 0x210u)});
        const std::uint32_t third = word(rt, self + 0x218u);
        ctx.gpr[3] = third;
        if (third == 0u) return;
        if (call_keep(rt, ctx, word(rt, word(rt, third) + 0x28u), {third}) == 0u) return;
    }
    tail_call(ctx, 0x089E0DE4u, {self, other, word(rt, self + 0x218u)});
}

// func_08BEFC38(this, pos, erase, count, fill): basic_string::replace(pos, erase, count, fill) with a character: the length after the edit must fit (0x3FFFFFFC - (length - erase) >= count,
// else the length error func_08B5ADDC(0x08CA3938)); func_08B8634C(this, pos, erase, count) makes room; then `count` copies of the sign-extended character t0 go to data + pos
// (one byte for a count of 1, func_08B36018 (memset) otherwise; nothing for 0). v0 = this.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the length error (the call does not return in practice; a0 holds the message afterwards), a count of 0 (the `beqz` to the epilogue: nothing is written) and a count above 1 (func_08B36018 / memset instead of the single byte store). The compared cases all have a count of 1.
void func_08BEFC38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], position = ctx.gpr[5], erase = ctx.gpr[6], count = ctx.gpr[7];
    const std::uint32_t fill = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(ctx.gpr[8])));
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t length = word(rt, word(rt, self) - 0xCu);
    const std::uint32_t room = 0x3FFFFFFCu - (length - erase);
    if (room < count) {
        ctx.gpr[4] = 0x08CA3938u;
        (void)call_keep(rt, ctx, 0x08B5ADDCu, {0x08CA3938u});
    }
    (void)call_keep(rt, ctx, 0x08B8634Cu, {});
    if (count != 0u) {
        const std::uint32_t destination = word(rt, self) + position;
        if (count == 1u) rt.memory().store8(destination, static_cast<std::uint8_t>(fill));
        else (void)call_keep(rt, ctx, 0x08B36018u, {destination, fill, count});
    }
    ctx.gpr[2] = self;
}

// func_08BAA778(out, begin, end, functor): for_each over the circular list that starts at *begin and ends at *end (the next element of e is *(e + 4)); each element e runs the
// pointer-to-member function {functor[2], functor[3]} (adjust bit 0: virtual, so functor[2] is an offset into the vtable) on e + (adjust >> 1) with (functor[0], functor[1]); the functor's four
// words are then copied to out. v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the plain (non-virtual) form of the pointer to member (adjust bit 0 clear: v1 stays the function word), and an element 0 (`movz`: the next pointer is read from address 0). The compared cases walk non-empty lists with the virtual form.
void func_08BAA778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], begin = ctx.gpr[5], end_pointer = ctx.gpr[6], functor = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t element = word(rt, begin);
    const std::uint32_t end = word(rt, end_pointer);
    const std::uint32_t adjust = word(rt, functor + 0xCu), second = word(rt, functor + 4u), first = word(rt, functor), function_word = word(rt, functor + 8u);
    ctx.gpr[2] = element;
    if (element != end) {
        for (;;) {
            const std::uint32_t receiver = element + static_cast<std::uint32_t>(sra1(adjust));
            const std::uint32_t function = (adjust & 1u) != 0u ? word(rt, word(rt, receiver) + function_word) : function_word;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {receiver, first, second});
            const std::uint32_t next = word(rt, element == 0u ? 0u : element + 4u);
            ctx.gpr[2] = next;
            if (next == end) break;
            element = next;
        }
    }
    put(rt, out + 4u, second);
    put(rt, out, first);
    put(rt, out + 8u, function_word);
    put(rt, out + 0xCu, adjust);
    ctx.gpr[2] = out;
}

// new holder from a temporary with the extra argument taken from a3 (43 instructions): t = a temporary holder on the frame (func_08A69B3C(&t)), o = new 24-byte block
// (func_08A5BC20(24, 4)), `ctor`(o, &t) constructs it, o[0] = vtable, o[5] = a3, the temporary is disposed (func_08A69D68(&t)); a holder slot frame[0] = o;
// func_08A5D414(&frame[4], &frame[0]) fills the holder frame[4], func_08A7C5C8(a0, &frame[4]) hands it over and func_08A5B9EC(&frame[4]) releases it. The result is the release's.
// Same body as func_08B8705C of batch 80 (PR #201), kept here as its own copy.
// Branches: none, the function is straight-line.
void new_holder_temp_a3(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t ctor, std::uint32_t vtable) {
    const std::uint32_t target = ctx.gpr[4], extra = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    (void)call_keep(rt, ctx, 0x08A69B3Cu, {frame.at(8u)});
    const std::uint32_t object = call_keep(rt, ctx, kAllocate, {24u, 4u});
    (void)call_keep(rt, ctx, ctor, {object, frame.at(8u)});
    put(rt, object, vtable);
    put(rt, object + 0x14u, extra);
    (void)call_keep(rt, ctx, 0x08A69D68u, {frame.at(8u)});
    put(rt, frame.at(0u), object);
    (void)call_keep(rt, ctx, 0x08A5D414u, {frame.at(4u), frame.at(0u)});
    (void)call_keep(rt, ctx, 0x08A7C5C8u, {target, frame.at(4u)});
    take(ctx, call_keep(rt, ctx, kReleaseLocal, {frame.at(4u)}, {}));
}
void func_08BE6154(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { new_holder_temp_a3(rt, ctx, 0x08A725A0u, 0x08CCD598u); }


// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Float math
// ---------------------------------------------------------------------------------------------------------------------------------------------------

// The polynomial both sine kernels end with: x is the reduced angle (f12 at that point); returns x + x * x^2 * (...) (f0). Operand order as the asm's mul.s.
inline float sine_polynomial(float x) {
    float f2 = psp_mul(x, x);
    float f1 = psp_mul(f2, fbits(0x362E9C4Au));
    float f0 = psp_mul(x, f2);
    f1 = f1 - fbits(0x394FB221u);
    f1 = psp_mul(f2, f1);
    f1 = f1 + fbits(0x3C08873Eu);
    f2 = psp_mul(f2, f1);
    f2 = f2 - fbits(0x3E2AAAA4u);
    f0 = psp_mul(f0, f2);
    return x + f0;
}

// func_08B499B8(x): sin(x) for a float x: |x| reduced modulo 2 pi (trunc(|x| / 2 pi) periods subtracted), folded to |.. - pi| - pi/2 and the polynomial above. Result in f0 (f12 = the reduced angle).
// Branches: none, the function is straight-line.
void func_08B499B8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    float x = fabs_bits(ctx.fpr[12]);
    const float scaled = psp_mul(x, fbits(0x3E22F983u));
    const std::uint32_t periods = ctx.fpu_float_to_word_ct<1u>(scaled);
    x = x + psp_mul(cvt_sw(periods), fbits(0xC0C90FDBu));
    x = x - fbits(0x40490FDBu);
    x = fabs_bits(x);
    x = x - fbits(0x3FC90FDBu);
    ctx.fpr[12] = x;
    ctx.fpr[0] = sine_polynomial(x);
}

// func_08B49CC8(x): cos(x): the same reduction of |x - pi/2|. Result in f0 (f12 = the reduced angle).
// Branches: none, the function is straight-line.
void func_08B49CC8(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    const float half_pi = fbits(0x3FC90FDBu);
    float x = fabs_bits(ctx.fpr[12] - half_pi);
    const float scaled = psp_mul(x, fbits(0x3E22F983u));
    const std::uint32_t periods = ctx.fpu_float_to_word_ct<1u>(scaled);
    x = x + psp_mul(cvt_sw(periods), fbits(0xC0C90FDBu));
    x = x - fbits(0x40490FDBu);
    x = fabs_bits(x);
    x = x - half_pi;
    ctx.fpr[12] = x;
    ctx.fpr[0] = sine_polynomial(x);
}

// func_08B6DEF8(approximate = a0, x = f12, y = f13): the kernel of a sine / cosine on a small angle. |x| below 2^-27 (and truncating to 0) answers x. Otherwise z = x * x, p = the polynomial of
// the constants at 0x08CB6504..0x08CB6514 evaluated on z, q = x * z; with a0 == 0 the result is x + q * (z * p - c6), else x - (z * (y * c7 - q * p) - y + q * c6).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none that matter: below 2^-27 the truncation is always 0 (answers x), and both that arm and the polynomial arm (a0 == 0 and a0 != 0) are taken.
void func_08B6DEF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const bool full = ctx.gpr[4] != 0u;
    float x = ctx.fpr[12];
    const float y = ctx.fpr[13];
    const std::uint32_t magnitude = bits_of(x) & 0x7FFFFFFFu;
    if (!(static_cast<std::int32_t>(0x31FFFFFFu) < static_cast<std::int32_t>(magnitude)) && ctx.fpu_float_to_word_ct<1u>(x) == 0u) {
        ctx.fpr[0] = x;
        return;
    }
    const float z = psp_mul(x, x);
    float p = psp_mul(z, float_at(rt, 0x08CB6504u));
    const float q = psp_mul(x, z);
    p = p - float_at(rt, 0x08CB6508u);
    p = psp_mul(z, p);
    p = p + float_at(rt, 0x08CB650Cu);
    p = psp_mul(z, p);
    p = p - float_at(rt, 0x08CB6510u);
    p = psp_mul(z, p);
    p = p + float_at(rt, 0x08CB6514u);
    if (full) {
        const float f2 = psp_mul(q, p);
        float f0 = float_at(rt, 0x08CB651Cu);
        const float f1 = float_at(rt, 0x08CB6518u);
        f0 = psp_mul(y, f0);
        const float f1b = psp_mul(q, f1);
        f0 = f0 - f2;
        f0 = psp_mul(z, f0);
        f0 = f0 - y;
        f0 = f0 + f1b;
        x = x - f0;
    } else {
        float f0 = psp_mul(z, p);
        f0 = f0 - float_at(rt, 0x08CB6518u);
        f0 = psp_mul(q, f0);
        x = x + f0;
    }
    ctx.fpr[12] = x;
    ctx.fpr[0] = x;
}

// func_08B847A8(x): the natural logarithm of a float, from the integer bit pattern: x <= 0 answers -inf for +-0 and the NaN 0xFF80FFFF for the negatives; otherwise the mantissa m and exponent e of x are
// split around the constant 0x3504F3 (m above it: the fraction (m - 2^23) / (m + 3 * 2^23)... both as integer-converted floats) and the result is e' + d * (c3 + d^2 * (c2 + d^2 * c1)) with d the
// quotient and e' the converted exponent; the three constants are read from 0x08CB6220..0x08CB6228. Result in f0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): x < 0 with x != 0 (the NaN 0xFF80FFFF: `bnez t0` takes the `ori` of its delay slot); the compared cases see only +-0 (-inf).
void func_08B847A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t x = bits_of(ctx.fpr[12]);
    const std::uint32_t mantissa = x & 0x007FFFFFu;
    const bool high = static_cast<std::int32_t>(0x003504F3u) < static_cast<std::int32_t>(mantissa);
    if (static_cast<std::int32_t>(x) <= 0) {
        ctx.fpr[0] = fbits((x & 0x7FFFFFFFu) != 0u ? 0xFF80FFFFu : 0xFF800000u);
        return;
    }
    std::uint32_t numerator = mantissa + 0xFF800000u, exponent_word = x, base = mantissa;
    if (!high) {
        numerator = mantissa;
        exponent_word = x + 0xFF800000u;
        base = mantissa + 0xFF800000u;
    }
    const std::uint32_t denominator = base + 0x01800000u;
    const float f13 = cvt_sw(numerator), f12 = cvt_sw(denominator);
    const float f11 = float_at(rt, 0x08CB6220u);
    const std::uint32_t exponent = static_cast<std::uint32_t>(static_cast<std::int32_t>(exponent_word) >> 23);
    const float f4 = f13 / f12;
    const float f10 = float_at(rt, 0x08CB6224u);
    const float f6 = float_at(rt, 0x08CB6228u);
    const float f5 = cvt_sw(exponent - 0x7Eu);
    const float f8 = psp_mul(f4, f4);
    const float f9 = psp_mul(f8, f11);
    const float f7 = f9 + f10;
    const float f3 = psp_mul(f7, f8);
    const float f2 = f3 + f6;
    const float f1 = psp_mul(f4, f2);
    ctx.fpr[0] = f5 + f1;
}

// func_08B84954(x): the exponential of a float. |x| above 0x42B00F33 (bit pattern) answers x for a NaN, 0 for a negative x and +inf otherwise. Otherwise n = round(|x| * c1) (the constant at
// 0x08CB623C), r = |x| * c1 - n (both negated for a negative x), the result is 2^n * ((c2 + r * c3 + (r * c4) * r) / (c5 - r * c6))^4 with the constants at 0x08CB6240..0x08CB624C and 2^n built
// from the exponent field ((n + 127) << 23). Result in f0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a NaN argument (the big-range arm's `bc1f` lands on the epilogue with f0 = f12 from its delay slot).
void func_08B84954(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const std::uint32_t magnitude = bits_of(x) & 0x7FFFFFFFu;
    if (static_cast<std::int32_t>(0x42B00F33u) < static_cast<std::int32_t>(magnitude)) {
        if (!(x == x)) ctx.fpr[0] = x;
        else if (x < 0.0f) ctx.fpr[0] = 0.0f;
        else ctx.fpr[0] = fbits(0x7F800000u);
        return;
    }
    const float f7 = fbits(magnitude);
    const float f6 = float_at(rt, 0x08CB623Cu);
    const float f2 = psp_mul(f7, f6);
    const float f4 = float_at(rt, 0x08CB6244u);
    const std::uint32_t rounded = ctx.fpu_float_to_word_ct<0u>(f2);
    const float f5 = cvt_sw(rounded);
    std::uint32_t n = ctx.fpu_float_to_word_ct<1u>(f5);
    float f3 = f2 - f5;
    if (x < 0.0f) {
        f3 = -f3;
        n = 0u - n;
    }
    const float f2b = float_at(rt, 0x08CB6240u);
    const float f19 = float_at(rt, 0x08CB6248u);
    const float f17 = psp_mul(f3, f2b);
    const float f15 = psp_mul(f3, f19);
    const float f18 = float_at(rt, 0x08CB624Cu);
    const float f13 = f17 + f4;
    const float f16 = psp_mul(f3, f18);
    const float f14 = psp_mul(f15, f3);
    const float f8 = fbits((n + 0x7Fu) << 23);
    const float f11 = f4 - f16;
    const float f12 = f13 + f14;
    const float f10 = f12 / f11;
    const float f9 = psp_mul(f10, f10);
    const float f4b = psp_mul(f9, f9);
    ctx.fpr[12] = f12;
    ctx.fpr[0] = psp_mul(f4b, f8);
}

// func_08B4B114(quaternion = a0, out = a1, t = f12-f14): the 4x4 matrix of the quaternion {x, y, z, w} (a0[0..3]) with the translation (f12, f13, f14) in the last row, written to out in the order of the
// original (the fourth column is 0, 0, 0, 1). The temporaries keep the asm's register names.
// Branches: none, the function is straight-line.
void func_08B4B114(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t quaternion = ctx.gpr[4], out = ctx.gpr[5];
    const float f2 = float_at(rt, quaternion + 8u), f1 = float_at(rt, quaternion + 4u);
    float f5 = float_at(rt, quaternion);
    const float f7 = f2 + f2, f6 = f1 + f1;
    float f0 = f5 + f5;
    const float f3 = float_at(rt, quaternion + 0xCu);
    const float f16 = psp_mul(f2, f7);
    const float f11 = psp_mul(f1, f6);
    f5 = psp_mul(f5, f0);
    const float f8 = 1.0f;
    const float f15 = psp_mul(f0, f3);
    const float f7b = psp_mul(f3, f7);
    float f1b = psp_mul(f0, f1);
    const float f3b = psp_mul(f3, f6);
    f0 = psp_mul(f0, f2);
    float f9 = f8 - f16;
    float f2b = psp_mul(f2, f6);
    float f10 = f8 - f5;
    float f4 = f8 - f11;
    const float f6b = f1b + f7b;
    const float f17 = f0 - f3b;
    const float f18 = f15 + f2b;
    f10 = f10 - f11;
    f4 = f4 - f16;
    f9 = f9 - f5;
    f1b = f1b - f7b;
    f0 = f0 + f3b;
    f2b = f2b - f15;
    put_float(rt, out + 0x3Cu, f8);
    put_float(rt, out, f4);
    put_float(rt, out + 0x10u, f1b);
    put_float(rt, out + 0x20u, f0);
    put_float(rt, out + 0x30u, ctx.fpr[12]);
    put_float(rt, out + 4u, f6b);
    put_float(rt, out + 0x14u, f9);
    put_float(rt, out + 0x24u, f2b);
    put_float(rt, out + 0x34u, ctx.fpr[13]);
    put_float(rt, out + 8u, f17);
    put_float(rt, out + 0x18u, f18);
    put_float(rt, out + 0x28u, f10);
    put_float(rt, out + 0x38u, ctx.fpr[14]);
    put(rt, out + 0xCu, 0u);
    put(rt, out + 0x1Cu, 0u);
    put(rt, out + 0x2Cu, 0u);
}

// func_08B4C804 / func_08B4CBDC(out, x, y, z): a quaternion from three Euler angles: with the half angles (x, y, z halved) func_08B498AC(&s, &c) gives the sine and cosine of each (three calls into
// a 24-byte frame), then out[1] = s_x*c... in the order of the asm; the two functions differ only in the signs of four of the eight sums (`sign` columns).
// Branches: none, the function is straight-line.
template <bool MinusFirst, bool MinusThird>
void euler_quaternion(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const float half = fbits(0x3F000000u);
    const float f12 = psp_mul(ctx.fpr[12], half);
    const float f22 = ctx.fpr[13], f21 = ctx.fpr[14];
    GuestFrame frame(ctx, 0x40u);
    (void)call_keep(rt, ctx, 0x08B498ACu, {frame.at(0u), frame.at(4u)}, {f12});
    (void)call_keep(rt, ctx, 0x08B498ACu, {frame.at(8u), frame.at(0xCu)}, {psp_mul(f22, half)});
    (void)call_keep(rt, ctx, 0x08B498ACu, {frame.at(0x10u), frame.at(0x14u)}, {psp_mul(f21, half)});
    float f0 = float_at(rt, frame.at(8u));
    float f5 = float_at(rt, frame.at(0u));
    float f3 = float_at(rt, frame.at(0xCu));
    float f1 = float_at(rt, frame.at(4u));
    const float f2 = float_at(rt, frame.at(0x14u));
    float f6 = psp_mul(f5, f3);
    f3 = psp_mul(f3, f1);
    f5 = psp_mul(f5, f0);
    f1 = psp_mul(f1, f0);
    f0 = float_at(rt, frame.at(0x10u));
    const float f7 = psp_mul(f2, f3);
    float f4 = psp_mul(f2, f1);
    const float f8 = psp_mul(f6, f0);
    const float f9 = psp_mul(f0, f5);
    f6 = psp_mul(f6, f2);
    f1 = psp_mul(f1, f0);
    const float f2b = psp_mul(f2, f5);
    f0 = psp_mul(f0, f3);
    f4 = MinusFirst ? f4 - f8 : f4 + f8;
    f6 = f6 + f1;
    const float f7b = MinusThird ? f7 - f9 : f7 + f9;
    f0 = f0 - f2b;
    put_float(rt, out + 4u, f4);
    put_float(rt, out + 0xCu, f7b);
    put_float(rt, out, f6);
    put_float(rt, out + 8u, f0);
}
// The two entry points differ only in the signs of two sums (func_08B4C804: first sum plus, third minus; func_08B4CBDC: first minus, third plus); the rest of the words are the same.
void func_08B4C804(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    euler_quaternion<false, true>(rt, ctx);
}
void func_08B4CBDC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    euler_quaternion<true, false>(rt, ctx);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Serializers (virtual getters of an object packed into a block)
// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_088FA224(this, object): packs five one-bit flags and two bytes that `object` answers through its virtual slots into the word this[0]: bit 0 = slot 8, bit 1 = slot 0x14, bit 2 = slot 0x18, bit 3 = slot
// 0x1C, bit 4 = slot 0x20, bits 5-12 = slot 0xC, bits 13-20 = slot 0x10 (each result inserted with `ins`; the word is reloaded and stored back after every call). v0 = the last answer, v1 = the last word.
// Branches: none, the function is straight-line.
[[maybe_unused]] void func_088FA224(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    const auto field = [&](std::uint32_t slot, std::uint32_t position, std::uint32_t width, bool first) {
        const std::uint32_t vtable = word(rt, object);
        if (first) (void)call_keep(rt, ctx, word(rt, vtable + slot), {object});
        else (void)call_keep(rt, ctx, word(rt, vtable + slot), {object, vtable});
        const std::uint32_t mask = ((1u << width) - 1u) << position;
        const std::uint32_t value = (word(rt, self) & ~mask) | ((ctx.gpr[2] << position) & mask);
        ctx.gpr[3] = value;
        put(rt, self, value);
    };
    field(0x08u, 0u, 1u, true);
    field(0x14u, 1u, 1u, false);
    field(0x18u, 2u, 1u, false);
    field(0x1Cu, 3u, 1u, false);
    field(0x20u, 4u, 1u, false);
    field(0x0Cu, 5u, 8u, false);
    field(0x10u, 13u, 8u, false);
}

// func_088CDD7C(this, object): this[0] = slot 8, this[1] = slot 0x10, this[2] = slot 0x14, this[3] = slot 0xC, this[4] = slot 0x1C of `object` (floats, in f0); slot 0x18 fills this + 0x14 (a1 = this + 0x14),
// and the byte at this + 0x20 = slot 0x20. v0 = the last answer (a byte).
// Branches: none, the function is straight-line.
void func_088CDD7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    GuestResult result = call_keep(rt, ctx, word(rt, word(rt, object) + 8u), {object}, {});
    put_float(rt, self, result.f0);
    result = call_keep(rt, ctx, word(rt, word(rt, object) + 0x10u), {object}, {});
    put_float(rt, self + 4u, result.f0);
    result = call_keep(rt, ctx, word(rt, word(rt, object) + 0x14u), {object}, {});
    put_float(rt, self + 8u, result.f0);
    result = call_keep(rt, ctx, word(rt, word(rt, object) + 0xCu), {object}, {});
    put_float(rt, self + 0xCu, result.f0);
    result = call_keep(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object}, {});
    put_float(rt, self + 0x10u, result.f0);
    (void)call_keep(rt, ctx, word(rt, word(rt, object) + 0x18u), {object, self + 0x14u});
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t value = call_keep(rt, ctx, word(rt, vtable + 0x20u), {object, vtable});
    rt.memory().store8(self + 0x20u, static_cast<std::uint8_t>(value));
}

// func_088A84B0(this, object): func_088CDCFC(this, object); slot 0x40 of `object` fills this + 0x9C, slot 0x44 fills this + 0x54, slot 0x3C answers a pointer p: with p func_088FA224(this + 0xAC, p), without it bit 0 of
// this[0x2B] is cleared; then this[0x2C] = slot 0x34 (float), this[0x2D] = slot 0x38 (float). v0 = the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): slot 0x3C answering 0 (func_088FA224 is skipped and bit 0 of the word at this + 0xAC is cleared: `lw v0,0xAC(s1); ins v0,zero,0,1; sw`).
void func_088A84B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x088CDCFCu, {self});
    (void)call_keep(rt, ctx, word(rt, word(rt, object) + 0x40u), {object, self + 0x9Cu});
    (void)call_keep(rt, ctx, word(rt, word(rt, object) + 0x44u), {object, self + 0x54u});
    const std::uint32_t pointer = call_keep(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    if (pointer != 0u) {
        (void)call_keep(rt, ctx, 0x088FA224u, {self + 0xACu, pointer});
    } else {
        put(rt, self + 0xACu, word(rt, self + 0xACu) & ~1u);
    }
    GuestResult result = call_keep(rt, ctx, word(rt, word(rt, object) + 0x34u), {object}, {});
    put_float(rt, self + 0xB0u, result.f0);
    result = call_keep(rt, ctx, word(rt, word(rt, object) + 0x38u), {object}, {});
    put_float(rt, self + 0xB4u, result.f0);
}

// func_088A9D3C(this, out): this[2] is an object o (null: nothing happens, v0 = 0). With u = o[0xD] (a0) and the float o[0xD]... of the asm: a = func_08955E54(o[3], this[1]; f12 = o[0xD]), b = func_08956650(o[3], this[1];
// f12 = o[0xD]), c = func_089566F0(o[3], this[1]; f12 = o[0xD]) (the object is reloaded from this[2] before each call), the vector {a, b, c} is in the frame and func_08B4ACE0(&v, this[2] + 0x150, &v)
// transforms it in place; out[0..2] = the result.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null this[2] (the `beqz` lands on the epilogue, v0 = 0).
void func_088A9D3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t object = word(rt, self + 8u);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    const float a = call_keep(rt, ctx, 0x08955E54u, {word(rt, object + 0xCu), word(rt, self + 4u)}, {float_at(rt, object + 0x34u)}).f0;
    object = word(rt, self + 8u);
    const float b = call_keep(rt, ctx, 0x08956650u, {word(rt, object + 0xCu), word(rt, self + 4u)}, {float_at(rt, object + 0x34u)}).f0;
    object = word(rt, self + 8u);
    const float c = call_keep(rt, ctx, 0x089566F0u, {word(rt, object + 0xCu), word(rt, self + 4u)}, {float_at(rt, object + 0x34u)}).f0;
    put_float(rt, frame.at(8u), c);
    put_float(rt, frame.at(0u), a);
    put_float(rt, frame.at(4u), b);
    (void)call_keep(rt, ctx, 0x08B4ACE0u, {frame.at(0u), word(rt, self + 8u) + 0x150u, frame.at(0u)});
    const float f2 = float_at(rt, frame.at(4u)), f1 = float_at(rt, frame.at(8u)), f0 = float_at(rt, frame.at(0u));
    put_float(rt, out + 4u, f2);
    put_float(rt, out, f0);
    put_float(rt, out + 8u, f1);
}

// func_088AA074(this, out): this[2] is an object o (null: nothing happens, v0 = 0). The same three float queries as func_088A9D3C with func_08955CC8, func_08955DD0 and func_08955D4C (a, b, c);
// func_08B4C804(&q, f12 = -a, f13 = b, f14 = c) builds a quaternion in the frame, func_08B4B864(&q, this[2] + 0x190) multiplies it in, and func_08B4A744(&q) answers its squared length: func_08B4A6A0(&q)
// scales it by 1 / sqrt(that) (f12); out[0..3] = the four floats of the frame.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null this[2] (the `beqz` lands on the epilogue, v0 = 0) is the only path the compared cases take, which is why the function is not registered.
[[maybe_unused]] void func_088AA074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t object = word(rt, self + 8u);
    ctx.gpr[2] = object;
    if (object == 0u) return;
    const float a = call_keep(rt, ctx, 0x08955CC8u, {word(rt, object + 0xCu), word(rt, self + 4u)}, {float_at(rt, object + 0x34u)}).f0;
    object = word(rt, self + 8u);
    const float b = call_keep(rt, ctx, 0x08955DD0u, {word(rt, object + 0xCu), word(rt, self + 4u)}, {float_at(rt, object + 0x34u)}).f0;
    object = word(rt, self + 8u);
    const float c = call_keep(rt, ctx, 0x08955D4Cu, {word(rt, object + 0xCu), word(rt, self + 4u)}, {float_at(rt, object + 0x34u)}).f0;
    (void)call_keep(rt, ctx, 0x08B4C804u, {frame.at(0u)}, {-a, b, c});
    (void)call_keep(rt, ctx, 0x08B4B864u, {frame.at(0u), word(rt, self + 8u) + 0x190u});
    const float length_squared = call_keep(rt, ctx, 0x08B4A744u, {frame.at(0u)}, {}).f0;
    const float scale = 1.0f / std::sqrt(length_squared);
    (void)call_keep(rt, ctx, 0x08B4A6A0u, {frame.at(0u)}, {scale});
    const float f2 = float_at(rt, frame.at(4u)), f3 = float_at(rt, frame.at(8u)), f1 = float_at(rt, frame.at(0xCu)), f0 = float_at(rt, frame.at(0u));
    put_float(rt, out + 4u, f2);
    put_float(rt, out, f0);
    put_float(rt, out + 0xCu, f1);
    put_float(rt, out + 8u, f3);
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Singles, second round
// ---------------------------------------------------------------------------------------------------------------------------------------------------

// func_08BAA56C(begin, end, pair): for_each over the circular list that starts at *begin and ends at *end (the next element of e is *(e + 4)), calling the pointer to member function
// {pair[0], pair[1]} (adjust bit 0: virtual, pair[0] an offset into the vtable) on e + (adjust >> 1) with no further argument. v0 = pair[0], v1 = pair[1].
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the plain (non-virtual) form of the pointer to member (adjust bit 0 clear), and an element 0 (`movz`: the next pointer is read from address 0). The compared cases walk non-empty lists with the virtual form.
void func_08BAA56C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t begin = ctx.gpr[4], end_pointer = ctx.gpr[5], pair = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    std::uint32_t element = word(rt, begin);
    const std::uint32_t end = word(rt, end_pointer);
    const std::uint32_t function_word = word(rt, pair), adjust = word(rt, pair + 4u);
    ctx.gpr[2] = element;
    if (element != end) {
        for (;;) {
            const std::uint32_t receiver = element + static_cast<std::uint32_t>(sra1(adjust));
            const std::uint32_t function = (adjust & 1u) != 0u ? word(rt, word(rt, receiver) + function_word) : function_word;
            ctx.gpr[3] = function;
            (void)call_keep(rt, ctx, function, {receiver});
            const std::uint32_t next = word(rt, element == 0u ? 0u : element + 4u);
            ctx.gpr[2] = next;
            if (next == end) break;
            element = next;
        }
    }
    ctx.gpr[2] = function_word;
    ctx.gpr[3] = adjust;
}

// func_08AF2D20(): s = func_08AF296C() (a record of three words); its words are moved into the frame (the record is left zeroed, the swap goes through the frame's
// zero words), func_08C3F8BC(first, second, &frame) consumes them, and a non-null first word is freed with func_08B35254. v0 = the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a zero first word (the `beqz` skips func_08B35254); the compared cases never saw a non-zero word in the record, which is why the function is not registered.
[[maybe_unused]] void func_08AF2D20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t record = call_keep(rt, ctx, 0x08AF296Cu, {});
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0xCu), 0u);
    put(rt, frame.at(4u), word(rt, record));
    put(rt, record, 0u);
    const std::uint32_t second = word(rt, frame.at(8u));
    put(rt, frame.at(8u), word(rt, record + 4u));
    put(rt, record + 4u, second);
    const std::uint32_t third = word(rt, frame.at(0xCu));
    put(rt, frame.at(0xCu), word(rt, record + 8u));
    put(rt, record + 8u, third);
    (void)call_keep(rt, ctx, 0x08C3F8BCu, {word(rt, frame.at(4u)), word(rt, frame.at(8u)), frame.at(0u)});
    const std::uint32_t first = word(rt, frame.at(4u));
    ctx.gpr[2] = first;
    if (first != 0u) (void)call_keep(rt, ctx, 0x08B35254u, {first});
}

// func_089ABF68(this): a local holder h is filled by func_089AA658(&h, this); its object o = h[0] (null: the assert func_08A5BB7C(0x08CA50AC), the code goes on with the null object) must answer
// func_089A717C(o) non-zero, and then func_089A7120(o) zero: only then a second holder g (frame[0]) is filled by func_089A6A58(&g, 0x08CF6ED8), handed to func_089ABDCC(this, &g) and released.
// h is released last. v0 = that release's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): null objects (the `bnez` skips the assert func_08A5BB7C(0x08CA50AC); the code goes on with the null object) and func_089A7120 answering 0 (the second holder: func_089A6A58, func_089ABDCC and its release). The compared cases take func_089A7120 non-zero.
void func_089ABF68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x089AA658u, {frame.at(4u), self});
    std::uint32_t object = word(rt, frame.at(4u));
    if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA50ACu});
    if (call_keep(rt, ctx, 0x089A717Cu, {object}) != 0u) {
        object = word(rt, frame.at(4u));
        if (object == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA50ACu});
        if (call_keep(rt, ctx, 0x089A7120u, {object}) == 0u) {
            (void)call_keep(rt, ctx, 0x089A6A58u, {frame.at(0u), 0x08CF6ED8u});
            (void)call_keep(rt, ctx, 0x089ABDCCu, {self, frame.at(0u)});
            (void)call_keep(rt, ctx, kReleaseLocal, {frame.at(0u)});
        }
    }
    (void)call_keep(rt, ctx, kReleaseLocal, {frame.at(4u)});
}

// func_08B86674(this, size, fill): basic_string::resize(size, fill) of the 1-byte-character string. A size above 0x3FFFFFFC is the length error func_08B5ADDC(0x08C97C18). A size above
// the length appends the difference (tail call func_08B86598(this, size - length, fill), v0 = 1); a smaller one erases the tail (tail call func_08B8634C(this, size, length - size, 0), v0 = 0);
// the same size does nothing (v0 = 0). The fill is the sign-extended low byte of a2.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the length error (the callee does not return) and the out-of-range check of the erase
// arm (func_08B5AEDC; it needs size > length inside the arm where size < length, which cannot happen).
void func_08B86674(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], size = ctx.gpr[5];
    const std::uint32_t fill = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(ctx.gpr[6])));
    std::uint32_t length;
    {
        GuestFrame frame(ctx, 0x10u);
        length = word(rt, word(rt, self) - 0xCu);
        if (0x3FFFFFFCu < size) {
            ctx.gpr[4] = 0x08C97C18u;
            ctx.gpr[6] = length;
            (void)call_keep(rt, ctx, 0x08B5ADDCu, {0x08C97C18u, size, length});
        }
    }
    if (length < size) {
        ctx.gpr[2] = 1u;
        tail_call(ctx, 0x08B86598u, {self, size - length, fill});
        return;
    }
    ctx.gpr[2] = 0u;
    if (size < length) tail_call(ctx, 0x08B8634Cu, {self, size, length - size, 0u});
}

// func_08949E8C(this): o = this[2], p = this[3]; t = (float)(int) the float at o + 0x594; when func_08949A08(p) answers non-zero and |t - (float) the halfword at p + 0x23E| < 100, t += random * 60 - 10
// (random = func_08B1ECFC with f12 = 1.0); the answer is the truncation of t. v0 = (int) t.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): func_08949A08 answering non-zero (the halfword at +0x23E, the distance test and the random term). The compared cases all take the `beql` (answer 0) and answer trunc(t).
void func_08949E8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t owner = word(rt, self + 8u), target = word(rt, self + 0xCu);
    const float raw = float_at(rt, owner + 0x594u);
    float t = cvt_sw(ctx.fpu_float_to_word_ct<1u>(raw));
    if (call_keep(rt, ctx, 0x08949A08u, {target}) != 0u) {
        const std::uint32_t half = rt.memory().load16(target + 0x23Eu);
        float difference = t - cvt_sw(half);
        if (difference < 0.0f) difference = -difference;
        if (difference < fbits(0x42C80000u)) {
            const float random = call_keep(rt, ctx, 0x08B1ECFCu, {}, {1.0f}).f0;
            t = t + (psp_mul(random, fbits(0x42700000u)) - fbits(0x41200000u));
        }
    }
    ctx.gpr[2] = ctx.fpu_float_to_word_ct<1u>(t);
}

// func_08B4F5E0(this, other): q = func_08C524EC(other) (a float): below 0.9 (0x3F666666) func_08B509C4(other, &x, &y, &z) then func_08B4C6BC(this, x, y, z); otherwise func_08B509CC(other, &x, &y, &z) then
// func_08B4C804(this, x, y, z). v0 = the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): none: both arms (below 0.9 and not) are taken.
void func_08B4F5E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const float q = call_keep(rt, ctx, 0x08C524ECu, {other}, {}).f0;
    const bool lower = q < fbits(0x3F666666u);
    (void)call_keep(rt, ctx, lower ? 0x08B509C4u : 0x08B509CCu, {other, frame.at(0u), frame.at(4u), frame.at(8u)});
    const float x = float_at(rt, frame.at(0u)), y = float_at(rt, frame.at(4u)), z = float_at(rt, frame.at(8u));
    (void)call_keep(rt, ctx, lower ? 0x08B4C6BCu : 0x08B4C804u, {self}, {x, y, z});
}

// func_08C52868(m, out_a, out_b, out_c): the Euler angles of the matrix m: *out_a = a = func_08C5250C(-m[2]) (a float) and s = sin(a) (func_08B499B8); when s is 0 *out_b = 0 and *out_c =
// atan2(-m[4], m[5]) (func_08B4A430); otherwise *out_b = atan2(m[6], m[10]) and *out_c = atan2(m[1], m[0]). v0 = the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the sine being 0 (the `bc1tl` takes its delay-slot store of 0.0 and jumps to the single atan2 of the +0x10 / +0x14 pair).
void func_08C52868(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t matrix = ctx.gpr[4], out_b = ctx.gpr[5], out_a = ctx.gpr[6], out_c = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const float a = call_keep(rt, ctx, 0x08C5250Cu, {}, {-float_at(rt, matrix + 8u)}).f0;
    put_float(rt, out_a, a);
    const float s = call_keep(rt, ctx, 0x08B499B8u, {}, {a}).f0;
    if (s == 0.0f) {
        put_float(rt, out_b, 0.0f);
        const float c = call_keep(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, matrix + 0x10u), float_at(rt, matrix + 0x14u)}).f0;
        put_float(rt, out_c, c);
    } else {
        const float b = call_keep(rt, ctx, 0x08B4A430u, {}, {float_at(rt, matrix + 0x18u), float_at(rt, matrix + 0x28u)}).f0;
        put_float(rt, out_b, b);
        const float c = call_keep(rt, ctx, 0x08B4A430u, {}, {float_at(rt, matrix + 4u), float_at(rt, matrix)}).f0;
        put_float(rt, out_c, c);
    }
}

// func_08904A04(this, a1, p): y = p[1]; unless y < this[9] (the float at +0x24): f = min(y, this[0xC]) (this[0xC] is +0x30), and func_089041C8(this, a1, this + 0x20, &v, &w, 0, the halfword at +0x4C minus 1, 0) runs with the
// frame v = {p[0], f, p[2]} and w = {p[0], this[9], p[2]}. v0 = 0 when y < this[9], else the call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): y below the limit (the `bc1t` lands on the epilogue with v0 = 0 from its delay slot).
void func_08904A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], vector = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const float y = float_at(rt, vector + 4u);
    const float limit = float_at(rt, self + 0x24u);
    ctx.gpr[2] = 0u;
    if (y < limit) return;
    const float upper = float_at(rt, self + 0x30u);
    const float x = float_at(rt, vector);
    float f = y;
    put_float(rt, frame.at(0u), x);
    if (upper < y) f = upper;
    const std::uint32_t count = rt.memory().load16(self + 0x4Cu) - 1u;
    const float z = float_at(rt, vector + 8u);
    put_float(rt, frame.at(4u), f);
    put_float(rt, frame.at(0xCu), x);
    put_float(rt, frame.at(0x10u), limit);
    put_float(rt, frame.at(0x14u), z);
    put_float(rt, frame.at(8u), z);
    (void)call_keep(rt, ctx, 0x089041C8u, {self, ctx.gpr[5], self + 0x20u, frame.at(0u), frame.at(0xCu), 0u, count, 0u});
}


// Drops one reference of a 1-byte-character string whose data pointer is `chars` (its representation is chars - 0xC) unless it is the shared empty one: func_08B60804(chars - 4, -1), and when the
// count is not positive func_08B8542C(rep, scratch) frees it. v0 is the last call's, or `skipped_v0` when the representation is the empty one.
inline void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t chars, std::uint32_t scratch, std::uint32_t skipped_v0) {
    const std::uint32_t rep = chars - 0xCu;
    ctx.gpr[2] = skipped_v0;
    if (rep == kStringEmpty) return;
    const std::uint32_t count = call_keep(rt, ctx, 0x08B60804u, {chars - 4u, 0xFFFFFFFFu});
    if (static_cast<std::int32_t>(count) <= 0) (void)call_keep(rt, ctx, 0x08B8542Cu, {rep, scratch});
}

// func_08C527C0(m, out_b, out_c, out_a): like func_08C52868 with the angle taken from m[1]: *out_a = a = func_08C5250C(m[1]) (the fourth argument); s = sin(a) (func_08B499B8);
// with s == 0 the second argument gets 0 and *out_c = atan2(m[8], m[10]) (func_08B4A430); otherwise *out_b = atan2(-m[9], m[5]) and *out_c = atan2(-m[2], m[0]). v0 = the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the sine being 0 (the `bc1tl` takes its delay-slot store of 0.0 and jumps to the single atan2 of the +0x20 / +0x28 pair).
void func_08C527C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t matrix = ctx.gpr[4], out_b = ctx.gpr[5], out_c = ctx.gpr[6], out_a = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const float a = call_keep(rt, ctx, 0x08C5250Cu, {}, {float_at(rt, matrix + 4u)}).f0;
    put_float(rt, out_a, a);
    const float s = call_keep(rt, ctx, 0x08B499B8u, {}, {a}).f0;
    if (s == 0.0f) {
        put_float(rt, out_b, 0.0f);
        const float c = call_keep(rt, ctx, 0x08B4A430u, {}, {float_at(rt, matrix + 0x20u), float_at(rt, matrix + 0x28u)}).f0;
        put_float(rt, out_c, c);
    } else {
        const float b = call_keep(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, matrix + 0x24u), float_at(rt, matrix + 0x14u)}).f0;
        put_float(rt, out_b, b);
        const float c = call_keep(rt, ctx, 0x08B4A430u, {}, {-float_at(rt, matrix + 8u), float_at(rt, matrix)}).f0;
        put_float(rt, out_c, c);
    }
}

// func_08AAD700(table, key, owner_word): a 2048-entry cache of 16-byte entries {key, check, owner, holder} at `table`, indexed by ((key >> 3) ^ (*owner_word >> 4)) & 0x7FF. A non-zero key whose entry holds the same key,
// the same check (key[5]) and the same owner word is evicted: key and check are zeroed, func_08A6A23C(entry + 8, 0x08CF6FDC) resets the owner field, a temporary holder is filled by func_08A5D414(&t,
// 0x08CF6ED8) and assigned to the entry's holder (func_08A5BAD8(entry + 0xC, t[0]) unless they are the same address) and released. v0 is what the instructions leave on each exit.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a zero key, a different check word, a different owner word (each lands on the epilogue with v0 = the word just compared) and the holder being the entry's own slot (the `beq` skips func_08A5BAD8).
void func_08AAD700(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t table = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    if (key == 0u) return;
    const std::uint32_t owner = word(rt, ctx.gpr[6]);
    ctx.gpr[6] = owner;
    const std::uint32_t index = (((static_cast<std::uint32_t>(static_cast<std::int32_t>(key) >> 3)) ^ (owner >> 4)) & 0x7FFu) << 4;
    const std::uint32_t entry = table + index;
    ctx.gpr[7] = entry;
    ctx.gpr[2] = index;
    const std::uint32_t stored_key = word(rt, entry);
    ctx.gpr[3] = stored_key;
    if (key != stored_key) return;
    const std::uint32_t check = word(rt, key + 0x14u);
    ctx.gpr[3] = check;
    const std::uint32_t stored_check = word(rt, entry + 4u);
    ctx.gpr[2] = stored_check;
    if (stored_check != check) return;
    const std::uint32_t stored_owner = word(rt, entry + 8u);
    ctx.gpr[2] = stored_owner;
    if (owner != stored_owner) return;
    put(rt, entry, 0u);
    put(rt, entry + 4u, 0u);
    (void)call_keep(rt, ctx, 0x08A6A23Cu, {entry + 8u, 0x08CF6FDCu});
    (void)call_keep(rt, ctx, 0x08A5D414u, {frame.at(0u), 0x08CF6ED8u});
    if (entry + 0xCu != frame.at(0u)) (void)call_keep(rt, ctx, 0x08A5BAD8u, {entry + 0xCu, word(rt, frame.at(0u))});
    (void)call_keep(rt, ctx, kReleaseLocal, {frame.at(0u)});
}

// func_08AF2238(this, a1): func_08B569A0(), func_08B564A8(), func_08AF22A4(this, a1), func_08B56940(); then, when the byte this[8] is set, the tail call func_08B58218(4). v0 = that byte, or 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the byte this[8] being 0 (the `beqz` to the return, v0 = 0; the delay slot already set a0 = 4).
void func_08AF2238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], second = ctx.gpr[5];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B569A0u, {});
        (void)call_keep(rt, ctx, 0x08B564A8u, {});
        (void)call_keep(rt, ctx, 0x08AF22A4u, {self, second});
        (void)call_keep(rt, ctx, 0x08B56940u, {});
        ctx.gpr[2] = rt.memory().load8(self + 0x20u);
        ctx.gpr[4] = 4u;
        if (ctx.gpr[2] == 0u) return;
    }
    tail_call(ctx, 0x08B58218u, {4u});
}

// func_08C2273C(this): this[0] = vtable; the strings at this[7] and this[6] are released (a representation other than the shared empty one loses a reference, func_08B60804(data - 4, -1),
// and is freed with func_08B8542C(rep, &frame + 1 / &frame) when the count is not positive); this[0] = the base vtable and the block is freed with func_08A5BC54(this, 0x24, 4). v0 = the free's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the shared empty strings (the `beq` lands on the next step).
void func_08C2273C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CD36C0u);
    release_string(rt, ctx, word(rt, self + 0x1Cu), frame.at(1u), 0x08CD36C0u);
    release_string(rt, ctx, word(rt, self + 0x18u), frame.at(0u), word(rt, self + 0x18u));
    put(rt, self, 0x08CD3680u);
    (void)call_keep(rt, ctx, 0x08A5BC54u, {self, 0x24u, 4u});
}

// func_08C21340(this, other): other[0x12] = this[5]; func_08A72B94(other[0xF]); other[0x10] -= 1; other[0xF] = this[4]; other[0xE] = this[3]; func_08A5B970(other[0xD]); other[0xD] = this[2];
// other[0xC] = this[1]; other[5] = (this[2] == 0 ? 0 : this[2][0xC][10]) (a null this[2][0xC] is the assert func_08A5BB7C(0x08CB02A4), the code goes on with it); a non-zero this[8] is stored in other[6]. v0 = this[2] (or the assert's).
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null this[2][0xC] (the assert func_08A5BB7C(0x08CB02A4); the code goes on with the null record).
void func_08C21340(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    put(rt, other + 0x48u, word(rt, self + 0x14u));
    (void)call_keep(rt, ctx, 0x08A72B94u, {word(rt, other + 0x3Cu)});
    const std::uint32_t remaining = word(rt, other + 0x40u) - 1u;
    const std::uint32_t old_holder = word(rt, other + 0x34u);
    put(rt, other + 0x40u, remaining);
    put(rt, other + 0x3Cu, word(rt, self + 0x10u));
    const std::uint32_t moved = word(rt, self + 0xCu);
    put(rt, other + 0x38u, moved);
    (void)call_keep(rt, ctx, 0x08A5B970u, {old_holder});
    put(rt, other + 0x34u, word(rt, self + 8u));
    const std::uint32_t first = word(rt, self + 4u);
    put(rt, other + 0x30u, first);
    ctx.gpr[3] = first;
    const std::uint32_t source = word(rt, self + 8u);
    ctx.gpr[2] = source;
    std::uint32_t value = 0u;
    if (source != 0u) {
        const std::uint32_t inner = word(rt, source + 0x30u);
        value = 0x08CB02A4u;
        if (inner == 0u) (void)call_keep(rt, ctx, 0x08A5BB7Cu, {value});
        value = word(rt, inner + 0x28u);
    }
    put(rt, other + 0x14u, value);
    const std::uint32_t last = word(rt, self + 0x20u);
    if (last != 0u) put(rt, other + 0x18u, last);
}

// func_08881F18(this, other): when func_088819D0(this[3], this[2]) answers non-zero, the byte this[1][8] is set, e = func_0886A9A0(this[1], 3) is non-null (the record at e + 0xC) with bit 3 of its word [1] set
// and other's virtual slot 0x24 answers a non-zero q: the tail call func_08881E10(this[3], q, bit 2 of record[1]). Every other path answers 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): every early exit (func_088819D0 answering 0, the byte this[1][8] clear, no record from func_0886A9A0, bit 3 clear, slot 0x24 answering 0), each an epilogue with v0 = 0. The compared cases all take the tail call.
void func_08881F18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], other = ctx.gpr[5];
    std::uint32_t query, record_word;
    {
        GuestFrame frame(ctx, 0x10u);
        if (call_keep(rt, ctx, 0x088819D0u, {word(rt, self + 0xCu), word(rt, self + 8u)}) == 0u) return;
        const std::uint32_t target = word(rt, self + 4u);
        const std::uint32_t flag = rt.memory().load8(target + 0x20u);
        ctx.gpr[2] = flag;
        if (flag == 0u) return;
        const std::uint32_t found = call_keep(rt, ctx, 0x0886A9A0u, {target, 3u});
        if (found == 0u) return;
        const std::uint32_t record = found + 0xCu;
        ctx.gpr[2] = (word(rt, record + 4u) >> 3) & 1u;
        if (ctx.gpr[2] == 0u) return;
        query = call_keep(rt, ctx, word(rt, word(rt, other) + 0x24u), {other});
        if (query == 0u) return;
        record_word = word(rt, record + 4u);
    }
    tail_call(ctx, 0x08881E10u, {word(rt, self + 0xCu), query, (record_word >> 2) & 1u});
}

// func_088CDC48(this, object): func_088CDE3C(this, object); this[0x11] = slot 0x48 of `object` (float); bits 0-3 of this[0x12] = the answers of slots 0x24, 0x28, 0x30, 0x34 (each inserted with
// `ins`, the word reloaded and stored after every call). v0 = the last answer.
// Branches: none, the function is straight-line.
void func_088CDC48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    (void)call_keep(rt, ctx, 0x088CDE3Cu, {self, object});
    GuestResult result = call_keep(rt, ctx, word(rt, word(rt, object) + 0x48u), {object}, {});
    put_float(rt, self + 0x44u, result.f0);
    result = call_keep(rt, ctx, word(rt, word(rt, object) + 0x24u), {object}, {});
    put(rt, self + 0x48u, (word(rt, self + 0x48u) & ~1u) | (result.v0 & 1u));
    unsigned bit = 1u;
    for (const std::uint32_t slot : {0x28u, 0x30u, 0x34u}) {
        const std::uint32_t vtable = word(rt, object);
        const std::uint32_t answer = call_keep(rt, ctx, word(rt, vtable + slot), {object, vtable});
        const std::uint32_t current = word(rt, self + 0x48u);
        const std::uint32_t value = (current & ~(1u << bit)) | ((answer & 1u) << bit);
        ctx.gpr[3] = value;
        put(rt, self + 0x48u, value);
        ++bit;
    }
}

// func_08C21118(this, object): s = a string built from object[5] (the C string at that pointer; a null pointer uses the string at 0x08CB02B4) by func_08B85EB8(&s, text, &frame + 1); a holder h is filled by
// func_08AAF890(&h, &s, object[6]), handed over with func_08A61584(object, &h) and released; s loses its reference (as in func_08C2273C). v0 = the last call's, or 0x08D74FD0 when s is the shared empty string.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a null text pointer (the default string) and the shared empty string.
void func_08C21118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    std::uint32_t text = word(rt, object + 0x14u);
    if (text == 0u) text = 0x08CB02B4u;
    (void)call_keep(rt, ctx, 0x08B85EB8u, {frame.at(4u), text, frame.at(1u)});
    (void)call_keep(rt, ctx, 0x08AAF890u, {frame.at(8u), frame.at(4u), word(rt, object + 0x18u)});
    (void)call_keep(rt, ctx, 0x08A61584u, {object, frame.at(8u)});
    (void)call_keep(rt, ctx, kReleaseLocal, {frame.at(8u)});
    release_string(rt, ctx, word(rt, frame.at(4u)), frame.at(0u), kStringEmpty);
}

// func_08B66604(x): a sine / cosine of x split by range: for |x| (bit pattern) above 0x3F490FDA (pi / 4) a non-finite x answers x - x, otherwise func_08B6BAFC(&y, x, 0) reduces it into y0, y1 and the quadrant
// n (v0), then func_08B6DFD0(1 - 2 * (n & 1), y0, y1) evaluates it; at or below pi / 4 the evaluation gets (1, x, 0). Result in f0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): |x| above pi/4: the non-finite test (f0 = x - x), func_08B6BAFC and the sign from the quadrant. The compared cases all have |x| at or below pi/4 (func_08B6DFD0(1, x, 0)).
void func_08B66604(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const std::uint32_t magnitude = bits_of(x) & 0x7FFFFFFFu;
    const bool non_finite = static_cast<std::int32_t>(0x7F7FFFFFu) < static_cast<std::int32_t>(magnitude);
    const bool reduce = static_cast<std::int32_t>(0x3F490FDAu) < static_cast<std::int32_t>(magnitude);
    GuestFrame frame(ctx, 0x20u);
    ctx.fpr[13] = 0.0f;
    std::uint32_t sign = 1u;
    float y0 = x, y1 = 0.0f;
    if (reduce) {
        if (non_finite) {
            ctx.fpr[0] = x - x;
            return;
        }
        const std::uint32_t quadrant = call_keep(rt, ctx, 0x08B6BAFCu, {frame.at(0u)}, {x, 0.0f}).v0;
        y0 = float_at(rt, frame.at(0u));
        y1 = float_at(rt, frame.at(4u));
        sign = 1u - ((quadrant & 1u) << 1);
    }
    ctx.fpr[0] = call_keep(rt, ctx, 0x08B6DFD0u, {sign}, {y0, y1}).f0;
}


// The call target of a pointer to member function {pointer, adjust}: the object is `base + (adjust >> 1)`; when adjust bit 0 is set `pointer` is a vtable offset (the function is *(*object + pointer)),
// else it is the function itself.
inline std::uint32_t ptmf_function(psprecomp::Runtime &rt, std::uint32_t object, std::uint32_t pointer, std::uint32_t adjust) {
    return (adjust & 1u) != 0u ? word(rt, word(rt, object) + pointer) : pointer;
}

// func_08C137D4(out, descriptor, object): r1 = the pointer to member function {descriptor[5], descriptor[6]} called on `object` (a0 = object + adjust >> 1, a1 = the function word); r2 = the pointer to member function
// {descriptor[7], descriptor[8]} called on r1 (a0 = r1 + adjust >> 1, a1 = r1); func_089FA280(out, r2). v0 = out.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): both virtual forms (adjust bit 0: the `bnez` / `beqz` land on the vtable loads, *(*receiver + word)).
void func_08C137D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], descriptor = ctx.gpr[5], object = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t adjust = word(rt, descriptor + 0x18u);
    const std::uint32_t receiver = object + static_cast<std::uint32_t>(sra1(adjust));
    const std::uint32_t pointer = word(rt, descriptor + 0x14u);
    const std::uint32_t first = call_keep(rt, ctx, ptmf_function(rt, receiver, pointer, adjust), {receiver, pointer});
    const std::uint32_t adjust2 = word(rt, descriptor + 0x20u);
    const std::uint32_t receiver2 = first + static_cast<std::uint32_t>(sra1(adjust2));
    const std::uint32_t second = call_keep(rt, ctx, ptmf_function(rt, receiver2, word(rt, descriptor + 0x1Cu), adjust2), {receiver2, first});
    (void)call_keep(rt, ctx, 0x089FA280u, {out, second});
    ctx.gpr[2] = out;
}

// func_08AFEB98(this, vector): this[0x14..0x16] = the three words of `vector`; when the float at block + 0x60 (block = this[0x36]) is 0 the floats this[0x11], this[0x12], this[0x13] are zeroed (v0 = this + 0x44).
// Otherwise f = func_08C45F94(this + 0x5C, block) is stored in the frame, r = func_08C45E18(&frame, this + 0x50) (a pair v0 / v1) is stored too, func_08C464B4(&frame + 0xC, *r.v0) and
// func_08C468E4(this + 0x44, &frame + 0xC, frame[2] = r.v1) finish it. v0 = the last call's.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the float at block + 0x60 equal to 0 (the `bc1t` lands on the three zero stores, v0 = this + 0x44 from its delay slot).
void func_08AFEB98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], vector = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const std::uint32_t w0 = word(rt, vector), w2 = word(rt, vector + 8u), w1 = word(rt, vector + 4u);
    const std::uint32_t block = word(rt, self + 0xD8u);
    put(rt, self + 0x50u, w0);
    put(rt, self + 0x54u, w1);
    put(rt, self + 0x58u, w2);
    if (float_at(rt, block + 0x60u) == 0.0f) {
        put_float(rt, self + 0x44u, 0.0f);
        put_float(rt, self + 0x4Cu, 0.0f);
        put_float(rt, self + 0x48u, 0.0f);
        ctx.gpr[2] = self + 0x44u;
        return;
    }
    const GuestResult first = call_keep(rt, ctx, 0x08C45F94u, {self + 0x5Cu, block}, {});
    put(rt, frame.at(0u), first.v0);
    const GuestResult second = call_keep(rt, ctx, 0x08C45E18u, {frame.at(0u), self + 0x50u}, {});
    put(rt, frame.at(8u), second.v1);
    put(rt, frame.at(0x30u), second.v0);
    put(rt, frame.at(0x34u), second.v1);
    put(rt, frame.at(4u), second.v0);
    (void)call_keep(rt, ctx, 0x08C464B4u, {frame.at(0xCu), word(rt, second.v0)});
    (void)call_keep(rt, ctx, 0x08C468E4u, {self + 0x44u, frame.at(0xCu), word(rt, frame.at(8u))});
}

// func_08C1FB10(this, stack): func_08A615B4(stack, 0), func_08A615BC(stack, this[3]); then, while the number of entries between stack[9] and the top pointer stack[10] ((stack[10] - stack[9]) >> 2) is above
// this[2] + stack[0x12], the entry before the top is an object o: its virtual slot 0x10 and then slot 4 run and the top pointer drops by 4. v0 = 0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a round of the loop (the `bnez` back to the body: the object before the top, its slots 0x10 and 4, the top pointer drops by 4). The compared cases leave at the first test.
void func_08C1FB10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], stack = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    (void)call_keep(rt, ctx, 0x08A615B4u, {stack, 0u});
    (void)call_keep(rt, ctx, 0x08A615BCu, {stack, word(rt, self + 0xCu)});
    const std::uint32_t pointer_slot = stack + 0x24u;
    const std::uint32_t base = word(rt, self + 8u);
    for (;;) {
        const std::uint32_t top = word(rt, pointer_slot + 4u);
        const std::uint32_t count = static_cast<std::uint32_t>(static_cast<std::int32_t>(top - word(rt, stack + 0x24u)) >> 2);
        const std::uint32_t limit = base + word(rt, stack + 0x48u);
        ctx.gpr[5] = stack;
        ctx.gpr[2] = limit < count ? 1u : 0u;
        if (!(limit < count)) break;
        const std::uint32_t object = word(rt, top - 4u);
        (void)call_keep(rt, ctx, word(rt, word(rt, object) + 0x10u), {object});
        (void)call_keep(rt, ctx, word(rt, word(rt, object) + 4u), {object});
        put(rt, pointer_slot + 4u, word(rt, pointer_slot + 4u) - 4u);
    }
}

// func_08B5547C(a0, a1, a2, a3): two packed words p = (a0 & 0xFFFF) | (a1 << 16) and q = (a2 & 0xFFFF) | (a3 << 16); func_08B59118(kGuState, 0xBFD) then the tail call func_08B553F8(kGuState, p, q).
// Branches: none, the function is straight-line.
void func_08B5547C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = (ctx.gpr[4] & 0xFFFFu) | (ctx.gpr[5] << 16);
    const std::uint32_t second = (ctx.gpr[6] & 0xFFFFu) | (ctx.gpr[7] << 16);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_keep(rt, ctx, 0x08B59118u, {kGuState, 0xBFDu});
    }
    tail_call(ctx, 0x08B553F8u, {kGuState, first, second});
}

// func_08BB0C98(begin, end, pair): for_each over the array of words [begin, end): each element e runs the pointer to member function {pair[0], pair[1]} (adjust bit 0: virtual, pair[0] an offset into the
// vtable) on e + (adjust >> 1) with no further argument. v0 = pair[0], v1 = pair[1].
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the virtual form of the pointer to member (adjust bit 0 set: `lw v0,0(a0); addu; lw v0,0(v0)`) and an empty array (the `beq` lands on the return). The compared cases walk non-empty arrays with the plain form.
void func_08BB0C98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t begin = ctx.gpr[4], end = ctx.gpr[5], pair = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t function_word = word(rt, pair), adjust = word(rt, pair + 4u);
    if (begin != end) {
        std::uint32_t cursor = begin;
        do {
            const std::uint32_t element = word(rt, cursor);
            cursor += 4u;
            const std::uint32_t receiver = element + static_cast<std::uint32_t>(sra1(adjust));
            const std::uint32_t function = (adjust & 1u) != 0u ? word(rt, word(rt, receiver) + function_word) : function_word;
            (void)call_keep(rt, ctx, function, {receiver});
        } while (end != cursor);
    }
    ctx.gpr[2] = function_word;
    ctx.gpr[3] = adjust;
}

// func_08907AA0(this): the whole number of rows between two floats: a = trunc(this[6] / this[5]), b = trunc(this[9][1] / this[5]) (this[9] is the pointer at +0x24); v0 = max(a - b - 1, 1).
// Branches the routes do not take (checked against the asm by hand): the `max` answering 1 (a difference below 2); the compared cases always answer the difference.
void func_08907AA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t other = word(rt, self + 0x24u);
    const float f2 = float_at(rt, self + 0x14u), f0 = float_at(rt, self + 0x18u);
    const float f1 = float_at(rt, other + 4u);
    const float a = f0 / f2, b = f1 / f2;
    const std::int32_t whole = static_cast<std::int32_t>(ctx.fpu_float_to_word_ct<1u>(a));
    const std::int32_t other_whole = static_cast<std::int32_t>(ctx.fpu_float_to_word_ct<1u>(b));
    const std::int32_t rows = whole - other_whole - 1;
    ctx.gpr[3] = 1u;
    ctx.gpr[2] = static_cast<std::uint32_t>(rows > 1 ? rows : 1);
}

// func_088CBFD4(): the address of a function-local static object (0x08CE9BA0) built on first use: the guard byte at 0x08CE9B98 is tested; when 0 and func_08B6441C(guard) lets this caller build it, the object gets
// vtable 0x08CC34D8 (stored in the delay slot of the release), words +4, +0xC, +0x10 = 0 and +0x14 = -1, func_08B64494(guard, object) releases the guard, and func_08B77B1C(0x088CC95C) registers the
// destructor. v0 = the object's address.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard byte already set and func_08B6441C answering 0 (both land on the epilogue with v0 = the object's address): the compared cases always build.
void func_088CBFD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t guard = 0x08CE9B98u, object = 0x08CE9BA0u;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t flag = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
    ctx.gpr[3] = flag;
    if (flag == 0u && call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
        put(rt, object + 0x14u, 0xFFFFFFFFu);
        put(rt, object + 4u, 0u);
        put(rt, object + 0xCu, 0u);
        put(rt, object + 0x10u, 0u);
        put(rt, object, 0x08CC34D8u);
        (void)call_keep(rt, ctx, 0x08B64494u, {guard, object});
        (void)call_keep(rt, ctx, 0x08B77B1Cu, {0x088CC95Cu});
    }
    ctx.gpr[2] = object;
}

// func_08BB1F48(): the same shape for the static object at 0x08D75F9C (guard byte 0x08D75F90): +0x38 = 0x20, +0x10 = 0x24, +0x14 = 0x20, the other words up to +0x34 = 0, and word 0 = 0 stored in the delay
// slot of func_08B64494(guard, 0x20); then func_08B77B1C(0x088B82DC). v0 = the object's address.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the guard byte already set and func_08B6441C answering 0 (both land on the epilogue with v0 = the object's address): the compared cases always build.
void func_08BB1F48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t guard = 0x08D75F90u, object = 0x08D75F9Cu;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t flag = static_cast<std::uint32_t>(static_cast<std::int32_t>(byte_at(rt, guard)));
    ctx.gpr[3] = flag;
    if (flag == 0u) {
        const bool build = call_keep(rt, ctx, 0x08B6441Cu, {guard}) != 0u;
        ctx.gpr[5] = 0x20u;
        if (build) {
            put(rt, object + 0x38u, 0x20u);
            put(rt, object + 0x10u, 0x24u);
            for (const std::uint32_t offset : {4u, 8u, 0xCu}) put(rt, object + offset, 0u);
            put(rt, object + 0x14u, 0x20u);
            for (const std::uint32_t offset : {0x18u, 0x1Cu, 0x20u, 0x24u, 0x28u, 0x2Cu, 0x30u, 0x34u}) put(rt, object + offset, 0u);
            put(rt, object, 0u);
            (void)call_keep(rt, ctx, 0x08B64494u, {guard, 0x20u});
            (void)call_keep(rt, ctx, 0x08B77B1Cu, {0x088B82DCu});
        }
    }
    ctx.gpr[2] = object;
}

// func_08C34904(this, key): first = func_08C34770(this, key), last = func_08C3483C(this, key); the number of steps func_08B5F100 needs from first to reach last (0 when they are the same) is counted;
// func_08C34650(this, first, last) is called with the two ends also stored in the frame. v0 = the count.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): first == last (the `beq` skips the loop, the count stays 0 from its delay slot) and a loop of more than one round (the `bne` back to the call).
void func_08C34904(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t first = call_keep(rt, ctx, 0x08C34770u, {});
    const std::uint32_t last = call_keep(rt, ctx, 0x08C3483Cu, {self, key});
    std::uint32_t count = 0u;
    if (first != last) {
        std::uint32_t node = first;
        do {
            node = call_keep(rt, ctx, 0x08B5F100u, {node});
            ++count;
        } while (node != last);
    }
    put(rt, frame.at(0u), first);
    put(rt, frame.at(4u), last);
    (void)call_keep(rt, ctx, 0x08C34650u, {self, first, last});
    ctx.gpr[2] = count;
}


// ---------------------------------------------------------------------------------------------------------------------------------------------------
// Math and graphics-state singles
// ---------------------------------------------------------------------------------------------------------------------------------------------------

inline void store16(psprecomp::Runtime &rt, std::uint32_t address, std::uint32_t value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline std::uint32_t sext16(std::uint32_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(value))); }

// func_08B4A430(y = f12, x = f13): atan2 of two floats. Both below the smallest normal (0x00800000) in magnitude answer 0. Otherwise the signs are folded away (bit 2 = y negative, bit 1 = x negative) and the
// smaller magnitude is divided by the larger (bit 0 set when x is the smaller one: the quotient is x / y), r = func_08B4A198(quotient); then r is mirrored as pi/2 - r (bit 0), pi - r (bit 1) and negated
// (bit 2). Result in f0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): both magnitudes below the smallest normal (0x00800000): the answer is 0.0.
void func_08B4A430(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    float a = ctx.fpr[12], b = ctx.fpr[13];
    const float tiny = fbits(0x00800000u);
    GuestFrame frame(ctx, 0x10u);
    if (!(tiny <= fabs_bits(b)) && !(tiny <= fabs_bits(a))) {
        ctx.fpr[0] = 0.0f;
        return;
    }
    std::uint32_t sign = 0u;
    if (a < 0.0f) {
        sign = 4u;
        a = -a;
    }
    if (b < 0.0f) {
        b = -b;
        sign |= 2u;
    }
    float quotient;
    if (a < b) {
        quotient = a / b;
    } else {
        quotient = b / a;
        sign |= 1u;
    }
    float f1 = call_keep(rt, ctx, 0x08B4A198u, {}, {quotient}).f0;
    if ((sign & 1u) != 0u) f1 = fbits(0x3FC90FDBu) - f1;
    if ((sign & 2u) != 0u) f1 = fbits(0x40490FDBu) - f1;
    if ((sign & 4u) != 0u) f1 = -f1;
    ctx.fpr[0] = f1;
}

// func_08B4E360(out = a0, fov = f12, aspect = f13, near = f14, far = f15): the perspective projection matrix: t = func_08B49F68(fov / 2) * near, the corner values of the frustum (t, -t, t * aspect...) as the asm
// computes them, written to out in the order of the original (the fourth column is 0, 0, -1, 0; the unused words 0). The temporaries keep the asm's register names.
// Branches: none, the function is straight-line.
void func_08B4E360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    float f12 = psp_mul(ctx.fpr[12], fbits(0x3F000000u));
    float f22 = ctx.fpr[13], f21 = ctx.fpr[14], f20 = ctx.fpr[15];
    GuestFrame frame(ctx, 0x20u);
    float f0 = call_keep(rt, ctx, 0x08B49F68u, {}, {f12}).f0;
    f0 = psp_mul(f0, f21);
    float f3 = 1.0f;
    float f5 = f20 - f21;
    float f2 = fbits(0xC0000000u);
    float f1 = -f0;
    const float f7 = psp_mul(f0, f22);
    f5 = f3 / f5;
    f2 = psp_mul(f20, f2);
    f22 = psp_mul(f1, f22);
    float f4 = f0 - f1;
    f20 = f21 + f20;
    f0 = f0 + f1;
    const float f6 = f7 - f22;
    f4 = f3 / f4;
    f2 = psp_mul(f21, f2);
    f20 = -f20;
    f3 = f3 / f6;
    f21 = f21 + f21;
    f22 = f22 + f7;
    f1 = fbits(0xBF800000u);
    put_float(rt, out + 0x2Cu, f1);
    put(rt, out + 0x3Cu, 0u);
    put(rt, out + 0x10u, 0u);
    put(rt, out + 0x30u, 0u);
    put(rt, out + 4u, 0u);
    f2 = psp_mul(f5, f2);
    f5 = psp_mul(f5, f20);
    put(rt, out + 0x34u, 0u);
    put_float(rt, out + 0x28u, f5);
    f0 = psp_mul(f4, f0);
    f4 = psp_mul(f4, f21);
    put_float(rt, out + 0x38u, f2);
    f22 = psp_mul(f3, f22);
    f3 = psp_mul(f3, f21);
    put_float(rt, out + 0x14u, f4);
    put_float(rt, out, f3);
    put_float(rt, out + 0x20u, f22);
    put_float(rt, out + 0x24u, f0);
    put(rt, out + 8u, 0u);
    put(rt, out + 0x18u, 0u);
    put(rt, out + 0xCu, 0u);
    put(rt, out + 0x1Cu, 0u);
}

// func_08B58D08(a0, a1, a2, a3): the graphics library records a draw buffer / region: it appends five command words to the display list (pointer kGuState + 0x5C: 0x9C000000 | a0 & 0xFFFFFF, 0x9D000000 | a0 >> 24 << 16 |
// (a2 + 0x3F) & ~0x3F & 0x7FF, 0xD2000000 | a1 & 3, 0x15000000, 0x16000000 | (a3 - 1) << 10 | (a2 - 1), each field masked as the `ins` instructions do), keeps a0 / a1 / a2 / a3 / the rounded width
// in the state block (+0x9C, +0xA0, +0xB0, +0xB2, +0xB4), the scaled offsets (+0xAC, +0xAE), the viewport fields (+0x134, +0x136, +0x130, +0x132 and the floats at +0x14C..+0x160, the two constants at 0x08CB6200 /
// 0x08CB6204), sets bit 2 of the flag word (+0x6C) and ends with the tail call func_08B589A0(kGuState, flags). v0 = v1 = 0x08CB0000 (the `lui`s of the constant loads).
// Branches: none, the function is straight-line.
void func_08B58D08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a0 = ctx.gpr[4], a1 = ctx.gpr[5], a2 = ctx.gpr[6], a3 = ctx.gpr[7];
    const std::uint32_t s2 = sext16(a2), s1 = sext16(a3);
    const std::uint32_t list = word(rt, kGuState + 0x5Cu);
    std::uint32_t v1 = 0x9D000000u | (((a0 >> 24) & 0xFFu) << 16);
    std::uint32_t t6 = a2 + 0x3Fu;
    std::uint32_t t1 = 0x16000000u | (((a3 - 1u) & 0x3FFu) << 10);
    t6 &= ~0x3Fu;
    const std::uint32_t scaled_height = (0x1000u - s1) << 3, scaled_width = (0x1000u - s2) << 3;
    const std::uint32_t t2 = 0x9C000000u | (a0 & 0xFFFFFFu);
    v1 = (v1 & ~0x7FFu) | (t6 & 0x7FFu);
    const std::uint32_t t4 = 0xD2000000u | (a1 & 3u);
    t1 = (t1 & ~0x3FFu) | ((a2 - 1u) & 0x3FFu);
    const std::uint32_t t5 = 0x15000000u;
    store16(rt, kGuState + 0xACu, scaled_width);
    store16(rt, kGuState + 0xAEu, scaled_height);
    put(rt, kGuState + 0xA0u, a1);
    put(rt, kGuState + 0x9Cu, a0);
    store16(rt, kGuState + 0xB0u, a2);
    store16(rt, kGuState + 0xB2u, a3);
    store16(rt, kGuState + 0xB4u, t6);
    put(rt, list + 4u, v1);
    put(rt, list, t2);
    put(rt, list + 8u, t4);
    put(rt, list + 0xCu, t5);
    put(rt, list + 0x10u, t1);
    const std::uint32_t flags = word(rt, kGuFlags) | 4u;
    const float f0 = float_at(rt, 0x08CB6200u), f1 = float_at(rt, 0x08CB6204u);
    put(rt, kGuListPointer, list + 0x14u);
    put_float(rt, kGuState + 0x150u, f0);
    put_float(rt, kGuState + 0x160u, f1);
    put(rt, kGuFlags, flags);
    store16(rt, kGuState + 0x134u, s2);
    store16(rt, kGuState + 0x136u, s1);
    put_float(rt, kGuState + 0x14Cu, f0);
    put(rt, kGuState + 0x154u, 0u);
    put(rt, kGuState + 0x158u, 0u);
    put_float(rt, kGuState + 0x15Cu, f1);
    store16(rt, kGuState + 0x130u, 0u);
    store16(rt, kGuState + 0x132u, 0u);
    ctx.gpr[2] = 0x08CB0000u;
    ctx.gpr[3] = 0x08CB0000u;
    tail_call(ctx, 0x08B589A0u, {kGuState, flags});
}

// func_08B58F84(): the graphics library starts a new display-list node: when a previous node (state + 0x2D0) exists its word +0xC gets the list pointer and the list pointer moves on by 8; then a 16-byte node
// {0, previous, next, 0} is taken from the list (pointer += 0x10) and becomes the current node, the list gets the command words 0x13000000, 0x10000000, 0x3C000000 and the block of twelve words of 0x3D...
// (0x3D3F8000 in three of them), the flag word (+0x6C) becomes 0xFFF, bits 0-2 of the halfword at +0xFF8 are set, +0x44 / +0x48 = 0x3FFFF and the words +0x38, +0x3C, +0x12D0..+0x12DC are cleared.
// v0 = the new node, v1 = the new halfword.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a previous node (the word at state + 0x2D0 non-zero: its +0xC gets the list pointer, which moves on by 8).
void func_08B58F84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t t3 = 0x3D3F8000u;
    if (word(rt, kGuState + 0x2D0u) != 0u) {
        const std::uint32_t list = word(rt, kGuListPointer);
        put(rt, word(rt, kGuState + 0x2D0u) + 0xCu, list);
        put(rt, kGuListPointer, list + 8u);
    }
    const std::uint32_t node = word(rt, kGuListPointer);
    const std::uint32_t after = node + 0x10u;
    put(rt, kGuListPointer, after);
    put(rt, node, 0u);
    const std::uint32_t previous = word(rt, kGuState + 0x2D0u);
    put(rt, node + 8u, after);
    put(rt, node + 4u, previous);
    put(rt, node + 0xCu, 0u);
    const std::uint32_t block = word(rt, kGuListPointer);
    put(rt, kGuState + 0x2D0u, node);
    const std::uint32_t words = block + 8u;
    put(rt, block, 0x13000000u);
    put(rt, block + 4u, 0x10000000u);
    put(rt, kGuState + 0x38u, 0u);
    put(rt, kGuState + 0x3Cu, 0u);
    put(rt, block + 8u, 0x3C000000u);
    for (const std::uint32_t offset : {8u, 0x10u, 0x18u, 0x20u, 0x28u}) put(rt, words + offset, 0x3D000000u);
    put(rt, words + 0x30u, 0x3D000000u);
    put(rt, words + 4u, t3);
    put(rt, words + 0xCu, 0x3D000000u);
    put(rt, words + 0x14u, t3);
    put(rt, words + 0x1Cu, 0x3D000000u);
    put(rt, words + 0x24u, t3);
    put(rt, words + 0x2Cu, 0x3D000000u);
    put(rt, kGuFlags, 0xFFFu);
    const std::uint32_t halfword = rt.memory().load16(kGuState + 0xFF8u) | 7u;
    put(rt, kGuListPointer, block + 0x3Cu);
    put(rt, kGuState + 0x44u, 0x3FFFFu);
    put(rt, kGuState + 0x48u, 0x3FFFFu);
    store16(rt, kGuState + 0xFF8u, halfword);
    put(rt, kGuState + 0x12DCu, 0u);
    put(rt, kGuState + 0x12D0u, 0u);
    put(rt, kGuState + 0x12D4u, 0u);
    put(rt, kGuState + 0x12D8u, 0u);
    ctx.gpr[2] = node;
    ctx.gpr[3] = halfword;
}

// func_08B7B8C4(a, b): strcasecmp. The ctype table at 0x08C96BD9 (bit 0: upper case) folds a byte c to c + 0x20; the loop indexes the table with the byte sign-extended (the asm's `seb`), the final
// difference with the unsigned bytes. Walks both strings while the folded bytes are equal and the first one is not 0; v0 = fold(a[i]) - fold(b[i]) of the last pair read.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): an empty first string (the `beqz` to the tail that reads only the second string's first byte).
void func_08B7B8C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t table = 0x08C96BD9u;
    std::uint32_t first = ctx.gpr[4], second = ctx.gpr[5];
    std::uint32_t c1 = rt.memory().load8(first), c2;
    const auto fold_signed = [&](std::uint32_t byte) {
        const std::uint32_t extended = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(byte)));
        return (rt.memory().load8(table + extended) & 1u) != 0u ? extended + 0x20u : extended;
    };
    if (static_cast<std::int8_t>(c1) == 0) {
        c2 = rt.memory().load8(second);
    } else {
        for (;;) {
            c2 = rt.memory().load8(second);
            const std::uint32_t folded1 = fold_signed(c1), folded2 = fold_signed(c2);
            ++first;
            ++second;
            if (folded1 != folded2) break;
            c1 = rt.memory().load8(first);
            if (static_cast<std::int8_t>(c1) == 0) {
                c2 = rt.memory().load8(second);
                break;
            }
        }
    }
    const auto fold = [&](std::uint32_t byte) { return (rt.memory().load8(table + byte) & 1u) != 0u ? byte + 0x20u : byte; };
    ctx.gpr[2] = fold(c1) - fold(c2);
}

// func_08B7BA50(s, c): strchr: the address of the first byte equal to the low byte of c (the terminating 0 counts), or 0. The asm scans a word at a time when s is aligned; the result is the same as a byte scan.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a match (`beq a3,v1` to the return of the pointer); the compared cases run to the terminating 0 and answer 0.
void func_08B7BA50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t wanted = ctx.gpr[5] & 0xFFu;
    std::uint32_t address = ctx.gpr[4];
    for (;;) {
        const std::uint32_t byte = rt.memory().load8(address);
        if (byte == wanted) {
            ctx.gpr[2] = address;
            return;
        }
        if (byte == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        ++address;
    }
}


// func_08B662C4(x): a second range-reduced wrapper, by quadrant. |x| (bit pattern) up to 0x3F490FD8 (pi / 4): func_08B6D564(x, 0); a non-finite x answers x - x; otherwise func_08B6BAFC(&y) reduces x into
// y0, y1 and the quadrant n (v0); n & 3 selects func_08B6D564(y0, y1) (0), -func_08B6DEF8(1, y0, y1) (1), -func_08B6D564(y0, y1) (2) or func_08B6DEF8(1, y0, y1) (3). Result in f0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a non-finite x (f0 = x - x) and the quadrants 0 and 3 (no negation: the fall-through into func_08B6D564 and the `bnez` into func_08B6DEF8). The compared cases have the quadrants 1 and 2 only.
void func_08B662C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12];
    const std::uint32_t magnitude = bits_of(x) & 0x7FFFFFFFu;
    GuestFrame frame(ctx, 0x20u);
    if (!(static_cast<std::int32_t>(0x3F490FD8u) < static_cast<std::int32_t>(magnitude))) {
        ctx.fpr[13] = 0.0f;
        ctx.fpr[0] = call_keep(rt, ctx, 0x08B6D564u, {}, {x, 0.0f}).f0;
        return;
    }
    if (static_cast<std::int32_t>(0x7F7FFFFFu) < static_cast<std::int32_t>(magnitude)) {
        ctx.fpr[0] = x - x;
        return;
    }
    const std::uint32_t quadrant = call_keep(rt, ctx, 0x08B6BAFCu, {frame.at(0u)}, {}).v0 & 3u;
    const float y0 = float_at(rt, frame.at(0u)), y1 = float_at(rt, frame.at(4u));
    ctx.fpr[12] = y0;
    ctx.fpr[13] = y1;
    if ((quadrant & 1u) != 0u) {
        float f0 = call_keep(rt, ctx, 0x08B6DEF8u, {1u}, {y0, y1}).f0;
        if (quadrant == 1u) f0 = -f0;
        ctx.fpr[0] = f0;
    } else {
        float f0 = call_keep(rt, ctx, 0x08B6D564u, {}, {y0, y1}).f0;
        if (quadrant == 2u) f0 = -f0;
        ctx.fpr[0] = f0;
    }
}

// func_08C46184(out = a0, quaternion = a1, translation = a2): the 4x4 matrix of the quaternion {x, y, z, w} with the translation {tx, ty, tz} (a2[0..2]) in the last row, written to out in the order of the
// original (the fourth column is 0, 0, 0, 1). The temporaries keep the asm's register names.
// Branches: none, the function is straight-line.
void func_08C46184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4], quaternion = ctx.gpr[5], translation = ctx.gpr[6];
    float f2 = float_at(rt, quaternion + 8u), f1 = float_at(rt, quaternion + 4u), f4 = float_at(rt, quaternion);
    float f6 = f2 + f2;
    const float f5 = f1 + f1;
    float f0 = f4 + f4;
    float f3 = float_at(rt, quaternion + 0xCu);
    const float f13 = psp_mul(f2, f6);
    const float f11 = psp_mul(f1, f5);
    f4 = psp_mul(f4, f0);
    const float f8 = 1.0f;
    const float f12 = psp_mul(f3, f0);
    f6 = psp_mul(f3, f6);
    f1 = psp_mul(f1, f0);
    f3 = psp_mul(f3, f5);
    f0 = psp_mul(f2, f0);
    float f10 = f8 - f4;
    f2 = psp_mul(f2, f5);
    float f9 = f8 - f13;
    float f7 = f8 - f11;
    const float f15 = f0 - f3;
    const float f14 = f1 + f6;
    const float f16 = f12 + f2;
    f9 = f9 - f4;
    f0 = f0 + f3;
    f10 = f10 - f11;
    f7 = f7 - f13;
    f1 = f1 - f6;
    f2 = f2 - f12;
    const float tz = float_at(rt, translation + 8u), tx = float_at(rt, translation), ty = float_at(rt, translation + 4u);
    put_float(rt, out + 0x3Cu, f8);
    put_float(rt, out, f7);
    put_float(rt, out + 0x10u, f1);
    put_float(rt, out + 0x20u, f0);
    put_float(rt, out + 0x30u, tx);
    put_float(rt, out + 4u, f14);
    put_float(rt, out + 0x14u, f9);
    put_float(rt, out + 0x24u, f2);
    put_float(rt, out + 0x34u, ty);
    put_float(rt, out + 8u, f15);
    put_float(rt, out + 0x18u, f16);
    put_float(rt, out + 0x28u, f10);
    put_float(rt, out + 0x38u, tz);
    put(rt, out + 0xCu, 0u);
    put(rt, out + 0x1Cu, 0u);
    put(rt, out + 0x2Cu, 0u);
}

// func_08C34770(map, key): the lower bound of a key in a sorted tree (nodes: left +8, right +0xC, string +0x10, number +0x14; the header at map + 4, the root at map + 8; the key's number at key[1] and its
// string at key[0], the strings are 1-byte-character strings with their length at data - 0xC). Nodes with a smaller number go right, with a bigger one left (and become the answer); equal numbers compare the
// strings with func_08B36170 (memcmp of the shorter length, a tie broken by the length difference): a negative answer goes right, else left. v0 = the last node that went left, or the header.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): the key's string shorter than the node's (`movz` keeps the key length), a non-zero memcmp answer (it replaces the length difference) and a negative order (go right). The compared cases only see equal strings or a longer key.
void func_08C34770(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t map = ctx.gpr[4], key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t node = word(rt, map + 8u);
    std::uint32_t best = map + 4u;
    if (node != 0u) {
        const std::uint32_t number = word(rt, key + 4u);
        do {
            const std::uint32_t node_number = word(rt, node + 0x14u);
            std::uint32_t next;
            if (node_number < number) {
                next = word(rt, node + 0xCu);
            } else if (number < node_number) {
                next = word(rt, node + 8u);
                best = node;
            } else {
                const std::uint32_t node_chars = word(rt, node + 0x10u);
                const std::uint32_t node_length = word(rt, node_chars - 0xCu);
                const std::uint32_t key_chars = word(rt, key);
                put(rt, frame.at(0u), node_length);
                const std::uint32_t key_length = word(rt, key_chars - 0xCu);
                put(rt, frame.at(4u), key_length);
                const std::uint32_t shorter = key_length < node_length ? key_length : node_length;
                std::int32_t order = static_cast<std::int32_t>(node_length - key_length);
                const std::uint32_t compared = call_keep(rt, ctx, 0x08B36170u, {word(rt, node + 0x10u), key_chars, shorter});
                if (compared != 0u) order = static_cast<std::int32_t>(compared);
                if (order < 0) {
                    next = word(rt, node + 0xCu);
                } else {
                    next = word(rt, node + 8u);
                    best = node;
                }
            }
            node = next;
        } while (node != 0u);
    }
    ctx.gpr[2] = best;
}

// func_08B84A2C(x = f12, y = f13): pow(x, y). y == 0 answers the constant at 0x08CB6250 (1.0); x == 0 answers 0; r = |x|, and unless that is the constant, r = func_08B84954(y * func_08B846C0(r)) (exp of y log r);
// a non-negative x answers r. A negative x needs an integral y (ceil(y) == floor(y), as integers): then the result is -r for an odd trunc(y) and r otherwise; any other y answers the NaN 0xFF80FFFF. Result in f0.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): y == 0 (answers the constant), a non-negative x (answers r), and a negative x (the NaN for a non-integral y; for an integral y the sign comes from the parity of trunc(y)).
void func_08B84A2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float x = ctx.fpr[12], y = ctx.fpr[13];
    GuestFrame frame(ctx, 0x20u);
    if (y == 0.0f) {
        ctx.fpr[0] = float_at(rt, 0x08CB6250u);
        return;
    }
    if (x == 0.0f) {
        ctx.fpr[0] = 0.0f;
        return;
    }
    float r = fabs_bits(x);
    if (!(r == float_at(rt, 0x08CB6250u))) {
        const float logarithm = call_keep(rt, ctx, 0x08B846C0u, {}, {r}).f0;
        r = call_keep(rt, ctx, 0x08B84954u, {}, {psp_mul(y, logarithm)}).f0;
    }
    if (!(x < 0.0f)) {
        ctx.fpr[0] = r;
        return;
    }
    if (ctx.fpu_float_to_word_ct<2u>(y) != ctx.fpu_float_to_word_ct<3u>(y)) {
        ctx.fpr[0] = fbits(0xFF80FFFFu);
        return;
    }
    if ((ctx.fpu_float_to_word_ct<1u>(y) & 1u) != 0u) r = -r;
    ctx.fpr[0] = r;
}

// func_08B8ED98(this): two reference-counted objects, this[3] then this[1]; for each: the strong count at +4 is decremented (ll / sc, one thread: a plain load and store), and when it was 1 the virtual slot 8
// of the object runs, then the weak count at +8 is decremented and, when it was 1, the virtual slot 0xC runs (for this[1] as the tail call). A null object does nothing. v0 as the instructions leave it.
// Branches the routes do not take (checked against the asm by hand, reading the branch TARGETS): a strong count of the first object that is 1 (the slot-8 call, then the weak count and the slot-0xC call) and, for the second, a strong count of 1 (slot 8, the weak count and the final jump through slot 0xC). The compared cases leave at the first count test of each.
void func_08B8ED98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t object;
    {
        GuestFrame frame(ctx, 0x10u);
        object = word(rt, self + 0xCu);
        if (object != 0u) {
            std::uint32_t old = word(rt, object + 4u);
            put(rt, object + 4u, old - 1u);
            ctx.gpr[2] = 1u;
            if (old == 1u) {
                (void)call_keep(rt, ctx, word(rt, word(rt, object) + 8u), {object});
                old = word(rt, object + 8u);
                put(rt, object + 8u, old - 1u);
                ctx.gpr[2] = 1u;
                if (old == 1u) (void)call_keep(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
            }
        }
        object = word(rt, self + 4u);
        if (object == 0u) return;
        std::uint32_t old = word(rt, object + 4u);
        put(rt, object + 4u, old - 1u);
        ctx.gpr[2] = 1u;
        if (old != 1u) return;
        (void)call_keep(rt, ctx, word(rt, word(rt, object) + 8u), {object});
        old = word(rt, object + 8u);
        put(rt, object + 8u, old - 1u);
        ctx.gpr[2] = 1u;
        if (old != 1u) return;
    }
    const std::uint32_t vtable = word(rt, object);
    const std::uint32_t function = word(rt, vtable + 0xCu);
    ctx.gpr[2] = vtable;
    ctx.gpr[25] = function;
    tail_call(ctx, function, {object});
}


}  // namespace
}  // namespace hot_batch_81
}  // namespace gt::game

#define HOT81(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_81::name, gt::ReturnKind::kind)
HOT81(0x08B54FB8u, func_08B54FB8, Word);
HOT81(0x08B864ECu, func_08B864EC, Word);
HOT81(0x08B59508u, func_08B59508, Word);
HOT81(0x08C436F4u, func_08C436F4, Word);
HOT81(0x089E0E68u, func_089E0E68, Word);
HOT81(0x08BEFC38u, func_08BEFC38, Word);
HOT81(0x08BAA778u, func_08BAA778, Word);
HOT81(0x08BE6154u, func_08BE6154, Word);
HOT81(0x08B499B8u, func_08B499B8, Float);
HOT81(0x08B49CC8u, func_08B49CC8, Float);
HOT81(0x08B6DEF8u, func_08B6DEF8, Float);
HOT81(0x08B847A8u, func_08B847A8, Float);
HOT81(0x08B84954u, func_08B84954, Float);
HOT81(0x08B4B114u, func_08B4B114, Word);
HOT81(0x08B4C804u, func_08B4C804, Word);
HOT81(0x08B4CBDCu, func_08B4CBDC, Word);
// Not registered (no compared case changed memory: the packed word was already what the call stores, so the gate cannot see its store): HOT81(0x088FA224u, func_088FA224, Word);
HOT81(0x088CDD7Cu, func_088CDD7C, Word);
HOT81(0x088A84B0u, func_088A84B0, Word);
HOT81(0x088A9D3Cu, func_088A9D3C, Word);
// Not registered (no compared case changed memory: every compared case saw a null object, so the gate cannot see its stores): HOT81(0x088AA074u, func_088AA074, Word);
HOT81(0x08BAA56Cu, func_08BAA56C, DoubleWord);
// Not registered (no compared case changed memory: the record's words were already zero, so the gate cannot see its moves): HOT81(0x08AF2D20u, func_08AF2D20, Word);
HOT81(0x089ABF68u, func_089ABF68, Word);
HOT81(0x08B86674u, func_08B86674, Word);
HOT81(0x08949E8Cu, func_08949E8C, Word);
HOT81(0x08B4F5E0u, func_08B4F5E0, Word);
HOT81(0x08C52868u, func_08C52868, Word);
HOT81(0x08904A04u, func_08904A04, Word);
HOT81(0x08C527C0u, func_08C527C0, Word);
HOT81(0x08AAD700u, func_08AAD700, Word);
HOT81(0x08AF2238u, func_08AF2238, Word);
HOT81(0x08C2273Cu, func_08C2273C, Word);
HOT81(0x08C21340u, func_08C21340, Word);
HOT81(0x08881F18u, func_08881F18, Word);
HOT81(0x088CDC48u, func_088CDC48, Word);
HOT81(0x08C21118u, func_08C21118, Word);
HOT81(0x08B66604u, func_08B66604, Float);
HOT81(0x08C137D4u, func_08C137D4, Word);
HOT81(0x08AFEB98u, func_08AFEB98, Word);
HOT81(0x08C1FB10u, func_08C1FB10, Word);
HOT81(0x08B5547Cu, func_08B5547C, Word);
HOT81(0x08BB0C98u, func_08BB0C98, DoubleWord);
HOT81(0x08907AA0u, func_08907AA0, Word);
HOT81(0x088CBFD4u, func_088CBFD4, Word);
HOT81(0x08BB1F48u, func_08BB1F48, Word);
HOT81(0x08C34904u, func_08C34904, Word);
HOT81(0x08B4A430u, func_08B4A430, Float);
HOT81(0x08B4E360u, func_08B4E360, Word);
HOT81(0x08B58D08u, func_08B58D08, Word);
HOT81(0x08B58F84u, func_08B58F84, Word);
HOT81(0x08B7B8C4u, func_08B7B8C4, Word);
HOT81(0x08B7BA50u, func_08B7BA50, Word);
HOT81(0x08B662C4u, func_08B662C4, Float);
HOT81(0x08C46184u, func_08C46184, Word);
HOT81(0x08C34770u, func_08C34770, Word);
HOT81(0x08B84A2Cu, func_08B84A2C, Float);
HOT81(0x08B8ED98u, func_08B8ED98, Word);
