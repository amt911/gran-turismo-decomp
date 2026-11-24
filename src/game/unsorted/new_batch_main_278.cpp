// ---- from main-session batch 199 (draft a22f6841914861b3873e964780c872f307d1c1f4) ----
// Main-session batch 199 (draft, UNVERIFIED): 440-460 byte functions written by hand from `tools.disasm` as structured C.
// Delay slots are read as "before the branch / call" (a value stored there is the one from before the call), `v0` ends as
// the asm leaves it, and every callee-saved register the original keeps a value in is set where the asm sets it. No names:
// none has evidence beyond what its comment says.
//
// Skipped on purpose:
//   0x08B57F2C - a VFPU 4x4 matrix inverse (lv.q / wm32-style prefix tricks, vtfm4, vrcp, vmscl on quaternion pairs). Its
//                intermediate VFPU layouts are what the vfpu helpers cannot model yet (same reason as func_08B54104 in
//                new_batch_16.cpp); a portable rewrite would be a guess.
//   0x08B63AF8 - ends without an epilogue: every path ends in func_08B6250C, a noreturn helper (`jal` and no `jr $ra`
//                after it), so the function never returns - a throw-like shape, which the batch rules skip.
//   0x08BF9F3C - the entry block's `bne $a2, $a3` jumps *backwards* to 0x08BF9ED4, i.e. into the body of func_08BF9ECC
//                (itself an 8-byte thunk at 0x08BF9ECC that jumps to 0x08BF9F3C). The function list disagrees with the code
//                there (216 bytes from 0x08BF9F3C runs past the `jr $ra` at 0x08BF9FA4 and into the next function at
//                0x08BF9FAC), so which unit owns the block is unknown and the recompiled original may not even contain it.
#include "decomp_registry.hpp"
#include "guest_call.hpp"
#include "gt/game/hot_common.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_199 {
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
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next
// instruction sees.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                             std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// A virtual call `object->slot(...)`: `lw v1, 0(object); lw v0, slot(v1); jalr v0`, both registers set like the original.
inline u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }

// s0-s7 and $fp are callee-saved: the body sets them like the original and the caller's values come back.
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

// The `ll/sc` increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
inline void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}
// The `ll/sc` decrement: leaves a0 = the old value, v1 = the address and v0 = 1 (the successful `sc`); true when the old
// value was 1 (the count reached zero).
inline bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}
// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot 8 runs,
// then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for both).
inline void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
}
// Releases a libstdc++ copy-on-write string rep at `text` (its rep is 0xC before the data, the count at -4): nothing for
// the empty rep 0x08D75298, else func_08B60804(&count, -1) and, when the old count was not positive, func_08B93554(rep,
// frame). v0 = the empty rep / the count step / the free call's result.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 text, u32 frame_base) {
    const u32 rep = text - 0xCu;
    ctx.gpr[2] = 0x08D75298u;
    if (rep == 0x08D75298u) return;
    if (signed_of(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, 0x08B93554u, {rep, frame_base});
}

// --- the graphics library's state block set up (no arguments) ----------------------------------------------------------
// 0x08B5A344: a flag at 0x08D6E0F0 and the block at 0x08D6CE00 cleared with 0x08B36018, the fields 0x400/density/table of
// the display list filled, a loop writing 0xC0..0xCB as bytes 0xC0..0xCB of 0x08D6D048 (an index table at +0x3C of each
// 4-byte word), four more commands (0x08C7AC00, 0x08C7AC40, 0x08C7AC30, 0x08C7AC50, 0x08C7AAB0, 0x08B84B5C, 0x08C7AC38),
// and two buffers handed to 0x08B56B70 and 0x08B59C90 with the second one's word +0x2C rounded up to a multiple of 0x2000.

// --- two sorted-list inserts of one 0x14-byte node (0x08BD0978, 0x08BE191C) --------------------------------------------
// The list hangs at owner + 0x98 (a sentinel node whose word +0 is the first one); every node's word +8 points 0xC bytes
// before its key. The arguments reach the compare helper func_08B36170 as (node's payload, the word the caller's a1 points
// at, the larger of the two keys - `movz v0, sp, v1` picks the slot holding the key when the caller's is smaller). A node
// whose byte +0x14 is set gets a new 0x14-byte node from func_08B62344 with the vtable below, holding a3, a2's second word
// and a2's first word, appended at node + 0x18; otherwise the function pointer in a2's first word (offset by a2's second
// word through the table at a3, when that word is odd) is called with a3 + (a2's second word >> 1). func_08BBCFA0(owner)
// runs last and v0 = 1 when something was inserted or called.
void insert_into_sorted_list(Runtime &rt, AllegrexContext &ctx, u32 vtable) {
    const u32 self = ctx.gpr[4];
    const u32 key_source = ctx.gpr[5];
    const u32 description = ctx.gpr[6];
    const u32 argument = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 first_word = word(rt, description);
    const u32 second_word = word(rt, description + 4u);  // the beqz's delay slot: read before the branch
    u32 inserted = 0u;
    ctx.gpr[22] = self;
    ctx.gpr[17] = key_source;
    ctx.gpr[30] = argument;
    ctx.gpr[16] = 0u;
    if (word(rt, self) == 0u) {
        ctx.gpr[2] = inserted;
        return;
    }
    call_set(rt, ctx, 0x08B34888u, {word(rt, self) + 0x6Cu});
    const u32 owner = word(rt, self);
    const u32 sentinel = owner + 0x98u;
    u32 node = word(rt, sentinel);
    u32 current = sentinel;
    if (node != sentinel) {
        const u32 key_owner = word(rt, key_source);     // s4, read once before the loop
        const u32 wanted = word(rt, key_owner - 0xCu);  // s0
        for (;;) {
            current = node;
            const u32 payload = word(rt, node + 8u);
            const u32 key = word(rt, payload - 0xCu);
            const u32 larger = (wanted < key) ? key : wanted;  // `movz v0, sp, v1`
            // a3..t3 are the caller's registers, passed on as they are (the callee copies t3 only to compare it with itself)
            const u32 answer = call_set(rt, ctx, 0x08B36170u, {payload, key_owner, larger, ctx.gpr[7], ctx.gpr[8],
                                                              ctx.gpr[9], ctx.gpr[10], ctx.gpr[11]});
            ctx.gpr[16] = wanted;
            if (answer != 0u || key != wanted) {
                const u32 next = word(rt, current);  // the bnel's delay slot
                current = next;
                if (next == sentinel) break;
                node = next;
                continue;
            }
            break;
        }
    }
    if (current != sentinel) {
        if (byte_u(rt, current + 0x14u) != 0u) {
            const u32 item = call_set(rt, ctx, 0x08B62344u, {0x14u});
            put(rt, item, vtable);
            put(rt, item + 8u, argument);
            put(rt, item + 0x10u, second_word);
            put(rt, item + 4u, 0u);
            put(rt, item + 0xCu, first_word);
            const u32 list = current + 0x18u;
            const u32 last = word(rt, list + 4u);
            if (last != 0u) {
                put(rt, list + 4u, item);
                put(rt, last + 4u, item);
            } else {
                put(rt, list, item);
            }
            put(rt, item + 4u, 0u);
            inserted = 1u;
            put(rt, list + 8u, word(rt, list + 8u) + 1u);
        } else {
            const u32 position = argument + static_cast<u32>(signed_of(second_word) >> 1);  // `sra`, not `srl`
            u32 target = first_word;
            if ((second_word & 1u) != 0u) target = word(rt, word(rt, position) + first_word);
            inserted = 1u;
            call_indirect(rt, ctx, target, {position});
        }
    }
    call_set(rt, ctx, 0x08BBCFA0u, {self});
    ctx.gpr[2] = inserted;
}
void func_08BD0978(Runtime &rt, AllegrexContext &ctx) { insert_into_sorted_list(rt, ctx, 0x08CC53F8u); }
void func_08BE191C(Runtime &rt, AllegrexContext &ctx) { insert_into_sorted_list(rt, ctx, 0x08CC68C0u); }

// --- a pair of shared-pointer pairs assigned into self (0x0887B980) -------------------------------------------------------
// func_0887B980(self = a0, source = a1, second = a2): the caller's a1 / a2 go to self +0x2C / +0x30, a pair of
// shared-pointer pairs built by func_08963418 into a temporary (frame +8) is assigned to self +0/+4 and +8/+0xC (use count
// up, the old block released: use count, virtual slot 8, weak count and slot 0xC on the last), then the temporary is
// released (func_08B8ED98) and func_0887B40C(self, 0, 0) finishes. v0 = that last call's result.

// --- the display list's block set up (0x08ACC704) ------------------------------------------------------------------------
// func_08ACC704(self = a0, mode = a1, third = a2, fourth = a3, fifth = t0): under the lock self + 0x60 (func_08B34888,
// released by the tail jump to func_08B34B24) a block at self + 0x98 is set up: func_08ACC640(self), the word self + 0x94
// becomes -1, self + 0x90 zero, and the word self + 0x9C takes 0 for a word 0x08D69524 of 13 or more, else the table at
// 0x08C9070C (0, 1, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xA, 0xB). Then func_08B2ED74(9, block + 8), block[0] = 0x2C4,
// func_08B2E314(5, 1), (1, 2), (1, 1), (6, 0) into block + 0xC, +0x10, +0x14, +0x18, block + 0x23C = third, block + 0x38 =
// fourth, block + 0x34 = mode; for mode 1, func_08B7C830(block + 0x3C, fifth, 0x200) and block's byte +0x23B cleared.
// Bytes +0x284 and +0x244, word +0x240 cleared, then the self virtual slot 0x1C runs with self and byte self + 0x8C
// becomes 1. v0 = that virtual call's, and the tail jump keeps it.

// --- a horizontal probe search (0x088BF1FC) -------------------------------------------------------------------------------
// func_088BF1FC(self = a0, mode = a1, untraced = a2, reference = a3, output = t0, f12 = a scale): at most ten probes (one
// when the scale is zero, ten from anything else - `bc1f`'s delay slot sets the count on both paths - with the scale
// clamped to 5 below that) of the object at self + 0x1C: its virtual slot 0x1C answers an object whose virtual slot 8
// fills the frame's position at sp + 0x28 (count 1 at sp, the mode, a2, self's word +0x18's byte +0x94 (1 for exactly 1)
// or 2 for mode 4, and the float func_088DF13C(self->+0xC, self->+0x10) points at). The horizontal distance from
// (reference[0], reference[1]) ends the loop at 25 (5 squared); the scale grows by 1 per probe that continues.
// output[0] = dx, output[1] = dy, f0 = the scale.

// --- a bit string decoded onto an object (0x08879190) ---------------------------------------------------------------------
// func_08879190(self = a0): decodes the bit string func_08AB856C builds from the two words at 0x08C8981C (with self and 2
// through func_08875658) onto self's target: func_0886B2F4(self[0]) -> func_0886A950(name 0x08CA008C) must both answer,
// then func_0886A9A0(self[0]'s object, 2) gives the target; the byte count at frame +0x14 must be 2 or more, then for
// every character up to frame +8 (the string is the word at frame +4 indexed by the running index >> 2) a hex digit,
// A-F or a-f is decoded (digit, a letter minus 0x37, lowercase minus 0x57) and its bit (index & 3) is passed to
// func_088852D0(target, string + index, bit); a zero character ends it, a negative word frame +4 calls
// func_088852C4(target, frame +8 != 0) instead. The temporary string at frame +0xC is released at the end.
void func_08879190(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[16] = frame.at(0xCu);
    ctx.gpr[17] = self;
    call_set(rt, ctx, 0x08AB85F0u, {frame.at(0xCu)});
    put(rt, frame.at(4u), word(rt, 0x08C8981Cu));
    put(rt, frame.at(8u), word(rt, 0x08C8981Cu + 4u));  // the jal's delay slot (`lw $t1, 0x4($v0)`)
    call_set(rt, ctx, 0x08875658u, {self, frame.at(4u), frame.at(0xCu), 2u});
    const u32 bits = call_set(rt, ctx, 0x08AB856Cu, {frame.at(0xCu)});
    ctx.gpr[18] = bits;
    if (signed_of(word(rt, frame.at(0x14u))) >= 2) {
        const u32 object = call_set(rt, ctx, 0x0886B2F4u, {word(rt, self)});
        if (object != 0u && call_set(rt, ctx, 0x0886A950u, {object, 0x08CA008Cu}) != 0u) {
            // func_0886A9A0 takes the object func_0886B2F4 answered and a 2 (the jal's delay slot), not the lookup's v0
            const u32 target = call_set(rt, ctx, 0x0886A9A0u, {object, 2u});
            ctx.gpr[17] = target;
            if (signed_of(word(rt, frame.at(4u))) < 0) {
                call_set(rt, ctx, 0x088852C4u, {target, word(rt, frame.at(8u)) != 0u ? 1u : 0u});
            } else if (signed_of(word(rt, frame.at(8u))) > 0) {
                u32 character = sext8(byte_u(rt, bits));
                u32 index = 0u;
                while (character != 0u) {
                    const u32 position = word(rt, frame.at(4u)) + index;  // the beqz's delay slot: the current index
                    const u32 digit = character - 0x30u;
                    const bool is_digit = digit < 0xAu;
                    const u32 lower = character - 0x61u;
                    const bool is_hex = is_digit || ((character - 0x41u) < 6u) || (lower < 6u);
                    if (is_hex) {
                        const u32 shift = lower < 0x1Au ? 0x20u : 0u;  // `movz v0, zero, v1` keeps 0x20 for a lowercase
                        const u32 value = is_digit ? digit : character - shift - 0x37u;
                        call_set(rt, ctx, 0x088852D0u, {target, position, (value >> (index & 3u)) & 1u});
                    }
                    ++index;
                    if (!(signed_of(index) < signed_of(word(rt, frame.at(8u))))) break;
                    character = sext8(byte_u(rt, bits + static_cast<u32>(signed_of(index) >> 2)));
                }
            }
        }
    }
    release_string(rt, ctx, word(rt, frame.at(0xCu)), frame.at(0u));
}

// --- a format-and-append loop over NUL-separated pieces (0x08C6F004) ------------------------------------------------------
// func_08C6F004(output = a0, source = a1, begin = a2, end = a3): formats the pieces of `source` (separated by NULs, from
// func_08C74968(slot, begin, end, slot2)) into `output`, which starts as the empty string 0x08D77498 + 0xC. Each pass
// formats one piece into a stack buffer of 2*(end-begin)+0x1E bytes (func_08B60890(source, buffer, piece, span)) - grown
// to the written count + 1 when that is short - and appends it (func_08C75AF8); a pass whose piece ends exactly at the
// pattern's end stops, otherwise the fill byte at 0x08CDBE28 is appended (growing through func_08C75840 when the capacity
// is short or the word -4 of the data is positive) at data + (length + 1) and the piece moves past its NUL
// (func_08B35F60). The temporary format at frame +4 is released at the end. v0 = output.

// --- the length/byte step of a string replace (0x08C74B68) ---------------------------------------------------------------
// func_08C74B68(self = a0, position = a1, count = a2, inserted = a3): the new length is length + inserted - count; when the
// capacity is short or the word -4 of the data is positive, func_08C74398(new length, capacity, frame) reallocates and the
// head [0, position) and the tail [position + count, ..) (one byte inline, else func_08B79A88) move into the new block;
// otherwise, with inserted equal to count the tail already sits right, else it moves by (inserted - count) bytes
// (func_08B79BAC). The old rep is released (empty rep 0x08D77498) and the new length and the terminator byte at
// 0x08CDBE28 are stored. v0 = data + new length.

// --- a constructor of two shared-pointer pairs and a 0x3C-byte member (0x0889F010) ---------------------------------------
// func_0889F010(self = a0, first = a1, source_a = a2, source_b = a3, second = t0, third = t1): the vtable word 0x08CC2420,
// two {pointer, control block} pairs from source_a (+0/+4, +8/+0xC) at self +4/+8 and +0xC/+0x10, two more from source_b
// (+0/+4, +8/+0xC) at self +0x14/+0x18 and +0x1C/+0x20 (each block's count at +4 incremented with `ll/sc`), then a
// 0x3C-byte object (func_08B62344, func_08B28D34, words +0x2C/+0x34/+0x38 and byte +0x30 cleared) built by
// func_0889C2C0(self + 0x28, first, source_a, source_b, t0 & 0xFF, self + 0x24) and kept at self + 0x24;
// func_08912E08(self + 0x30) runs and, unless t1 & 0xFF is set, the function ends in a tail jump to
// func_08911D80(self + 0x30, self + 0x24, source_a, first).

// --- a mode word mapped through two tables (0x08AB7754) ------------------------------------------------------------------
// No arguments. func_08B2ED74(8, sp) writes two words; func_08ABD974 -> func_08B35FD8(that, 0x08CB42D0) answers a flag and
// the first word picks a case from the 12-entry table at 0x08C904B4 (index 0 -> 0, 1 -> flag ? 1 : 2, 2..11 -> index + 1,
// 12 and up -> 0). func_08ABD5EC(sp + 8, 0xE) then fills up to 0xE ids (count in v0, the first at sp + 8): when the picked
// value is not one of the first count-1 following ids it falls back to the first one. func_08B49858(0x08D69524, value)
// runs, func_08B2ED74(9, sp + 4) writes one more word, the 13-entry table at 0x08C904E4 maps the value (0, 1, 1, 2, ... 0xB;
// 0 from 13 up) and func_08C7AC60(mapped, word(sp + 4)) answers v0.

} // namespace
} // namespace new_batch_main_199
} // namespace gt::game

#define MAIN199(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_199::name, gt::ReturnKind::kind)
MAIN199(0x08879190u, func_08879190, Word);
MAIN199(0x08BD0978u, func_08BD0978, Word);
MAIN199(0x08BE191Cu, func_08BE191C, Word);

// ---- from main-session batch 200 (draft 0c2af54dd0c1835c8459485170d907aa182e6760) ----
// New batch main 200: twelve functions of 116-124 instructions, written by hand from `tools/disasm` (the old drafts of
// 2026-11-20/21 where one existed, re-read against the asm and corrected). Every step is the original's, in the
// original's order: a delay slot runs before its branch / call (a branch-likely delay slot only when the branch is
// taken), `lui [HI]` immediates are the relocated high half of the printed `[LO]` address, callee-saved registers are
// set where the asm sets them and put back at the end, and v0 ends as the asm leaves it on every exit path. No names:
// none has evidence.
//
// The element-count idioms are the original's own arithmetic, not a rewrite: `(end - begin) >> 2` times 0x2FA0BE83
// (43's inverse modulo 2^32: 0xAC-byte records) and `(node - base) >> 4` times 0xEEEEEEEF (15's inverse: 0xF0-byte
// records) both take the low product word, so a non-multiple pointer difference gives the original's own answer.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_200 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose v0 / v1 / f0 stay in the registers like the original's.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
// A guest call with the argument registers written as the original writes them before its `jal` (a0-a3, t0-t3 in
// order); a register the call does not name keeps what an earlier step left there.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, target, arguments).v0;
}
// The same for a callee whose v1 the original reads afterwards.
GuestResult call_both(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    return call_keep(rt, ctx, target, arguments);
}
// A virtual call the way the compiler issues it: v1 = vtable (object[0]), v0 = the slot's function, then `jalr v0`.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, function, arguments);
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<s8>(value))); }
u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<s16>(value))); }
// The high half a relocated `lui` leaves in its register (a `lw` through it is the [LO] the disassembly prints): the
// value a callee that never writes v0 hands back when the original calls through another register.
u32 address_high(u32 address) { return (address + 0x8000u) & 0xFFFF0000u; }

// s0-s7 and fp are callee-saved: the body sets them where the original sets them and the caller's values come back.
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
// f20 is callee-saved too.
class SavedFloat20 {
public:
    explicit SavedFloat20(AllegrexContext &ctx) : ctx_(ctx), f20_(ctx.fpr[20]) {}
    ~SavedFloat20() { ctx_.fpr[20] = f20_; }
    SavedFloat20(const SavedFloat20 &) = delete;
    SavedFloat20 &operator=(const SavedFloat20 &) = delete;

private:
    AllegrexContext &ctx_;
    float f20_;
};

// --- 0x08A5159C ---------------------------------------------------------------------------------------------------
// (self = a0): under the lock 0x08CF6D34 (func_08B344E0 / func_08B3457C) byte self + 0x5C cleared, then the entries of
// the ring self + 0x10 (halfword each) / self + 0x28 (flag byte) are walked from index (word +0x24) down to that index
// plus one modulo the halfword count +0x1C inclusive: a flag byte whose bit 0 is clear and a non-zero entry halfword
// pick the object (word self, or word self + 4 when bits 1-2 of the flag are 3), which is asked through
// func_08C7ACB0(object, entry, frame + 0x18) and then gets func_08C7ACA8(object, entry again, a record built at frame
// +0: word +0x20 of the answer shifted 6, the byte +9 / byte +0xB difference shifted 6, the byte +8 / +9 / (+8 >> 1)
// halves, zero, and word self + 0xC plus index * halfword self + 0x1E). The flag byte then gets bit 0 and self + 0x24
// steps down (wrapping to count - 1). func_08B28C44(self + 0x30) follows the unlock; the whole round repeats while
// byte self + 0x5C stays clear. v0 = that byte (non-zero on the exit), v1 untouched.

// --- 0x088E6250 ---------------------------------------------------------------------------------------------------
// (self = a0): runs the state machine of the two objects word self and word self + 4 in turn. Per object: with the word
// at +0x50C equal to 1 the lock word at +0x508 is taken when it is zero (an `ll`/`sc` test-and-set), then on the word
// at +0x530: 0 stores 0 back; 1 asks the virtual at vtable +0x90 with 1 (only for the first object, and only when its
// word +0x534 is not 1) or 0; 2..5 ask the virtuals at vtable +0x94 / +0x98 / +0x9C / +0xA0 with no argument; 6 (and
// every value above 6) first calls func_088E60B4(self) and func_088EE660(object) when the first object's second word
// holds an object whose state is 6, and asks the virtual at vtable +0x8C if that answered non-zero. The answer goes
// back to +0x530: non-zero calls func_088ECAC8(object), zero clears the byte at +0x525. The lock word is then set back
// to zero when it reads 1. v0 = 2 on the normal exit.
void func_088E6250(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    u32 index = 0u;
    u32 object = word(rt, self);
    for (;;) {
        if (object == 0u) {
            ++index;
            if (index == 2u) break;
            object = word(rt, self + 4u);
            continue;
        }
        const u32 lock = object + 0x508u;
        if (word(rt, object + 0x50Cu) == 1u) {
            if (word(rt, lock) == 0u) put(rt, lock, 1u);  // the original's ll/sc test-and-set
            const u32 state = word(rt, object + 0x530u);
            u32 flag = 0u;
            u32 answer = 0u;
            if (state < 7u) {
                switch (state) {
                case 1u: {
                    const u32 table = word(rt, object);
                    u32 argument = 0u;
                    if (index == 0u && word(rt, object + 0x534u) != 1u) argument = 1u;
                    ctx.gpr[2] = table;  // v0 still holds the vtable when the `jalr` runs
                    ctx.gpr[3] = table;
                    answer = call_keep(rt, ctx, word(rt, table + 0x90u), {object, argument}).v0;
                    flag = 1u;
                    break;
                }
                case 2u: answer = call_virtual(rt, ctx, object, 0x94u, {object}).v0; flag = 1u; break;
                case 3u: answer = call_virtual(rt, ctx, object, 0x98u, {object}).v0; flag = 1u; break;
                case 4u: answer = call_virtual(rt, ctx, object, 0x9Cu, {object}).v0; flag = 1u; break;
                case 5u: answer = call_virtual(rt, ctx, object, 0xA0u, {object}).v0; flag = 1u; break;
                case 6u: {
                    bool ask = true;
                    if (index == 0u) {
                        const u32 second = word(rt, self + 4u);
                        if (second != 0u && word(rt, second + 0x530u) != 6u) {
                            ask = false;  // straight to the virtual below
                        } else {
                            call(rt, ctx, 0x088E60B4u, {self});
                            if (call(rt, ctx, 0x088EE660u, {object}) == 0u) {
                                flag = 1u;
                                ask = false;
                            }
                        }
                    }
                    if (ask) {
                        answer = call_virtual(rt, ctx, object, 0x8Cu, {object}).v0;
                        flag = 1u;
                    }
                    break;
                }
                default: break;
                }
            }
            put(rt, object + 0x530u, answer);  // the delay slot of the test below
            if (answer != 0u) {
                call(rt, ctx, 0x088ECAC8u, {object});
            } else if (flag != 0u) {
                put8(rt, object + 0x525u, 0u);  // the delay slot of the branch-likely, so only when the flag is set
            }
        }
        if (word(rt, lock) == 1u) put(rt, lock, 0u);
        ++index;
        if (index == 2u) break;
        object = word(rt, self + 4u);
    }
    ctx.gpr[2] = 2u;
}

// --- 0x089A9E58 ---------------------------------------------------------------------------------------------------
// (self = a0, pairs = a1, extra = a2, f12 = a scale): builds two point pairs and hands them to the virtual at vtable
// +0x10C of self. With a positive scale: the direction from pairs[0]/pairs[1] to pairs[2]/pairs[3] times the scale
// halved and divided by its length gives (step_x, step_y), and two steps along pairs (8 bytes each, two points per
// step: (x + step_y, y - step_x) then (x - step_y, y + step_x)) fill frame +0..+0x1F while extra's 16-byte records
// fill frame +0x20..+0x3F; the call gets (self, word 0x08C8CC14, 4, frame, extra ? frame + 0x20 : 0). Otherwise the
// frame gets extra's first and second records around pairs' two points and the call gets (self, word 0x08C8CC24, 2,
// frame + 0x20, extra ? frame : 0). v0 = that answer. The register spins before each block only spend time.

// --- 0x0888A2A8 ---------------------------------------------------------------------------------------------------
// (self = a0, other = a1): with other null, func_088BA... the pair of words self + 0x40 (begin / end of a vector of
// 0xAC-byte records) is emptied first: a non-empty one through func_08BAAD9C(begin, end), an empty one through
// func_08BAB0A0(end, 0, frame + 0xAC), and a local {vtable 0x08CC3528, 0} at frame +0x150 is turned into {0x08CC1AA8, 0}
// (the same local at frame +0xA4 is built on the way). func_088CD73C(self, other) then runs, other's virtual at vtable
// +8 answers a count: below the vector's size the records from begin + count * 0xAC to end move down through
// func_08BAAD9C, otherwise count - size records are appended through func_08BAB0A0(end, count - size, frame). Then for
// every record (begin + i * 0xAC): other's virtual at vtable +0x18 is asked, and a non-zero answer gets
// func_088CD9C8(record, that answer, other's virtual at vtable +0x3C). v0 = 0.

// --- 0x089C8818 ---------------------------------------------------------------------------------------------------
// (out = a0, first = a1, second = a2): *first / *second are std::string-like: the word before the data pointer holds
// the length. func_08B88F44(frame + 0xC, first) builds a temporary first. When *first is shorter than *second the
// empty representation 0x08D74FD0 takes first's place and the compare flag comes from the caller's s5, which this path
// never writes. Otherwise func_08B8D96C(frame + 8, first, len(*first) - len(*second), len(*second)) slices *first,
// func_08B36170(*second, that slice, the smaller length, that length's slot) compares them, and the slice is released
// (unless it is the empty representation 0x08D74FD0, and unless func_08B60804(slice - 4, -1) answers above zero:
// func_08B8542C(slice - 0xC, frame + 2)). func_08B89054(frame + 0xC, second) runs when the compare answered non-zero
// or the lengths were equal. Unless func_08AA43CC(frame + 0xC) answers non-zero: func_08A5CC40(out),
// func_08B89124(frame + 4, it, frame + 0xC), func_08B855E0(frame + 0xC, frame + 4) and the release of frame + 4.
// func_08AA58D4(out, frame + 0xC) and the release of frame + 0xC finish. v0 = out, except on the last release path,
// whose call's answer is what the asm leaves in v0.

// --- 0x08B0029C ---------------------------------------------------------------------------------------------------
// (self = a0, record = a1, pair = a2, quad = a3, triple = t0, triple_b = t1, flag_a = t2, flag_b = t3, then the
// caller's stack words at +0, +4, +8): quad's four floats go to self + 0..+0xC, pair's three to self + 0x20, triple's
// three to self + 0x38 (three zeros when triple is null), self + 0x80 / +0x8C are zeroed and the halfword +0xE2 gets
// flag_a | (flag_b ? 0x10 : 0) | (stack byte +0 ? 0x20 : 0) | (stack byte +8 ? 0 : 0x80); +0xE0 gets the stack word +4's
// low half, +0xEA / +0xEC / +0xD0 become -1, +0xD8 the record, +0xD4 / +0xDC zero. func_08BAE278(self, frame + 0x14:
// the record's +0x24, +0x28, +0x2C) forwards (self, frame + 0x14), and its v0 / v1 pairs go to frame +0 / +4 and
// frame +0x30 / +0x34 before func_08BAC284(frame + 0x20, v0, v1); func_08C46508(self + 0x20, frame + 0x20), +0xE6
// becomes 0x7FFF, func_08AFEA88(self), func_08AFEB98(self, frame + 8: triple_b's three floats or zeros),
// func_08B055EC(self + 0xB0) and func_08AFFCF4(self) follow. v0 = its answer.

// --- 0x08C48CDC ---------------------------------------------------------------------------------------------------
// (self = a0, first = a1, second = a2): the iterator records *first (frame + 0x10: its words at +0, +4, +8, +0xC) and
// *second (frame + 0: the same) are copied to the frame and nothing else happens when first[2] equals second[2]. With
// self + 0xC set, its entry's index ((first[2] - word(word(self + 4))) >> 4 times 0xEEEEEEEF, truncated to a halfword)
// goes to func_08C46DAC(self + 0xC, index, self, second[1]). func_08C46D74(first[2], frame + 0x10, first[2]) then
// answers a slot whose sign-extended halfword times 0xF0, added to word(first[4]), is the next position (zero when the
// halfword is non-negative); the loop repeats while that position is not second[2], each round calling
// func_08C46DAC(previous position, its index, self, it) and func_08C46D74(it, frame + 0x10, it) and keeping the position
// in frame +0x18 / +0x1C. At the end func_08C46DAC(position, the index of self's word +8, self, position) runs.
// v0 = its answer, or word(first) on the early exit.
void func_08C48CDC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 first_a = word(rt, first);
    const u32 first_b = word(rt, first + 4u);
    const u32 first_c = word(rt, first + 8u);
    const u32 first_d = word(rt, first + 0xCu);
    const u32 second_a = word(rt, second);
    const u32 second_b = word(rt, second + 4u);
    const u32 second_c = word(rt, second + 8u);
    const u32 second_d = word(rt, second + 0xCu);
    put(rt, frame.at(0x10u), first_a);
    put(rt, frame.at(0x14u), first_b);
    put(rt, frame.at(0x1Cu), first_d);
    put(rt, frame.at(0u), second_a);
    put(rt, frame.at(4u), second_b);
    put(rt, frame.at(0xCu), second_d);
    put(rt, frame.at(0x18u), first_c);
    put(rt, frame.at(8u), second_c);
    const auto base_of = [&](u32 node) -> u32 {
        const u32 base = word(rt, word(rt, self + 4u));
        return sext16(((node - base) >> 4) * 0xEEEEEEEFu);
    };
    const auto index_of = [&](u32 node) -> u32 { return node == 0u ? 0xFFFFFFFFu : base_of(node); };
    if (first_c == second_c) {
        ctx.gpr[2] = first_a;
        return;
    }
    u32 node = first_c;
    if (word(rt, self + 0xCu) != 0u) {
        call(rt, ctx, 0x08C46DACu, {word(rt, self + 0xCu), index_of(first_c), self, second_b});
    }
    put(rt, frame.at(0x1Cu), first_c);  // the step call's delay slot
    const u32 step = call(rt, ctx, 0x08C46D74u, {first_c, frame.at(0x10u), first_c});
    const s32 first_offset = static_cast<s16>(static_cast<u16>(step));
    u32 following = 0u;
    if (first_offset < 0) {
        const u32 base = word(rt, word(rt, frame.at(0x14u)));
        following = base + static_cast<u32>(first_offset) * 0xF0u;
    }
    put(rt, frame.at(0x18u), following);
    if (following != second_c) {
        u32 previous = first_c;
        node = following;
        for (;;) {
            put(rt, frame.at(0x1Cu), word(rt, frame.at(0x18u)));  // the lookup call's delay slot
            call(rt, ctx, 0x08C46DACu, {previous, index_of(node), self, node});
            const u32 answer = call(rt, ctx, 0x08C46D74u, {word(rt, frame.at(0x18u)), frame.at(0x10u), node});
            const s32 offset = static_cast<s16>(static_cast<u16>(answer));
            u32 next = 0u;
            if (offset < 0) {
                const u32 base = word(rt, word(rt, frame.at(0x14u)));
                next = base + static_cast<u32>(offset) * 0xF0u;
            }
            put(rt, frame.at(0x18u), next);
            if (next == second_c) break;
            previous = node;
            node = next;
        }
    }
    const u32 end_index = word(rt, self + 8u) == 0u ? 0xFFFFFFFFu : base_of(word(rt, self + 8u));
    call(rt, ctx, 0x08C46DACu, {node, end_index, self, node});
}

// --- 0x08902F00 ---------------------------------------------------------------------------------------------------
// (out = a0, self = a1, other = a2, f12 = a position): with a position at or below zero, or above self's word +0x14,
// *other's 12 bytes are copied to frame + 0x14, its +4 float raised by 0.5, frame +0x50 / +0x54 set to 0 / -1 and
// func_08902554(self, frame + 0x20, frame + 0x14) run; func_0890265C(self, frame + 0x14, {frame + 0x50, frame + 0x54})
// then answers the value that is used as the position (zero when it is not positive); a negative one writes 0 to out
// and -1.0 to out + 4. func_08900F94(self) is asked with that position, and with a positive count at self + 0x54 the
// records at word(self + 0x58) (+ i * 0x28) are tried in a zig-zag walk over the indices (i & 1 ? sum : difference,
// wrapped into [0, count) by the count): func_08902C00(frame + 0xC, self, other, record, the next record or the first)
// writes a float at frame + 0x10, and the walk ends when it is at least zero (out + 4 = it, out = frame + 0xC). An
// exhausted walk writes out + 4 = -1.0 and out = 0. v0 = out.
void func_08902F00(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 self = ctx.gpr[5];
    const u32 other = ctx.gpr[6];
    const float position = ctx.fpr[12];
    SavedRegisters saved(ctx);
    SavedFloat20 saved_float(ctx);
    GuestFrame frame(ctx, 0x90u);
    float used = position;
    if (position <= 0.0f || float_at(rt, self + 0x14u) < position) {
        call(rt, ctx, 0x08B79BACu, {frame.at(0x14u), other, 0xCu});
        put_float(rt, frame.at(0x18u), float_at(rt, frame.at(0x18u)) + 0.5f);
        put(rt, frame.at(0x50u), 0u);
        put(rt, frame.at(0x54u), 0xFFFFFFFFu);
        call(rt, ctx, 0x08902554u, {self, frame.at(0x20u), frame.at(0x14u)});
        put(rt, frame.at(4u), word(rt, frame.at(0x50u)));
        put(rt, frame.at(8u), word(rt, frame.at(0x54u)));
        const float value = call_keep(rt, ctx, 0x0890265Cu, {self, frame.at(0x14u), frame.at(4u)}).f0;
        put_float(rt, frame.at(0xCu), value);
        if (value < 0.0f) {
            put(rt, out, 0u);
            put_float(rt, out + 4u, -1.0f);
            ctx.gpr[2] = out;
            return;
        }
        put_float(rt, frame.at(0u), 0.0f);
        used = 0.0f < value ? float_at(rt, frame.at(0xCu)) : float_at(rt, frame.at(0u));
    }
    ctx.fpr[12] = used;
    const u32 first = call(rt, ctx, 0x08900F94u, {self});
    s32 count = static_cast<s32>(word(rt, self + 0x54u));
    if (count <= 0) {
        put_float(rt, out + 4u, -1.0f);
        put(rt, out, 0u);
        ctx.gpr[2] = out;
        return;
    }
    u32 cursor = first;
    u32 index = 0u;
    u32 sum = cursor + index;  // v1: the sum, recomputed in the loop's back branch delay slot
    for (;;) {
        cursor = (index & 1u) != 0u ? sum : cursor - index;
        const u32 adjusted = static_cast<s32>(cursor) < 0 ? cursor + static_cast<u32>(count) : cursor;
        const u32 slot = static_cast<u32>(static_cast<s32>(adjusted) % count);
        const u32 table = word(rt, self + 0x58u);
        const u32 record = table + slot * 0x28u;
        const s32 following = static_cast<s32>(slot) + 1;
        const u32 next = count == following ? table : table + static_cast<u32>(following) * 0x28u;
        call(rt, ctx, 0x08902C00u, {frame.at(0xCu), self, other, record, next});
        const float value = float_at(rt, frame.at(0x10u));
        if (0.0f <= value) {
            put_float(rt, out + 4u, value);
            put_float(rt, out, float_at(rt, frame.at(0xCu)));
            ctx.gpr[2] = out;
            return;
        }
        ++index;
        count = static_cast<s32>(word(rt, self + 0x54u));
        if (!(static_cast<s32>(index) < count)) break;
        sum = cursor + index;
    }
    put_float(rt, out + 4u, -1.0f);
    put(rt, out, 0u);
    ctx.gpr[2] = out;
}

// --- 0x08A4D0E8 ---------------------------------------------------------------------------------------------------
// (self = a0, text = a1, a2 = the draw selector, a3 = the number of lines, t0 = a flag byte, t1 = a second flag byte,
// f12 = f20 the line step): lays out `a3` lines of text (the prologue stores a3 and t1's byte at frame +0 / +4): each
// round cuts the line at the cursor up to '\n' or the terminator (the length is the characters after the first, so a
// line starting with '\n' or a terminator is empty), allocates 0x1E + length + 1 bytes rounded down to a 16-byte
// multiple on the guest stack, copies the line there through func_08B7C830 and terminates it, stores a zero float at
// self + 0x28 and calls func_08A4CFCC(self, copy, a2, t0's byte, 1). Then self + 0x34 rises to self + 0x28 when it was
// lower, and self + 0x28 becomes (self + 0x50 - self + 0x48) - self + 0x28, halved when the low two bits of the byte
// self + 0x58 are 1 (left alone for 0 and 3). When the second flag byte is set, func_08A4CFCC(self, copy, a2, t0's
// byte, 0) runs again, then func_08A4CF24(self) ends the round; the next line starts after the '\n'. v0 = the line
// count the loop's last test loads (the a3 argument).

// --- 0x08B5A018 ---------------------------------------------------------------------------------------------------
// (self = a0, want = a1): the pool of 0xFF 16-byte entries at self (heads at self + 0x200 / +0x204, the spare index at
// +0x206, the used count at +0x208). With the used count below want the list at self + 0x204 is walked: each round
// func_08B59BC0(self, self + 0x204, entry) publishes and func_08B59D6C(self, entry) collects the entry, the round
// repeats while the count is still short (an exhausted list answers 0xFF). From the list at self + 0x200 the first
// entry whose count (halfword +6) reaches want is taken, keeping the largest count seen (short ones are walked through
// the byte +3 links), the tail of that chain raises the used count and stores it at self + 0x208; an entry whose count
// is exactly want is published with func_08B59BC0(self, self + 0x200, it) and answers that entry. Otherwise the spare
// entry at index (signed) byte self + 0x206 takes the remainder: its byte +3 moves to self + 0x206, +5 = the found
// entry, +4 = the found entry's +4, +6 = want, +8 = the found entry's +8; the previous entry's +5 is re-linked, the
// found entry keeps count - want with its +8 advanced by want * 0x2000, and its +4 becomes the spare index's low byte.
// v0 = the spare index (signed) or 0xFF.

// --- 0x08C55DF0 ---------------------------------------------------------------------------------------------------
// (out = a0, text = a1, flag = a2's low byte): *out cleared, the record at text + word(word(text) - 0xC) inspected.
// When its word +0x14 is zero the object at +0x70 (when set) is asked through func_08C70238 and the record reloaded;
// with `flag` set, or with bit 0x1000 of its word +0xC clear, the flags step follows. Otherwise the record's word +0x78
// is a byte cursor (words +8 / +0xC; exhausted asks the virtual +0x24, advancing asks +0x28) and the record's word
// +0x7C a table whose byte +0x18 array (sign-extended) tests bit 8 of the character and of each following one, the
// cursor advancing; -1 or a character without bit 8 ends the run. The flags step reloads the record: a non-zero word
// +0x14 ends the function with a tail call to func_08C54CFC(record, that word | 4); otherwise *out becomes 1 and v0 = 1.

// --- 0x08B01D1C ---------------------------------------------------------------------------------------------------
// (a0 = a pointer to the container, a1 = where the result goes, a2..a3, t0..t3 and the caller's stack words +0x40,
// +0x44, +0x48, +0x4C complete the element description): the element is looked up, or inserted. With *out null,
// func_08B01804(container, a3, t3) is asked and its answer stored at *out; zero asks func_08B01934(container, a3, t3)
// the same way, and an answer there moves the base entry's halfword +0xEC into the new element's +6 and the element's
// index ((*out - container[0x44]) >> 3) into the base's +0xEC. An existing element whose halfword 0 has bit 0 clear
// goes through func_08B01154(element, container). func_08B011F0(element, container, a2, t0, t1, stack +0x40,
// stack +0x44, t3) then looks it up; zero asks func_08B01B2C(container, a2, t0, t1, stack +0x40, stack +0x44) to build
// one, which also gets the previous entry's halfword +4 into its +0x52 and its own index ((element - container[0x60]) >>
// 2 times 0x3CF3CF3D) into that halfword. The element's byte +0x51 becomes 1 and the three descriptors (t2's, stack
// +0x48's and stack +0x4C's first three words) land at +0x0C / +0x14, +0x18 / +0x20 and +0x00 / +0x08. v0 = the
// element, or 0 when neither call made one.

} // namespace
} // namespace new_batch_main_200
} // namespace gt::game

#define MAIN200(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_200::name, gt::ReturnKind::kind)
MAIN200(0x088E6250u, func_088E6250, Word);
MAIN200(0x08C48CDCu, func_08C48CDC, Word);
MAIN200(0x08902F00u, func_08902F00, Word);

// ---- from main-session batch 201 (draft 12681f9bb7caf2a6da12ed4855057c60fdbbc7c1) ----
// Main-session batch 201: nine functions written by hand from `tools/disasm` (drafts — the gate runs after this
// session). No name has evidence, so they keep the recompiler's func_XXXXXXXX names. Every step is the original's,
// in the original's order: a delay slot runs before its call / branch (a branch-likely delay slot only when the
// branch is taken), callee-saved registers and floats are set where the asm sets them and put back at the end, and
// v0 / f0 end as the asm leaves them on every exit path (untouched where the original never writes v0).
// Input shapes for the synthetic generator: /tmp/claude-1000/shapes_main201.txt (and config/synth_shapes.txt when
// this file lands).
// Static audits on this file: frame_audit 0 wrong (the seven GuestFrame sizes match their prologues; 0x088F2D98 has
// no frame), literal_check 0 missing, sp_loads none, arg_reads 1 — 0x08B36170 copies t3 to t2 only to compare them
// with each other before writing either, so the argument is not read (the batch-95 verdict for the same callee).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_201 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using s8 = std::int8_t;
using s16 = std::int16_t;

// A guest call with the argument registers written to ctx exactly as the original writes them before its `jal`
// (a0-a3, t0-t3 in order); a register the call does not name keeps what an earlier call left there. Afterwards v0
// holds the result, as after the `jal`.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same, leaving v0/v1/f0 as the callee left them: the original reads those after the `jal` where a shape needs
// the answer more than a pointer (the float getters of 0x089ACEE8, 0x08B498AC in 0x0895B8C4).
GuestResult call_set_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments (f12, f13, ...), v0/v1/f0 as the callee left them.
GuestResult call_setf_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                           std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned float_reg = 12u;
    for (const float argument : float_arguments) {
        if (float_reg <= 19u) ctx.fpr[float_reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

s8 signed_byte_at(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
s16 signed_half_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }

// The compiler's word-at-a-time struct copy (4 bytes each, `bytes` a multiple of 4).
void copy_words(Runtime &rt, u32 destination, u32 source, u32 bytes) {
    for (u32 offset = 0; offset < bytes; offset += 4u) put(rt, destination + offset, word(rt, source + offset));
}
// MIPS `ins rd, rs, pos, size`: `size` bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}

// s0-s7 and fp are callee-saved: the body sets them where the original sets them (a callee may read them) and the
// caller's values come back at the end.
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

// f20-f23 are callee-saved too.
class SavedFloats {
public:
    explicit SavedFloats(AllegrexContext &ctx)
        : ctx_(ctx), f20_(ctx.fpr[20]), f21_(ctx.fpr[21]), f22_(ctx.fpr[22]), f23_(ctx.fpr[23]) {}
    ~SavedFloats() {
        ctx_.fpr[20] = f20_;
        ctx_.fpr[21] = f21_;
        ctx_.fpr[22] = f22_;
        ctx_.fpr[23] = f23_;
    }
    SavedFloats(const SavedFloats &) = delete;
    SavedFloats &operator=(const SavedFloats &) = delete;

private:
    AllegrexContext &ctx_;
    float f20_, f21_, f22_, f23_;
};

// 0x08AF5D60 (a0 = owner, its word at +0x9C is the entry table; a1 = entry index; a2 = a 4-float destination, a3 =
// a 3-float destination; t0 and t1 = two per-entry tables): reads the 0x80-byte entry at table + index*0x80. A
// non-negative s16 at +8 recurses first with copies of the two vectors; then the 0x20-byte slot at table1 + index*0x20
// (a byte at +0x1C picks the way) either copies its two vectors straight out or takes copies through 0x08B4BAA8 /
// 0x08B4AA34 / 0x08B4B858; 0x08C3FBBC and 0x08C3FB94 publish the two vectors at table0 + index*0x20 (+0x10 for the
// second); a non-negative s16 at +6 recurses last with the results. v0 is the last call's result, as the asm leaves it.

// The stream buffer 0x08C57080 walks: +8 is the read cursor, +0xC the end; the virtual at +0x24 answers the current
// character when the cursor reached the end (the peek), the one at +0x28 the next character and advances (the take).
u32 stream_peek(Runtime &rt, AllegrexContext &ctx, u32 buffer) {
    const u32 cursor = word(rt, buffer + 8u);
    if (cursor < word(rt, buffer + 0xCu)) return rt.memory().load8(cursor);
    const u32 table = word(rt, buffer);
    return call_indirect(rt, ctx, word(rt, table + 0x24u), {buffer});
}
u32 stream_take(Runtime &rt, AllegrexContext &ctx, u32 buffer) {
    const u32 cursor = word(rt, buffer + 8u);
    if (cursor < word(rt, buffer + 0xCu)) {
        put(rt, buffer + 8u, cursor + 1u);
        return rt.memory().load8(cursor);
    }
    const u32 table = word(rt, buffer);
    return call_indirect(rt, ctx, word(rt, table + 0x28u), {buffer});
}

// 0x08C57080 (a0 = stream object, a1 = output bytes, a2 = limit, a3 = delimiter byte): 0x08C55FDC(sp, self, 1) writes
// a status byte at sp; a zero byte ends with no data. Otherwise the buffer at self + word(self[0] - 0xC) + 0x78 is
// peeked and its characters copied to the output until the delimiter, end of buffer or the limit; each copy bumps the
// counter at self + 4. The end sets (self + word(self[0] - 0xC)) + 0x14 to its old value OR-ed with 2 (end of buffer),
// 4 (nothing copied) or 0. v0 = self on every exit, as the asm leaves it.
void func_08C57080(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx);
    const u32 self = ctx.gpr[4];
    u32 output = ctx.gpr[5];
    const s32 limit = static_cast<s32>(ctx.gpr[6]);
    const u32 delimiter = ctx.gpr[7] & 0xFFu;
    put(rt, self + 4u, 0u);
    call_set(rt, ctx, 0x08C55FDCu, {frame.at(0u), self, 1u});
    u32 state = 0u;
    if (rt.memory().load8(frame.at(0u)) != 0u) {
        const u32 buffer = word(rt, self + word(rt, word(rt, self) - 0xCu) + 0x78u);
        u32 character = stream_peek(rt, ctx, buffer);
        for (;;) {
            if (!(static_cast<s32>(word(rt, self + 4u) + 1u) < limit)) {
                state = character != 0xFFFFFFFFu ? 0u : 2u;
                break;
            }
            if (character == 0xFFFFFFFFu) {
                state = 2u;
                break;
            }
            if (character == delimiter) break;
            put8(rt, output, character);
            output += 1u;
            put(rt, self + 4u, word(rt, self + 4u) + 1u);
            character = stream_take(rt, ctx, buffer);
            if (character != 0xFFFFFFFFu) character = stream_peek(rt, ctx, buffer);
        }
    }
    if (limit > 0) put8(rt, output, 0u);
    if (word(rt, self + 4u) == 0u) state |= 4u;
    if (state != 0u) {
        const u32 derived = self + word(rt, word(rt, self) - 0xCu);
        call_set(rt, ctx, 0x08C54CFCu, {derived, word(rt, derived + 0x14u) | state});
    }
    ctx.gpr[2] = self;
}

// 0x088C10B0 (a0 = a wheel-like object: +8 is a settings block, +0x24 a state block; a1 = an output block: word +0xC
// and +0x10 cleared, the float answer at +8): 0x08B4A430(self, out, -a1[0], a1[4]) answers a float that 0x0893C60C
// (sp + 4, sp) turns into the stored value. With the state block's byte +0xF equal to 1 the settings' +0x6C4 picks a
// steering angle (0.1745, or 0.1745 - x*0.8727/8.3333 when it is below -8.3333, with the output word at +0xC = -1 or
// the settings pointer); otherwise the magnitude of the answer against 1.0472 and the byte +0xF == 2 select a branch
// that sets +0xC / +0x10 from the same value's sign. A non-zero counter at state +0x6C is decremented and returned;
// else 0x088C0DCC(self, 0, state[0x97] & 1, state[0x97] & 2) with the angle in f13 decides, and v0 = -0x4C.

// 0x088C6CB8 (a0 = a record: word +0 the element count, +4 the array of 0x40-byte elements whose float at +0x14 is
// ordered, 0x088C50C4(elements, index) answering an element; a1 = the start index, a2 / a3 = two output words, f12 =
// the value being located): with fewer than 2 elements it tail-calls 0x088C6B34 with the same arguments (f12 too).
// Otherwise, when the second-to-last element's value is not above the last's, a value below the first element answers
// (last, 0) type 1, one not below element count-2 answers (count-2, 0) type 2, and the spiral (offsets 0, +1, -2,
// +3, ... around a1) answers (predecessor, element) type 3 for the element whose predecessor is above the value and
// which is not; count+1 steps without one answer 0. v0 = the type, or the tail callee's answer.

// 0x088F2D98 (a0 = a particle-like record: the s16 at +8 is the first entry index, the word at +4 the entry table of
// 0x40-byte entries, the u16 at +0xA an entry index written back; f12 = a time step): walks the entry chain the low
// 13 bits of each entry's first word links. Each entry integrates four floats (the settings' +0x54 / +0x60 / +0x6C and
// the 360 wrap of +0x14, the -9.81 term of +0x18 with the sub-object at self + byte[3]*56), picks the table float at
// 0x08C8A364 + (byte[2] & 7) * 4 over the max of its +8 and 1, and when the resulting +0x24 value reaches that entry's
// +0x2C remembers the entry; the remembered index goes into the next entry's low 13 bits, or back to self's +8 when
// none was, and the next entry's index into the low 13 bits of the current entry's first word. v0 is not set on the
// path the original never writes it on (the gate compares none).

// 0x0895B8C4 (a0 = a state object: +8 and +0xC are two sub-objects; f12 = a value): 0x0895B59C(self) gates
// everything (0 when it answers zero). r = self[8] and q = self[0xC]; r + 0x778 has its words +4 / +8 cleared, and
// 0x08B498AC(sp + 4, sp, r[0x51C]) fills two floats at sp. The global 0x08CF35A0 (all fields in .bss) then selects a
// first weight at r + 0x778 + 4: T[0x2C] nonzero picks t = x when T[0x30] != 0 else r[0x6F8], and the weight is
// 1.0 when T[0x38] <= d, 0.0 when d <= -T[0x34], else (d + T[0x34]) / (T[0x38] + T[0x34]). T[0x3C] does the same for
// a second weight at +8 from q[0x2C] and the [0x34] floats of 0x08955300(self, 2) and (self, 3). v0 = 0 or 1.
void func_0895B8C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(ctx);
    SavedFloats saved_floats(ctx);
    const u32 self = ctx.gpr[4];
    const float input = ctx.fpr[12];
    ctx.fpr[20] = input;
    if (call_set(rt, ctx, 0x0895B59Cu, {self}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 settings = word(rt, self + 8u);
    const u32 weights = settings + 0x778u;
    put(rt, weights + 4u, 0u);
    put(rt, weights + 8u, 0u);
    const u32 block = word(rt, self + 0xCu);
    call_setf_keep(rt, ctx, 0x08B498ACu, {frame.at(4u), frame.at(0u)}, {float_at(rt, settings + 0x51Cu)});
    const u32 table = 0x08CF35A0u;
    const auto band_weight = [](float value, float high, float band) {
        if (high <= value) return 1.0f;
        if (value <= -band) return 0.0f;
        return (value + band) / (high + band);
    };
    if (word(rt, table + 0x2Cu) != 0u) {
        const float scale = word(rt, table + 0x30u) != 0u ? input : float_at(rt, settings + 0x6F8u);
        const float rate = float_at(rt, block + 0x10u);
        const float c = float_at(rt, settings + 0x6C4u);
        const float p = float_at(rt, settings + 0x6C8u) - psp_mul(scale, rate);
        const float d = (psp_mul(c, float_at(rt, frame.at(0u))) - psp_mul(p, float_at(rt, frame.at(4u)))) - c;
        const float left = psp_mul(float_at(rt, settings + 0x5FCu), float_at(rt, block + 0x270u + 0xA8u));
        const float right = psp_mul(float_at(rt, settings + 0x600u), float_at(rt, block + 0x270u + 0xACu));
        const float slip = (left - right) - d;
        put_float(rt, weights + 4u,
                  band_weight(slip, float_at(rt, table + 0x38u), float_at(rt, table + 0x34u)));
    }
    ctx.gpr[3] = 1u;
    if (word(rt, table + 0x3Cu) != 0u) {
        const float scale = word(rt, table + 0x40u) != 0u ? input : float_at(rt, settings + 0x6F8u);
        float spread = psp_mul(scale + scale, float_at(rt, block + 0x2Cu));
        const u32 first = call_set(rt, ctx, 0x08955300u, {self, 2u});
        const float before = float_at(rt, first + 0x34u);
        const u32 second = call_set(rt, ctx, 0x08955300u, {self, 3u});
        float difference = float_at(rt, second + 0x34u) - before;
        if (spread < 0.0f) {
            spread = -spread;
            difference = -difference;
        }
        put_float(rt, weights + 8u,
                  band_weight(difference - spread, float_at(rt, table + 0x48u), float_at(rt, table + 0x44u)));
    }
    ctx.gpr[2] = 1u;
}

// 0x089ACEE8 (a0 = self, a1 = a reference-counted object): 0x08968C84(a1) gates it. 0x089A8C70(self) picks the
// rectangle source: its two objects come from 0x089A8C94(self) (the +0x34 fields of two entries) or, when
// 0x089A8C70 answered zero, from the +0x00 / +0x04 / +0x08 / +0x0C floats of 0x089A8B38..0x089A8B80(self). The
// overlap rectangle's x / y and right / bottom edges are compared with the two floats 0x089AA95C(self, sp, sp + 4)
// reads from self + 0xE4 / +0xE8; when the point lies inside, the object replaces self + 0x4C after 0x08A5BA08 /
// 0x08A5B930 refcount it (self + 0x4C added one, the old one released). v0 is whatever the last call of the taken
// path left.
void func_089ACEE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(ctx);
    SavedFloats saved_floats(ctx);
    const u32 self = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    if (call_set(rt, ctx, 0x08968C84u, {object}) == 0u) return;
    float left_edge = 0.0f;
    float top_edge = 0.0f;
    float right_edge = 0.0f;
    float bottom_edge = 0.0f;
    if (call_set(rt, ctx, 0x089A8C70u, {self}) != 0u) {
        const u32 second = call_set(rt, ctx, 0x089A8C94u, {self});
        const u32 first = call_set(rt, ctx, 0x089A8B98u, {self});
        const float first_x = float_at(rt, first);
        const float second_x = float_at(rt, second);
        const float first_y = float_at(rt, first + 4u);
        const float second_y = float_at(rt, second + 4u);
        float min_x = second_x;
        if (second_x < first_x) min_x = first_x;
        float min_y = second_y;
        if (second_y < first_y) min_y = first_y;
        float max_x = second_x + float_at(rt, second + 8u);
        const float first_right = first_x + float_at(rt, first + 8u);
        if (max_x < first_right) max_x = first_right;
        float max_y = second_y + float_at(rt, second + 0xCu);
        const float first_bottom = first_y + float_at(rt, first + 0xCu);
        if (!(max_y < first_bottom)) max_y = first_bottom;
        const float width = max_x - min_x;
        const float height = max_y - min_y;
        if (width <= 0.0f || height <= 0.0f) {
            left_edge = 0.0f;
            top_edge = 0.0f;
            right_edge = 0.0f;
            bottom_edge = 0.0f;
        } else {
            right_edge = min_x + width;
            bottom_edge = min_y + height;
            left_edge = min_x;
            top_edge = min_y;
        }
    } else {
        const float first = call_set_keep(rt, ctx, 0x089A8B38u, {self}).f0;
        const float second = call_set_keep(rt, ctx, 0x089A8B50u, {self}).f0;
        const float third = call_set_keep(rt, ctx, 0x089A8B68u, {self}).f0;
        const float fourth = call_set_keep(rt, ctx, 0x089A8B80u, {self}).f0;
        left_edge = first;
        top_edge = second;
        right_edge = left_edge + third;
        bottom_edge = top_edge + fourth;
    }
    ctx.fpr[20] = top_edge;
    ctx.fpr[21] = left_edge;
    ctx.fpr[22] = right_edge;
    ctx.fpr[23] = bottom_edge;
    call_set(rt, ctx, 0x089AA95Cu, {self, frame.at(0u), frame.at(4u)});
    const float point_x = float_at(rt, frame.at(0u));
    if (!(left_edge <= point_x)) return;
    if (!(point_x < right_edge)) return;
    const float point_y = float_at(rt, frame.at(4u));
    if (top_edge <= point_y) return;
    if (point_y < bottom_edge) return;
    if (object != 0u) call_set(rt, ctx, 0x08A5BA08u, {object});
    const u32 previous = word(rt, self + 0x4Cu);
    put(rt, self + 0x4Cu, object);
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
}

// 0x08AC3DDC (a0 = self, a1 = a name): takes the lock at self + 0x18C (0x08B344E0), copies the name into
// self + 0x1B6 and self + 0x236 (0x80 bytes each, the byte at +0x235 cleared), marks self + 0x1B5, and when the name's
// length is 1..0x7E and the character at self + 0x236 + length - 1 is not '/' runs 0x08B7B9AC(self + 0x236,
// "off"). Then 0x08B2DE00 picks the 0x3C-byte block: zero, 0x08B0EC60(sp + 0xC0, self + 0x1B6) fills it, otherwise
// 0x08B0CE58(sp + 0x80) with word 5 at its head. The block moves to sp + 0x40; self + 0x1B4 becomes 1 when its first
// word is zero and its second is 3, else 0. The byte at +0x2B5 is cleared and v0 = self[0x1B4] after the lock's
// release (0x08B3457C).

// 0x08B2FC48 (a0 = self: word +0xC is the source id, +0xA0 the compared key, +0x928 a 16-entry table of 0x98-byte
// records (the last one ends at +0x1218), +0x1C a name field; a1 = an array of 0x98-byte output records, a2 = the
// entry limit, a3 / t0 = two flag bytes): asks 0x08C7AEA0(self + 0xC, sp, sp + 0x84) for a list of keys (word 0xC0 at
// sp before the call), then walks it. Each step clears the record at a1 + index*0x98, copies the six key bytes at
// list + 4 into it and puts -1 at +8; the table entry whose +0x928 / +0x92C words both match the key's first two words
// supplies the word at +0x930, and the key is compared with self + 0xA0 (0x08B36170); with the flags set the record's
// +0x10 takes the name (self + 0x1C or 0x08C7AF00(list + 4)'s text) and its byte +0x90 is marked. v0 = the index
// reached (0, -1 or the count).

} // namespace
} // namespace new_batch_main_201
} // namespace gt::game

#define MAIN201(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_201::name, gt::ReturnKind::kind)
MAIN201(0x08C57080u, func_08C57080, Word);
MAIN201(0x0895B8C4u, func_0895B8C4, Word);
MAIN201(0x089ACEE8u, func_089ACEE8, Word);

// ---- from main-session batch 202 (draft 63662175d3be76c2677e812138152e2ec1403843) ----
// Main-session batch 202: functions no gate could give useful inputs before, written as structured C from `tools.disasm`.
// Each body keeps the original's input-output contract: every argument register a callee reads is passed, the delay slots
// that run before a call or a branch are honoured, and `v0` ends as the asm leaves it.
//
// 0x08B65A7C  the C++ runtime's verbose terminate handler (its strings: "terminate called recursively",
//             "terminate called after throwing an instance of '", "terminate called without an active exception");
//             the once-flag is the byte at 0x08D72BA8.
// 0x08B7527C  a driver loop: it runs the state pass 0x08B7394C over a 0xF20-byte local state block, calls the caller's
//             callback (a0) with the block as its a0 when the pass answers 0 or 5, and ends 3 when either answers non-zero.
// 0x08911B80  formats two names from a source object's vtable answers and installs two counted holders (+0x34, +0x3C).
// 0x08B01934  the record of one key in the object's 8-byte member container (vector at +0x44, s16 key at +0x50, table
//             at +0x58); 0x08B0225C is the same body over the 6-byte container (vector +0x7C, key +0x88, table +0x90).
// 0x0893ECC4  the piecewise curve keyed by the byte at word(a0 + 0xC) + 0x282 (f12 in, f0 out, v0 = 3).
//
// Not drafted here (unsure, left for a later pass): 0x08A63C5C (state machine dispatching through `jr` to its own
// internal labels), 0x08ADCF48 (64-bit clamp), 0x08B43FC8 (linked-list walk handing a stack structure to three callees),
// 0x08C16480 / 0x08C47728 (sorted-array insert / vector growth). 0x08BB2B00 and 0x08C496A4 already have bodies in
// hot_batch_336.cpp and hot_batch_97.cpp (unregistered), so they are not duplicated here.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_202 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08B65A7C — __gnu_cxx::__verbose_terminate_handler.

constexpr u32 kTerminateCalled = 0x08D72BA8u;  // one byte: the recursion guard of the terminate handler

// The per-thread stdio block (0x08B796C4 reads $k0); its word at +0xC is the stream the handler writes to.
u32 stderr_stream(Runtime &rt, AllegrexContext &ctx) {
    const u32 block = call(rt, ctx, 0x08B796C4u);
    return word(rt, block + 0xCu);
}

// fwrite(text, 1, count, stream) = 0x08B79618, the stream fetched first as the asm does (a3 is set before a0-a2).
void write_text(Runtime &rt, AllegrexContext &ctx, u32 text, u32 count) {
    const u32 stream = stderr_stream(rt, ctx);
    call(rt, ctx, 0x08B79618u, {text, 1u, count, stream});
}

// fputs(text, stream) = 0x08B78808.
void write_string(Runtime &rt, AllegrexContext &ctx, u32 text) {
    const u32 stream = stderr_stream(rt, ctx);
    call(rt, ctx, 0x08B78808u, {text, stream});
}


// ---------------------------------------------------------------------------------------------------------------------
// 0x08B7527C — run the state pass until it or the callback answers.


// ---------------------------------------------------------------------------------------------------------------------
// 0x08911B80 — format two names from a source object and install two counted holders.

// The ll/sc reference decrement of the counted holder: a0 = the old value, v1 = the address, v0 = 1 (the `sc` answer).
u32 atomic_decrement(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}

// The shared holder's release: the strong count at +4, then the weak count at +8; the last of either runs the
// holder's virtual destructor (slots 8 and 0xC of the vtable at +0, re-read before each call as the asm does).
void release_holder(Runtime &rt, AllegrexContext &ctx, u32 holder) {
    if (holder == 0u) return;
    if (atomic_decrement(rt, ctx, holder + 4u) != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder});
    if (atomic_decrement(rt, ctx, holder + 8u) != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder});
}

// The string formatter 0x08B3767C (out, size, format, a3, t0, t1); the argument registers are set in address order.
void format_text(Runtime &rt, AllegrexContext &ctx, u32 out, u32 format, u32 a3, u32 t0, u32 t1) {
    ctx.gpr[6] = format;
    ctx.gpr[4] = out;
    ctx.gpr[5] = 0x20u;
    ctx.gpr[7] = a3;
    ctx.gpr[8] = t0;
    ctx.gpr[9] = t1;
    call(rt, ctx, 0x08B3767Cu, {out, 0x20u, format, a3, t0, t1});
}

constexpr u32 kFormatSuffix = 0x08C8A5F8u;    // the fifth/sixth argument of both format calls
constexpr u32 kKindNames = 0x08CDD8E4u;       // word table indexed by the source's type answer (its low two bits are kept)


// ---------------------------------------------------------------------------------------------------------------------
// 0x08B01934 / 0x08B0225C — the record of one key in the object's two member containers.
//
// One member: a vector of fixed-size records at `vector` (begin, end, cap at +0, +4, +8), an s16 key at `index` (-1 =
// empty) whose table object sits 8 bytes further (the base pointer at the table's +0, records of the same size indexed
// by the key), the record size in bytes, the vector's growth function and the table's updater. The body answers the
// record for the key: with key -1 the record (`record`, built on the stack by the caller as the asm does) is appended
// to the vector - through the growth function when the vector is full - and with any other key the record at that index
// of the table is refreshed through the updater and then overwritten with `record`.

struct Member {
    u32 vector;       // begin, end, cap
    u32 index;        // the s16 key; the table object at index + 8
    u32 element_size; // 8 (0x08B01934) or 6 (0x08B0225C)
    u32 grow;         // the vector's growth function (0x08C47C08 / 0x08C47728)
    u32 update;       // the table's updater (0x08C46854 / 0x08C47DFC)
};

// The 0x08B01934 record's fields: a 15-bit value above bit 0, a 16-bit tag, then the sentinel the asm builds from
// `addiu $v0, $zero, -1` (its `ins` masks print no hex 0x7FFF / 0xFFFF in the words themselves).
constexpr u32 kRecordValueMask = 0x7FFFu;
constexpr u32 kHalfWordMask = 0xFFFFu;
constexpr u32 kRecordSentinel = 0xFFFFu;

// The record copy the asm writes: two words for an 8-byte record, a word and a halfword for a 6-byte one (its sixth
// byte is never part of the record).
void copy_record(Runtime &rt, u32 destination, u32 source, u32 element_size) {
    put(rt, destination, word(rt, source));
    if (element_size == 8u) put(rt, destination + 4u, word(rt, source + 4u));
    else rt.memory().store16(destination + 4u, rt.memory().load16(source + 4u));
}

u32 member_record(Runtime &rt, AllegrexContext &ctx, const Member &member, u32 frame, u32 record) {
    const s32 key = static_cast<std::int16_t>(rt.memory().load16(member.index));
    if (key == -1) {
        const u32 end = word(rt, member.vector + 4u);
        if (end == word(rt, member.vector + 8u)) {
            call(rt, ctx, member.grow, {member.vector, end, record});
        } else if (end != 0u) {
            copy_record(rt, end, record, member.element_size);
            put(rt, member.vector + 4u, end + member.element_size);
        } else {
            put(rt, member.vector + 4u, member.element_size);
        }
        return word(rt, member.vector + 4u) - member.element_size;
    }
    const u32 table = word(rt, member.index + 8u);
    u32 indexed = 0u;
    if (key >= 0) indexed = word(rt, table) + member.element_size * static_cast<u32>(key);
    // The structure handed to the updater (sp+0x18) and the two surrounding ones; the words at sp+0x38 / sp+0x48 are
    // the original's stale stack slots, so they are read, not invented.
    put(rt, frame + 8u, word(rt, frame + 0x48u));
    put(rt, frame + 0xCu, table);
    put(rt, frame + 0x10u, indexed);
    put(rt, frame + 0x14u, 0u);
    put(rt, frame + 0x3Cu, table);
    put(rt, frame + 0x40u, indexed);
    put(rt, frame + 0x44u, 0u);
    put(rt, frame + 0x4Cu, table);
    put(rt, frame + 0x50u, indexed);
    put(rt, frame + 0x54u, 0u);
    put(rt, frame + 0x18u, word(rt, frame + 0x38u));
    put(rt, frame + 0x1Cu, table);
    put(rt, frame + 0x20u, indexed);
    put(rt, frame + 0x24u, 0u);
    call(rt, ctx, member.update, {frame + 0x60u, member.index, frame + 0x18u});
    for (u32 offset = 0u; offset < 0x10u; offset += 4u) put(rt, frame + 0x28u + offset, word(rt, frame + 0x60u + offset));
    copy_record(rt, indexed, record, member.element_size);
    return indexed;
}

void func_08B01934(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    const u32 tag = ctx.gpr[6];
    GuestFrame frame(ctx, 0x80u);
    const u32 base = ctx.gpr[29];
    // The record: the 15-bit value above bit 0 (the asm clears bit 0 first, so the stack word it loads is dead), the
    // 16-bit tag, then the sentinel word.
    const u32 record0 = ((value & kRecordValueMask) << 1) | ((tag & kHalfWordMask) << 16);
    put(rt, base, record0);
    rt.memory().store16(base + 2u, static_cast<std::uint16_t>(tag));
    rt.memory().store16(base + 6u, static_cast<std::uint16_t>(kRecordSentinel));
    rt.memory().store16(base + 4u, static_cast<std::uint16_t>(kRecordSentinel));
    const Member member{self + 0x44u, self + 0x50u, 8u, 0x08C47C08u, 0x08C46854u};
    ctx.gpr[2] = member_record(rt, ctx, member, base, base);
}

void func_08B0225C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    const u32 base = ctx.gpr[29];
    // The record is the constant sentinel {0xFFFF, 0x0000, 0xFFFF}; the second argument register is reused as its first
    // word and neither of the function's value arguments is read.
    put(rt, base, 0xFFFFu);
    rt.memory().store16(base + 4u, 0xFFFFu);
    const Member member{self + 0x7Cu, self + 0x88u, 6u, 0x08C47728u, 0x08C47DFCu};
    ctx.gpr[2] = member_record(rt, ctx, member, base, base);
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x0893ECC4 — the curve of the angle f12 (radians per second): revolutions per minute, then a piecewise ramp.

void func_0893ECC4(Runtime &rt, AllegrexContext &ctx) {
    constexpr float kTwoPi = 6.283185482025146484375f;  // 0x40C90FDB
    float rpm = ctx.fpr[12] * 60.0f;
    rpm = rpm / kTwoPi;
    if (rpm < 0.0f) rpm = -rpm;
    const u32 profile = word(rt, ctx.gpr[4] + 0xCu) + 0x270u;
    const u32 kind = rt.memory().load8(profile + 0x12u);
    ctx.gpr[2] = 3u;
    if (kind == 3u) {
        if (rpm < 1200.0f) ctx.fpr[0] = 399.839996337890625f;                    // 0x43C7EB85
        else if (rpm < 1500.0f) ctx.fpr[0] = 477275.71875f / rpm;                // 0x48E90B77
        else if (rpm < 5900.0f)
            ctx.fpr[0] = ((rpm - 1500.0f) * -0.0015454543754458426818847656f + 68.0f)  // 0xBACA90D8, 68.0
                         * 716.20001220703125f * 9.80000019073486328125f / rpm;         // 0x44330CCD, 0x411CCCCD
        else if (rpm < 6500.0f) ctx.fpr[0] = (6500.0f - rpm) / 600.0f * 72.8047637939453125f;  // 0x42919C0A
        else ctx.fpr[0] = 0.0f;
    } else {
        if (rpm < 400.0f) ctx.fpr[0] = 349.860015869140625f;  // 0x43AEEE15
        else if (rpm < 1040.0f)
            ctx.fpr[0] = (rpm - 400.0f) * -0.0721218958497047424316406250f + 349.860015869140625f;  // 0xBD93B4A5
        else if (rpm < 5600.0f) ctx.fpr[0] = 315844.21875f / rpm;                                       // 0x489A3887
        else if (rpm < 6000.0f) ctx.fpr[0] = (6000.0f - rpm) / 400.0f * 56.400753021240234375f;          // 0x42619A5F
        else ctx.fpr[0] = 0.0f;
    }
}

} // namespace
} // namespace new_batch_main_202
} // namespace gt::game

#define MAIN202(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_202::name, gt::ReturnKind::kind)
MAIN202(0x0893ECC4u, func_0893ECC4, Float);
MAIN202(0x08B01934u, func_08B01934, Word);
MAIN202(0x08B0225Cu, func_08B0225C, Word);

// ---- from main-session batch 203 (draft fe270df6de3183823d7a8816016a4d5697874415) ----
// Main-session batch 203: eleven functions the earlier batches left written but unverified (the gate could not run
// them with useful inputs: they take objects from outside, jump through vtables, or walk structures the arena never
// held). Written by hand as structured C from `tools.disasm`; four of them (08BDF848, 08A3C930, 08ACFAF0, 08ACFCF8)
// start from the draft of commit 3a91a22 and follow their own asm again.
// No name has evidence, so they keep the recompiler's func_XXXXXXXX names. Every exit leaves v0 as the asm does: a
// call's result where the asm ends with a call, the word it loads where it loads one, the constant it sets otherwise.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_203 {
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
// The same for a callee with float arguments ($f12...) and a float result ($f0); returns the result.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                 std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}
// A `lw v1, slot(reg); lw v0, 0(v1); jalr v0` (v0 = `v0_before`, the word the callee starts with when it does not
// write v0) or a two-step `lw v0, 0(obj); lw v1, slot(v0); jalr v1` (v0_before = the vtable, v1 = the target).
u32 call_through(Runtime &rt, AllegrexContext &ctx, u32 v0_before, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = v0_before;
    ctx.gpr[3] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
s32 signed_byte(Runtime &rt, u32 a) { return static_cast<std::int8_t>(rt.memory().load8(a)); }
s32 signed_half(Runtime &rt, u32 a) { return static_cast<std::int16_t>(rt.memory().load16(a)); }
void put16(Runtime &rt, u32 a, u32 v) { rt.memory().store16(a, static_cast<std::uint16_t>(v)); }
s32 sar(s32 value, unsigned bits) { return static_cast<s32>(value >> bits); }
// `mult` + `mflo`: the low 32 bits of the signed product (the vector code's count from a byte distance).
u32 mult_low(s32 a, s32 b) { return static_cast<u32>(static_cast<std::int64_t>(a) * static_cast<std::int64_t>(b)); }
// `min rd, rs, rt`: the signed minimum.
s32 signed_min(s32 a, s32 b) { return a < b ? a : b; }

// The release of a string cell `data` (data - 0xC is the cell, the shared empty one being 0x08D74FD0): the count at
// data - 4 goes down through 08B60804 and, when it reaches 0, 08B8542C frees the cell with `flag`'s address as the
// tag output. v0 is the last call's result; a skipped cell leaves v0 alone.
void release_cell(Runtime &rt, AllegrexContext &ctx, u32 data, u32 flag_address) {
    const u32 cell = data - 0xCu;
    if (cell == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) <= 0) {
        call(rt, ctx, 0x08B8542Cu, {cell, flag_address});
    }
}

// --- 0x08947F70 ---
// a0 = a car. Asks three per-gear objects for their ratios (08955300(car, 2) and (car, 3), skipping the gear whose
// +0x84 is zero, multiplying the +0x98 of the second answer by the +0x34 of the third), scales the global at
// 0x08CF2EB4 by the sum and hands it to 08947C74; then, from the gear count at +0x58E of the car's +8 object, the
// per-gear level (0x08CF2EA4 / 0x08CF2EB0 or 0x08CF2EAC) either drives 08947C04 and divides by its answer, or, when
// 08947E60's answer is not positive, drives 08947C74 again and multiplies. The answer is in $f0.
void func_08947F70(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 car = ctx.gpr[4];
    const float step = std::bit_cast<float>(0x3A832920u);   // 0.00100068
    const float scale = std::bit_cast<float>(0x46151B5Eu);  // 9542.84
    const u32 gears = word(rt, car + 8u);
    const float base = call_float(rt, ctx, 0x08947E60u);
    const float factor = call_float(rt, ctx, 0x0895549Cu, {car});
    float total = 0.0f;
    for (u32 gear = 2u; gear != 4u; ++gear) {
        const u32 entry = call(rt, ctx, 0x08955300u, {car, gear});
        if (float_at(rt, entry + 0x84u) == 0.0f) continue;
        const u32 second = call(rt, ctx, 0x08955300u, {car, gear});
        const float level = float_at(rt, second + 0x98u);  // the delay slot of the next jal
        const u32 third = call(rt, ctx, 0x08955300u, {car, gear});
        float value = level * float_at(rt, third + 0x34u);
        if (value < 0.0f) value = -value;
        total = total + value * step;
    }
    float ratio = float_at(rt, 0x08CF2EB4u);
    if (ratio < total) ratio = total;
    call_float(rt, ctx, 0x08947C74u, {car}, {ratio * factor});
    if (!(0.0f < base)) {
        // No positive answer from 08947E60: the level drives the writer with a negative sign and multiplies.
        const float count = static_cast<float>(signed_half(rt, gears + 0x58Eu));
        float level = (float_at(rt, 0x08CF2EA4u) * scale) / count;
        const float cap = float_at(rt, 0x08CF2EACu);
        if (cap < level) level = cap;
        float argument = ((level * count) / scale) * base;
        argument = -argument;
        call_float(rt, ctx, 0x08947C74u, {car}, {factor * argument});
        ctx.fpr[0] = level * base;
        return;
    }
    const float count = static_cast<float>(signed_half(rt, gears + 0x58Eu));
    float level = (float_at(rt, 0x08CF2EA4u) * scale) / count;
    const float cap = float_at(rt, 0x08CF2EB0u);
    if (cap < level) level = cap;
    const float held = level * base;                                // f21 in the asm
    const float argument = factor * (base * ((level * count) / scale));
    const float answer = call_float(rt, ctx, 0x08947C04u, {car}, {argument});
    ctx.fpr[0] = held * (answer / argument);
}

// --- 0x08A24E54 ---
// a0 = an object whose +0x58 stream exists (nothing happens, v0 = 0, when it is null): re-reads the stream's name
// (08A23838, 08ABBB60, 08B85EB8 into the frame) and assigns it to the two string cells at +0x64 and +0x68
// (08B89124, 08B855E0), giving each copy to a two-base temporary over +0x10 / +0x34 (08AC08D4, 08AC0B68), then
// releases the temporaries and the three frame cells from the last one back.
void func_08A24E54(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 self = ctx.gpr[4];
    if (word(rt, self + 0x58u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08A23838u);
    const u32 stream = word(rt, self + 0x58u);
    const u32 empty = 0x08D74FD0u;
    put(rt, frame.at(0x14u), empty + 0xCu);
    put(rt, frame.at(0x10u), empty + 0xCu);
    const u32 text = frame.at(0x18u);
    const u32 name = call(rt, ctx, 0x08ABBB60u, {stream});
    call(rt, ctx, 0x08B85EB8u, {text, name, frame.at(5u)});
    call(rt, ctx, 0x08B89124u, {frame.at(0xCu), self + 0x64u, text});
    call(rt, ctx, 0x08B855E0u, {frame.at(0x10u), frame.at(0xCu)});
    release_cell(rt, ctx, word(rt, frame.at(0xCu)), frame.at(4u));
    call(rt, ctx, 0x08B89124u, {frame.at(8u), self + 0x68u, text});
    call(rt, ctx, 0x08B855E0u, {frame.at(0x14u), frame.at(8u)});
    release_cell(rt, ctx, word(rt, frame.at(8u)), frame.at(3u));
    // The two two-base temporaries over self + 0x10 and self + 0x34 (0x08AB930C / 0x08AB91D8 per base pair).
    put(rt, frame.at(0x2Cu), 0x08AB930Cu);
    put(rt, frame.at(0x30u), self + 0x10u);
    put(rt, frame.at(0x34u), 0x08AB91D8u);
    put(rt, frame.at(0x38u), self + 0x10u);
    call(rt, ctx, 0x08AC08D4u, {word(rt, frame.at(0x10u)), frame.at(0x2Cu)});
    put(rt, frame.at(0x1Cu), 0x08AB930Cu);
    put(rt, frame.at(0x20u), self + 0x34u);
    put(rt, frame.at(0x24u), 0x08AB91D8u);
    put(rt, frame.at(0x28u), self + 0x34u);
    call(rt, ctx, 0x08AC08D4u, {word(rt, frame.at(0x14u)), frame.at(0x1Cu)});
    call(rt, ctx, 0x08AC0B68u);
    release_cell(rt, ctx, word(rt, frame.at(0x18u)), frame.at(2u));
    release_cell(rt, ctx, word(rt, frame.at(0x14u)), frame.at(1u));
    release_cell(rt, ctx, word(rt, frame.at(0x10u)), frame.at(0u));
}

// --- 0x08AE6BCC ---
// a0 = a list sentinel (its first word is rewritten), a1..a3 = the values, t0 = a count, t1 = a flag. Announces the
// request (08AE6E94), measures it (08AE69A0) and, with the measure's answer added to a3 when t1 is set, clamps the
// result per kind (a2: the twelve table entries at 0x08C919A8) into the node's +0x16. Equal to a3 answers 0
// (08AE6920 when t0 is not positive), otherwise a node is taken from the pool at 0x08D409C0, filled (+0x10 = a1,
// +0x14 = the kind as a half, +0x16 = the clamped half, +0x18 = a3, +0x1C = 0, the two floats) and linked last into
// a0's ring; v0 = the node, or 0.

// --- 0x08B7450C ---
// a0 = a 0x4E8-byte object (copied into the frame first; its +0x74 word decides an alternative set-up) and the
// output array of 0x133 words at +0..+0x4CC, a1 = a table of 0x133 8-byte entries and the words at +0x99C..+0x9A8.
// The table's kind (+0x9A8) is 1 (the entry at the byte table 0x08D72BB4 pointed at by +0x9A0 must be 4: the value
// becomes the frame copy's word at that index plus +0x99C) or 2 (+0x9A4 is passed on) or neither (08B77B04 announces
// it); the fallback reads a length through 08B727A4 and builds the value with 08B73ED0. Then every entry fills one
// output word: tag 2 copies the frame word it names, tag 1 adds +0 of the entry to the built value, tag 3 decodes
// and builds another. v0 is the last tag's leftover.

// --- 0x08C1CA2C ---
// a1 = the output bytes, a2 = the Base64 text, a3 = its length; a0 is not read. Decodes the leading quads while at
// least five characters remain, then the last quad with its '=' padding (only a length of exactly four reaches it).
// The 6-bit value of a character is the signed byte at 0x08CE129C + c (-1 when the character is not in the
// alphabet). v0 = the number of bytes written, or -1.
void func_08C1CA2C(Runtime &rt, AllegrexContext &ctx) {
    u32 output = ctx.gpr[5];
    u32 input = ctx.gpr[6];
    u32 remaining = ctx.gpr[7];
    u32 written = 0u;
    const auto value_of = [&rt](s32 c) { return static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(0x08CE129Cu + static_cast<u32>(c)))); };
    if (remaining >= 5u) {
        for (;;) {
            const s32 first = value_of(signed_byte(rt, input));
            if (first == -1) {
                ctx.gpr[2] = 0xFFFFFFFFu;
                return;
            }
            const s32 second = value_of(signed_byte(rt, input + 1u));
            if (second == -1) {
                ctx.gpr[2] = 0xFFFFFFFFu;
                return;
            }
            u32 bits = (static_cast<u32>(first) << 18) | (static_cast<u32>(second) << 12);
            const s32 third = value_of(signed_byte(rt, input + 2u));
            if (third == -1) {
                ctx.gpr[2] = 0xFFFFFFFFu;
                return;
            }
            bits |= static_cast<u32>(third) << 6;
            const s32 fourth = value_of(signed_byte(rt, input + 3u));
            if (fourth == -1) {
                ctx.gpr[2] = 0xFFFFFFFFu;
                return;
            }
            bits |= static_cast<u32>(fourth);
            rt.memory().store8(output, static_cast<std::uint8_t>(bits >> 16));
            rt.memory().store8(output + 1u, static_cast<std::uint8_t>(bits >> 8));
            rt.memory().store8(output + 2u, static_cast<std::uint8_t>(bits));
            remaining -= 4u;
            input += 4u;
            written += 3u;
            output += 3u;  // the asm's `t1 += 3` in the beqz's delay slot
            if (remaining < 5u) break;
        }
    }
    if (remaining != 4u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const s32 first = value_of(signed_byte(rt, input));
    if (first == -1) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const s32 second = value_of(signed_byte(rt, input + 1u));
    if (second == -1) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    u32 bits = (static_cast<u32>(first) << 18) | (static_cast<u32>(second) << 12);
    const s32 third_raw = signed_byte(rt, input + 2u);
    if (third_raw == 0x3D) {
        if (signed_byte(rt, input + 3u) != 0x3D) {
            ctx.gpr[2] = 0xFFFFFFFFu;
            return;
        }
        rt.memory().store8(output, static_cast<std::uint8_t>(bits >> 16));
        ctx.gpr[2] = written + 1u;
        return;
    }
    const s32 third = value_of(third_raw);
    if (third == -1) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    bits |= static_cast<u32>(third) << 6;
    const s32 fourth_raw = signed_byte(rt, input + 3u);
    if (fourth_raw == 0x3D) {
        rt.memory().store8(output, static_cast<std::uint8_t>(bits >> 16));
        ctx.gpr[2] = written + 2u;
        return;
    }
    const s32 fourth = value_of(fourth_raw);
    if (fourth == -1) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    bits |= static_cast<u32>(fourth);
    rt.memory().store8(output, static_cast<std::uint8_t>(bits >> 16));
    rt.memory().store8(output + 1u, static_cast<std::uint8_t>(bits >> 8));
    rt.memory().store8(output + 2u, static_cast<std::uint8_t>(bits));
    ctx.gpr[2] = written + 3u;
}

// --- 0x08BDF848 ---
// a0 = destination vector (begin/end/capacity at +0/+4/+8, 0x1C-byte elements), a1 = source: copy-assigns the source
// onto the destination, allocating a fresh buffer when the destination's capacity is shorter than the source and
// copy-assigning in place otherwise. A count is the compiler's `sra 2` and 0xB6DB6DB7 multiply's low word. v0 = a0.
void copy_element(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 source) {
    // The first word is stored before the calls (the asm's `jal` delay slot), then the two sub-objects are copied.
    put(rt, destination, word(rt, source));
    call(rt, ctx, 0x08BDDFDCu, {destination + 4u, source + 4u});
    call(rt, ctx, 0x08BDEC3Cu, {destination + 0x10u, source + 0x10u});
}

// --- 0x08A3C930 ---
// a0 and a1 are unused, a2 = 1, 2 or 3 (which of a3's three objects take part), a3 = three object pointers: builds a
// temporary at sp+0xC (08A3A9D0, 08A3A318, 08A3A320) and takes the code from 08A3A318 (kind 1) or from the slot 0x30
// code of a3 + 4 masked to a byte (kinds 2 and 3, kind 3 also reading the slot 0x54 float of a3 + 8), then calls
// 08A3A520 with the temporary's object, the word at sp+8 and that code. Two reference-counted temporaries (sp+4,
// sp+8) are released and the temporary destroyed. v0 is never written by the body.

// --- 0x08ACFAF0, 0x08ACFCF8 ---
// a0 = an object: its destructor body. Stores the vtable 0x08CD6B38, destroys the two runs of 0xD4-byte elements
// (from +0x378 down to +0x2A4 and from +0x1D0 down to +0xFC) through their vtable's first slot, calls 08B28DC4 on
// +0xB8 and the two 0x18-byte containers at +0xAC and +0x94 (each steps its +4 element pointer back, releasing the
// reference-counted element it names, then frees the lists behind its -8 node's +0x80 and +0x84), and tail-calls
// 0x08ABCF2C with a0 = the object.
void release_element(Runtime &rt, AllegrexContext &ctx, u32 element) {
    const u32 first = word(rt, element + 4u);
    put(rt, element + 4u, first - 1u);
    if (first != 1u) return;
    const u32 target = word(rt, word(rt, element) + 8u);
    call_through(rt, ctx, target, target, {element});
    const u32 second = word(rt, element + 8u);
    put(rt, element + 8u, second - 1u);
    if (second == 1u) {
        const u32 last = word(rt, word(rt, element) + 0xCu);
        call_through(rt, ctx, last, last, {element});
    }
}
void destroy_container(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 end = word(rt, address - 4u);
    for (;;) {
        if (word(rt, address + 4u) == end) break;
        const u32 current = word(rt, address + 4u);
        if (current == word(rt, address)) {
            const u32 block = word(rt, current + 0x80u);
            put(rt, address + 4u, block + 0x78u);
            put(rt, address, block);
        } else {
            put(rt, address + 4u, current - 8u);
        }
        const u32 element = word(rt, word(rt, address + 4u) + 4u);
        if (element != 0u) release_element(rt, ctx, element);
    }
    const u32 node = word(rt, address - 8u);
    put(rt, address + 8u, 0u);
    if (node == 0u) return;
    u32 link = word(rt, node + 0x80u);
    while (link != 0u) {
        const u32 next = word(rt, link + 0x80u);
        call(rt, ctx, 0x08B35254u, {link});
        link = next;
    }
    u32 entry = node;
    while (entry != 0u) {
        const u32 next = word(rt, entry + 0x84u);
        call(rt, ctx, 0x08B35254u, {entry});
        entry = next;
    }
}
void destroy_object(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x20u);
        put(rt, self, 0x08CD6B38u);
        call(rt, ctx, 0x08B1BFACu, {self + 0x5F8u});
        call(rt, ctx, 0x08B1BFACu, {self + 0x528u});
        call(rt, ctx, 0x08AFB0F8u, {self + 0x44Cu});
        // The two runs of 0xD4-byte elements, each destroyed from the top down.
        const u32 stop = self + 0x2A4u;
        if (stop != 0u && stop != self + 0x44Cu) {
            u32 element = self + 0x44Cu - 0xD4u;
            for (;;) {
                const u32 target = word(rt, word(rt, element));
                call_through(rt, ctx, target, target, {element});
                if (element == stop) break;
                element -= 0xD4u;
            }
        }
        const u32 start = self + 0xFCu;
        if (start != 0u && start + 0x1A8u != start) {
            u32 element = start + 0x1A8u - 0xD4u;
            for (;;) {
                const u32 target = word(rt, word(rt, element));
                call_through(rt, ctx, target, target, {element});
                if (element == start) break;
                element -= 0xD4u;
            }
        }
        call(rt, ctx, 0x08B28DC4u, {self + 0xB8u});
        // The two 0x18-byte containers, self + 0xAC and self + 0x94.
        const u32 marker_end = self + 0x88u;
        if (marker_end != 0u && marker_end != self + 0xB8u) {
            u32 marker = self + 0xB8u;
            u32 container = self + 0xACu;
            for (;;) {
                marker -= 0x18u;  // the asm steps it before looking at the container
                destroy_container(rt, ctx, container);
                if (marker == marker_end) break;
                container -= 0x18u;
            }
        }
    }
    tail_call(ctx, 0x08ABCF2Cu, {self});
}

// --- 0x0899CB44 ---
// a0 = a vector object: its virtual +0x10C must answer non-zero and its +0x24 an object. Builds the array of
// elements + 1 words at sp+0x14 (element count of the +0x28 object's {+0x14, +0x18} range, first word from the
// object's +0x1C through 08A76AAC, the rest copied), hands it to +0x24's virtual +0xCC and releases the two
// temporaries and the buffer. v0 = 1 on every path.
void func_0899CB44(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 self = ctx.gpr[4];
    const u32 entry_vtable = word(rt, self);
    const u32 entry_target = word(rt, entry_vtable + 0x10Cu);
    if (call_through(rt, ctx, entry_target, entry_target, {self}) == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (word(rt, self + 0x24u) == 0u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 holder = word(rt, self + 0x28u);
    if (holder == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA483Cu});
    const u32 elements = static_cast<u32>(sar(static_cast<s32>(word(rt, holder + 0x18u) - word(rt, holder + 0x14u)), 2));
    const u32 count = elements + 1u;
    const u32 bytes = count << 2;
    put(rt, frame.at(0x14u), 0u);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    const u32 block = call(rt, ctx, 0x08A5BC20u, {bytes, 4u});
    put(rt, frame.at(0x1Cu), block + bytes);
    put(rt, frame.at(0x14u), block);
    put(rt, frame.at(0x18u), block);  // the delay slot of the jal
    call(rt, ctx, 0x08A76AA0u, {frame.at(4u)});
    call(rt, ctx, 0x08B85AF4u, {block, count, frame.at(4u), frame.at(1u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    const u32 begin = word(rt, frame.at(0x14u));
    put(rt, frame.at(0x18u), begin + bytes);
    put(rt, frame.at(8u), word(rt, self + 0x1Cu));  // the delay slot of the jal
    call(rt, ctx, 0x08A76AACu, {frame.at(0xCu), frame.at(8u)});
    if (begin != frame.at(0xCu)) call(rt, ctx, 0x08A5BAD8u, {begin, word(rt, frame.at(0xCu))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    for (u32 index = 0u; index < elements; ++index) {
        const u32 to = word(rt, frame.at(0x14u)) + 4u * (index + 1u);
        const u32 from = word(rt, word(rt, self + 0x28u) + 0x14u) + 4u * index;
        if (to != from) call(rt, ctx, 0x08A5BAD8u, {to, word(rt, from)});
    }
    const u32 holder_object = frame.at(0x10u);
    call(rt, ctx, 0x08A76AA0u, {holder_object});
    const u32 target_object = word(rt, self + 0x24u);
    if (target_object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA47D4u});
    const u32 begin_now = word(rt, frame.at(0x14u));
    const u32 end_now = word(rt, frame.at(0x18u));
    const u32 target_vtable = word(rt, target_object);
    const u32 target = word(rt, target_vtable + 0xCCu);
    call_through(rt, ctx, target_vtable, target,
                 {target_object, holder_object, static_cast<u32>(sar(static_cast<s32>(end_now - begin_now), 2)), begin_now});
    call(rt, ctx, 0x08A5B9ECu, {holder_object});
    call(rt, ctx, 0x08B856B0u, {word(rt, frame.at(0x14u)), word(rt, frame.at(0x18u)), frame.at(0u)});
    const u32 free_begin = word(rt, frame.at(0x14u));
    const u32 free_bytes = static_cast<u32>(sar(static_cast<s32>(word(rt, frame.at(0x1Cu)) - free_begin), 2)) << 2;
    if (free_begin != 0u) call(rt, ctx, 0x08A5BC54u, {free_begin, free_bytes, 4u});
    ctx.gpr[2] = 1u;
}

// --- 0x089CA474 ---
// a0 = the output string cell, a1 = a name, a2 = 1 (a fixed text, 08B8A4C0 with 0x08CA668C and 8) or 2 (a string
// from a3 + 4's virtual +0xC, appended with 08B855E0), a3 = the source object. Continues only while the cell at
// sp+0xC holds a non-empty string (word at cell - 0xC): builds two temporaries (089C5850, 089BAC14), a third from
// the source's virtual +0xC, joins them with 089CA288 into the output cell and releases everything. v0 is the last
// release's answer.
void func_089CA474(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x50u);
    const u32 out = ctx.gpr[4];
    const u32 name = ctx.gpr[5];
    const u32 kind = ctx.gpr[6];
    const u32 source = ctx.gpr[7];
    const u32 empty = 0x08D74FD0u;
    put(rt, frame.at(0xCu), empty + 0xCu);
    if (kind == 1u) {
        call(rt, ctx, 0x08B8A4C0u, {frame.at(0xCu), 0x08CA668Cu, 8u});
    } else if (kind == 2u) {
        const u32 inner = word(rt, source + 4u);
        if (inner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
        const u32 vtable = word(rt, inner);
        const u32 target = word(rt, vtable + 0xCu);
        call_through(rt, ctx, vtable, target, {frame.at(8u), inner});
        call(rt, ctx, 0x08B855E0u, {frame.at(0xCu), frame.at(8u)});
        release_cell(rt, ctx, word(rt, frame.at(8u)), frame.at(2u));
    }
    const u32 handle = word(rt, frame.at(0xCu));
    if (word(rt, handle - 0xCu) == 0u) {
        ctx.gpr[2] = 0u;
    } else {
        const u32 text = frame.at(0x10u);
        const u32 value = frame.at(0x14u);
        call(rt, ctx, 0x089C5850u, {text, name});
        call(rt, ctx, 0x089BAC14u, {value});
        const u32 first = word(rt, text);
        if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA63A0u});
        const u32 inner = word(rt, source);
        if (inner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
        const u32 vtable = word(rt, inner);
        const u32 target = word(rt, vtable + 0xCu);
        call_through(rt, ctx, vtable, target, {frame.at(4u), inner});
        call(rt, ctx, 0x089CA288u, {first, value, frame.at(4u), frame.at(0xCu)});
        release_cell(rt, ctx, word(rt, frame.at(4u)), frame.at(1u));
        if (out != value) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, value)});
        call(rt, ctx, 0x08A5B9ECu, {value});
        call(rt, ctx, 0x08A5B9ECu, {text});
    }
    release_cell(rt, ctx, word(rt, frame.at(0xCu)), frame.at(0u));
}

} // namespace
} // namespace new_batch_main_203
} // namespace gt::game

#define MAIN203(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_203::name, gt::ReturnKind::kind)
MAIN203(0x08947F70u, func_08947F70, Float);
MAIN203(0x0899CB44u, func_0899CB44, Word);
MAIN203(0x089CA474u, func_089CA474, Word);
MAIN203(0x08A24E54u, func_08A24E54, Word);
MAIN203(0x08C1CA2Cu, func_08C1CA2C, Word);

// ---- from main-session batch 204 (draft be0b6b6add57fdc351f5367ee5d196ede331a4d7) ----
// New batch 204 (main session): emitted functions no src/ file defines or registers, written by hand
// from `tools/disasm` as structured C. Draft batch: no gate was run here (a separate pass verifies),
// so no name has evidence and every function keeps func_XXXXXXXX.
//
// 0x08B754C4, 0x08B75710 and 0x08B75B04 are left out on purpose: they build a 0xA50-byte record
// through 0x08B7470C (which stores sp into the record at +0x74 and ra into +0x4D0), snapshot it,
// compare it with 0x08B73D9C and then reload ra from the snapshot's +0x4D0 — the save/restore shape
// the decomp workflow excludes. 0x08C2E514 (a gcd-cycle rotate whose branch targets enter the middle of
// three loops) and 0x08C76B7C (eight labels that jump into the middle of one another, reading s5 before
// it is set on the first pass) are left for a pass that can afford to be wrong.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_204 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same with float arguments ($f12, $f13, ...).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
s32 signed_half(u32 value) { return static_cast<std::int16_t>(value); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }

// ---------------------------------------------------------------------------------------------------
// 0x089F3D20 (a0 = object): stores the two vtables (at +0 and +0x38), destroys the eight temporaries at
// +0x24C..+0x268 through 0x08A5B9EC, then releases the eight holders at +0x22C..+0x248: a holder whose
// "empty" sentinel is 0x08D74FD0 is skipped, one that 0x08B60804 takes (result > 0) is skipped, and the
// rest go to 0x08B8542C with a scratch byte of the frame (sp+7 down to sp+0). v0 = 0x089E2430's result.

// ---------------------------------------------------------------------------------------------------
// The red-black tree of 0x08C34ABC. A node holds the children at +8 (left) and +0xC (right), the string
// at +0x10 and the secondary key (an unsigned word) at +0x14; the string's length sits at -0xC of the
// string pointer, as the stdio/container code of this binary keeps it. The tree object holds the root
// at +8 and the leftmost node at +0xC.
//
// The string comparison only runs when the secondary keys are equal, and compares the shorter of the two
// strings through 0x08B36170; when that answers 0 the length difference decides. Both lengths travel
// through sp+0 and sp+4 exactly as the asm selects between them (movn in the first test, movz in the
// second), so the two slots are written and read back.
bool key_string_less(Runtime &rt, AllegrexContext &ctx, u32 frame_base, u32 key, u32 node) {
    const u32 key_string = word(rt, key);
    const u32 key_length = word(rt, key_string - 0xCu);
    put(rt, frame_base + 4u, key_length);
    const u32 node_string = word(rt, node + 0x10u);
    const u32 node_length = word(rt, node_string - 0xCu);
    put(rt, frame_base, node_length);
    const u32 shortest = node_length < key_length ? word(rt, frame_base) : word(rt, frame_base + 4u);
    const s32 compared = static_cast<s32>(call(rt, ctx, 0x08B36170u, {key_string, node_string, shortest}));
    const s32 difference = compared != 0 ? compared : static_cast<s32>(key_length - node_length);
    return difference < 0;
}
// The second test, after the descent: node < key, the string compared the other way round.
bool node_below_key(Runtime &rt, AllegrexContext &ctx, u32 frame_base, u32 node, u32 key, u32 key_secondary) {
    const u32 node_secondary = word(rt, node + 0x14u);
    if (node_secondary < key_secondary) return true;
    if (key_secondary < node_secondary) return false;
    const u32 node_string = word(rt, node + 0x10u);
    const u32 node_length = word(rt, node_string - 0xCu);
    put(rt, frame_base, node_length);
    const u32 key_string = word(rt, key);
    const u32 key_length = word(rt, key_string - 0xCu);
    put(rt, frame_base + 4u, key_length);
    const u32 shortest = key_length < node_length ? word(rt, frame_base + 4u) : word(rt, frame_base);
    const s32 compared = static_cast<s32>(call(rt, ctx, 0x08B36170u, {node_string, key_string, shortest}));
    const s32 difference = compared != 0 ? compared : static_cast<s32>(node_length - key_length);
    return difference < 0;
}

// 0x08C34ABC (a0 = tree, a1 = key record: string pointer at +0, secondary key at +4): the insert-unique
// position search followed by the insert. v0 = the node (the found one, or the new one), v1 = the word
// the asm reads back from the byte it stores at sp+0xC (1 inserted, 0 not).
// When the tree is empty the asm reads s0 without ever setting it: that flag is the caller's s0.
// The descent ends where the walk found a null child (incoming s0 on the empty tree, otherwise key <
// node for the left child and node <= key for the right one); a "less" position first steps back to the
// node's predecessor (0x08B5F174) unless the node is the leftmost, which inserts directly.
void func_08C34ABC(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x40u);
    const u32 frame_base = frame.at(0);
    const u32 tree = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 key_secondary = word(rt, key + 4u);
    u32 node = word(rt, tree + 8u);
    bool less = ctx.gpr[16] != 0u;
    u32 last = tree + 4u;
    while (node != 0u) {
        const u32 node_secondary = word(rt, node + 0x14u);
        if (key_secondary < node_secondary) less = true;
        else if (node_secondary < key_secondary) less = false;
        else less = key_string_less(rt, ctx, frame_base, key, node);
        const u32 child = word(rt, node + (less ? 8u : 0xCu));
        if (child == 0u) {
            last = node;
            break;
        }
        node = child;
    }
    u32 current = last;
    bool insert;
    if (!less) {
        insert = node_below_key(rt, ctx, frame_base, last, key, key_secondary);
    } else if (last == word(rt, tree + 0xCu)) {
        insert = true;
    } else {
        current = call(rt, ctx, 0x08B5F174u, {last});
        insert = node_below_key(rt, ctx, frame_base, current, key, key_secondary);
    }
    u32 result = current;
    if (insert) {
        result = call(rt, ctx, 0x08C34990u, {tree, 0u, last, key});
        put_byte(rt, frame.at(0xCu), 1u);
    } else {
        put_byte(rt, frame.at(0xCu), 0u);
    }
    ctx.gpr[2] = result;
    ctx.gpr[3] = word(rt, frame.at(0xCu));
}

// ---------------------------------------------------------------------------------------------------
// 0x088AC6DC (a0 = self, a1 = other, f0 ignored): 1 when the segment from self to other crosses the ring
// of eight 0x44-byte records each object holds from +0x64 (the pairs wrap around: record 7 to record 0
// first), 0 otherwise. The first two tests cull on the height (|self+0x10 - other+0x10| <= 4.0) and on
// the radius sum at +0x3C4 against the horizontal distance. The ring walk keeps the furthest crossing
// (f18/f19/f21), halves it and hands both objects the two float arguments 0x088AC66C takes.
// Only the last comparison's sign survives each step, as in the asm.
void func_088AC6DC(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    const float height = float_at(rt, self + 0x10u) - float_at(rt, other + 0x10u);
    if (4.0f < std::fabs(height)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const float other_z = float_at(rt, other + 8u);
    const float self_z = float_at(rt, self + 8u);
    const float self_x = float_at(rt, self + 0xCu);
    const float other_x = float_at(rt, other + 0xCu);
    const float radius_self = float_at(rt, self + 0x3C4u);
    const float delta_z = other_z - self_z;
    const float radius_other = float_at(rt, other + 0x3C4u);
    const float delta_x = other_x - self_x;
    const float squared_z = delta_z * delta_z;
    const float squared_x = delta_x * delta_x;
    const float radius_sum = radius_self + radius_other;
    const float distance = std::sqrt(squared_z + squared_x);
    if (radius_sum < distance) {
        ctx.gpr[2] = 0u;
        return;
    }
    float best_distance = 0.0f;
    float best_x = 0.0f;
    float best_y = 0.0f;
    u32 other_record = other + 0x64u;
    for (u32 outer = 0u; outer < 8u; ++outer, other_record += 0x44u) {
        const float point_x = float_at(rt, other_record + 0x10u);
        const float point_y = float_at(rt, other_record + 0x14u);
        float near_x = 0.0f;
        float near_y = 0.0f;
        float near_distance = 0.0f;
        u32 a = self + 0x240u;
        u32 b = self + 0x64u;
        bool exhausted = true;
        for (u32 inner = 0u; inner < 8u; ++inner) {
            const float a_x = float_at(rt, a + 0x10u);
            const float a_y = float_at(rt, a + 0x14u);
            const float b_x = float_at(rt, b + 0x10u);
            const float b_y = float_at(rt, b + 0x14u);
            const float point_to_a_y = point_y - a_y;
            const float point_to_a_x = point_x - a_x;
            const float a_to_b_x = b_x - a_x;
            const float a_to_b_y = b_y - a_y;
            const float side_1 = a_to_b_x * point_to_a_y;
            const float side_2 = a_to_b_y * point_to_a_x;
            const float side_3 = delta_z * point_to_a_y;
            const float side_4 = delta_x * point_to_a_x;
            const float first_cross = side_1 - side_2;
            const float second_cross = side_3 - side_4;
            if (first_cross < 0.0f) {
                // Out of the inner ring: the asm's jump skips the best-value update below.
                exhausted = false;
                break;
            }
            if (second_cross < 0.0f) {
                // The asm tests its "already crossing" flag here but both answers continue the walk.
                a = b;
                b += 0x44u;
                continue;
            }
            const float point_to_b_y = point_y - b_y;
            const float point_to_b_x = point_x - b_x;
            const float third_cross = delta_z * point_to_b_y - delta_x * point_to_b_x;
            if (0.0f < third_cross) {
                a = b;
                b += 0x44u;
                continue;
            }
            const float denominator = second_cross - third_cross;
            const float scale = denominator != 0.0f ? second_cross / denominator : 0.0f;
            const float offset_y = scale * a_to_b_y - point_to_a_y;
            const float offset_x = scale * a_to_b_x - point_to_a_x;
            near_y = offset_y;
            near_x = offset_x;
            near_distance = delta_z * offset_x + delta_x * offset_y;
            a = b;
            b += 0x44u;
        }
        if (exhausted && best_distance < near_distance) {
            best_y = near_y;
            best_x = near_x;
            best_distance = near_distance;
        }
    }
    if (!(0.0f < best_distance)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const float half = 0.5f;
    const float scaled_y = best_y * half;
    const float scaled_x = best_x * half;
    call_keepf(rt, ctx, 0x088AC66Cu, {other}, {scaled_x, scaled_y});
    call_keepf(rt, ctx, 0x088AC66Cu, {self}, {-scaled_x, -scaled_y});
    ctx.gpr[2] = 1u;
}

// ---------------------------------------------------------------------------------------------------
// 0x08B42E74 (a0 = object, a1-a3 = three halves, t0/t1 = two words, t2 = a pointer or 0, t3 = a byte,
// the first stack argument = a float's bits): the diagnostic printer. The object's vtable slot 0x98
// answers a count (below 2 gives 0); 0x08B3EBD0(object + 0x154, a2) finds a description whose +0 is the
// bytes to copy out of t2 into a 0x18-byte header; then 0x08B42A90 or 0x08B411D4 (picked by the
// stack argument's sign) prints the header and the copied text.
// v0 = 1 when the chosen printer answered non-zero, 0 otherwise.

// ---------------------------------------------------------------------------------------------------
// 0x08B7B3C0 (a0 = stdio record, a1 = buffer or 0, a2 = mode, a3 = size): setvbuf over the record the
// stdio globals point at: +0/+0x10 the buffer, +4 read count, +8 write count, +0xC the flags (0x80 =
// the buffer is ours, 0x8 = unbuffered-ish, 0x1/0x2 the mode bits), +0x14 the size, +0x18 the line
// buffer size, +0x43 the one-byte buffer. The globals' +0x38 slot decides whether stdio is set up
// (0x08B78430 when it is not); the globals' +0x3C receives 0x08B785A4 on the buffer path.
// Modes 3 and up and a negative size answer -1; a mode-2 record works on the one-byte buffer; a 0
// buffer allocates (size, or 0x400 when 0, and 0x400 again when that fails, which answers -1).
// v0 = 0 on success, -1 on the failures above.
void func_08B7B3C0(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 record = ctx.gpr[4];
    u32 buffer = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    u32 size = ctx.gpr[7];
    u32 globals = call(rt, ctx, 0x08B796C4u, {});
    if (word(rt, globals + 0x38u) == 0u) {
        globals = call(rt, ctx, 0x08B796C4u, {});
        call(rt, ctx, 0x08B78430u, {globals});
    }
    if (mode >= 3u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    if (static_cast<s32>(size) < 0) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    call(rt, ctx, 0x08B7807Cu, {record});
    u32 flags = rt.memory().load16(record + 0xCu);
    put(rt, record + 4u, 0u);
    put(rt, record + 0x18u, 0u);
    if ((flags & 0x80u) != 0u) {
        // The record owns its buffer: release it and forget it.
        globals = call(rt, ctx, 0x08B796C4u, {});
        call(rt, ctx, 0x08B354A4u, {globals, word(rt, record + 0x10u)});
        flags = rt.memory().load16(record + 0xCu);
        put(rt, record + 0x10u, 0u);
    }
    flags &= 0xFFFFFF7Cu;  // the asm's mask: -0x84
    rt.memory().store16(record + 0xCu, flags);
    if (mode == 2u) {
        // The one-byte buffer at +0x43.
        rt.memory().store16(record + 0xCu, flags | 2u);
        put(rt, record + 0x14u, 1u);
        put(rt, record + 0x10u, record + 0x43u);
        put(rt, record + 8u, 0u);
        put(rt, record, record + 0x43u);
        ctx.gpr[2] = 0u;
        return;
    }
    if (buffer == 0u) {
        const u32 wanted = size != 0u ? size : 0x400u;
        u32 block = call(rt, ctx, 0x08B35DA0u, {wanted});
        if (block == 0u) {
            block = call(rt, ctx, 0x08B35DA0u, {0x400u});
            if (block == 0u) {
                rt.memory().store16(record + 0xCu, flags | 2u);
                put(rt, record + 0x14u, 1u);
                put(rt, record + 0x10u, record + 0x43u);
                put(rt, record + 8u, 0u);
                put(rt, record, record + 0x43u);
                ctx.gpr[2] = 0xFFFFFFFFu;
                return;
            }
            size = 0x400u;
        }
        buffer = block;
        flags = rt.memory().load16(record + 0xCu) | 0x80u;
        rt.memory().store16(record + 0xCu, flags);
    }
    if (mode != 0u) {
        // Line buffered: the count at +0x18 is minus the size, and bit 0 goes on.
        put(rt, record + 0x18u, buffer != 0u ? 0u - size : 0u);
        flags |= 1u;
        rt.memory().store16(record + 0xCu, flags);
    }
    // The buffer goes in place through the stdio globals.
    globals = call(rt, ctx, 0x08B796C4u, {});
    flags = rt.memory().load16(record + 0xCu);
    put(rt, globals + 0x3Cu, 0x08B785A4u);
    put(rt, record + 0x10u, buffer);
    put(rt, record, buffer);
    put(rt, record + 0x14u, size);
    if ((flags & 8u) != 0u) put(rt, record + 8u, (flags & 3u) == 0u ? size : 0u);
    ctx.gpr[2] = 0u;
}

// ---------------------------------------------------------------------------------------------------
// A shared-count release (the std::__shared_count shape of this binary): the strong count at +4 goes
// down by one and, when it was the last one, the block's vtable slot 8 runs; then the weak count at +8
// the same way with slot 0xC. The asm does the decrement with ll/sc and retries; here nothing else can
// run between the two instructions of a case, so the retry writes old - 1 once.
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 block) {
    const u32 use_count = word(rt, block + 4u);
    put(rt, block + 4u, use_count - 1u);
    if (use_count != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, block) + 8u), {block});
    const u32 weak_count = word(rt, block + 8u);
    put(rt, block + 8u, weak_count - 1u);
    if (weak_count == 1u) call_indirect(rt, ctx, word(rt, word(rt, block) + 0xCu), {block});
}

// 0x08ACF5DC (a0 = object): the object's destructor. The vtable 0x08CD6B38 goes in at +0, the parts at
// +0x5F8 and +0x528 are torn down through 0x08B1BFAC and the one at +0x44C through 0x08AFB0F8; then the
// two 0xD4-byte blocks of [+0x2A4, +0x44C) and the two of [+0xFC, +0x2A4) go through their own vtable
// slot 0, each run from the last one backwards. 0x08B28DC4(object + 0xB8) follows, then the two 0x18-byte
// chunk records at +0x88 and +0xA0 are emptied: the cursor at +4 walks back 8 bytes at a time (down a
// chunk through the record's +0x80/+0x78 pair when it meets the record's own cursor) releasing each
// shared block, and the block at -8 is freed with its +0x80 and +0x84 chains through 0x08B35254.
// 0x08ABCF2C(object) runs last and the function ends in the tail call to 0x08B64654.

// ---------------------------------------------------------------------------------------------------
// 0x08AD45F4 (a0 = object, a1 and a2 = the two words the tail virtual gets after a0): resets the object
// and starts a turn-in animation. The VFPU's matrix 4 (C400, registers 16-19) is the compiler's scratch:
// it is saved at sp+0x40, zeroed for the twelve words the object gets at +0, +0x10 and +0x20 and put back
// at the end. +0x30 is cleared, +0x34/+0x38 take the two floats the table record at
// +0x9C + 0x10 * byte(+0x85) gives (an angle in turns through the small-angle series, then one minus it
// times the record's +0xC), 0x08AD7334 fills the three floats at sp and 0x08AD791C computes the rotation
// from the record's +4 and +0xC. The two rotation columns then go out as VFPU columns: C000 (registers
// 0-3 = the three stack floats and 0) at +0x70 and C010 (registers 1-4, negated) at +0x60, +0x40 and
// +0x50 zeroed, +0x80 takes the vtable entry the call goes through.
// v0 = the virtual's result.

} // namespace
} // namespace new_batch_main_204
} // namespace gt::game

#define NEW204(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_204::name, gt::ReturnKind::kind)
NEW204(0x08B7B3C0u, func_08B7B3C0, Word);
NEW204(0x088AC6DCu, func_088AC6DC, Word);
NEW204(0x08C34ABCu, func_08C34ABC, DoubleWord);

