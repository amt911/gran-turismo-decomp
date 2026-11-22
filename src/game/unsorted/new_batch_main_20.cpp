// New batch 20 (main session): script-attribute accessors and container helpers no route reaches, written as structured C
// from `tools.disasm` and meant for verification by synthetic cases. Most of them are one of a few shapes:
//  - an attribute accessor for a script class: a2 selects read or write, a3 points at the holder of the object, and the
//    value goes through the text builder (0x088547C4 / 0x08854678 / 0x088A66xx / 0x08A85B1C / 0x08A5BAD8 / 0x08A5B9EC);
//  - a block of an object copied into a fresh allocation and handed to a text builder;
//  - a node inserted into a tree with an allocated 0x18-byte payload;
//  - a pointer-to-member call (`this + (*ptr >> 1)`, the vtable slot when bit 0 is set).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace new_batch_main_20 {
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
// A call whose v0 and v1 are both read afterwards (`jalr` shapes); v0 / v1 / f0 afterwards are the callee's.
u32 call_keeping_v1(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    const gt::GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

u32 signed_byte(Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
u32 signed_half(Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}
// `wsbw`: the low byte, sign-extended to a word.
u32 sign_extend_low_byte(u32 value) { return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(value & 0xFFu))); }

// The callee-saved registers a function keeps values in: stored into its frame where its prologue stores them (a callee can
// read the stack below its caller's frame), given back when it returns.
class Saved {
public:
    Saved(Runtime &rt, AllegrexContext &ctx, const GuestFrame &frame, std::initializer_list<std::pair<unsigned, u32>> slots)
        : ctx_(ctx) {
        for (unsigned i = 0; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
        for (const auto &[reg, offset] : slots) put(rt, frame.at(offset), ctx.gpr[reg]);
    }
    ~Saved() {
        for (unsigned i = 0; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    Saved(const Saved &) = delete;
    Saved &operator=(const Saved &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

enum class StoreKind { Byte, Half };
enum class LoadKind { SignedByte, UnsignedByte, UnsignedHalf };

void store_kind(Runtime &rt, u32 address, u32 value, StoreKind kind) {
    if (kind == StoreKind::Byte) rt.memory().store8(address, static_cast<std::uint8_t>(value));
    else rt.memory().store16(address, static_cast<std::uint16_t>(value));
}
u32 load_kind(Runtime &rt, u32 address, LoadKind kind) {
    switch (kind) {
    case LoadKind::SignedByte: return signed_byte(rt, address);
    case LoadKind::UnsignedByte: return rt.memory().load8(address);
    case LoadKind::UnsignedHalf: return rt.memory().load16(address);
    }
    return 0u;
}

// A script attribute's accessor: a0 = the text a read goes into, a1 unused, a2 = 0 to read and > 0 to write, a3 = a pointer
// to the holder of the object the attribute belongs to. Write: build a handle at frame + 8 (0x088547C4, the string
// 0x08C9D3BC reports a missing one), take the object 0x088A6608 hands out, and store the virtual +0x3C result of the
// holder's object in the field (the string 0x08C9D394 reports a missing object). Read: the same through 0x088A65E8, and the
// field is appended to the text with 0x08A85B1C before 0x08A5BAD8 assigns the text over the destination.
void attribute_accessor(Runtime &rt, AllegrexContext &ctx, u32 field, StoreKind store, LoadKind load) {
    GuestFrame frame(ctx, 0x20u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {31u, 0x1Cu}});
    const u32 text = ctx.gpr[4];
    const u32 holder = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        const u32 handle = ctx.gpr[29] + 8u;
        call(rt, ctx, 0x088547C4u, {handle});
        u32 owner = word(rt, handle);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
        const u32 middle = call(rt, ctx, 0x08854678u, {owner});
        call(rt, ctx, 0x08A5B9ECu, {handle});
        const u32 object = call(rt, ctx, 0x088A6608u, {middle});
        const u32 subject = word(rt, holder);
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D394u});
        ctx.gpr[3] = word(rt, subject);
        const u32 value = call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x3Cu), {subject});
        store_kind(rt, object + field, value, store);
        ctx.gpr[2] = value;
        return;
    }
    const u32 handle = ctx.gpr[29] + 4u;
    call(rt, ctx, 0x088547C4u, {handle});
    u32 owner = word(rt, handle);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
    const u32 middle = call(rt, ctx, 0x08854678u, {owner});
    call(rt, ctx, 0x08A5B9ECu, {handle});
    const u32 object = call(rt, ctx, 0x088A65E8u, {middle});
    call(rt, ctx, 0x08A85B1Cu, {ctx.gpr[29], load_kind(rt, object + field, load)});
    const u32 appended = word(rt, ctx.gpr[29]);
    if (text != ctx.gpr[29]) call(rt, ctx, 0x08A5BAD8u, {text, appended});
    call(rt, ctx, 0x08A5B9ECu, {ctx.gpr[29]});
}

// The same shape through the 0x0881BD98 / 0x0881B068 builder pair: a0 = the text, a2 = 0 to read and > 0 to write, a3 = a
// pointer to the holder. The field is a word.
void member_accessor(Runtime &rt, AllegrexContext &ctx, u32 field) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {19u, 0x1Cu}, {31u, 0x20u}});
    const u32 text = ctx.gpr[4];
    const u32 holder = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        const u32 handle = ctx.gpr[29] + 8u;
        call(rt, ctx, 0x0881BD98u, {handle});
        u32 owner = word(rt, handle);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
        const u32 object = call_keeping_v1(rt, ctx, 0x0881B068u, {owner});  // s1 = its result, in the call's delay slot
        call(rt, ctx, 0x08A5B9ECu, {handle});
        const u32 subject = word(rt, holder);
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98EFCu});
        ctx.gpr[3] = word(rt, subject);
        const u32 value = call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x3Cu), {subject});
        put(rt, object + field, value);
        ctx.gpr[2] = value;
        return;
    }
    const u32 handle = ctx.gpr[29] + 4u;
    call(rt, ctx, 0x0881BD98u, {handle});
    u32 owner = word(rt, handle);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    const u32 object = call(rt, ctx, 0x0881B068u, {owner});
    call(rt, ctx, 0x08A5B9ECu, {handle});
    call(rt, ctx, 0x08A85B1Cu, {ctx.gpr[29], word(rt, object + field)});
    const u32 appended = word(rt, ctx.gpr[29]);
    if (text != ctx.gpr[29]) call(rt, ctx, 0x08A5BAD8u, {text, appended});
    call(rt, ctx, 0x08A5B9ECu, {ctx.gpr[29]});
}

// 0x0881F0D8 alone: the two-word version of `member_accessor` (fields +0x18 and +0x1C). The write path stores the virtual
// +0x50 result at +0x18 and the callee's v1 at +0x1C; the read path passes both words to 0x08A5DEE8 as a2 / a3.
void two_word_accessor(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {19u, 0x1Cu}, {31u, 0x20u}});
    const u32 text = ctx.gpr[4];
    const u32 holder = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        const u32 handle = ctx.gpr[29] + 8u;
        call(rt, ctx, 0x0881BD98u, {handle});
        u32 owner = word(rt, handle);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
        const u32 object = call_keeping_v1(rt, ctx, 0x0881B068u, {owner});
        call(rt, ctx, 0x08A5B9ECu, {handle});
        const u32 subject = word(rt, holder);
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98EFCu});
        ctx.gpr[3] = word(rt, subject);
        const u32 first = call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x50u), {subject});
        put(rt, object + 0x18u, first);
        put(rt, object + 0x1Cu, ctx.gpr[3]);  // v1 as the callee left it
        return;
    }
    const u32 handle = ctx.gpr[29] + 4u;
    call(rt, ctx, 0x0881BD98u, {handle});
    u32 owner = word(rt, handle);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98F48u});
    const u32 object = call(rt, ctx, 0x0881B068u, {owner});
    call(rt, ctx, 0x08A5B9ECu, {handle});
    call(rt, ctx, 0x08A5DEE8u, {ctx.gpr[29], ctx.gpr[29], word(rt, object + 0x18u), word(rt, object + 0x1Cu)});
    const u32 appended = word(rt, ctx.gpr[29]);
    if (text != ctx.gpr[29]) call(rt, ctx, 0x08A5BAD8u, {text, appended});
    call(rt, ctx, 0x08A5B9ECu, {ctx.gpr[29]});
}

// a0 = the text, a2 = 0 for the read shape and nonzero for the write one, a3 = a pointer to the holder: read forms the text
// with `read_fn`, write stores the holder's virtual +0x3C result with `write_fn`. Both builders start at 0x088547C4 (the
// string 0x08C9E188 reports a missing handle, 0x08C9E14C a missing object).
void text_pair(Runtime &rt, AllegrexContext &ctx, u32 read_fn, u32 write_fn) {
    GuestFrame frame(ctx, 0x20u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {31u, 0x1Cu}});
    const u32 text = ctx.gpr[4];
    const u32 holder = ctx.gpr[7];
    if (ctx.gpr[6] != 0u) {
        const u32 handle = ctx.gpr[29];
        call(rt, ctx, 0x088547C4u, {handle});
        u32 owner = word(rt, handle);
        if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9E188u});
        const u32 object = call_keeping_v1(rt, ctx, 0x08854678u, {owner});
        const u32 subject = word(rt, holder);
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9E14Cu});
        ctx.gpr[3] = word(rt, subject);
        const u32 value = call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x3Cu), {subject});
        call(rt, ctx, write_fn, {object, value});
        call(rt, ctx, 0x08A5B9ECu, {handle});
        return;
    }
    const u32 handle = ctx.gpr[29] + 4u;
    call(rt, ctx, 0x088547C4u, {handle});
    u32 owner = word(rt, handle);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9E188u});
    const u32 object = call(rt, ctx, 0x08854678u, {owner});
    const u32 value = call(rt, ctx, read_fn, {object});
    const u32 appended = ctx.gpr[29] + 8u;
    call(rt, ctx, 0x08A85B1Cu, {appended, value});
    const u32 line = word(rt, appended);
    if (text != appended) call(rt, ctx, 0x08A5BAD8u, {text, line});
    call(rt, ctx, 0x08A5B9ECu, {appended});
    call(rt, ctx, 0x08A5B9ECu, {handle});
}

// a0 = the text, a2 > 0 asks the holder's object for an index (virtual +0x3C), and the signed byte of the table word at
// `field` for that index (clamped at 0) is appended to the text.
void indexed_byte_text(Runtime &rt, AllegrexContext &ctx, u32 field) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {19u, 0x1Cu}, {20u, 0x20u}, {31u, 0x24u}});
    const u32 text = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    const u32 handle = ctx.gpr[29] + 4u;
    call(rt, ctx, 0x088418B4u, {handle});
    u32 owner = word(rt, handle);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9C584u});
    const u32 object = call(rt, ctx, 0x0884177Cu, {owner});
    call(rt, ctx, 0x08A5B9ECu, {handle});
    const u32 table = call_keeping_v1(rt, ctx, 0x0888EB9Cu, {object});
    u32 index = call(rt, ctx, 0x0888EB74u, {object});
    if (static_cast<s32>(count) > 0) {
        const u32 subject = word(rt, holder);
        if (subject == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9C520u});
        ctx.gpr[3] = word(rt, subject);
        index = call_indirect(rt, ctx, word(rt, ctx.gpr[3] + 0x3Cu), {subject});
    }
    if (static_cast<s32>(index) < 0) index = 0u;  // max index, 0
    const u32 value = sign_extend_low_byte(word(rt, table + index * 4u + field));
    call(rt, ctx, 0x08A85B1Cu, {ctx.gpr[29], value});
    const u32 line = word(rt, ctx.gpr[29]);
    if (text != ctx.gpr[29]) call(rt, ctx, 0x08A5BAD8u, {text, line});
    call(rt, ctx, 0x08A5B9ECu, {ctx.gpr[29]});
}

// A pointer-to-member call: `this + (*member >> 1)` is the adjusted object, and when bit 0 of the pointer is set the target
// comes from its vtable slot; otherwise the slot field itself holds the function.
u32 member_target(Runtime &rt, u32 adjusted, u32 member, u32 slot) {
    if ((member & 1u) != 0u) return word(rt, word(rt, adjusted) + slot);
    return slot;
}

// a0 = an object with a pointer-to-member at +0x18 (+0x14 its slot), a1 = the text, a2 = a pointer to the holder, t0 = an
// argument for 0x08AA2920 (a0 at the call). Two member calls: the first on the holder's object, the second with the first
// call's result as its `this`, the text as a0 and the 0x08AA2920 result as a2; then `finish` shapes the text.
void member_call_text(Runtime &rt, AllegrexContext &ctx, u32 finish) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {19u, 0x1Cu}, {31u, 0x20u}});
    const u32 self = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    const u32 argument = call(rt, ctx, 0x08AA2920u, {ctx.gpr[8], ctx.gpr[5]});
    const u32 member = word(rt, self + 0x18u);
    const u32 object = word(rt, holder);
    const u32 slot = word(rt, self + 0x14u);
    const u32 adjusted = object + (member >> 1);
    const u32 target = member_target(rt, adjusted, member, slot);
    const u32 first = call_indirect(rt, ctx, target, {adjusted, member, slot, target});
    const u32 member2 = word(rt, self + 0x20u);
    const u32 slot2 = word(rt, self + 0x1Cu);
    const u32 adjusted2 = first + (member2 >> 1);
    const u32 target2 = member_target(rt, adjusted2, member2, slot2);
    const u32 buffer = ctx.gpr[29];
    call_indirect(rt, ctx, target2, {buffer, adjusted2, argument});
    const u32 shaped = ctx.gpr[29] + 4u;
    call(rt, ctx, finish, {shaped, buffer});
    const u32 line = word(rt, shaped);
    if (text != shaped) call(rt, ctx, 0x08A5BAD8u, {text, line});
    call(rt, ctx, 0x08A5B9ECu, {shaped});
    call(rt, ctx, 0x08A5B9ECu, {buffer});
}

// Inserts a node into the tree at a0 (a1 = 0 and a2 != a0 + 4 pick the ordered path), allocating it with `allocate`, the
// payload's two members with `construct`, and linking it with 0x08B5F2B8. The ordered path packs the two distances into
// frame words 0 and 4, compares them with `compare` and keeps its answer unless it is 0. v0 = the new node.
void node_insert(Runtime &rt, AllegrexContext &ctx, u32 allocate, u32 compare, u32 construct) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame,
                {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {19u, 0x1Cu}, {20u, 0x20u}, {21u, 0x24u}, {22u, 0x28u}, {31u, 0x2Cu}});
    const u32 self = ctx.gpr[4];
    const u32 side = ctx.gpr[5];
    const u32 other = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    u32 flag;
    if (side != 0u || other == self + 4u) {
        flag = 1u;
    } else {
        const u32 left = word(rt, holder);
        const u32 right = word(rt, other + 0x10u);
        const u32 first = word(rt, left - 0xCu);
        const u32 second = word(rt, right - 0xCu);
        put(rt, ctx.gpr[29], first);
        put(rt, ctx.gpr[29] + 4u, second);
        const u32 chosen = second < first ? ctx.gpr[29] : ctx.gpr[29] + 4u;
        const u32 distance = first - second;
        const u32 answer = call(rt, ctx, compare, {left, right, word(rt, chosen)});
        const u32 measured = answer != 0u ? answer : distance;
        flag = static_cast<s32>(measured) < 0 ? 1u : 0u;
    }
    const u32 node = call(rt, ctx, allocate, {0x18u});
    const u32 payload = node + 0x10u;
    if (payload != 0u) {
        call(rt, ctx, construct, {payload, holder});
        call(rt, ctx, construct, {node + 0x14u, holder + 4u});
    }
    call(rt, ctx, 0x08B5F2B8u, {flag, node, other, self + 4u});
    put(rt, self + 0x14u, word(rt, self + 0x14u) + 1u);
    ctx.gpr[2] = node;
}

// a0 = the text. Copies the object member [first, end) 0x10 bytes at a time into a fresh allocation (size = end - first) and
// hands it to 0x08A52AB8 (a1 = the word at `global`, a3 = -1, t0 = 0x0884F734, t1 = 0), then assigns the text over a0 and
// bumps the word at 0x08CE2CEC; v0 = the bumped counter.
void copy_block_text(Runtime &rt, AllegrexContext &ctx, u32 first, u32 end, u32 global) {
    GuestFrame frame(ctx, 0x20u);
    Saved saved(rt, ctx, frame, {{16u, 0x10u}, {17u, 0x14u}, {18u, 0x18u}, {31u, 0x1Cu}});
    const u32 text = ctx.gpr[4];
    const u32 handle = ctx.gpr[29] + 4u;
    call(rt, ctx, 0x088547C4u, {handle});
    u32 owner = word(rt, handle);
    if (owner == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
    const u32 middle = call(rt, ctx, 0x08854678u, {owner});
    call(rt, ctx, 0x08A5B9ECu, {handle});
    const u32 object = call(rt, ctx, 0x088A5D0Cu, {middle});
    const u32 buffer = call(rt, ctx, 0x08B2B5E0u, {end - first});
    for (u32 offset = 0; offset < end - first; offset += 0x10u) {
        put(rt, buffer + offset, word(rt, object + first + offset));
        put(rt, buffer + offset + 4u, word(rt, object + first + offset + 4u));
        put(rt, buffer + offset + 8u, word(rt, object + first + offset + 8u));
        put(rt, buffer + offset + 0xCu, word(rt, object + first + offset + 0xCu));
    }
    call(rt, ctx, 0x08A52AB8u, {ctx.gpr[29], word(rt, global), buffer, 0xFFFFFFFFu, 0x0884F734u, 0u});
    const u32 line = word(rt, ctx.gpr[29]);
    if (text != ctx.gpr[29]) call(rt, ctx, 0x08A5BAD8u, {text, line});
    call(rt, ctx, 0x08A5B9ECu, {ctx.gpr[29]});
    const u32 counter = word(rt, 0x08CE2CECu) + 1u;
    put(rt, 0x08CE2CECu, counter);
    ctx.gpr[2] = counter;
}

// --- the attribute accessors (one per field) ---

// --- the word-field accessors ---

// --- the text-builder pairs ---

// --- the indexed table readers ---

// 0x08842804: the same reader over a halfword table at +0x70, indexed by two bytes per entry; the value is the loaded
// halfword's low byte sign-extended (`wsbw`) and then shifted right 16 (arithmetic), as the asm does.

// --- the pointer-to-member calls ---

// --- the node insertions ---
void func_08B947FC(Runtime &rt, AllegrexContext &ctx) { node_insert(rt, ctx, 0x08B35DA0u, 0x08B36170u, 0x08B9432Cu); }
void func_08C4EA54(Runtime &rt, AllegrexContext &ctx) { node_insert(rt, ctx, 0x08B62344u, 0x08B36170u, 0x08C7464Cu); }

// --- the copied blocks ---

// 0x08BE78E8 (a0 = the destination elements, a1 = how many, a2 = the element copied from): each element is 0x60 bytes —
// words 4 and 8, floats 0xC..0x1C, the vtable 0x08CCFF88 at 0 — and each half (destination + 0x20 and + 0x40) is copied with
// 0x0896418C, the halves' word 0x10 taken from the source's 0x30 and 0x50.
void func_08BE78E8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{23u, 0x1Cu}, {17u, 0x4u}, {31u, 0x24u}, {30u, 0x20u}, {22u, 0x18u}, {21u, 0x14u}, {20u, 0x10u},
                                 {19u, 0xCu}, {18u, 0x8u}, {16u, 0x0u}});
    const u32 count = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    for (u32 i = 0; i < count; ++i) {
        const u32 element = ctx.gpr[4] + i * 0x60u;
        if (element == 0u) continue;
        put(rt, element + 4u, word(rt, source + 4u));
        put(rt, element + 8u, word(rt, source + 8u));
        for (u32 offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, element + offset, word(rt, source + offset));
        const u32 first = element + 0x20u;
        const u32 second = element + 0x40u;
        const u32 source_first = source + 0x20u;
        const u32 source_second = source + 0x40u;
        put(rt, element, 0x08CCFF88u);
        call(rt, ctx, 0x0896418Cu, {first, source_first});
        put(rt, first + 0x10u, word(rt, source_first + 0x10u));
        call(rt, ctx, 0x0896418Cu, {second, source_second});
        put(rt, second + 0x10u, word(rt, source_second + 0x10u));
    }
}

// 0x08C10EE8 (a0 = the first source element, a1 = the source end, a2 = the destination): the same 0x60-byte elements copied
// forward with 0x0896418C, the vtable 0x08CCFF88 at 0, word 0x30 taken from the source's 0x30 and 0x50. v0 = the
// destination end.
void func_08C10EE8(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    Saved saved(rt, ctx, frame, {{22u, 0x18u}, {16u, 0x0u}, {31u, 0x20u}, {23u, 0x1Cu}, {21u, 0x14u}, {20u, 0x10u}, {19u, 0xCu},
                                 {18u, 0x8u}, {17u, 0x4u}});
    const u32 end = ctx.gpr[5];
    u32 destination = ctx.gpr[6];
    for (u32 element = ctx.gpr[4]; element != end; element += 0x60u, destination += 0x60u) {
        if (destination == 0u) continue;
        put(rt, destination + 4u, word(rt, element + 4u));
        put(rt, destination + 8u, word(rt, element + 8u));
        for (u32 offset = 0xCu; offset <= 0x1Cu; offset += 4u) put(rt, destination + offset, word(rt, element + offset));
        const u32 first = destination + 0x20u;
        const u32 second = destination + 0x40u;
        const u32 source_first = element + 0x20u;
        const u32 source_second = element + 0x40u;
        put(rt, destination, 0x08CCFF88u);
        call(rt, ctx, 0x0896418Cu, {first, source_first});
        put(rt, first + 0x10u, word(rt, source_first + 0x10u));
        call(rt, ctx, 0x0896418Cu, {second, source_second});
        put(rt, second + 0x10u, word(rt, source_second + 0x10u));
    }
    ctx.gpr[2] = destination;
}

// 0x08BE5A78 (a0 = a vector {begin, end, ...}, a1 = the destination, a2 = the source): removes the elements of 0x30 bytes
// between the source and the destination — the count is ((end - source) >> 4) * 0xAAAAAAAB (the low word of the product,
// exact because the distance is a multiple of 0x30) — moves them up with a word-by-word copy of offsets 4..0x2C, calls
// 0x08BE58F0(destination, old end, frame) and lowers the vector's end by (source - destination) rounded down to 16. v0 =
// the destination.

// 0x08B1C590 (a0 = a counter {word +0x10, word +0x14, handle +0x18, ...}, a1 unused, a2 = a low word, a3 = a high word):
// takes a timestamp in the frame (0x08B28D34), locks a0 + 4 (0x08B34D40) and waits until the counter has passed the given
// 64-bit target (a negative or zero high word means "add the target to the counter first"). When it has, the handle at
// +0x18 is stored in the frame and the counter takes the frame's address. Unlocks (0x08B34D80), releases the timestamp
// (0x08B28CD4) and frees the frame (0x08B28BE0); v0 / v1 = the low / high word waited for.
void func_08B1C590(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x60u);
    Saved saved(rt, ctx, frame, {{20u, 0x50u}, {18u, 0x48u}, {17u, 0x44u}, {16u, 0x40u}, {31u, 0x54u}, {19u, 0x4Cu}});
    const u32 self = ctx.gpr[4];
    u32 low = ctx.gpr[6];
    u32 high = ctx.gpr[7];
    call(rt, ctx, 0x08B28D34u, {ctx.gpr[29]});
    call(rt, ctx, 0x08B34D40u, {self + 4u});
    if (static_cast<s32>(high) < 0 || (high == 0u && low == 0u)) {
        const u32 base_low = word(rt, self + 0x10u);
        const u32 base_high = word(rt, self + 0x14u);
        const u32 difference = base_low - low;
        const u32 borrow = base_low < difference ? 1u : 0u;
        high = base_high - high - borrow;
        low = difference;
    }
    const u32 target_low = word(rt, self + 0x10u);
    const u32 target_high = word(rt, self + 0x14u);
    bool expired = false;
    if (static_cast<s32>(target_high) < static_cast<s32>(high)) expired = true;
    else if (target_high == high && target_low < low) expired = true;
    u32 handle = 0u;
    if (expired) {
        handle = word(rt, self + 0x18u);
        put(rt, self + 0x18u, ctx.gpr[29]);
        put(rt, ctx.gpr[29] + 0x30u, low);
        put(rt, ctx.gpr[29] + 0x34u, high);
        put(rt, ctx.gpr[29] + 0x2Cu, handle);
    }
    call(rt, ctx, 0x08B34D80u, {self + 4u});
    if (handle != 0u) call(rt, ctx, 0x08B28CD4u, {ctx.gpr[29]});
    call(rt, ctx, 0x08B28BE0u, {ctx.gpr[29]});
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}

} // namespace
} // namespace new_batch_main_20
} // namespace gt::game

#define NEW20(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_20::name, gt::ReturnKind::kind)
NEW20(0x08B947FCu, func_08B947FC, Word);
NEW20(0x08C4EA54u, func_08C4EA54, Word);
NEW20(0x08B1C590u, func_08B1C590, DoubleWord);
NEW20(0x08BE78E8u, func_08BE78E8, Void);
NEW20(0x08C10EE8u, func_08C10EE8, Word);
