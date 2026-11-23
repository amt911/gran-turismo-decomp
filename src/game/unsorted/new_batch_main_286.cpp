// ---- from main-session batch 247 (draft ad550d6593378491eeb673d239761a37cdfc0a4e) ----
// New batch main 247: functions the gate could never compare (no route reaches them with a usable object), written by
// hand from `tools.disasm`. Nothing here is verified yet: the batch's synthetic shapes (the 0x08BA86F4 / 0x08BA9DB8 /
// 0x08BDC338 lines handed over with this batch) are what let the generator build a vector whose three pointers are
// valid, which is the input these bodies need to get past their first branch.
//
// The four drafted ones, and the shape each one has:
//   - 0x08BA86F4 and 0x08BA9DB8: libstdc++ vector<8-byte element>::insert(position, count, value) for an element
//     {word +0, shared-pointer control block +4}. The two are instruction-for-instruction the same body and differ only
//     in the eight library callees they call and in the throw message, so one body takes them as a table.
//   - 0x08BDC338: the same insertion, same instruction sequence again (only its callees and the throw message differ),
//     except that the original leaves v0 = the new capacity / untouched instead of v0 = a0. `hot_batch_116.cpp` holds a
//     parked copy of this body (registered nowhere); delete that one when this registers — one address, one body.
//   - 0x089E519C: an initialiser that, for a0 == 1 and a1 == 0xFFFF, sets up two objects and then registers fourteen
//     label/name pairs through 0x08C09694, each with two words copied from a global pair into the frame; for a0 == 0 it
//     shuts those two objects down and tail-calls the second teardown.
// Dropped, not drafted (the asm was read, the body not written):
//   - 0x088F82D0: about ninety VFPU instructions (lv.s, vsub.q, vdot.t, vdiv.s, vexp2.t, vscl.q, vone.q on matrix
//     columns and with E/S prefixes) that end with the VFPU register file holding its results and then write 0x10-byte
//     vertex records and two display-list runs. A transcription without checking every prefix and the harness's VFPU
//     comparison would be a guess.
//   - 0x08B2D824: the worker-thread body 0x08B2D314 starts through 0x08C7AA18(0x08CB53E8, 0x08B2D824, 0x20, 0x1000,
//     0x00200000, 0) — a streaming read over the globals 0x08D68CF8 (its state word), 0x08D68D50 and 0x08D68D54, with
//     swl/swr and lwl/lwr on an unaligned buffer and five import calls. Its behaviour is a function of those globals,
//     which no synthetic shape sets, so an input that compares is not something this batch can produce.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_247 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call that leaves v0 / v1 / f0 as the callee left them (the next instruction sees them), with the argument
// registers set in order from a0 (EABI: a0-a3, then t0-t3).
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
// object->vtable[slot](): v1 = the vtable, v0 = the target (both compared by the harness only through what follows).
u32 call_virtual_at(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call_set(rt, ctx, ctx.gpr[2], arguments);
}
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }

// The ll/sc pair on a shared object's use count, single-threaded here (the host never preempts inside one): the word is
// read into the register the original reads it into (v0 for the increment's `ll`, a0 for the decrement's), incremented
// or decremented and written back, and v0 ends as the successful `sc` leaves it (1). v1 holds the address in both.
void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old == 1u;
}
// One use goes away: at zero the object's control block runs slot 8 (dispose), then its weak count (+8) goes down and at
// zero slot 0xC (the control block itself), both with a0 = the control block.
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 block) {
    if (!count_down(rt, ctx, block + 4u)) return;
    call_virtual_at(rt, ctx, block, 8u, {block});
    if (count_down(rt, ctx, block + 8u)) call_virtual_at(rt, ctx, block, 0xCu, {block});
}

// The eight library callees one vector<8-byte element> insertion needs. The three copies of this body (below) differ in
// nothing else but these addresses and the throw message.
struct InsertCallees {
    u32 move_tail;      // moves the (count) elements at the end out of the way
    u32 fill;           // copy-constructs `count` elements at a position from the frame element
    u32 fill_short;     // constructs the (count - after) new tail elements
    u32 move_short;     // shifts the `after` following elements up
    u32 throw_message;  // the length_error message handed to the throw helper
    u32 build_front;    // copy-constructs the range before the position into the new block
    u32 build_fill;     // fills the new range
    u32 destroy;        // destroys the old range
};
constexpr u32 kThrow = 0x08B5ADDCu;    // the length_error throw
constexpr u32 kAllocate = 0x08B35DA0u; // operator new
constexpr u32 kFree = 0x08B35254u;     // operator delete

// How the original leaves v0 when it returns: some copies end with `addu v0, a0, zero` (v0 = whatever a0 holds at the
// epilogue), the other one with the capacity it just stored in the vector.
enum class Return { A0, Capacity };

// The body itself, transcribed from 0x08BA86F4 / 0x08BA9DB8 / 0x08BDC338, which are the same instructions:
//
//   vector = {begin +0, end +4, capacity +8}, element = {value +0, control block +4}; count == 0 does nothing.
//   With room: the value is copied to the frame element (frame +0xC, its control block's use count incremented); the
//   elements after the position move up by count (`move_tail` for the tail, then word and shared-pointer assignments
//   backwards, each new block counted up and the old one released) or, when fewer follow, `fill_short` / `move_short`;
//   `fill` copy-constructs at the position; the frame element's block is released.
//   Full: size + max(size, count) elements (more than 0x1FFFFFFF throws; `kAllocate` gets the bytes), `build_front` /
//   `build_fill` / `build_front` build the new range, `destroy` destroys the old one, `kFree` frees it, and begin / end /
//   capacity are updated.
void insert_shared_values(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, const InsertCallees &callees,
                          Return returns) {
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    put(rt, frame.at(0x20u), count);
    if (count == 0u) {
        if (returns == Return::A0) ctx.gpr[2] = ctx.gpr[4];
        return;
    }
    const u32 end = word(rt, vector + 4u);
    const u32 room = static_cast<u32>(signed_of(word(rt, vector + 8u) - end) >> 3);
    if (room < count) {
        // No room: a fresh block of size + max(size, count) elements.
        const u32 begin = word(rt, vector);
        const u32 size = static_cast<u32>(signed_of(end - begin) >> 3);
        put(rt, frame.at(8u), size);
        if (0x1FFFFFFFu - size < count) call_set(rt, ctx, kThrow, {callees.throw_message});
        const u32 grow = size < count ? count : size;
        const u32 total = size + grow;
        u32 bytes = 0xFFFFFFF8u;  // a wrapped total asks for -8 bytes
        u32 block = 0u;
        if (total < size) {
            block = call_set(rt, ctx, kAllocate, {bytes});
        } else {
            bytes = total << 3;
            if (total != 0u) block = call_set(rt, ctx, kAllocate, {bytes});
        }
        const u32 front = call_set(rt, ctx, callees.build_front, {word(rt, vector), position, block, frame.at(3u)});
        call_set(rt, ctx, callees.build_fill, {front, count, end, frame.at(2u)});
        const u32 back = call_set(rt, ctx, callees.build_front,
                                  {position, word(rt, vector + 4u), front + (count << 3), frame.at(1u)});
        call_set(rt, ctx, callees.destroy, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        const u32 old_begin = word(rt, vector);
        if (old_begin != 0u) call_set(rt, ctx, kFree, {old_begin});
        const u32 capacity = block + bytes;
        put(rt, vector + 8u, capacity);
        put(rt, vector + 4u, back);
        put(rt, vector, block);
        ctx.gpr[2] = returns == Return::A0 ? ctx.gpr[4] : capacity;
        return;
    }
    // Room: the value goes to a frame element first, holding one use of its control block.
    const u32 element = frame.at(0xCu);
    const u32 block = word(rt, value + 4u);
    put(rt, element + 4u, block);
    put(rt, element, word(rt, value));
    if (block != 0u) count_up(rt, ctx, block + 4u);
    const u32 after = static_cast<u32>(signed_of(end - position) >> 3);
    if (count < after) {
        const u32 source_end = end - (count << 3);
        call_set(rt, ctx, callees.move_tail, {source_end, end, end, frame.at(6u)});
        const u32 middle = static_cast<u32>(signed_of(source_end - position) >> 3);
        put(rt, vector + 4u, word(rt, vector + 4u) + (count << 3));
        if (signed_of(middle) > 0) {
            u32 source = source_end;
            u32 destination = end;
            for (u32 index = 0u; index != middle; ++index) {
                source -= 8u;
                destination -= 8u;
                const u32 moved = word(rt, source);
                const u32 old_block = word(rt, destination + 4u);
                put(rt, destination, moved);
                const u32 new_block = word(rt, source + 4u);
                const u32 slot = destination + 4u;
                if (new_block == old_block) continue;
                if (new_block != 0u) count_up(rt, ctx, new_block + 4u);
                const u32 previous = word(rt, slot);
                if (previous != 0u) release_shared(rt, ctx, previous);
                put(rt, slot, new_block);
            }
        }
        call_set(rt, ctx, callees.fill, {position, position + (count << 3), element});
    } else {
        call_set(rt, ctx, callees.fill_short, {end, count - after, element, frame.at(5u)});
        const u32 new_end = word(rt, vector + 4u) + ((count - after) << 3);
        put(rt, vector + 4u, new_end);
        call_set(rt, ctx, callees.move_short, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (after << 3));
        call_set(rt, ctx, callees.fill, {position, end, element});
    }
    // `lw s0, 0x10($sp)`: the frame element's control block again (sp_loads lists it, it is the word stored at frame
    // +0xC + 4 above, not a register reload).
    const u32 element_block = word(rt, frame.at(0x10u));
    if (element_block != 0u) release_shared(rt, ctx, element_block);
    if (returns == Return::A0) ctx.gpr[2] = ctx.gpr[4];
}

// The fourteen label/name registrations of 0x089E519C: two words copied from `source` into the frame pair at `offset`
// (a descriptor 0x08C09694 stores at `name`), labelled by `label`.
struct Registration {
    u32 name;
    u32 label;
    u32 source;
    u32 offset;
};
constexpr Registration kRegistrations[] = {
    {0x08CF5724u, 0x08CA78ECu, 0x08C8D680u, 0x68u},
    {0x08CF5714u, 0x08CA78F8u, 0x08C8D678u, 0x60u},
    {0x08CF5704u, 0x08CA7904u, 0x08C8D670u, 0x58u},
    {0x08CF56F4u, 0x08CA7918u, 0x08C8D668u, 0x50u},
    {0x08CF56E4u, 0x08CA7924u, 0x08C8D660u, 0x48u},
    {0x08CF56D4u, 0x08CA7930u, 0x08C8D658u, 0x40u},
    {0x08CF56C4u, 0x08CA7940u, 0x08C8D650u, 0x38u},
    {0x08CF56B4u, 0x08CA7950u, 0x08C8D648u, 0x30u},
    {0x08CF56A4u, 0x08CA795Cu, 0x08C8D640u, 0x28u},
    {0x08CF5694u, 0x08CA7968u, 0x08C8D638u, 0x20u},
    {0x08CF5684u, 0x08CA7978u, 0x08C8D630u, 0x18u},
    {0x08CF5674u, 0x08CA7980u, 0x08C8D628u, 0x10u},
    {0x08CF5664u, 0x08CA7994u, 0x08C8D620u, 0x08u},
    {0x08CF5654u, 0x08CA79A4u, 0x08C8D618u, 0x00u},
};

} // namespace

// 0x08BA86F4 (a0 = vector of {word, shared-pointer} elements, a1 = position, a2 = count, a3 = the value to copy): the
// libstdc++ insertion with room or with a fresh block, as the shared body above does it. v0 = a0 at the epilogue.

// 0x08BA9DB8: the same body again, inserted through the mirror library (0x08BA91A8 / 0x08BA9600 / 0x08BA9068 /
// 0x08BA90B8 / 0x08BA9158 / 0x08BA9108 / 0x08BA8D90, message 0x08CA0740).

// 0x08BDC338: the same insertion once more (0x08BDBFE0 / 0x08BDC228 / 0x08BDC030 / 0x08BDC080 / 0x08BDC1D8 /
// 0x08BDC188 / 0x08B8ECAC, message 0x08CA1A44). This one leaves v0 = the capacity it computed (or untouched when
// count == 0). hot_batch_116.cpp keeps a parked copy of this body; delete that one when this registers.

// 0x089E519C (a0 = mode, a1 = 0xFFFF for the wildcard): mode 1 with 0xFFFF sets two objects up (0x08B28DCC twice, then
// 0x08A92114 and 0x0897D534 with their tables) and registers the fourteen descriptors; mode 1 with any other a1 returns
// 0xFFFF. Mode 0 with 0xFFFF shuts the two objects down (0x08B28DC4 on the second, then a tail call on the first); any
// other mode returns 0xFFFF (mode out of range: the same, as the branch's delay slot sets v0 either way).
void func_089E519C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x80u);
    const u32 mode = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    if (mode == 1u) {
        ctx.gpr[2] = 0xFFFFu;
        if (argument != 0xFFFFu) return;
        call_set(rt, ctx, 0x08B28DCCu, {0x08CF575Cu});
        call_set(rt, ctx, 0x08B28DCCu, {0x08CF5734u});
        call_set(rt, ctx, 0x08A92114u,
                 {0x08CF5638u, 0x08CA78E0u, 0x1F0u, 0x089E4D24u, 0x089E94F8u, 0x089E4D48u});
        call_set(rt, ctx, 0x0897D534u, {0x08CF5628u, 0x08CA78E0u, 0x089E67DCu, 0x089E6798u});
        for (const Registration &registration : kRegistrations) {
            put(rt, frame.at(registration.offset), word(rt, registration.source));
            put(rt, frame.at(registration.offset + 4u), word(rt, registration.source + 4u));
            call_set(rt, ctx, 0x08C09694u,
                     {registration.name, registration.label, frame.at(registration.offset), 0x08CF5628u});
        }
        return;
    }
    ctx.gpr[2] = 0xFFFFu;
    if (mode != 0u) return;
    if (argument != 0xFFFFu) return;
    call_set(rt, ctx, 0x08B28DC4u, {0x08CF5734u});
    tail_call(ctx, 0x08B28DC4u, {0x08CF575Cu});
}
} // namespace new_batch_main_247
} // namespace gt::game

#define MAIN247(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_247::name, gt::ReturnKind::kind)
MAIN247(0x089E519Cu, func_089E519C, Word);

// ---- from main-session batch 248 (draft c8ac2029cf4361b6655599a54467da832f7902af) ----
// New batch 248 (main session): hand-written drafts from `tools/disasm`, not gated yet.
//   0x08B04000: two passes over an object's 240-byte elements: (1) from the index at +0x1C, walk the chain the pointer at
//               +0x24 holds (next index at +0xE8, negative ends); every element whose halfword at +0xE2 lacks 0x80 is
//               handed to 0x08B00DF0 and has bit 14 of that halfword cleared, unless its flags say otherwise (bit 2 set,
//               bit 14 of the same word set, and bit 14 of the 6-byte table entry at +0x7C + 6*(+0xEA) set). (2) For the
//               slot at +0xC0, walk the index list [0x2C, 0x30): each later element is compared by its float at
//               +0x98 + 4*slot and, unless a flag or a predicate says otherwise, merged (0x08AFD540, 0x08AFFA70, the two
//               virtuals at +0xDC's slot 8); the round closes with 0x08B02FD4's report and 0x08B00E90. The loop advances
//               the cursor by exactly one index per round; v0 = the final cursor, 0 on an early out.
//   0x08921584: rescales an object's angles from a second object's settings: ten virtual queries (vtable slots 0x1C, 0x28,
//               0x30) pick four signs/halves from the flags plus two rates, the object's floats at +0x14 and +0x30 are
//               clamped, the angles at +0xC and +0x10 wrap into [-pi, pi] and the position at +0x1C/+0x20 moves by the
//               rates; flag 0x34 toggles, and the last query resets the whole state (v0 = the object at +0xC).
//   0x08B6C158: an odd/even function of a 64-bit double (a0/a1) with a second 64-bit value (a2/a3): x*x feeds an eleven-step
//               polynomial in x through the soft-double helpers 0x08B71394 (multiply), 0x08B71328 (add) and 0x08B712B0
//               (subtract, it flips the high word's sign); the magnitude in a1 & 0x7fffffff picks one of three branches that
//               differ in the constants and in one exponent adjustment, and 0x08B71DC8 decides early whether the small
//               constant pair at 0x08CB6818 is the answer. v0/v1 = the 64-bit result.
//   0x089E0768: draws one quad through a 2x4 matrix: the eight floats at a0+0x1E8 are scaled by the pair at a2+0x18/+0x1C
//               into the outgoing argument block, 0x089A8BF4 picks a scale factor, the source object's
//               coordinates at +0x12/+0x14 become the offset and the two rates through 0x089A8B68/0x089A8B80, flags decide
//               re-centring (0x089A8B20 + 0x089C3DA8), then the output at a1 receives the virtual call at slot 0x110, the
//               three flag bits at a0+0x1B0, the two floats at a0+0x1E0/+0x1E4 and either 0x089A9654 or 0x089ACCA0.
// The batch's other candidates are not written here: 0x08874C04 (a 0x240-byte frame with three string buffers, inline
// vtables, ll/sc reference counting and eight constructor calls) and 0x08BBFAAC (a std::vector insert whose four
// helpers take the addresses of frame flags) were not confident enough to hand in blind.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_248 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose argument registers are set like the original sets them before its `jal` (a0-a3, t0-t3 in order, so a
// callee that reads one of them sees this call's value and not a previous callee's leftover). Afterwards v0, v1 and f0 are
// the callee's, as after the `jal`.
GuestResult call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
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
// The same with the callee's float arguments ($f12-$f19).
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, float_arguments);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// The callee-saved registers the original saves in its prologue and restores on every exit (s0..s<N-1>): the gate compares
// them, so a body that borrows them puts the caller's values back.
template <unsigned N>
struct SavedRegisters {
    AllegrexContext &ctx;
    u32 saved[N];
    explicit SavedRegisters(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < N; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < N; ++i) ctx.gpr[16u + i] = saved[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;
};
// The frame pointer is callee-saved and compared as well.
struct SavedFramePointer {
    AllegrexContext &ctx;
    u32 saved;
    explicit SavedFramePointer(AllegrexContext &c) : ctx(c), saved(c.gpr[30]) {}
    ~SavedFramePointer() { ctx.gpr[30] = saved; }
    SavedFramePointer(const SavedFramePointer &) = delete;
    SavedFramePointer &operator=(const SavedFramePointer &) = delete;
};

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
s32 signed_half(u32 value) { return static_cast<s32>(static_cast<std::int16_t>(value)); }
s32 signed_half_at(Runtime &rt, u32 address) { return signed_half(half_at(rt, address)); }
// MIPS `ins rt, rs, pos, size`: `size` bits of `value` replace the field at `pos` of `target`.
u32 insert_bits(u32 target, u32 value, unsigned pos, unsigned size) {
    const u32 mask = (size >= 32u ? 0xFFFFFFFFu : ((1u << size) - 1u)) << pos;
    return (target & ~mask) | ((value << pos) & mask);
}
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }

// ---------------------------------------------------------------------------------------------------------------
// 0x08B04000 (a0 = the object): the two passes described at the top of the file.

// ---------------------------------------------------------------------------------------------------------------
// 0x08921584 (a0 = the object): the virtual queries and the two clamps described at the top of the file.
void func_08921584(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];   // s1
    SavedRegisters<3> saved(ctx);  // s0-s2
    GuestFrame frame(ctx, 0x20u);
    // 0x089260EC answers the object the eight virtual queries go to (a0+8 holds it).
    u32 object = call_set(rt, ctx, 0x089260ECu, {word(rt, self + 0x8u)}).v0;  // s0
    // Slot 0x30: two rate queries; each answer arrives in f0 and is moved into f23/f22 in the *next* call's delay slot,
    // so f23 holds the first answer and f22 the second.
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x30u), {object, 0u});
    const float answer_first = ctx.fpr[0];   // f23
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x30u), {object, 4u});
    const float answer_second = ctx.fpr[0];  // f22
    // Slot 0x1C: the four signs (1, 2, 3, 4) become +-0.5 or 0 into f21/f20, then the two flags at 0x13/0x12.
    float scale_x = float_bits(0xBF000000u);  // f21 = -0.5
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, 1u});
    if (ctx.gpr[2] == 0u) scale_x = 0.0f;
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, 2u});
    if (ctx.gpr[2] != 0u) scale_x = float_bits(0x3F000000u);
    float scale_y = float_bits(0xBF000000u);  // f20 = -0.5, set before the query below
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, 3u});
    if (ctx.gpr[2] == 0u) scale_y = 0.0f;
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, 4u});
    if (ctx.gpr[2] != 0u) scale_y = float_bits(0x3F000000u);
    const u32 flag_13 = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, 0x13u});  // s2
    const u32 flag_12 = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x1Cu), {object, 0x12u});  // v1
    if (byte_at(rt, self + 0x34u) == 0u) {
        if (flag_13 != 0u) {
            // 0x089216B4: the height at +0x30 follows the rate, clamped into [1e-6, 180]; +0x14 drifts by 0.01 * scale_y.
            const float drift = psp_mul(scale_y, float_bits(0xBC23D70Au));  // f20 * -0.01
            const float step = scale_x / float_bits(0x40400000u);            // f21 / 3
            put_float(rt, self + 0x14u, float_at(rt, self + 0x14u) + drift);
            float value = float_at(rt, self + 0x30u) + step;
            const float low = float_bits(0x358637BDu);   // 1e-6
            const float high = float_bits(0x43340000u);  // 180
            if (value < low) value = low;
            if (high < value) value = high;
            put_float(rt, self + 0x30u, value);
        } else if (flag_12 != 0u) {
            // 0x0892172C: the two rates move the position at +0x1C/+0x20.
            const float move_x = psp_mul(scale_y + answer_first, float_bits(0x3CA3D70Au));   // (f20 + f23) * 0.02
            const float move_y = psp_mul(scale_x + answer_second, float_bits(0xBCA3D70Au));  // (f21 + f22) * -0.02
            put_float(rt, self + 0x1Cu, float_at(rt, self + 0x1Cu) + move_x);
            put_float(rt, self + 0x20u, float_at(rt, self + 0x20u) + move_y);
        } else {
            // 0x08921770: the idling path: +0x2C drifts and stops at 0, then the two angles wrap into [-pi, pi].
            float speed = float_at(rt, self + 0x2Cu) + psp_mul(scale_x, float_bits(0x3DCCCCCDu));  // f21 * 0.1
            if (speed < 0.0f) speed = 0.0f;
            put_float(rt, self + 0x2Cu, speed);
            const float pi = float_bits(0x40490FDBu);
            const float two_pi = float_bits(0x40C90FDBu);
            float yaw = float_at(rt, self + 0xCu) + psp_mul(answer_second, float_bits(0x3CA3D70Au));
            if (pi < yaw) yaw = yaw - two_pi;
            else if (yaw < -pi) yaw = yaw + two_pi;
            put_float(rt, self + 0xCu, yaw);
            float pitch = float_at(rt, self + 0x10u) + psp_mul(answer_first, float_bits(0x3CA3D70Au));
            if (pi < pitch) pitch = pitch - two_pi;
            else if (pitch < -pi) pitch = pitch + two_pi;
            put_float(rt, self + 0x10u, pitch);
        }
    }
    // 0x0892189C: slot 0x28 with 0x1C toggles the flag at +0x34; with 0x1B it resets both vectors, the angles and the flag.
    if (call_indirect(rt, ctx, word(rt, word(rt, object) + 0x28u), {object, 0x1Cu}) != 0u) {
        put8(rt, self + 0x34u, byte_at(rt, self + 0x34u) ^ 1u);
    }
    if (call_indirect(rt, ctx, word(rt, word(rt, object) + 0x28u), {object, 0x1Bu}) != 0u) {
        for (u32 offset = 0xCu; offset < 0x2Cu; offset += 4u) put(rt, self + offset, 0u);
        put_float(rt, self + 0x30u, float_bits(0x42480000u));  // 50
        put_float(rt, self + 0x2Cu, float_bits(0x40A00000u));  // 5
        put8(rt, self + 0x34u, 0u);
    }
    ctx.gpr[2] = self + 0xCu;  // v0 = the object at +0xC (0x089218D8, the exit's delay slot)
}

// ---------------------------------------------------------------------------------------------------------------
// The soft-double helpers 0x08B71394 (multiply), 0x08B71328 (add) and 0x08B712B0 (subtract) take two 64-bit values in
// a0/a1 and a2/a3 and return one in v0/v1.
struct DoubleWord {
    u32 low;
    u32 high;
};
DoubleWord double_constant(Runtime &rt, u32 address) {
    return DoubleWord{word(rt, address), word(rt, address + 4u)};
}
DoubleWord double_operation(Runtime &rt, AllegrexContext &ctx, u32 function, DoubleWord left, DoubleWord right) {
    const GuestResult result =
        call_set(rt, ctx, function, {left.low, left.high, right.low, right.high});
    return DoubleWord{result.v0, result.v1};
}

// 0x08B6C158 (a0/a1 = the 64-bit value, a2/a3 = a second one): the three-range polynomial described at the top.
void func_08B6C158(Runtime &rt, AllegrexContext &ctx) {
    const DoubleWord value{ctx.gpr[4], ctx.gpr[5]};   // s4, s5
    const DoubleWord other{ctx.gpr[6], ctx.gpr[7]};   // sp+0x10, sp+0x14
    SavedRegisters<8> saved(ctx);                     // s0-s7
    GuestFrame frame(ctx, 0x50u);
    const u32 magnitude = insert_bits(value.high, 0u, 31u, 1u);   // s2: `ins s2, zero, 31, 1`
    const bool small_constant_maybe = !(0x3E3FFFFFu < magnitude);
    // 0x08B71DC8: the odd/even classification; a zero answer returns the constant pair at 0x08CB6818 unchanged.
    if (small_constant_maybe && call_set(rt, ctx, 0x08B71DC8u, {value.low, value.high}).v0 == 0u) {
        const DoubleWord answer = double_constant(rt, 0x08CB6818u);
        ctx.gpr[2] = answer.low;
        ctx.gpr[3] = answer.high;
        return;
    }
    // The eleven-step polynomial in value*value (0x08B6C1D4-0x08B6C2D4).
    const DoubleWord square = double_operation(rt, ctx, 0x08B71394u, value, value);  // s0, s1
    DoubleWord series = double_operation(rt, ctx, 0x08B71394u, square, double_constant(rt, 0x08CB6820u));
    series = double_operation(rt, ctx, 0x08B71328u, series, double_constant(rt, 0x08CB6828u));
    series = double_operation(rt, ctx, 0x08B71394u, square, series);
    series = double_operation(rt, ctx, 0x08B712B0u, series, double_constant(rt, 0x08CB6830u));
    series = double_operation(rt, ctx, 0x08B71394u, square, series);
    series = double_operation(rt, ctx, 0x08B71328u, series, double_constant(rt, 0x08CB6838u));
    series = double_operation(rt, ctx, 0x08B71394u, square, series);
    series = double_operation(rt, ctx, 0x08B712B0u, series, double_constant(rt, 0x08CB6840u));
    series = double_operation(rt, ctx, 0x08B71394u, square, series);
    series = double_operation(rt, ctx, 0x08B71328u, series, double_constant(rt, 0x08CB6848u));
    series = double_operation(rt, ctx, 0x08B71394u, square, series);  // s6, s7
    DoubleWord result;
    if (magnitude <= 0x3FD33332u) {
        // 0x08B6C3EC: the small-magnitude branch.
        const DoubleWord first = double_operation(rt, ctx, 0x08B71394u, square, double_constant(rt, 0x08CB6850u));
        const DoubleWord scaled = double_operation(rt, ctx, 0x08B71394u, square, series);
        const DoubleWord product = double_operation(rt, ctx, 0x08B71394u, value, other);
        const DoubleWord moved = double_operation(rt, ctx, 0x08B712B0u, scaled, product);
        const DoubleWord shifted = double_operation(rt, ctx, 0x08B712B0u, first, moved);
        result = double_operation(rt, ctx, 0x08B712B0u, double_constant(rt, 0x08CB6818u), shifted);
    } else {
        // 0x08B6C2F0: the other two share their tail; only the saved constant pair and the subtracted pair differ.
        DoubleWord saved_pair;   // sp+0x0 / sp+0x4
        DoubleWord stored_pair;  // s2 / s3
        if (magnitude <= 0x3FE90000u) {
            // 0x08B6C4A8: the constant's exponent drops by 0x200000 and 0x08CB6818 moves into the saved slot.
            // The pair goes through the frame: 0x08B6C4AC/(0x08B6C4B0) store it, 0x08B6C4BC/(0x08B6C4C0) load it back.
            put(rt, frame.at(0x8u), 0u);
            put(rt, frame.at(0xCu), magnitude + 0xFFE00000u);
            stored_pair = DoubleWord{word(rt, frame.at(0x8u)), word(rt, frame.at(0xCu))};
            saved_pair = double_operation(rt, ctx, 0x08B712B0u, double_constant(rt, 0x08CB6818u), stored_pair);
        } else {
            stored_pair = double_constant(rt, 0x08CB6858u);
            saved_pair = double_constant(rt, 0x08CB6860u);
        }
        const DoubleWord first = double_operation(rt, ctx, 0x08B71394u, square, double_constant(rt, 0x08CB6850u));
        const DoubleWord restored = double_operation(rt, ctx, 0x08B712B0u, first, stored_pair);
        const DoubleWord scaled = double_operation(rt, ctx, 0x08B71394u, square, series);
        const DoubleWord product = double_operation(rt, ctx, 0x08B71394u, value, other);
        const DoubleWord moved = double_operation(rt, ctx, 0x08B712B0u, scaled, product);
        const DoubleWord shifted = double_operation(rt, ctx, 0x08B712B0u, restored, moved);
        result = double_operation(rt, ctx, 0x08B712B0u, saved_pair, shifted);
    }
    ctx.gpr[2] = result.low;
    ctx.gpr[3] = result.high;
}

// ---------------------------------------------------------------------------------------------------------------
// 0x089E0768 (a0 = the source state, a1 = the output, a2 = the measured object, a3 = handed on, t0 = the virtual call's
// argument, f12/f13 = two offsets): the quad draw described at the top of the file.
void func_089E0768(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];        // s2
    const u32 output = ctx.gpr[5];      // s3
    const u32 source = ctx.gpr[6];      // s4
    const u32 argument = ctx.gpr[7];    // fp
    const u32 extra = ctx.gpr[8];       // s7
    const float offset_in_x = ctx.fpr[12];  // f30
    const float offset_in_y = ctx.fpr[13];  // f29
    SavedRegisters<8> saved(ctx);           // s0-s7
    SavedFramePointer saved_frame_pointer(ctx);
    GuestFrame frame(ctx, 0x80u);
    const float row_first = float_at(rt, source + 0x18u);   // f9
    const float row_second = float_at(rt, source + 0x1Cu);  // f8
    // 0x089E0800: the eight floats at +0x1E8 scaled by the pair, into the outgoing argument block at sp+0.
    for (u32 i = 0u; i < 4u; ++i) {
        const float a = float_at(rt, self + 0x1E8u + i * 8u);
        const float b = float_at(rt, self + 0x1ECu + i * 8u);
        put_float(rt, frame.at(i * 8u), psp_mul(row_first, a));
        put_float(rt, frame.at(i * 8u + 4u), psp_mul(row_second, b));
    }
    // 0x089A8BF4 reads a0 only and answers f0; the block above is not an argument of its.
    const float base = call_setf(rt, ctx, 0x089A8BF4u, {output}, {}).f0;  // f27
    u32 measured = word(rt, self + 0x210u);
    float range_x = 0.0f;    // f28
    float range_y = 0.0f;    // f25
    float rate_x = 1.0f;     // f26
    float rate_y = 1.0f;     // f22
    if (measured != 0u && measured != source) {
        call_setf(rt, ctx, 0x089A8C0Cu, {output}, {psp_mul(base, base)});
        measured = word(rt, self + 0x210u);  // s0
        u32 probe = measured;                // s1
        const s32 source_left = signed_half_at(rt, source + 0x12u);   // s5
        const s32 source_top = signed_half_at(rt, source + 0x14u);    // s6
        if (measured == 0u) {
            call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA779Cu});
            measured = word(rt, self + 0x210u);
        }
        range_x = psp_mul(static_cast<float>(source_left - signed_half_at(rt, probe + 0x12u)),
                          float_bits(0x3F000000u));  // * 0.5
        probe = measured;
        if (measured == 0u) {
            call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA779Cu});
            measured = word(rt, self + 0x210u);
        }
        range_y = psp_mul(static_cast<float>(source_top - signed_half_at(rt, probe + 0x14u)),
                          float_bits(0x3F000000u));  // * 0.5
        probe = measured;
        if (measured == 0u) {
            call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA779Cu});
            measured = word(rt, self + 0x210u);
        }
        const float source_rate_x = static_cast<float>(signed_half_at(rt, source + 0x12u));  // f21
        const float source_rate_y = static_cast<float>(signed_half_at(rt, source + 0x14u));  // f20
        rate_x = source_rate_x / static_cast<float>(signed_half_at(rt, probe + 0x12u));
        // 0x089E0920: this last check has no reload of the object (0x089E0934 reads whatever s0 holds).
        if (measured == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA779Cu});
        rate_y = source_rate_y / static_cast<float>(signed_half_at(rt, measured + 0x14u));
    }
    // 0x089E095C: the two scale factors, then the offsets against the caller's.
    float limit_x = call_setf(rt, ctx, 0x089A8B68u, {output}, {}).f0;  // f24
    float limit_y = call_setf(rt, ctx, 0x089A8B80u, {output}, {}).f0;  // f23
    const float small = float_bits(0x34000000u);  // 2^-23
    float delta_x = offset_in_x - range_x;        // f21
    bool restarted = false;                       // s5
    float delta_y = offset_in_y - range_y;        // f20
    if (small <= std::fabs(delta_x) || small <= std::fabs(delta_y)) {
        const u32 target = call_set(rt, ctx, 0x089A8B20u, {output}).v0;
        limit_x = psp_mul(limit_x, rate_x);  // 0x089E09BC, after that call and before 0x089C3DA8
        limit_y = psp_mul(limit_y, rate_y);
        call_setf(rt, ctx, 0x089C3DA8u, {target},
                  {delta_x, delta_y});
        restarted = true;
    }
    // 0x089E09CC: the output's setup.
    call_indirect(rt, ctx, word(rt, word(rt, output) + 0x110u), {output, extra});
    put8(rt, output + 0x118u, 1u);
    const u32 flags = word(rt, self + 0x1B0u);
    put8(rt, output + 0x150u, (flags >> 7) & 1u);
    put8(rt, output + 0x151u, (flags >> 8) & 1u);
    put8(rt, output + 0x152u, (flags >> 9) & 1u);
    call_set(rt, ctx, 0x089641C4u, {output + 0x160u, self + 0x1C0u});
    put_float(rt, output + 0x170u, float_at(rt, self + 0x1E0u));
    put(rt, output + 0x11Cu, source);
    put_float(rt, output + 0x174u, float_at(rt, self + 0x1E4u));
    if ((word(rt, self + 0x1A0u) & 0x20u) != 0u) {
        const u32 first = call_set(rt, ctx, 0x08A06FA8u, {self}).v0;  // s1
        const u32 second = call_set(rt, ctx, 0x08A06FB0u, {self}).v0; // s0
        const u32 third = call_set(rt, ctx, 0x08A06FB8u, {self}).v0;
        call_setf(rt, ctx, 0x089ACCA0u, {output, first, second, third, frame.at(0u)}, {limit_x, limit_y});
    } else {
        call_setf(rt, ctx, 0x089A9654u, {output, frame.at(0u), argument},
                  {limit_x, limit_y, float_at(rt, self + 0x11Cu)});
    }
    call_set(rt, ctx, 0x089A8C24u, {output});
    call_indirect(rt, ctx, word(rt, word(rt, output) + 0x114u), {output});
    if (restarted) {
        const u32 target = call_set(rt, ctx, 0x089A8B20u, {output}).v0;
        call_setf(rt, ctx, 0x089C3DA8u, {target}, {range_x - offset_in_x, range_y - offset_in_y});
    }
    call_setf(rt, ctx, 0x089A8C0Cu, {output}, {base});
}

} // namespace
} // namespace new_batch_main_248
} // namespace gt::game

#define NEW248(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_248::name, gt::ReturnKind::kind)
NEW248(0x08921584u, func_08921584, Word);
NEW248(0x08B6C158u, func_08B6C158, DoubleWord);
NEW248(0x089E0768u, func_089E0768, Word);

// ---- from main-session batch 249 (draft 947f9e94ef1dff7ac647c8d11d82ab4bb895155e) ----
// New batch, main session 249: hand-written structured C from `tools/disasm`, not gated here.
//   0x0888D9A8: one request entry of an object's table (see update_request_entry / finish_request below).
//   0x0897BC08: push_back(true) on the bit vector at self + 0x74, then the trailing-set-bit loop (a body of this
//               address already sits unregistered in src/game/unsorted/hot_batch_117.cpp; this one is independent).
//   0x088CA740: the basis/sampling step (see func_088CA740).
//   0x08A76330: the tokenizer (see func_08A76330).
//   0x088D351C: the ray-vs-triangle-style scan (see func_088D351C).
// The float registers the original saves around its body (f20-f24) need no model: the original restores them, and
// this file never writes them. Same for s0-s7 and fp.
// `tools.arg_reads` verdicts for the calls of this file: 0x0888A878 is listed with a2/a3, which it only ever loads
// with lwl/lwr pairs at its top (both halves written, so the incoming value never shows); 0x088DA1C8 is listed with a3,
// a loop counter it clears at 0x088DA238 before the only read (0x088DA20C). `tools.sp_loads` lists 0x088D351C's
// `lw s5, 0xA0($sp)` / `lw s7, 0xA8($sp)`: they are not saves but the range 0x088FAD1C fills in the frame, modelled here
// as the loop's begin and end.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_249 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call whose argument registers are set the way the original sets them before its `jal`: a callee that reads a
// register the call does not pass sees what an earlier, different call left there (batches 93/96).
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
             std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// The same, keeping v0 and v1 (a 64-bit answer split over the pair: the caller reads v1 too).
u32 call_pair(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
              std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the `jalr` v1 holds the vtable and v0 the target, and the callee's
// v0/v1/f0 replace them (a virtual that never writes v0 returns the target).
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments,
                 std::initializer_list<float> floats = {}) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// A guest call whose answer is the f0 it leaves (0x08B4ABEC and friends): two integer arguments, the result in f0.
float call_float(Runtime &rt, AllegrexContext &ctx, u32 target, u32 first, u32 second) {
    call_set(rt, ctx, target, {first, second});
    return ctx.fpr[0];
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 signed_byte(u32 value) { return static_cast<u32>(static_cast<std::int8_t>(static_cast<std::uint8_t>(value))); }
u32 signed_half(u32 value) { return static_cast<u32>(static_cast<std::int16_t>(static_cast<std::uint16_t>(value))); }
s32 sext8(u32 value) { return static_cast<std::int8_t>(static_cast<std::uint8_t>(value)); }

// ---------------------------------------------------------------------------------------------------------------
// 0x0888D9A8 (a0 = self, a1 = argument, a2 = request). One request entry of the object's three tables (168-byte
// entries at +0x34->+0xD78, their ids at +0x1018, 432-byte counters at +0x1560): the entry's state is copied out to
// a stack block at sp+0x10, patched from the request, run through the id lookups, and copied back. sp+0..sp+8 is a
// three-word argument block handed to 0x088D2774 / 0x088D26B4 / 0x0888B24C. `fresh` is the request's byte at +5.
void update_request_entry(Runtime &rt, AllegrexContext &ctx, u32 self, u32 argument, u32 request, u32 base) {
    const u32 buffer = base + 0x10u;
    const u32 table = word(rt, self + 0x34u);
    const u32 index = signed_half(half_at(rt, request));  // s2: `lh`
    // 0x0888D9F4: the entry's state comes into the stack block.
    call_set(rt, ctx, 0x0888A878u, {buffer, table + static_cast<u32>(static_cast<s32>(index) * 168) + 0xD78u});
    // 0x0888DA20: patch the block: a flag byte, the request's halfword, a count of one and the request count byte.
    const u32 byte4 = signed_byte(byte_at(rt, request + 4u));  // `lb`
    const s32 byte5 = sext8(byte_at(rt, request + 5u));        // `lb`
    const u32 fresh = byte5 != 0 ? 1u : 0u;                    // s4: `sltu s4, zero, v0`
    const u32 old_count = byte_at(rt, buffer + 0x13u);          // a0: `lbu`
    u32 counter_byte = (byte4 + 1u) & 0xFFu;                    // v1
    if (fresh != 0u) counter_byte = 0u;                         // `movn v1, zero, s4`
    put8(rt, buffer + 0x11u, 1u);
    put8(rt, buffer + 0x13u, counter_byte);
    put_half(rt, buffer + 0x14u, half_at(rt, request + 2u));
    put8(rt, buffer + 0x12u, fresh);
    // 0x0888DA54: a new request carries its own 64-bit pair (the halfword's sign as the high word), an old one keeps
    // the block's.
    u32 pair_low;
    u32 pair_high;
    if (fresh != 0u) {
        pair_low = word(rt, request + 8u);
        pair_high = static_cast<u32>(static_cast<s32>(pair_low) >> 31);
    } else {
        pair_low = word(rt, buffer + 0x20u);
        pair_high = word(rt, buffer + 0x24u);
    }
    put(rt, buffer + 0x20u, pair_low);
    put(rt, buffer + 0x24u, pair_high);
    // 0x0888DA90: `count` words from the request's +0x10 and `count` sign-extended halfwords from its +0x50.
    const u32 count = old_count + 1u;
    for (u32 i = 0u; i < count; ++i) {
        put(rt, buffer + 0x28u + i * 4u, word(rt, request + 0x10u + i * 4u));
        put(rt, buffer + 0x68u + i * 4u, signed_half(half_at(rt, request + 0x50u + i * 2u)));
    }
    // 0x0888DAB4: the entry's id object, then the request's word at +0x70.
    const u32 moved = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), index});  // s3
    const u32 holder = word(rt, self + 4u);
    const u32 request_word = word(rt, request + 0x70u);
    if (fresh != 0u) {
        // 0x0888DAE8: the stored pair decides whether the request is written back (the table's byte at +0x1C35
        // flips the sense of the comparison).
        const u32 entry = table + static_cast<u32>(static_cast<s32>(index) * 168) + 0x1018u;
        if (byte_at(rt, entry + 0x11u) != 0u) {
            const u32 stored_low = word(rt, entry + 0x20u);
            const u32 stored_high = word(rt, entry + 0x24u);
            bool replace;
            if (stored_low == pair_low && stored_high == pair_high) {
                replace = false;  // the same pair: nothing to write back
            } else {
                bool lower;  // a1
                if (pair_high < stored_high) lower = true;
                else if (pair_high != stored_high) lower = false;
                else lower = pair_low < stored_low;
                replace = byte_at(rt, table + 0x1C35u) != (lower ? 1u : 0u);
            }
            if (replace) {
                call_set(rt, ctx, 0x0888A878u, {entry, buffer});
                call_set(rt, ctx, 0x0888AF70u, {table, buffer});
            }
        }
        // 0x0888DB48: the argument block, then the moved entry's update (its +0x38 takes the first answer).
        put(rt, base, index);
        put(rt, base + 4u, request_word);
        put(rt, base + 8u, signed_half(half_at(rt, request + 2u)) + 1u);
        put(rt, moved + 0x38u, call_set(rt, ctx, 0x088D2774u, {word(rt, holder + 0x618u), base, holder}));
        call_set(rt, ctx, 0x088D0934u, {moved, byte4, request_word});
        call_set(rt, ctx, 0x088D097Cu, {moved, word(rt, request + 8u)});
        put(rt, moved + 0x194u, request_word);
        put(rt, moved + 0x70u, request_word);
        // 0x0888DBB4: the counter by index, wrapped to a halfword; `(wrapped - 1) % 100` picks the slot at +0x1C of
        // the 432-byte record (the remainder keeps the sign of the dividend).
        const u32 record = table + static_cast<u32>(static_cast<s32>(index) * 432) + 0x1560u;
        const u32 wrapped = signed_half(half_at(rt, record + 2u) + half_at(rt, request + 2u));
        put_half(rt, record + 4u, wrapped);
        const s32 slot = static_cast<s32>(wrapped) - 1;
        put(rt, record + 0x1Cu + static_cast<u32>(slot % 100) * 4u, pair_low);
    } else {
        // 0x0888DC1C: an old request only fills the argument block and refreshes the moved entry once.
        put(rt, base, index);
        put(rt, base + 4u, request_word);
        put(rt, base + 8u, signed_half(half_at(rt, request + 2u)));
        put(rt, moved + 0x38u, call_set(rt, ctx, 0x088D26B4u, {word(rt, holder + 0x618u), base, byte4}));
        call_set(rt, ctx, 0x088D0934u, {moved, byte4, request_word});
    }
    // 0x0888DC58: the block goes back, then the two id lookups must agree before the last step.
    call_set(rt, ctx, 0x0888A878u, {table + static_cast<u32>(static_cast<s32>(index) * 168) + 0xD78u, buffer});
    call_set(rt, ctx, 0x0888AD94u, {table, index, buffer});
    if (call_set(rt, ctx, 0x088D62FCu, {argument}) != 0u) return;
    if (call_set(rt, ctx, 0x088DA1C8u, {word(rt, self + 4u), index}) != 1u) return;
    const u32 first = call_pair(rt, ctx, 0x0888CAFCu, {self, index, fresh, 1u});
    const u32 second = ctx.gpr[3];  // v1 of that call, tested at 0x0888DD20
    put(rt, base, first);
    put(rt, base + 4u, second);
    put(rt, buffer + 0xB0u, first);
    put(rt, buffer + 0xB4u, second);
    u32 selected;
    if (fresh != 0u) {
        selected = word(rt, word(rt, holder + 0x618u) + 0x20Cu);
    } else {
        // 0x0888DCE8: the compiler reaches the slot as `base + 12 * slot - 12`, then loads at +0xC.
        const u32 slot_index = word(rt, moved + 0x2Cu);
        selected = word(rt, word(rt, holder + 0x618u) + slot_index * 12u);
    }
    if (call_set(rt, ctx, 0x08918438u, {word(rt, self + 8u), selected, 0u}) == 0u) return;
    if (second != 0u) return;
    call_set(rt, ctx, 0x0888B24Cu, {self, 0u, 0u, base});
}

// 0x0888DD40: v0 = the request's byte at +5 (sign-extended `lb`); when it is set, 0x089936DC(self->+0x18) answers and
// becomes v0 instead. Every exit of the body above lands here.
void finish_request(Runtime &rt, AllegrexContext &ctx, u32 self, u32 request) {
    const u32 flag = signed_byte(byte_at(rt, request + 5u));
    ctx.gpr[2] = flag;
    if (flag != 0u) ctx.gpr[2] = call_set(rt, ctx, 0x089936DCu, {word(rt, self + 0x18u)});
}


// ---------------------------------------------------------------------------------------------------------------
// The bit just before the end iterator {word pointer, bit offset} of a bit vector: the compiler's step back by one
// (a word lower and bit 31 when the offset was 0). shl/shr through 0x1F as the `sllv` does.
u32 bit_before_end(Runtime &rt, u32 end_pointer, u32 end_offset) {
    const s32 previous = static_cast<s32>(end_offset) - 1;
    const u32 sign = previous < 0 ? 0x1Fu : 0u;  // ((previous >> 31) u>> 27)
    s32 words = previous < 0 ? static_cast<s32>(end_offset) + 0x1E : previous;
    words >>= 5;
    s32 bit = static_cast<s32>((static_cast<u32>(previous) + sign) & 0x1Fu) - static_cast<s32>(sign);
    u32 address = end_pointer + (static_cast<u32>(words) << 2);
    if (bit < 0) {
        bit += 0x20;
        address -= 4u;
    }
    return (word(rt, address) >> (static_cast<u32>(bit) & 31u)) & 1u;
}

// 0x0897BC08 (a0 = self): push_back(true) on the bit vector at self + 0x74 {begin word pointer +0, begin offset +4,
// end word pointer +8 (self + 0x7C), end offset +0xC (self + 0x80), end-of-storage pointer +0x10}, announced through
// 0x0897BB18(self, 0x089D5B30()). With room the bit at the end iterator is set and the iterator steps; full, a block of
// 2 x size + 31 bits (63 when empty, rounded down to words; 0x08A5BC20(bytes, 4)) takes the bits one by one, the new
// bit and the tail, the old block is released (0x08A5BC54) and the members updated. Then, while the bit before the end
// is set: 0x0897B6E4(self), 0x0897B800(self), by the byte at +0x88 0x08979C60 / 0x08977318(self) and
// 0x0897BB68(self), then 0x08B1A844(). When it is clear the end iterator steps back, the announcement runs again and,
// for an empty vector, 0x08978574(self) decides on the tail jump to 0x0897851C(self). v0 = the begin word pointer.

// ---------------------------------------------------------------------------------------------------------------
// 0x088CA740 (a0 = self, a1 = four output vectors, a2 = flag byte, f12 = scale): the basis/sampling step for the
// object at self+0xC. The flag picks one of two virtual asks (slots 0x18, arguments 9 then 2, or 8 then 1) whose
// answers select `mode` (s6) and `variant` (s7); both fall back to 6/5. 0x088CA2BC samples with (mode, the 64-bit
// pair at self+0x30, two frame vectors, 0.0 and 1.0 + scale as its floats) twice, its answers carried back into
// self+0x30; 0x088C969C closes on two of the vectors. The frame gets a matrix (vec_a with w 1 in rows 0 and 1,
// vec_c with w 1 in row 2), transformed by vec_c (0x08B4A8D4), normalized, so is vec_b; their cross product fills
// the third row of the matrix the four output vectors go through (0x08B4A774). v0 = that last call's.

// ---------------------------------------------------------------------------------------------------------------
// The libstdc++ copy-on-write std::string the tokenizer builds (the 0x08A6xxxx / 0x08B86xxx library): the object is one
// word, the rep pointer; the rep keeps the length at -0xC, the capacity at -8 and the shared flag at -4, the characters
// start at the rep and the shared empty rep is 0x08D74FD0.
constexpr u32 kEmptyStringRep = 0x08D74FD0u;
// 0x08B864EC reserves; so does the release pair 0x08B60804 (count down) + 0x08B8542C (free).
constexpr u32 kStringReserve = 0x08B864ECu;
constexpr u32 kStringRelease = 0x08B8542Cu;

// `sb` of one character into the string at `slot` with the reserve step of 0x08B864EC when the capacity is short or the
// rep is shared (`word(rep - 4) > 0`), then the length and the terminator (the shape hot_batch_114's func_08A6A40C holds).
void string_append(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 value) {
    u32 rep = word(rt, slot);
    const u32 length = word(rt, rep - 0xCu);
    const u32 wanted = length + 1u;
    u32 at = length;
    if (word(rt, rep - 8u) < wanted || static_cast<s32>(word(rt, rep - 4u)) > 0) {
        call_set(rt, ctx, kStringReserve, {slot, wanted});
        rep = word(rt, slot);
        at = word(rt, rep - 0xCu);
    }
    put8(rt, rep + at, value);
    rep = word(rt, slot);
    put(rt, rep - 4u, 0u);
    put(rt, rep - 0xCu, wanted);
    put8(rt, rep + wanted, 0u);
}

// The release step: unless the string is the empty rep, the count at rep-4 goes down (0x08B60804) and a count that was
// not positive frees the rep (0x08B8542C with the frame slot as its allocator argument).
void string_release(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 frame_slot) {
    const u32 rep = word(rt, slot);
    if (rep - 0xCu == kEmptyStringRep) return;
    if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {rep - 4u, 0xFFFFFFFFu})) > 0) return;
    call_set(rt, ctx, kStringRelease, {rep - 0xCu, frame_slot});
}

bool identifier_start(s32 value) {
    return static_cast<u32>(value - 0x41) < 26u || static_cast<u32>(value - 0x61) < 26u;
}
bool identifier_char(s32 value) { return static_cast<u32>(value - 0x30) < 10u || identifier_start(value); }

// 0x08A76330 (a0 = self, a1 = pointer to the character cursor): tokenize until the NUL. An iteration clears the working
// string (the shared empty rep), skips the whitespace (0x9, 0xA, 0xB, 0xC, 0xD, 0x20), then takes one token: an
// identifier (a letter or '_', then letters and digits and '_'), a quoted string (with `\'` for a quote, the closing
// quote consumed) or `::` (pushing the separator 0x08CF6FDC when the vector is empty); any other character ends the
// run (and the cursor is not advanced: the original re-reaches the same character, as its loop does). Each token is
// built as a value object (0x08A69654 from the string) into the vector {begin, current, end} at sp+0x10 (push 0x08C25730,
// element copy 0x08A6972C when there is room) and released (0x08A69D68); the working string is released too. The tail
// hands the vector to 0x08A760B4(self, vector), destroys its elements (0x08C24F04) and frees its storage
// (0x08A5BC54) when there is one; v0 is then that free's answer or (end - begin) >> 2.

// ---------------------------------------------------------------------------------------------------------------
// 0x088D351C (a0 = the output record, a1 = a pointer whose word is the object looked up through 0x088DB5C4, a2/a3 = two
// 3-float vectors): a segment-segment intersection in the XZ plane (0x08B4ABEC is a 2D cross product of the +0 and +4
// lanes). The record is cleared (its +0x18 field is read as an object later) and the two lookups plus 0x088FAB84 / the
// range from 0x088FAD1C gate the walk over the 16-byte elements of that range: each element (its partner 0x10 bytes on,
// or at +0x10 + 16*the signed halfword at +0xC when that is negative) is tested against the a2->a3 segment; when the
// parameters land in (0, 1) the record gets the intersection point (+0..+8 summed onto +0x18), its length at +0x24, the
// parameter at +0x28, the flag +0x2C and a perpendicular direction at +0xC and +0x18. v0 = the record.

} // namespace
} // namespace new_batch_main_249
} // namespace gt::game

#define NEW249(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_249::name, gt::ReturnKind::kind)
// ---- from main-session batch 250 (draft 8f53ffc1804f4e30aafa98aec43db647bd8dfa8d) ----
// New batch main 250 (draft, unverified): four of the six functions listed for this batch, hand-written as structured C
// from `tools.disasm`. They were never verified because the gate could not give them useful inputs; the input shapes
// derived for the synthetic generator are in /tmp/claude-1000/shapes_main250.txt (one `reg:` line per address, the
// format host/synth_case.cpp parses). The structures in the asm:
//  - 0x08B2D824: a worker entry (started by 0x08C7AA18 from 0x08B2DACC); a1's first word is the only input it reads
//    before it writes the global 0x08D68CF8 and the buffers 0x08D68D50 / 0x08D68D54.
//  - 0x08C5BD6C: two scan positions {stream object, char} (a1, a2) and a locale facet table (a3 + 0x6C); returns the
//    {stream, char} pair in v0 / v1.
//  - 0x089B7574: object a0 (members up to +0x174) and a text token a1 (old draft, corrected: the 0x41A0 constant is
//    20.0, not 2.0, and the frame +0x18 word is read in the jal's delay slot, before the call).
//  - 0x088D390C: point holder a0, three-float outputs a1 / a2 (old draft, corrected: 0x40A0 is 5.0, not 30.0).
// Not drafted, and why: 0x08C677D4 (an 880-byte 64-bit integer formatter with a dynamically grown stack and a local
// locale-cache creation path) and 0x088F82D0 (a VFPU-heavy float pass: vsub/vdot/vexp2/vscl on columns C000-C030,
// whose exact register side effects the differential harness compares) are both too easy to get subtly wrong by hand;
// they need their own reading pass, not a draft.
// Note: every jal delay slot runs before its call, so a value moved there is the *previous* call's result, and v0 is
// left at every exit with what the asm leaves there.
// Audits (before the gate ran): tools.frame_audit 0 wrong, tools.literal_check 0 missing, tools.arg_reads 0. tools.sp_loads
// lists five frame words read into callee-saved registers that the prologue never saved (089B7574 sp+8 = the token,
// 08B2D824 sp+0 = the stream buffer pointer); both are modelled as word(rt, frame.at(off)).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_250 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

constexpr u32 kEnd = 0xFFFFFFFFu;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order):
// what the callee does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// A `jalr $v0` through a vtable slot: the callee starts with v0 = its own address, as after the original's
// `lw v0, slot(vtable)`.
u32 call_at(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    ctx.gpr[2] = target;
    return call(rt, ctx, target, arguments);
}

// The same with float arguments in $f12-$f15; v0 afterwards is the callee's, f0 its float result.
u32 callf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
          std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// Where the prologue of a function stores a callee-saved register: `reg` 0-31 is a general register, 32-63 a floating
// one (32 + its number).
struct SavedSlot {
    unsigned reg;
    u32 offset;
};

// The callee-saved registers (s0-s7, fp, f20-f31) a function keeps values in, and the stores of its prologue: the
// rewrite stores them where the asm does (a callee can read the stack below its caller's frame), sets the registers
// as the asm does because callees can read them too, and gives the caller's values back when it returns.
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

u32 sext8(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value))); }
s32 smin(s32 a, s32 b) { return a < b ? a : b; }

// A little-endian word at an address of any alignment (lwl + lwr).
u32 unaligned_word(Runtime &rt, u32 address) {
    return rt.memory().load8(address) | rt.memory().load8(address + 1u) << 8u |
           rt.memory().load8(address + 2u) << 16u | static_cast<u32>(rt.memory().load8(address + 3u)) << 24u;
}
// The same for a store (swl + swr at +3 / +0, the little-endian pair).
void put_unaligned_word(Runtime &rt, u32 address, u32 value) {
    for (unsigned i = 0; i < 4u; ++i) rt.memory().store8(address + i, static_cast<std::uint8_t>(value >> (8u * i)));
}

// 0x08B2D824 (a0 unused, a1 = an argument block whose first word it reads): a worker entry. It opens 0x08CB5454 (the
// first result must not be negative), prepares a stream from it (0x08C7AA40 / 0x08C7A820 / 0x08C7A870) and writes a
// header: the word of 0x08D68D50 at the stream buffer, four bytes, then four bytes of zeros; the unaligned word the
// header covers is checked. It then copies the block at 0x08D68D54 (0x08D68D50 bytes, 0x10000 at a time, 08C7A818
// per piece, 08B2D2CC between the last piece and the block) and reads back in 0x10000-byte chunks, comparing each with
// 08B36170 against the block and counting the matches, until a chunk is shorter than 0x10000. Finally the count is
// written (twice), the stream is closed and the thread state 0x08D68CF8 goes 5 -> 1; v0 = 0x08C7AA68(0)'s result.
// Error paths close the handle, set the state (2, 3, 4, 6 or 7) and keep going (or answer 0 for the first two).

// 0x08C5BD6C (a0 unused, a1 / a2 = the two scan positions {stream object, current char}, a3 = a facet whose +0x6C
// gives the character-class table, t0 = a pointer to the iostate word, t1 = a record whose +0x14 takes the value);
// v0 / v1 = the answer pair {stream object, current char}. A scan position's object is null once its stream is
// exhausted; its +8 / +0xC words are a pointer into a buffer and its end (a byte is fetched there), otherwise the
// vtable's +0x24 answers the next char (the +0x28 slot advances). The digit count decides when the accumulated value is
// committed (2: as is, 4: minus 1900), otherwise the iostate gets bit 4; bit 2 records whether the two positions agree
// on still having a char.
enum class Phase { Loop, Dispatch, Main, After, Classify, Digit };

void func_08C5BD6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 facet = ctx.gpr[7];
    const u32 state_word = ctx.gpr[8];
    const u32 record = ctx.gpr[9];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x30u}, {17u, 0x34u}, {18u, 0x38u}, {19u, 0x3Cu}, {20u, 0x40u},
                                           {21u, 0x44u}, {22u, 0x48u}, {23u, 0x4Cu}, {30u, 0x50u}, {31u, 0x54u}});
    // The two scan positions are read before the facet call (its delay slot only stores t1).
    u32 a_object = word(rt, ctx.gpr[5]);
    u32 a_char = word(rt, ctx.gpr[5] + 4u);
    u32 b_object = word(rt, ctx.gpr[6]);
    u32 b_char = word(rt, ctx.gpr[6] + 4u);
    const u32 classes = call(rt, ctx, 0x08B64C80u, {facet + 0x6Cu});
    u32 a_pending = 0u;
    u32 b_pending = 0u;
    u32 digits = 0u;
    u32 value = 0u;
    u32 class_entry = 0u;
    put(rt, frame.at(0x10u), value);
    // The pending char an "A" step holds when its stream could not give one; 0xFFFFFFFF marks "fetch again".
    auto peek_a = [&]() -> u32 {
        const u32 at = word(rt, a_object + 8u);
        if (at < word(rt, a_object + 0xCu)) return rt.memory().load8(at);
        return call_at(rt, ctx, word(rt, word(rt, a_object) + 0x24u), {a_object});
    };
    auto peek_b = [&]() -> u32 {
        const u32 at = word(rt, b_object + 8u);
        if (at < word(rt, b_object + 0xCu)) return rt.memory().load8(at);
        return call_at(rt, ctx, word(rt, word(rt, b_object) + 0x24u), {b_object});
    };
    auto advance_a = [&]() {
        const u32 at = word(rt, a_object + 8u);
        if (at < word(rt, a_object + 0xCu)) put(rt, a_object + 8u, at + 1u);
        else call_at(rt, ctx, word(rt, word(rt, a_object) + 0x28u), {a_object});
    };
    Phase phase = Phase::Loop;
    for (;;) {
        switch (phase) {
        case Phase::Loop:
            // Resolve A's char (the object can already be exhausted, which only leaves -1 behind).
            if (a_object == 0u) {
                a_pending = a_char;
                a_char = kEnd;
            } else if (a_char == kEnd) {
                a_char = peek_a();
                if (a_char != kEnd) a_pending = a_char;
                else {
                    a_pending = kEnd;
                    a_object = 0u;
                }
            } else {
                a_pending = a_char;
            }
            // Resolve B's char the same way.
            if (b_object == 0u) {
                b_pending = b_char;
                b_char = kEnd;
            } else if (b_char == kEnd) {
                b_char = peek_b();
                if (b_char != kEnd) b_pending = b_char;
                else {
                    b_pending = kEnd;
                    b_object = 0u;
                }
            } else {
                b_pending = b_char;
            }
            phase = Phase::Dispatch;
            continue;
        case Phase::Dispatch: {
            const bool a_end = a_char == kEnd;
            const bool b_end = b_char == kEnd;
            if (a_end != b_end && digits != 4u) {
                phase = Phase::Classify;
                continue;
            }
            phase = Phase::Main;
            continue;
        }
        case Phase::Main:
            a_char = a_pending;
            if (digits == 2u) {
                put(rt, record + 0x14u, value);
            } else if (digits == 4u) {
                value -= 0x76Cu;
                put(rt, frame.at(0x10u), value);
                put(rt, record + 0x14u, value);
            } else {
                put(rt, state_word, word(rt, state_word) | 4u);
            }
            phase = Phase::After;
            continue;
        case Phase::After: {
            u32 a_item;
            if (a_object == 0u) {
                a_item = kEnd;
            } else if (a_pending == kEnd) {
                a_char = peek_a();
                a_item = a_char;
                if (a_char == kEnd) a_object = 0u;
            } else {
                a_item = a_pending;
            }
            u32 b_item;
            if (b_object == 0u) {
                b_item = kEnd;
            } else if (b_pending == kEnd) {
                const u32 at = word(rt, b_object + 8u);
                if (at < word(rt, b_object + 0xCu)) {
                    b_item = rt.memory().load8(at);
                    b_pending = b_item;
                } else {
                    // The vtable answer is the new B char and goes back through the dispatch (not the compare).
                    b_char = peek_b();
                    if (b_char != kEnd) b_pending = b_char;
                    else {
                        b_pending = kEnd;
                        b_object = 0u;
                    }
                    phase = Phase::Dispatch;
                    continue;
                }
            } else {
                b_item = b_pending;
            }
            if ((a_item != kEnd) == (b_item != kEnd)) put(rt, state_word, word(rt, state_word) | 2u);
            ctx.gpr[2] = a_object;
            ctx.gpr[3] = a_char;
            return;
        }
        case Phase::Classify: {
            u32 class_char;
            if (a_object == 0u) {
                class_char = kEnd;
            } else if (a_pending == kEnd) {
                // The same peek, but it only refreshes the pending char.
                const u32 at = word(rt, a_object + 8u);
                if (at < word(rt, a_object + 0xCu)) a_pending = rt.memory().load8(at);
                else a_pending = peek_a();
                if (a_pending == kEnd) a_object = 0u;
                class_char = sext8(a_pending);
            } else {
                class_char = sext8(a_pending);
            }
            const u32 index = class_char & 0xFFu;
            u32 entry = sext8(rt.memory().load8(classes + 0x11Du + index));
            if (entry == 0u) {
                const u32 answer = call_at(rt, ctx, word(rt, word(rt, classes) + 0x20u), {classes, class_char, 0x2Au});
                if (answer != 0x2Au)
                    rt.memory().store8(classes + 0x11Du + index, static_cast<std::uint8_t>(answer));
                entry = sext8(answer);
            }
            class_entry = entry;
            if (((entry - 0x30u) & 0xFFu) >= 10u) {
                phase = Phase::Main;
                continue;
            }
            phase = Phase::Digit;
            continue;
        }
        case Phase::Digit:
            value = value * 10u + class_entry - 0x30u;
            put(rt, frame.at(0x10u), value);
            if (a_object != 0u) {
                advance_a();
                a_pending = kEnd;
            }
            ++digits;
            phase = Phase::Loop;
            continue;
        }
    }
}

// 0x089B7574 (a0 = the object, a1 = a text token): 0x08A36C3C builds a temporary token at frame +8 (an assert of
// 0x08CA5910 when it is empty), 0x08A34DA8 stores it under 0x08CA5918 and the token is released. With bit 2 of the
// object's +0x60 and the holder at +0x12C: three 0x08A76AA0 / 0x08A76AAC copies at frame +0xC / +0x10 / +4, the +0x10
// one assigned from the +4 copy and both released, then the holder's vtable +0xCC virtual with (holder, frame +0xC, 1,
// frame +0x10) and both released. 0x0896A35C feeds frame +0x18, 0x089B7378 the object's +0x154, 0x0896A9C4 (+0x124,
// then +0x120) the frame pair at +0x14 / +0x18 with 0x0896A4E0 between them. 0x089B5E10's answer, when non-null, is
// turned by 0x089BD4DC / 0x08969B98 into the frame quad +0x24..+0x30: the object's +0x164 / +0x160 take (y + w) - w/20
// and w/20 + y (the 0x41A0 constant is 20.0), 0x0896A35C / 0x0896A9C4 (of the negated z) then 0x08969B98 (of +0x120).
// The advance (w - w/10) compared with +0x154 either latches the state at +0x168..+0x174 and byte +0x159 = 1 or clamps
// the frame +0x18 and runs 0x08969D2C. With +0x128 set, 0x0896A35C and 0x08B70F28 / 0x08B71328 (2.5) / 0x08B712B0
// (2.9375) / 0x08B71EC8 (integer float words) give a value that 0x0896A9C4 takes with frame +0x34..+0x40. Finally byte
// +0x158 = 1, +0x15C = +0x138, 0x08967538 (+0x120, 0), 0x089AB114, 0x089C4558(1) and 0x089B601C(object, a1).

// 0x088D390C (a0 = point holder p, a1 = self, a2 = out, a3 = mode, f12 = a, f13 = b): self's and out's three words
// are cleared; when 0x088DB5C4(p->+0) fails the function ends there. Two 0x088D2BC8(p) reads at (a + 2) + b and a - 2,
// and two 0x088D32E0(p, mode) vectors at frame +0x54 and +0x48: the first is the axis (kept at +0x90 / +0x88), the
// second is copied to +0x3C / +0x9C, and 0x08B4AA68 gives their cross product at +0x3C. Its length (0x08B4A720, sqrt,
// 0x08B4A5E8) normalises it at +0x84: 0x08B66E88 of -y and 0x08B66FBC of (x, z) give out +0 and out +4.
// 0x08B4A9D0(+0x60, +0x6C = (0, 1, 0), +0x84) normalised to +0x24 / +0x6C, 0x08B4AA68(+0x30 = +0x90, +0x24) with
// +5.0 on y (0x40A0) is +0xA8, 0x08B4A5E8(+0x0C) and 0x08B4AA34(+0x18 = +0x90, +0x0C) with the same +5.0 on y is +0xB4.
// 0x088FAB84(0x088DB5C4(p->+0), +0xC0, +0xA8) then (+0xB4) fill +0xC0, copied to +0x00 / +0xB4; 0x08B4AA68(+0x00,
// +0xA8) normalised to +0x6C and 0x08B4A9D0(+0x60, +0x84, +0x6C); 0x08B66FBC of (x, z) of +0x60 gives out +8, and self
// takes +0x90's three floats. The frame pair at +0xF0 / +0xF4 (0, -1) is written as the original does.
void func_088D390C(Runtime &rt, AllegrexContext &ctx) {
    const u32 p = ctx.gpr[4];
    const u32 self = ctx.gpr[5];
    const u32 out = ctx.gpr[6];
    const u32 mode = ctx.gpr[7];
    const float a = ctx.fpr[12];
    const float b = ctx.fpr[13];
    GuestFrame frame(ctx, 0x140u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x120u}, {23u, 0x11Cu}, {22u, 0x118u}, {20u, 0x110u}, {31u, 0x124u},
                                           {21u, 0x114u}, {19u, 0x10Cu}, {18u, 0x108u}, {17u, 0x104u}, {16u, 0x100u},
                                           {55u, 0x13Cu}, {54u, 0x138u}, {53u, 0x134u}, {52u, 0x130u}});
    const u32 v00 = frame.at(0u), v0c = frame.at(0x0Cu), v18 = frame.at(0x18u), v24 = frame.at(0x24u);
    const u32 v30 = frame.at(0x30u), v3c = frame.at(0x3Cu), v48 = frame.at(0x48u), v54 = frame.at(0x54u);
    const u32 v60 = frame.at(0x60u), v6c = frame.at(0x6Cu), v84 = frame.at(0x84u), v90 = frame.at(0x90u);
    const u32 v9c = frame.at(0x9Cu), va8 = frame.at(0xA8u), vb4 = frame.at(0xB4u), vc0 = frame.at(0xC0u);
    ctx.gpr[30] = out;
    ctx.gpr[23] = p;
    ctx.gpr[22] = self;
    ctx.gpr[20] = mode;
    put(rt, self, 0u);
    put(rt, self + 4u, 0u);
    put(rt, self + 8u, 0u);
    put(rt, out, 0u);
    put(rt, out + 4u, 0u);
    put(rt, out + 8u, 0u);
    if (call(rt, ctx, 0x088DB5C4u, {word(rt, p)}) == 0u) return;
    callf(rt, ctx, 0x088D2BC8u, {p}, {(a + 2.0f) + b});
    const float first = ctx.fpr[0];
    callf(rt, ctx, 0x088D2BC8u, {p}, {a - 2.0f});
    const float second = ctx.fpr[0];
    callf(rt, ctx, 0x088D32E0u, {v54, p, mode}, {first});
    put_float(rt, v90, float_at(rt, v54));
    put_float(rt, v90 + 4u, float_at(rt, v54 + 4u));
    put_float(rt, v90 + 8u, float_at(rt, v54 + 8u));
    callf(rt, ctx, 0x088D32E0u, {v48, p, mode}, {second});
    ctx.gpr[20] = va8;
    const float x2 = float_at(rt, v48), y2 = float_at(rt, v48 + 4u), z2 = float_at(rt, v48 + 8u);
    put_float(rt, v3c, x2);
    put_float(rt, v3c + 4u, y2);
    put_float(rt, v3c + 8u, z2);
    put_float(rt, v9c, x2);
    put_float(rt, v9c + 4u, y2);
    put_float(rt, v9c + 8u, z2);
    call(rt, ctx, 0x08B4AA68u, {v3c, v3c, v90});
    put_float(rt, v84, float_at(rt, v3c));
    put_float(rt, v84 + 4u, float_at(rt, v3c + 4u));
    put_float(rt, v84 + 8u, float_at(rt, v3c + 8u));
    call(rt, ctx, 0x08B4A720u, {v84});
    const float cross_length = std::sqrt(ctx.fpr[0]);
    callf(rt, ctx, 0x08B4A5E8u, {v84}, {1.0f / cross_length});
    callf(rt, ctx, 0x08B66E88u, {v84}, {-float_at(rt, v84 + 4u)});
    put_float(rt, out, ctx.fpr[0]);
    callf(rt, ctx, 0x08B66FBCu, {v84}, {float_at(rt, v84), float_at(rt, v84 + 8u)});
    put_float(rt, out + 4u, ctx.fpr[0]);
    put(rt, v6c, 0u);
    put_float(rt, v6c + 4u, 1.0f);
    put(rt, v6c + 8u, 0u);
    call(rt, ctx, 0x08B4A9D0u, {v60, v6c, v84});
    const float t0 = float_at(rt, v60), t1 = float_at(rt, v60 + 4u), t2 = float_at(rt, v60 + 8u);
    put_float(rt, v24, t0);
    put_float(rt, v24 + 4u, t1);
    put_float(rt, v24 + 8u, t2);
    put_float(rt, v6c, t0);
    put_float(rt, v6c + 4u, t1);
    put_float(rt, v6c + 8u, t2);
    callf(rt, ctx, 0x08B4A5E8u, {v24}, {1.0f});
    put_float(rt, v30, float_at(rt, v90));
    put_float(rt, v30 + 4u, float_at(rt, v90 + 4u));
    put_float(rt, v30 + 8u, float_at(rt, v90 + 8u));
    call(rt, ctx, 0x08B4AA68u, {v30, v30, v24});
    const float c0 = float_at(rt, v30), c1 = float_at(rt, v30 + 4u), c2 = float_at(rt, v30 + 8u);
    put_float(rt, va8, c0);
    put_float(rt, va8 + 4u, c1 + 5.0f);
    put_float(rt, va8 + 8u, c2);
    put_float(rt, v0c, float_at(rt, v6c));
    put_float(rt, v0c + 4u, float_at(rt, v6c + 4u));
    put_float(rt, v0c + 8u, float_at(rt, v6c + 8u));
    callf(rt, ctx, 0x08B4A5E8u, {v0c}, {1.0f});
    put_float(rt, v18, float_at(rt, v90));
    put_float(rt, v18 + 4u, float_at(rt, v90 + 4u));
    put_float(rt, v18 + 8u, float_at(rt, v90 + 8u));
    call(rt, ctx, 0x08B4AA34u, {v18, v18, v0c});
    const float d0 = float_at(rt, v18), d1 = float_at(rt, v18 + 4u), d2 = float_at(rt, v18 + 8u);
    put_float(rt, vb4, d0);
    put_float(rt, vb4 + 4u, d1 + 5.0f);
    put_float(rt, vb4 + 8u, d2);
    put(rt, frame.at(0xF0u), 0u);
    put(rt, frame.at(0xF4u), 0xFFFFFFFFu);
    const u32 first_handle = call(rt, ctx, 0x088DB5C4u, {word(rt, p)});
    call(rt, ctx, 0x088FAB84u, {first_handle, vc0, va8});
    put_float(rt, va8, float_at(rt, vc0));
    put_float(rt, va8 + 4u, float_at(rt, vc0 + 4u));
    put_float(rt, va8 + 8u, float_at(rt, vc0 + 8u));
    const u32 second_handle = call(rt, ctx, 0x088DB5C4u, {word(rt, p)});
    call(rt, ctx, 0x088FAB84u, {second_handle, vc0, vb4});
    const float e0 = float_at(rt, vc0), e1 = float_at(rt, vc0 + 4u), e2 = float_at(rt, vc0 + 8u);
    put_float(rt, v00 + 8u, e2);
    put_float(rt, vb4 + 8u, e2);
    put_float(rt, v00, e0);
    put_float(rt, v00 + 4u, e1);
    put_float(rt, vb4, e0);
    put_float(rt, vb4 + 4u, e1);
    call(rt, ctx, 0x08B4AA68u, {v00, v00, va8});
    put_float(rt, v6c, float_at(rt, v00));
    put_float(rt, v6c + 4u, float_at(rt, v00 + 4u));
    put_float(rt, v6c + 8u, float_at(rt, v00 + 8u));
    call(rt, ctx, 0x08B4A720u, {v6c});
    const float length = std::sqrt(ctx.fpr[0]);
    callf(rt, ctx, 0x08B4A5E8u, {v6c}, {1.0f / length});
    call(rt, ctx, 0x08B4A9D0u, {v60, v84, v6c});
    const float gx = float_at(rt, v60), gy = float_at(rt, v60 + 4u), gz = float_at(rt, v60 + 8u);
    put_float(rt, frame.at(0x7Cu), gz);
    put_float(rt, frame.at(0x80u), gy);
    put_float(rt, frame.at(0x78u), gx);  // the delay slot of 0x08B66FBC
    callf(rt, ctx, 0x08B66FBCu, {v60}, {std::sqrt(psp_mul(gx, gx) + psp_mul(gy, gy)), gz});
    put_float(rt, out + 8u, ctx.fpr[0]);
    put_float(rt, self + 8u, float_at(rt, v90 + 8u));
    put_float(rt, self, float_at(rt, v90));
    put_float(rt, self + 4u, float_at(rt, v90 + 4u));
}
} // namespace
} // namespace new_batch_main_250
} // namespace gt::game

#define NEWMAIN250(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_250::name, gt::ReturnKind::kind)
NEWMAIN250(0x088D390Cu, func_088D390C, Word);
NEWMAIN250(0x08C5BD6Cu, func_08C5BD6C, DoubleWord);

// ---- from main-session batch 251 (draft a57a09c5a534f6ef10a2c30ea5e8410c4b623555) ----
// New batch 251 (main session): long functions (254-270 instructions) written by hand from `tools.disasm` as structured C.
// Two of them resume an old draft (its commit is named above the function) and were re-read against the words; three are new.
// Every `jal`'s delay slot runs before the call, so a value moved there is the *previous* call's result, and v0 is left at
// each exit with what the asm leaves there (a callee's result or the constant a compare built).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_251 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the `jal` (a0-a3, t0-t3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// The same with float arguments in $f12-$f15.
u32 call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
              std::initializer_list<float> floats) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// A `jalr $v0` through a vtable slot: the callee starts with v0 = its own address, as after the original's `lw v0, slot(vtable)`.
u32 call_at(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    ctx.gpr[2] = target;
    return call_set(rt, ctx, target, arguments);
}
u32 call_atf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
             std::initializer_list<float> floats) {
    ctx.gpr[2] = target;
    return call_setf(rt, ctx, target, arguments, floats);
}

// Where the prologue of a function stores a callee-saved register: the rewrite stores them where the asm does (a callee can read
// the stack below its caller's frame), sets the registers as the asm does because callees can read them too, and gives the
// caller's values back when it returns.
struct SavedSlot {
    unsigned reg;
    u32 offset;
};
class SavedRegisters {
public:
    SavedRegisters(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots) : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
        fp_ = ctx.gpr[30];
        for (const SavedSlot &slot : slots) put(rt, frame.at(slot.offset), ctx.gpr[slot.reg]);
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
        ctx_.gpr[30] = fp_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
};

// `ins rt, rs, position, size`: rt's `size` bits from `position` replaced by rs's low bits.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position, unsigned size) {
    const std::uint32_t mask = (size >= 32u ? 0xFFFFFFFFu : (1u << size) - 1u) << position;
    return (value & ~mask) | ((source << position) & mask);
}
// The four-character tags the original builds from a `lui` / `ori` pair, big-endian in the word ('DCOP' -> 0x44434F50).
constexpr std::uint32_t tag_of(char a, char b, char c, char d) {
    return (static_cast<std::uint32_t>(a) << 24) | (static_cast<std::uint32_t>(b) << 16) |
           (static_cast<std::uint32_t>(c) << 8) | static_cast<std::uint32_t>(d);
}

// The counted blocks of these functions (PSP libstdc++'s shared_ptr control block): a strong count at +4, a weak count at +8,
// and the vtable's +8 / +0xC slots (dispose, destroy). The original bumps and drops a count with a `ll` / `sc` retry loop; the
// harness' runs are single-threaded, so a plain read / write leaves the same word.
void retain(Runtime &rt, u32 object) { put(rt, object + 4u, word(rt, object + 4u) + 1u); }
// A bare `ll` / `sc` bump of the word at `address`.
void count_up(Runtime &rt, u32 address) { put(rt, address, word(rt, address) + 1u); }
// Drops a count and returns the value it had: 1 means it is now zero.
u32 drop_strong(Runtime &rt, u32 object) {
    const u32 before = word(rt, object + 4u);
    put(rt, object + 4u, before - 1u);
    return before;
}
u32 drop_weak(Runtime &rt, u32 object) {
    const u32 before = word(rt, object + 8u);
    put(rt, object + 8u, before - 1u);
    return before;
}
// `if (--*strong == 0) { vtable[8](this); if (--*weak == 0) vtable[0xC](this); }`, the vtable reloaded for each call as the asm does.
void release(Runtime &rt, AllegrexContext &ctx, u32 object) {
    if (object == 0u) return;
    if (drop_strong(rt, object) != 1u) {
        ctx.gpr[2] = 1u;  // the `addiu v0, zero, 1` the asm leaves next to the count test
        return;
    }
    call_at(rt, ctx, word(rt, word(rt, object) + 8u), {object});
    if (drop_weak(rt, object) != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    call_at(rt, ctx, word(rt, word(rt, object) + 0xCu), {object});
}

// --- 0x08BD837C (old draft: commit 37204d10, src/game/unsorted/new_batch_main_36.cpp, re-read against the words) ---
// (a0 = out, a1 = a {object, counted block} pair, a2 = an argument, a3 / t0 = two more): out's two words are cleared, and so
// they stay when the pair's object is null or 0x08BD5D54(object, a2) answers object + 0x98. Otherwise, under object +0x6C's lock
// (0x08B34888 / 0x08B34B24), 0x08B2CB18(object + 8, 0x10, t0, a3, t0) gives a handle h: a null answer releases h through
// 0x08BBFF7C and 0x08BBCFA0. With h, its four words are cleared, the pair's block's strong count rises (three bumps) and falls
// (three drops) around a 0x18-byte control block {vtable 0x08CC55E0, strong 1, weak 1, h, object, block} (0x08B62344); when that
// allocation failed, 0x08909D98(h), 0x08BBD3B4(object, h) and 0x08BBEA18(object + 0x98, block) run. 0x08B8E380(frame + 4, h, h)
// spills its argument registers into the caller's frame, the handle's +0xC takes h, the control block's weak count rises and the
// handle's +0x10 slot drops its previous block's weak count (its vtable +0xC when that reaches zero) before taking the new one.
// out then takes h and the block (strong count up), the temporary control block is released and 0x08BBCFA0(pair) ends it. v0 = out.

// --- 0x0886ECD4 (old draft: commit 0f7ab10f, src/game/unsorted/new_batch_main_38.cpp, re-read against the words) ---
// A command dispatcher: a0 is the scene object, a1 an object whose first word leads to a callback, a2 the command text. When
// 0x0886B1CC(word(a0 + 0x1C)) answers non-zero the function ends with that answer. Otherwise 0x0886A9A0(a0, 2) and (a0, 5) give
// two parts and 0x08AC2690(a2) the command's four-character tag, dispatched below. v0 is the guard's result, the tag constant a
// failing comparison left, or the last handler's result.
// DCOP / DCOT / CMOF: token 0 into one stack buffer, its payload into [sp] through 0x08AC2910, token 1 into a second buffer, its
// payload into [sp + 0x3C] through 0x08AC2A8C, then the scene's +0x70 object, the payload byte and the second payload to `target`.
void dispatch_token_pair(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self, u32 command, u32 first_offset,
                         u32 second_offset, u32 target) {
    const u32 first = frame.at(first_offset);
    call_set(rt, ctx, 0x08AC2730u, {first, command, 0u});
    call_set(rt, ctx, 0x08AC2910u, {first, frame.at(0u), 0u});
    call_set(rt, ctx, 0x08AC26DCu, {first});
    const u32 second = frame.at(second_offset);
    call_set(rt, ctx, 0x08AC2730u, {second, command, 1u});
    call_set(rt, ctx, 0x08AC2A8Cu, {second, frame.at(0x3Cu), 0u});
    call_set(rt, ctx, 0x08AC26DCu, {second});
    call_set(rt, ctx, target, {word(rt, self + 0x70u), rt.memory().load8(frame.at(0u)), word(rt, frame.at(0x3Cu))});
}
// POSX / POSY / POSZ: the same two-token read into two stack slots; the second token's payload pointer (v0 of 0x08AC2900) goes
// to 0x0886CBE8 with word(a0 + 0x1C) and the first payload's word at +0x40.
void dispatch_position(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self, u32 command, u32 first_offset,
                       u32 second_offset) {
    const u32 first = frame.at(first_offset);
    call_set(rt, ctx, 0x08AC2730u, {first, command, 0u});
    call_set(rt, ctx, 0x08AC28E8u, {first});  // a float the original drops
    const u32 second = frame.at(second_offset);
    call_set(rt, ctx, 0x08AC26DCu, {first});
    call_set(rt, ctx, 0x08AC2730u, {second, command, 1u});
    const u32 payload = call_set(rt, ctx, 0x08AC2900u, {second});
    call_set(rt, ctx, 0x08AC26DCu, {second});
    call_set(rt, ctx, 0x0886CBE8u, {word(rt, self + 0x1Cu), frame.at(0x40u), payload});
}
// ANIM: with the second part present, the callback at word(word(a1) + 0xC) runs with the command's token length; then the
// scene's byte +0x7C is cleared, a bitfield in the stack word at +0x40 is set to 2 and the pair goes on to 0x0886F854 and
// 0x0886F894. v0 is the tag when the second part is null, else 0x0886F894's result.
void dispatch_anim(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, u32 self, u32 arg, u32 command, u32 part2,
                   u32 part5) {
    if (part5 == 0u) {
        ctx.gpr[2] = tag_of('A', 'N', 'I', 'M');
        return;
    }
    if (part2 != 0u) {
        const u32 callback = word(rt, word(rt, arg) + 0xCu);
        const u32 length = call_set(rt, ctx, 0x08AC26BCu, {command});
        call_atf(rt, ctx, callback, {arg, length}, {});
        const u32 scene = word(rt, self + 0x70u);
        ctx.fpr[12] = float_at(rt, scene + 0x80u);
        ctx.fpr[13] = ctx.fpr[0];
        ctx.fpr[14] = float_at(rt, scene + 0x84u);
        call_setf(rt, ctx, 0x088725E4u, {part2, rt.memory().load8(scene + 0x7Cu)},
                  {ctx.fpr[12], ctx.fpr[13], ctx.fpr[14]});
    }
    const u32 scene = word(rt, self + 0x70u);
    rt.memory().store8(scene + 0x7Cu, 0u);
    const u32 flags = word(rt, frame.at(0x40u));
    put(rt, frame.at(0x40u), ins(flags, 2u, 5u, 3u));
    call_set(rt, ctx, 0x0886F854u, {frame.at(0x40u), arg, command});
    call_set(rt, ctx, 0x0886F894u, {part5, frame.at(0x40u)});
}
void func_0886ECD4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 arg = ctx.gpr[5];
    const u32 command = ctx.gpr[6];
    GuestFrame frame(ctx, 0xA0u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x8Cu}, {18u, 0x88u}, {17u, 0x84u}, {31u, 0x98u}, {21u, 0x94u},
                                           {20u, 0x90u}, {16u, 0x80u}});
    ctx.gpr[19] = self;
    ctx.gpr[18] = command;
    ctx.gpr[17] = arg;
    if (call_set(rt, ctx, 0x0886B1CCu, {word(rt, self + 0x1Cu)}) != 0u) return;  // v0 = the guard's result
    const u32 part2 = call_set(rt, ctx, 0x0886A9A0u, {self, 2u});
    const u32 part5 = call_set(rt, ctx, 0x0886A9A0u, {self, 5u});
    const u32 tag = call_set(rt, ctx, 0x08AC2690u, {command});
    if (tag == tag_of('D', 'C', 'O', 'P')) {
        dispatch_token_pair(rt, ctx, frame, self, command, 0x10u, 0x0Cu, 0x088730B0u);
        return;
    }
    if (tag > tag_of('D', 'C', 'O', 'P')) {
        if (tag == tag_of('P', 'O', 'S', 'X')) {
            if (part2 == 0u) {
                ctx.gpr[2] = 0u;  // the comparison's sltu result
                return;
            }
            dispatch_position(rt, ctx, frame, self, command, 0x38u, 0x34u);
            return;
        }
        if (tag > tag_of('P', 'O', 'S', 'X')) {
            if (tag == tag_of('P', 'O', 'S', 'Y')) {
                if (part2 == 0u) {
                    ctx.gpr[2] = tag_of('P', 'O', 'S', 'Z');  // the constant the delay slot left
                    return;
                }
                dispatch_position(rt, ctx, frame, self, command, 0x30u, 0x2Cu);
                return;
            }
            ctx.gpr[2] = tag_of('P', 'O', 'S', 'Z');
            if (tag != tag_of('P', 'O', 'S', 'Z')) return;
            if (part2 == 0u) return;
            dispatch_position(rt, ctx, frame, self, command, 0x28u, 0x24u);
            return;
        }
        ctx.gpr[2] = tag_of('D', 'C', 'O', 'T');
        if (tag != tag_of('D', 'C', 'O', 'T')) return;
        dispatch_token_pair(rt, ctx, frame, self, command, 0x08u, 0x04u, 0x08873260u);
        return;
    }
    if (tag == tag_of('C', 'A', 'M', 'S')) {
        const u32 first = frame.at(0x20u);
        const u32 scene = word(rt, self + 0x70u);
        call_set(rt, ctx, 0x08AC2730u, {first, command, 0u});
        const u32 flag = call_set(rt, ctx, 0x08AC2894u, {first});
        rt.memory().store8(scene + 0x7Cu, static_cast<std::uint8_t>(flag));
        call_set(rt, ctx, 0x08AC26DCu, {first});
        const u32 second = frame.at(0x1Cu);
        const u32 scene2 = word(rt, self + 0x70u);
        call_set(rt, ctx, 0x08AC2730u, {second, command, 1u});
        call_setf(rt, ctx, 0x08AC28E8u, {second}, {});
        put_float(rt, scene2 + 0x80u, ctx.fpr[0]);
        call_set(rt, ctx, 0x08AC26DCu, {second});
        const u32 scene3 = word(rt, self + 0x70u);
        put_float(rt, scene3 + 0x84u, 1.0f);
        ctx.gpr[2] = scene3;  // v0 = the last scene load
        return;
    }
    if (tag == tag_of('C', 'M', 'O', 'F')) {
        dispatch_token_pair(rt, ctx, frame, self, command, 0x18u, 0x14u, 0x08873194u);
        return;
    }
    ctx.gpr[2] = tag_of('A', 'N', 'I', 'M');
    if (tag != tag_of('A', 'N', 'I', 'M')) return;
    dispatch_anim(rt, ctx, frame, self, arg, command, part2, part5);
}

// --- 0x088B3834 ---
// A factory over a global pooled allocator. The pool state is at 0x08D75F5C (K below): a vector of chunk pointers (+4 begin,
// +8 end), a circular free list of {next, size-in-8-byte-units} nodes (+0x14 head), the next chunk's size (+0x18), the units
// handed out (+0x1C), the units in use (+0x20) and their high-water mark (+0x24); 0x08D75F50 is its once-flag and 0x08D75F68 the
// sentinel node the list starts life with. The function first asks the free list for 2 units (creating a 0x1001-unit chunk, and
// more after it, when no node has 2), then puts those 2 units back right away, and finally builds the 0x338-byte object
// (vtable 0x08CC30D0, 0x088B301C(object, a1, a2)) that out's +4 takes. v0 is that constructor's result.
namespace pool {
constexpr u32 kState = 0x08D75F5Cu;   // +0x00, +0x04 begin, +0x08 end, +0x0C sentinel next, +0x10, +0x14 list head,
                                      // +0x18 chunk units, +0x1C, +0x20 in use, +0x24 peak
constexpr u32 kOnce = 0x08D75F50u;
constexpr u32 kSentinel = 0x08D75F68u;
constexpr u32 kUnitBytes = 8u;

// The state's once-guarded initializer (it runs here again even after the first one, the asm repeats the whole block).
void initialize(Runtime &rt, AllegrexContext &ctx) {
    if (rt.memory().load8(kOnce) != 0u) return;
    if (call_set(rt, ctx, 0x08B6441Cu, {kOnce}) == 0u) return;
    put(rt, kState + 0x18u, 0x1001u);
    put(rt, kState + 0x0Cu, kSentinel);
    put(rt, kState + 0x10u, 0u);
    put(rt, kState + 0x04u, 0u);
    put(rt, kState + 0x08u, 0u);
    put(rt, kState + 0x14u, kSentinel);
    put(rt, kState + 0x1Cu, 0u);
    put(rt, kState + 0x20u, 0u);
    put(rt, kState + 0x24u, 0u);
    put(rt, kState, 0u);
    call_set(rt, ctx, 0x08B64494u, {kOnce});
    call_set(rt, ctx, 0x08B77B1Cu, {0x088B276Cu});
}

// Inserts `block` (its +4 holds the size in 8-byte units) into the address-ordered circular free list, merging it with an
// adjacent free block on either side, and drops its size from the units in use. The search walks the circle from the head and
// stops at the first (prev, cur) pair the block fits between, wrapping around like the original's three compares.
void insert(Runtime &rt, u32 block) {
    u32 prev = word(rt, kState + 0x14u);
    for (;;) {
        const u32 cur = word(rt, prev);
        const bool sorted = prev < block;       // a3
        const bool before_cur = block < cur;    // the block fits before the next node
        const bool forward = prev < cur;        // the pair has not wrapped around
        const bool position = sorted ? (before_cur || !forward) : (!forward && before_cur);
        if (position) break;
        prev = cur;
    }
    const u32 size = word(rt, block + 4u);
    put(rt, kState + 0x20u, word(rt, kState + 0x20u) - size);
    const u32 next = word(rt, prev);
    if (block + size * kUnitBytes == next) {   // the block ends where the next one starts: merge
        put(rt, block + 4u, size + word(rt, next + 4u));
        put(rt, block, word(rt, next));
    } else {
        put(rt, block, next);
    }
    const u32 previous_size = word(rt, prev + 4u);
    if (block == prev + previous_size * kUnitBytes) {  // the previous node ends where the block starts: merge
        put(rt, prev + 4u, previous_size + word(rt, block + 4u));
        put(rt, prev, word(rt, block));
    } else {
        put(rt, prev, block);
    }
    put(rt, kState + 0x14u, prev);
}
} // namespace pool

void func_088B3834(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 argument1 = ctx.gpr[5];
    const u32 argument2 = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u},
                                           {31u, 0x2Cu}, {16u, 0x10u}});
    ctx.gpr[22] = argument2;
    ctx.gpr[21] = argument1;
    ctx.gpr[19] = out;
    put(rt, out, 0x08CC30D0u);
    put(rt, out + 4u, 0u);
    pool::initialize(rt, ctx);
    // 2 units from the free list; a node with fewer units is skipped (the walk stops back at the head), and a chunk is created
    // and inserted when nothing fits. Each pass re-reads the head (after an insert the head is the insert position).
    u32 block = 0u;
    bool taken = false;
    u32 prev = word(rt, pool::kState + 0x14u);
    u32 node = word(rt, prev);
    for (;;) {
        const u32 units = word(rt, node + 4u);
        if (units >= 2u) {
            if (units == 2u) {
                put(rt, prev, word(rt, node));
            } else {
                const u32 rest = units - 2u;
                put(rt, node + 4u, rest);
                node += rest * pool::kUnitBytes;
                put(rt, node + 4u, 2u);
            }
            put(rt, pool::kState + 0x14u, prev);
            const u32 used = word(rt, pool::kState + 0x20u) + 2u;
            if (word(rt, pool::kState + 0x24u) < used) put(rt, pool::kState + 0x24u, used);
            put(rt, pool::kState + 0x20u, used);
            block = node + 8u;
            taken = true;
            break;
        }
        if (word(rt, pool::kState + 0x14u) == node) {
            // The head node has fewer than 2 units and the walk came full circle: create a chunk of the current size.
            u32 chunk_units = word(rt, pool::kState + 0x18u);
            u32 chunk_bytes = chunk_units << 3;
            if (chunk_units < 3u) {
                chunk_units = 2u;
                chunk_bytes = 0x10u;
            }
            const u32 chunk = call_set(rt, ctx, 0x08B2B4D8u, {0x10u, chunk_bytes});
            if (chunk == 0u) {
                taken = false;
                break;
            }
            put(rt, frame.at(0u), chunk);
            const u32 chunk_size = word(rt, pool::kState + 0x18u);
            put(rt, pool::kState + 0x18u, chunk_size + (chunk_size >> 1));
            const u32 begin = word(rt, pool::kState + 0x04u);
            if (begin == word(rt, pool::kState + 0x08u)) {
                call_set(rt, ctx, 0x08BB13D4u, {pool::kState, begin, frame.at(0u)});
            } else {
                if (begin != 0u) put(rt, begin, chunk);
                put(rt, pool::kState + 0x04u, begin + 4u);
            }
            put(rt, pool::kState + 0x1Cu, word(rt, pool::kState + 0x1Cu) + chunk_units);
            put(rt, chunk + 4u, chunk_units);
            const u32 used = word(rt, pool::kState + 0x20u) + chunk_units;
            put(rt, pool::kState + 0x20u, used);
            // The asm tests `chunk + 8` for zero here (never true) before inserting.
            pool::insert(rt, chunk);
            prev = word(rt, pool::kState + 0x14u);
            if (prev == 0u) {
                taken = false;
                break;
            }
            node = word(rt, prev);
            continue;
        }
        prev = node;
        node = word(rt, prev);
    }
    pool::initialize(rt, ctx);
    if (taken) pool::insert(rt, block - 8u);   // the 2 units go straight back
    const u32 object = call_set(rt, ctx, 0x08B62344u, {0x338u});
    call_set(rt, ctx, 0x088B301Cu, {object, argument1, argument2});
    put(rt, out + 4u, object);
}

// --- 0x088FCCCC ---
// A shared_ptr-shaped assignment: a0's +0x28 pair is replaced. A 0x548-byte object is built (vtable 0x08CC4300, 0x088FCA30 with
// a1 / a2 and 1) and a control block {vtable 0x08CC4370, strong 1, weak 1, object} follows (0x08B62344); the pair on the stack
// {object, control} is spilled through 0x08B8E380 and swapped into the destination, the old control released. The new object's
// +4 object is locked (0x08B34888), and either a member-function pointer decoded from the pair at 0x08C8A4D8 (0x08BBC08C) is
// called on it, or (when word(lock + 0x2C) is not zero) a 0x14-byte holder {vtable 0x08CC4388, object, control, pair} is put
// into lock + 0x34 (its previous +0x38 disposed of through vtable slot 0, and +0x38 set to the once-guarded 0x08D76448). The
// lock is released (0x08B34B24) and the control block dropped twice. v0 is 1 or the last vtable call's result.
namespace shared {
constexpr u32 kObjectVtable = 0x08CC4300u;
constexpr u32 kControlVtable = 0x08CC4370u;
constexpr u32 kHolderVtable = 0x08CC4388u;
constexpr u32 kPairSource = 0x08C8A4D8u;   // {function, adjustment} the 0x08BBC08C decode starts from
constexpr u32 kOnce = 0x08D761B0u;
constexpr u32 kDeleter = 0x08D76448u;
constexpr u32 kDeleterVtable = 0x08CC43A0u;
} // namespace shared


// --- 0x08C7759C: std::basic_filebuf's underflow (the throws it reaches name it) ---
// A0's filebuf is checked (bit 3 of +0x2C set, +0x82 clear), a pending putback is folded back into the get area (+0x8C), and an
// unread byte is returned as is. With the get area exhausted, +0x90's codecvt (vtable slots 0x10 / 0x14 / 0x18 / 0x20) is used:
// 0x08B61718 reads more into the buffer (+0x94, size +0x98, indexes +0x9C / +0xA0), the conversion may grow that buffer
// (0x08B6461C / 0x08B79A88 / 0x08B79BAC / 0x08B61FC0), and the get area (+4 / +8 / +0xC) is set to the buffer with +0x81 flagged.
// A null codecvt, a partial character and a failed read end in 0x08B5A9FC / 0x08B5AA30, which do not return. v0 is the byte or
// -1.
namespace filebuf {
constexpr u32 kThrow = 0x08B5AA30u;
constexpr u32 kIncomplete = 0x08CB5CCCu;      // "basic_filebuf::underflow incomplete character in file"
constexpr u32 kInvalidSequence = 0x08CB5D04u; // "basic_filebuf::underflow invalid byte sequence in file"
constexpr u32 kReadError = 0x08CB5D3Cu;       // "basic_filebuf::underflow error reading the file"
constexpr u32 kMaxLength = 0x08CB5C90u;       // "basic_filebuf::underflow codecvt::max_length() is not valid"
constexpr u32 kNoCodecvt = 0x08CB5C90u;       // the null-codecvt path runs the same assert blocks
constexpr u32 kEof = 0xFFFFFFFFu;
} // namespace filebuf

void func_08C7759C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {31u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u},
                                           {17u, 0x14u}});
    if ((word(rt, self + 0x2Cu) & 8u) == 0u) {
        ctx.gpr[2] = filebuf::kEof;
        return;
    }
    if (rt.memory().load8(self + 0x82u) != 0u) {
        ctx.gpr[2] = filebuf::kEof;
        return;
    }
    u32 read_pointer = word(rt, self + 8u);
    if (rt.memory().load8(self + 0x8Cu) != 0u) {
        const u32 current = word(rt, self + 4u);
        const u32 saved_position = word(rt, self + 0x84u);
        const u32 saved_end = word(rt, self + 0x88u);
        const u32 moved = (read_pointer ^ current) != 0u ? 1u : 0u;
        read_pointer = moved + saved_position;
        put(rt, self + 4u, word(rt, self + 0x78u));
        put(rt, self + 0xCu, saved_end);
        put(rt, self + 0x84u, read_pointer);
        put(rt, self + 8u, read_pointer);
        rt.memory().store8(self + 0x8Cu, 0u);
    }
    if (read_pointer < word(rt, self + 0xCu)) {
        ctx.gpr[2] = rt.memory().load8(read_pointer);
        return;
    }
    const u32 source = word(rt, self + 0x90u);
    const u32 unit = word(rt, self + 0x7Cu);
    const u32 count = unit < 2u ? unit - 1u : 1u;
    put(rt, frame.at(0u), count);
    if (source == 0u) {
        call_set(rt, ctx, 0x08B5A9FCu, {unit - 1u});
        call_set(rt, ctx, filebuf::kThrow, {filebuf::kNoCodecvt});
        call_set(rt, ctx, filebuf::kThrow, {filebuf::kReadError});
        return;  // the original's three calls do not return
    }
    u32 status = 0u;     // s3: 0 none, 1 a partial character, 2 an invalid byte sequence, 3 consumed
    u32 wrote_eof = 0u;  // s4's later use
    u32 excess = 0u;     // s2
    // The shared no-data tail: the get area goes back to the buffer start; a partial character throws instead of returning eof.
    const auto reset = [&](u32 code) {
        const u32 base = word(rt, self + 0x78u);
        rt.memory().store8(self + 0x81u, 0u);
        put(rt, self + 0xCu, base);
        put(rt, self + 4u, base);
        put(rt, self + 8u, base);
        put(rt, self + 0x14u, 0u);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0x18u, 0u);
        if (code != 1u) {
            ctx.gpr[2] = filebuf::kEof;
            return;
        }
        call_set(rt, ctx, filebuf::kThrow, {filebuf::kIncomplete});
    };
    const auto throw_status = [&](u32 code) {
        if (code == 2u) {
            call_set(rt, ctx, filebuf::kThrow, {filebuf::kInvalidSequence});
            return;
        }
        call_set(rt, ctx, filebuf::kThrow, {filebuf::kReadError});
    };
    // 0x08C77850: the get area takes `produced` bytes from the buffer; a non-positive count either resets or throws.
    const auto set_area = [&](u32 produced) {
        if (static_cast<s32>(produced) <= 0) {
            if (wrote_eof != 0u) {
                reset(status);
                return;
            }
            throw_status(status);
            return;
        }
        const u32 base = word(rt, self + 0x78u);
        if ((word(rt, self + 0x2Cu) & 8u) != 0u) {
            put(rt, self + 0xCu, base + produced);
            put(rt, self + 4u, base);
            put(rt, self + 8u, base);
        } else {
            put(rt, self + 0xCu, base);
            put(rt, self + 4u, base);
            put(rt, self + 8u, base);
        }
        rt.memory().store8(self + 0x81u, 1u);
        put(rt, self + 0x14u, 0u);
        put(rt, self + 0x10u, 0u);
        put(rt, self + 0x18u, 0u);
        ctx.gpr[2] = rt.memory().load8(word(rt, self + 8u));
    };
    u32 converted_size;  // s4 while it holds the size the conversion needs
    u32 scaled;          // a3
    if (call_at(rt, ctx, word(rt, word(rt, source) + 0x18u), {source}) != 0u) {
        // 0x08C7768C: the pending putback is already in the buffer, so one read fills it.
        status = 0u;
        u32 produced = call_set(rt, ctx, 0x08B61718u, {self + 0x24u, word(rt, self + 4u), count});
        status = 0u;
        if (produced == 0u) {
            reset(status);
            return;
        }
        wrote_eof = 0u;
        set_area(produced);
        return;
    }
    {
        const u32 units = call_at(rt, ctx, word(rt, word(rt, source) + 0x14u), {source});
        if (static_cast<s32>(units) <= 0) {
            const u32 again = call_at(rt, ctx, word(rt, word(rt, source) + 0x20u), {source});
            converted_size = (again + count) - 1u;
            scaled = count;
        } else {
            scaled = count * units;
            converted_size = scaled;
        }
    }
    const u32 available = word(rt, self + 0xA0u) - word(rt, self + 0x9Cu);
    excess = available < scaled ? scaled - available : 0u;
    if (rt.memory().load8(self + 0x81u) != 0u) {
        u32 adjusted = 0u;
        if (available == 0u) adjusted = excess;
        if ((word(rt, self + 0xCu) ^ word(rt, self + 4u)) == 0u) excess = adjusted;
    }
    u32 carried = 0u;  // s3's use here: the bytes already in the buffer
    if (static_cast<s32>(word(rt, self + 0x98u)) < static_cast<s32>(converted_size)) {
        const u32 grown = call_set(rt, ctx, 0x08B6461Cu, {converted_size});
        if (available != 0u) {
            call_set(rt, ctx, 0x08B79A88u, {grown, word(rt, self + 0x9Cu), available});
            carried = available;
        }
        const u32 old_buffer = word(rt, self + 0x94u);
        if (old_buffer != 0u) call_set(rt, ctx, 0x08B61FC0u, {old_buffer});
        put(rt, self + 0x94u, grown);
        put(rt, self + 0x98u, converted_size);
    } else if (available != 0u) {
        call_set(rt, ctx, 0x08B79BACu, {word(rt, self + 0x94u), word(rt, self + 0x9Cu), available});
        carried = available;
    }
    u32 buffer = word(rt, self + 0x94u);
    put(rt, self + 0x60u, word(rt, self + 0x48u));
    put(rt, self + 0x64u, word(rt, self + 0x4Cu));
    put(rt, self + 0x68u, word(rt, self + 0x50u));
    put(rt, self + 0x6Cu, word(rt, self + 0x54u));
    put(rt, self + 0x70u, word(rt, self + 0x58u));
    put(rt, self + 0x74u, word(rt, self + 0x5Cu));
    put(rt, self + 0x9Cu, buffer);
    put(rt, self + 0xA0u, buffer + carried);
    status = 0u;
    wrote_eof = 0u;
    bool need_read = static_cast<s32>(excess) > 0;
    for (;;) {
        if (need_read) {
            if (static_cast<s32>(word(rt, self + 0x98u)) <
                static_cast<s32>((word(rt, self + 0xA0u) - buffer) + excess)) {
                call_set(rt, ctx, filebuf::kThrow, {filebuf::kMaxLength});
                return;  // does not return in the original
            }
            const u32 got = call_set(rt, ctx, 0x08B61718u, {self + 0x24u, word(rt, self + 0xA0u), excess});
            u32 end;
            if (got == 0u) {
                wrote_eof = 1u;
                end = word(rt, self + 0xA0u);
            } else if (got == 0xFFFFFFFFu) {
                throw_status(status);
                return;
            } else {
                end = word(rt, self + 0xA0u) + got;
            }
            put(rt, self + 0xA0u, end);
        }
        need_read = true;
        status = call_set(rt, ctx, word(rt, word(rt, source) + 0x10u),
                          {source, self + 0x48u, word(rt, self + 0x9Cu), word(rt, self + 0xA0u), self + 0x9Cu,
                           word(rt, self + 4u), word(rt, self + 4u) + count, frame.at(4u)});
        u32 consumed;
        if (status == 3u) {
            const u32 base = word(rt, self + 0x94u);
            const u32 total = word(rt, self + 0xA0u) - base;
            put(rt, frame.at(8u), total);
            consumed = count < total ? count : total;
            call_set(rt, ctx, 0x08B79A88u, {word(rt, self + 4u), base, consumed});
            put(rt, self + 0x9Cu, word(rt, self + 0x94u) + consumed);
        } else {
            consumed = word(rt, frame.at(4u)) - word(rt, self + 4u);
        }
        if (status == 2u || consumed != 0u) {
            set_area(consumed);
            return;
        }
        if (wrote_eof != 0u) {
            excess = 1u;
            reset(status);
            return;
        }
        buffer = word(rt, self + 0x94u);
    }
}

} // namespace
} // namespace new_batch_main_251
} // namespace gt::game

#define NEWMAIN251(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_251::name, gt::ReturnKind::kind)
NEWMAIN251(0x0886ECD4u, func_0886ECD4, Word);
NEWMAIN251(0x088B3834u, func_088B3834, Word);
NEWMAIN251(0x08C7759Cu, func_08C7759C, Word);

// ---- from main-session batch 252 (draft c757722ae1a663f2f387838df0df7df3a10626bd) ----
// New batch, main session 252: five long functions (276-280 instructions) the recompiler emits and nothing had taken, written as
// structured C from `tools.disasm` (every branch target read, every delay slot read as "before the call/branch", a `bnel` / `beql`
// delay slot as "when the branch is taken"). Three of them start from the older draft in `new_batch_main_42.cpp` (commit 720e308)
// and were re-read against the asm; the object pairs, the argument registers of the pair call and the float operand order were
// corrected there.
//
// Written (registered at the end of this file): func_088C22C8 (per-frame state update), func_088FD508 (installs a counted pair and
// a delegate), func_0891ABC4 (steering floats), func_089B1874 (merges two pointer lists and writes the merged order into the
// caller's vector), func_08A2BDF0 (formats one string per entry of a 0x40-byte block into a shared object's vector and hands that
// object to the caller's out parameter).
//
// The roles below come from the accesses, not from the game: `self` is a0, `state` / `entry` / `member` a fixed object reached
// through a word, and `counted` a reference-counted object whose strong counter sits at +4 and weak one at +8 and whose vtable
// words at offsets 8 and 0xC run when a counter reaches zero (the ll / sc pairs read as a plain load and store, as
// `src/game/unsorted/hot_batch_118.cpp` does: one thread, and the sc's success flag is 1 in v0 either way). No name is claimed:
// none of these functions has evidence for one.
//
// Every call goes through `call_set` / `call_setf` (v0, v1 and f0 stay where the original's `jal` leaves them) and passes the
// argument registers the original has set at that point; the callee-saved registers are restored on the way out (SavedRegs) and
// the floating ones the original saves (f20-f25) are kept there too (SavedFloat). Frames are the original's own sizes, so the
// addresses a callee writes through (`sp + N`) land in the original's slots.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_252 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call: v0 / v1 / f0 afterwards are the callee's, as after the original's `jal`, and a callee that never writes v0 hands
// the caller's own v0 on.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments, {}).v0;
}
GuestResult call_setf(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                  std::initializer_list<float> floats) {
    return call_keep(rt, ctx, target, arguments, floats);
}

// A float constant exactly as the original's `lui` / `ori` pair (or bare `lui`) builds it.
float f32(u32 bits) { return std::bit_cast<float>(bits); }

void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put16(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 byte_u(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s32 signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

// The callee-saved registers s0-s7 / fp as they were at entry.
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

// A floating-point register as it was at entry (the original saves it in its frame and restores it before returning).
class SavedFloat {
public:
    SavedFloat(AllegrexContext &ctx, unsigned index) : ctx_(ctx), index_(index), saved_(ctx.fpr[index]) {}
    ~SavedFloat() { ctx_.fpr[index_] = saved_; }
    SavedFloat(const SavedFloat &) = delete;
    SavedFloat &operator=(const SavedFloat &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned index_;
    float saved_;
};

// The ll / sc counters (one thread: a plain load and store). A decrement leaves the old value in a0, v0 = 1 (the sc's success
// flag) and v1 = the address.
u32 atomic_decrement(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    return old;
}

void atomic_increment(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// A virtual call through `lw v1, 0(object); lw v0, slot(v1); jalr v0` with the arguments the original sets.
GuestResult virtual_call(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments,
                         std::initializer_list<float> floats = {}) {
    ctx.gpr[3] = word(rt, object);
    return call_keep(rt, ctx, word(rt, ctx.gpr[3] + slot), arguments, floats);
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088C22C8 (a0 = self): the per-frame state update. 0x088BCA5C(self) runs first; the state block at self+0x24 has a mode byte at
// +0xA5: 7 and 9 just run 0x088BD0C4(self, 0.0) and return its answer. With the byte at word(self+8)+3 set the object is ended
// (a value of 2 runs 0x088C4BB0(self)). Otherwise the mode picks the work: mode 0 asks the member at self+0x1C for index 7 and
// reports a different answer through 0x088BF94C; modes 1/2/4/5 look the entry up with 0x088DF13C(word(self+12), word(self+16)),
// and an entry whose byte +0x25 is set (or the state's halfword +2 having bit 1) runs a 3-or-7 sequence of the member's virtual
// words 0x18 / 0x24 reporting through 0x088BF94C and 0x088BF978, while a clear one raises byte +0x52 to 0x78, sets bit 1 of the
// halfword and drives the same words. Then 0x088BEC3C(self) (its f0 kept in f21) clears bit 0 of word(self+0x2C) when the state
// byte +0x12 is set, 0x088BC21C(self) runs, four lane records (0x0894268C(word(self+4), lane, byte at lane record +0xE9)) are
// summed, and the ratio (float at word(self+8)+0x720 over a quarter of that sum, absolute) goes through 0x088BFE70 /
// 0x088C0738 / 0x088C10B0 / 0x088BE248 / 0x088BC000 / 0x088C12B0. v0 is the last call's, or 4 / 0 on the two early exits.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x088FD508 (a0 = self, a1 = source, a2 = other): installs a new counted pair, the same shape as 0x088FD0F0
// (`hot_batch_118.cpp`) with the pair built from source / other. 0x089624FC(source, source, other) decides first (zero returns
// 0, a null a2 returns its answer); the object at a2 answers virtual word 0x38, and two -1 words return 0. A 0x548-byte object is
// allocated (0x08B62344), constructed with 0x088FCA30(object, source, other, 0) and given the vtable 0x08CC51A8; a 0x10-byte
// holder takes 0x08CC51B8 and the counts 1 / 1 with the object at +0xC, and 0x08B8E380(sp+4, object, object, 1) runs on it. The
// pair at self+0x10 / +0x14 is replaced (the previous holder released through its counted vtable words 8 / 0xC), the global pair
// 0x08C8A4E8 goes through 0x08BBC08C, the new object's owner word at +4 is locked (0x08B34888) and read at +0x2C: not zero
// inserts a 0x14-byte node 0x08CC51D0 into the owner's 0x34 / 0x38 list (a guarded static 0x08CC51E8 at 0x08D76498 built under
// the byte 0x08D761B8) and releases the holder; zero invokes the delegate pair (bit 0 of the second word selects a virtual slot)
// with the object and the second word's half as the this adjustment. The owner is unlocked (0x08B34B24) and the holder released
// (the second time as a doubled release). v0 = 1 at the end, 0 on the early exits.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x0891ABC4 (a0 = self, a1 = other): the steering update. The float holder at self+0x1F8 answers three values (0x0891A698,
// 0x0891A874, 0x0891A864). With the kind word at self+0x1FC equal to 1, 3 or 4 the controller from 0x089260F4(self) is asked for
// four floats (its virtual word 0x30 with 0 / 4 / 8) and seven indices (word 0x28 with 0x19, 0x1A, 1, 2, 3, 4, 8): the flags fold
// into the two accumulators (+-1 and the byte at holder+0x29 picking -2.0 / 0.7 or -0.2 / 0.2), indices 0x19 / 0x1A pass the
// accumulator through 0x08B66684, the last index / 1.0 is written at holder+0x29, the third float scaled by 0x3C8EFA35 (degrees to
// radians) joins f23, and the sum goes to float(self+0x200). Kind 2 skips all that. Any other kind takes the other branch: the
// holder's float at self+0x200 is fed to 0x088D2BC8(word(other+0x61C)) and stored back (the store sits in the call's delay slot),
// 0x088DA460(other) is compared with the first accumulator and the accumulator moves by 0x088DA460's answer (subtracting under a
// half of it), then relaxes toward the stored value by the factor at self+0x204. Both branches end with 0x0891A6A0(holder, f21,
// f22) / 0x0891A87C(holder, f23) and 0x0891A884(holder, f12, f13, f14) with the constants kind 3 and 4 pick, the other object
// stored at holder+8, 0x0892632C(holder, self+0x10), and 0x08B4A774(self+0x1E4, self+0x10, sp) with a {0, 0, 0, 1.0} record;
// v0 is that last call's.

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x089B1874 (a0 = passed on, a1 = list, a2 = key, a3 = source): collects the elements of `source` that do not have bit 7 of
// word(element+0x60), merges them with a locked copy of `list` into a 8-byte-per-item array (element, rank) whose ranks are split
// at `key`: the elements of the copy up to and including `key` keep their positions 0.., the `key` one records the boundary, and
// the collected elements plus the rest of the copy get the ranks from there on. The array goes through 0x08BF9240 (fill) and
// 0x08BF933C (the ordered container), each collected element's virtual slot 0x160 runs with a1 = a0, and the container's rank
// fields are copied back into `list`'s array, which was resized to the merged count (0x08BF9B6C erase or 0x08BFB68C fill). The
// lock is released (0x08B3457C), the temporaries are destroyed and freed. v0 is the last free's answer, or 0 when there was
// nothing to free.
void func_089B1874(Runtime &rt, AllegrexContext &ctx) {
    const u32 argument = ctx.gpr[4];  // s6: a1 of every virtual slot 0x160
    const u32 list = ctx.gpr[5];      // s3
    const u32 key = ctx.gpr[6];       // s4
    const u32 source = ctx.gpr[7];    // s2
    SavedRegs saved(ctx);
    GuestFrame frame(ctx, 0x60u);
    constexpr u32 kLock = 0x08CF4AA4u;
    const u32 collected = frame.at(0x10u);   // vector of 4-byte elements
    const u32 temporary = frame.at(0x1Cu);   // its locked copy
    const u32 records = frame.at(0x28u);     // 8-byte-per-item array
    const u32 container = frame.at(0x34u);   // 0x08BF933C's container
    const auto words_between = [](u32 from, u32 to) {
        return static_cast<u32>(static_cast<s32>(to - from) >> 2);
    };
    // (to - from) >> shift << shift: the byte count a `0x08A5BC54` frees for an item size of 1 << shift.
    const auto block_bytes = [](u32 from, u32 to, unsigned shift) {
        return static_cast<u32>(static_cast<s32>(to - from) >> shift) << shift;
    };
    const auto allocate = [&rt, &ctx](u32 size) { return call_set(rt, ctx, 0x08A5BC20u, {size, 4u}); };
    // 0x08A5BC54(block, bytes, 4): frees a block from 0x08A5BC20.
    const auto free_block = [&rt, &ctx](u32 block, u32 bytes) { return call_set(rt, ctx, 0x08A5BC54u, {block, bytes, 4u}); };

    const u32 count = words_between(word(rt, source), word(rt, source + 4u));
    put(rt, collected + 4u, 0u);
    put(rt, collected, 0u);
    put(rt, collected + 8u, 0u);
    (void)call_set(rt, ctx, 0x08BFB8DCu, {collected, count});  // reserve(count)
    u32 end = 0u;  // $a3
    for (u32 index = 0u; index < count; ++index) {
        const u32 element_at = word(rt, source) + index * 4u;  // $a2: the source slot the push_back reads
        const u32 element = word(rt, element_at);
        if ((word(rt, element + 0x60u) & 0x80u) != 0u) continue;
        const u32 capacity = word(rt, collected + 8u);
        if (end == capacity) {
            (void)call_set(rt, ctx, 0x08BFB9D8u, {collected, end, element_at});  // push_back (reallocates, updates the end)
            end = word(rt, collected + 4u);
        } else {
            // A null end takes the `beql` path: no store, and the end still moves by one slot.
            if (end != 0u) put(rt, end, element);
            end += 4u;
            put(rt, collected + 4u, end);
        }
    }
    if (word(rt, collected) != end) {
        put(rt, temporary + 8u, 0u);
        put(rt, temporary + 4u, 0u);
        put(rt, temporary, 0u);
        (void)call_set(rt, ctx, 0x08B344E0u, {kLock});
        if (temporary != list) (void)call_set(rt, ctx, 0x08BFBB38u, {temporary, list});
        (void)call_set(rt, ctx, 0x08B3457Cu, {kLock});
        const u32 copied = words_between(word(rt, temporary), word(rt, temporary + 4u));
        const u32 kept = words_between(word(rt, collected), word(rt, collected + 4u));
        const u32 total = copied + kept;
        const u32 bytes = total * 8u;
        const u32 records_begin = allocate(bytes);
        put(rt, records, records_begin);
        put(rt, records + 4u, records_begin + bytes);   // the end is the whole block before 0x08BF9240 fills it
        put(rt, records + 8u, records_begin + bytes);
        put(rt, frame.at(8u), 0u);
        (void)call_set(rt, ctx, 0x08BF9240u, {records_begin, total, frame.at(8u), frame.at(3u)});
        put(rt, frame.at(0xCu), 0u);
        // The ranks: the copy's elements first (the `key` one moves the running count past the collected ones), then the
        // collected ones from the boundary on (the copy's size when the key element never shows up).
        u32 boundary = copied;
        u32 position = 0u;
        for (u32 index = 0u; index < copied; ++index) {
            const u32 element = word(rt, word(rt, temporary) + index * 4u);
            if (element == key) {
                boundary = position;
                position += kept;
            }
            put(rt, records_begin + position * 8u + 4u, position);
            put(rt, records_begin + position * 8u, element);
            ++position;
        }
        u32 rank = boundary;
        for (u32 index = 0u; index < kept; ++index) {
            const u32 element = word(rt, word(rt, collected) + index * 4u);
            put(rt, records_begin + (boundary + index) * 8u + 4u, rank);
            put(rt, records_begin + (boundary + index) * 8u, element);
            ++rank;
        }
        const u32 records_end = word(rt, records + 4u);
        (void)call_set(rt, ctx, 0x08BF933Cu, {container, records_begin, records_end});
        const u32 second = word(rt, container + 8u);
        if (second != 0u)
            (void)call_set(rt, ctx, 0x08BFA640u,
                           {records_begin, records_end, second, word(rt, container + 4u), records_end});
        else
            (void)call_set(rt, ctx, 0x08BFA8F8u, {records_begin, records_end, records_end, word(rt, container + 4u)});
        (void)call_set(rt, ctx, 0x08BF8B58u, {word(rt, container + 8u)});
        const u32 elements_end = word(rt, collected + 4u);  // s1: loaded once into a register, the calls cannot move it
        for (u32 slot = word(rt, collected); slot != elements_end; slot += 4u)
            (void)virtual_call(rt, ctx, word(rt, slot), 0x160u, {word(rt, slot), argument});
        (void)call_set(rt, ctx, 0x08B344E0u, {kLock});
        const u32 size = words_between(word(rt, list), word(rt, list + 4u));
        put(rt, frame.at(4u), 0u);
        if (total < size) {
            (void)call_set(rt, ctx, 0x08BF9B6Cu, {list, word(rt, list) + total * 4u, word(rt, list + 4u)});
        } else {
            (void)call_set(rt, ctx, 0x08BFB68Cu, {list, word(rt, list + 4u), total - size, frame.at(4u)});
        }
        for (u32 index = 0u; index < total; ++index)
            put(rt, word(rt, list) + index * 4u, word(rt, records_begin + index * 8u));
        (void)call_set(rt, ctx, 0x08B3457Cu, {kLock});
        // The container's element destructor is trivial: its walk (0x089B1B2C) touches no memory.
        (void)call_set(rt, ctx, 0x08BF8BC4u, {records_begin, word(rt, records + 4u), frame.at(2u)});
        // The records block goes back first, then the two vectors.
        const u32 records_begin2 = word(rt, records);
        if (records_begin2 != 0u)
            (void)free_block(records_begin2, block_bytes(records_begin2, word(rt, records + 8u), 3u));
        else
            ctx.gpr[2] = 0u;
    }
    // Destroy a vector: 0x08BF8B48 walks it (a 4-byte element), then its block is freed when it has one.
    const auto destroy_vector = [&](u32 slot, u32 tag) {
        (void)call_set(rt, ctx, 0x08BF8B48u, {word(rt, slot), word(rt, slot + 4u), tag});
        const u32 block = word(rt, slot);
        if (block == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        (void)free_block(block, block_bytes(block, word(rt, slot + 8u), 2u));
    };
    if (word(rt, collected) != end) destroy_vector(temporary, frame.at(1u));
    destroy_vector(collected, frame.at(0u));
}

// ----------------------------------------------------------------------------------------------------------------------------------------------------
// 0x08A2BDF0 (a0 = out): formats one string per entry of the block the first import call fills, keeps them in a vector of the
// shared object built at fp+0x2C (0x08A98A70) and hands that object to `out` (0x08A5BAD8). 0x08C7A8F0(fp+0x30, 0x40, fp+0x28)
// fills a 0x40-byte block and its count (a negative answer ends the function); `count` empty strings are default-constructed on
// a variable-length stack array (`subu $sp, $sp, $v1`, `(((count - 1) << 2) + 0x22) & ~0xF`, the base kept in $s3). Per entry:
// 0x08C7A8F8(entry, info = fp+0x70 with its size word 0x60, fp+0x28) fills a 0x60-byte info block (a negative answer ends the
// loop and skips the copy-out), nine (format, value) pairs go through 0x08A6BBE8 (assign from a literal) and 0x08A85B1C (build a
// temporary from a value at temps fp+0x24 down to fp+4; each temporary is copied onto the answer when that is not the temporary
// itself and destroyed), and the last temporary at fp is 0x08A76ABC'd into the vector at object+0x14 (grow with 0x08B8A03C when
// the next slot equals the limit) and destroyed. Afterwards the object is copied out, the stack strings are destroyed from the
// last one down to the base (0x08A5B9EC), and the object at fp+0x2C is destroyed last: v0 is that call's answer.
// The 0x40-byte block is an output (`a1` is its size), so nothing here reads it before 0x08C7A8F0 writes it.

} // namespace
} // namespace new_batch_main_252
} // namespace gt::game

#define NEW252(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_252::name, gt::ReturnKind::kind)
NEW252(0x089B1874u, func_089B1874, Word);

