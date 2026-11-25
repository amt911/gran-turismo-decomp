// New batch, main session 57: 80 functions the recompiler emitted, none decompiled before, written by hand as structured C
// from `tools.disasm`. Several families repeat one shape with other constants (a stream extraction with eight vtable slots, a
// scope lookup that stores or reads a float attribute, a tree walk that appends to a vector, two tree-node inserts, the
// byte-wise reader that advances a cursor); the helpers below hold them. v0 is left as the original leaves it on every path
// (a callee that never writes v0 hands the previous value on, so the call helpers start it the way the original's `jal` / `jalr`
// has it). No names: none has evidence. DRAFT: unverified, the differential gate has not run.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "gt/vfpu.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_57 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call as the compiler issues it: v0 / v1 / f0 keep the callee's values, so a callee that never writes v0 hands
// the value the caller had on to the code after the call.
GuestResult call_full(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {},
                      std::initializer_list<float> floats = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_full(rt, ctx, target, arguments).v0;
}
// `lw v1, (object); lw v0, slot(v1); jalr v0`: at the jalr v0 holds the target and v1 the vtable.
GuestResult call_virtual(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments,
                         std::initializer_list<float> floats = {}) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[3] = table;
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// `lw v0, (object); lw v1, slot(v0); jalr v1`: at the jalr v0 holds the vtable and v1 the target.
GuestResult call_virtual_in_v1(Runtime &rt, AllegrexContext &ctx, u32 object, u32 slot, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    const u32 target = word(rt, table + slot);
    ctx.gpr[2] = table;
    ctx.gpr[3] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
// A `jalr` to a target already in a register (v0 holds it at the call).
GuestResult call_target(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments,
                        std::initializer_list<float> floats = {}) {
    ctx.gpr[2] = target;
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u32 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<std::uint8_t>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<std::uint16_t>(value)); }
u32 signed_byte(u32 value) { return static_cast<u32>(static_cast<std::int8_t>(value)); }
u32 signed_half(u32 value) { return static_cast<u32>(static_cast<std::int16_t>(value)); }
float float_bits(u32 bits) { return std::bit_cast<float>(bits); }
// An unaligned little-endian word (`lwl` + `lwr`).
u32 unaligned_word(Runtime &rt, u32 address) {
    return byte_at(rt, address) | (byte_at(rt, address + 1u) << 8) | (byte_at(rt, address + 2u) << 16) | (byte_at(rt, address + 3u) << 24);
}

// The assert helper every "must not be null" check calls with a message.
constexpr u32 kAssert = 0x08A5BB7Cu;
// The scope closer every `{scope}` ends with.
constexpr u32 kCloseScope = 0x08A5B9ECu;

// --- 0x08C576B4 family: a stream extraction (a0 = the stream, a1 = the value); a sentry on the frame, then the stream's
// buffer's virtual `slot` reads into a 0x18-byte block and the state bits it set are or-ed in.
void stream_extract(Runtime &rt, AllegrexContext &ctx, u32 slot) {
    const u32 stream = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    call(rt, ctx, 0x08C55FDCu, {frame.at(0), stream, 0u});
    if (byte_at(rt, frame.at(0)) == 0u) {
        ctx.gpr[2] = stream;
        return;
    }
    put(rt, frame.at(0x04), 0u);
    const u32 base = stream + word(rt, word(rt, stream) - 0xCu);
    const u32 buffer = word(rt, base + 0x84u);
    if (buffer == 0u) {
        ctx.gpr[3] = 0xFFFFFFFFu;
        call(rt, ctx, 0x08B5A9FCu, {0u});
        return;
    }
    put(rt, frame.at(0x18), word(rt, base + 0x78u));
    put(rt, frame.at(0x14), 0xFFFFFFFFu);
    put(rt, frame.at(0x1C), 0xFFFFFFFFu);
    put(rt, frame.at(0x10), 0u);
    ctx.gpr[3] = 0xFFFFFFFFu;
    const u32 target = word(rt, word(rt, buffer) + slot);
    const GuestResult result = call_target(rt, ctx, target, {buffer, frame.at(0x18), frame.at(0x10), base, frame.at(0x04), value});
    put(rt, frame.at(0x08), result.v0);
    put(rt, frame.at(0x0C), result.v1);
    put(rt, frame.at(0x20), result.v0);
    put(rt, frame.at(0x24), result.v1);
    const u32 state = word(rt, frame.at(0x04));
    if (state != 0u) {
        const u32 target_stream = stream + word(rt, word(rt, stream) - 0xCu);
        call(rt, ctx, 0x08C54CFCu, {target_stream, word(rt, target_stream + 0x14u) | state});
    }
    ctx.gpr[2] = stream;
}

// --- a float attribute (0x089C2AC8 family): a0 = the object written, a2 > 0 sets, else reads; a3 = a holder (pointer to the
// object the virtual slot 0x54 asks). `open` fills a scope slot on the frame; the two assert messages are the null checks'.
void float_attribute(Runtime &rt, AllegrexContext &ctx, u32 open, u32 message_scope, u32 message_holder, u32 offset) {
    u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<s32>(count) > 0) {
        const u32 scope = frame.at(0x08);
        call(rt, ctx, open, {scope});
        self = word(rt, frame.at(0x08));
        if (self == 0u) call(rt, ctx, kAssert, {message_scope});
        holder = word(rt, holder);
        if (holder == 0u) call(rt, ctx, kAssert, {message_holder});
        const GuestResult result = call_virtual(rt, ctx, holder, 0x54u, {holder});
        put_float(rt, self + offset, result.f0);
        call(rt, ctx, kCloseScope, {scope});
        return;
    }
    call(rt, ctx, open, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) call(rt, ctx, kAssert, {message_scope});
    const float value = float_at(rt, object + offset);
    ctx.fpr[12] = value;
    const u32 second = frame.at(0x04);
    call_full(rt, ctx, 0x08A87EECu, {second}, {value});
    const u32 loaded = word(rt, frame.at(0x04));
    ctx.gpr[5] = loaded;
    if (self != second) call(rt, ctx, 0x08A5BAD8u, {self, loaded});
    call(rt, ctx, kCloseScope, {second});
    call(rt, ctx, kCloseScope, {frame.at(0)});
}

// --- 0x08A41554 family: walks a tree from its leftmost node to its header (a1 + the header offset), appending a value built
// from each node (node + 0x10) to the vector at a0 + 0x14 (begin +4, end +8, `capacity` end); returns a0.
void tree_to_vector(Runtime &rt, AllegrexContext &ctx, u32 header_offset, u32 first_offset) {
    const u32 vector_owner = ctx.gpr[4];
    const u32 tree = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const u32 header = tree + header_offset;
    call(rt, ctx, 0x08A98A70u, {vector_owner, tree});
    u32 node = word(rt, tree + first_offset);
    while (node != header) {
        const u32 owner = word(rt, vector_owner);
        const u32 vector = owner + 0x14u;
        if (owner == 0u) call(rt, ctx, kAssert, {0x08CADBF4u});
        call(rt, ctx, 0x08A6D2D4u, {frame.at(0), node + 0x10u});
        u32 end = word(rt, vector + 4u);
        const u32 limit = word(rt, vector + 8u);
        if (end != limit) {
            if (end != 0u) {
                call(rt, ctx, 0x08A76ABCu, {end, frame.at(0)});
                end = word(rt, vector + 4u);
            }
            put(rt, vector + 4u, end + 4u);
        } else {
            call(rt, ctx, 0x08B8A03Cu, {vector, end, frame.at(0)});
        }
        call(rt, ctx, kCloseScope, {frame.at(0)});
        node = call(rt, ctx, 0x08B5F16Cu, {node});
    }
    ctx.gpr[2] = vector_owner;
}

// --- 0x08828BA4 family: a scope lookup; a2 > 0 passes the object's flag (the virtual slot 0x3C of the holder at a3) to `set`,
// otherwise `get`'s answer is stored through a0 (a smart pointer on the frame, replaced when different).
void scope_flag(Runtime &rt, AllegrexContext &ctx, u32 set, u32 get) {
    const u32 out = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 scope = frame.at(0x04);
    call(rt, ctx, 0x088248E4u, {scope, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    u32 object = word(rt, frame.at(0x04));
    if (object == 0u) call(rt, ctx, kAssert, {0x08C99B4Cu});
    const u32 member = word(rt, object + 0x14u);
    call(rt, ctx, kCloseScope, {scope});
    if (static_cast<s32>(count) > 0) {
        holder = word(rt, holder);
        if (holder == 0u) call(rt, ctx, kAssert, {0x08C99B24u});
        const GuestResult result = call_virtual(rt, ctx, holder, 0x3Cu, {holder});
        call(rt, ctx, set, {member, result.v0 != 0u ? 1u : 0u});
        return;
    }
    const u32 value = call(rt, ctx, get, {member});
    call(rt, ctx, 0x08A97A6Cu, {frame.at(0), value});
    if (out != frame.at(0)) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, frame.at(0))});
    call(rt, ctx, kCloseScope, {frame.at(0)});
}

// --- 0x08BFE808 family: inserts a new node (key from a3, 0x18 bytes) into the tree whose header is this + 4 and returns it.
// `left` when the key is the smaller one or the parent is the header or `force` is set; `copy` is the second member's copy.
void tree_insert(Runtime &rt, AllegrexContext &ctx, u32 copy) {
    const u32 tree = ctx.gpr[4];
    const u32 force = ctx.gpr[5];
    const u32 parent = ctx.gpr[6];
    const u32 key = ctx.gpr[7];
    const u32 header = tree + 4u;
    u32 left = 1u;
    if (force == 0u && parent != header) left = word(rt, key) < word(rt, parent + 0x10u) ? 1u : 0u;
    const u32 node = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    const u32 payload = node + 0x10u;
    if (payload != 0u) {
        call(rt, ctx, 0x08A6972Cu, {payload, key});
        call(rt, ctx, copy, {node + 0x14u, key + 4u});
    }
    call(rt, ctx, 0x08B5F2B8u, {left, node, parent, header});
    put(rt, tree + 0x14u, word(rt, tree + 0x14u) + 1u);
    ctx.gpr[2] = node;
}

// --- 0x08C0C518 family: two pointer-to-member calls (the member descriptor is two words: an adjusted `this` shifted left by
// one with bit 0 = virtual, and the function or vtable offset). A source object is built, passed through the first call
// and the result to the second together with the new object on the frame.
u32 member_this(u32 object, u32 adjust) { return object + static_cast<u32>(static_cast<s32>(adjust) >> 1); }
u32 member_target(Runtime &rt, u32 self, u32 adjust, u32 pointer) {
    return (adjust & 1u) != 0u ? word(rt, word(rt, self) + pointer) : pointer;
}
void member_pair(Runtime &rt, AllegrexContext &ctx, u32 build, u32 construct, u32 close) {
    const u32 binder = ctx.gpr[4];
    u32 source = ctx.gpr[6];
    const u32 extra = ctx.gpr[8];
    GuestFrame frame(ctx, 0x30u);
    const u32 built = call(rt, ctx, build, {extra});
    call(rt, ctx, construct, {frame.at(0), extra + 4u});
    const u32 first_adjust = word(rt, binder + 0x18u);
    const u32 first_pointer = word(rt, binder + 0x14u);
    source = word(rt, source);
    const u32 first_self = member_this(source, first_adjust);
    const u32 first_target = member_target(rt, first_self, first_adjust, first_pointer);
    const GuestResult first = call_target(rt, ctx, first_target, {first_self, first_adjust, first_pointer, first_target});
    const u32 second_adjust = word(rt, binder + 0x20u);
    const u32 second_pointer = word(rt, binder + 0x1Cu);
    const u32 second_self = member_this(first.v0, second_adjust);
    const u32 second_target = member_target(rt, second_self, second_adjust, second_pointer);
    call_target(rt, ctx, second_target, {second_self, built, frame.at(0)});
    call(rt, ctx, close, {frame.at(0)});
}

// --- 0x088BA534 family: copies `size` bytes from the cursor at *a0 into a3 (a1) and advances the cursor: 1, 2, 4 and 8 bytes are
// read directly (the 2 as two bytes, the 4 and 8 unaligned), anything else goes to the library copy 0x08B79A88.
void read_and_advance(Runtime &rt, AllegrexContext &ctx) {
    const u32 cursor = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 size = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const u32 source = word(rt, cursor);
    if (size == 2u) {
        put_half(rt, destination, (byte_at(rt, source + 1u) << 8) | byte_at(rt, source));
    } else if (size == 1u) {
        put_byte(rt, destination, byte_at(rt, source));
    } else if (size == 4u) {
        put(rt, destination, unaligned_word(rt, source));
    } else if (size == 8u) {
        const u32 low = unaligned_word(rt, source);
        const u32 high = unaligned_word(rt, source + 4u);
        put(rt, destination, low);
        put(rt, destination + 4u, high);
    } else {
        ctx.gpr[5] = word(rt, cursor);
        call(rt, ctx, 0x08B79A88u, {destination, word(rt, cursor), size});
    }
    ctx.gpr[2] = word(rt, cursor) + size;
    put(rt, cursor, ctx.gpr[2]);
}

// 0x08C576B4 .. 0x08C57F58: the stream extraction with the buffer's virtual slot 0x30 / 0x2C / 0x28 / 0x24 / 0x20 / 0x1C / 0x18 / 0x0C.
void func_08C577F0(Runtime &rt, AllegrexContext &ctx) { stream_extract(rt, ctx, 0x2Cu); }
void func_08C57E1C(Runtime &rt, AllegrexContext &ctx) { stream_extract(rt, ctx, 0x18u); }
void func_08C57F58(Runtime &rt, AllegrexContext &ctx) { stream_extract(rt, ctx, 0x0Cu); }

// The two tree-to-vector walks, the two scope-flag lookups, the two tree inserts, the two member pairs, the two readers, the three float attributes.
void func_08BFE808(Runtime &rt, AllegrexContext &ctx) { tree_insert(rt, ctx, 0x08A5D424u); }
void func_08C25B2C(Runtime &rt, AllegrexContext &ctx) { tree_insert(rt, ctx, 0x08A76ABCu); }
void func_088BA534(Runtime &rt, AllegrexContext &ctx) { read_and_advance(rt, ctx); }
void func_088F19DC(Runtime &rt, AllegrexContext &ctx) { read_and_advance(rt, ctx); }

// 0x08AD3E70: the first time the counter at 0x08D404D0 reaches 1: 0x08AD354C(1) answers a number, a text is formatted from it
// with the format 0x08CB4A3C into a buffer on the frame, 0x08B10204 turns that into a 0x3C-byte block, which is copied to the
// bottom of the frame, and its word at +0x20 goes to 0x08D404CC and to 0x08AD3D7C. v0 = the lui leftover 0x08D40000 when the
// counter is not 1.
void func_08AD3E70(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x490u);
    const u32 count = word(rt, 0x08D404D0u) + 1u;
    ctx.gpr[2] = (0x08D404D0u >> 16) << 16;
    ctx.gpr[3] = count;
    ctx.gpr[4] = 1u;
    put(rt, 0x08D404D0u, count);
    if (count != 1u) return;
    const u32 number = call(rt, ctx, 0x08AD354Cu, {1u});
    call(rt, ctx, 0x08B37640u, {frame.at(0x40), 0x08CB4A3Cu, number});
    call(rt, ctx, 0x08B10204u, {frame.at(0x440), frame.at(0x40), 1u});
    for (u32 offset = 0u; offset < 15u * 4u; offset += 4u) put(rt, frame.at(offset), word(rt, frame.at(0x440u + offset)));
    const u32 value = word(rt, frame.at(0x20));
    put(rt, 0x08D404CCu, value);
    call(rt, ctx, 0x08AD3D7Cu, {value});
}

// 0x08B62934 (a0 = a cursor object, a1 = an offset, a2 = a value, a3 = an object, t0 / t1 / t2 = a (word, word, word) triple, t3 = the
// result block): when the words at +4 of a0 and a3 are equal the block gets (t0, a2, a state 1 or 6, or 1 for a1 == -2) and the answer is 0;
// when they differ and t0 == t2 and the first word equals the word at +4 of t1 the block gets a2 at +8 and the answer is 0; otherwise the
// virtual at slot 0x1C of the object at +8 of a0 answers.

// 0x08BAB554 (a0 = an object): stores the vtable 0x08CC1A10, tears down the three records at +0xF0, +0xA0 and +0x50 (each
// 0x08BAA4E0(first, second, frame) and, for a non-null first, 0x08B35254(first)), releases the object at +0xC through its virtual
// slot 0 (a1 = the word at +8) and clears +8 / +0xC, then stores the vtable 0x08CBF368 and ends in 0x08B64654(object).

// 0x08BAC46C (a0 = a vector of 0x20-byte elements, a1 = the destination, a2 = the first element to move): moves the elements from a2
// to the end down to a1 (0x1C bytes and a byte each), 0x08BAC45C(destination end, the old end, frame) closes the gap, the vector's end
// (+4) drops by the moved size rounded down to 0x20, and a1 is returned.
void func_08BAC46C(Runtime &rt, AllegrexContext &ctx) {
    const u32 vector = ctx.gpr[4];
    const u32 destination = ctx.gpr[5];
    const u32 first = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    const s32 count = static_cast<s32>(word(rt, vector + 4u) - first) >> 5;
    u32 to = destination;
    u32 from = first;
    for (s32 index = 0; index < count; ++index) {
        u32 values[7];
        for (u32 slot = 0u; slot < 7u; ++slot) values[slot] = word(rt, from + 4u * slot);
        for (u32 slot = 0u; slot < 7u; ++slot) put(rt, to + 4u * slot, values[slot]);
        put_byte(rt, to + 0x1Cu, byte_at(rt, from + 0x1Cu));
        from += 0x20u;
        to += 0x20u;
    }
    call(rt, ctx, 0x08BAC45Cu, {to, word(rt, vector + 4u), frame.at(0)});
    const u32 moved = (first - destination) & ~((1u << 5) - 1u);
    put(rt, vector + 4u, word(rt, vector + 4u) - moved);
    ctx.gpr[2] = destination;
}

// 0x08BDB598 (a0 = the base of 8-byte entries (word, signed word), a1 = the hole, a2 = the top, t0 / t1 = the value): the sift-up of a
// heap push; the entry whose (high, low) is smaller than the value moves down while the hole stays above the top, the value goes
// into the hole, v0 = the hole's address.
void func_08BDB598(Runtime &rt, AllegrexContext &ctx) {
    const u32 base = ctx.gpr[4];
    u32 hole = ctx.gpr[5];
    const s32 top = static_cast<s32>(ctx.gpr[6]);
    const u32 low = ctx.gpr[8];
    const s32 high = static_cast<s32>(ctx.gpr[9]);
    const auto parent_of = [](u32 index) {
        const s32 distance = static_cast<s32>(index - 1u);
        return static_cast<u32>((distance + static_cast<s32>(static_cast<u32>(distance) >> 31)) >> 1);
    };
    const auto smaller = [&](u32 entry_low, s32 entry_high) { return entry_high < high || (entry_high == high && entry_low < low); };
    if (top < static_cast<s32>(hole)) {
        u32 candidate = parent_of(hole);
        u32 entry_low = word(rt, base + candidate * 8u);
        s32 entry_high = static_cast<s32>(word(rt, base + candidate * 8u + 4u));
        while (smaller(entry_low, entry_high)) {
            put(rt, base + hole * 8u, entry_low);
            put(rt, base + hole * 8u + 4u, static_cast<u32>(entry_high));
            const u32 next = parent_of(candidate);
            hole = candidate;
            if (!(top < static_cast<s32>(candidate))) break;
            entry_low = word(rt, base + next * 8u);
            entry_high = static_cast<s32>(word(rt, base + next * 8u + 4u));
            candidate = next;
        }
    }
    put(rt, base + hole * 8u, low);
    put(rt, base + hole * 8u + 4u, static_cast<u32>(high));
    ctx.gpr[2] = base + hole * 8u;
}

// Erases the element at `erased` of a vector of `size`-byte elements (a0 = the vector, its end at +4): the later elements move down one place
// (every word but the first, the vtable; the count is the compiler's division of the byte distance by `size` through the multiplicative
// inverse `inverse` after the shift `shift`), the end drops by `size` and the virtual at slot 0 of the new last element runs on it (the
// destructor). Returns `erased`.
void erase_and_destroy(Runtime &rt, AllegrexContext &ctx, u32 size, unsigned shift, u32 inverse) {
    const u32 vector = ctx.gpr[4];
    const u32 erased = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    u32 end = word(rt, vector + 4u);
    const u32 first = erased + size;
    if (first != end) {
        const s32 count = static_cast<s32>(static_cast<u32>(static_cast<s32>(end - first) >> shift) * inverse);
        if (count > 0) {
            u32 from = first;
            u32 to = erased;
            for (s32 index = 0; index < count; ++index) {
                for (u32 offset = 4u; offset < size; offset += 4u) put(rt, to + offset, word(rt, from + offset));
                from += size;
                to += size;
            }
            end = word(rt, vector + 4u);
        }
    }
    const u32 last = end - size;
    put(rt, vector + 4u, last);
    call_virtual_in_v1(rt, ctx, last, 0u, {last});
    ctx.gpr[2] = erased;
}

// 0x08BF4814: erase_and_destroy for 0x28-byte elements.
// 0x08BECA44: the same for 0x30-byte elements.

// 0x08ACBACC (a0 = an object): 0x08B34888(object + 0xA8), then a state from 0x08C7B198; the flags at +0xA4 / +0xA5 and the answers of
// 0x08C7B188 / 0x08C7B1B0 / 0x08C7B1E8 / 0x08B34CA0 decide what runs before the tail call 0x08B34B24(object + 0xA8). v0 at the
// tail call is the register the asm holds on each path.

// 0x08AEC61C: 0x08C7AA20(the word at 0x08D41AD0 + 0x49C, 1, 0), then every 0x6C-byte record of the table at +0x4B0 (count at +0x4B4)
// whose word at +4 is non-zero has the words +4 + 4k copied to +0x38 + 4k for the set bits k = 1..12 of that flag word, the flag
// word or-ed into +0x38 and +4 cleared; 0x08C7AA08(the same word, 1) ends it and v0 = the word at +0x4BC.
void func_08AEC61C(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kTable = 0x08D41AD0u;
    GuestFrame frame(ctx, 0x10u);
    call(rt, ctx, 0x08C7AA20u, {word(rt, kTable + 0x49Cu), 1u, 0u});
    if (static_cast<s32>(word(rt, kTable + 0x4B4u)) > 0) {
        s32 index = 0;
        u32 offset = 0u;
        do {
            const u32 record = word(rt, kTable + 0x4B0u) + offset;
            const u32 flags = word(rt, record + 4u);
            if (flags != 0u) {
                const u32 pending = record + 4u;
                const u32 target = record + 0x38u;
                for (u32 bit = 1u; bit < 13u; ++bit) {
                    if (((flags >> bit) & 1u) != 0u) put(rt, target + 4u * bit, word(rt, pending + 4u * bit));
                }
                put(rt, target, word(rt, target) | flags);
                put(rt, pending, 0u);
            }
            ++index;
            offset += 0x6Cu;
        } while (index < static_cast<s32>(word(rt, kTable + 0x4B4u)));
    }
    const u32 result = word(rt, kTable + 0x4BCu);
    call(rt, ctx, 0x08C7AA08u, {word(rt, kTable + 0x49Cu), 1u});
    ctx.gpr[2] = result;
}

// The `ll/sc` steps of a reference count (single-threaded here, the host never preempts inside one): the decrement leaves v1 = the
// word's address and v0 = 1 (the successful `sc`); true when the old value was 1.
bool count_down(Runtime &rt, AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    ctx.gpr[3] = address;
    ctx.gpr[4] = old;
    ctx.gpr[2] = 1u;
    put(rt, address, old - 1u);
    return old == 1u;
}
void count_up(Runtime &rt, AllegrexContext &ctx, u32 address) {
    put(rt, address, word(rt, address) + 1u);
    ctx.gpr[3] = address;
    ctx.gpr[2] = 1u;
}

// 0x08B95120 (a0 = the owner, a1 = a tree node): releases the tree below a1 (the right child first, by recursion, then the node's two
// copy-on-write strings at +0x14 and +0x10 unless they are the shared empty rep 0x08D75298, which 0x08B93554 frees when their count
// was not positive, then the node 0x08B35254), going on with the left child (+8).
void func_08B95120(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kEmptyRep = 0x08D75298u;
    const u32 owner = ctx.gpr[4];
    u32 node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    while (node != 0u) {
        call(rt, ctx, 0x08B95120u, {owner, word(rt, node + 0xCu)});
        const u32 left = word(rt, node + 8u);
        u32 text = word(rt, node + 0x14u);
        u32 rep = text - 0xCu;
        if (rep != kEmptyRep) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0) call(rt, ctx, 0x08B93554u, {rep, frame.at(1)});
        }
        text = word(rt, node + 0x10u);
        rep = text - 0xCu;
        if (rep != kEmptyRep) {
            if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0) call(rt, ctx, 0x08B93554u, {rep, frame.at(0)});
        }
        call(rt, ctx, 0x08B35254u, {node});
        node = left;
    }
}

// 0x08BBE774 (a0 = a vector of 0x10-byte elements, a1 = the destination, a2 = the first element to move): the elements from a2 to the end
// move down to a1 (0x08B957D8(to, from) and the words +4, +8, +0xC), 0x08BBE6F0(destination end, old end, frame) closes the gap, the
// vector's end (+4) drops by the moved size rounded down to 0x10, v0 = a1.

// 0x08BCF9E4 (a0, a1, a2, a3, t0 = five values, t1 = a pair (object, shared holder)): the pair is copied to the frame with the holder's
// use count (+4) raised, 0x08961928(a1, a2, 0x088FEFC0, a0, a3, t0, frame) runs, and the copy's holder is released: use count down,
// at the last use its virtual slot 8, then the weak count (+8) down and at the last one slot 0xC.

// 0x08BF48E4 (a0 = a binder holding two member descriptors, a2 = a pointer to the source object, t0 = the extra object): like the
// 0x08C0C518 family but the second call is a tail call: 0x08AA2A30(extra) and the float 0x08AA2898(extra + 4) are taken, the first
// member runs on the source, and the second member (a0 = its object, a1 = the first answer of 0x08AA2A30, f12 = the float) is jumped to.

// 0x08C0EBC0 (a0 = a vector of 12-byte elements, a1 = the destination, a2 = the first element to move): the elements from a2 to the end
// are assigned down to a1 (0x08C0E654(to, from) when they differ), 0x08C0DCF0(destination end, old end, frame) closes the gap, the
// vector's end (+4) drops by the moved size rounded down to 4, v0 = a1.

// 0x088187AC (a0 = a smart pointer slot, a1 = a count, a2 = a holder): a1 > 0 asks the holder's virtual slot 0x3C, takes (answer - 5) modulo
// 7 (signed) and hands it to 0x08A85B1C on a scope on the frame whose result replaces a0's when different; otherwise 0x08A76AAC(scope,
// 0x08CF6ED8) does the same; the scope is closed (0x08A5B9EC).

// A copy-on-write string slot is released: the rep sits 0xC bytes before the data, its count at -4; unless it is the empty rep
// `empty`, 0x08B60804(&count, -1) steps the count and `release`(rep, block) frees it when the old count was not positive.
// v0 is the empty rep when that is what the slot held (the compare's constant), else the last call's.
void release_string(Runtime &rt, AllegrexContext &ctx, u32 text, u32 empty, u32 release, u32 block) {
    const u32 rep = text - 0xCu;
    if (rep == empty) {
        ctx.gpr[2] = empty;
        return;
    }
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu})) <= 0) call(rt, ctx, release, {rep, block});
}

// 0x08879CB8 (a0 = a holder): 0x08AB85F0 and 0x08AB856C fill / read a string on the frame, 0x08B77BFC turns it into a number; when the
// frame's word at +0xC is not positive 0x0886B4E8(*holder, 2) replaces the number. The object 0x0886B080(*holder) answers its virtual
// slot 8 with (object, number, 0, 0) when it exists, 0x08875524(holder) is handed to 0x0887BEF4(.., 0), 0x0886BFC0(*holder) runs and
// the string is released (the empty rep is 0x08D75298; freed by 0x08B93554).

// 0x08997A4C (a0 = an object): stores the vtable 0x08CCBE20 and 0x08CCC078 (+0x38), closes the scopes at +0x184 and +0x180, frees the
// two vectors at +0x164 and +0x158 (0x08BF0F4C(begin, end, frame), then 0x08A5BC54(begin, byte size, 4) when begin is set) unless the
// list head at +0x158 is the +0x170 sentinel, releases the object at +0x154 (0x08A5B930) and ends in 0x08994D78(object).
void func_08997A4C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    put(rt, self, 0x08CCBE20u);
    put(rt, self + 0x38u, 0x08CCC078u);
    call(rt, ctx, kCloseScope, {self + 0x184u});
    call(rt, ctx, kCloseScope, {self + 0x180u});
    const u32 list = self + 0x158u;
    if (list != 0u && list != self + 0x170u) {
        u32 vector = self + 0x164u;
        for (u32 left = 2u; left != 0u; --left) {
            call(rt, ctx, 0x08BF0F4Cu, {word(rt, vector), word(rt, vector + 4u), frame.at(0)});
            const u32 begin = word(rt, vector);
            const u32 bytes = static_cast<u32>(static_cast<s32>(word(rt, vector + 8u) - begin) >> 2) << 2;
            if (begin != 0u) call(rt, ctx, 0x08A5BC54u, {begin, bytes, 4u});
            vector -= 0xCu;
        }
    }
    const u32 member = word(rt, self + 0x154u);
    if (member != 0u) call(rt, ctx, 0x08A5B930u, {member});
    call(rt, ctx, 0x08994D78u, {self});
}

// 0x089C10C8 (a0 = an object): 0x08B344E0(object + 0x2B0), a string 0x089C15FC(scope, 0x08CF6ED8) replaces the one at +0x264, a string
// is built on the frame (0x08B85EB8(frame, 0x08C8CAE8, frame + 1)) and passed with object + 0x230 to 0x08B855E0, released (empty rep
// 0x08D74FD0, freed by 0x08B8542C), and 0x08B3457C(object + 0x2B0) ends it.

// 0x089C5AD8 (a0 = an object, a1 = a pointer to a key): under the lock 0x08B344E0(0x08CF4E98) walks the list from the node at +0x54 to the
// sentinel at +0x4C; a node whose member (+0x14, null-checked) answers 0x089F0B0C(member, *key) non-zero ends the walk with
// v0 = 0x089F0B28(member, *key); no node gives 0. The lock is released (0x08B3457C) before returning.

// 0x08A14418 (a0 = an object): byte +0x10C = 1, word +0x108 = 0, 0x08B28C74(object + 0xD8), 0x08B2E224 / 0x08B2E28C(*(object + 0x104)),
// a scope 0x089C9C34(frame) whose object (null-checked) gives an object through 0x089C4FD4 that gets byte +0x1B4 = 1; then 0x08B1C47C,
// 0x08A1AA24(object + 0x18, 0), 0x08AC0090 and 0x08AC00D8 (object + 0x18), the virtual slot 0x128 of the 0x089C4FD4 object, 0x089C5E4C(scope
// object) and the scope is closed.

// 0x08A37510 (a0, a1, a2 = a count, a3 = a holder): a2 > 0: a scope 0x08A36FD0(scope, a1, a2, a3) gives an object (null-checked), the
// holder's object (+0, null-checked) answers its virtual slot 0xC into a string on the frame (a0 = the string, a1 = the object),
// 0x08A36E44(scope object, string) takes it, the string is released (empty rep 0x08D74FD0, freed by 0x08B8542C) and the scope closed.
// a2 <= 0 does nothing.
void func_08A37510(Runtime &rt, AllegrexContext &ctx) {
    const u32 a1 = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    if (static_cast<s32>(count) <= 0) return;
    const u32 scope = frame.at(8);
    const u32 text = frame.at(4);
    call(rt, ctx, 0x08A36FD0u, {scope, a1, count, holder});
    const u32 scoped = word(rt, frame.at(8));
    if (scoped == 0u) call(rt, ctx, kAssert, {0x08CAD0CCu});
    holder = word(rt, holder);
    if (holder == 0u) call(rt, ctx, kAssert, {0x08CAD0C4u});
    call_virtual(rt, ctx, holder, 0xCu, {text, holder});
    call(rt, ctx, 0x08A36E44u, {scoped, text});
    release_string(rt, ctx, word(rt, frame.at(4)), 0x08D74FD0u, 0x08B8542Cu, frame.at(0));
    call(rt, ctx, kCloseScope, {scope});
}

// 0x08A4FB80 (a0 = an object): 0x08A4F8B4(object) gives a handle (0 ends with 0); 0x08C7AC78(handle, frame) must answer 0 (else 0); then
// 0x08A4F76C(0 .. object, handle) and the object's four bytes +4..+7 are filled from the frame's record (the halfword +0x50 and its low
// byte, the halfword +0x52, the two floats +0x30 and -(+0x34) truncated plus one), the width byte rounded up to a multiple of 4, and the
// halfword +0x12 = ((width / 2) * the byte at +5 + 15) & ~15; v0 = 1.

// 0x08A1EC18 (a0 = a buffer, a1 = a length, a2 / a3 / t0 / t1 = four output words, all cleared): 0x08AF6CE8 must answer non-zero (else 0);
// the length is reduced by the word at +0x20 of the buffer when that is set (the reduction is the tail), the head (length - tail bytes)
// goes to a block from 0x08B2B5E0 (copied by 0x08B79A88) and its pointer / size to a2 / a3, the tail, when there is one, to a block from
// 0x08B59AC8 (pointer in t0, size in t1); v0 = 1.
void func_08A1EC18(Runtime &rt, AllegrexContext &ctx) {
    const u32 buffer = ctx.gpr[4];
    u32 length = ctx.gpr[5];
    const u32 head_out = ctx.gpr[6];
    const u32 head_size_out = ctx.gpr[7];
    const u32 tail_out = ctx.gpr[8];
    const u32 tail_size_out = ctx.gpr[9];
    GuestFrame frame(ctx, 0x20u);
    put(rt, head_out, 0u);
    put(rt, head_size_out, 0u);
    put(rt, tail_out, 0u);
    put(rt, tail_size_out, 0u);
    if (call(rt, ctx, 0x08AF6CE8u, {buffer, length, head_out, head_size_out, tail_out, tail_size_out}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 reduction = word(rt, buffer + 0x20u);
    const u32 tail = reduction != 0u ? length - reduction : 0u;
    length -= tail;
    put(rt, head_size_out, length);
    const u32 head = call(rt, ctx, 0x08B2B5E0u, {length});
    put(rt, head_out, head);
    call(rt, ctx, 0x08B79A88u, {head, buffer, word(rt, head_size_out)});
    if (tail != 0u) {
        const u32 block = call(rt, ctx, 0x08B59AC8u, {tail});
        put(rt, tail_out, block);
        put(rt, tail_size_out, tail);
        call(rt, ctx, 0x08B79A88u, {word(rt, tail_out), buffer + length, tail});
    }
    ctx.gpr[2] = 1u;
}

// 0x08AB4168 (a0 = the result string slot, a1 = a value, a2 = a holder): the holder's object (null-checked) answers its virtual slot 0x3C,
// 0x08AB4090(string A, a1, answer) fills a string on the frame, 0x08A6D2D4 copies it, 0x08A76ABC(a0, copy) stores it into a0's slot, the
// copy's scope is closed, the first string is released (empty rep 0x08D74FD0, freed by 0x08B8542C); v0 = a0.

// 0x08AC8FDC (a2 = a name): 0x08AD337C(0x08D00000) gives an object (0 ends with 0); 0x08B10204(frame + 0x40, name, 1) builds a 0x3C-byte
// block that is copied to the bottom of the frame; when its first word is zero 0x08AD35C8(the word at +0x20) runs and the object gets
// bytes +0xC / +0xD = 1, +4 = that word, +8 = the word at +0x10; v0 = 1 then, else 0.
void func_08AC8FDC(Runtime &rt, AllegrexContext &ctx) {
    const u32 name = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    ctx.gpr[4] = 0x08D00000u;
    const u32 object = call(rt, ctx, 0x08AD337Cu, {0x08D00000u});
    if (object == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call(rt, ctx, 0x08B10204u, {frame.at(0x40), name, 1u});
    for (u32 offset = 0u; offset < 15u * 4u; offset += 4u) put(rt, frame.at(offset), word(rt, frame.at(0x40u + offset)));
    if (word(rt, frame.at(0)) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 member = word(rt, frame.at(0x20));
    call(rt, ctx, 0x08AD35C8u, {member});
    put_byte(rt, object + 0xDu, 1u);
    put(rt, object + 4u, member);
    put(rt, object + 8u, word(rt, frame.at(0x10)));
    put_byte(rt, object + 0xCu, 1u);
    ctx.gpr[2] = 1u;
}

// 0x08B9355C (a0 = the owner, a1 = a tree node): like 0x08B95120 but each node also releases a shared holder (+0x18): the weak count
// (+8) goes down and at the last one its virtual slot 0xC runs; then the string at +0x10 (empty rep 0x08D75298, freed by 0x08B93554) and
// the node (0x08B35254); the left child (+8) goes on, the right child (+0xC) by recursion first.
void func_08B9355C(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    u32 node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    while (node != 0u) {
        call(rt, ctx, 0x08B9355Cu, {owner, word(rt, node + 0xCu)});
        const u32 shared = word(rt, node + 0x18u);
        const u32 left = word(rt, node + 8u);
        if (shared != 0u && count_down(rt, ctx, shared + 8u)) call_virtual(rt, ctx, shared, 0xCu, {shared});
        release_string(rt, ctx, word(rt, node + 0x10u), 0x08D75298u, 0x08B93554u, frame.at(0));
        call(rt, ctx, 0x08B35254u, {node});
        node = left;
    }
}

// 0x08BE5DC0 (a0 = a vector of 0x28-byte elements, a1 = the destination, a2 = the first element to move): the elements from a2 to the end
// move down to a1 (every word but the first), 0x08BE593C(destination end, old end, frame) closes the gap, the vector's end (+4) drops by
// the moved size rounded down to 8, v0 = a1.

// 0x08BEDF34 (a0 = a circular list head, a1 = a key, a2 = a value): every node whose member (+8, null-checked) passes 0x08A76F20(member, key)
// gets a new 0xC-byte record (0x08A5BC20(0xC, 4); its +8 built by 0x089FDAC8(record + 8, node + 8), then 0x08B5C078(record, value)) and
// the pair (list, node) goes to 0x08BEDE24; v0 is the last call's.

// 0x08C0D458 (a0 = a vector of 0x40-byte elements (begin, end +4, capacity end +8), a1 = the position, a2 = the value): when the vector is
// full or the position is not the end 0x08C0D100(vector, position, value) inserts in the middle (the grown vector's begin is reloaded);
// otherwise the value is built at the end (the words +4..+0x1C copied, the vtable 0x08CCFC90, 0x089FA280 on the +0x20 and +0x30 parts)
// and the end moves up by 0x40. v0 = begin + the position's index (from the begin before the call) * 0x40.

// 0x08C20910 (a0 = an object, a1 = a target): a scope 0x08A813F4(frame) is filled; 0x08A6159C(target, pairs * 2) gives an array and the
// scope object's (null-checked) virtual slot 0xA0 is called with (object, array entry, entry + 4) for every second index below the
// object's word at +8 doubled; 0x08A61584(target, scope) and the scope's close end it.

// 0x08C49C28 (a0 = a vector of 0x54-byte elements, a1 = the destination, a2 = the first element to move): the elements from a2 to the end
// move down to a1 (21 words each), 0x08C47024(destination end, old end, frame) closes the gap, the vector's end (+4) drops by the moved size
// rounded down to 4, v0 = a1.

// 0x0885F46C (a0 = a holder of an object): when the byte +0x5AA of the object is set 0x08893664(object + 0x600) runs and v0 = 1. Otherwise
// the object at +0x5AC is asked 0x088D7158(that, 1) (its answer is the result) and, after 0x08893664(object + 0x600), 0x08B1C47C and
// 0x08889CF0(object + 0x4584), 0x088DA0BC(that) is polled up to 50 times with 0x08B2E2B8(10000) between; then 0x08917AB0(object + 0x5CC) and
// 0x088DA0BC is polled with 0x08B2E2B8(100000) between until it answers zero.
void func_0885F46C(Runtime &rt, AllegrexContext &ctx) {
    const u32 holder = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    u32 object = word(rt, holder);
    if (byte_at(rt, object + 0x5AAu) != 0u) {
        call(rt, ctx, 0x08893664u, {object + 0x600u});
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 worker = word(rt, object + 0x5ACu);
    const u32 answer = call(rt, ctx, 0x088D7158u, {worker, 1u});
    call(rt, ctx, 0x08893664u, {word(rt, holder) + 0x600u});
    call(rt, ctx, 0x08B1C47Cu);
    call(rt, ctx, 0x08889CF0u, {word(rt, holder) + 0x4584u});
    for (u32 left = 0x32u; left != 0u;) {
        const u32 busy = call(rt, ctx, 0x088DA0BCu, {worker});
        --left;
        if (busy == 0u) break;
        call(rt, ctx, 0x08B2E2B8u, {0x2710u});
    }
    call(rt, ctx, 0x08917AB0u, {word(rt, holder) + 0x5CCu});
    while (call(rt, ctx, 0x088DA0BCu, {worker}) != 0u) call(rt, ctx, 0x08B2E2B8u, {0x10000u | 0x86A0u});
    ctx.gpr[2] = answer;
}

// 0x08936FAC (a0 = an object): stores the vtable 0x08CC67B8; the 0x2C pointers at +0x8C + 4i and the 0x2C at +0x13C + 4i each, when set,
// have their virtual slot 4 called (the destructor); then the objects at +0x1EC and +0x1F0 likewise, 0x08938FCC(object + 0x2D0) and
// 0x08938FCC(object + 0x1F4), and the tail call 0x08B64654(object).

// 0x089FB48C (a0 = an index, a1 = a value, a2 = an element): the table 0x089F9BFC() holds 12-byte entries; entry a0 is assigned from a2 (0x08C0E654 when
// they differ), 0x089F9E54() gives an object, 0x08B88F44(frame + 0xC, value) builds a string, 0x08C0DEF8(object, string) answers (v0, v1),
// which are stored in the frame (+4, +0x20, +0x24 and the byte +8), and the string is released (empty rep 0x08D74FD0, freed by 0x08B8542C).

// 0x08A083F0 (a0 = a smart pointer slot, a2 = a count, a3 = a holder): a scope 0x08A08228(frame + 4) is filled; for a2 > 0 the holder's object
// (null-checked) answers its virtual slot 0x3C (else -1); the scope's object (null-checked) answers its slot 0x1B0 with that value as a float,
// which 0x08A87EEC(frame, f12) turns into a string that replaces a0's when different; both scopes are closed.

// 0x08ABECA0 (a0 = an object): under the lock 0x08B344E0(object + 0x11C), each of the 16 records of 0x10 bytes at +0x1C has its word +4
// cleared and, when its table (+0xC) and its block (+8) are set and its count (+0) is non-zero, the table's entries i get the address
// block + 32 * i; 0x08B3457C(the lock), 0x08ABF6E0(object + 0x144, 0) and four zero words at +0xC..+0x18 follow; v0 = 4.
void func_08ABECA0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 lock = self + 0x11Cu;
    call(rt, ctx, 0x08B344E0u, {lock});
    u32 record = self + 0x1Cu;
    for (u32 index = 0u; index < 0x10u; ++index, record += 0x10u) {
        put(rt, record + 4u, 0u);
        if (word(rt, record + 0xCu) == 0u || word(rt, record + 8u) == 0u || word(rt, record) == 0u) continue;
        for (u32 entry = 0u; entry < word(rt, record); ++entry) {
            put(rt, word(rt, record + 0xCu) + 4u * entry, word(rt, record + 8u) + (entry << 5));
        }
    }
    call(rt, ctx, 0x08B3457Cu, {lock});
    call(rt, ctx, 0x08ABF6E0u, {self + 0x144u, 0u});
    for (u32 index = 0u; index < 4u; ++index) put(rt, self + 0xCu + 4u * index, 0u);
    ctx.gpr[2] = 4u;
}

// 0x08BBF7E8 (a0 = the first source element, a1 = the end, a2 = the destination): copy-constructs 0x14-byte elements (0x08B9432C(to, from) for
// the first part, then a vector of 16-byte items: its block from 0x08B35DA0, filled by 0x08BBEC68(begin, end, block, frame), the three
// pointers at +4 / +8 / +0xC, and the word +0x10); the destination after the last element is returned.

// 0x0880E0F8 (a0 = an object, a1 / a2 = two values, a3 = an index, f12 / f13 / f14 = floats): the object's table (word +0) holds 16-byte rows;
// the row (a3 - (trunc(float +8 of the table) - 1)) is passed to 0x089DB078(a1, a2, a2, a3', row floats); then 0x089DAEA0(a1, a2, f12 =
// the first float, f13 = the second, f14 = row[2] * the table's first float * the third float, f15 = the third float * (row[3] * the table's
// second float)) runs and f0 = f14 + the first float.
void func_0880E0F8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 a1 = ctx.gpr[5];
    const u32 a2 = ctx.gpr[6];
    u32 index = ctx.gpr[7];
    const float first = ctx.fpr[12];
    const float second = ctx.fpr[13];
    const float third = ctx.fpr[14];
    GuestFrame frame(ctx, 0x30u);
    u32 row = word(rt, self);
    const u32 whole = ctx.fpu_float_to_word(float_at(rt, row + 8u), 1u);
    index = (index - (whole - 1u)) << 4;
    row += index;
    call_full(rt, ctx, 0x089DB078u, {a1, a2, a2, index}, {float_at(rt, row), float_at(rt, row + 4u), float_at(rt, row + 8u), float_at(rt, row + 0xCu)});
    const u32 table = word(rt, self);
    float scaled = psp_mul(float_at(rt, row + 8u), float_at(rt, table));
    float other = psp_mul(float_at(rt, row + 0xCu), float_at(rt, table + 4u));
    scaled = psp_mul(scaled, third);
    other = psp_mul(third, other);
    call_full(rt, ctx, 0x089DAEA0u, {a1, a2}, {first, second, scaled, other});
    ctx.fpr[0] = scaled + first;
}

// 0x08833574 (a0 = a smart pointer slot): a scope 0x088297A8(frame + 4) gives an object (null-checked) turned into another by 0x088294D0, the
// scope is closed, 0x0885EE70 / 0x08891644 lead to an object whose length 0x088920A0 answers; for a positive length a block of that size
// (0x08B2B5E0) is filled from the data 0x088914A0 returns (0x08B79A88), 0x08A44D9C(frame, block, length, 1) builds a string that replaces a0's
// when different and is closed.

// 0x08834104 (a0 = a smart pointer slot, a2 = a count, a3 = a holder): a scope 0x088297A8 gives an object (null-checked) turned into another by
// 0x088294D0 and the scope is closed; for a2 > 0 the holder's object (null-checked) answers its virtual slot 0x3C (else 0);
// 0x0885E22C(object, answer) gives a value that 0x08A97A6C(frame, value) turns into a string replacing a0's when different, then closed.

// A word or halfword attribute read or written through a scope (0x0884C8E8 and 0x089C0450): a2 > 0 stores the holder's (a3, null-checked)
// virtual slot 0x3C answer at `offset` of the scope's object (`open` fills the scope slot), otherwise the value at `offset` (a signed halfword
// or a word) goes to 0x08A85B1C and the string it builds replaces a0's when different.
enum class Width { Half, Word };
void int_attribute(Runtime &rt, AllegrexContext &ctx, u32 open, u32 message_scope, u32 message_holder, u32 offset, Width width) {
    const u32 self = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    if (static_cast<s32>(count) > 0) {
        const u32 scope = frame.at(8);
        call(rt, ctx, open, {scope});
        const u32 object = word(rt, frame.at(8));
        if (object == 0u) call(rt, ctx, kAssert, {message_scope});
        holder = word(rt, holder);
        if (holder == 0u) call(rt, ctx, kAssert, {message_holder});
        const u32 answer = call_virtual(rt, ctx, holder, 0x3Cu, {holder}).v0;
        if (width == Width::Half) put_half(rt, object + offset, answer);
        else put(rt, object + offset, answer);
        call(rt, ctx, kCloseScope, {scope});
        return;
    }
    call(rt, ctx, open, {frame.at(0)});
    const u32 object = word(rt, frame.at(0));
    if (object == 0u) call(rt, ctx, kAssert, {message_scope});
    const u32 value = width == Width::Half ? signed_half(half_at(rt, object + offset)) : word(rt, object + offset);
    call(rt, ctx, 0x08A85B1Cu, {frame.at(4), value});
    if (self != frame.at(4)) call(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(4))});
    call(rt, ctx, kCloseScope, {frame.at(4)});
    call(rt, ctx, kCloseScope, {frame.at(0)});
}

// 0x0884C8E8: the halfword at +0x22.
// 0x089C0450: the word at +0x22C + 0x1C.

// 0x0885017C (a0 = a smart pointer slot): a scope 0x088547C4(frame + 4) gives an object (null-checked) turned by 0x08854678 and 0x088A5D0C into
// another; a 16-byte record from 0x08B2B5E0(0x10) takes its words +0x248 / +0x24C / +0x250 / +0x254, 0x08A52AB8(frame, the word at
// 0x08CE2CFC, record, -1, 0x0884F734, 0) builds a string that replaces a0's when different; the counter at 0x08CE2CEC goes up (v0 = new value).

// 0x088A2934 (a0 = an object, a1 = the output block): under the lock 0x08B344E0(*(object + 4) + 4) a set object (+8) has the 13 words at
// +0x10 (or at +0x44 when the byte +0x78 is set) copied to the block and v0 = 1; the lock is released (0x08B3457C); 0 otherwise.
void func_088A2934(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    u32 result = 0u;
    call(rt, ctx, 0x08B344E0u, {word(rt, self + 4u) + 4u});
    if (word(rt, self + 8u) != 0u) {
        const u32 source = self + (byte_at(rt, self + 0x78u) == 0u ? 0x10u : 0x44u);
        for (u32 offset = 0u; offset < 13u * 4u; offset += 4u) put(rt, out + offset, word(rt, source + offset));
        result = 1u;
    }
    call(rt, ctx, 0x08B3457Cu, {word(rt, self + 4u) + 4u});
    ctx.gpr[2] = result;
}

// 0x088BE160 (a0 = an object): the record at +0x24 (a0's) and the table 0x088C50B8(frame, the word +0x1C, the byte +0x10 of the record) are
// walked: the previous index (the record's byte +0x11 less one, wrapped by the count 0x088BB57C left in s1) is looked up with 0x088C50C4; when
// that entry's signed byte +2 is not below -126 the record's byte +0x12 takes the byte +0x13 and the float +0x18 the float +0x1C; then
// 0x088C5324 (the byte +0x11, or 0 when equal to the count) gives an index whose entry's byte becomes the record's +0x13: 0 stores the
// float at +0x1360 of the object at +4, 1 or 2 store 0x088BDEE0(object, index), larger ones leave the float.

// 0x08A7FBE8 (a0 = a smart pointer slot): a scope 0x08A7C3A4(frame + 4) gives an object (null-checked), 0x08A7F8E8(frame + 8, object, 0, 0x08CB14B4)
// builds a string, 0x08A6D2D4 copies it into the scope at +0xC which replaces a0's when different; the copy's scope is closed, the first
// string released (empty rep 0x08D74FD0, freed by 0x08B8542C) and the first scope closed.

// 0x08AC79B0 (a0 = a pair pointer (word, word), a1 = a value): the six 0x1C-byte slots at 0x08CFFD90 are tried in order, each under its lock
// (0x08B34888 / 0x08B34B24 of the word +8); the first whose byte +0xD is clear is taken (byte = 1, the pair to +0x10 / +0x14, the value to
// +0x18, 0x08B28C74(the word +4)) and its word +0 is returned; when none is free 0x08B1B92C(a0, a block {0, 0x3000} on the frame, a1, 0) answers.

// 0x08AE740C (a0 = an object): 0x08AE6E4C(object + 0x14), then for each of the signed byte +0x36 halfwords at +0x38 with a non-negative value
// 0x08AEF874(value, 3, 0, 0) and 0x08AEF1A8(value); the signed halfword +0x34, when not negative, goes to 0x08ADC230; the object is cut out
// of its list (+0 / +4 links, +0xC cleared) and pushed on the list headed at 0x08D409A4; v0 = 0x08D409A4.

// 0x08B043D8 (a0 = a pointer to an object): six slots of the object (+0x98, +0x1C, +0xC, +0x50, +0x6C, +0x88) are reset to
// (halfword -1, halfword -1, word 0) and the six vectors (+0x2C, +0x38, +0, +0x44, +0x60, +0x7C) are handed, each with its begin and its
// end (+4), to 0x08C49930, 0x08C49DC0, 0x08C49A28, 0x08C49B00, 0x08C49C28 and, as the tail call, 0x08C49D08.

// 0x08B2BB74 (a0 = an object): after 0x08B2BB10(object) the blocks of the table at +0x44 (count +0x48, 8 bytes each) are compacted from the
// cursor at +0x2C: a block (the table word +4) that is not at the cursor either becomes a free gap (its header word odd: the gap's size
// word and a link, chained from +0x38) or is moved to the cursor by 0x08B79BAC(cursor, block, header + 4) (the table entry then points at
// the cursor); the cursor then steps over the block at it (size word rounded down to 4, plus 4). The cursor ends in +0x3C.

// 0x08B4211C (a0 = an object): under the lock 0x08B344E0(object + 0x12C): 0x08B3EAE4(object + 0x154); when the virtual slot 0xA4 answers non-zero
// slot 0x40 runs; the object at +0xB0, when set, gets its slots 0xC and 0 called and is given back (0x08B2CE7C(object + 0xCC, it)) with +0xB0
// cleared; 0x08B41AF4(object, -1), 0x08B3EBFC(object + 0x8DC), the object at +0x930 (when set) given back and cleared, 0x08B404C8 and 0x08B3FF98
// (object) and the tail call 0x08B3457C(the lock).

// 0x08B61E98 (a0 = a size): an allocation of size + 0x50 bytes (0x08B35DA0); when it fails the interrupts are suspended (0x08C7A888, the state
// kept at 0x08D6E198) and, for sizes up to 0x200 + 0x50, the first clear bit of the pool mask at 0x08D6E194 gives a 0x200-byte pool block at
// 0x08D6E1A0 + 512 * bit (the bit is set); the interrupts are restored (0x08C7A8A8) and without a block 0x08B6459C runs. A block is counted
// (the word +4 of 0x08B62100()), cleared (0x08B36018(block, 0, 0x50)) and block + 0x50 is returned.
void func_08B61E98(Runtime &rt, AllegrexContext &ctx) {
    const u32 size = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const u32 total = size + 0x50u;
    u32 block = call(rt, ctx, 0x08B35DA0u, {total});
    if (block == 0u) {
        const u32 state = call(rt, ctx, 0x08C7A888u);
        put(rt, 0x08D6E198u, state);
        if (total < 0x201u) {
            const u32 mask = word(rt, 0x08D6E194u);
            u32 remaining = mask;
            u32 bit = 0u;
            while ((remaining & 1u) != 0u) {
                ++bit;
                if (bit == 0x20u) break;
                remaining >>= 1;
            }
            if (bit != 0x20u) {
                put(rt, 0x08D6E194u, mask | (1u << bit));
                block = 0x08D6E1A0u + (bit << 9);
            }
        }
        call(rt, ctx, 0x08C7A8A8u, {state});
        if (block == 0u) {
            call(rt, ctx, 0x08B6459Cu);
            return;
        }
    }
    const u32 counters = call(rt, ctx, 0x08B62100u);
    put(rt, counters + 4u, word(rt, counters + 4u) + 1u);
    call(rt, ctx, 0x08B36018u, {block, 0u, 0x50u});
    ctx.gpr[2] = block + 0x50u;
}

// 0x08BAAAE0 (a0 = the first 0xAC-byte element, a1 = the end, a2 = the destination): copy-constructs the elements (0xA4 bytes, then the vtable
// 0x08CC3528 at +0xA4 and the word +0xA8 of the source at +0xA8); null destinations are skipped; the destination after the last is returned.

// 0x08BBF46C (a0 = a vector of 0x14-byte elements, a1 = the destination, a2 = the first element to move): the elements from a2 to the end move
// down to a1 (0x08B957D8(to, from), 0x08BBECF0(to + 4, from + 4), the word +0x10), 0x08BBF3A8(destination end, old end, frame) closes the gap,
// the vector's end (+4) drops by the moved size rounded down to 4, v0 = a1.

// 0x08BDEE7C (a0 = a vector of 0x1C-byte elements, a1 = the destination, a2 = the first element to move): like 0x08BBF46C: the word +0, then
// 0x08BDDFDC(to + 4, from + 4) and 0x08BDEC3C(to + 0x10, from + 0x10); 0x08BDDCC8(destination end, old end, frame) closes the gap.

// 0x08C19A58 (a0 = a smart pointer slot, t0 = a member descriptor pair (pointer, adjusted this)): a scope 0x08A23C8C(frame + 4) is filled; the
// member runs on the scope's object (adjusted, a virtual one looked up) with a0 = a string slot on the frame; 0x08A6D2D4 copies the string
// into another that replaces a0's when different (0x08A5BAD8) and is closed; the first string is released (empty rep 0x08D74FD0, freed by
// 0x08B8542C) and the scope closed.

// 0x08C286C4 (a0 = a tree (leftmost +0xC, header +4, root +8, count +0x14), a1 = the first node, a2 = the end): erasing the whole tree resets
// it (0x08C28648(tree, root) frees the nodes; header links, root and count cleared); otherwise the nodes from a1 up to a2 are unlinked
// one by one (0x08B5F100 the successor, 0x08B5F444(node, header) the unlink, the node's two scopes at +0x14 / +0x10 closed, the node freed
// by 0x08A5BC54(node, 0x18, 4), the count down by one).

} // namespace
} // namespace new_batch_main_57
} // namespace gt::game

#define NEW_MAIN57(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_57::name, gt::ReturnKind::kind)
NEW_MAIN57(0x08C577F0u, func_08C577F0, Word);
NEW_MAIN57(0x08C57E1Cu, func_08C57E1C, Word);
NEW_MAIN57(0x08C57F58u, func_08C57F58, Word);
NEW_MAIN57(0x08BFE808u, func_08BFE808, Word);
NEW_MAIN57(0x08C25B2Cu, func_08C25B2C, Word);
NEW_MAIN57(0x088BA534u, func_088BA534, Word);
NEW_MAIN57(0x088F19DCu, func_088F19DC, Word);
NEW_MAIN57(0x08AD3E70u, func_08AD3E70, Word);
NEW_MAIN57(0x08BAC46Cu, func_08BAC46C, Word);
NEW_MAIN57(0x08BDB598u, func_08BDB598, Word);
NEW_MAIN57(0x08AEC61Cu, func_08AEC61C, Word);
NEW_MAIN57(0x08B95120u, func_08B95120, Word);
NEW_MAIN57(0x08997A4Cu, func_08997A4C, Word);
NEW_MAIN57(0x08A37510u, func_08A37510, Word);
NEW_MAIN57(0x08A1EC18u, func_08A1EC18, Word);
NEW_MAIN57(0x08AC8FDCu, func_08AC8FDC, Word);
NEW_MAIN57(0x08B9355Cu, func_08B9355C, Word);
NEW_MAIN57(0x0885F46Cu, func_0885F46C, Word);
NEW_MAIN57(0x08ABECA0u, func_08ABECA0, Word);
NEW_MAIN57(0x0880E0F8u, func_0880E0F8, Float);
NEW_MAIN57(0x088A2934u, func_088A2934, Word);
NEW_MAIN57(0x08B61E98u, func_08B61E98, Word);
