// New batch main 1016 (draft, unverified): six functions handed to this batch, structured C from tools.disasm.
// Drafts of 089A2C00 / 08A192D4 / 08A7B8C8 / 08BBAA74 existed in earlier batches; each was checked against the asm again
// here and corrected where it differed (089A2C00's out-of-range phase answers 0xFFFF, not 1; 08BBAA74's null-world exit
// leaves v0 = 0; 08BA86F4 leaves v0 untouched when count is 0).
// Registration: v0 is what the asm leaves on every path, argument registers are set per call, and delay slots run before
// the call they belong to. Skip notes: none of the six is a setjmp-shaped record save.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1016 {
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
// A guest call whose v0 / v1 / f0 stay in the context like the original's, with the arguments in a0-a3 / t0-t3.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                      std::initializer_list<float> float_arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, float_arguments);
    take(ctx, result);
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments = {}) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments).v0;
}
GuestResult callf(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments,
                  std::initializer_list<float> float_arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    unsigned freg = 12u;
    for (const float argument : float_arguments) {
        if (freg <= 15u) ctx.fpr[freg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments, float_arguments);
}
// `lw v1, 0(object); lw v0, slot(v1); jalr v0`: both registers are set as the original sets them.
u32 call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
    return call(rt, ctx, ctx.gpr[2], arguments);
}
inline s32 signed_of(u32 value) { return static_cast<s32>(value); }
inline u32 sext16(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(value))); }
u32 half_u(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put8(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }

// ---------------------------------------------------------------------------------------------------------------------
// 0x089A2C00 (a0 = phase, a1 = id): the module's event handler. Phase 1 with id 0xFFFF registers the module: two helper
// calls, then fourteen name/handler pairs (each a two-word descriptor read from a table at 0x08C8C580..0x08C8C5E8 into the
// frame, the pair handed to 0x08BF5AC0). Phase 0 with id 0xFFFF tail-calls 0x08B28DC4. v0 = 0xFFFF for an unknown phase or
// a non-wildcard id; the last registration's answer on the success path.

// ---------------------------------------------------------------------------------------------------------------------
// The child-walk pair 0x08A192D4 / 0x08A7B8C8: one body, two constant sets. The node at a0 + 0x14 is walked for the visitor
// a1: its path is built (0x08A7F8E8), announced (0x08A8DB8C) and the child list (0x08A7D1F8 over a frame sentinel) is
// iterated; a child whose owner is the walked node goes straight to the visitor's slot 0x28, another owner is looked up
// (0x08A7C0E0), given its own path and passed through 0x08A8CE2C (or wrapped through 0x08A7BCC0 / 0x08A7B544), then
// 0x08A9032C applies the visitor to the child. The temporaries and the two strings are destroyed on the way out.
struct ChildWalk {
    u32 container_null;
    u32 child_null;
    u32 output_null;
    u32 wrapper_null;
    u32 separator;
    u32 path1;
    u32 held;
    u32 path2;
    u32 output;
    u32 cursor;
    u32 alternate;
    u32 key;
};

// The libstdc++ COW-string release: empty string 0x08D74FD0 short-circuits, else the refcount step 0x08B60804 and, when it
// does not answer > 0, the block destruction 0x08B8542C.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 slot, u32 scratch) {
    const u32 pointer = word(rt, slot);
    if (pointer - 0xCu == 0x08D74FD0u) {
        ctx.gpr[2] = 0x08D74FD0u;
        ctx.gpr[3] = 0x000A0000u;
        return;
    }
    if (signed_of(call(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {pointer - 0xCu, scratch});
}

void walk_children(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, const ChildWalk &k) {
    const u32 self = ctx.gpr[4];
    const u32 visitor = ctx.gpr[5];
    u32 node = word(rt, self + 0x14u);
    if (node == 0u) call(rt, ctx, 0x08A5BB7Cu, {k.container_null});
    call(rt, ctx, 0x08A7F8E8u, {frame.at(k.path1), node, 0u, k.separator});
    call(rt, ctx, 0x08A76724u, {frame.at(0x34u), frame.at(k.path1)});
    call(rt, ctx, 0x08A8DB8Cu, {visitor, frame.at(0x34u)});
    call(rt, ctx, 0x08A75A88u, {frame.at(0x34u)});
    node = word(rt, self + 0x14u);
    const u32 sentinel = frame.at(0x44u);
    put(rt, frame.at(0x54u), 0u);
    put(rt, sentinel, 0u);
    put(rt, frame.at(0x48u), 0u);
    put(rt, frame.at(0x4Cu), sentinel);
    put(rt, frame.at(0x50u), sentinel);
    if (node == 0u) call(rt, ctx, 0x08A5BB7Cu, {k.container_null});
    call(rt, ctx, 0x08A7D1F8u, {node, frame.at(0x40u)});
    u32 current = word(rt, frame.at(0x4Cu));
    while (current != sentinel) {
        const u32 attribute = word(rt, current + 0x14u);
        if (attribute == 0u) call(rt, ctx, 0x08A5BB7Cu, {k.child_null});
        const u32 holder = word(rt, attribute + 0x10u);
        if (holder != 0u && word(rt, holder + 4u) != 0u) {
            if (holder == word(rt, self + 0x14u)) {
                call_virtual(rt, ctx, attribute, 0x28u, {attribute, visitor});
            } else {
                put(rt, frame.at(k.key), holder);
                call(rt, ctx, 0x08A7C0E0u, {frame.at(k.held), frame.at(k.key)});
                const u32 output = word(rt, frame.at(k.held));
                if (output == 0u) call(rt, ctx, 0x08A5BB7Cu, {k.output_null});
                call(rt, ctx, 0x08A7F8E8u, {frame.at(k.path2), output, 0u, k.separator});
                call(rt, ctx, 0x08A76724u, {frame.at(0x28u), frame.at(k.path2)});
                call(rt, ctx, 0x08A8CE2Cu, {frame.at(k.output), visitor, frame.at(0x28u)});
                call(rt, ctx, 0x08A75A88u, {frame.at(0x28u)});
                if (word(rt, frame.at(k.output)) == 0u) {
                    const u32 again = word(rt, frame.at(k.held));
                    if (again == 0u) call(rt, ctx, 0x08A5BB7Cu, {k.output_null});
                    const u32 converted = call(rt, ctx, 0x08A7BCC0u, {again});
                    call(rt, ctx, 0x08A7B544u, {frame.at(k.alternate), converted, frame.at(k.held)});
                    const u32 wrapped = word(rt, frame.at(k.alternate));
                    if (wrapped == 0u) call(rt, ctx, 0x08A5BB7Cu, {k.wrapper_null});
                    call_virtual(rt, ctx, wrapped, 0x28u, {wrapped, visitor});
                    call(rt, ctx, 0x08A5B9ECu, {frame.at(k.alternate)});
                }
                call(rt, ctx, 0x08A76724u, {frame.at(k.cursor), frame.at(k.path2)});
                const u32 child = word(rt, current + 0x14u);
                if (child == 0u) call(rt, ctx, 0x08A5BB7Cu, {k.child_null});
                const u32 converted_child = call(rt, ctx, 0x08A5D444u, {child});
                call(rt, ctx, 0x08A9032Cu, {visitor, frame.at(k.cursor), converted_child, current + 0x10u});
                call(rt, ctx, 0x08A75A88u, {frame.at(k.cursor)});
                call(rt, ctx, 0x08A5B9ECu, {frame.at(k.output)});
                release_string(rt, ctx, frame.at(k.path2), frame.at(1u));
                call(rt, ctx, 0x08A5B9ECu, {frame.at(k.held)});
            }
        }
        current = call(rt, ctx, 0x08B5F100u, {current});
    }
    call(rt, ctx, 0x08A8C1C4u, {visitor});
    call(rt, ctx, 0x08C00860u, {frame.at(0x40u), word(rt, frame.at(0x48u))});
    release_string(rt, ctx, frame.at(k.path1), frame.at(0u));
}

// 0x08A192D4 — the walk of the children of a0->+0x14 for the visitor a1.

// 0x08A7B8C8 — the same body with its own constants (and its own frame slots).

// ---------------------------------------------------------------------------------------------------------------------
// 0x08B2D824 (a0 unused, a1 = an argument block whose first word it reads): a worker entry. It opens 0x08CB5454, prepares
// a stream, writes a header through the stream buffer, copies the block at 0x08D68D54 (0x08D68D50 bytes, 0x10000 at a
// time), reads it back in 0x10000-byte chunks (counting the chunks 0x08B36170 finds equal), writes the count twice,
// closes the stream and leaves the thread state 0x08D68CF8 at 1; v0 = 0x08C7AA68(0)'s answer.
struct SavedSlot {
    unsigned reg;
    u32 offset;
};
class SavedRegisters {
public:
    SavedRegisters(Runtime &rt, AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots)
        : ctx_(ctx) {
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
s32 smin(s32 a, s32 b) { return a < b ? a : b; }
u32 unaligned_word(Runtime &rt, u32 address) {
    return rt.memory().load8(address) | rt.memory().load8(address + 1u) << 8u |
           rt.memory().load8(address + 2u) << 16u | static_cast<u32>(rt.memory().load8(address + 3u)) << 24u;
}
void put_unaligned_word(Runtime &rt, u32 address, u32 value) {
    for (unsigned i = 0u; i < 4u; ++i) put8(rt, address + i, value >> (8u * i));
}


// ---------------------------------------------------------------------------------------------------------------------
// 0x08BA86F4 (a0 = vector {begin +0, end +4, capacity +8}, a1 = position, a2 = count, a3 = the value {word +0, control
// block +4}): libstdc++ vector<8-byte element>::insert. count == 0 does nothing and leaves v0 untouched; with room the
// value is copy-constructed in the frame, the tail moves up, and the frame element is released; otherwise a block of
// size + max(size, count) elements is built. The control block's strong count is at +4 (vtable +0, dispose at slot 8,
// free at slot 0xC), the weak count at +8.
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
void release_shared(Runtime &rt, AllegrexContext &ctx, u32 block) {
    if (!count_down(rt, ctx, block + 4u)) return;
    call_virtual(rt, ctx, block, 8u, {block});
    if (count_down(rt, ctx, block + 8u)) call_virtual(rt, ctx, block, 0xCu, {block});
}

void func_08BA86F4(Runtime &rt, AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4];
    const u32 position = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    put(rt, frame.at(0x20u), count);
    if (count == 0u) return;  // v0 is untouched, as the original leaves it
    const u32 end = word(rt, vector + 4u);
    const u32 capacity = word(rt, vector + 8u);
    const u32 room = capacity - end;
    if (static_cast<u32>(signed_of(room) >> 3) < count) {
        const u32 begin = word(rt, vector);
        const u32 size = static_cast<u32>(signed_of(end - begin) >> 3);
        put(rt, frame.at(8u), size);
        if (0x1FFFFFFFu - size < count) call(rt, ctx, 0x08B5ADDCu, {0x08CA0718u});
        const u32 grow = size < count ? count : size;
        const u32 total = size + grow;
        u32 bytes = 0xFFFFFFF8u;
        u32 block = 0u;
        if (total < size) {
            block = call(rt, ctx, 0x08B35DA0u, {bytes});
        } else {
            bytes = total << 3;
            if (total != 0u) block = call(rt, ctx, 0x08B35DA0u, {bytes});
        }
        const u32 front = call(rt, ctx, 0x08BA7F84u, {begin, position, block, frame.at(3u)});
        const u32 inserted = call(rt, ctx, 0x08BA7F34u, {front, count, value, frame.at(2u)});
        const u32 back =
            call(rt, ctx, 0x08BA7F84u, {position, word(rt, vector + 4u), inserted + (count << 3), frame.at(1u)});
        call(rt, ctx, 0x08BA76B8u, {word(rt, vector), word(rt, vector + 4u), frame.at(0u)});
        const u32 old_begin = word(rt, vector);
        if (old_begin != 0u) call(rt, ctx, 0x08B35254u, {old_begin});
        put(rt, vector + 8u, block + bytes);
        put(rt, vector + 4u, back);
        put(rt, vector, block);
        ctx.gpr[2] = block + bytes;
        return;
    }
    // With room: the value goes to the frame element first, holding one use of its control block.
    const u32 element = frame.at(0xCu);
    const u32 block = word(rt, value + 4u);
    put(rt, element + 4u, block);
    put(rt, element, word(rt, value));
    if (block != 0u) count_up(rt, ctx, block + 4u);
    const u32 after = static_cast<u32>(signed_of(word(rt, vector + 4u) - position) >> 3);
    if (count < after) {
        const u32 source_end = word(rt, vector + 4u) - (count << 3);
        call(rt, ctx, 0x08BA7FD4u, {source_end, word(rt, vector + 4u), word(rt, vector + 4u), frame.at(6u)});
        const u32 middle = static_cast<u32>(signed_of(source_end - position) >> 3);
        put(rt, vector + 4u, word(rt, vector + 4u) + (count << 3));
        if (signed_of(middle) > 0) {
            u32 source = source_end;
            u32 destination = word(rt, vector + 4u) - (count << 3);
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
        call(rt, ctx, 0x08BA7880u, {position, position + (count << 3), element});
    } else {
        call(rt, ctx, 0x08BA7E94u, {end, count - after, element, frame.at(5u)});
        const u32 new_end = word(rt, vector + 4u) + ((count - after) << 3);
        put(rt, vector + 4u, new_end);
        call(rt, ctx, 0x08BA7EE4u, {position, end, new_end, frame.at(4u)});
        put(rt, vector + 4u, word(rt, vector + 4u) + (after << 3));
        call(rt, ctx, 0x08BA7880u, {position, end, element});
    }
    const u32 element_block = word(rt, frame.at(0x10u));
    if (element_block != 0u) release_shared(rt, ctx, element_block);
}

// ---------------------------------------------------------------------------------------------------------------------
// 0x08BBAA74 (self, actor): returns with v0 = 0 when self->+8 (the world object) is null. Otherwise it sets two graphics
// states up, reads the actor's position (0x08AF26F0) and direction (0x08AF2724), passes the world's +0x68 to
// 0x08AF8904 and walks self->+0xC: for each element (virtual slot 8 is the count, slot 0xC fills an index half at frame
// +0x1C and the box/plane/point pointers at +0x20 / +0x28 / +0x24) the record world->+0xA4[world->+0x30[i].half(+0x2C)]
// is tested against the scale at 0x08CE9ECC (squared distance and dot product), and a passing record runs the graphics
// calls below; the world is closed with 0x08AF8434.

} // namespace
} // namespace new_batch_main_1016
} // namespace gt::game

#define NEW1016(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1016::name, gt::ReturnKind::kind)
NEW1016(0x08BA86F4u, func_08BA86F4, Word);
