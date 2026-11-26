// Main-session batch 1067: 0x088193E8 (a0 = output string, a2 = count, a3 = object pointer). Structured from the asm; the body is a
// run of "build a string, push_back it into the vector at record+0x14" blocks, chosen by the type byte at +4 of the looked-up entry.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1067 {
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
GuestResult call_result(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }

struct SavedCalleeSaved {
    AllegrexContext &ctx;
    u32 saved[8];
    explicit SavedCalleeSaved(AllegrexContext &c) : ctx(c) {
        for (unsigned i = 0u; i < 8u; ++i) saved[i] = c.gpr[16u + i];
    }
    ~SavedCalleeSaved() {
        for (unsigned i = 0u; i < 8u; ++i) ctx.gpr[16u + i] = saved[i];
    }
};

constexpr u32 kEmptyBlock = 0x08D74FD0u;
constexpr u32 kAssertRecord = 0x08C98EA8u;

// The record pointer at sp+0x94; a null one raises the assert (0x08A5BB7C) and the asm goes on with it.
u32 record_checked(Runtime &rt, AllegrexContext &ctx, u32 sp) {
    const u32 record = word(rt, sp + 0x94u);
    if (record == 0u) call(rt, ctx, 0x08A5BB7Cu, {kAssertRecord});
    return record;
}

// vector<string>::push_back(string at `slot`) on the vector at record+0x14 (finish +4, end of storage +8), then the temporary dies.
void push_back(Runtime &rt, AllegrexContext &ctx, u32 record, u32 slot) {
    const u32 vec = record + 0x14u;
    u32 finish = word(rt, vec + 4u);
    if (finish == word(rt, vec + 8u)) {
        call(rt, ctx, 0x08B8A03Cu, {vec, word(rt, record + 0x18u), slot});
    } else {
        if (finish != 0u) {
            call(rt, ctx, 0x08A76ABCu, {finish, slot});
            finish = word(rt, vec + 4u);
        }
        put(rt, vec + 4u, finish + 4u);
    }
    call(rt, ctx, 0x08A5B9ECu, {slot});
}

// The ref-counted temporary at `address`: the shared empty block is left alone, otherwise 0x08B60804 settles the count and
// 0x08B8542C frees it when it is not positive (its second argument is the stack byte at sp+`allocator`).
void release_block(Runtime &rt, AllegrexContext &ctx, u32 address, u32 allocator) {
    const u32 data = word(rt, address);
    const u32 block = data - 0xCu;
    if (block == kEmptyBlock) return;
    if (static_cast<s32>(call(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu})) > 0) return;
    call(rt, ctx, 0x08B8542Cu, {block, allocator});
}

// A string built from a char pointer: temporary at `temp`, string at `slot`, pushed into the record's vector; the temporary is released.
void push_text(Runtime &rt, AllegrexContext &ctx, u32 sp, u32 record, u32 text, u32 temp, u32 slot, u32 alloc_byte, u32 release_byte) {
    call(rt, ctx, 0x08B85EB8u, {temp, text, sp + alloc_byte});
    call(rt, ctx, 0x08A6D2D4u, {slot, temp});
    push_back(rt, ctx, record, slot);
    release_block(rt, ctx, temp, sp + release_byte);
}
// A string made from a number by 0x08A85B1C, pushed.
void push_number(Runtime &rt, AllegrexContext &ctx, u32 record, u32 slot, u32 value) {
    call(rt, ctx, 0x08A85B1Cu, {slot, value});
    push_back(rt, ctx, record, slot);
}
// A string made from a 64-bit pair (v0, v1 of 0x089300C0 / 0x089300B0) by 0x08A5DEE8, pushed.
void push_pair(Runtime &rt, AllegrexContext &ctx, u32 record, u32 slot, const GuestResult &pair) {
    call(rt, ctx, 0x08A5DEE8u, {slot, pair.a1, pair.v0, pair.v1});
    push_back(rt, ctx, record, slot);
}

void func_088193E8(Runtime &rt, AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[6]);
    const u32 source = ctx.gpr[7];
    if (count <= 0) return;
    SavedCalleeSaved saved(ctx);
    GuestFrame frame(ctx, 0x1D0u);
    const u32 sp = ctx.gpr[29];
    const u32 record_slot = sp + 0x94u;

    call(rt, ctx, 0x08817FCCu, {sp + 0x90u});
    u32 object = word(rt, sp + 0x90u);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98D6Cu});
    const u32 owner = call(rt, ctx, 0x08817A54u, {object});
    call(rt, ctx, 0x08A5B9ECu, {sp + 0x90u});

    object = word(rt, source);
    if (object == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});
    const u32 vtable = word(rt, object);
    const u32 key = call_indirect(rt, ctx, word(rt, vtable + 0x3Cu), {object});
    const u32 entry = call(rt, ctx, 0x08817DD0u, {owner, key});
    bool found = entry != 0u;
    if (found) found = call(rt, ctx, 0x08817934u, {entry}) != 0u;
    if (!found) {
        const u32 text = sp + 0x14u;
        call(rt, ctx, 0x08A76AACu, {text, 0x08CF6ED8u});
        if (out != text) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, text)});
        call(rt, ctx, 0x08A5B9ECu, {text});
        return;
    }

    call(rt, ctx, 0x08A98A70u, {record_slot});
    u32 record = record_checked(rt, ctx, sp);
    const u32 name = call(rt, ctx, 0x08817934u, {entry});
    push_text(rt, ctx, sp, record, name, sp + 0x88u, sp + 0x8Cu, 0x10u, 0x8u);

    const u32 kind = byte_at(rt, entry + 4u);
    if (kind == 1u) {
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x84u, word(rt, entry));
        record = record_checked(rt, ctx, sp);
        const u32 label = call(rt, ctx, 0x08B20968u, {0x08CE2AA4u, byte_at(rt, entry + 5u)});
        push_text(rt, ctx, sp, record, label, sp + 0x7Cu, sp + 0x80u, 0xFu, 0x7u);
        GuestResult pair = call_result(rt, ctx, 0x089300C0u, {word(rt, entry + 8u)});
        record = record_checked(rt, ctx, sp);
        push_pair(rt, ctx, record, sp + 0x78u, pair);
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x74u, byte_at(rt, entry + 0xCu));
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x70u, word(rt, entry + 0x10u));
        record = record_checked(rt, ctx, sp);
        push_text(rt, ctx, sp, record, entry + 0x14u, sp + 0x68u, sp + 0x6Cu, 0xEu, 0x6u);
    } else if (kind == 2u) {
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x64u, word(rt, entry));
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x60u, byte_at(rt, entry + 5u));
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x5Cu, byte_at(rt, entry + 6u));
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x58u, byte_at(rt, entry + 7u));
        const u32 value = word(rt, entry + 8u);
        if (value == 0x481907FFu) {
            record = record_checked(rt, ctx, sp);
            push_text(rt, ctx, sp, record, 0x08C87DF8u, sp + 0x50u, sp + 0x54u, 0xDu, 0x5u);
        } else {
            call(rt, ctx, 0x08ABE090u, {value, sp + 0x9Cu, 0x80u});
            record = record_checked(rt, ctx, sp);
            push_text(rt, ctx, sp, record, sp + 0x9Cu, sp + 0x48u, sp + 0x4Cu, 0xCu, 0x4u);
        }
    } else if (kind == 3u) {
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x44u, word(rt, entry));
        GuestResult pair = call_result(rt, ctx, 0x089300C0u, {word(rt, entry + 0x10u)});
        record = record_checked(rt, ctx, sp);
        push_pair(rt, ctx, record, sp + 0x40u, pair);
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x3Cu, byte_at(rt, entry + 0x14u));
        pair = call_result(rt, ctx, 0x089300B0u, {word(rt, entry + 0x18u)});
        record = record_checked(rt, ctx, sp);
        push_pair(rt, ctx, record, sp + 0x38u, pair);
        record = word(rt, record_slot);
        const u32 subtype = byte_at(rt, entry + 5u);
        if (record == 0u) call(rt, ctx, 0x08A5BB7Cu, {kAssertRecord});
        const u32 label = call(rt, ctx, 0x08B20968u, {0x08CE2A9Cu, subtype});
        push_text(rt, ctx, sp, record, label, sp + 0x30u, sp + 0x34u, 0xBu, 0x3u);
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x2Cu, word(rt, entry + 8u));
        record = record_checked(rt, ctx, sp);
        push_number(rt, ctx, record, sp + 0x28u, byte_at(rt, entry + 0xCu));
        if (subtype == 3u) {
            const u32 temp = sp + 0x98u;
            call(rt, ctx, 0x08A85B1Cu, {temp, word(rt, entry + 0x1Cu)});
            u32 item = word(rt, temp);
            if (item == 0u) call(rt, ctx, 0x08A5BB7Cu, {0x08C98EB0u});
            call_indirect(rt, ctx, word(rt, word(rt, item) + 0xCu), {sp + 0x9Cu, item});
            record = word(rt, record_slot);
            if (record == 0u) call(rt, ctx, 0x08A5BB7Cu, {kAssertRecord});
            push_text(rt, ctx, sp, record, word(rt, sp + 0x9Cu), sp + 0x20u, sp + 0x24u, 0xAu, 0x2u);
            release_block(rt, ctx, sp + 0x9Cu, sp + 0x1u);
            call(rt, ctx, 0x08A5B9ECu, {temp});
            // falls to the common tail below
        } else {
            call(rt, ctx, 0x08ABE090u, {word(rt, entry + 0x1Cu), sp + 0x11Cu, 0x80u});
            record = record_checked(rt, ctx, sp);
            push_text(rt, ctx, sp, record, sp + 0x11Cu, sp + 0x18u, sp + 0x1Cu, 0x9u, 0x0u);
        }
    }
    if (out != record_slot) call(rt, ctx, 0x08A5BAD8u, {out, word(rt, record_slot)});
    call(rt, ctx, 0x08A5B9ECu, {record_slot});
}

} // namespace
} // namespace new_batch_main_1067
} // namespace gt::game

#define NEW1067(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1067::name, gt::ReturnKind::kind)
NEW1067(0x088193E8u, func_088193E8, Word);
