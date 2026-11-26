// Main-session batch 1080: GT's inflate front end (gzip/custom header, stored/fixed/dynamic block setup), 0x08B25968.
// The object (a0) is a bit reader: +4 bit buffer, +8 input ptr, +0xC input end, +0x10 window, +0x18 output ptr, +0x1C output end,
// +0x20 s16 bit count (negative = missing bits), +0x22 u16 window position, +0x24 huffman table memory, +0x28 header flag byte;
// vtable slot 2 refills the input, slot 3 flushes the output. The asm keeps the reader in registers and spills it around each call.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1080 {
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

constexpr u32 kBuildTable = 0x08B24DF8u;
constexpr u32 kDecodeBlock = 0x08B2531Cu;
constexpr u32 kOrderTable = 0x08CE0C50u;
constexpr u32 kLengthBase = 0x08CE0BD4u;
constexpr u32 kDistBase = 0x08CE0B5Cu;

struct Reader {
    Runtime &rt;
    AllegrexContext &ctx;
    u32 obj;
    u32 buf = 0, ptr = 0, end = 0, window = 0, out = 0, out_end = 0;
    s32 count = 0;
    u32 wpos = 0;

    u32 u16at(u32 a) { return rt.memory().load16(a); }
    void reload() {
        buf = word(rt, obj + 4u);
        ptr = word(rt, obj + 8u);
        end = word(rt, obj + 0xCu);
        window = word(rt, obj + 0x10u);
        out = word(rt, obj + 0x18u);
        out_end = word(rt, obj + 0x1Cu);
        count = static_cast<std::int16_t>(u16at(obj + 0x20u));
        wpos = u16at(obj + 0x22u);
    }
    void spill() {
        put(rt, obj + 4u, buf);
        put(rt, obj + 8u, ptr);
        put(rt, obj + 0xCu, end);
        put(rt, obj + 0x18u, out);
        put(rt, obj + 0x1Cu, out_end);
        rt.memory().store16(obj + 0x20u, static_cast<std::uint16_t>(count));
        rt.memory().store16(obj + 0x22u, static_cast<std::uint16_t>(wpos));
    }
    void call_slot(u32 slot) { call(rt, ctx, word(rt, word(rt, obj) + slot * 4u), {obj}); }
    // count was negative and has had 16 added: take the next halfword (asking the input to refill when it is used up).
    void top_up() {
        if (ptr == end) {
            spill();
            call_slot(2u);
            reload();
        }
        buf |= u16at(ptr) << (static_cast<u32>(count) & 31u);
        ptr += 2u;
    }
    void drop(u32 bits) {
        buf >>= (bits & 31u);
        count -= static_cast<s32>(bits);
        if (count < 0) {
            count += 16;
            top_up();
        }
    }
    void flush() {
        spill();
        call_slot(3u);
    }
};

void func_08B25968(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x1A0u);
    const u32 sp = ctx.gpr[29];
    auto set8 = [&](u32 a, u32 v) { rt.memory().store8(a, static_cast<std::uint8_t>(v)); };
    Reader r{rt, ctx, self};
    u32 result = 0;

    r.count = static_cast<std::int16_t>(r.u16at(self + 0x20u));
    rt.memory().store16(self + 0x22u, 0u);
    r.buf = word(rt, self + 4u);
    r.ptr = word(rt, self + 8u);
    r.end = word(rt, self + 0xCu);
    r.window = word(rt, self + 0x10u);
    r.out = word(rt, self + 0x18u);
    r.out_end = word(rt, self + 0x1Cu);
    r.wpos = 0;
    if (r.count < 0) {
        r.count += 16;
        if (r.ptr == r.end) {
            rt.memory().store16(self + 0x20u, static_cast<std::uint16_t>(r.count));
            put(rt, self + 8u, r.end);
            r.call_slot(2u);
            r.reload();
        }
        r.buf |= r.u16at(r.ptr) << (static_cast<u32>(r.count) & 31u);
        r.ptr += 2u;
    }

    bool failed = false;
    if ((rt.memory().load8(self + 0x28u)) != 0u) {
        const u32 first = r.buf & 0xFFFFu;
        r.drop(16);
        if (first == 0x8B1Fu) {
            const u32 method = r.buf & 0xFFu;
            r.drop(8);
            if (method != 8u) {
                failed = true;
            } else {
                const u32 flags = r.buf & 0xFFu;
                r.drop(8);
                for (int i = 0; i < 6; ++i) r.drop(8);
                const u32 extra = r.buf & 0xFFFFu;
                if (flags & 4u) {
                    for (u32 i = 0; i != extra; ++i) r.drop(8);
                }
                if (flags & 8u) {
                    u32 c;
                    do {
                        c = r.buf & 0xFFu;
                        r.drop(8);
                    } while (c != 0u);
                }
                if (flags & 0x10u) {
                    u32 c;
                    do {
                        c = r.buf & 0xFFu;
                        r.drop(8);
                    } while (c != 0u);
                }
                if (flags & 2u) {
                    r.drop(8);
                    r.drop(8);
                }
            }
        } else {
            const u32 second = r.buf & 0xFFFFu;
            r.drop(16);
            r.drop(16);
            r.drop(16);
            const u32 magic = first | (second << 16);
            if (magic != 0xFFF7EEC5u && magic != 0xFEFDDB45u) failed = true;
        }
    }

    while (!failed) {
        const u32 final_block = r.buf & 1u;
        put(rt, sp + 0x160u, final_block);
        r.drop(1);
        const u32 kind = r.buf & 3u;
        r.drop(2);
        if (kind == 2u) {
            const u32 hlit = (r.buf & 0x1Fu) + 0x101u;
            r.drop(5);
            const u32 hdist = (r.buf & 0x1Fu) + 1u;
            r.drop(5);
            const u32 hclen = (r.buf & 0xFu) + 4u;
            r.drop(4);
            for (u32 i = 0; i < hclen; ++i) {
                set8(sp + rt.memory().load8(kOrderTable + i), r.buf & 7u);
                r.drop(3);
            }
            for (u32 i = hclen; i < 19u; ++i) set8(sp + rt.memory().load8(kOrderTable + i), 0u);
            u32 table = word(rt, self + 0x24u);
            r.spill();
            const s32 used = static_cast<s32>(call(rt, ctx, kBuildTable, {table, 0x600u, sp, 0x13u, 7u, 0u, 0u}));
            if (used < 0) { failed = true; break; }
            const u32 lens = sp + 0x1Eu;
            const u32 clen_table = table + 4u;
            s32 remaining = static_cast<s32>(hlit + hdist);
            r.reload();
            u32 writer = lens;
            u32 prev = 0;
            while (remaining > 0) {
                const u32 entry = clen_table + ((r.buf & 0x7Fu) << 2);
                const u32 sym = r.u16at(entry);
                const u32 len = rt.memory().load8(entry + 3u);
                r.count -= static_cast<s32>(len);
                r.buf >>= (len & 31u);
                if (r.count < 0) {
                    r.count += 16;
                    r.top_up();
                }
                if (sym < 16u) {
                    set8(writer, sym);
                    ++writer;
                    --remaining;
                    prev = sym;
                    continue;
                }
                u32 repeat;
                if (sym == 16u) {
                    repeat = (r.buf & 3u) + 3u;
                    r.drop(2);
                } else if (sym == 17u) {
                    repeat = (r.buf & 7u) + 3u;
                    r.drop(3);
                    prev = 0;
                } else if (sym == 18u) {
                    repeat = (r.buf & 0x7Fu) + 11u;
                    r.drop(7);
                    prev = 0;
                } else {
                    failed = true;
                    break;
                }
                if (remaining < static_cast<s32>(repeat)) { failed = true; break; }
                for (u32 k = 0; k < repeat; ++k) set8(writer + k, prev);
                writer += repeat;
                remaining -= static_cast<s32>(repeat);
            }
            if (failed) break;
            table = word(rt, self + 0x24u);
            r.spill();
            const s32 lit_used = static_cast<s32>(call(rt, ctx, kBuildTable, {table, 0x600u, lens, hlit, 9u, kLengthBase, 0x101u}));
            if (lit_used < 0) { failed = true; break; }
            const u32 dist_table = table + (static_cast<u32>(lit_used) << 2);
            const s32 dist_used = static_cast<s32>(call(rt, ctx, kBuildTable, {dist_table, 0x600u - static_cast<u32>(lit_used), lens + hlit, hdist, 6u, kDistBase, 0u}));
            if (dist_used < 0) { failed = true; break; }
            if (call(rt, ctx, kDecodeBlock, {self, table, dist_table}) == 0u) { failed = true; break; }
            r.reload();
        } else if (kind == 0u) {
            r.drop(static_cast<u32>(r.count) & 7u);
            const u32 length = r.buf & 0xFFFFu;
            r.drop(16);
            const u32 inverse = r.buf & 0xFFFFu;
            r.drop(16);
            if (length != (inverse ^ 0xFFFFu)) { failed = true; break; }
            for (u32 i = 0; i != (length & 0xFFFFu) ; ++i) {
                const u32 byte = r.buf & 0xFFu;
                rt.memory().store8(r.out, static_cast<std::uint8_t>(byte));
                if (r.window != 0u) {
                    rt.memory().store8(r.window + r.wpos, static_cast<std::uint8_t>(byte));
                    r.wpos = (r.wpos + 1u) & 0x7FFFu;
                }
                ++r.out;
                if (r.out == r.out_end) {
                    r.spill();
                    r.call_slot(3u);
                    r.reload();
                }
                r.drop(8);
            }
        } else if (kind == 1u) {
            u32 p = sp + 0x1Eu;
            for (; p < sp + 0xAEu; ++p) set8(p, 8u);
            for (; p < sp + 0x11Eu; ++p) set8(p, 9u);
            for (; p < sp + 0x136u; ++p) set8(p, 7u);
            for (; p < sp + 0x13Eu; ++p) set8(p, 8u);
            for (u32 q = sp; q < sp + 0x1Eu; ++q) set8(q, 5u);
            const u32 table = word(rt, self + 0x24u);
            r.spill();
            const u32 used = call(rt, ctx, kBuildTable, {table, 0x600u, sp + 0x1Eu, 0x120u, 9u, kLengthBase, 0x101u});
            const u32 dist_table = table + (used << 2);
            call(rt, ctx, kBuildTable, {dist_table, 0x600u - used, sp, 0x1Eu, 5u, kDistBase, 0u});
            if (call(rt, ctx, kDecodeBlock, {self, table, dist_table}) == 0u) { failed = true; break; }
            r.reload();
        } else {
            failed = true;
            break;
        }
        if (final_block != 0u) {
            r.spill();
            r.call_slot(3u);
            result = 1u;
            break;
        }
    }
    ctx.gpr[2] = failed ? 0u : result;
}
} // namespace
} // namespace new_batch_main_1080
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1080::name, gt::ReturnKind::kind)
NEWM(0x08B25968u, func_08B25968, Word);
