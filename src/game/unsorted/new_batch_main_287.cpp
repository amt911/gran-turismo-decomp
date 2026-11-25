// ---- from main-session batch 253 (draft dbb327661444bffe110bea1cbccc6b66f42e371a) ----
// New batch, main session 253: five long functions (282-296 instructions) the recompiler emits and nothing had taken, written
// as structured C from `tools.disasm` (every branch target read; a `jal`'s delay slot read as "before the call", only a
// `bnel` / `beql` delay slot as "when the branch is taken"). Drafted, not verified: no gate ran on them here.
//
// The two that had an earlier draft (0x08C21D48, 0x08BCD6A8, from `new_batch_main_42.cpp` at 720e308) were re-read against the
// asm line by line: 0x08BCD6A8 tail-calls the vtable of the block it just took over, which the old draft only did when a second
// word was non-zero (it tail-called address 0 otherwise) and 0x08C21D48 enters its "reachable fewer than held" second loop with
// the stale s2 (`self`) when `reached == 0`, which is kept as the asm leaves it.
//
// The roles below come from the accesses, not from the game: `self` is a0, `counted` a reference-counted object whose strong
// counter sits at +4 and weak one at +8 and whose vtable words at +8 and +0xC run when a counter reaches zero (the ll / sc pairs
// read as a plain load and store, as `src/game/unsorted/hot_batch_90.cpp` does: one host thread, and the `sc` success flag is 1
// in v0 either way). No name is claimed: none of these functions has evidence for one.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_253 {
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
// The `seb` pseudo-instruction: the low byte of `value`, sign-extended.
s32 sign8(u32 value) { return static_cast<std::int8_t>(value & 0xFFu); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// The callee-saved registers s0-s7 / fp as they were at entry (the original saves and restores them).
class SavedRegs {
public:
    explicit SavedRegs(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        saved_fp_ = ctx.gpr[30];
    }
    ~SavedRegs() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = saved_fp_;
    }
    SavedRegs(const SavedRegs &) = delete;
    SavedRegs &operator=(const SavedRegs &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 saved_fp_;
};

// The ll / sc increment of a reference count (single-threaded here): v1 = the address, v0 = 1 (the successful `sc`).
void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}
// The decrement: leaves a0 = the old value, v1 = the address and v0 = 1 (the `sc`); true when the old value was 1.
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}
// Releases one use of a counted object: the strong count (+4) goes down; when that was the last use the vtable word at +8 runs
// with the object, then the weak count (+8) goes down and when that was the last one the word at +0xC runs. A null one does
// nothing.
void release_counted(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (!count_down(rt, ctx, object + 4u)) return;
    (void)call_indirect(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (!count_down(rt, ctx, object + 8u)) return;
    (void)call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08C21D48 (a0 = self, a1 = argument): splices two counted-pointer sequences. 0x08A6158C(argument) answers the source holder
// and 0x08A6159C(argument, word(self+8)) the destination list, kept at frame +0x20; 0x08A76ABC(frame +0x14, holder) fills a
// first temporary whose virtual word 0x78 decides whether 0x08A98C1C(frame +0x18, 1) then 0x08A5BAD8 / 0x08A5B9EC rebuild
// frame +0x14. The element at frame +0x14 answers virtual word 0x7C = the count the destination list already holds (s7 =
// word(self+8)): equal counts run over every element with the temporary at frame +0x10 (word 0x84 is asked for the element's
// index, word 0xC4 takes the temporary), more reached than held runs the elements past the held count through frame +8 and then
// the rest through frame +8 as well (0x08A76AA0), fewer reached than held runs the held ones through frame +4 and then
// 0x08A98C1C(frame +0x18, reached - held + 1) over the range with the last element through word 0xC4. Every null element reports
// through 0x08A5BB7C, each temporary is released with 0x08A5B9EC. 0x08A61584(argument, frame +0x14) closes it; v0 is the last
// call's answer.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08BCD6A8 (a0 = self): the destructor of a composite object. The owner at self+4 is locked (0x08B34888), its 0x30 byte marks
// the "destroying" state around 0x08B34B2C when its 0x2C counter is not zero (0x08B34B24 unlocks in both cases), self+0x540 is
// told about it (0x08BBC0A0), the counted object at self+0x53C is released, the pair at self+0x528 / 0x52C is released through its
// vtable's first word, the sub-object vtables at 0x3C / 0x6C / 0x70 / 0x74 are stored (0x74 twice, the last store wins) and the
// counted objects at 0x38 / 0x2C / 0x1C / 0x14 / 0xC / 4 are released. Then the owner's own teardown: locked again; with its
// 0x2C counter zero the block at 0x38 is released through its vtable's first word, 0x08B28BE0(owner) runs and the work goes on
// in a tail call to 0x08B64654(owner); otherwise the counter goes down one and, when it reaches zero, the block at 0x34 / 0x38 is
// taken over (released through its vtable at +8) and the pair (block at 0x38, block at 0x34) goes to a tail call through the
// vtable of the former. With no such pair the function returns with the unlock's v0.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08C5F3E4 (a0 = object, a1 = fill byte, a2 = destination, a3 = source, t0 = fill end, t1 = copy length, t2 = selector):
// a range transcoder. The object's word +0xC, masked with 0xB0, picks the shape: 0x20 copies `t1` bytes first and fills the
// other (t0 - t1) after them; 0x10 with a non-zero t2 goes through the decoder state 0x08B64C80(object + 0x6C) answers (its byte
// +0x1C says whether the table is already up: 0 builds it, virtual slot 0x1C over a 256-byte identity table, sets the flag to 1
// (2 when the table differs from the identity one) and virtual slot 0x18 answers the mapped value of a selector; the bytes at
// +0x4A / +0x48 / +0x4D / +0x95 / +0x75 are the mapped values once the table is up). The source's first byte is tried against
// them in that order and, with at least two source bytes, the second one too; a matched leading byte (or two) is written to the
// destination. Everything after that is the plain shape: fill (t0 - t1) bytes with a1, then copy t1 (minus the bytes already
// taken) from the source.
void func_08C5F3E4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 filler = static_cast<std::int8_t>(ctx.gpr[5]);  // seb: the memset value
    const u32 dest_in = ctx.gpr[6];                           // s2
    const u32 source = ctx.gpr[7];                            // s3
    const u32 fill_end = ctx.gpr[8];                          // t0
    const u32 copy_bytes = ctx.gpr[9];                        // t1 / s4
    const u32 selector = ctx.gpr[10] & 0xFFu;                 // t2
    const u32 length = fill_end - copy_bytes;                 // s5
    u32 matched = ctx.gpr[23];  // s7: read before any of its writes on the "table already up" path
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x130u);
    const u32 shape = word(rt, self + 0xCu) & 0xB0u;
    if (shape == 0x20u) {
        (void)call(rt, ctx, 0x08B79A88u, {dest_in, source, copy_bytes});
        (void)call(rt, ctx, 0x08B36018u, {dest_in + copy_bytes, static_cast<u32>(filler), length});
        return;
    }
    u32 dest = dest_in;
    u32 skipped = 0u;  // s1: source bytes the copy does not repeat
    u32 copied = 0u;   // s0: source offset the copy starts at
    if (shape == 0x10u && selector != 0u) {
        const u32 state = call(rt, ctx, 0x08B64C80u, {self + 0x6Cu});
        // The state's table, built once: identity table, virtual slot 0x1C over it, the flag, then virtual slot 0x18 with
        // `code`; the answer is the mapped value sign-extended (seb).
        const auto select_code = [&](u32 code) -> s32 {
            for (u32 index = 0u; index != 0x100u; ++index) put8(rt, frame.at(index), index);
            const u32 table = state + 0x1Du;
            (void)call_indirect(rt, ctx, word(rt, word(rt, state) + 0x1Cu),
                                {state, frame.at(0u), frame.at(0x100u), table});
            put8(rt, state + 0x1Cu, 1u);
            if (call(rt, ctx, 0x08B36170u, {frame.at(0u), table, 0x100u}) != 0u) put8(rt, state + 0x1Cu, 2u);
            return sign8(call_indirect(rt, ctx, word(rt, word(rt, state) + 0x18u), {state, code}));
        };
        const s32 first = sign8(byte_u(rt, source));
        const s32 mapped = byte_u(rt, state + 0x1Cu) != 0u ? signed_byte(rt, state + 0x4Au) : select_code(0x2Du);
        if (mapped == first) {
            matched = 1u;
        } else if (byte_u(rt, state + 0x1Cu) != 0u) {
            if (signed_byte(rt, state + 0x48u) == first) matched = 1u;
        } else if (select_code(0x2Bu) == first) {
            matched = 1u;
        }
        bool wide = false;  // the destination takes the source's first two bytes
        const bool pair = byte_u(rt, state + 0x1Cu) == 0u ? select_code(0x30u) == first
                                                          : signed_byte(rt, state + 0x4Du) == first;
        if (pair && static_cast<s32>(copy_bytes) >= 2) {
            const s32 second = signed_byte(rt, source + 1u);
            if (byte_u(rt, state + 0x1Cu) != 0u) {
                wide = signed_byte(rt, state + 0x95u) == second;
            } else {
                wide = select_code(0x78u) == second;
            }
            if (!wide) {
                if (byte_u(rt, state + 0x1Cu) != 0u) {
                    wide = signed_byte(rt, state + 0x75u) == second;
                } else {
                    wide = select_code(0x58u) == second;
                }
            }
            if (wide) {
                put8(rt, dest, byte_u(rt, source));
                put8(rt, dest + 1u, byte_u(rt, source + 1u));
                dest += 2u;
                skipped = 2u;
                copied = 2u;
            }
        }
        if (!wide && matched != 0u) {
            put8(rt, dest, byte_u(rt, source));
            dest += 1u;
            skipped = 1u;
            copied = 1u;
        }
    }
    (void)call(rt, ctx, 0x08B36018u, {dest, static_cast<u32>(filler), length});
    (void)call(rt, ctx, 0x08B79A88u, {dest + length, source + copied, copy_bytes - skipped});
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x0890F1C0 (a0 = out, a1 = owner, a2 = key) -> 0 or 1: 0x08B0EC60(frame +0x60, owner) fills a 0x3C-byte record that is copied
// to frame +0x18; a non-zero first word returns 0. 0x089624EC(key) answers its argument, a pair {object +0, counted block +4} (an
// empty one zeroes frame +0x10 / +0x14). Otherwise, under the lock at object +0x6C (0x08B34888 / 0x08B34B24),
// 0x08B2CB18(object + 8, record word +0x10, 0x80) gives a handle, stored at frame +0x10; the block's use count rises four times
// and falls three (releases), a 0x18-byte control block {vtable 0x08CC2F90, counts 1 / 1, handle, object, block} is allocated
// (0x08B62344) at frame +0x14 and 0x08B8E380(frame +0x14, handle, handle) links it. `out` is assigned {frame +0x10, frame
// +0x14} like a shared pointer (a changed block counts up, the old one is released); frame +0x14 is released again. When out's
// first word is non-zero a closure {0x0890EF6C, out, pointer, record word +0x10} is registered with
// 0x08AC0C38(owner, closure pointer, closure, 1), 0x08AC0B50() runs and the answer is 1, else 0.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08BE476C (a0 = out, a1 = pair, a2 = argument, a3 = extra) -> out: pair is {object +0, counted block +4}. A non-empty pair:
// under the lock at object +0x6C (0x08B34888 / 0x08B34B24) 0x08B2CB18(object + 8, argument, extra) gives a handle h; the block's
// use count rises four times and falls three (releases), a 0x18-byte control block {vtable 0x08CC2F90, counts 1 / 1, h, object,
// block} is allocated (0x08B62344) at frame +0xC and 0x08B8E380(frame +0xC, h, h) links it; frame +8 = h. An empty pair zeroes
// frame +8 / +0xC. Then 0x08BE43B0(frame +0, frame +8) fills a frame pair, a 0xA0-byte object (0x08B62344;
// 0x08BE45C4(object, frame +0, argument, 0) initialises it) and a 0x10-byte control block {vtable 0x08CC6928, counts 1 / 1,
// object} are allocated and stored at out +0 / +4; the object is torn down when that control allocation fails
// (0x08BE427C(object + 0x98, object), 0x08B28BE0(object + 0x6C), 0x08B2C3BC(object + 8), its +4 block released,
// 0x08B64654(object)), 0x08B8E380(out + 4, object, object) links the pair and both frame blocks (+4 and +0xC) are released.
// Returns out.

} // namespace
} // namespace new_batch_main_253
} // namespace gt::game

#define NEW253(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_253::name, gt::ReturnKind::kind)
NEW253(0x08C5F3E4u, func_08C5F3E4, Word);

// ---- from main-session batch 254 (draft b1de6376269b6fdb9404042db669f21a9df9cbda) ----
// New batch main 254: four large functions (956-1204 bytes) the recompiler emits and no src/ file defines or
// registers. Written by hand from `tools/disasm` as structured C; a draft batch, not yet verified by the
// differential gate. Three of them started as the unverified drafts in hot_batch_116 / 117 / 118 ("compared 0 in
// the full gate"), re-read line by line against the asm and fixed where they differed; the input shapes in
// /tmp/claude-1000/shapes_main254.txt are the other half of the work: these functions dereference their
// arguments, so the synthetic generator needs to know what each one points at.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_254 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose result is the v0 the caller's next instruction sees (and the next call is handed).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// The same with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order):
// what the callee does not take stays as the caller had it.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call(rt, ctx, target, arguments);
}
// The same with the float arguments in f12-f15 as well.
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
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.fpr[0] = result.f0;
    return result;
}
// A virtual call `object->slot(...)`: v1 = object[0], v0 = vtable[slot], the call, a0 = the object.
u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
s32 signed_of(u32 value) { return static_cast<s32>(value); }

// The compiler's division of a byte distance by an element size: (asm `lui 0xCCCC` / `ori 0xCCCD`) a distance >> 2 is a
// multiple of 5 for 20-byte elements, so the low word of the product is the count (0xCCCCCCCD * 5 = 2^34 + 1).
constexpr u32 kDivideByFive = 0xCCCCCCCDu;
// The same for 12-byte entries (asm `lui 0xAAAA` / `ori 0xAAAB`: 0xAAAAAAAB * 3 = 2^33 + 1).
constexpr u32 kDivideByThree = 0xAAAAAAABu;
// The empty-rep string the libstdc++ copy-on-write strings share (asm `lui 0xA` / `addiu -0x7268`).
constexpr u32 kEmptyStringRep = 0x08D75298u;
// The source location 0x08A5BB7C is called with when an entry's node pointer (entry + 4) is null.
constexpr u32 kEntryAssert = 0x08CB39CCu;

// The `ll/sc` reference-count steps of shared objects (single-threaded here, the host never preempts inside one):
// the decrement leaves a0 = the old value, v1 = the word's address and v0 = 1 (the successful sc); true when the
// old value was 1.
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}
// Releases one use of a shared object: its use count (+4) goes down; when that was the last use the virtual slot
// 8 runs, then the weak count (+8) goes down and when that was the last one slot 0xC runs (a0 = the object for
// both). An empty object (a null control block) is not released.
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (!count_down(rt, ctx, object + 4u)) return;
    call_virtual_at(rt, ctx, object, 8u, {object});
    if (count_down(rt, ctx, object + 8u)) call_virtual_at(rt, ctx, object, 0xCu, {object});
}
// What follows 0x08B34888(owner) when one use is dropped: a count of 0 first unlocks, tells the weak holder (+0x34 /
// +0x38, virtual slot 0) and runs 0x08B28BE0 / 0x08B64654; otherwise the count steps down and, when it reaches 0, the
// weak state is taken out (slots 8 and 0 run after the unlock, 0x08B34C1C first when the byte at +0x30 is set).
void lock_drop_use(Runtime &rt, AllegrexContext &ctx, u32 owner) {
    if (word(rt, owner + 0x2Cu) == 0u) {
        call(rt, ctx, 0x08B34B24u, {owner});
        const u32 holder = word(rt, owner + 0x38u);
        if (holder != 0u) {
            call_virtual_at(rt, ctx, holder, 0u, {holder, word(rt, owner + 0x34u)});
            put(rt, owner + 0x38u, 0u);
            put(rt, owner + 0x34u, 0u);
        }
        call(rt, ctx, 0x08B28BE0u, {owner});
        call(rt, ctx, 0x08B64654u, {owner});
        return;
    }
    put(rt, owner + 0x2Cu, word(rt, owner + 0x2Cu) - 1u);
    u32 dropped_second = 0u;
    u32 dropped_first = 0u;
    if (word(rt, owner + 0x2Cu) == 0u) {
        const u32 weak = word(rt, owner + 0x34u);
        if (weak != 0u) {
            dropped_second = word(rt, owner + 0x38u);
            dropped_first = weak;
            put(rt, owner + 0x34u, 0u);
            put(rt, owner + 0x38u, 0u);
        }
        if (byte_u(rt, owner + 0x30u) != 0u) call(rt, ctx, 0x08B34C1Cu, {owner});
    }
    call(rt, ctx, 0x08B34B24u, {owner});
    if (dropped_first != 0u) call_virtual_at(rt, ctx, dropped_first, 8u, {dropped_first});
    if (dropped_second != 0u) call_virtual_at(rt, ctx, dropped_second, 0u, {dropped_second, dropped_first});
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08BBFAAC(vector, position, count, value): the libstdc++ vector<20-byte element>::insert(position, count, value)
// for the element {string pointer +0, inner vector of 16-byte records at +4 (begin, end, capacity), word +0x10}.
// Nothing when count == 0 (v0 is left as it came in).
// With room: the value is copied to the frame element at sp + 0x10 (string by 0x08B9432C, inner vector duplicated
// through 0x08B35DA0 / 0x08BBEC68, word at +0x20), the elements after the position move up by count
// (0x08BBF7E8 for the tail, then string 0x08B957D8 + inner-vector 0x08BBECF0 assignments plus the word, backwards)
// or, when fewer follow, 0x08BBF8CC / 0x08BBF9B8; 0x08BBEED0 fills the gap; the frame element's inner vector is
// destroyed (0x08BBEBE4) and freed (0x08B35254) and its string released (0x08B60804, then 0x08B93554 unless it is
// the empty rep 0x08D75298). v0 = 0x08D75298, or the release's result when the string is not the empty rep.
// Full: size + max(size, count) elements (beyond 0x0CCCCCCC = 0x08B5ADDC(0x08CA0DC4); 0x08B35DA0 allocates),
// 0x08BBF640 / 0x08BBF554 / 0x08BBF640 build the new range, 0x08BBF734 destroys the old one, 0x08B35254 frees it
// (a0 = block) and begin / end / capacity are updated; v0 = the new capacity.
// The /20 and /5 steps are the compiler's multiply: (bytes >> 2) is a multiple of 5, so the low word of
// (n * 0xCCCCCCCD) is n / 5.
void func_08BBFAAC(Runtime &rt, AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x70u);
    const u32 element = frame.at(0x10u);
    put(rt, frame.at(0x30u), count);
    if (count == 0u) return;
    const u32 current_end = word(rt, vector + 4u);
    const u32 room = static_cast<u32>(signed_of(word(rt, vector + 8u) - current_end) >> 2) * kDivideByFive;
    if (room < count) {
        // No room: reallocate into size + max(size, count) elements and rebuild the three ranges.
        const u32 begin = word(rt, vector);
        const u32 size = static_cast<u32>(signed_of(current_end - begin) >> 2) * kDivideByFive;
        put(rt, frame.at(0xCu), size);
        if (0x0CCCCCCCu - size < count) call(rt, ctx, 0x08B5ADDCu, {0x08CA0DC4u});
        const u32 grow = size < count ? count : size;
        const u32 total = size + grow;
        u32 bytes = 0xFFFFFFF0u;
        u32 block = 0u;
        if (!(total < size)) {
            bytes = total * 20u;
            if (total != 0u) block = call(rt, ctx, 0x08B35DA0u, {bytes});
        } else {
            block = call(rt, ctx, 0x08B35DA0u, {bytes});
        }
        const u32 front = call(rt, ctx, 0x08BBF640u, {begin, position, block, frame.at(6u)});
        call(rt, ctx, 0x08BBF554u, {front, count, value, frame.at(5u)});
        const u32 back = call(rt, ctx, 0x08BBF640u, {position, current_end, front + count * 20u, frame.at(4u)});
        call(rt, ctx, 0x08BBF734u, {word(rt, vector), word(rt, vector + 4u), frame.at(3u)});
        const u32 old_block = word(rt, vector);
        if (old_block != 0u) call(rt, ctx, 0x08B35254u, {old_block});
        ctx.gpr[2] = block + bytes;
        put(rt, vector + 8u, block + bytes);
        put(rt, vector + 4u, back);
        put(rt, vector, block);
        return;
    }
    // Room: build the element to insert in the frame, then move / fill.
    call(rt, ctx, 0x08B9432Cu, {element, value});
    const u32 inner = value + 4u;
    const u32 source_begin = word(rt, inner);
    const u32 source_end = word(rt, inner + 4u);
    u32 record_bytes = 0u;
    u32 records_block = 0u;
    put(rt, frame.at(0x14u), 0u);
    const u32 records = static_cast<u32>(signed_of(source_end - source_begin) >> 4);
    put(rt, frame.at(0x18u), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    if (records != 0u) {
        record_bytes = records << 4;
        records_block = call(rt, ctx, 0x08B35DA0u, {record_bytes});
    }
    put(rt, frame.at(0x14u), records_block);
    put(rt, frame.at(0x18u), records_block);
    put(rt, frame.at(0x1Cu), records_block + record_bytes);
    const u32 copied = call(rt, ctx, 0x08BBEC68u, {word(rt, inner), word(rt, inner + 4u), records_block, frame.at(2u)});
    const u32 tail_end = word(rt, vector + 4u);
    const u32 quarters = static_cast<u32>(signed_of(tail_end - position) >> 2);  // 5 * the elements before position
    const u32 before = quarters * kDivideByFive;                                   // the elements before position
    put(rt, frame.at(0x20u), word(rt, value + 0x10u));
    put(rt, frame.at(0x18u), copied);
    if (count < before) {
        const u32 shift = count * 20u;
        const u32 moved_end = tail_end - shift;
        call(rt, ctx, 0x08BBF7E8u, {moved_end, tail_end, tail_end, frame.at(9u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + shift);
        const u32 middle = static_cast<u32>(signed_of(moved_end - position) >> 2) * kDivideByFive;
        if (signed_of(middle) > 0) {
            u32 destination = tail_end;
            u32 source = moved_end;
            for (u32 index = 0u; index != middle; ++index) {
                destination -= 0x14u;
                source -= 0x14u;
                call(rt, ctx, 0x08B957D8u, {destination, source});
                call(rt, ctx, 0x08BBECF0u, {destination + 4u, source + 4u});
                put(rt, destination + 0x10u, word(rt, source + 0x10u));
            }
        }
        call(rt, ctx, 0x08BBEED0u, {position, position + shift, element});
    } else {
        call(rt, ctx, 0x08BBF8CCu, {tail_end, count - before, element, frame.at(8u)});
        const u32 new_end = word(rt, vector + 4u) + (count - before) * 20u;
        put(rt, vector + 4u, new_end);
        call(rt, ctx, 0x08BBF9B8u, {position, tail_end, new_end, frame.at(7u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (quarters << 2));
        call(rt, ctx, 0x08BBEED0u, {position, tail_end, element});
    }
    call(rt, ctx, 0x08BBEBE4u, {word(rt, frame.at(0x14u)), word(rt, frame.at(0x18u)), frame.at(1u)});
    if (word(rt, frame.at(0x14u)) != 0u) call(rt, ctx, 0x08B35254u, {word(rt, frame.at(0x14u))});
    ctx.gpr[2] = kEmptyStringRep;
    const u32 text = word(rt, element);
    if (text - 0xCu != kEmptyStringRep) {
        if (signed_of(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0) call(rt, ctx, 0x08B93554u, {text - 0xCu, frame.at(0u)});
    }
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08874C04(self, a1, text): the record at self + 0x14 is prepared from `text` ("name:sub%extra"). With bit 0 of
// the halfword at holder + 0x26 set, a 0x68-byte object is built (vtables 0x08CBFB30 then 0x08CBFAF0, the two
// empty-string reps 0x08D752A4 at +0xC / +0x10, +0x24 / +0x25 cleared and bit 16 of the word at +0x24 cleared) and
// put into the holder's shared pointer at holder + 0 / +4 through a 0x10-byte control block (vtable 0x08CBFBB0,
// counts 1 / 1) and 0x08B8E380; the replaced control block's uses are released (counts at +4 and +8, slots 8 and
// 0xC). Then the text is split at the last ':' (0x3A) and the last '%' (0x25): the part before the ':' goes to
// sp + 8, the part after the '%' to sp + 0x28, and the middle part to sp + 0x48. When sp + 8 equals the literal at
// 0x08C9F99C and 0x0886AF64(a1) is non-zero, sp + 0x108 = formatted(sp + 0x108, 0x08C9F9A4, middle) and
// sp + 0x188 = formatted(sp + 0x188, 0x08C9F9B8, middle); the object then stores a1's results from 0x0886B0A4 (at
// the holder's +8) and 0x0886B0C8(a1, 0) (at +4), sets bit 16 of the holder's +0x24, and copies the alternate
// (byte +0x24 = 2, strings at +0xC / +0x10 through 0x08B35F60 / 0x08B945C4) or just the middle part (byte = 1, at
// +0xC). Ends 0x08B99268(self + 0x14) and 0x0886BC14(a1); v0 = 1.

// ---------------------------------------------------------------------------------------------------------------------
// 0x0894106C(self, a1, first_word, second_word, mode, f12 = x, f13 = y, f14 = z, f15 = angle) -> 0: the object's
// frame setup. With state = self[2] and spec = self[3], 0x089545BC fills the 5-word vector at sp + 8; for mode 2 / 1
// the offset spec->+0 / -spec->+0x70 is rotated by 0x08B498AC(angle) into x and y. The vector {x, z, -y} with the two
// words goes to the virtual slot 8 of the object at self[5] (when set); state +0x10 / +0x14 / +0x18 / +0x690 / +0x694
// and the cells +0x6FC / +0x700 / +0x704 are set and 0x0893C3E4 / 0x08954870 run. For the four records
// 0x08955300(self, i): a position from the transform 0x0893C780, the record's bytes +0xE9..+0xED and floats +0xBC /
// +0xD0 from the transform's fields (0x08954618 / 0x08954670 / 0x08954688); the counts go to state +0x69D / +0x69E and
// bit 4 of state +0xC is cleared. A second pass sets each record's floats +0x54 / +0x58 / +0xCC / +0x10 / +0x14 /
// +0x18 / +0x1C / +0xC from spec through 0x0894E9B4 / 0x0894E714 / 0x0893D0F4 / 0x0893D064. Then 0x08945F6C(self, 1),
// the angle 0x08B4A430(spec->+0x60 - +0x64, spec->+0x18) added to state +0x6FC, 0x0893C3E4 again, state +0x18 from
// spec->+0x5C / state->+0x3C, 0x08954870 and the copies into state +0x764.. / +0x770; unless the byte state +3 is 4,
// 0x0895A004(self[0], byte state +0).

// ---------------------------------------------------------------------------------------------------------------------
// 0x08BC9A34(result, owner_arg, a, b, t0): the shared-object construction of 0x08BD25E4 for a 0xA4-byte object
// (vtable 0x08CC5200, +0x30 = 0x08BC7ED8, +0x38 = owner_arg, members at +0x20 / +0x28 / +0x2C / +0x34 / +0x80..
// cleared) whose two 0x20-element members (+0x3C, +0x5C) are built by 0x08B3767C(member, 0x20, 0x08CA0F38 /
// 0x08CA0F40, 0x089312DC(a, b)); the owner O = *(t0) is counted under its own lock 0x08B34888 / 0x08B34B24 (three
// times, one of them after result[0] = object; the 0x14-byte control block has vtable 0x08CC5240, counts 1 / 1,
// +0xC = object, +0x10 = O, and gets the fourth count). result[1] = control, then the incoming count drops through
// the lock (O + 0x2C, weak holder +0x34 / +0x38, byte +0x30, 0x08B34C1C, 0x08B28BE0, 0x08B64654) three times: before
// the "no control block -> virtual slot 4 on the object" check, and twice around 0x08BBD79C(result + 4, object + 0x20,
// object). v0 = result.

// ---------------------------------------------------------------------------------------------------------------------
// 0x08C366E8(first, last, depth): the quicksort loop over an array of 12-byte entries {string +0, node +4, byte +8}.
// The order is the node's key at +0x14 (`slt`), then the strings at +0 (0x08B35FD8); an entry whose node is null goes
// through 0x08A5BB7C(0x08CB39CC) first. Sixteen entries or fewer return (v0 = 1, the leftover of the size test); with
// depth 0 the range is handed to 0x08C36494(first, last, last). Otherwise each round takes depth - 1, picks the median
// of first / middle / last-1 (copied into the frame element at sp: word +0, 0x08AAE214(sp + 4, pivot + 4), byte +8),
// partitions [first, last) around that copy with 0x08C35D44 (v0 = the cut), releases the copy (0x08A5B9EC(sp + 4)),
// sorts [cut, last) recursively with the reduced depth, moves last to cut and repeats while 17 entries or more
// remain; the round whose counter reaches the incoming depth hands the rest to 0x08C36494 instead (v0 = that call's
// result, as the asm leaves it, and v0 = the round counter when the range ran short first).
bool entry_less(Runtime &rt, AllegrexContext &ctx, u32 left, u32 right) {
    u32 left_node = word(rt, left + 4u);
    if (left_node == 0u) {
        call(rt, ctx, 0x08A5BB7Cu, {kEntryAssert});
        left_node = word(rt, left + 4u);
    }
    u32 right_node = word(rt, right + 4u);
    if (right_node == 0u) {
        call(rt, ctx, 0x08A5BB7Cu, {kEntryAssert});
        right_node = word(rt, right + 4u);
    }
    const s32 left_key = signed_of(word(rt, left_node + 0x14u));
    const s32 right_key = signed_of(word(rt, right_node + 0x14u));
    if (left_key < right_key) return true;
    if (right_key < left_key) return false;
    return signed_of(call(rt, ctx, 0x08B35FD8u, {word(rt, left), word(rt, right)})) < 0;
}
void func_08C366E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    u32 last = ctx.gpr[5];
    const u32 depth_limit = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    const u32 temporary = frame.at(0u);
    s32 count = static_cast<s32>(static_cast<u32>(signed_of(last - first) >> 2) * kDivideByThree);
    put(rt, frame.at(0x20u), depth_limit);
    put(rt, frame.at(0x14u), first);
    if (count < 17) {
        ctx.gpr[2] = 1u;
        return;
    }
    if (depth_limit == 0u) {
        call(rt, ctx, 0x08C36494u, {first, last, last});
        return;
    }
    u32 depth = depth_limit;
    put(rt, frame.at(0x24u), first + 4u);
    u32 rounds = 0u;
    for (;;) {
        --depth;
        const u32 mid = first + static_cast<u32>(count / 2) * 12u;
        const u32 last_entry = last - 12u;
        u32 pivot;
        if (entry_less(rt, ctx, first, mid)) {
            if (entry_less(rt, ctx, mid, last_entry)) pivot = mid;
            else if (entry_less(rt, ctx, first, last_entry)) pivot = last_entry;
            else pivot = first;
        } else if (entry_less(rt, ctx, first, last_entry)) {
            pivot = first;
        } else if (entry_less(rt, ctx, mid, last_entry)) {
            pivot = last_entry;
        } else {
            pivot = mid;
        }
        const u32 copied_word = word(rt, pivot);
        put(rt, temporary, copied_word);
        call(rt, ctx, 0x08AAE214u, {temporary + 4u, pivot + 4u});
        put8(rt, temporary + 8u, byte_u(rt, pivot + 8u));
        const u32 cut = call(rt, ctx, 0x08C35D44u, {first, last, temporary});
        call(rt, ctx, 0x08A5B9ECu, {temporary + 4u});
        call(rt, ctx, 0x08C366E8u, {cut, last, depth});
        last = cut;
        count = static_cast<s32>(static_cast<u32>(signed_of(last - first) >> 2) * kDivideByThree);
        if (count < 17) {
            ctx.gpr[2] = rounds;
            return;
        }
        ++rounds;
        if (rounds == depth_limit) {
            call(rt, ctx, 0x08C36494u, {first, last, last});
            return;
        }
    }
}

} // namespace
} // namespace new_batch_main_254
} // namespace gt::game

#define NEW254(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_254::name, gt::ReturnKind::kind)
NEW254(0x08BBFAACu, func_08BBFAAC, Word);
NEW254(0x08C366E8u, func_08C366E8, Word);

// ---- from main-session batch 255 (draft 21db5ffcd38e5f17fbe360cba0335951ba0d4224) ----
// New batch 255 (main session): functions emitted by the recompiler that no src/ file defines or
// registers. Written by hand from `tools/disasm` as structured C; this is a draft batch, not yet
// verified by the differential gate. The helpers below cover the shapes this batch repeats: a plain
// guest call, a call through a vtable slot, and the FPU's r3000-style arithmetic (every invalid
// operation or NaN operand gives the PSP's default NaN 0x7FC00000, not the host's).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_255 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

constexpr u32 kDefaultNaN = 0x7FC00000u;
// FILE::_flags bit 5, the end-of-file mark the seek paths clear with `ins v0, 0, 5, 1`.
constexpr u32 kEndOfFile = 0x20u;

// mul.s: hot::psp_mul carries the FPU's inf * 0 case (the PSP's NaN, not the host's); a quiet NaN
// operand gives the default NaN here as well.
float fmul(float a, float b) {
    if (std::isnan(a) || std::isnan(b)) return std::bit_cast<float>(kDefaultNaN);
    return psp_mul(a, b);
}
float fadd(float a, float b) {
    if (std::isnan(a) || std::isnan(b)) return std::bit_cast<float>(kDefaultNaN);
    if (std::isinf(a) && std::isinf(b) && ((a > 0.0f) != (b > 0.0f))) return std::bit_cast<float>(kDefaultNaN);
    return a + b;
}
float fsub(float a, float b) {
    if (std::isnan(a) || std::isnan(b)) return std::bit_cast<float>(kDefaultNaN);
    if (std::isinf(a) && std::isinf(b) && ((a > 0.0f) == (b > 0.0f))) return std::bit_cast<float>(kDefaultNaN);
    return a - b;
}

// A guest call; afterwards v0 holds its result (the batch files' convention).
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = call_guest(rt, ctx, target, arguments);
    return ctx.gpr[2];
}
// A guest call that leaves v0 / v1 / f0 as the callee left them: a callee that never writes v0 hands
// the previous one on, and the caller's f0 is the callee's.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// The same with float arguments (f12-f15).
GuestResult call_keepf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                       std::initializer_list<float> float_arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A call through a vtable slot: `lw v0, slot(vtable); jalr v0` leaves the slot's address in v0 at the
// call, so a callee that never writes v0 answers it.
GuestResult call_slot(Runtime &rt, AllegrexContext &ctx, u32 object, u32 offset,
                      std::initializer_list<u32> arguments = {}) {
    const u32 target = word(rt, word(rt, object) + offset);
    ctx.gpr[2] = target;
    return call_keep(rt, ctx, target, arguments);
}
// 0x089641C4(dst, src): one 16-byte quad (VFPU lv.q / sv.q), skipped when both point at the same place.
void copy_quad(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 source) {
    (void)call(rt, ctx, 0x089641C4u, {destination, source});
}
// The original's two delay loops (`addiu v0, v0, -1` with the same instruction in the delay slot); they
// only burn time and leave v0 = -1, which the call after them overwrites.
void spin(u32 start) {
    u32 value = start;
    do { value -= 2u; } while (value != 0xFFFFFFFFu);
}

// 0x08A076B4 (a0 = this, a1 = the drawn object, a2 / a3 = two 16-byte blocks, f12 = a length): lays the
// tube around a path out as quads. this->0x11C gives the radius, raised to half of the smaller position
// axis; the length is added to it for the far ring. The sin/cos table at 0x08CF5F7C (initialized on first
// use under the guard at 0x08CF5F68, as the pointer 0x08CF5F78) rotates eight samples per arc around the
// four per-arc centres, whose floats are the 0x08CDEB50 table paired with this->0x11C and the position;
// four arcs of eight quads and a closing quad each land at sp+0x78, sixty-eight quads of the matrix array
// at sp+0x2D0 (the 74-quad block is pre-filled with 1.0) and, in the end, the object's virtual +0x10C
// draws them (a1 from 0x08C8CC14, 0x4A, the vertices, 0, the matrices).
void func_08A076B4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x7E0u);
    const u32 self = ctx.gpr[4];
    const u32 x = ctx.gpr[5];
    const u32 block_a = ctx.gpr[6];
    const u32 block_b = ctx.gpr[7];
    const float length = ctx.fpr[12];

    // 0x0896A35C(self, 0, 0, sp, sp + 4): two floats of the path position. Nothing writes those slots
    // before they are read again at 0x08A07758, so one read serves both uses.
    (void)call(rt, ctx, 0x0896A35Cu, {self, 0u, 0u, frame.at(0u), frame.at(4u)});
    const float pos_x = float_at(rt, frame.at(0u));
    const float pos_y = float_at(rt, frame.at(4u));

    // The drawn object's virtual +0x108 takes the first block, then 0x089A9654(x, 0, 0) the position and
    // this->0x11C; its v0 is what the function answers when there is nothing to draw.
    call_slot(rt, ctx, x, 0x108u, {x, block_a});
    (void)call_keepf(rt, ctx, 0x089A9654u, {x, 0u, 0u}, {pos_x, pos_y, float_at(rt, self + 0x11Cu)});

    // f24 = max(this->0x11C, min(pos_x, pos_y) * 0.5), then f25 = f24 + the length; not above zero means
    // nothing to draw, with f0 = f25 - f24 (the subtract sits in the branch's delay slot).
    float limit = float_at(rt, self + 0x11Cu);
    float smallest = fmul(pos_x, 0.5f);
    const float half_y = fmul(pos_y, 0.5f);
    if (!(smallest < half_y)) smallest = half_y;
    if (limit < smallest) limit = smallest;
    const float span = fadd(limit, length);
    if (!(0.0f < span)) {
        ctx.fpr[0] = fsub(span, limit);
        return;
    }
    const float grow = fsub(span, limit);
    const float inner_x = fsub(pos_x, limit);
    const float inner_y = fsub(pos_y, limit);
    const float outer_x = fadd(pos_x, grow);
    const float outer_y = fadd(pos_y, grow);
    const float inward = -grow;

    // The two static guards: 0x08CF5F70 has no initializer, 0x08CF5F68 fills the eight sin/cos pairs at
    // 0x08CF5F7C and stores the table pointer 0x08CF5F78. The steps apart, the delay loops.
    if (rt.memory().load8(0x08CF5F70u) == 0u && call(rt, ctx, 0x08B6441Cu, {0x08CF5F70u}) != 0u) {
        spin(7u);
        (void)call(rt, ctx, 0x08B64494u, {0x08CF5F70u});
    }
    if (rt.memory().load8(0x08CF5F68u) == 0u && call(rt, ctx, 0x08B6441Cu, {0x08CF5F68u}) != 0u) {
        float angle = 0.0f;
        for (u32 slot = 0x08CF5F7Cu; slot != 0x08CF5FBCu; slot += 8u) {
            const float sampled = angle;  // f12 for both calls: the step is added after it is loaded
            const float first = call_keepf(rt, ctx, 0x08B66524u, {}, {sampled}).f0;
            angle = fadd(sampled, std::bit_cast<float>(0x3E490FD8u));  // pi / 16, in the delay slot
            const float second = call_keepf(rt, ctx, 0x08B662C4u, {}, {sampled}).f0;
            put_float(rt, slot, second);
            put_float(rt, slot + 4u, first);
        }
        put(rt, 0x08CF5F78u, 0x08CF5F7Cu);
        (void)call(rt, ctx, 0x08B64494u, {0x08CF5F68u});
    }
    spin(0x49u);

    // 74 quads of 1.0 frame the arc quads at sp+0x2D0 (one store of the unrolled fill sits in the
    // loop's delay slot, so the last quad is filled by the iteration before it).
    for (u32 at = 0x2D0u; at != 0x770u; at += 0x10u) {
        put_float(rt, frame.at(at), 1.0f);
        put_float(rt, frame.at(at + 4u), 1.0f);
        put_float(rt, frame.at(at + 8u), 1.0f);
        put_float(rt, frame.at(at + 0xCu), 1.0f);
    }

    // The four columns the arc seams and the mirror take their pairs from. The original keeps them in the
    // frame at sp+0x28..0x74 (with an unread prefix at sp+0x08..0x24) and reads them back per arc; they are
    // workspace only, so they live here.
    const float column[4][4] = {
        {limit, 0.0f, limit, inward},
        {pos_x, limit, outer_x, limit},
        {inner_x, pos_y, inner_x, outer_y},
        {0.0f, inner_y, inward, inner_y},
    };

    const u32 out_base = frame.at(0x78u);   // the vertices, nine quads per arc
    const u32 mat_base = frame.at(0x2D0u);  // the quad pairs, 18 per arc
    u32 table = 0x08CDEB50u;                // the per-arc sin/cos pair
    u32 centre = frame.at(0x08u);           // the per-arc centre, two floats
    u32 step = 0u;                          // [sp+0x770]: 0x12 per arc
    u32 seam_at = frame.at(0x100u);         // [sp+0x77C]: the seam quad's second pair
    u32 pair_at = frame.at(0x2D0u + 0x110u);  // [sp+0x780]: the second block's quad per arc
    for (;;) {
        const u32 arc = step / 0x12u;
        u32 quad_out = out_base + step * 8u;
        u32 quad_a = mat_base + step * 16u;
        u32 quad_b = quad_a + 0x10u;
        for (u32 sample = 0u; sample != 0x40u; sample += 8u) {
            copy_quad(rt, ctx, quad_a, block_a);
            const float pitch = float_at(rt, table);
            const float yaw = float_at(rt, table + 4u);
            const u32 circle = word(rt, 0x08CF5F78u) + sample;
            const float sin = float_at(rt, circle);
            const float cos = float_at(rt, circle + 4u);
            const float base_x = float_at(rt, centre);
            const float base_y = float_at(rt, centre + 4u);
            // (sin, cos) rotated by the arc's angle: the ring offsets on both axes.
            const float around = fsub(fmul(sin, pitch), fmul(cos, yaw));
            const float across = fadd(fmul(sin, yaw), fmul(pitch, cos));
            const float near_x = fadd(fmul(limit, around), base_x);
            const float far_x = fadd(base_x, fmul(span, around));
            const float near_y = fadd(fmul(limit, across), base_y);
            const float far_y = fadd(base_y, fmul(span, across));
            copy_quad(rt, ctx, quad_b, block_b);
            put_float(rt, quad_out, near_x);
            put_float(rt, quad_out + 4u, near_y);
            put_float(rt, quad_out + 8u, far_x);
            put_float(rt, quad_out + 0xCu, far_y);
            quad_out += 0x10u;
            quad_a += 0x20u;
            quad_b += 0x20u;
        }
        // The arc's closing quad: the first block's quad after the last pair, a seam quad from the
        // column, the second block's quad, and the column's other pair at the mirror.
        copy_quad(rt, ctx, mat_base + (step + 0x10u) * 16u, block_a);
        const u32 seam = out_base + (step + 0x10u) * 8u;
        put_float(rt, seam, column[arc][0]);
        put_float(rt, seam + 4u, column[arc][1]);
        copy_quad(rt, ctx, pair_at, block_b);
        put_float(rt, seam_at, column[arc][2]);
        put_float(rt, seam_at + 4u, column[arc][3]);
        step += 0x12u;
        centre += 8u;
        table += 8u;
        seam_at += 0x90u;
        pair_at += 0x120u;
        if (step == 0x48u) break;
    }

    // The tail: both blocks once more at sp+0x750 / sp+0x760, the closing seam quad (0, limit, inward,
    // limit) at sp+0x2B8, and the draw.
    copy_quad(rt, ctx, frame.at(0x750u), block_a);
    put_float(rt, frame.at(0x2B8u), 0.0f);
    put_float(rt, frame.at(0x2BCu), limit);
    copy_quad(rt, ctx, frame.at(0x760u), block_b);
    put_float(rt, frame.at(0x2C0u), inward);
    put_float(rt, frame.at(0x2C4u), limit);
    const u32 draw = word(rt, word(rt, x) + 0x10Cu);
    ctx.gpr[2] = 0x00490000u;  // the `lui v0, 0x49` whose load of a1 sits in the `jalr`'s delay slot
    (void)call_keep(rt, ctx, draw, {x, word(rt, 0x08C8CC14u), 0x4Au, out_base, 0u, mat_base});
}

// 0x08B789A4 (a0 = the reent whose first word is errno, a1 = FILE, a2 = offset, a3 = whence): fseeko of the
// C library statically linked into GT. The stream's layout is newlib's: the initializer of func_08B7865C
// (0x08B7865C, hot_batch_272) stores the four function pointers +0x20 read, +0x24 write, +0x28 seek,
// +0x2C close at the object it then passes here, and +0x28 is called with (_cookie, position, whence), which
// the ESPIPE / regular-file paths prove: +0 _p, +4 _r, +8 _w, +0xC _flags (u16), +0xE _file (s16),
// +0x10 _bf._base, +0x1C _cookie, +0x28 _seek, +0x30 _ub._base,
// +0x38 _ubuf, +0x3C _nbuf, +0x4C the block size an optimized seek aligns to, +0x50 the cached absolute
// position. 0x08B7B804, the default seek, is the one the regular-file probe recognizes. Flags used:
// 8 writing, 0x100 append, 2 unbuffered, 4 reading, 0x10, 0x20 end-of-file, 0x400
// seek optimized, 0x800 no seek optimization, 0x1000 the cached position is valid, 0x2000 never adjust.
//
// Paths: SEEK_CUR first turns into an absolute position (the buffer's window gives the difference). An
// absolute target either lands inside the read buffer (move _p, lower _r — the fast path) or is sought to
// the block boundary, the rest skipped by filling the buffer. Streams without optimization, or with
// writes/unbuffered state pending, flush and seek straight away. errno: 0x16 EINVAL for a bad whence,
// 0x1D ESPIPE without a seek.
void func_08B789A4(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x70u);
    const u32 reent = ctx.gpr[4];
    const u32 file = ctx.gpr[5];
    u32 offset = ctx.gpr[6];
    u32 whence = ctx.gpr[7];
    bool havepos = false;
    u32 curoff = 0u;
    // The buffer base the "no buffer yet" test at 0x08A34 reads, loaded where the original loads it (after
    // the calls that can still make one).
    u32 buffer = 0u;

    // CHECK_INIT: the reent's stdio table at +0x38 is built on first use (0x08B796C4 is the reent lookup).
    if (word(rt, call(rt, ctx, 0x08B796C4u, {reent}) + 0x38u) == 0u) {
        (void)call(rt, ctx, 0x08B78430u, {call(rt, ctx, 0x08B796C4u, {reent})});
    }
    u32 flags = rt.memory().load16(file + 0xCu);
    if ((flags & 0x100u) != 0u && (flags & 8u) != 0u) (void)call(rt, ctx, 0x08B7807Cu, {file});  // append flush
    const u32 seek = word(rt, file + 0x28u);
    if (seek == 0u) {  // 0x08E48
        put(rt, reent, 0x1Du);
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }

    // The ungetc buffer's release both ends share: the stream's own buffer is only forgotten, another one
    // goes back through 0x08B354A4.
    const auto release_ungetc = [&]() {
        const u32 base = word(rt, file + 0x30u);
        if (base == 0u) return;
        if (base == file + 0x40u) {
            put(rt, file + 0x30u, 0u);
            return;
        }
        (void)call(rt, ctx, 0x08B354A4u, {call(rt, ctx, 0x08B796C4u, {}), base});
        put(rt, file + 0x30u, 0u);
    };
    // 0x08DC0: flush, seek to the offset, drop the ungetc buffer and empty the read buffer.
    const auto flush_and_seek = [&]() -> u32 {
        if (call(rt, ctx, 0x08B7807Cu, {file}) != 0u) return 0xFFFFFFFFu;
        if (call(rt, ctx, seek, {word(rt, file + 0x1Cu), offset, whence}) == 0xFFFFFFFFu) return 0xFFFFFFFFu;
        release_ungetc();
        rt.memory().store16(file + 0xCu, static_cast<std::uint16_t>(rt.memory().load16(file + 0xCu) & ~kEndOfFile));
        put(rt, file, word(rt, file + 0x10u));
        put(rt, file + 4u, 0u);
        return 0u;
    };
    // 0x08BA8: seek to the block the target falls in, then skip the rest by filling the buffer.
    const auto seek_aligned = [&](u32 target) -> u32 {
        const u32 aligned = target & (0u - word(rt, file + 0x4Cu));
        if (call(rt, ctx, seek, {word(rt, file + 0x1Cu), aligned, 0u}) == 0xFFFFFFFFu) return flush_and_seek();
        put(rt, file + 4u, 0u);
        put(rt, file, word(rt, file + 0x10u));
        release_ungetc();
        rt.memory().store16(file + 0xCu, static_cast<std::uint16_t>(rt.memory().load16(file + 0xCu) & ~kEndOfFile));
        const u32 remaining = target - aligned;
        if (remaining == 0u) return 0u;
        if (call(rt, ctx, 0x08B7B0CCu, {file}) != 0u) return flush_and_seek();
        const u32 read = word(rt, file + 4u);
        if (read < remaining) return flush_and_seek();
        put(rt, file + 4u, read - remaining);
        put(rt, file, word(rt, file) + remaining);
        return 0u;
    };
    // 0x08B34: where the target sits inside the buffer's window (v1) and how wide the window is (a2), then
    // the fast path — move _p and the unread count — or a real seek.
    const auto align = [&](u32 target) -> u32 {
        u32 unread = 0u;   // v1: the unread count
        u32 start = 0u;    // v1 (later): the window's first byte
        if (havepos) {     // the SEEK_CUR block already knows the position
            unread = word(rt, file + 4u);
            start = curoff;
        } else if ((flags & 0x1000u) == 0u) {  // no cached position: ask the seek function
            const u32 here = call(rt, ctx, seek, {word(rt, file + 0x1Cu), 0u, 1u});
            if (here == 0xFFFFFFFFu) return flush_and_seek();
            flags = rt.memory().load16(file + 0xCu);
            unread = word(rt, file + 4u);
            start = here - unread;
        } else {
            unread = word(rt, file + 4u);
            start = word(rt, file + 0x50u) - unread;
        }
        u32 ahead = unread;    // v1
        u32 extent = 0u;       // a2
        if (word(rt, file + 0x30u) == 0u) {  // no ungetc buffer: the read buffer alone
            const u32 beyond = word(rt, file) - word(rt, file + 0x10u);
            extent = beyond + unread;
            ahead = start - beyond;
        } else {
            const u32 span = word(rt, file + 0x38u) - word(rt, file + 0x10u);
            if (!havepos) start -= word(rt, file + 0x3Cu);
            extent = span + word(rt, file + 0x3Cu);
            ahead = unread + start - span;
        }
        // 0x08B88 compares signed, 0x08B98 unsigned, as the original does (slt then sltu).
        if ((flags & 0x2000u) != 0u || static_cast<s32>(target) < static_cast<s32>(ahead) ||
            !(target < extent + ahead)) {
            return seek_aligned(target);
        }
        const u32 beyond = target - ahead;
        put(rt, file, word(rt, file + 0x10u) + beyond);  // _p
        put(rt, file + 4u, extent - beyond);             // _r
        release_ungetc();
        rt.memory().store16(file + 0xCu, static_cast<std::uint16_t>(flags & ~kEndOfFile));
        return 0u;
    };

    // 0x08A14: whence. SEEK_CUR becomes an absolute position; anything above SEEK_END is EINVAL.
    if (whence == 1u) {
        (void)call(rt, ctx, 0x08B7807Cu, {file});
        flags = rt.memory().load16(file + 0xCu);
        if ((flags & 0x1000u) == 0u) {
            curoff = call(rt, ctx, seek, {word(rt, file + 0x1Cu), 0u, 1u});
            if (curoff == 0xFFFFFFFFu) {
                ctx.gpr[2] = 0xFFFFFFFFu;
                return;
            }
            flags = rt.memory().load16(file + 0xCu);
        } else {
            curoff = word(rt, file + 0x50u);
        }
        if ((flags & 4u) != 0u) {
            curoff -= word(rt, file + 4u);
            if (word(rt, file + 0x30u) != 0u) curoff -= word(rt, file + 0x3Cu);
        } else if ((flags & 8u) != 0u && word(rt, file) != 0u) {
            curoff += word(rt, file) - word(rt, file + 0x10u);
        }
        // Every path loads the buffer base here, after the calls that may have made one (0x08CF8 / 0x8D10 /
        // 0x8C9C's delay slot, which runs whether or not the branch is taken).
        buffer = word(rt, file + 0x10u);
        offset += curoff;
        whence = 0u;
        havepos = true;
    } else if (whence == 2u) {
        buffer = word(rt, file + 0x10u);
    } else if (whence != 0u) {
        put(rt, reent, 0x16u);
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    } else {
        buffer = word(rt, file + 0x10u);
    }

    // 0x08A34: a stream without a buffer gets one before any of this is looked at.
    if (buffer == 0u) (void)call(rt, ctx, 0x08B797F0u, {file});
    flags = rt.memory().load16(file + 0xCu);

    // 0x08A40: pending writes, no buffering or no optimization flush and seek; a regular file behind the
    // default seek function is marked optimized and aligned instead.
    if ((flags & 0x81Au) != 0u) {
        ctx.gpr[2] = flush_and_seek();
        return;
    }
    if ((flags & 0x400u) == 0u) {
        bool optimized = false;
        if (seek == 0x08B7B804u) {
            const s32 handle = static_cast<std::int16_t>(rt.memory().load16(file + 0xEu));
            if (handle >= 0 && call(rt, ctx, 0x08B78EC8u, {reent, static_cast<u32>(handle), frame.at(0u)}) == 0u &&
                (word(rt, frame.at(4u)) & 0xF000u) == 0x8000u) {
                put(rt, file + 0x4Cu, 0x400u);
                flags |= 0x400u;
                rt.memory().store16(file + 0xCu, static_cast<std::uint16_t>(flags));
                optimized = true;
            }
        }
        if (!optimized) {
            flags |= 0x800u;
            rt.memory().store16(file + 0xCu, static_cast<std::uint16_t>(flags));
            ctx.gpr[2] = flush_and_seek();
            return;
        }
    }

    // 0x08B2C: SEEK_END adds the file's size to the offset first.
    u32 target = offset;
    if (whence != 0u) {
        const s32 handle = static_cast<std::int16_t>(rt.memory().load16(file + 0xEu));
        if (call(rt, ctx, 0x08B78EC8u, {reent, static_cast<u32>(handle), frame.at(0u)}) != 0u) {
            ctx.gpr[2] = flush_and_seek();
            return;
        }
        target = offset + word(rt, frame.at(0x10u));
        flags = rt.memory().load16(file + 0xCu);
    }
    ctx.gpr[2] = align(target);
}

} // namespace
} // namespace new_batch_main_255
} // namespace gt::game

#define NEW255(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_255::name, gt::ReturnKind::kind)
NEW255(0x08A076B4u, func_08A076B4, Word);
NEW255(0x08B789A4u, func_08B789A4, Word);

// ---- from main-session batch 257 (draft 802eb2fda7ae3d79dce3fe35a61ad9b96c8c31d3) ----
// New batch, main session 257: long functions the recompiler emitted that no earlier batch took, written by hand as
// structured C from `tools.disasm`. Drafts only: every one is still unverified until the differential gate runs it.
//
//  0x0881FEF4  walks the scene behind an object and fills an output block (started from the draft of commit 69238b3 and
//              re-read against the asm: the mode guard there is inverted, and the last smart-pointer release leaves v0
//              as the original does).
//  0x08815DBC  a per-frame display update of an object: a mode change through the vtable, three slot updates, a
//              countdown counter, the digits, then the value blocks.
//  0x08B6683C  the two-operand soft-float core of the statically linked libgcc: a stack record per operand shape, its
//              single rounding helper 0x08B685B0, and errno (EDOM / EINVAL) at the record's exception word.
//
// v0 (and v1 where the original answers a pair) is left as the original leaves it on every exit: the last call's result
// where the original ends with a `jal`, the address it compares against where it ends with a `lui`/`addiu`, the record's
// pair, or the caller's v0 on an early return.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_257 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values.
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                        std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_result(rt, ctx, target, arguments).v0;
}

// The usual release of a smart-pointer slot: nothing when it holds the static block 0x08D74FD0, else 0x08B60804 decides
// (a count; 0 or less releases) and 0x08B8542C releases with `extra` as its second argument. v0 is left as the original
// leaves it: the constant address it compares first, then the count (while it is positive), then the release's result.
void release_slot(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 extra) {
    const u32 value = word(rt, slot);
    const u32 block = value - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u;
    if (block == 0x08D74FD0u) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {value - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {block, extra});
}

// --- 0x0881FEF4: a1 = a mode (below 5 does nothing: the `slti`/`bnez` at the head jumps to the epilogue), a2 = the output
// block (a pointer at +0 and four smart-pointer destinations at +4 / +8 / +0xC / +0x10). The object at *(a2) is asked for a
// 16-byte value through its vtable slot +0xC, which is converted (0x08B88F44) and parsed into a 0x3C-byte block on the
// frame (0x08B10204). A non-zero first word takes the 0x08A40C14 path; a zero one walks the scene (0x08A32A94, 0x08A32350,
// 0x08B35254, 0x08A40C24, 0x08A5B9EC). Then *(a2)+4 is looked up (0x08A76ABC) and its virtual +0x70 read (0x08A40CF8,
// 0x08A41258), *(a2)+8 the same way (0x08A5BAD8), and the count from 0x08A40BF8 is handed to 0x0881FAF8 with the block at
// +0x34. For every element an identifier pair (0x08930170) and a float (virtual +0x54) go to 0x0881FB28 against the record
// at +0x48. The frame's smart pointers are released at the end.
void func_0881FEF4(Runtime &rt, AllegrexContext &ctx) {
    const u32 mode = ctx.gpr[5];
    const u32 output = ctx.gpr[6];
    GuestFrame frame(ctx, 0x100u);
    if (static_cast<s32>(mode) < 5) return;
    const u32 object = word(rt, output);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99294u});
    // A 16-byte value through vtable +0xC (the value slot at 0x18 receives it, the object is its second argument), then
    // 0x08B88F44 converts it into the 0x14 slot and 0x08B10204 parses it into the 0x90 block of the frame.
    const u32 value_slot = frame.at(0x18u);
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {value_slot, object});
    call(rt, ctx, 0x08B88F44u, {frame.at(0x14u), value_slot});
    call(rt, ctx, 0x08B10204u, {frame.at(0x90u), word(rt, frame.at(0x14u)), 1u});
    // 0x3C bytes: twelve words in 16-byte chunks plus the three after them (the asm stores those three out of order).
    for (u32 index = 0u; index != 15u; ++index)
        put(rt, frame.at(0x48u) + index * 4u, word(rt, frame.at(0x90u) + index * 4u));
    if (word(rt, frame.at(0x48u)) != 0u) {
        call(rt, ctx, 0x08A40C14u, {frame.at(0x1Cu), 0x08CF6ED8u});
    } else {
        call(rt, ctx, 0x08A32A94u, {frame.at(0x38u)});
        const u32 scene = word(rt, frame.at(0x38u));
        if (scene == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9931Cu});
        const u32 pair = word(rt, frame.at(0x68u));
        call(rt, ctx, 0x08A32350u, {scene, pair, word(rt, frame.at(0x58u)), 1u});
        call(rt, ctx, 0x08B35254u, {pair});
        const u32 again = word(rt, frame.at(0x38u));
        if (again == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9931Cu});
        call(rt, ctx, 0x08A40C24u, {frame.at(0x1Cu), again + 0x14u});
        call(rt, ctx, 0x08A5B9ECu, {frame.at(0x38u)});
    }
    release_slot(rt, ctx, frame.at(0x14u), frame.at(2u));
    // *(a2)+4 -> the 0x20 slot; its virtual +0x70 (a0 = 0x10, a2 = the 0x20 slot) -> the 0x24 slot; then the count.
    call(rt, ctx, 0x08A76ABCu, {frame.at(0x20u), output + 4u});
    const u32 first = word(rt, frame.at(0x1Cu));
    if (first == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99324u});
    call_indirect(rt, ctx, word(rt, word(rt, first) + 0x70u), {frame.at(0x10u), first, frame.at(0x20u)});
    call(rt, ctx, 0x08A40CF8u, {frame.at(0x24u), frame.at(0x10u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x10u)});
    const u32 counted = word(rt, frame.at(0x24u));
    if (counted == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99330u});
    call(rt, ctx, 0x08A41258u, {frame.at(0x28u), counted, 0u});
    // *(a2)+8 -> the 0x2C slot, the same virtual through the 8 / 0xC slots, then the 0x24 slot is replaced from the 0xC one.
    call(rt, ctx, 0x08A76ABCu, {frame.at(0x2Cu), output + 8u});
    const u32 third = word(rt, frame.at(0x28u));
    if (third == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99324u});
    call_indirect(rt, ctx, word(rt, word(rt, third) + 0x70u), {frame.at(8u), third, frame.at(0x2Cu)});
    call(rt, ctx, 0x08A40CF8u, {frame.at(0xCu), frame.at(8u)});
    call(rt, ctx, 0x08A5BAD8u, {frame.at(0x24u), word(rt, frame.at(0xCu))});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
    const u32 list = word(rt, frame.at(0x24u));
    if (list == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99330u});
    const u32 count = call(rt, ctx, 0x08A40BF8u, {list});
    call(rt, ctx, 0x0881FAF8u, {count, frame.at(0x34u)});
    call(rt, ctx, 0x08A76ABCu, {frame.at(0x30u), output + 0xCu});
    call(rt, ctx, 0x08A76ABCu, {frame.at(0x34u), output + 0x10u});
    if (static_cast<s32>(count) > 0) {
        for (u32 index = 0u; index != count; ++index) {
            const u32 owner = word(rt, frame.at(0x24u));
            if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99330u});
            call(rt, ctx, 0x08A41258u, {frame.at(4u), owner, index});
            call(rt, ctx, 0x08A5BAD8u, {frame.at(0x28u), word(rt, frame.at(4u))});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
            const u32 entry = word(rt, frame.at(0x28u));
            if (entry == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99324u});
            call(rt, ctx, 0x08A419E0u, {frame.at(0x38u), entry});
            const u32 record = word(rt, frame.at(0x38u));
            if (record == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99340u});
            call_indirect(rt, ctx, word(rt, word(rt, record) + 0x9Cu), {frame.at(0x3Cu), record, frame.at(0x30u)});
            const u32 inner = word(rt, frame.at(0x3Cu));
            if (inner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99294u});
            call_indirect(rt, ctx, word(rt, word(rt, inner) + 0xCu), {frame.at(0x40u), inner});
            const GuestResult identifier = call_result(rt, ctx, 0x08930170u, {word(rt, frame.at(0x40u))});
            const u32 second = word(rt, frame.at(0x38u));
            if (second == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99340u});
            call_indirect(rt, ctx, word(rt, word(rt, second) + 0x9Cu), {frame.at(0x44u), second, frame.at(0x34u)});
            const u32 number = word(rt, frame.at(0x44u));
            if (number == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C99294u});
            const GuestResult value = call_indirect(rt, ctx, word(rt, word(rt, number) + 0x54u), {number}, {});
            put(rt, frame.at(0x48u), identifier.v0);
            put(rt, frame.at(0x4Cu), identifier.v1);
            put_float(rt, frame.at(0x50u), value.f0);
            put(rt, frame.at(0x54u), index);
            call(rt, ctx, 0x0881FB28u, {index, frame.at(0x48u)});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x44u)});
            release_slot(rt, ctx, frame.at(0x40u), frame.at(1u));
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x3Cu)});
            call(rt, ctx, 0x08A5B9ECu, {frame.at(0x38u)});
        }
    }
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x34u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x30u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x2Cu)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x28u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x24u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x20u)});
    call(rt, ctx, 0x08A5B9ECu, {frame.at(0x1Cu)});
    release_slot(rt, ctx, frame.at(0x18u), frame.at(0u));
}
// The three level slots reset to zero (0x0880FAC takes the slot, the value and a level index 2 / 1 / 0).
void reset_slots(Runtime &rt, AllegrexContext &ctx, u32 self) {
    call(rt, ctx, 0x08810FACu, {self, word(rt, self + 0x6Cu), 0u, 0u, 0u, 2u});
    call(rt, ctx, 0x08810FACu, {self, word(rt, self + 0x68u), 0u, 0u, 0u, 1u});
    call(rt, ctx, 0x08810FACu, {self, word(rt, self + 0x64u), 0u, 0u, 0u, 0u});
}
// The tail both non-early exits share: the shown mode byte (+0xC) becomes the state's mode word's low byte, the shown flag
// byte (+0xD) the state's flag byte; v0 is the mode word.
u32 finish(Runtime &rt, u32 self, u32 state) {
    const u32 mode = word(rt, state + 0x110u);
    rt.memory().store8(self + 0xCu, static_cast<std::uint8_t>(mode));
    rt.memory().store8(self + 0xDu, static_cast<std::uint8_t>(rt.memory().load8(state + 0x108u)));
    return mode;
}

// --- 0x08815DBC: a0 = the object (its byte at +0xC is the shown mode, +0xD the shown flag, +0x23C / +0x5C / +0x60 / +0x64 /
// +0x68 / +0x6C / +0x88 the display blocks, +0x20 a running value, +0x26C the last time and +0x270 / +0x274 / +0x278 three
// counters), a1 = the state block (its word +0xA8 is the mode, +0x8C the time, +0x90 a scale, +0x94 / +0x98 / +0x9C three
// session counts, +0xB0 the level, +0x108 a flag byte, +0x110 a mode word). A mode change goes through 0x0880F6B0 (when the
// state asks for mode 1) or 0x0880F7E0; the three +0xD0 / +0xD4 / +0xD8 blocks take the time (0x08810010); a countdown at
// +0x278 is restarted from the time delta (level / 10); the digit count at +0x274 is advanced with 0x0880DBCC (counting
// down from 9) or 0x0880DC94 (counting up); then the two value blocks are shaped by 0x08812198 from the session counts,
// the three +0x6C / +0x68 / +0x64 slots are set (0x08810FAC, only on a mode change), the +0x20 value grows by the session
// count and 0x08A69B3C / 0x08969160 / 0x08A7BFD0 produce one more block that 0x08812198 takes.
//
// v0: the sign-extended byte the last early exit compared (the shown mode), or the state's mode word at +0x110.

// --- 0x08B6683C: a two-operand 64-bit soft-float core of the statically linked runtime (FINDINGS: the 0x08B6xxxx range
// holds the "soft-float / 64-bit code", func_08B66E88 / func_08B6728C / func_08B6EF68). a0/a1 is the first 64-bit operand,
// a2/a3 the second, and the global 0x08C96698 a mode: -1 leaves the work to 0x08B685B0 alone, 0 takes the plain rounding,
// 2 raises the exception. 0x08B685B0 rounds the two operands and returns the pair; the frame holds the record
// {+0 selector, +4 the constant table 0x08CB5E38, +8/+0xC operand one, +0x10/+0x14 operand two, +0x18/+0x1C the result
// pair, +0x20 the exception word} that 0x08B6E524 finishes. What the helpers do is read off their call sites (none of them
// is named here): 0x08B6E27C / 0x08B6E4FC test a pair (zero / equal), 0x08B71980 and 0x08B71BD0 an operand's sign,
// 0x08B71394 / 0x08B71A08 / 0x08B6E52C round, 0x08B71720 builds a seed pair, 0x08B77D40 gives the errno location. The
// constant pairs 0x08CB65E0 .. 0x08CB65FC are seeds and limits.
//
// Every exit answers the recorded pair: v0 = +0x18, v1 = +0x1C (or the initial 0x08B685B0 pair on the early returns).
void func_08B6683C(Runtime &rt, AllegrexContext &ctx) {
    const u32 first_low = ctx.gpr[4];
    const u32 first_high = ctx.gpr[5];
    const u32 second_low = ctx.gpr[6];
    const u32 second_high = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    const u32 record = frame.at(0u);
    u32 low;
    u32 high;
    {
        const GuestResult rounded =
            call_result(rt, ctx, 0x08B685B0u, {first_low, first_high, second_low, second_high});
        low = rounded.v0;
        high = rounded.v1;
    }
    const u32 mode = word(rt, 0x08C96698u);
    const auto raise = [&](u32 value) {
        const u32 where = call(rt, ctx, 0x08B77D40u);
        put(rt, where, value);
    };
    // The record's tail: the exception word goes to errno when it is set, then the pair at +0x18 / +0x1C is the answer.
    const auto take_record = [&]() {
        if (word(rt, frame.at(0x20u)) != 0u) raise(word(rt, frame.at(0x20u)));
        low = word(rt, frame.at(0x18u));
        high = word(rt, frame.at(0x1Cu));
    };
    // 0x08B6E524 says whether the record already carries the answer; otherwise the exception is EDOM / EINVAL.
    const auto finish_with_edom = [&]() {
        if (call(rt, ctx, 0x08B6E524u, {record}) == 0u) raise(0x21u);
        take_record();
    };
    const auto finish_with_einval = [&]() {
        raise(0x22u);
        take_record();
    };
    const auto finish_with_check = [&]() {  // the 4-selector record re-tests the mode before the EINVAL
        if (call(rt, ctx, 0x08B6E524u, {record}) != 0u) take_record();
        else finish_with_einval();
    };
    const auto finish_with_constant = [&]() {  // the seed pair at 0x08CB65E0 / 0x08CB65E4
        put(rt, frame.at(0x18u), word(rt, 0x08CB65E0u));
        put(rt, frame.at(0x1Cu), word(rt, 0x08CB65E4u));
        take_record();
    };
    const auto zero_then_edom = [&]() {
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        finish_with_edom();
    };
    const auto build_record = [&](u32 selector) {
        put(rt, frame.at(0x0u), selector);
        put(rt, frame.at(0x4u), 0x08CB5E38u);
        put(rt, frame.at(0x8u), first_low);
        put(rt, frame.at(0xCu), first_high);
        put(rt, frame.at(0x10u), second_low);
        put(rt, frame.at(0x14u), second_high);
        put(rt, frame.at(0x20u), 0u);
    };
    // The 4-selector record (an undefined shape): when the zero test passes, a mode of 2 raises EINVAL.
    const auto record_four = [&]() {
        if (call(rt, ctx, 0x08B71980u, {low, high, 0u, 0u}) != 0u) return;
        if (call(rt, ctx, 0x08B6E27Cu, {first_low, first_high}) == 0u) return;
        if (call(rt, ctx, 0x08B6E27Cu, {second_low, second_high}) == 0u) return;
        build_record(4u);
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        if (mode == 2u) finish_with_einval();
        else finish_with_check();
    };
    // The 3-selector record (a multiplication-like shape): the seed pair differs with the mode.
    const auto record_three = [&]() {
        build_record(3u);
        if (mode == 0u) {
            const u32 factor_low = word(rt, 0x08CB65F0u);
            const u32 factor_high = word(rt, 0x08CB65F4u);
            put(rt, frame.at(0x18u), word(rt, 0x08CB65E8u));
            put(rt, frame.at(0x1Cu), word(rt, 0x08CB65ECu));
            const GuestResult inner =
                call_result(rt, ctx, 0x08B71394u, {second_low, second_high, factor_low, factor_high});
            if (static_cast<s32>(call(rt, ctx, 0x08B71BD0u, {first_low, first_high, 0u, 0u})) >= 0) {
                finish_with_check();
                return;
            }
            const GuestResult probe = call_result(rt, ctx, 0x08B6E52Cu, {inner.v0, inner.v1});
            if (call(rt, ctx, 0x08B71A08u, {inner.v0, inner.v1, probe.v0, probe.v1}) == 0u) {
                finish_with_check();
                return;
            }
            put(rt, frame.at(0x18u), word(rt, 0x08CB65F8u));
            put(rt, frame.at(0x1Cu), word(rt, 0x08CB65FCu));
            finish_with_check();
            return;
        }
        // The mode-dependent seed pair: the globals at 0x08C96690 / 0x08C96694 held on the frame at 0x30 / 0x34.
        put(rt, frame.at(0x30u), word(rt, 0x08C96690u));
        put(rt, frame.at(0x34u), word(rt, 0x08C96694u));
        put(rt, frame.at(0x18u), word(rt, frame.at(0x30u)));
        put(rt, frame.at(0x1Cu), word(rt, frame.at(0x34u)));
        const GuestResult inner = call_result(rt, ctx, 0x08B71394u,
                                              {second_low, second_high, word(rt, 0x08CB65F0u),
                                               word(rt, 0x08CB65F4u)});
        if (static_cast<s32>(call(rt, ctx, 0x08B71BD0u, {first_low, first_high, 0u, 0u})) >= 0) {
            if (mode == 2u) finish_with_einval();
            else finish_with_check();
            return;
        }
        const GuestResult probe = call_result(rt, ctx, 0x08B6E52Cu, {inner.v0, inner.v1});
        if (call(rt, ctx, 0x08B71A08u, {inner.v0, inner.v1, probe.v0, probe.v1}) == 0u) {
            if (mode == 2u) finish_with_einval();
            else finish_with_check();
            return;
        }
        put(rt, frame.at(0x18u), word(rt, frame.at(0x30u)));
        put(rt, frame.at(0x1Cu), word(rt, frame.at(0x34u)) ^ 0x80000000u);
        if (mode == 2u) finish_with_einval();
        else finish_with_check();
    };
    // The 1-selector record used when the operands are ordered: a zero mode copies the first operand, mode 2 the seed.
    const auto record_one_ordered = [&]() {
        build_record(1u);
        if (call(rt, ctx, 0x08B71980u, {second_low, second_high, 0u, 0u}) != 0u) return;
        put(rt, frame.at(0x18u), first_low);
        put(rt, frame.at(0x1Cu), first_high);
        if (mode == 2u) finish_with_constant();
        else finish_with_edom();
    };
    const auto run = [&]() {
        if (mode == 0xFFFFFFFFu) return;
        if (call(rt, ctx, 0x08B6E4FCu, {second_low, second_high}) != 0u) return;
        if (call(rt, ctx, 0x08B6E4FCu, {first_low, first_high}) != 0u) {
            record_one_ordered();
            return;
        }
        if (call(rt, ctx, 0x08B71980u, {first_low, first_high, 0u, 0u}) != 0u) {
            // The rounded pair came out non-zero: either a zero test on it, or the 1/3-selector records.
            if (call(rt, ctx, 0x08B6E27Cu, {low, high}) != 0u) {
                record_four();
                return;
            }
            if (call(rt, ctx, 0x08B6E27Cu, {first_low, first_high}) == 0u) {
                record_four();
                return;
            }
            if (call(rt, ctx, 0x08B6E27Cu, {second_low, second_high}) == 0u) {
                record_four();
                return;
            }
            if (call(rt, ctx, 0x08B6E4FCu, {low, high}) != 0u) {
                build_record(1u);  // record five: a seed pair from 0x08B71720, or zeros
                if (mode == 0u) {
                    zero_then_edom();
                    return;
                }
                const GuestResult seed = call_result(rt, ctx, 0x08B71720u, {0u, 0u, 0u, 0u});
                put(rt, frame.at(0x18u), seed.v0);
                put(rt, frame.at(0x1Cu), seed.v1);
                if (mode != 2u) finish_with_edom();
                else finish_with_einval();
                return;
            }
            record_three();
            return;
        }
        if (call(rt, ctx, 0x08B71980u, {second_low, second_high, 0u, 0u}) != 0u) {
            // The second operand is not a plain zero: test its zero-ness and sign before the 1-selector record.
            if (call(rt, ctx, 0x08B6E27Cu, {second_low, second_high}) == 0u) return;
            if (static_cast<s32>(call(rt, ctx, 0x08B71BD0u, {second_low, second_high, 0u, 0u})) >= 0) return;
            build_record(1u);  // record six
            if (mode == 0u) {
                zero_then_edom();
                return;
            }
            put(rt, frame.at(0x18u), word(rt, 0x08C96690u));
            put(rt, frame.at(0x1Cu), word(rt, 0x08C96694u) ^ 0x80000000u);
            if (mode != 2u) finish_with_edom();
            else finish_with_einval();
            return;
        }
        build_record(1u);  // record two: both operands zero, the seed pair stays zero
        put(rt, frame.at(0x18u), 0u);
        put(rt, frame.at(0x1Cu), 0u);
        if (mode == 0u) finish_with_edom();
        else finish_with_constant();
    };
    run();
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

} // namespace
} // namespace new_batch_main_257
} // namespace gt::game

#define NEW257(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_257::name, gt::ReturnKind::kind)
NEW257(0x0881FEF4u, func_0881FEF4, Word);
NEW257(0x08B6683Cu, func_08B6683C, DoubleWord);

// ---- from main-session batch 258 (draft f2ff5503a599dbb44805fb6e1d4a8e6b734f97c4) ----
// New batch main 258 (main session): functions the recompiler emitted that no src/ file defines or registers. Written by
// hand from `tools.disasm` as structured C; a draft batch, not verified by the differential gate yet.
//
// Drafted: 0x08962AC0 (a shared-pointer pair assignment), 0x08A8D574 (the scope walk behind func_08A8DAB4, a vector
// append then a tail jump here).
// Skipped: 0x08C661C4 — a formatting path through a global table and facet-like objects: it resolves 0x08D77484 /
// 0x08D77498 through 0x08B5C2B0, indexes the result, calls virtual slots 0x10 / 0x14 / 0x18 / 0x8 / 0xC of the looked-up
// objects and keeps their -0xC / -0x4 headers (0x08B6461C, 0x08C742D8, 0x08C74884, 0x08C75524, 0x08B60804); a 0x100-byte
// buffer filled 0..0xFF, compared with 0x08B36170 and copied with 0x08B79A88 feeds two further 0x08B64C80 lookups. The
// lifetimes of those facet pointers could not be pinned down by hand, so it is left for a pass that can check them.
// Skipped: 0x089F1AEC — a matrix build: ~90 float multiplies/adds whose operands are reused across f16..f31, several of
// them in the delay slots of the three 0x08A06E94 calls and two 0x089A8B68 / 0x089A8B80 answers, then four 0x089641C4 row
// writes and a virtual slot 0x10C call with six arguments. Transcribing that arithmetic without the gate risks a silent
// value difference that only shows in the callee's writes, so it is left for a pass that can compare it.
//
// The input shapes the synthetic generator needs are in /tmp/claude-1000/shapes_main258.txt (as requested for this
// batch); `config/synth_shapes.txt` takes one struct per address, so the other argument registers of each of these two
// functions fall back to the generator's own arena layout.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_258 {
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

// --- 0x08962AC0: the reference counting of the shared-pointer control blocks --------------------------------
// libstdc++'s counted base: the counter at +4 is the use count, the one at +8 the weak count, and the block's vtable
// holds the dispose routine at +8 and the destroy routine at +0xC. The asm's ll/sc pair is one increment or one
// decrement here; both leave v0 = 1 (the successful `sc` answer, the `addiu v0, zero, 1` of the retry loop), which the
// exit paths below hand back untouched, so both helpers keep ctx.gpr[2] current.
void add_reference(Runtime &rt, AllegrexContext &ctx, u32 control) {
    put(rt, control + 4u, word(rt, control + 4u) + 1u);
    ctx.gpr[2] = 1u;
}
// One decrement; when the use count falls from 1 the block's dispose slot runs and the weak count is decremented too,
// and a weak count falling from 1 runs the destroy slot.
void release_reference(Runtime &rt, AllegrexContext &ctx, u32 control) {
    if (control == 0u) return;
    const u32 uses = word(rt, control + 4u);
    put(rt, control + 4u, uses - 1u);
    ctx.gpr[2] = 1u;
    if (uses != 1u) return;
    call_indirect(rt, ctx, word(rt, word(rt, control) + 8u), {control});
    const u32 weaks = word(rt, control + 8u);
    put(rt, control + 8u, weaks - 1u);
    ctx.gpr[2] = 1u;
    if (weaks == 1u) call_indirect(rt, ctx, word(rt, word(rt, control) + 0xCu), {control});
}

// 0x08962AC0 (out = a0, self = a1, source = a2): assigns the two shared pointers the source pair names to `out`, the
// pair's first word through func_08BE476C(out + 0x10, self, source[0], 0x80) and the second through the member at
// self + 8. out's four words are zeroed first. A set source[0] stores the new pointer, moves the reference from the
// temporary when out[1] did not hold it already (otherwise the temporary's own reference is released), releases the
// block it replaced and finally releases the temporary's reference. A set source[1] rebuilds the member: func_089625E0
// fixes the pointer, a 0x18-byte control block (vtable 0x08CC2FA8, use and weak count 1, the pointer at +0xC, the
// member's pointer at +0x10, its old control block at +0x14) comes from func_08B62344, func_08B8E380 fills out + 8..+0xC
// through func_08BE449C(out + 8, temporary pair at sp + 0, source[1], member + 8) and the replaced control block is
// released. The old block's counts are the asm's own: three increments and three releases around the allocation.

// --- 0x08A8D574: the symbol-scope lookup of the vector append --------------------------------------------------
// func_08A5BAD8(dst, *(src)): the asm's assignment, skipped when dst is the very object src names (the
// `beql s6, v0` / address compare the compiler leaves at every member assignment).
void assign_holder(Runtime &rt, AllegrexContext &ctx, u32 destination, u32 source) {
    if (destination != source) (void)call(rt, ctx, 0x08A5BAD8u, {destination, word(rt, source)});
}
// func_08A759E0(out, vector, 0) builds a holder for item 0 (func_08A6972C), its word is read before func_08A69D68
// destroys the temporary, and the word is compared with the type's empty rep.
bool probe_first_item(Runtime &rt, AllegrexContext &ctx, u32 out, u32 vector, u32 empty_rep) {
    (void)call(rt, ctx, 0x08A759E0u, {out, vector, 0u});
    const u32 found = word(rt, out);
    (void)call(rt, ctx, 0x08A69D68u, {out});
    return found == empty_rep;
}

// 0x08A8D574 (owner = a0, vector = a1, mode = a2): the work func_08A8DAB4 hands over after it appended one item to the
// vector at owner + 0x14. A temporary holder (sp + 0x3C) starts empty (func_08A5BAB8 with the empty rep 0x08CF6ED8).
// Item 0 of `vector` is probed twice: when its word is one of the two empty reps (0x08CF6F04, then 0x08CF6F18) the
// temporary takes owner + 0x24 or owner + 0x20 through func_08A5BAD8. Otherwise the scope at owner + 0x20 goes into the
// key's holder (func_08AAE3A8) and is walked: while the scope's table at +0x24 (func_08C2B010) has no entry for item 0
// the walk moves to the parent (func_08AAE358, only while the vector holds two or more items) and ends at a null scope.
// An entry found is taken as the temporary (its word at +0x14) and, when that word's +0x14 is 8, the item is inserted
// with func_08A8D308(sp + 0x2C, owner, key, item, mode) and becomes the temporary.
// The result holder (sp + 0x40) then starts empty (func_08AAE204); an empty temporary makes its source owner + 0x20 and
// walks every item from 0, a filled one starts at item 1 and has its virtual slot 0x18 called once more at the end. Each
// step inserts through func_08A8D308 and asks the temporary's virtual slot 0x18 (a holder written at sp + 0x44 / sp) to
// move on. Finally the result goes back into owner + 0x20 (func_08A5BAD8, unless empty or its own address) and both
// temporaries are released (func_08A5B9EC); v0 is that last release's. A null scope in either walk is reported through
// func_08A5BB7C (0x08CB1D54 on the walk, 0x08CB1D60 on the temporary) and read through anyway, as the original does.
void func_08A8D574(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 vector = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    const u32 temporary = frame.at(0x3Cu);
    const u32 key = frame.at(0x44u);
    const u32 result = frame.at(0x40u);
    (void)call(rt, ctx, 0x08A5BAB8u, {temporary, 0x08CF6ED8u});
    if (probe_first_item(rt, ctx, frame.at(0x38u), vector, 0x08CF6F04u)) {
        assign_holder(rt, ctx, temporary, owner + 0x24u);
    } else if (probe_first_item(rt, ctx, frame.at(0x34u), vector, 0x08CF6F18u)) {
        assign_holder(rt, ctx, temporary, owner + 0x20u);
    } else {
        (void)call(rt, ctx, 0x08AAE3A8u, {key, owner + 0x20u});
        while (word(rt, key) != 0u) {
            (void)call(rt, ctx, 0x08A759E0u, {frame.at(0x30u), vector, 0u});
            const u32 entry = call(rt, ctx, 0x08C2B010u, {word(rt, key) + 0x24u, frame.at(0x30u)});
            (void)call(rt, ctx, 0x08A69D68u, {frame.at(0x30u)});
            const u32 scope = word(rt, key);
            if (scope == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D54u});
            if (entry != scope + 0x28u) {
                assign_holder(rt, ctx, temporary, entry + 0x14u);
                if (word(rt, word(rt, temporary) + 0x14u) == 8u) {
                    (void)call(rt, ctx, 0x08A759E0u, {frame.at(0x28u), vector, 0u});
                    (void)call(rt, ctx, 0x08A8D308u, {frame.at(0x2Cu), owner, key, frame.at(0x28u), mode});
                    (void)call(rt, ctx, 0x08A5BAD8u, {temporary, word(rt, frame.at(0x2Cu))});
                    (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(0x2Cu)});
                    (void)call(rt, ctx, 0x08A69D68u, {frame.at(0x28u)});
                }
                break;
            }
            const auto size = static_cast<s32>(word(rt, vector + 4u) - word(rt, vector)) >> 2;
            if (static_cast<u32>(size) < 2u) break;
            if (scope == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D54u});
            (void)call(rt, ctx, 0x08AAE358u, {frame.at(0x24u), scope});
            (void)call(rt, ctx, 0x08A5BAD8u, {key, word(rt, frame.at(0x24u))});
            (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(0x24u)});
        }
        (void)call(rt, ctx, 0x08A5B9ECu, {key});
    }
    (void)call(rt, ctx, 0x08AAE204u, {result, 0x08CF6ED8u});
    if (word(rt, temporary) != 0u) {
        for (u32 index = 1u; index < call(rt, ctx, 0x08A759BCu, {vector}); ++index) {
            const u32 held = word(rt, temporary);
            if (held == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D60u});
            call_indirect(rt, ctx, word(rt, word(rt, held) + 0x18u), {key, held});
            const u32 scope = word(rt, key);
            if (scope != 0u) {
                (void)call(rt, ctx, 0x08A759E0u, {frame.at(0x20u), vector, index});
                const u32 entry = call(rt, ctx, 0x08C2B010u, {scope + 0x24u, frame.at(0x20u)});
                (void)call(rt, ctx, 0x08A69D68u, {frame.at(0x20u)});
                const u32 current = word(rt, key);
                if (current == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D54u});
                if (entry == current + 0x28u) {
                    (void)call(rt, ctx, 0x08A759E0u, {frame.at(0x18u), vector, index});
                    (void)call(rt, ctx, 0x08A8D308u, {frame.at(0x1Cu), owner, key, frame.at(0x18u), mode});
                    (void)call(rt, ctx, 0x08A5BAD8u, {temporary, word(rt, frame.at(0x1Cu))});
                    (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(0x1Cu)});
                    (void)call(rt, ctx, 0x08A69D68u, {frame.at(0x18u)});
                } else {
                    assign_holder(rt, ctx, temporary, entry + 0x14u);
                    if (word(rt, word(rt, temporary) + 0x14u) == 8u) {
                        (void)call(rt, ctx, 0x08A759E0u, {frame.at(0x10u), vector, index});
                        (void)call(rt, ctx, 0x08A8D308u, {frame.at(0x14u), owner, key, frame.at(0x10u), mode});
                        (void)call(rt, ctx, 0x08A5BAD8u, {temporary, word(rt, frame.at(0x14u))});
                        (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(0x14u)});
                        (void)call(rt, ctx, 0x08A69D68u, {frame.at(0x10u)});
                    }
                }
            }
            (void)call(rt, ctx, 0x08A5B9ECu, {key});
        }
        const u32 held = word(rt, temporary);
        if (held == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D60u});
        call_indirect(rt, ctx, word(rt, word(rt, held) + 0x18u), {frame.at(0xCu), held});
        (void)call(rt, ctx, 0x08A5BAD8u, {result, word(rt, frame.at(0xCu))});
        (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
    } else {
        assign_holder(rt, ctx, result, owner + 0x20u);
        for (u32 index = 0u; index < call(rt, ctx, 0x08A759BCu, {vector}); ++index) {
            (void)call(rt, ctx, 0x08A759E0u, {frame.at(4u), vector, index});
            (void)call(rt, ctx, 0x08A8D308u, {frame.at(8u), owner, result, frame.at(4u), mode});
            (void)call(rt, ctx, 0x08A5BAD8u, {temporary, word(rt, frame.at(8u))});
            (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(8u)});
            (void)call(rt, ctx, 0x08A69D68u, {frame.at(4u)});
            const u32 step = word(rt, temporary);
            if (step == 0u) (void)call(rt, ctx, 0x08A5BB7Cu, {0x08CB1D60u});
            call_indirect(rt, ctx, word(rt, word(rt, step) + 0x18u), {frame.at(0u), step});
            (void)call(rt, ctx, 0x08A5BAD8u, {result, word(rt, frame.at(0u))});
            (void)call(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
        }
    }
    const u32 value = word(rt, result);
    if (value != 0u && owner + 0x20u != result) (void)call(rt, ctx, 0x08A5BAD8u, {owner + 0x20u, value});
    (void)call(rt, ctx, 0x08A5B9ECu, {result});
    (void)call(rt, ctx, 0x08A5B9ECu, {temporary});
}

} // namespace
} // namespace new_batch_main_258
} // namespace gt::game

#define NEWMAIN258(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_258::name, gt::ReturnKind::kind)
NEWMAIN258(0x08A8D574u, func_08A8D574, Word);

// ---- from main-session batch 259 (draft 8d18069a38eafb9773a544391030c601d6078a41) ----
// New batch 259 (main session): functions no src/ file defines or registers, written by hand from
// `tools/disasm` as structured C. This is a draft: the differential gate has not run on it yet.
// Delay slots are read as "before the branch / call"; v0 (and f0 where the original ends with it)
// is what the asm leaves on every exit. s0-s7 / fp / ra and f20-f31 are callee-saved: the bodies set
// them where the asm sets them and put the caller's values back.
//
// Two of the four addresses this batch was given are left out, both libstdc++ library code (AGENTS.md: the
// statically linked SDK/libstdc++ is kept as asm until the game code is done, and neither pasted nor guessed):
//   - 0x08B6300C: a vtable member (its address appears only as a slot of the .rodata table at 0x08CDC024, next to
//     the 0x08B62674..0x08B62EA0 members and the `St9exception` / `St13...` typeinfo names) and only in .eh_frame;
//     no `jal` or `j` in .text reaches it. A 20-block `num_get`-shaped state machine over iterator pairs.
//   - 0x08C6A6C0: likewise a vtable member, slot 0x08CDB960 of the 0x08C59xxx / 0x08C6Axxx locale-and-istream
//     cluster (0x08C5AC7C's comment already calls that key a locale facet). Same shared-block maze; a faithful
//     structured rewrite is not something this draft can stand behind without the gate.
// Both need their input shapes (iterator pairs, facet objects) before they can ever be compared; skipping them
// here keeps the batch's two verified-able bodies honest.
//
// Static audits of the two bodies below (`tools.frame_audit` 0 wrong, `tools.literal_check` 0 missing):
//   - `tools.sp_loads`: the two `lw $s0, 4($sp)` at 088D85C0 / 088D8664 are modelled — s0 is the word at sp + 4,
//     which 0x08B8E380 may have written, and the C++ re-reads it from the frame (`word(rt, slot4)`).
//   - `tools.arg_reads`: 0x08B2CB18's a3 / t0 and 0x08B8E380's a3-t3 are the two known harmless kinds: the first is
//     the same three-argument call the already decompiled 0x0890F1C0 makes, the second only spills the registers it
//     is handed into its own frame (a va-list prologue) and the harness ignores that dead stack.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_259 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set the way the original's `jal` finds them: afterwards v0 / v1 /
// f0 are the callee's (a callee that never writes v0 hands the previous one back).
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
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
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void copy3(Runtime &rt, u32 destination, u32 source) {
    for (u32 at = 0u; at < 0xCu; at += 4u) put(rt, destination + at, word(rt, source + at));
}

// s0-s7, fp and ra are callee-saved: the body sets them where the asm sets them (a callee may read them) and the
// caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// f20-f31 are callee-saved too.
class SavedFpr {
public:
    explicit SavedFpr(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 12u; ++i) saved_[i] = ctx.fpr[20u + i];
    }
    ~SavedFpr() {
        for (unsigned i = 0u; i < 12u; ++i) ctx_.fpr[20u + i] = saved_[i];
    }
    SavedFpr(const SavedFpr &) = delete;
    SavedFpr &operator=(const SavedFpr &) = delete;

private:
    AllegrexContext &ctx_;
    float saved_[12];
};

// The reference-count word at object + 4 (an `ll`/`sc` pair in the original, one retry loop each; the case runs
// single-threaded, so one read-modify-write is the same sequence).
void add_ref(Runtime &rt, u32 object) {
    const u32 count = word(rt, object + 4u);
    put(rt, object + 4u, count + 1u);
}
// Returns the count as it was before the decrement (the original compares the value the `ll` returned).
u32 release_ref(Runtime &rt, u32 where) {
    const u32 count = word(rt, where);
    put(rt, where, count - 1u);
    return count;
}
// The shared-owner release the original inlines everywhere: the first count reaching zero runs vtable slot +8 with
// the object, the second one slot +0xC; the table is re-read for the second call.
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (release_ref(rt, object + 4u) != 1u) return;
    u32 table = word(rt, object);
    call_indirect(rt, ctx, word(rt, table + 8u), {object});
    if (release_ref(rt, object + 8u) != 1u) return;
    table = word(rt, object);
    call_indirect(rt, ctx, word(rt, table + 0xCu), {object});
}

// 0x088D81C4 (a0 = object): resource-load loop. 0x08B28C44(object + 0x6CC) runs first every round; a nonzero word at
// object + 0x14 opens the body, and the byte at object + 0x6F8 ends the loop (its value is v0). The body builds the
// name "crs/" at sp + 8, loads object + 4 with 0x088D5FDC(object + 4, sp + 0xC, 0x3C), resolves the name through
// 0x08B0EC60 into a 0x3C-byte record at sp + 0x90, and takes the word at +0 / the one at +0x10 of its copy: with the
// first one zero the second one is the value that goes on. The two words at object + 0x6B4 / + 0x6B8 are reset and
// the old owner released; a nonzero value then looks up the entry at object + 0x6A4 (0x089624EC returns it), locks
// the node at +0x6C, fills a 0x80-byte block at node + 8 (0x08B2CB18) into a new 0x18-byte holder (vtable
// 0x08CC2F90), and moves the pair into the two slots with their reference counts. Everything else is bookkeeping:
// 0x08B8E380(&slot, pointer, pointer), 0x088FE08C(object + 0x18, sp + 8, +0x6B4, value), 0x088D6048(object + 4,
// sp + 8, 0x40), object + 0x640's 0x088E6A9C, the two words at +0x6C0 / +0x6C4 into +0x20 / +0x24 of *object+0x130,
// and 0x088FF228(object + 0x14, sp + 8).

// 0x088C8AE8 (a0 = object, f12 / f13 = two limits): fills a grid of 12-byte records from the object's blocks.
// 0x088D2C30(object + 4, kind) selects the two record kinds (6 or 2 for the first, 6 or 1 for the second; 9 resp. 8
// when the probe answers nonzero). The row pitch is the constant 5.0 at object + 0xAC8 (at least 1), the column pitch
// at +0xAC4, and the records go to object + 0x250 with the count at +0xACC (capacity 0x8C; a full array drops the
// rest). 0x088D32E0(record, object + 4, kind, value) builds a record; 0x08B4AA68 / 0x08B4AA34 subtract / add 3-float
// vectors, 0x08B4A720 returns the squared length, 0x08B4A5E8 scales in place and 0x088C896C(object, vector) takes
// each result. v0 is 0, except on the early exit of the row-count clamp (the value just stored) and the two early
// returns that zero the byte at object + 8.

} // namespace
} // namespace new_batch_main_259
} // namespace gt::game

#define NEW259(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_259::name, gt::ReturnKind::kind)

