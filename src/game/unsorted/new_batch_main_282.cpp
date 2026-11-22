// ---- from main-session batch 223 (draft c228d1b43862a03fd11a78cc1db4368ed9026b92) ----
// New batch main 223: functions the synthetic gate could not run for lack of valid inputs. Written by hand from
// `tools/disasm` as structured C; every delay slot is read as "before the call / branch", every argument register a
// callee reads is passed explicitly (a register the asm leaves as the previous callee's leftover is passed as it
// stands), and `v0` ends where the asm leaves it. Input shapes for the synthetic generator:
// /tmp/claude-1000/shapes_main223.txt. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_223 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using s64 = std::int64_t;

inline void take(AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call whose results stay in the registers like the original's: v0 / v1 / f0 of the callee are what the next
// instruction sees.
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    take(ctx, result);
    return result;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order); the
// registers the list does not name keep what the previous callee left, as in the asm.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
inline u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
inline u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
inline void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
inline void put16(Runtime &rt, u32 address, u32 value) {
    rt.memory().store16(address, static_cast<std::uint16_t>(value));
}
inline u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
inline u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
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

// Releases one use of a counted object: the count at +4 goes down; on the last use the object's virtual slot 4 runs
// (a0 = the object, v1 = its vtable, v0 = the slot, exactly as the callers load them).
inline void release_counted(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    const u32 count = word(rt, object + 4u) - 1u;
    put(rt, object + 4u, count);
    if (count != 0u) return;
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 4u);
    call_set(rt, ctx, ctx.gpr[2], {object});
}

// 0x08AE7618 (object = a0, context = a1, kind = a2, mode = a3, first = t0, count = t1, second = t2, third = t3):
// looks a record up through 0x08ADBE98(kind, mode, &sp+0x60, mode); a null record answers 0 (a0, which the epilogue
// returns). The record's word +0 is a flag: bit 0 set means a4 = 1 and the entries come from 0x08AE70C0's conversion
// table, clear means a4 = the signed byte at +9 (or 1 when the signed byte at +8 is 4). The 0x5C-byte block at sp is
// built from the record (+8 type, +0x10/+0x14/+0x1C/+0x20/+0x24/+0x28/+0x2C words), the object's +0x10 node (+8 word,
// +0x11 flags: 0x10 swaps the two +0x38/+0x3C words, 0x20 sets both to -1) and the arguments: +4 = first, +8 =
// count | 0x08000000 when count is 128 or more, 0 when it is not positive, else (kind << 16) | count | 0x04000000,
// +0x0C = second, +0x10 = third, +0x14/+0x4C = the node's signed halves +0x20/+0x22, +0x50 = bit 6 of the flags.
// 0x08AE70C0(object, context, kind, a4, block) builds the result (0 answers 0); its +0x3C word is the record's +0x30;
// then, once per entry, +0x2C (the record's +0x30+4i word) is set and the entry's signed half at +0x30+2i is replayed
// through 0x08AEF874(half, 0, block, 0) and 0x08AEF874(half, 7, 0, 0). The record's +0x18/+0x1A halves go to the
// result's +0x48/+0x4A, +0x4C gets (word +0x0C << 12) / 44100 (the compiler's magic 0x2F8DF18F, shift 13), and a
// record flagged 4 first goes through 0x08ADCEC4(kind, mode, result + 0x30) whose answer lands at +0x2C; finally
// +0x54/+0x58 take the two swapped words, +0x28 the record, and the result comes back.
void func_08AE7618(Runtime &rt, AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 context = ctx.gpr[5];
    const u32 kind = ctx.gpr[6];
    const u32 mode = ctx.gpr[7];
    const u32 first = ctx.gpr[8];
    const u32 count = ctx.gpr[9];
    const u32 second = ctx.gpr[10];
    const u32 third = ctx.gpr[11];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0xA0u);
    ctx.gpr[16] = count;    // s0
    ctx.gpr[18] = first;    // s2
    ctx.gpr[19] = second;   // s3
    ctx.gpr[21] = third;    // s5
    ctx.gpr[22] = kind;     // s6
    put(rt, frame.at(0x64u), object);
    put(rt, frame.at(0x68u), context);
    put(rt, frame.at(0x6Cu), mode);
    const u32 record = call_set(rt, ctx, 0x08ADBE98u, {kind, mode, frame.at(0x60u), mode});
    ctx.gpr[20] = record;   // s4
    if (record == 0u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    u32 flag_step = 0u;    // $fp: added to the record index every entry
    u32 emit_state = 1u;   // s7: which of the two reference pairs the 0x80 case expects next
    u32 value;             // a0, then s1: the +0x20 word of the block and the entry count
    u32 entries;           // s1
    if ((word(rt, record) & 1u) == 0u) {
        value = sext8(byte_u(rt, record + 9u));
        entries = sext8(byte_u(rt, record + 8u)) == 4u ? 1u : value;
        flag_step = 0u;
        emit_state = 1u;
    } else {
        value = 1u;
        entries = sext8(byte_u(rt, record + 9u));
        flag_step = 1u;
        emit_state = 0u;
    }
    const u32 node = word(rt, word(rt, frame.at(0x64u)) + 0x10u);
    put(rt, frame.at(4u), first);
    put(rt, frame.at(0u), word(rt, node + 8u));
    if (!(signed_of(count) < 0x80)) {
        put(rt, frame.at(8u), count | 0x08000000u);
    } else if (signed_of(count) <= 0) {
        put(rt, frame.at(8u), 0u);
    } else {
        put(rt, frame.at(8u), (kind << 16) | count | 0x04000000u);
    }
    put(rt, frame.at(0x0Cu), second);
    put(rt, frame.at(0x10u), third);
    put(rt, frame.at(0x14u), sext16(half_u(rt, node + 0x20u)));
    put(rt, frame.at(0x20u), value);
    put(rt, frame.at(0x18u), sext8(byte_u(rt, record + 8u)));
    put(rt, frame.at(0x24u), word(rt, record + 0x10u));
    put(rt, frame.at(0x28u), word(rt, record + 0x14u));
    put(rt, frame.at(0x30u), word(rt, record + 0x1Cu));
    const u32 record20 = word(rt, record + 0x20u);
    put(rt, frame.at(0x34u), record20);
    put(rt, frame.at(0x38u), word(rt, record + 0x24u));
    put(rt, frame.at(0x40u), 0u);
    put(rt, frame.at(0x3Cu), word(rt, record + 0x28u));
    put(rt, frame.at(0x48u), word(rt, frame.at(0x60u)));
    put(rt, frame.at(0x44u), word(rt, record + 0x2Cu));
    put(rt, frame.at(0x4Cu), sext16(half_u(rt, node + 0x22u)));
    const u32 flags = byte_u(rt, node + 0x11u);
    if ((flags & 0x10u) != 0u) {
        put(rt, frame.at(0x3Cu), record20);
        put(rt, frame.at(0x38u), 0u);
    }
    if ((flags & 0x20u) != 0u) {
        put(rt, frame.at(0x3Cu), 0xFFFFFFFFu);
        put(rt, frame.at(0x38u), 0xFFFFFFFFu);
    }
    put(rt, frame.at(0x50u), (flags >> 6) & 1u);
    const u32 result = call_set(rt, ctx, 0x08AE70C0u, {object, context, kind, entries, frame.at(0u)});
    ctx.gpr[21] = result;   // s5
    if (result == 0u) {
        ctx.gpr[4] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    put(rt, result + 0x3Cu, word(rt, frame.at(0x30u)));
    u32 remaining = entries;   // s1
    u32 step = 0u;             // s3
    u32 index = 0u;            // s2
    u32 slot = result + 0x30u; // s0
    while (signed_of(remaining) > 0) {
        put(rt, frame.at(0x1Cu), step);
        put(rt, frame.at(0x2Cu), word(rt, record + 0x30u + (index << 2)));
        remaining -= 1u;
        step += emit_state;
        index += flag_step;
        ctx.gpr[17] = remaining;   // s1
        ctx.gpr[19] = step;        // s3
        ctx.gpr[18] = index;       // s2
        call_set(rt, ctx, 0x08AEF874u, {sext16(half_u(rt, slot + 8u)), 0u, frame.at(0u), 0u});
        call_set(rt, ctx, 0x08AEF874u, {sext16(half_u(rt, slot + 8u)), 7u, 0u, 0u});
        slot += 2u;
    }
    put16(rt, result + 0x48u, half_u(rt, record + 0x18u));
    put16(rt, result + 0x4Au, half_u(rt, record + 0x1Au));
    const s32 shifted = static_cast<s32>(word(rt, record + 0x0Cu) << 12);
    const s32 high = static_cast<s32>((static_cast<s64>(shifted) * 0x2F8DF18Fu) >> 32);
    put16(rt, result + 0x4Cu, static_cast<u32>((high >> 13) - (shifted >> 31)));
    if ((word(rt, record) & 4u) != 0u) {
        const u32 converted = call_set(rt, ctx, 0x08ADCEC4u, {kind, mode, result + 0x30u});
        put(rt, result + 0x2Cu, converted);
    }
    put(rt, result + 0x54u, word(rt, frame.at(0x38u)));
    put(rt, result + 0x28u, record);
    put(rt, result + 0x58u, word(rt, frame.at(0x3Cu)));
    ctx.gpr[4] = result;
    ctx.gpr[2] = result;
}

// 0x08B53258 (self = a0, list = a1, scale = a2, argument = a3, index_base = t0, stride = t1): rewrites the self + 4
// table of self + 0xC entries, 8 bytes each: the first byte is the kind, the upper 24 bits of the word are an offset
// into the self + 0 block, and the target is that base plus the offset. Per kind, with value = word(entry + 4):
//   0: the pair (0x10000000 with bits 16-23 from (base+value) >> 24, 0x01000000 with the low 24 bits);
//   1: the same with 0x0A000000, of scale + argument * value;
//   2: skipped when list is null; word(list) (null: skipped) goes through 0x08B531FC(word(list), value), or
//      word(index_base + value * stride) when index_base is set, into the 0x0A000000 pair;
//   3: the 0x09000000 pair of base + value;
//   0x80: skipped when scale is 0; scale + value * argument is read at +0x18: bit 1 set gives a 0x08CE0DB8 pair when
//      the last 0x80 entry did not give one, else two zero words (state = 1); bit 1 clear gives the 0x08CE0DA8 pair
//      when the last one did (state = 0), else two zero words;
//   0x81: skipped when scale is 0; the bit-1-clear handling of 0x80 without reading the record.
// Anything else leaves the entry alone. The count at self + 0xC is re-read after every entry.
void func_08B53258(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];        // s1
    const u32 list = ctx.gpr[5];        // s6
    const u32 scale = ctx.gpr[6];       // s3
    const u32 argument = ctx.gpr[7];    // a3, kept at sp + 0
    const u32 index_base = ctx.gpr[8];  // s4
    const u32 stride = ctx.gpr[9];      // $fp
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[16] = 0u;         // s0: the target
    ctx.gpr[17] = self;       // s1
    ctx.gpr[18] = 0u;         // s2: the entry index
    ctx.gpr[19] = scale;      // s3
    ctx.gpr[20] = index_base; // s4
    ctx.gpr[21] = 2u;         // s5
    ctx.gpr[22] = list;       // s6
    ctx.gpr[23] = 0u;         // s7: the reference-pair state
    ctx.gpr[30] = stride;     // $fp
    put(rt, frame.at(0u), argument);
    // The pair of words the cases store: the high word takes bits 16-23 from (value >> 24), the low one the low 24 bits.
    const auto store_pair = [&](u32 target, u32 high_base, u32 low_base, u32 value) {
        put(rt, target, high_base | ((value >> 24) << 16));
        put(rt, target + 4u, low_base | ((value << 8) >> 8));
    };
    // The two zero words of the 0x80/0x81 cases, or the reference pair when the state is not the wanted one.
    const auto emit_reference = [&](u32 target, u32 address, u32 wanted) {
        if (ctx.gpr[23] == wanted) {
            put(rt, target, 0u);
            put(rt, target + 4u, 0u);
        } else {
            store_pair(target, 0x10000000u, 0x0A000000u, address);
            ctx.gpr[23] = wanted;
        }
    };
    u32 index = 0u;
    u32 count = half_u(rt, self + 0x0Cu);
    while (index < count) {
        const u32 base = word(rt, self);
        const u32 entry = word(rt, self + 4u) + (index << 3);
        const u32 packed = word(rt, entry);
        const u32 kind = byte_u(rt, entry);
        const u32 value = word(rt, entry + 4u);
        const u32 target = base + (packed >> 8);
        ctx.gpr[16] = target;
        ctx.gpr[18] = index;
        if (kind == 2u) {
            if (list != 0u) {
                if (index_base == 0u) {
                    const u32 name = word(rt, list);
                    if (name != 0u) store_pair(target, 0x10000000u, 0x0A000000u,
                                               call_set(rt, ctx, 0x08B531FCu, {name, value}));
                } else {
                    store_pair(target, 0x10000000u, 0x0A000000u, word(rt, index_base + value * stride));
                }
            }
        } else if (kind < 3u) {
            if (kind == 0u) {
                store_pair(target, 0x10000000u, 0x01000000u, base + value);
            } else if (kind == 1u) {
                if (scale != 0u) store_pair(target, 0x10000000u, 0x0A000000u, scale + argument * value);
            }
        } else if (kind == 0x80u) {
            if (scale != 0u) {
                const u32 record = scale + value * argument;
                if ((word(rt, record + 0x18u) & 2u) != 0u) emit_reference(target, 0x08CE0DB8u, 1u);
                else emit_reference(target, 0x08CE0DA8u, 0u);
            }
        } else if (kind == 0x81u) {
            if (scale != 0u) emit_reference(target, 0x08CE0DA8u, 0u);
        } else if (kind == 3u) {
            store_pair(target, 0x10000000u, 0x09000000u, base + value);
        }
        index += 1u;
        count = half_u(rt, self + 0x0Cu);
    }
}

// 0x08B38568 (self = a0, source = a1): builds a 0x10-byte result (v0 = its address, 0 when the optional built around
// word(self) at sp+8 is empty after 0x08B39804(&optional, &source_block, source) and 0x08B3A3D0(0x08CB5568) runs).
// The optional's +4 word is an array size through 0x08B37B44: that many words are allocated (0x08B6461C) into
// self + 4 and zeroed; 0x08B37B34(self) fills self + 8 and + 0xC, which 0x08B38160(self, &optional, self + 8,
// self + 0xC) uses. The result is the 0x10-byte object from 0x08B62344(0x10) after 0x08B37710: its +0xC word is
// 0x08B3D4B8's answer (read in the delay slot of the 0x08B3846C call, so it is the first call's), +8 is the count of
// self + 0x14 records ((self+0x18 - self+0x14) >> 4) and +4 an array of 8-byte entries (0x08B6461C(count << 3)), one
// per record: the byte at +4 is the record's +8 byte, the word at +0 the record's +0xC word, and the record's +9
// byte is cleared. The two temporaries at sp and sp+0x18 are released on the way out (their vtables set to
// 0x08CDA908 / 0x08CDA8F8 first).

// 0x08B38E74 (context = a0): the parser behind the bracket syntax. 0x08B38938(context) reads one character (-1 at
// the end): '^' (0x5E) before the value clears the flag stored at the result's +0x14, ']' (0x5D) as the first
// character reports 0x08CB55CC, and -1 reports 0x08CB55B0. Otherwise a 0x18-byte object is allocated
// (0x08B62344(0x18)): vtable 0x08CD93D8, +4/+8 zero, the +8 sub-object's +4/+8 zero, +0x14 the flag. Each pass then
// reads a character: '-' reads a second one (0x5D pushes the previous character's pair, the ('-', '-') pair and
// finishes; -1 is an error; anything else pushes (previous, second) and continues), 0x5D finishes, -1 is an error
// and anything else pushes (previous, previous) as a pair of the vector at object + 0x10. The pairs go through
// 0x08C4F48C when the vector is full, otherwise in place at its +4 pointer (a zero pointer becomes 8 without a
// store); the result object is put at context + 0x10 (releasing what was there) with its count raised, and the
// function answers 0 (its epilogue returns v1). Both errors destroy the new object through its virtual slot 4 and
// answer 0x08B388D4's result.

// 0x08B5C4B0 (destination = a0, source = a1): fills `destination` (word +0 = the data of the empty string at
// 0x08D774A4) from the source's word(+0) object's +0x10 name array: the first name (word +0) null runs
// 0x08C74FAC(destination, 0, word(0x08D77498), 1, 0x2A) instead; when the word at +4 exists, the five words of the
// array are compared with 0x08B35FD8(word i, word i+4) and, if none matches, the first name is appended with
// 0x08C74DD8(destination, name, 0x08B35F60(name)). A match reserves 0x80 bytes (0x08C75840), appends the word at
// 0x08C95ED8 (a global whose word is a string) and its length, an '=', the first name, then ';' and the word at
// 0x08C95ED8 + i for i = 4, 8, 12, 16, 20 (0x08C75AF8 with 0x08B35F60's length). v0 = destination.

} // namespace
} // namespace new_batch_main_223
} // namespace gt::game

#define MAIN223(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_223::name, gt::ReturnKind::kind)
MAIN223(0x08AE7618u, func_08AE7618, Word);
MAIN223(0x08B53258u, func_08B53258, Void);

// ---- from main-session batch 224 (draft 82d86f6a5a48aa6fcde0ea6fde5b285de039d6ea) ----
// New batch 224 (main session): functions emitted by the recompiler that no src/ file defines or
// registers. Written by hand from `tools/disasm` as structured C; this is a draft batch, not yet
// verified by the differential gate.
//
// The shapes the batch repeats: a call whose v0/v1/f0 the next instruction reads (call_keep), the
// compiler's virtual call (v1 = vtable, v0 = the slot), the standard library's COW string (a header
// at data-0xC: length, capacity, reference count) with its release / push_back inline, and the
// compiler's vector {begin, end, capacity}.
//
// Left (read, not written), 2 of the 9 addresses:
//  - 0x08B758A4: a coroutine/continuation resume — 0x08B7470C builds a 0x4E8-byte context on its frame
//    (the s0-s7/sp/ra record) and the 6 path writes word(frame+0x9B8) over its own saved ra before the
//    epilogue returns through it. setjmp/longjmp-shaped, so not drafted.
//  - 0x08B30660: reads two stack-passed arguments (lbu/lw at fp+0x60 / fp+0x64) and builds an argument
//    block with unaligned swl/swr stores at sp+0..sp+0xB for the import 0x08C7AEC0; the header layout
//    was not certain enough to write.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_224 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0/v1/f0 as the callee left them: the instructions after it read the
// callee's registers, and a callee that never writes v0 hands the previous one on.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The compiler's virtual call: `lw v1, 0(object); lw v0, offset(v1); jalr v0` — v1 is the vtable at
// the call, v0 the slot, and afterwards both are the callee's.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 function = word(rt, table + offset);
    ctx.gpr[3] = table;
    ctx.gpr[2] = function;
    return call_keep(rt, ctx, function, arguments);
}
s32 signed_half_at(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// ---------------------------------------------------------------------------------------------------
// 0x088EC7D8 (a0 = object, a1 = a {text, length, .., byte +9, word +0x1C} record, a2 / a3 = two output
// words): the object's +0x538 holder runs its virtual 0xD4; when the record has a text and a length a
// block is built on the stack (`length` bytes of the text through func_08B79A88 and, behind them, the
// word 3 and two zero bytes), and the holder's 0x68 virtual with (the signed half at 0x08CDD3AC, the
// table 0x08CA0B44, 0, that block, 0) gives the first id (s4, -1 when skipped), whose 0x74 virtual
// result goes to *a2. The holder's 0x68 with (the signed half at self+0x58C, 0x08CA0B34, 0, 0, 0) gives
// the second id, whose 0x74 result goes to *a3. Then a 0x40-byte record at the frame is cleared and
// filled: +0 = -1, +2 = the holder's 0x90 virtual byte, +8 = the second id, +0xC = the first id,
// +0x10 = the length, +0x38 = 9, +0x3C / +0x3D = -1, +0x3E = 0, +0x3F = 1, +0x34 = 0, +0x35 = 1 and,
// when the record's byte at +9 is set, +0x30 = its word at +0x1C. The holder's 0x68 with (the signed
// half at self+0x578, 0x08CA0B5C, 0, record, 0) gives a third id (s1); the 0x74 virtual result goes to
// the third id's +4 when not null, the holder's 0xD8 virtual runs and v0 = that 0x74 result (s0).

// ---------------------------------------------------------------------------------------------------
// 0x089AFA10 (a0 = object, a1 = an object whose member +0x14 owns the tree, a2 = a vector to fill): the
// key comes from the getter 0x089FEDFC (the word at 0x08CF5C44); the tree at frame +0x20 is made empty
// (its header at +0x24: no root, links to itself, count 0) and, under the lock 0x08CF4AA4, the caller's
// vector is assigned from the tree's owner and erased (func_08BFBB38, func_08BF9B6C). lower_bound(tree,
// &key) (func_08BF9104) either finds a node whose key at +0x10 is not above the key, or a hint_insert
// (func_08BF99BC) with the key and a zero second word makes one. The node's +0x14 receives a 12-byte
// object (func_08B62344) whose words are 0x08CCEAC8, 0x08BF8A30 and 0; then the object's +0x14 member is
// told about the tree through 0x0897876C and the guard at frame +0x1C takes the vtable 0x08CCEAB8.
// Every pointer of the temporary vector whose object does not carry 0x200 in its +0x138 flags is pushed
// back into a2 (the inline push_back, func_08BFB9D8 when the vector is full) and, under the lock again,
// a2 is assigned the temporary; the byte self+0x3C is set, 0x08B1C47C runs, the vector is emptied
// (func_08BF8B48 then func_08A5BC54) and the guard is released (0x089FD82C). v0 is the last call's.

// ---------------------------------------------------------------------------------------------------
// The standard library's COW string: a cell header sits 0xC bytes before the data pointer (length at
// -0xC, capacity at -8, reference count at -4); the shared empty string is the cell 0x08D74FD0.
constexpr u32 kStringEmptyCell = 0x08D74FD0u;
constexpr u32 kStringExchangeAndAdd = 0x08B60804u;  // __exchange_and_add(int *, int), the old value
constexpr u32 kStringRepDestroy = 0x08B8542Cu;      // _Rep::_M_destroy(rep *, const allocator &)
constexpr u32 kStringFromCString = 0x08B85EB8u;     // string(out, const char *, const allocator &)
constexpr u32 kStringAssign = 0x08B855E0u;          // operator=(const string &)
constexpr u32 kStringGrow = 0x08B864ECu;            // the growth of an append

// The release of the cell the string at `slot` holds: the shared empty cell does nothing, otherwise
// the reference count at data-4 goes down and at zero the cell is destroyed with `flag_slot` (the
// allocator argument the original passes from its frame).
void release_string(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 flag_slot) {
    const u32 data = word(rt, slot);
    if (data - 0xCu == kStringEmptyCell) {
        ctx.gpr[4] = slot;  // the branch's delay slot
        return;
    }
    const u32 references = call(rt, ctx, kStringExchangeAndAdd, {data - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(references) > 0) {
        ctx.gpr[4] = slot;
        return;
    }
    ctx.gpr[4] = slot;
    (void)call(rt, ctx, kStringRepDestroy, {data - 0xCu, flag_slot});
}

// `basic_string::push_back` inline: the cell grows (func_08B864EC(slot, length + 1)) when its capacity
// is short or it is shared, then the byte goes at the current length, the reference count is cleared,
// the new length stored and the terminator written behind the byte.
void append_byte(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 value) {
    const u32 data = word(rt, slot);
    const u32 length = word(rt, data - 0xCu);
    const u32 capacity = word(rt, data - 0x8u);
    const u32 shared = word(rt, data - 0x4u);
    const u32 want = length + 1u;
    if (capacity < want || static_cast<s32>(shared) > 0) (void)call(rt, ctx, kStringGrow, {slot, want});
    const u32 current = word(rt, slot);
    const u32 current_length = word(rt, current - 0xCu);
    put8(rt, current + current_length, value);
    put(rt, current - 0x4u, 0u);
    put(rt, current - 0xCu, want);
    put8(rt, current + want, 0u);
}

// 0x08A473D0 (a0 = the string to build, a1 = the source string): when the word at 0x08D69524 is below
// 10 and its bit in 0x3D8 is set, the source is copied through a COW string that turned every '.' (0x2E)
// into a ',' (0x2C) (push_back inline, the growth through func_08B864EC), that copy is rebuilt from its
// bytes (func_08B85EB8) and assigned (func_08B855E0); otherwise the source is assigned directly. The
// result is constructed at a0 from the string's bytes (func_08B85EB8) and a0 is answered. Each temporary
// cell is released (0x08B60804 / 0x08B8542C, the shared empty cell skipped).
void func_08A473D0(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const u32 empty_data = kStringEmptyCell + 0xCu;
    put(rt, frame.at(0x0Cu), empty_data);  // the string handed to the final construction
    put(rt, frame.at(0x10u), empty_data);  // the accumulated copy
    const u32 mode = word(rt, 0x08D69524u);
    const bool replace_dots = mode < 10u && ((1u << mode) & 0x3D8u) != 0u;
    if (replace_dots) {
        const u32 text = word(rt, source);
        if (word(rt, text - 0xCu) != 0u) {
            for (u32 index = 0u; index < word(rt, word(rt, source) - 0xCu); ++index) {
                const s32 character = static_cast<std::int8_t>(rt.memory().load8(word(rt, source) + index));
                append_byte(rt, ctx, frame.at(0x10u), character == 0x2E ? 0x2Cu : static_cast<u32>(character));
            }
        }
        (void)call(rt, ctx, kStringFromCString, {frame.at(0x08u), word(rt, frame.at(0x10u)), frame.at(0x03u)});
        release_string(rt, ctx, frame.at(0x10u), frame.at(0x02u));
        (void)call(rt, ctx, kStringAssign, {frame.at(0x0Cu), frame.at(0x08u)});
        release_string(rt, ctx, frame.at(0x08u), frame.at(0x01u));
    } else {
        (void)call(rt, ctx, kStringAssign, {frame.at(0x0Cu), source});
    }
    (void)call(rt, ctx, kStringFromCString, {destination, word(rt, frame.at(0x0Cu)), frame.at(0x04u)});
    release_string(rt, ctx, frame.at(0x0Cu), frame.at(0x00u));
    ctx.gpr[2] = destination;
}

// ---------------------------------------------------------------------------------------------------
// 0x088A7F1C (a0 = object, a1 = a transform, a2 = another, a3 = a third, t0 = the extra argument the
// first call gets, t1 = a fourth transform, f12 / f13 / f14 = three floats): the local matrix at
// a0+0x104 is rebuilt from a2 and a1 under their +0x50 and +0x38 halves (0x08BAE264, 0x08BAE250,
// 0x08BAE2B0, 0x08BAE2FC and 0x08BAE330 are the float library's pair, matrix and product helpers).
// When the matrix's +4 and +8 words differ, or the float at the word the matrix points to, +0xC, is
// below f12, the matrices are combined with the negated xyz of 0x08BAC230(a1, a3) and t1
// (0x08BAC284, 0x08BAE278) into three 4-vectors whose w is f12 / f13 / f14, which 0x0896108C adds to
// a0+0x104. v0 is that call's result (the matrix's word on the early exit).

// ---------------------------------------------------------------------------------------------------
// 0x08B472AC (a0 = object with a list at +0x2C): the list's count (func_08B1FDB0) sizes two arrays on
// the stack: one 0x0C-byte record per entry, and one vtable word per entry. Walking the list from
// +0x30, an entry whose vtable's byte at +0x30 is set gives a record {vtable[0], 0xB0E, 0} and its
// vtable in the word array, until the count is reached or the list ends. That call descriptor list goes
// to func_08C7AE18(records, count, 0, 1); a null answer, or a negative one other than 0x80410717, ends
// the function. Otherwise each record again walks its entry: a state word at +8 with 0x100 / 0x200 /
// 0x800, or 2 / 4 / 8, selects the object (the word array entry) for the object's 0x94 and then 0xA4
// virtual with the object's vtable word as the second argument; in the 2 / 4 / 8 arm a byte at +0x31
// sends it to func_08B46AA0 instead, and a byte at +0x32 picks func_08B471FC over func_08B4665C after
// func_08B3F910 / func_08B463D4 marked it (the marked arm's 0x94 virtual carries 1 in a2). v0 is the
// queue's answer, 0 when the list is empty, or the last virtual call's.

// ---------------------------------------------------------------------------------------------------
// Shared pointers of the standard library: an object's reference count sits at control+8 (the ll/sc
// loops of the original), and a released count of one runs the object's virtual 0xC with the control
// block as a0. Wide strings use their own empty cell (0x08D75298) and _M_destroy (0x08B93554).
void count_up(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }
u32 count_down(Runtime &rt, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    return old;
}
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 control) {
    if (control == 0u) return;
    if (count_down(rt, control + 8u) == 1u) (void)call_virtual(rt, ctx, control, 0x0Cu, {control});
}
void release_wide_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 flag_slot) {
    if (data - 0xCu == 0x08D75298u) return;
    const u32 references = call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(references) > 0) return;
    (void)call(rt, ctx, 0x08B93554u, {data - 0xCu, flag_slot});
}

// 0x088B0818 (a0 = object with a tree at +8 under the lock +0x20, a1 = a key string, a2 = a pair
// {object, control block}): under the lock, a null pair does nothing. The key is copy-constructed at
// frame +4 (func_08B93CD8) and looked up (func_08BAEE1C); the end marker is self+0xC. At a node the
// key's cell is compared with the node's key cell (+0x10) through func_08B36170 over the shorter
// length, a non-zero answer used as is and otherwise the length difference: below zero inserts.
// func_08B9432C copy-constructs the pair at frame +0x10, func_08BAFA9C inserts it with the found node
// as the hint and the temporary's control block (frame +0x18) and key cell (frame +0x10) are
// released. Then the node's +0x14 takes the object (it and the passed control block get a reference
// each), the control block it held at +0x18 is released and replaced, and the passed control block's
// extra reference is released. The key temporary (frame +4) goes and the lock is released; v0 is the
// unlock's.

// ---------------------------------------------------------------------------------------------------
// 0x089DEBD4 (a0 = object, a1 = a holder whose word is an object with a count): the object's +0x140
// word, xor 0x80 and shifted right 7, picks one of its 12-byte vectors at +0xE0 + 0xC * bit + 0xC
// (begin, end, capacity). The holder's object answers its count through the 0x7C virtual (a null one is
// reported by func_08A5BB7C first): with a count the vector is resized to it (func_08BF1080 when it
// already holds more, otherwise func_08BF1768), and for every index two floats are fetched: the object's
// 0x84 virtual fills a temporary (index), that temporary's 0x84 (0) fills another, whose 0x54 virtual
// answers the float stored at the entry's +0 (func_08A5B9EC releases it); then the first temporary's
// 0x84 (1) and 0x54 give the float at the entry's +4. Both temporaries are released each round. v0 is
// the holder when the loop ends. A zero count instead empties the vector (its three words swapped with
// a zero vector on the frame, func_08BF0BF0) and frees its storage (func_08B35254).

} // namespace
} // namespace new_batch_main_224
} // namespace gt::game

#define MAIN224(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_224::name, gt::ReturnKind::kind)
MAIN224(0x08A473D0u, func_08A473D0, Word);

// ---- from main-session batch 225 (draft 4638a1abcc68654344cf6dd2fe246f1d10c0ee1b) ----
// Main-session batch 225: functions the extra seeds made emittable, written by hand as structured C from `tools.disasm`.
// Draft only (no gate run in this session): each body keeps the original's v0 on every exit path, every argument register a
// callee reads, the delay slot of a `jal` before the call, and the frame size of the original's prologue.
// Four of the eight candidates are here: 088DF790 (the object state reset), 08AE5878 (the input dispatch), 088FA60C (the
// segment-list projection) and 08B30284 (the stream/resource loader). Left out on purpose:
//   08C51058  an rb-tree hint-insert descent with an inlined size-then-bytes key compare (167 insns, four interleaved compare
//             loops); its branch structure was not decoded with enough confidence to write it faithfully.
//   08C28AC0  inserts through three stack temporaries with exception-abort paths (08A5BB7C(0x08CB15A0)) and two virtual slots.
//   08C67404  formats into a stack buffer with a *dynamic* frame (`subu sp` from a computed size, `ins`/`ext`), so the frame
//             audit cannot see it and the stack block has no fixed offset.
//   08B477D4  a 0x5A0-byte command-block state machine around ten virtual slots and two Sony imports; not modelled here.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_225 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using u16 = std::uint16_t;
using s32 = std::int32_t;

// A guest call; afterwards `v0` holds its result, as after the original's `jal`.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}

// The same, keeping v0 / v1 / f0 as the callee left them (a caller that reads v1 after the call).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A guest call with the argument registers written like the original writes them before the jal (a0-a3, t0-t3).
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call(rt, ctx, target, arguments);
}

// `lw v0, off(a0); jalr v0`: v0 = the function pointer before the call, then the callee's v0 (one that never writes v0
// returns its own address).
u32 call_pointer(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

s32 signed_of(u32 value) { return static_cast<s32>(value); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
s32 signed_half(Runtime &rt, u32 address) { return static_cast<std::int16_t>(rt.memory().load16(address)); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }

// The callee-saved registers a body keeps state in: the original restores them before it returns or tail-calls.
struct SavedRegs {
    AllegrexContext &ctx;
    u32 saved[8];
    u32 saved_fp;
    explicit SavedRegs(AllegrexContext &c) : ctx(c), saved_fp(c.gpr[30]) {
        for (unsigned i = 0u; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedRegs() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
        ctx.gpr[30] = saved_fp;
    }
};

// 0x088DF790 (a0 = object): resets the object's state — clears the byte and word flags, calls 088D5DF8(self+4) and stores its
// result at +0x59C, fills the 4-entry table at +0x77C (+0x20 each) and +0x780 (+0 each) with -1, calls 088DC1B8(self, 3), then
// per entry of 088D9F80(self): 088E8184 and 088E8CC0 on *(self+0xC0)+0x2F8*i, clears +0x2D0/+0x2D4 and stamps the index. Finally
// 088F89AC(*(self+0x14C)) when set, 088D9BBC(self, 0, 0), and a memset of *(self+0x644) with 0x340 zeros as a tail call.

// 0x08AE5878 (a0 = object, a1 = index): asks the object's +0x34 handler twice (first answer = `first`, second = `second`).
// When `first` lies inside the object's +0x88 / +0x89 signed-byte range the pair is packed into one opcode word
// ((index | 0xB0) << 16 | first << 8 | second) and sent through the object's +0x10 -> +0x10 -> +0x8 chain; then, and also for a
// `first` below +0x88, `first` selects a 0..0x7F action (table 0x08C916EC) that writes one of the object's input bit fields or
// forwards the request to 08AE85E8 / 08AE3678 / 08AE3708. Returns 1, or 0 when 08AE3708 reports no change.
void func_08AE5878(Runtime &rt, AllegrexContext &ctx) {
    const u32 index = ctx.gpr[5];
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x60u);
    SavedRegs saved(ctx);
    ctx.gpr[19] = index;
    ctx.gpr[16] = self;

    const u32 first = call_pointer(rt, ctx, word(rt, self + 0x34u), {self});
    ctx.gpr[17] = first;
    const u32 second = call_pointer(rt, ctx, word(rt, self + 0x34u), {self});
    ctx.gpr[18] = second;

    if (signed_of(first) >= signed_byte(rt, self + 0x88u)) {
        if (!(signed_byte(rt, self + 0x89u) < signed_of(first))) {
            const u32 holder = word(rt, self + 0x10u);
            const u32 node = word(rt, holder + 0x10u);
            const u32 packed = ((index | 0xB0u) << 16) | (first << 8) | second;
            call_set(rt, ctx, 0x08AE0CD8u, {word(rt, node + 0x8u), packed});
        }
    }

    ctx.gpr[3] = 1u;
    if (!(first < 0x80u)) {
        ctx.gpr[2] = 1u;
        return;
    }

    const u32 id = static_cast<u32>(signed_half(rt, self + 0x1Au));
    const u32 input = self + index + 0x5Cu;
    // 08AE85E8(id, &block): three or four words of the block, the fourth left untouched when `with_second` is false.
    const auto send = [&](u32 code, u32 extra, bool with_second) {
        put(rt, frame.at(0x0u), code);
        put(rt, frame.at(0x4u), index);
        put(rt, frame.at(0x8u), extra);
        if (with_second) put(rt, frame.at(0xCu), second);
        call_set(rt, ctx, 0x08AE85E8u, {id, frame.at(0x0u)});
        ctx.gpr[3] = 1u;
    };
    const u32 bit = 1u << (index & 31u);

    switch (first) {
    case 0:
        send(0xDu, 0u, true);
        break;
    case 6:
        put8(rt, input, second);
        break;
    case 7:
        send(0xDu, 2u, true);
        break;
    case 10:
        send(0xDu, 4u, true);
        break;
    case 11:
        send(0xDu, 3u, true);
        break;
    case 64: {
        const u32 flags = rt.memory().load16(self + 0x58u);
        if (signed_of(second) < 0x40) {
            if (((flags >> (index & 31u)) & 1u) != 0u) send(7u, 0u, false);
            put16(rt, self + 0x58u, flags & ~bit);
        } else {
            put16(rt, self + 0x58u, flags | bit);
        }
        break;
    }
    case 96:
        put8(rt, input, rt.memory().load8(input) + 1u);
        break;
    case 97:
        put8(rt, input, rt.memory().load8(input) - 1u);
        break;
    case 99:
        if (second == 0x16u) {
            const u32 holder = word(rt, self + 0x10u);
            const u32 node = word(rt, holder + 0x10u);
            call_set(rt, ctx, 0x08AE0CD8u, {word(rt, node + 0x8u), rt.memory().load8(input)});
        } else if (signed_of(second) < 0x17) {
            if (second == 0x14u) call_set(rt, ctx, 0x08AE3678u, {self, rt.memory().load8(input)});
        } else if (second == 0x1Eu) {
            const u32 changed = call_set(rt, ctx, 0x08AE3708u, {self});
            ctx.gpr[3] = changed == 0u ? 0u : 1u;
        }
        break;
    case 120:
        send(0xAu, 0u, false);
        break;
    case 121:
        send(0xEu, 0u, false);
        break;
    case 123:
        send(7u, 0u, false);
        break;
    case 126:
        put16(rt, self + 0x56u, rt.memory().load16(self + 0x56u) | bit);
        break;
    case 127:
        put16(rt, self + 0x56u, rt.memory().load16(self + 0x56u) & ~bit);
        break;
    default:
        break;
    }
    ctx.gpr[2] = ctx.gpr[3];
}

// 0x088FA60C (a0 = object, a1 = out block, a2 = origin point, a3 = target point, t0 = the answer): walks the object's
// +0x4 segment list (088FAD1C fills {base, cursor, end} at sp+0x80; each 16-byte record's signed half at +0xC chains to the
// next through +0x10). The origin and the target are subtracted (08BAC428 at the entry) and each segment is projected on the
// x/z plane; the first segment whose two cross products agree gets its unit direction in the out block's first three floats,
// -(direction . segment start) at +0xC, 0.5 at +0x10 / +0x14 and 0 at +0x18. Returns t0, or -1 when the list is empty, the
// base is null or the list runs out. The two pair-returning helpers (08BAC270, 08BBBE54) hand their arguments back in v0:v1.
// Branches the routes do not take (checked against the asm by hand): the base-null exit (s3 == 0) and the empty list exit.
void func_088FA60C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 origin = ctx.gpr[6];
    const u32 target = ctx.gpr[7];
    const u32 answer = ctx.gpr[8];
    GuestFrame frame(ctx, 0xD0u);
    SavedRegs saved(ctx);
    const float saved_f20 = ctx.fpr[20];
    const float saved_f21 = ctx.fpr[21];
    ctx.fpr[20] = 0.0f;
    ctx.fpr[21] = std::bit_cast<float>(0x3F000000u);  // 0.5
    ctx.gpr[23] = out;
    ctx.gpr[16] = self;
    ctx.gpr[21] = origin;
    ctx.gpr[20] = answer;

    const GuestResult pair = call_keep(rt, ctx, 0x08BAC270u, {target, origin});
    put(rt, frame.at(0x70u), pair.v0);
    put(rt, frame.at(0x18u), pair.v0);
    put(rt, frame.at(0x4u), frame.at(0x18u));  // stack argument of the next call
    put(rt, frame.at(0x74u), pair.v1);
    put(rt, frame.at(0x1Cu), pair.v1);
    call_set(rt, ctx, 0x08BAC428u, {frame.at(0x2Cu), pair.v0, pair.v1, frame.at(0x18u)});

    const float dx = float_at(rt, frame.at(0x2Cu));
    const float dz = float_at(rt, frame.at(0x34u));
    put_float(rt, frame.at(0x40u), dz);
    put_float(rt, frame.at(0x44u), -dx);
    put_float(rt, frame.at(0x38u), dz);
    put_float(rt, frame.at(0x3Cu), -dx);
    put(rt, frame.at(0x10u), answer);
    put(rt, frame.at(0x14u), answer);
    call_set(rt, ctx, 0x088FAD1Cu, {frame.at(0x80u), word(rt, self + 0x4u), frame.at(0x10u)});

    // The `lw $s3/$s6, 0x80/0x88($sp)` the audit flags are not reloads of saved registers: they read the block 088FAD1C
    // filled at sp+0x80 (modelled here as frame.at).
    const u32 base = word(rt, frame.at(0x80u));
    u32 cursor = word(rt, frame.at(0x84u));
    const u32 end = word(rt, frame.at(0x88u));
    put(rt, frame.at(0x20u), base);
    put(rt, frame.at(0x24u), cursor);
    put(rt, frame.at(0x28u), end);

    u32 value = answer;
    for (;;) {
        if (!(cursor < end)) {
            value = 0xFFFFFFFFu;
            break;
        }
        if (base == 0u) {
            value = 0xFFFFFFFFu;
            break;
        }
        const s32 index = signed_half(rt, cursor);
        cursor += 2u;
        const u32 first = base + static_cast<u32>(index * 16);
        const s32 offset = signed_half(rt, first + 0xCu);
        u32 second = first + 0x10u;
        if (offset >= 0) second += static_cast<u32>(offset * 16);

        const float origin_z = float_at(rt, origin + 0x8u);
        const float origin_x = float_at(rt, origin + 0x0u);
        const float first_z = float_at(rt, first + 0x8u);
        const float second_x = float_at(rt, second + 0x0u);
        const float second_z = float_at(rt, second + 0x8u);
        const float first_x = float_at(rt, first + 0x0u);
        const float edge_z = first_z - origin_z;
        const float span_z = second_z - origin_z;
        const float edge_x = first_x - origin_x;
        const float span_x = second_x - origin_x;
        put_float(rt, frame.at(0x4Cu), edge_z);
        put_float(rt, frame.at(0x48u), edge_x);
        put_float(rt, frame.at(0x50u), span_x);
        put_float(rt, frame.at(0x54u), span_z);

        const GuestResult handed = call_keep(rt, ctx, 0x08BBBE54u, {frame.at(0x48u), frame.at(0x38u)});
        put(rt, frame.at(0x70u), handed.v0);
        put(rt, frame.at(0x74u), handed.v1);
        put(rt, frame.at(0x8u), handed.v0);
        put(rt, frame.at(0xCu), handed.v1);
        call_set(rt, ctx, 0x08BBBF50u, {frame.at(0x58u), handed.v0, handed.v1});

        const float product0 = float_at(rt, frame.at(0x58u));
        const float product1 = float_at(rt, frame.at(0x5Cu));
        const float product2 = float_at(rt, frame.at(0x60u));
        const float product3 = float_at(rt, frame.at(0x64u));
        const float side_first = product0 + product1;
        const float side_second = product2 + product3;
        if (0.0f < side_first) continue;
        if (side_second < 0.0f) continue;
        const float difference = side_second - side_first;
        const float cross = psp_mul(edge_x, span_z) - psp_mul(edge_z, span_x);
        if (0.0f < cross) continue;
        if (cross + difference < 0.0f) continue;

        const float end_z = float_at(rt, second + 0x8u);
        const float start_z = float_at(rt, first + 0x8u);
        const float end_x = float_at(rt, second + 0x0u);
        const float start_x = float_at(rt, first + 0x0u);
        put_float(rt, frame.at(0x30u), 0.0f);
        put_float(rt, frame.at(0x2Cu), end_z - start_z);
        put_float(rt, frame.at(0x34u), start_x - end_x);
        const u32 normal = call(rt, ctx, 0x08BBBE8Cu, {frame.at(0x2Cu)});
        put(rt, frame.at(0x0u), normal);  // stack argument of the normalizer
        call_set(rt, ctx, 0x08BBBEF8u, {frame.at(0x2Cu), normal});

        const float normal_x = float_at(rt, frame.at(0x2Cu));
        const float normal_y = float_at(rt, frame.at(0x30u));
        const float normal_z = float_at(rt, frame.at(0x34u));
        put_float(rt, out + 0x8u, normal_z);
        put_float(rt, out + 0x0u, normal_x);
        put_float(rt, out + 0x4u, normal_y);
        put_float(rt, out + 0x18u, 0.0f);
        const float length_z = float_at(rt, first + 0x8u);
        const float length_x = float_at(rt, first + 0x0u);
        put_float(rt, out + 0x14u, 0.5f);
        const float along_z = psp_mul(normal_z, length_z);
        const float along_x = psp_mul(normal_x, length_x);
        put_float(rt, out + 0x10u, 0.5f);
        put_float(rt, out + 0xCu, -(along_x + along_z));
        break;
    }
    ctx.gpr[2] = value;
    ctx.fpr[20] = saved_f20;
    ctx.fpr[21] = saved_f21;
}

// 0x08B30284 (a0 = object, a1 = a 0x48-byte configuration block): under the object's +0x8FC lock, brings the object's stream
// up to date and loads one 0x44-described resource. The two byte counters at +0x4 / +0xC gate an early return; the type at
// config+0 selects one of three per-type tables (1 -> 0x08B315F4, 2 -> 0x08B30DA0, 0 -> 0x08B30F78), 08C7AEA8 opens it with
// the config's words, the 0x64 field sizes a 1000-byte-per-unit buffer past a 64-bit base from 08B2E05C, and 08C7AE98 writes
// config's +0x28 / +0x30 / +0x38 / +0x3C block. Returns 1 when 08B2F550 accepted config+0x8, else 0. The a0 that the two Sony
// import thunks (+0x08B2E05C, +0x08B2E0B0) see is whatever 08C7AEA8 leaves behind, as in the original.
} // namespace
} // namespace new_batch_main_225
} // namespace gt::game

#define MAIN225(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_225::name, gt::ReturnKind::kind)
MAIN225(0x088FA60Cu, func_088FA60C, Word);
MAIN225(0x08AE5878u, func_08AE5878, Word);

// ---- from main-session batch 226 (draft 897d049be6278361298b6a1e89aec351b92940d4) ----
// New batch main-226 (draft, unverified): eight functions the recompiler emitted that nothing in src/ defines yet.
// Written by hand from `tools.disasm` as structured C. None of them has a name with evidence, so everything keeps
// its address and the comments carry what the asm says. Input shapes for the synthetic generator:
// /tmp/claude-1000/shapes_main226.txt (see also config/synth_shapes.txt).
//
// The families:
//  - the string-keyed tree's insert-with-hint, three per-TU instantiations of the same body (0x08BAFA9C,
//    0x08C335FC, 0x08C4ECB0) that differ only in their two tree helpers;
//  - a vector insert-fill over 32-byte elements (0x08BBB178);
//  - a boxed value asked for a float, twelve type-tagged arms (0x08C4DC48);
//  - a 4x4 linear solve with partial pivoting (0x08C52F8C);
//  - a four-value builder with two modes (0x0896E614);
//  - a record parser with a variable-length stack array (0x08A2BA74).
// Before registering: the four audits (frame_audit, literal_check, sp_loads, arg_reads) and the synthetic gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_226 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same, keeping v0/v1/f0/a1 as the callee left them (the callees whose f0 the code stores, and the one whose
// a1 the next call reads).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with a value in $f12 (the elimination helper of the solve takes its factor there).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       float value) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {value});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// --- the string-keyed tree's insert-with-hint (three instantiations) ---
//
// The tree at a0: +0x04 the header node (what `end()` is), +0x0C a node compared against the hint, +0x10 the
// rightmost node, +0x14 the element count. A node: +0x08/+0x0C its children, +0x10 the key, a std::string whose
// characters are at the word read there and whose length is the word 0xC bytes before them.
// The instantiation supplies its own insert-unique (a0 = tree, a1 = key, answers the node) and insert-at-position
// (a0 = tree, a1 = 0 or the position itself, a2 = the position, a3 = the key, answers the node).
constexpr u32 kKeyCompare = 0x08B36170u;    // memcmp(left, right, shorter)
constexpr u32 kPreviousNode = 0x08B5F174u;  // --iterator
constexpr u32 kNextNode = 0x08B5F100u;      // ++iterator

// The original's key comparison: memcmp over the shorter length, the difference of the lengths breaking the tie.
s32 key_compare(Runtime &rt, AllegrexContext &ctx, u32 left, u32 right) {
    const u32 left_length = word(rt, left - 0xCu);
    const u32 right_length = word(rt, right - 0xCu);
    const u32 shorter = left_length < right_length ? left_length : right_length;
    const s32 compared = static_cast<s32>(call(rt, ctx, kKeyCompare, {left, right, shorter}));
    if (compared != 0) return compared;
    return static_cast<s32>(left_length - right_length);
}
u32 node_key(Runtime &rt, u32 node) { return word(rt, node + 0x10u); }

// a0 = tree, a1 = the hint node, a2 = the key; v0 = the node the insert answered, or the hint when the key is
// already at that position (the iterator the original hands back then).
void insert_with_hint(Runtime &rt, AllegrexContext &ctx, u32 insert_unique, u32 insert_at) {
    const u32 tree = ctx.gpr[4];
    const u32 hint = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    if (hint == tree + 4u) {
        // The hint is end(): the new key goes after the rightmost node when it is greater than that node's key.
        if (word(rt, tree + 0x14u) != 0u) {
            const u32 key_data = word(rt, key);
            const u32 rightmost = word(rt, tree + 0x10u);
            if (key_compare(rt, ctx, node_key(rt, rightmost), key_data) < 0) {
                ctx.gpr[2] = call(rt, ctx, insert_at, {tree, 0u, rightmost, key});
                return;
            }
        }
        ctx.gpr[2] = call(rt, ctx, insert_unique, {tree, key});
        return;
    }
    const u32 key_data = word(rt, key);
    if (key_compare(rt, ctx, key_data, node_key(rt, hint)) >= 0) {
        // key >= the hint's key: the key belongs after the hint.
        if (key_compare(rt, ctx, node_key(rt, hint), key_data) >= 0) {
            ctx.gpr[2] = hint;  // equal: the key is the hint's own
            return;
        }
        if (word(rt, tree + 0x10u) == hint) {
            ctx.gpr[2] = call(rt, ctx, insert_at, {tree, 0u, hint, key});
            return;
        }
        const u32 next = call(rt, ctx, kNextNode, {hint});
        if (key_compare(rt, ctx, key_data, node_key(rt, next)) >= 0) {
            ctx.gpr[2] = call(rt, ctx, insert_unique, {tree, key});
            return;
        }
        // Between the hint and its successor: on the successor when the hint has a right child (the non-zero first
        // argument asks the callee to take the position's left slot), otherwise on the hint itself.
        if (word(rt, hint + 0xCu) != 0u) {
            ctx.gpr[2] = call(rt, ctx, insert_at, {tree, next, next, key});
        } else {
            ctx.gpr[2] = call(rt, ctx, insert_at, {tree, 0u, hint, key});
        }
        return;
    }
    // key < the hint's key: the key belongs before the hint.
    if (hint == word(rt, tree + 0xCu)) {
        ctx.gpr[2] = call(rt, ctx, insert_at, {tree, hint, hint, key});
        return;
    }
    const u32 previous = call(rt, ctx, kPreviousNode, {hint});
    if (key_compare(rt, ctx, node_key(rt, previous), key_data) >= 0) {
        ctx.gpr[2] = call(rt, ctx, insert_unique, {tree, key});
        return;
    }
    if (word(rt, previous + 0xCu) != 0u) {
        ctx.gpr[2] = call(rt, ctx, insert_at, {tree, hint, hint, key});
    } else {
        ctx.gpr[2] = call(rt, ctx, insert_at, {tree, 0u, previous, key});
    }
}

void func_08BAFA9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x60u);
    insert_with_hint(rt, ctx, 0x08BAF940u, 0x08BAF828u);
}
void func_08C335FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x60u);
    insert_with_hint(rt, ctx, 0x08C334A0u, 0x08C333A0u);
}
void func_08C4ECB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x60u);
    insert_with_hint(rt, ctx, 0x08C4EB54u, 0x08C4EA54u);
}

// --- a vector of 32-byte elements: insert `count` copies of a value at a position ---
//
// a0 = the vector {begin, finish, end of storage}, a1 = the position, a2 = the count, a3 = the value.
// With room left past the finish, the elements after the position move up and the new range is filled; otherwise a
// new block is allocated (max(size, count) more elements), the head, the fill and the tail are built into it and
// the old block is destroyed and freed.
void func_08BBB178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x60u);
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    if (count == 0u) return;  // v0 is still the caller's then; the original writes nothing either
    const u32 start = word(rt, vector);
    const u32 finish = word(rt, vector + 4u);
    const u32 after = static_cast<u32>(static_cast<s32>(finish - position) >> 5);  // elements after the position
    if ((static_cast<u32>(static_cast<s32>(word(rt, vector + 8u) - finish) >> 5)) < count) {
        // No room: the new capacity is max(size, count) elements more than the size (the size of a vector whose
        // largest possible capacity would overflow reports through 0x08B5ADDC first).
        const u32 size = static_cast<u32>(static_cast<s32>(finish - start) >> 5);
        if (0x07FFFFFFu - size < count) (void)call(rt, ctx, 0x08B5ADDCu, {0x08CA0C58u});
        put(rt, frame.at(8u), size);
        const u32 wanted = size < count ? count : size;
        const u32 grown = size + wanted;
        // The overflow arm of the capacity check asks for a negative byte count, as the original does.
        const u32 bytes = grown < size ? 0xFFFFFFE0u : grown << 5;
        const u32 fresh = bytes != 0u ? call(rt, ctx, 0x08B35DA0u, {bytes}) : 0u;
        const u32 head_end = call(rt, ctx, 0x08BBA7E4u, {start, position, fresh, frame.at(3u)});
        const u32 filled = call(rt, ctx, 0x08BBA854u, {head_end, count, after, frame.at(2u)});
        (void)call(rt, ctx, 0x08BBA7E4u, {position, finish, filled + (count << 5), frame.at(1u)});
        const u32 ended = call(rt, ctx, 0x08BBAE24u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        if (word(rt, vector) != 0u) (void)call(rt, ctx, 0x08B35254u, {word(rt, vector)});
        const u32 new_end = fresh + bytes;
        ctx.gpr[2] = new_end;
        put(rt, vector + 8u, new_end);
        put(rt, vector + 4u, ended);
        put(rt, vector, fresh);
        return;
    }
    // The value is copied out first: the callees that fill read it from the frame, never from the caller's object.
    for (u32 word_index = 0; word_index < 8u; ++word_index)
        put(rt, frame.at(0x0Cu) + 4u * word_index, word(rt, value + 4u * word_index));
    if (count < after) {
        // The elements after the position move up by `count`: the last `count` into unused storage, the rest down
        // from there.
        (void)call(rt, ctx, 0x08BBAE34u, {finish - (count << 5), finish, finish, frame.at(6u)});
        const u32 centre = finish - (count << 5);
        put(rt, vector + 4u, word(rt, vector + 4u) + (count << 5));
        const s32 moves = static_cast<s32>(centre - position) >> 5;
        u32 source = centre - 0x20u;
        u32 target = finish;
        for (s32 moved = 0; moved != moves; ++moved) {
            target -= 0x20u;
            source -= 0x20u;
            for (u32 word_index = 0; word_index < 8u; ++word_index)
                put(rt, target + 4u * word_index, word(rt, source + 4u * word_index));
        }
        (void)call(rt, ctx, 0x08BBA77Cu, {position, position + (count << 5), frame.at(0x0Cu)});
        return;
    }
    // The position is close enough to the finish that the new elements run past it: move the elements after the
    // position to the end of the new range, fill the gap, then copy the value into the elements that stay.
    (void)call(rt, ctx, 0x08BBAEA0u, {finish, count - after, frame.at(0x0Cu), frame.at(5u)});
    const u32 extended = word(rt, vector + 4u) + ((count - after) << 5);
    put(rt, vector + 4u, extended);
    (void)call(rt, ctx, 0x08BBAF0Cu, {position, finish, extended, frame.at(4u)});
    put(rt, vector + 4u, word(rt, vector + 4u) + (after << 5));
    (void)call(rt, ctx, 0x08BBA77Cu, {position, finish, frame.at(0x0Cu)});
}

// --- a boxed value asked for a float ---
//
// a0 = the box {type tag at +0, value pointer at +4}, a1 = where the float goes. Each arm compares the tag with
// one type's singleton and reads the value through the pointer with that type's conversion. v0 = 1 when an arm
// took it, 0 otherwise.
constexpr u32 kTagOneWordFloat = 0x08B9DDB0u;  // the type whose value is one raw float word
constexpr u32 kTagTwoWords = 0x08C4D18Cu;
constexpr u32 kTagSignedByte = 0x08BC473Cu;
constexpr u32 kTagUnsignedByteA = 0x08BC48B4u;
constexpr u32 kTagSignedHalf = 0x08BC45C4u;
constexpr u32 kTagUnsignedHalf = 0x08BC444Cu;
constexpr u32 kTagWordAsSigned = 0x08BC42D4u;
constexpr u32 kTagUnsignedWord = 0x08B9E8D4u;
constexpr u32 kTagPairFloat = 0x08BC415Cu;
constexpr u32 kTagFixed64 = 0x08BC3FE4u;
constexpr u32 kTagUnsignedByteB = 0x08B9CEBCu;
constexpr u32 kTagSignedByteB = 0x08BC4A2Cu;
constexpr u32 kPairToFloat = 0x08B6EF68u;   // (low word, high word) -> $f0
constexpr u32 kWordsToFloat = 0x08B71EC8u;  // (low word, high word) -> $f0, the two-word arm's own helper
constexpr u32 kFixed64Compare = 0x08B6EE4Cu;  // (low word, high word, 0, 0) -> int

// The tag word is read before the type function runs, as the original's call delay slot does.
bool tag_matches(Runtime &rt, AllegrexContext &ctx, u32 self, u32 type_function) {
    const u32 tag = word(rt, self);
    return tag == call(rt, ctx, type_function, {});
}
// The PSP's integer-to-float sequence for an unsigned word: GCC halves it (keeping the lowest bit) and doubles the
// result, which is exact for every input and is what the original executes.
float float_of_unsigned(u32 value) {
    if (static_cast<s32>(value) >= 0) return static_cast<float>(static_cast<s32>(value));
    const float half = static_cast<float>(static_cast<s32>((value >> 1) + (value & 1u)));
    return half + half;
}
float float_of_signed_byte(Runtime &rt, u32 address) { return static_cast<float>(static_cast<std::int8_t>(rt.memory().load8(address))); }
float float_of_signed_half(Runtime &rt, u32 address) { return static_cast<float>(static_cast<std::int16_t>(rt.memory().load16(address))); }
float float_of_unsigned_half(Runtime &rt, u32 address) { return static_cast<float>(rt.memory().load16(address)); }
float float_of_unsigned_byte(Runtime &rt, u32 address) { return static_cast<float>(rt.memory().load8(address)); }

void func_08C4DC48(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x10u);
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    // Every arm ends this way: the float reaches the caller both through the out pointer and through $f0.
    const auto store = [&](float converted) {
        put_float(rt, out, converted);
        ctx.fpr[0] = converted;
        ctx.gpr[2] = 1u;
    };
    if (tag_matches(rt, ctx, self, kTagOneWordFloat)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(float_at(rt, value));
    }
    if (tag_matches(rt, ctx, self, kTagTwoWords)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(call_keep(rt, ctx, kWordsToFloat, {word(rt, value), word(rt, value + 4u)}).f0);
    }
    if (tag_matches(rt, ctx, self, kTagSignedByte)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(float_of_signed_byte(rt, value));
    }
    if (tag_matches(rt, ctx, self, kTagUnsignedByteA)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(float_of_unsigned_byte(rt, value));
    }
    if (tag_matches(rt, ctx, self, kTagSignedHalf)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(float_of_signed_half(rt, value));
    }
    if (tag_matches(rt, ctx, self, kTagUnsignedHalf)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(float_of_unsigned_half(rt, value));
    }
    if (tag_matches(rt, ctx, self, kTagWordAsSigned)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(static_cast<float>(static_cast<s32>(word(rt, value))));
    }
    if (tag_matches(rt, ctx, self, kTagUnsignedWord)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(float_of_unsigned(word(rt, value)));
    }
    if (tag_matches(rt, ctx, self, kTagPairFloat)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(call_keep(rt, ctx, kPairToFloat, {word(rt, value), word(rt, value + 4u)}).f0);
    }
    if (tag_matches(rt, ctx, self, kTagFixed64)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) {
            const u32 low = word(rt, value);
            const u32 high = word(rt, value + 4u);
            if (static_cast<s32>(call(rt, ctx, kFixed64Compare, {low, high, 0u, 0u})) > 0) {
                return store(call_keep(rt, ctx, kPairToFloat, {low, high}).f0);
            }
            // The 64-bit value halved into (low, high) as the original shifts it, then doubled back.
            const u32 halved_low = (low >> 1) | (high << 31);
            const u32 halved_high = high >> 1;
            const float doubled = call_keep(rt, ctx, kPairToFloat, {(low & 1u) | halved_low, halved_high}).f0;
            return store(doubled + doubled);
        }
    }
    if (tag_matches(rt, ctx, self, kTagUnsignedByteB)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(float_of_unsigned_byte(rt, value));
    }
    if (tag_matches(rt, ctx, self, kTagSignedByteB)) {
        const u32 value = word(rt, self + 4u);
        if (value != 0u) return store(float_of_signed_byte(rt, value));
    }
    ctx.gpr[2] = 0u;
}

// --- a 4x4 linear solve with partial pivoting ---
//
// a0 = the matrix (its 4x4 floats copied into the frame), a1 = the four floats the solution is written to, a2 =
// the four right-hand values. The frame holds four columns of four floats 0x10 bytes apart (column c, row r at
// 0x10 + 0x10c + 4r) and the right-hand side at sp; the pivot search swaps rows in every column, the elimination
// factors the rows below the pivot, and the back substitution divides by the diagonal. v0 = 1 when it solved, 0 at
// a zero pivot.
void func_08C52F8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0xB0u);
    const u32 matrix = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 rhs = ctx.gpr[6];
    const u32 columns = frame.at(0x10u);
    const u32 solution = frame.at(0u);
    put(rt, frame.at(0x50u), out);
    (void)call(rt, ctx, 0x08B4E734u, {columns, matrix});
    for (u32 row = 0u; row < 4u; ++row) put_float(rt, solution + 4u * row, float_at(rt, rhs + 4u * row));
    for (u32 column = 0u; column < 4u; ++column) {
        const bool rows_below = column + 1u < 4u;
        float pivot = std::fabs(float_at(rt, columns + 0x14u * column));
        if (rows_below) {
            for (u32 row = column + 1u; row < 4u; ++row) {
                const float candidate = std::fabs(float_at(rt, columns + 0x10u * column + 4u * row));
                if (pivot < candidate) {
                    for (u32 each = 0u; each < 4u; ++each) {
                        const u32 lane = columns + 0x10u * each;
                        const float below = float_at(rt, lane + 4u * column);
                        const float above = float_at(rt, lane + 4u * row);
                        put_float(rt, lane + 4u * column, above);
                        put_float(rt, lane + 4u * row, below);
                    }
                    const float lower = float_at(rt, solution + 4u * column);
                    const float upper = float_at(rt, solution + 4u * row);
                    put_float(rt, solution + 4u * column, upper);
                    put_float(rt, solution + 4u * row, lower);
                    pivot = candidate;
                }
            }
        }
        if (pivot == 0.0f) {  // a singular matrix (a NaN pivot continues, as the equality does)
            ctx.gpr[2] = 0u;
            return;
        }
        if (!rows_below) continue;
        for (u32 row = column + 1u; row < 4u; ++row) {
            const float factor =
                -float_at(rt, columns + 0x10u * column + 4u * row) / float_at(rt, columns + 0x14u * column);
            (void)call_keepf(rt, ctx, 0x08B4E24Cu, {columns, row, column}, factor);
            put_float(rt, solution + 4u * row,
                      float_at(rt, solution + 4u * row) + psp_mul(factor, float_at(rt, solution + 4u * column)));
        }
    }
    for (s32 index = 3; index >= 0; --index) {
        const u32 row = static_cast<u32>(index);
        float total = float_at(rt, solution + 4u * row);
        put_float(rt, out + 4u * row, total);
        for (u32 later = row + 1u; later < 4u; ++later)
            total -= psp_mul(float_at(rt, out + 4u * later), float_at(rt, columns + 0x10u * later + 4u * row));
        if (row + 1u < 4u) put_float(rt, out + 4u * row, total);
        put_float(rt, out + 4u * row, total / float_at(rt, columns + 0x14u * row));
    }
    ctx.gpr[2] = 1u;
}

// --- a four-value builder: a temporary object, four values, moved into the caller's holder ---
//
// a0 = the caller's holder, a1 = the second argument the temporary's constructor is handed (the original leaves
// $a1 untouched, so the caller's value goes through), a2 = the mode, a3 = the source.
// Mode 0 reads four values into the frame (0x08966B20) and assigns them into a four-element vector; mode 1 builds
// the vector from the source (0x08A98674) and, when it holds exactly four elements, takes each element's virtual
// slot 0x3C and passes the four answers to 0x089668D8. Both release the vector, then the temporary object.
constexpr u32 kValueCtor = 0x08A85B1Cu;     // (element, value) -> a temporary the element may take
constexpr u32 kValueMove = 0x08A5BAD8u;     // (holder, source object)
constexpr u32 kValueRelease = 0x08A5B9ECu;  // (slot)
constexpr u32 kNullObject = 0x08A5BB7Cu;    // (message)

// One element assignment: the temporary is built from the value, the element takes the temporary's object unless
// it is the temporary itself, and the temporary is released.
void assign_element(Runtime &rt, AllegrexContext &ctx, u32 element, u32 slot, u32 value) {
    (void)call(rt, ctx, kValueCtor, {slot, value});
    const u32 temporary = word(rt, slot);
    if (element != slot) (void)call(rt, ctx, kValueMove, {element, temporary});
    (void)call(rt, ctx, kValueRelease, {slot});
}


// --- a record parser with a variable-length stack array ---
//
// a0 = the caller's holder. A temporary object (0x08A98A70) and a 0x290-byte text buffer are made; 0x08B31FAC
// fills the buffer and answers how many records it holds. Each record is 0x14 bytes of the buffer and one word of
// a stack array whose size the count picks (the original's `subu $sp`): the three 0x08A6BBE8 call sites take the
// record's string, its second string and its third field, each moved into the object the call answers unless that
// is the frame slot itself, and 0x08B8A03C stores the record into the object's list at +0x14. The array and the
// temporary are released at the end. v0 is the last release's.
void func_08A2BA74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x70u);
    const u32 fp = frame.at(0u);  // the original's frame pointer; the two frames below it must not move it
    const u32 holder = ctx.gpr[4];
    const u32 object_slot = fp + 0x18u;
    put(rt, fp + 0x24u, holder);
    (void)call(rt, ctx, 0x08A98A70u, {object_slot});
    GuestFrame text_frame(ctx, 0x290u);
    const u32 text = text_frame.at(0u);
    const s32 count = static_cast<s32>(call(rt, ctx, 0x08B31FACu, {0x08D69170u, text, 0x20u}));
    if (count > 0) {
        const u32 offsets = static_cast<u32>(count - 1) * 4u;
        put(rt, fp + 0x20u, offsets);
        GuestFrame array_frame(ctx, (offsets + 0x22u) & ~15u);
        const u32 array = array_frame.at(0u);
        put(rt, fp + 0x2Cu, array);
        for (s32 made = 0; made != count; ++made) (void)call(rt, ctx, 0x08A6BA44u, {array + 4u * static_cast<u32>(made)});
        for (s32 index = 0; index != count; ++index) {
            const u32 record = text + 0x14u * static_cast<u32>(index);
            const u32 element = array + 4u * static_cast<u32>(index);
            u32 moved_to = call(rt, ctx, 0x08A6BBE8u, {element, 0x08CAC004u});
            (void)call(rt, ctx, kValueCtor, {fp + 0x14u, word(rt, record)});
            if (moved_to != fp + 0x14u) (void)call(rt, ctx, kValueMove, {moved_to, word(rt, fp + 0x14u)});
            (void)call(rt, ctx, kValueRelease, {fp + 0x14u});
            moved_to = call(rt, ctx, 0x08A6BBE8u, {element, 0x08CAC00Cu});
            (void)call(rt, ctx, 0x08B85EB8u, {fp + 0x0Cu, record + 4u, fp + 1u});
            (void)call(rt, ctx, 0x08A6D2D4u, {fp + 0x10u, fp + 0x0Cu});
            if (moved_to != fp + 0x10u) (void)call(rt, ctx, kValueMove, {moved_to, word(rt, fp + 0x10u)});
            (void)call(rt, ctx, kValueRelease, {fp + 0x10u});
            const u32 characters = word(rt, fp + 0x0Cu);
            if (characters - 0x0Cu != 0x08D74FD0u) {  // the shared empty string is not released
                const s32 shared = static_cast<s32>(call(rt, ctx, 0x08B60804u, {0x08CAFFFCu, 0xFFFFFFFFu}));
                if (shared <= 0) (void)call(rt, ctx, 0x08B8542Cu, {characters - 0x0Cu, fp});
            }
            moved_to = call(rt, ctx, 0x08A6BBE8u, {element, 0x08CAC018u});
            (void)call(rt, ctx, 0x08A950D0u, {fp + 0x8u, record + 0x0Cu, 6u, 0x10u});
            if (moved_to != fp + 0x8u) (void)call(rt, ctx, kValueMove, {moved_to, word(rt, fp + 0x8u)});
            (void)call(rt, ctx, kValueRelease, {fp + 0x8u});
            const u32 object = word(rt, object_slot);
            if (object == 0u) (void)call(rt, ctx, kNullObject, {0x08CAC044u});
            const u32 list = object + 0x14u;
            (void)call(rt, ctx, 0x08A76ABCu, {fp + 0x4u, element});
            const u32 first = word(rt, list + 4u);
            if (first == word(rt, list + 8u)) {
                (void)call(rt, ctx, 0x08B8A03Cu, {list, word(rt, object + 0x18u), fp + 0x4u});
            } else if (first == 0u) {
                put(rt, list + 4u, 4u);
            } else {
                (void)call(rt, ctx, 0x08A76ABCu, {first, fp + 0x4u});
                put(rt, list + 4u, word(rt, list + 4u) + 4u);
            }
            (void)call(rt, ctx, kValueRelease, {fp + 0x4u});
        }
        if (word(rt, fp + 0x24u) != object_slot)
            (void)call(rt, ctx, kValueMove, {word(rt, fp + 0x24u), word(rt, object_slot)});
        // The array's elements are released from the last one down to the first.
        for (u32 element = array + offsets;; element -= 4u) {
            (void)call(rt, ctx, kValueRelease, {element});
            if (element == array) break;
        }
    } else if (word(rt, fp + 0x24u) != object_slot) {
        (void)call(rt, ctx, kValueMove, {word(rt, fp + 0x24u), word(rt, object_slot)});
    }
    (void)call(rt, ctx, kValueRelease, {object_slot});
}

} // namespace
} // namespace new_batch_main_226
} // namespace gt::game

#define MAIN226(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_226::name, gt::ReturnKind::kind)
MAIN226(0x08A2BA74u, func_08A2BA74, Word);
MAIN226(0x08BAFA9Cu, func_08BAFA9C, Word);
MAIN226(0x08BBB178u, func_08BBB178, Word);
MAIN226(0x08C335FCu, func_08C335FC, Word);
MAIN226(0x08C4DC48u, func_08C4DC48, Word);
MAIN226(0x08C4ECB0u, func_08C4ECB0, Word);
MAIN226(0x08C52F8Cu, func_08C52F8C, Word);

// ---- from main-session batch 227 (draft 1fed1ba202f578b0a991949d5153db9f1cabe608) ----
// New batch 227 (main session): functions emitted by the recompiler that no src/ file defines or
// registers, none of which the gate could reach with useful inputs before the synthetic shapes of
// /tmp/claude-1000/shapes_main227.txt. Written by hand from `tools/disasm` (Ghidra pseudocode as a
// reading aid only); this is a draft batch, not yet verified by the differential gate.
//
// Conventions, as in the other batches: `call_set` writes the argument registers the original sets
// before the jal (a0-a3, t0-t3 in order) and leaves the rest as the caller had them, `call_keep`
// carries the callee's v0 / v1 / f0 on into the context; the delay slot of a call runs BEFORE the
// call; callee-saved registers (s0-s7, fp) are never written here (the original restores them);
// dead stack (saved registers, spills nothing reads) is not reproduced, the frames callees read are.
//
// Static audits after the last edit: `tools.frame_audit` 0 wrong, `tools.literal_check` 8 functions and
// 0 literals missing, `tools.arg_reads` 0 calls (the two that take the caller's untouched a1 pass it
// explicitly). `tools.sp_loads` lists nine callee-saved reloads, all modelled here as words of the
// frame: 0881E0EC / 0881E198 / 0881E24C / 0881E2CC (the collection in sp+8, the array in sp+0x10, the
// list in sp+0xC), 089BB048 (the found object in sp+0x20), 089DC860 / 089DC9E4 / 089DCA50 (the shape in
// sp+0x14) and 089DCA18 (the holder the slot-0x84 call fills at sp).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_227 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 sign_fill(u32 value) { return static_cast<u32>(static_cast<s32>(value) >> 31); }
inline u32 arithmetic_shift(u32 value, unsigned by) { return static_cast<u32>(static_cast<s32>(value) >> by); }

// A guest call whose v0 / v1 / f0 stay in the context like the original's (a callee that writes no v0
// hands the previous one on to the code after it — call_guest starts it with the caller's v0).
inline GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address,
                            std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// A call with the argument registers set as the original sets them before the jal.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}

// The word at `offset` of the vtable the object's first word points to (the original's `lw v1, 0(o)`,
// `lw v0, N(v1)`, `jalr v0`).
inline u32 slot_of(Runtime &rt, u32 object, u32 offset) { return word(rt, word(rt, object) + offset); }

// The release of a string cell whose data pointer is `data` (hot_batch_95's release_cell, with the
// caller's frame base as the flag): nothing when data - 0xC is the shared empty cell 0x08D74FD0,
// otherwise 0x08B60804(data - 4, -1) and, when its result is not positive, 0x08B8542C(data - 0xC, base).
// a1 after the release is not observable here: both call sites set it for their next call.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 data, u32 base) {
    if (data - 0xCu == 0x08D74FD0u) return;
    const u32 references = call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu});
    if (signed_of(references) <= 0) (void)call_set(rt, ctx, 0x08B8542Cu, {data - 0xCu, base});
}

// A call with one float argument in $f12 (0x08A87EEC's shape).
GuestResult call_float(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// --- the scene lookup 0x089BAF40 ----------------------------------------------------------
//
// a0 = the scene, a1 = a holder whose word is a counted block (its header 0xC bytes before it). With
// that header zero the scene's key holder at +0x58 is reset to an empty one (0x08A696E4 /
// 0x08A6A23C / 0x08A69D68) and the function ends. Otherwise 0x08A7BDBC(self) answers a search
// object, the key at sp+0x14 is built (0x08A5D438) and looked up with the scene's name
// (0x08A7BCC0) through 0x08A7D5FC; a miss destroys the key. The cache 0x089BA9B4(self) may be null
// (then the argument's holder is copied into the scene's +0x58 and the key destroyed). With the
// cache, a second probe at sp+0x20 over the pair at sp+0x18 / sp+0x1C decides: a hit takes the value
// holder at sp+0x24 (the object's slot 0x1C, called with sp+0x24 and sp+0xC), compares it with the
// scene, and either keeps, inserts (0x08A7DE18 / 0x08A7F5FC / 0x08A6A23C / 0x08A7E6E0 around an
// object_retain + object_release pair) or just stores the scene's +0x58 into the cache entry; a miss
// (or a null probe) resets +0x58 from the wanted holder and stores the entry with 0x08A7E6E0.
// v0 is the last call's (the key holder's destruction), never a constant.
void scene_lookup(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    if (word(rt, word(rt, argument) - 0xCu) == 0u) {
        const u32 temporary = frame.at(0x10u);
        (void)call_set(rt, ctx, 0x08A696E4u, {temporary});
        (void)call_set(rt, ctx, 0x08A6A23Cu, {self + 0x58u, temporary});
        (void)call_set(rt, ctx, 0x08A69D68u, {temporary});
        return;
    }
    const u32 search = call_set(rt, ctx, 0x08A7BDBCu, {self});
    if (search == 0u) return;  // v0 = 0, as the search left it
    const u32 key = frame.at(0x14u);
    const u32 stored = frame.at(0x18u);
    const u32 wanted = frame.at(0x1Cu);
    const u32 probe = frame.at(0x20u);
    (void)call_set(rt, ctx, 0x08A5D438u, {key});
    const u32 name = call_set(rt, ctx, 0x08A7BCC0u, {self});
    (void)call_set(rt, ctx, 0x08A7D5FCu, {search, key, name});
    if (word(rt, key) == 0u) {
        (void)call_set(rt, ctx, 0x08A5B9ECu, {key});
        return;
    }
    const u32 cache = call_set(rt, ctx, 0x089BA9B4u, {self});
    if (cache == 0u) {
        const u32 temporary = frame.at(0u);
        (void)call_set(rt, ctx, 0x08A69654u, {temporary, argument});
        (void)call_set(rt, ctx, 0x08A6A23Cu, {self + 0x58u, temporary});
        (void)call_set(rt, ctx, 0x08A69D68u, {temporary});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {key});
        return;
    }
    (void)call_set(rt, ctx, 0x08A6972Cu, {stored, self + 0x58u});
    (void)call_set(rt, ctx, 0x08A69654u, {wanted, argument});
    (void)call_set(rt, ctx, 0x08A5D438u, {probe});
    (void)call_set(rt, ctx, 0x08A7D5FCu, {cache, probe, stored});
    if (word(rt, probe) == 0u) {
        if (word(rt, stored) != word(rt, wanted))
            (void)call_set(rt, ctx, 0x08A6A23Cu, {self + 0x58u, wanted});
        (void)call_set(rt, ctx, 0x08A7E6E0u, {cache, self + 0x58u, key});
    } else {
        const u32 value = frame.at(0x24u);
        const u32 slot = frame.at(0xCu);
        (void)call_set(rt, ctx, 0x08A76AA0u, {value});
        const u32 found = word(rt, probe);
        if (found == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5CA0u});
        const u32 method = word(rt, word(rt, found) + 0x1Cu);
        (void)call_set(rt, ctx, 0x08A76AA0u, {slot});
        (void)call_keep(rt, ctx, method, {found, value, slot});  // a0 = the object, a1 / a2 the holders
        (void)call_set(rt, ctx, 0x08A5B9ECu, {slot});
        if (word(rt, value) != self) {
            const u32 inserted = frame.at(4u);
            (void)call_set(rt, ctx, 0x08A7F5FCu, {inserted, cache, wanted});
            (void)call_set(rt, ctx, 0x08A6A23Cu, {self + 0x58u, inserted});
            (void)call_set(rt, ctx, 0x08A69D68u, {inserted});
            (void)call_set(rt, ctx, 0x08A7E6E0u, {cache, self + 0x58u, key});
        } else if (word(rt, stored) != word(rt, wanted)) {
            const u32 inserted = frame.at(8u);
            (void)call_set(rt, ctx, 0x08A5BA28u, {self});
            (void)call_set(rt, ctx, 0x08A7DE18u, {cache, stored});
            (void)call_set(rt, ctx, 0x08A7F5FCu, {inserted, cache, wanted});
            (void)call_set(rt, ctx, 0x08A6A23Cu, {self + 0x58u, inserted});
            (void)call_set(rt, ctx, 0x08A69D68u, {inserted});
            (void)call_set(rt, ctx, 0x08A7E6E0u, {cache, self + 0x58u, key});
            (void)call_set(rt, ctx, 0x08A5B970u, {self});
        }
        (void)call_set(rt, ctx, 0x08A5B9ECu, {value});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {probe});
    (void)call_set(rt, ctx, 0x08A69D68u, {wanted});
    (void)call_set(rt, ctx, 0x08A69D68u, {stored});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {key});
}

// --- the image-shape floats 0x089DC808 ----------------------------------------------------
//
// a0 = the output holder, a2 = the mode, a3 = a word whose first word is the holder object. The shape
// at sp+0x14 comes from 0x089DB3B4. Mode 0: a 4-element vector at sp+0x20 (0x08A98C1C) is filled with
// the four floats of the block 0x089DA904(shape) — each element replaced through a temporary holder
// at sp+0x10 / sp+0xC / sp+8 / sp+4 that 0x08A87EEC($f12) builds — and the vector replaces *out.
// Mode 1: the object's slot 0x7C gives a count, up to four floats are read from the sub-objects the
// object's slot 0x84 (a0 = sp, a1 = object, a2 = index) answers, each through that object's slot
// 0x54, into sp+0x20..sp+0x2C (all 1.0 first, sp+0x30 = -1); the shape is then updated through
// 0x089DA90C(shape, sp+0x20). Any other mode does nothing but the shape's destruction. v0 is the
// last call's (the shape holder's destruction at sp+0x14), not a constant.
void image_shape_floats(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 out = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 holder_pointer = ctx.gpr[7];
    const u32 shape_slot = frame.at(0x14u);
    (void)call_set(rt, ctx, 0x089DB3B4u, {shape_slot, ctx.gpr[5]});  // a1 is the caller's, never set here
    ctx.gpr[2] = 1u;  // the asm's v0 = 1 before the mode test
    if (mode == 0u) {
        (void)call_set(rt, ctx, 0x08A98C1Cu, {frame.at(0x20u), 4u});
        u32 shape = word(rt, shape_slot);
        if (shape == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA752Cu});
        const u32 floats = call_set(rt, ctx, 0x089DA904u, {shape});
        for (u32 index = 0u; index < 4u; ++index) {
            const u32 element = word(rt, word(rt, frame.at(0x20u)) + 0x14u) + 4u * index;
            const u32 temporary = frame.at(0x10u - 4u * index);
            (void)call_float(rt, ctx, 0x08A87EECu, {temporary}, {float_at(rt, floats + 4u * index)});
            if (element != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {element, word(rt, temporary)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        }
        if (out != frame.at(0x20u)) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0x20u))});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x20u)});
    } else if (mode == 1u) {
        u32 holder = word(rt, holder_pointer);
        if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA74F8u});
        const u32 count = call_indirect(rt, ctx, slot_of(rt, holder, 0x7Cu), {holder});
        put_float(rt, frame.at(0x20u), 1.0f);
        put(rt, frame.at(0x30u), 0xFFFFFFFFu);
        put_float(rt, frame.at(0x2Cu), 1.0f);
        put_float(rt, frame.at(0x28u), 1.0f);
        put_float(rt, frame.at(0x24u), 1.0f);
        if (count != 0u) {
            for (u32 index = 0u; index != 4u && index != count; ++index) {
                holder = word(rt, holder_pointer);
                if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA74F8u});
                const u32 target = frame.at(0u);
                const u32 vtable = word(rt, holder);  // the asm's v0 at the jalr (the slot goes in v1)
                ctx.gpr[2] = vtable;
                (void)call_keep(rt, ctx, word(rt, vtable + 0x84u), {target, holder, index});
                u32 value = word(rt, target);
                if (value == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA5CA0u});
                const GuestResult read = call_indirect(rt, ctx, slot_of(rt, value, 0x54u), {value}, {});
                put_float(rt, frame.at(0x20u) + 4u * index, read.f0);
                (void)call_set(rt, ctx, 0x08A5B9ECu, {target});
            }
        }
        holder = word(rt, shape_slot);
        if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA752Cu});
        (void)call_set(rt, ctx, 0x089DA90Cu, {holder, frame.at(0x20u)});
    }
    (void)call_set(rt, ctx, 0x08A5B9ECu, {shape_slot});
}

// --- the menu car list 0x0881E0AC (MCarCollection::getCarsForMenu, adhoc registrar 0x383108) -----
//
// a0 = the output holder, a3 = the menu argument list. The collection object (0x0881BD98 at sp+8,
// 0x0881B068 to its engine object at sp+0x24) and the argument list (0x08A98674 at sp+0xC) are
// resolved; an empty array is built at sp+0x10 (0x08A98A70). For each argument (the list's
// [begin, end) word pairs at +0x14 / +0x18), the object's slot 0x50 answers two words, which
// 0x08930458 splits into a count (s6) and a key (s5 = the slot's v1, read in the call's delay slot);
// 0x0881A840(engine, -, slot's v0, key) then returns the flag buffer. Each of the count bytes gives
// the item count in its low nibble at +2: a positive one builds an item at sp+4 (0x0884CD28 with the
// index and the count) and pushes it into the array's vector at +0x14; when no item was built for an
// argument at all, one item with a zero index and count is built and pushed instead. Finally the
// array replaces *out (0x08A5BAD8) and the array and the list are destroyed.
void menu_car_list(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    const u32 out = ctx.gpr[4];
    const u32 arguments = ctx.gpr[7];
    const u32 self_holder = frame.at(8u);
    const u32 item = frame.at(4u);
    (void)call_set(rt, ctx, 0x0881BD98u, {self_holder, ctx.gpr[5]});  // a1 is the caller's, never set here
    u32 collection = word(rt, self_holder);
    if (collection == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    const u32 engine = call_set(rt, ctx, 0x0881B068u, {collection});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {self_holder});
    (void)call_set(rt, ctx, 0x08A98674u, {frame.at(0xCu), arguments});
    (void)call_set(rt, ctx, 0x08A98A70u, {frame.at(0x10u)});
    u32 index = 0u;
    for (;;) {
        const u32 list = word(rt, frame.at(0xCu));
        if (list == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99264u});
        const u32 begin = word(rt, list + 0x14u);
        if (!(index < arithmetic_shift(word(rt, list + 0x18u) - begin, 2))) break;
        u32 object = word(rt, begin + 4u * index);
        if (object == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98EFCu});
        const GuestResult method = call_indirect(rt, ctx, slot_of(rt, object, 0x50u), {object}, {});
        const u32 object_v0 = method.v0;
        const u32 object_v1 = method.v1;  // the call's delay slot keeps it for 0x0881A840
        const s32 count = static_cast<s32>(call_set(rt, ctx, 0x08930458u, {object_v0, object_v1}));
        const GuestResult items = call_keep(rt, ctx, 0x0881A840u, {engine, 0u, object_v0, object_v1});
        bool built = false;  // the asm's v1 flag is sticky over the inner loop
        if (count > 0) {
            const u32 buffer = items.v0;
            for (u32 position = 0u; position < static_cast<u32>(count); ++position) {
                const u32 length = rt.memory().load8(buffer + position + 2u) & 0xFu;
                if (length == 0u) continue;
                u32 array = word(rt, frame.at(0x10u));
                if (array == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99264u});
                array += 0x14u;
                (void)call_set(rt, ctx, 0x0884CD28u, {item, 0u, object_v0, object_v1, position, length});
                const u32 end = word(rt, array + 4u);
                if (end == word(rt, array + 8u)) {
                    (void)call_set(rt, ctx, 0x08B8A03Cu, {array, end, item});
                } else {
                    if (end != 0u) (void)call_set(rt, ctx, 0x08A76ABCu, {end, item});
                    put(rt, array + 4u, word(rt, array + 4u) + 4u);
                }
                (void)call_set(rt, ctx, 0x08A5B9ECu, {item});
                built = true;
            }
        }
        if (!built) {
            u32 array = word(rt, frame.at(0x10u));
            if (array == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08C99264u});
            array += 0x14u;
            (void)call_set(rt, ctx, 0x0884CD28u, {frame.at(0u), 0u, object_v0, object_v1, 0u, 0u});
            const u32 end = word(rt, array + 4u);
            if (end == word(rt, array + 8u)) {
                (void)call_set(rt, ctx, 0x08B8A03Cu, {array, end, frame.at(0u)});
            } else {
                if (end != 0u) (void)call_set(rt, ctx, 0x08A76ABCu, {end, frame.at(0u)});
                put(rt, array + 4u, word(rt, array + 4u) + 4u);
            }
            (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
        ++index;
    }
    const u32 array = frame.at(0x10u);
    if (out != array) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, array)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {array});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
}

// --- the member list 0x08A3013C -----------------------------------------------------------
//
// a0 = the output holder. The engine 0x08B09A04(0x08CF6648) is asked (0x08B2FC48) for up to 0x10
// records of 0x98 bytes at sp+0x20; the count it returns (0 or less ends the function) sizes a vector
// at sp+0x1C (0x08A98C1C) and a variable-length array of 4-byte holders just below the frame
// (0x08A6BA44 each). Per record, four labeled parts are appended to that record's holder — the 64-bit
// value at +0/+4 through 0x08A5DEE8, the text at +0x10 through 0x08B85EB8 and 0x08A6D2D4, the word
// at +8 through 0x08A85B1C, the byte at +0x90 through 0x08A97A6C — and the holder is assigned into
// the vector's element `i` (0x08A76ABC). The vector then replaces *out (0x08A5BAD8); the array's
// holders and the vector at sp+0x1C are destroyed, the array in reverse order.
void member_list(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xA00u);
    const u32 fp = frame.at(0u);
    const u32 out = ctx.gpr[4];
    const u32 engine = call_set(rt, ctx, 0x08B09A04u, {0x08CF6648u});
    const s32 count = static_cast<s32>(call_set(rt, ctx, 0x08B2FC48u, {engine, fp + 0x20u, 0x10u, 1u, 1u}));
    if (count <= 0) return;
    const u32 vector = fp + 0x1Cu;
    (void)call_set(rt, ctx, 0x08A98C1Cu, {vector, static_cast<u32>(count)});
    const u32 array_base = ctx.gpr[29] - ((static_cast<u32>(count) - 1u) * 4u + 0x20u);
    ctx.gpr[29] = array_base;
    for (u32 index = 0u; index < static_cast<u32>(count); ++index)
        (void)call_set(rt, ctx, 0x08A6BA44u, {array_base + 4u * index});
    const u32 empty = fp + 4u;
    for (u32 index = 0u; index < static_cast<u32>(count); ++index) {
        const u32 record = fp + 0x20u + 0x98u * index;
        const u32 text = fp + 0x30u + 0x98u * index;
        const u32 holder = array_base + 4u * index;
        // the 64-bit value at the record's +0 / +4
        {
            const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {holder, 0x08CAC7BCu});
            const u32 temporary = fp + 0x18u;
            (void)call_set(rt, ctx, 0x08A5DEE8u, {temporary, 0u, word(rt, record), word(rt, record + 4u)});
            if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        }
        // the text at the record's +0x10
        {
            const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {holder, 0x08CAC7C4u});
            const u32 made = fp + 0x10u, part = fp + 0x14u;
            (void)call_set(rt, ctx, 0x08B85EB8u, {made, text, fp + 1u});
            (void)call_set(rt, ctx, 0x08A6D2D4u, {part, made});
            if (slot != part) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, part)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {part});
            release_string(rt, ctx, word(rt, made), fp);
        }
        // the word at the record's +8
        {
            const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {holder, 0x08CAC7D0u});
            const u32 temporary = fp + 0xCu;
            (void)call_set(rt, ctx, 0x08A85B1Cu, {temporary, word(rt, record + 8u)});
            if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        }
        // the byte at the record's +0x90
        {
            const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {holder, 0x08CAC7D8u});
            const u32 temporary = fp + 8u;
            (void)call_set(rt, ctx, 0x08A97A6Cu, {temporary, rt.memory().load8(record + 0x90u)});
            if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
            (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        }
        // the holder becomes the vector's element `index` (the element's address is computed in the
        // call's delay slot, from the vector's begin pointer)
        const u32 element = word(rt, word(rt, vector) + 0x14u) + 4u * index;
        (void)call_set(rt, ctx, 0x08A76ABCu, {empty, holder});
        if (element != empty) (void)call_set(rt, ctx, 0x08A5BAD8u, {element, word(rt, empty)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {empty});
    }
    if (out != vector) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, vector)});
    for (u32 index = static_cast<u32>(count); index > 0u; --index)
        (void)call_set(rt, ctx, 0x08A5B9ECu, {array_base + 4u * (index - 1u)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {vector});
    ctx.gpr[29] = fp;
}

// --- the shared-object text 0x08A30594 ----------------------------------------------------
//
// a0 = the output holder, a2 = a count (0 or less returns before anything happens, leaving v0 as the
// caller had it), a3 = a word whose first word is the HObject. Builds a line into the string builder
// at sp+0x28 (0x08A6BA44) out of the lookup 0x08B2F8A4(engine, object's slot 0x3C, sp+0x30):
// "address" + the word at sp+0x30, "nickname" + the pointer at sp+0x34 (converted into sp+0xD8 by
// 0x08A950D0, or the constant 0x08CF6ED8 when null), "index" + the pointer again, a fourth part that
// 0x08B85EB8 builds at sp+0x10 out of the text at sp+0x48, the 64-bit value at sp+0x38 through
// 0x08A5DEE8, the 64-bit value at sp+0xD0 through 0x08A83160; then the builder's text is copied to
// sp+4 (0x08A76ABC) and assigned to *out (0x08A5BAD8) before every temporary is destroyed.
// Every append has the same shape: the builder's slot for the label (0x08A6BBE8) in `slot`, a
// temporary at a fixed stack offset that a constructor fills, the assignment into the slot when the
// slot is not the temporary itself, and the temporary's destruction (0x08A5B9EC).
void shared_object_text(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x200u);
    const u32 out = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 holder_pointer = ctx.gpr[7];
    if (count <= 0) return;
    const u32 holder = word(rt, holder_pointer);
    const u32 engine = call_set(rt, ctx, 0x08B09A04u, {0x08CF6648u});
    if (holder == 0u) (void)call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAC378u});
    const u32 name = call_indirect(rt, ctx, slot_of(rt, holder, 0x3Cu), {holder});
    if (call_set(rt, ctx, 0x08B2F8A4u, {engine, name, frame.at(0x30u)}) == 0u) return;
    const u32 builder = frame.at(0x28u);
    (void)call_set(rt, ctx, 0x08A6BA44u, {builder});
    // "address" + the word at sp+0x30
    {
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAC7ECu});
        const u32 temporary = frame.at(0x24u);
        (void)call_set(rt, ctx, 0x08A85B1Cu, {temporary, word(rt, frame.at(0x30u))});
        if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    }
    const u32 nickname = word(rt, frame.at(0x34u));
    if (nickname != 0u) {
        // "nickname" + the pointer converted into the 16-element buffer at sp+0xD8
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAC7F0u});
        const u32 temporary = frame.at(0x20u);
        (void)call_set(rt, ctx, 0x08A950D0u, {temporary, frame.at(0xD8u), nickname, 0x10u});
        if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    } else {
        // "nickname" + the constant holder 0x08CF6ED8
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAC7F0u});
        const u32 temporary = frame.at(0x1Cu);
        (void)call_set(rt, ctx, 0x08A76AACu, {temporary, 0x08CF6ED8u});
        if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    }
    // "index" + the pointer again
    {
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAC7F8u});
        const u32 temporary = frame.at(0x18u);
        (void)call_set(rt, ctx, 0x08A85B1Cu, {temporary, nickname});
        if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    }
    // the fourth part: 0x08B85EB8(sp+0x10, sp+0x48, sp+1) then a copy at sp+0x14
    {
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAC800u});
        const u32 made = frame.at(0x10u), part = frame.at(0x14u);
        (void)call_set(rt, ctx, 0x08B85EB8u, {made, frame.at(0x48u), frame.at(1u)});
        (void)call_set(rt, ctx, 0x08A6D2D4u, {part, made});
        if (slot != part) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, part)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {part});
        release_string(rt, ctx, word(rt, frame.at(0x10u)), frame.at(0u));
    }
    // the 64-bit value at sp+0x38 through 0x08A5DEE8
    {
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAC80Cu});
        const u32 temporary = frame.at(0xCu);
        (void)call_set(rt, ctx, 0x08A5DEE8u,
                       {temporary, 0u, word(rt, frame.at(0x38u)), word(rt, frame.at(0x3Cu))});
        if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    }
    // the 64-bit value at sp+0xD0 through 0x08A83160
    {
        const u32 slot = call_set(rt, ctx, 0x08A6BBE8u, {builder, 0x08CAC820u});
        const u32 temporary = frame.at(8u);
        (void)call_set(rt, ctx, 0x08A83160u,
                       {temporary, 0u, word(rt, frame.at(0xD0u)), word(rt, frame.at(0xD4u))});
        if (slot != temporary) (void)call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, temporary)});
        (void)call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    }
    // the builder's text copied to sp+4, assigned to *out, destroyed
    const u32 copy = frame.at(4u);
    (void)call_set(rt, ctx, 0x08A76ABCu, {copy, builder});
    if (out != copy) (void)call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, copy)});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {copy});
    (void)call_set(rt, ctx, 0x08A5B9ECu, {builder});
}

// --- the reply wait 0x08B477D4 -----------------------------------------------------------
//
// a0 = connection object, a1 / a2 / a3 = three words handed to the message builder (sp+0x5C,
// sp+0x58 and sp+0x60 of the local message), t0 = a peer word (a negative one returns 0 before
// anything happens), t1 = a mode byte. The function builds the message at sp+0x48, asks the object
// (0x08B464E0) whether something is pending for the peer, and then either sends (0x08C7AE48) or
// waits for the answer (0x08C7AE08, timeout 1000000). Returns 1 when the exchange went through and
// 0 otherwise. The 0x8041xxxx results are the network layer's error codes: 0x80410001 (not found)
// and a negative one other than 0x8041070C is a failure, 0x8041070C (timeout) takes the long path
// that copies 0x44 bytes of the peer's block back through the object's virtual slot 0x80.
// Every exit after the message has been built destroys it first (0x08B47ACC(sp+0x48), the shared tail
// at 0x08B47A6C); only the two checks before the build return without it.
u32 reply_exchange(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x5A0u);
    const u32 self = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 third = ctx.gpr[7];
    const u32 peer = ctx.gpr[8];
    const u32 mode = ctx.gpr[9] & 0xFFu;
    if (rt.memory().load8(self + 0x8Cu) == 0u) return 0u;   // 0x08B47A7C: no message yet, nothing to destroy
    if (signed_of(peer) < 0) return 0u;
    u32 result;
    const u32 message = frame.at(0x48u);
    (void)call_set(rt, ctx, 0x08B47AB8u, {message});
    const GuestResult status = call_indirect(rt, ctx, slot_of(rt, self, 0x3Cu), {self}, {});
    put(rt, frame.at(0x50u), status.v0);
    put(rt, frame.at(0x54u), status.v1);
    put(rt, frame.at(0x5Cu), first);
    (void)call_set(rt, ctx, 0x08B47AE8u, {message, second, third});
    put(rt, frame.at(0u), 0u);
    if (call_set(rt, ctx, 0x08B464E0u, {self, peer}) != 0u) {
        (void)call_set(rt, ctx, 0x08B3FA98u, {frame.at(4u)});
        if (call_set(rt, ctx, 0x08B47708u, {self, peer, 0u, frame.at(4u)}) == 0u) {
            result = 0u;
        } else {
            const u32 object = call_set(rt, ctx, 0x08B3F8A0u, {frame.at(4u)});
            const u32 short_value = call_set(rt, ctx, 0x08B3FA54u, {frame.at(4u)});
            const u32 fourth = call_set(rt, ctx, 0x08B47BC0u, {message, 0u});
            const u32 fifth = call_set(rt, ctx, 0x08B47BB4u, {message});
            const u32 waited =
                call_set(rt, ctx, 0x08C7AE48u, {peer, object, short_value & 0xFFFFu, fourth, fifth, 0xF4240u, mode});
            if (signed_of(waited) < 0) {
                result = 0u;
            } else {
                put(rt, frame.at(0u), call_set(rt, ctx, 0x08B47BB4u, {message}));
                result = 1u;
            }
        }
    } else {
        put(rt, frame.at(0u), call_set(rt, ctx, 0x08B47BB4u, {message}));
        const u32 sent = call_set(rt, ctx, 0x08B47BC0u, {message, 0u});
        const u32 outcome = call_set(rt, ctx, 0x08C7AE08u, {peer, sent, frame.at(0u), 0xF4240u, mode});
        if (outcome == 0x80410001u) {
            result = 0u;
        } else if (signed_of(outcome) >= 0) {
            result = 1u;
        } else if (outcome != 0x8041070Cu) {
            result = 0u;
        } else {
            // 0x8041070C: the timeout path, the peer's 0x44-byte block copied back at sp+4
            (void)call_indirect(rt, ctx, slot_of(rt, self, 0x14u), {self});
            const u32 block = call_indirect(rt, ctx, slot_of(rt, self, 0x80u), {self, peer});
            if (block == 0u) {
                (void)call_indirect(rt, ctx, slot_of(rt, self, 0xA4u), {self, peer});
            } else {
                // two empty holder objects at sp+8 and sp+0x1E, then the zeros at sp+0x34..sp+0x47
                // (overwritten by the copy) and the 0x11 words of the peer's block at sp+4
                put(rt, frame.at(4u), 0u);
                (void)call_set(rt, ctx, 0x08B3FA98u, {frame.at(8u)});
                (void)call_set(rt, ctx, 0x08B3FA98u, {frame.at(0x1Eu)});
                (void)call_set(rt, ctx, 0x08B3F838u, {frame.at(8u)});
                (void)call_set(rt, ctx, 0x08B3F838u, {frame.at(0x1Eu)});
                rt.memory().store8(frame.at(0x34u), 0u);
                rt.memory().store8(frame.at(0x35u), 0u);
                rt.memory().store8(frame.at(0x36u), 0u);
                put(rt, frame.at(0x38u), 0u);
                put(rt, frame.at(0x3Cu), 0u);
                put(rt, frame.at(0x40u), 0u);
                put(rt, frame.at(0x44u), 0u);
                for (u32 index = 0u; index < 0x11u; ++index)
                    put(rt, frame.at(4u) + 4u * index, word(rt, block + 4u * index));
                (void)call_indirect(rt, ctx, slot_of(rt, self, 0xA4u), {self, peer});
                (void)call_indirect(rt, ctx, slot_of(rt, self, 0x94u), {self, frame.at(4u)});
            }
            (void)call_indirect(rt, ctx, slot_of(rt, self, 0x18u), {self});
            result = 0u;
        }
    }
    (void)call_set(rt, ctx, 0x08B47ACCu, {message});
    return result;
}

// --- the parse-token dispatcher 0x08B39148 ------------------------------------------------
//
// Every node is an intrusive counted object: the vtable at +0, the count at +4. `push_node` is the
// asm's store into the holder at `slot` (0x08B392BC..0x08B39388 and 0x08B39368): the old node's count
// is decremented (the decremented word is stored in the branch delay slot, before the conditional
// dispose call), the new node stored, and its count incremented.
void push_node(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 node) {
    const u32 old = word(rt, slot);
    if (old != 0u) {
        const u32 count = word(rt, old + 4u) - 1u;
        put(rt, old + 4u, count);
        if (count == 0u) (void)call_set(rt, ctx, word(rt, word(rt, old) + 4u), {old});
    }
    put(rt, slot, node);
    put(rt, node + 4u, word(rt, node + 4u) + 1u);
}

// The allocator call 0x08B62344(size); zero means one byte there.
u32 allocate_node(Runtime &rt, AllegrexContext &ctx, u32 size) { return call_set(rt, ctx, 0x08B62344u, {size}); }

// A 0x0C-byte leaf whose only value is a word at +8 (case '$', the default token and '^').
u32 make_word_leaf(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 value) {
    const u32 node = allocate_node(rt, ctx, 0xCu);
    put(rt, node, vtable);
    put(rt, node + 8u, value);
    put(rt, node + 4u, 0u);
    return node;
}
// The same leaf with a halfword at +8 and a zero halfword at +0xA (the 0x08CD93B0 leaf).
u32 make_half_leaf(Runtime &rt, AllegrexContext &ctx, u32 vtable, u32 value) {
    const u32 node = allocate_node(rt, ctx, 0xCu);
    put(rt, node, vtable);
    rt.memory().store16(node + 8u, static_cast<std::uint16_t>(value));
    put(rt, node + 4u, 0u);
    rt.memory().store16(node + 0xAu, 0u);
    return node;
}

// The '.' node (0x18 bytes, vtable 0x08CDA938): a vector of 8-byte elements at +8 (begin / end / cap
// at +8 / +0xC / +0x10) which gets one zero element pushed into it, and a zero byte at +0x14.
void push_range_node(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self) {
    const u32 node = allocate_node(rt, ctx, 0x18u);
    put(rt, node + 4u, 0u);
    put(rt, node + 0xCu, 0u);
    put(rt, node + 0x10u, 0u);
    put(rt, node + 8u, 0u);
    put(rt, node, 0x08CDA938u);
    rt.memory().store8(node + 0x14u, 0u);
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    const u32 begin = word(rt, node + 0xCu);
    if (begin == word(rt, node + 0x10u)) {
        (void)call_set(rt, ctx, 0x08C4F48Cu, {node + 8u, begin, frame.at(0u)});
    } else {
        if (begin != 0u) {
            put(rt, begin + 4u, 0u);
            put(rt, begin, 0u);
        }
        put(rt, node + 0xCu, begin + 8u);
    }
    push_node(rt, ctx, self + 0x10u, node);
}

void token_dispatch(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    s32 token = static_cast<s32>(call_set(rt, ctx, 0x08B38938u, {self}));
    u32 result = 0u;  // what v1 becomes on every path (v0 = v1 at the epilogue)
    switch (token) {
    case 0x24:  // '$': the token stream is at its end, else a value reference
        if (rt.memory().load8(word(rt, self + 8u)) == 0u)
            push_node(rt, ctx, self + 0x10u, make_half_leaf(rt, ctx, 0x08CD93B0u, 0xBu));
        else
            push_node(rt, ctx, self + 0x10u, make_word_leaf(rt, ctx, 0x08CD9388u, 0x24u));
        break;
    case 0x28: result = 2u; break;   // '('
    case 0x29: result = 3u; break;   // ')'
    case 0x2A: result = 6u; break;   // '*'
    case 0x2B: result = 7u; break;   // '+'
    case 0x2E:                       // '.': a range node
        push_range_node(rt, ctx, frame, self);
        break;
    case 0x3F: result = 8u; break;   // '?'
    case 0x5B:                       // '[': the sub-parser
        result = call_set(rt, ctx, 0x08B38E74u, {self});
        break;
    case 0x5C:                       // '\': the next token is taken and becomes the leaf's value
        token = static_cast<s32>(call_set(rt, ctx, 0x08B38938u, {self}));
        if (token == -1) {
            result = 0xDu;
            break;
        }
        push_node(rt, ctx, self + 0x10u, make_word_leaf(rt, ctx, 0x08CD9388u, static_cast<u32>(token)));
        break;
    case 0x5E:  // '^': at the end of the stream a 0x0A leaf, else a value reference
        if (word(rt, self + 8u) - 1u == word(rt, self + 4u))
            push_node(rt, ctx, self + 0x10u, make_half_leaf(rt, ctx, 0x08CD93B0u, 0xAu));
        else
            push_node(rt, ctx, self + 0x10u, make_word_leaf(rt, ctx, 0x08CD9388u, 0x5Eu));
        break;
    case 0x7B: result = 4u; break;   // '{'
    case 0x7C: result = 1u; break;   // '|'
    case 0x7D: result = 5u; break;   // '}'
    case -1:
        result = 0xDu;
        break;
    default:  // any other token: a value reference carrying the token
        push_node(rt, ctx, self + 0x10u, make_word_leaf(rt, ctx, 0x08CD9388u, static_cast<u32>(token)));
        break;
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// --- the buffered reader 0x08AC48D0 ------------------------------------------------------
//
// a0 = a buffered stream (the iostream wrapper at +8, a 64-bit position at +0x120, the buffered
// window at +0x128: its 64-bit start and the 32-bit byte count at +0x130, the bytes at +0x134),
// a1 = destination, a2 = requested count. Returns the bytes delivered: the window is used while the
// position lies inside it, otherwise the stream is seeked to the 64-bit position rounded down to 64
// bytes (0 when the position is at or past the window's end with equal high words) and a chunk of
// (request rounded up to 1024 bytes, clamped to 0x2000, at least 0x400) bytes is read into it again.
u32 buffered_read(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 self = ctx.gpr[4];
    u32 destination = ctx.gpr[5];
    u32 size = ctx.gpr[6];
    u32 total = 0u;
    if (size == 0u) return 0u;
    for (;;) {
        u32 wrapped = 0u;
        u32 count = 0u;
        for (;;) {
            const u32 position_lo = word(rt, self + 0x120u);
            const u32 position_hi = word(rt, self + 0x124u);
            const u32 available = word(rt, self + 0x130u);
            const u32 base_lo = word(rt, self + 0x128u);
            const u32 base_hi = word(rt, self + 0x12Cu);
            const u32 end_lo = base_lo + available;
            const u32 end_hi = base_hi + (end_lo < base_lo ? 1u : 0u) + sign_fill(available);
            const u32 want_lo = position_lo + size;
            const u32 want_hi = position_hi + (want_lo < position_lo ? 1u : 0u);
            // inside the window: position >= start (the start's word pair <= the position's) and
            // position < end; the high words compare signed, the low ones unsigned.
            const bool after_start = signed_of(position_hi) > signed_of(base_hi) ||
                                     (position_hi == base_hi && position_lo >= base_lo);
            const bool before_end = signed_of(position_hi) < signed_of(end_hi) ||
                                    (position_hi == end_hi && position_lo < end_lo);
            if (after_start && before_end) {
                const bool window_ends_first = signed_of(end_hi) < signed_of(want_hi) ||
                                               (end_hi == want_hi && end_lo < want_lo);
                count = (window_ends_first ? end_lo : want_lo) - position_lo;
                break;
            }
            // not in the window: seek, then read a chunk into it. The position is rounded down to a
            // 64-byte boundary, except the at-or-past-the-end case with equal high words (0).
            u32 position = position_lo & ~63u;
            if (position_hi == end_hi && position_lo >= end_lo) position = 0u;
            const GuestResult sought = call_keep(rt, ctx, 0x08B0C5C0u, {self + 8u, 0u, position, 0u});
            if (sought.v0 != position) return 0xFFFFFFFFu;
            if (sought.v1 != 0u) return 0xFFFFFFFFu;
            const u32 request = (size + 0x3FFu) & ~0x3FFu;
            put(rt, frame.at(8u), request);
            put(rt, frame.at(4u), 0x2000u);
            put(rt, frame.at(0u), 0x400u);
            const u32 chosen = request < 0x2001u ? frame.at(8u) : frame.at(4u);
            u32 limit = word(rt, chosen);
            if (limit < 0x400u) limit = 0x400u;
            put(rt, frame.at(0xCu), self + 0x134u);
            put(rt, frame.at(0x10u), limit);
            const s32 got = signed_of(call_set(rt, ctx, 0x08B0C650u, {self + 8u, frame.at(0xCu), chosen, 0u}));
            if (got < 0) return static_cast<u32>(got);
            const u32 read = static_cast<u32>(got);
            const u32 read_hi = (position + read < position ? 1u : 0u) + arithmetic_shift(read, 31);
            if (signed_of(read) < signed_of(limit)) wrapped = 1u;
            put(rt, self + 0x128u, position);
            put(rt, self + 0x12Cu, 0u);
            put(rt, self + 0x130u, read);
            const u32 now_hi = word(rt, self + 0x124u);
            if (signed_of(now_hi) < signed_of(read_hi)) continue;
            if (now_hi != read_hi) return total;
            if (word(rt, self + 0x120u) < position + read) continue;
            return total;
        }
        // the window covers [position, position + count): copy from the window's byte area
        (void)call_set(rt, ctx, 0x08B79A88u,
                       {destination, self + 0x134u + (word(rt, self + 0x120u) - word(rt, self + 0x128u)), count});
        const u32 before_lo = word(rt, self + 0x120u);
        const u32 before_hi = word(rt, self + 0x124u);
        const u32 after_lo = before_lo + count;
        const u32 after_hi = before_hi + (after_lo < before_lo ? 1u : 0u) + arithmetic_shift(count, 31);
        total += count;
        destination += count;
        put(rt, self + 0x120u, after_lo);
        put(rt, self + 0x124u, after_hi);
        size -= count;
        if (wrapped != 0u) return total;
        if (size == 0u) return total;
    }
}

} // namespace
void func_08B39148(Runtime &rt, AllegrexContext &ctx) { token_dispatch(rt, ctx); }
void func_08AC48D0(Runtime &rt, AllegrexContext &ctx) { ctx.gpr[2] = buffered_read(rt, ctx); }
void func_08B477D4(Runtime &rt, AllegrexContext &ctx) { ctx.gpr[2] = reply_exchange(rt, ctx); }
void func_08A30594(Runtime &rt, AllegrexContext &ctx) { shared_object_text(rt, ctx); }
void func_089DC808(Runtime &rt, AllegrexContext &ctx) { image_shape_floats(rt, ctx); }
void func_089BAF40(Runtime &rt, AllegrexContext &ctx) { scene_lookup(rt, ctx); }
} // namespace new_batch_main_227
} // namespace gt::game

#define MAIN227(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_227::name, gt::ReturnKind::kind)
MAIN227(0x08B39148u, func_08B39148, Word);
MAIN227(0x08AC48D0u, func_08AC48D0, Word);
MAIN227(0x08B477D4u, func_08B477D4, Word);
MAIN227(0x08A30594u, func_08A30594, Word);
MAIN227(0x089DC808u, func_089DC808, Word);
MAIN227(0x089BAF40u, func_089BAF40, Word);

// ---- from main-session batch 228 (draft 354eb505c9a2008ff3b314c77e2569720bcdc4df) ----
// New batch 228 (main session): functions the recompiler emitted that no src/ file defines or registers. Written by
// hand from `.venv/bin/python -m tools.disasm` as structured C. This is a draft batch, not yet verified by the
// differential gate: the shapes the synthetic cases need are in /tmp/claude-1000/shapes_main228.txt.
//
// Cross-check before the gate: each of the four bodies below was also transcribed into a small MIPS interpreter that
// runs the function's own instructions with stub callees, and both ran on 120-1500 random inputs each (088FE6BC 1500,
// 089966DC 1500, 08B12D50 200 per failure path + 8 CRC passes + 2 full-record passes, 088676F0 600) with 0 differences
// in the compared memory and v0. That proves the reading of the asm, not the C: the gate still has to prove the C.
// The four functions left out are named at the end of the namespace: 0x08886B00 (a 0x230-byte std::string command
// parser), 0x08AB6BA0 (the GetFileModifyTime Adhoc native: an inlined 4x4 copy, five smart-pointer temporaries and two
// virtual calls), 0x08B26EB8 (a weighted-tree builder with an in-place heap and bit-length walks) and 0x089C79F0 (a
// recursive smart-pointer container walk) — each left because a misread there is likelier than the gain.
//
// The delay slot of a `jal` / `jalr` runs before the call, a `beql` / `bnel` / `bc1fl` / `bc1tl` delay slot only when
// the branch is taken, and the delay slot of any other branch on both paths (every such slot is named below).
// `call` stores each result in v0, as the asm does; f0-f19 are not compared by the harness (only f0 of a Float
// return), so a float local stays a C++ local unless the asm leaves it in a register a caller reads.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_228 {
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

// The `ll` / `sc` reference-count decrement (one thread: a plain load and store). The old value is returned and
// v1/a0/v0 are left as the original's ll/sc sequence leaves them (the `addiu`, the `sc` flag).
u32 atomic_decrement(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x088FE6BC (a0 = object): the object's counted members are dropped and the lock at +0x10 is released in a tail call.
//
// The object at +0 is a structure with reference-counted member pairs at (0x28, 0x2C), (0x20, 0x24), (0x08, 0x0C),
// (0x34, 0x38) and (0x3C, 0x40): the first word is the member pointer, the second the counted block whose strong
// counter is at +4 and whose weak counter is at +8 (the vtable words at offsets 8 and 0xC run when a counter reaches
// zero, see the `assign_counted` family of hot_batch_90). Each pair is cleared: a non-null member is nulled, its
// counted block is nulled and the count is released; the last pair tests only the counted block. Then the bytes at
// +8, +9 and +0xC of the object are cleared, and the function ends as a tail call to 0x08B3457C(object + 0x10), the
// unlock; v0 is that call's result. The lock 0x08B344E0(object + 0x10) is taken first. Every pair reads the object's
// inner pointer from +0 again: the four virtual calls may change it.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (atomic_decrement(rt, ctx, object + 4u) != 1u) return;
    const u32 vtable = word(rt, object);
    ctx.gpr[3] = vtable;
    (void)call_indirect(rt, ctx, word(rt, vtable + 8u), {object});  // the asm's `lw v1, 0(s0)` reloads it too
    if (atomic_decrement(rt, ctx, object + 8u) != 1u) return;
    const u32 weak_vtable = word(rt, object);
    ctx.gpr[3] = weak_vtable;
    (void)call_indirect(rt, ctx, word(rt, weak_vtable + 0xCu), {object});
}

void reset_counted_pair(Runtime &rt, AllegrexContext &ctx, u32 object, u32 pointer_offset, u32 counter_offset,
                        bool test_pointer) {
    if (test_pointer && word(rt, object + pointer_offset) == 0u) return;
    put(rt, object + pointer_offset, 0u);
    // The `beqz`'s delay slot stores the zero on both paths: read the counter first, then clear it.
    const u32 counted = word(rt, object + counter_offset);
    put(rt, object + counter_offset, 0u);
    if (counted != 0u) release_counted(rt, ctx, counted);
}


// ---------------------------------------------------------------------------------------------------------------------
// 0x089966DC (self, other, outX, outY): two floats derived from `other`'s transform, written through outX/outY.
//
// `other` is a scene object: 0x08965CE0(other) is its word at +0x4C (an anchor), and 0x0896A35C / 0x08969B98 fill four
// floats from a transform (position at the quad's +0/+4, size at +8/+0xC). When the anchor is not self's +0x130 word
// the quad is taken relative to that anchor (the 0x10/0x14 quad is subtracted from the 0x20/0x24 one). Then
// 0x0896A35C(self->+0x130) fills the 0/4/8/C quad and 0x0896A35C(self, 0, 0, sp+0x38, sp+0x3C) the 0x38/0x3C pair.
//
// With the quad's lower value (0x38/0x3C) below its upper one (0x8/0xC) the output is an interpolation (bit 0 of the
// flags at +0x12C for X, bit 1 for Y):
//   bit set:   scale = the quad's +0x20 value plus the delta (+0 / +4); a negative sum is 0, a sum that reaches the
//              distance (lower - sp+0x28 / sp+0x2C) is skipped (X: outX is left untouched and the Y half runs; Y: the
//              function returns with outY untouched), otherwise 1; then (limit < scale * distance - level) keeps
//              limit (lower - upper) and the result is stored only when negative (0 otherwise);
//   bit clear: (distance * 0.5 - level) clamped up to the limit and stored only when negative.
// With the lower value not below the upper one the quad's fourth word (sp+0x30 / sp+0x34) is stored unchanged.
// v0 ends as the flags word masked with the bit of the half that ran (0x12C & 1, & 2), as the asm leaves it; the
// `other == 0` early return leaves v0 alone.
void func_089966DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { GuestFrame frame(ctx, 0x60u);
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    const u32 out_x = ctx.gpr[6];
    const u32 out_y = ctx.gpr[7];
    if (other == 0u) return;
    put_float(rt, frame.at(0x20u), 0.0f);
    put_float(rt, frame.at(0x2Cu), 0.0f);
    put_float(rt, frame.at(0x24u), 0.0f);
    put_float(rt, frame.at(0x28u), 0.0f);  // the jal's delay slot
    const u32 anchor_of_other = call(rt, ctx, 0x08965CE0u, {other});
    if (anchor_of_other == word(rt, self + 0x130u)) {
        (void)call(rt, ctx, 0x0896A35Cu,
                   {other, frame.at(0x20u), frame.at(0x24u), frame.at(0x28u), frame.at(0x2Cu)});
    } else {
        (void)call(rt, ctx, 0x08969B98u,
                   {other, frame.at(0x20u), frame.at(0x24u), frame.at(0x28u), frame.at(0x2Cu)});
        (void)call(rt, ctx, 0x08969B98u, {word(rt, self + 0x130u), frame.at(0x10u), frame.at(0x14u), 0u, 0u});
        put_float(rt, frame.at(0x20u), float_at(rt, frame.at(0x20u)) - float_at(rt, frame.at(0x10u)));
        put_float(rt, frame.at(0x24u), float_at(rt, frame.at(0x24u)) - float_at(rt, frame.at(0x14u)));
    }
    (void)call(rt, ctx, 0x0896A35Cu,
               {word(rt, self + 0x130u), frame.at(0u), frame.at(4u), frame.at(8u), frame.at(0xCu)});
    put(rt, frame.at(0x38u), 0u);
    put(rt, frame.at(0x3Cu), 0u);
    put(rt, frame.at(0x34u), 0u);
    put(rt, frame.at(0x30u), 0u);  // the jal's delay slot
    (void)call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, frame.at(0x38u), frame.at(0x3Cu)});

    const float low_x = float_at(rt, frame.at(0x38u));
    const float high_x = float_at(rt, frame.at(8u));
    const float level_x = float_at(rt, frame.at(0x20u));
    bool store_x = true;
    float x_value = 0.0f;
    if (!(low_x < high_x)) {
        x_value = float_at(rt, frame.at(0x30u));  // the bc1f's delay slot; v0 keeps the last call's result
    } else {
        const u32 flags_x = word(rt, self + 0x12Cu);
        ctx.gpr[2] = flags_x & 1u;
        if ((flags_x & 1u) != 0u) {
            const float distance = low_x - float_at(rt, frame.at(0x28u));
            float scale = level_x + float_at(rt, frame.at(0u));
            bool computed = true;
            if (scale < 0.0f) {
                scale = 0.0f;
            } else if (!(distance < scale)) {
                computed = false;  // the bc1fl leaves outX untouched
                store_x = false;
            } else {
                scale = 1.0f;
            }
            if (computed) {
                float result = scale * distance - level_x;
                const float limit = low_x - high_x;
                if (!(limit < result)) result = limit;  // the bc1fl's delay slot moves the limit in
                x_value = result < 0.0f ? result : 0.0f;
            }
        } else {
            const float distance = low_x - float_at(rt, frame.at(0x28u));
            float result = distance * 0.5f - level_x;
            const float limit = low_x - high_x;
            if (!(limit < result)) result = limit;
            x_value = result < 0.0f ? result : 0.0f;
        }
    }
    if (store_x) put_float(rt, out_x, x_value);

    const float low_y = float_at(rt, frame.at(0x3Cu));
    const float high_y = float_at(rt, frame.at(0xCu));
    if (!(low_y < high_y)) {
        put_float(rt, out_y, float_at(rt, frame.at(0x34u)));  // the bc1f's delay slot
    } else {
        const u32 flags_y = word(rt, self + 0x12Cu);
        ctx.gpr[2] = flags_y & 2u;
        const float level_y = float_at(rt, frame.at(0x24u));
        const float distance = low_y - float_at(rt, frame.at(0x2Cu));
        if ((flags_y & 2u) != 0u) {
            float scale = level_y + float_at(rt, frame.at(4u));
            if (scale < 0.0f) {
                scale = 0.0f;
            } else if (!(distance < scale)) {
                return;  // the bc1f ends the function with outY untouched
            } else {
                scale = 1.0f;
            }
            float result = scale * distance - level_y;
            const float limit = low_y - high_y;
            if (!(limit < result)) result = limit;
            put_float(rt, out_y, result < 0.0f ? result : 0.0f);
        } else {
            float result = distance * 0.5f - level_y;
            const float limit = low_y - high_y;
            if (!(limit < result)) result = limit;
            put_float(rt, out_y, result < 0.0f ? result : 0.0f);
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08B12D50 (self, index): true when `index`'s bit is set in the second bitmap, after marking it in the first one and
// hashing the block it selects.
//
// The byte at +0x258 opens the check (zero returns 0 with the lock taken and released). The bit (index & 7) of the byte
// at (word +0x268)[index >> 3] is tested: when already set the answer is the same bit of the second bitmap
// (+0x26C) and nothing else runs. Otherwise the bit is set and the sector index (index >> 7) is compared with +0x238:
// a new one locks +0x1B8, stores it, and 0x08B11F30(self, (word +0x234) + (sector << 11), word +0x23C, 0x800) refills
// the record block. Then 0x08B11F8C(self, self + 0x12C, index << 16) opens a read; a negative answer runs
// 0x08B11E3C(self) (which clears both bitmaps) and ends with the bitmap answer. Otherwise 0x20 passes of
// 0x08B33D3C(handle, word +0x270, 0x800) fill the buffer and four CRC chains (one per table kept in the globals
// 0x08CB616C, 0x08CB6170, 0x08CB6174, 0x08CB6178) accumulate over the 64 KiB; a pass that does not return 0x800
// releases the handle, clears the bitmaps and ends with the bitmap answer. Then the handle is released and the 16-byte
// record at (word +0x23C) + ((index << 4) & 0x7FF) is compared with the four CRCs: equal marks the bit of the second
// bitmap. The answer is always the second bitmap's bit. Lock +0x1E0 is held throughout. v0 = 0 or 1.
u32 crc_block(Runtime &rt, u32 pointer, u32 length, u32 crc, u32 table) {
    for (u32 n = 0u; n != length; ++n) {
        const u32 index = rt.memory().load8(pointer + n) ^ (crc >> 24);
        crc = word(rt, table + index * 4u) ^ (crc << 8);
    }
    return crc;
}


// ---------------------------------------------------------------------------------------------------------------------
// 0x088676F0 (self, unused, list, counter, voices, skip): the per-item voice weights of a sound mixer update.
//
// `list` holds {begin, end} of 8-byte items (a float at +0 and a payload word at +4). The mixer state `base` is
// 0x08893788(self + 0x600): (word(self + 0x600)) + sign-extended byte 1 of that word * 0x26D0 + 0xFD8. For every item,
// with `remaining` = the item count minus the items already processed:
//   weight = (|item[0]| - 100) * -0.01 + 1, clamped to [0, 1]; * 0.4 when remaining >= 2; 0 when remaining >= 4;
// the object's voice float at self + 4 * payload + 0x4260 is moved 8 % toward the weight, then (*voices)++ and the new
// value read: not above 0 ends the item there. When it is above 0, the counter is below 6 and remaining < 5, the item
// starts a voice: 0x088DA588(word(self + 0x5AC), payload) gives a voice object (null ends the item) whose virtual slot
// 0x50 opens it, 0x08892F74(base, counter, its answer) stores it, virtual slot 0x54 and 0x08892FE4(base, counter, its
// answer) store its second half, and 0x088DCD28(word(self + 0x5AC), payload) finds a record that
// 0x088A3BD0(record, sp, sp + 0x40) fills: the floats at sp+0x18, +0x1C, +0x14 go to base + 0x2644/0x265C/0x2674 +
// 4 * counter and the counter grows; a missing record (or the skipped payload equal to it) zeroes those three slots
// instead. The stored weight and the payload word go to base + 0x22C4 / + 0x22F4 + 4 * counter. The caller reads no
// return value (v0 is path-dependent in the original, so the registration is Void).

// Left out of this batch (not drafted):
// 0x08886B00 (a0, a1, a2): a 0x230-byte frame command parser — splits a '%' / ':' delimited text into three std::string
//   temporaries and matches them against 0x08CA06C0 / 0x08CA06C8 / 0x08CA06D8 / 0x08CA0700 before two 0x08B945C4 copies
//   that start the 0x68 object from a 0x184 temporary and finish through 0x08885994.
// 0x08AB6BA0 (the Adhoc native GetFileModifyTime, config/symbol_addrs.txt): copies a 4x4 through an inlined loop, then
//   five smart-pointer temporaries with virtual slots 0xC, 0x50, 0x54 and five releases, and ends through 0x08B60804 /
//   0x08B8542C on the 0x08D74FD0 sentinel.
// 0x08B26EB8 (a0, a1, a2): builds a weighted tree over 16-byte nodes (link/length/weight fields) with an in-place
//   heap-up/heap-down walk and the bit-length loop of a canonical-code builder.
// 0x089C79F0 (self, flags, node, list): recursive (calls itself) container walk with three smart-pointer temporaries
//   per level.

} // namespace
} // namespace new_batch_main_228
} // namespace gt::game

#define NEW228(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_228::name, gt::ReturnKind::kind)
NEW228(0x089966DCu, func_089966DC, Word);

