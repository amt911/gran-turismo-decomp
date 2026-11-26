// Main-session batch 1088 (draft, unverified): 0x08C5FBAC, a format-string driven extractor over two stream-buffer iterators
// (the shape of a time_get "extract via format" routine): iterators are {streambuf*, cached char}; the format characters
// are narrowed through a ctype facet; '%' selects a conversion by the jump table at 0x08CDBBA4.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1088 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

constexpr u32 kEof = 0xFFFFFFFFu;

s32 seb(u32 value) { return static_cast<std::int8_t>(value & 0xFFu); }
s32 load_signed_byte(Runtime &rt, u32 address) { return static_cast<std::int8_t>(rt.memory().load8(address)); }

struct Extractor {
    Runtime &rt;
    AllegrexContext &ctx;
    GuestFrame frame;
    u32 self;      // a0 (kept at 0x1F8)
    u32 io;        // a3 (s2)
    u32 err;       // t0 (s3)
    u32 tm;        // t1 (0x1FC)
    u32 format;    // t2 (0x200)
    u32 facet;     // s5: the time facet
    u32 ctype;     // s4: the ctype facet
    u32 beg;       // address of the begin iterator (sp+0x28: streambuf, sp+0x2C: cached char)
    u32 end;       // address of the end iterator (sp+0x20, sp+0x24)
    u32 fp = 0;    // index into the format

    Extractor(Runtime &r, AllegrexContext &c) : rt(r), ctx(c), frame(c, 0x240u) {
        self = c.gpr[4];
        io = c.gpr[7];
        err = c.gpr[8];
        tm = c.gpr[9];
        format = c.gpr[10];
        beg = frame.at(0x28u);
        end = frame.at(0x20u);
    }

    u32 call(u32 target, std::initializer_list<u32> arguments) { return call_guest(rt, ctx, target, arguments); }
    GuestResult call_pair(u32 target, std::initializer_list<u32> arguments) { return call_guest(rt, ctx, target, arguments, {}); }

    // Reads the character an iterator points at (the cached one, else a probe of the stream buffer). End of input
    // clears the iterator's buffer; a character is cached. 0 buffer = already at the end.
    u32 peek(u32 it) {
        const u32 buffer = word(rt, it);
        if (buffer == 0u) return kEof;
        u32 c = word(rt, it + 4u);
        if (c != kEof) return c;
        const u32 cur = word(rt, buffer + 8u);
        if (cur < word(rt, buffer + 0xCu)) {
            c = rt.memory().load8(cur);
        } else {
            c = call(word(rt, word(rt, buffer) + 0x24u), {buffer});
        }
        if (c == kEof) {
            put(rt, it, 0u);
        } else {
            put(rt, it + 4u, c);
        }
        return c;
    }
    // The signed character as the narrowing and comparison code uses it (-1 at the end).
    s32 peek_signed() { return seb(peek(beg)); }

    // ++begin: nothing when the buffer is gone; the next character is forgotten.
    void advance() {
        const u32 buffer = word(rt, beg);
        if (buffer == 0u) return;
        const u32 cur = word(rt, buffer + 8u);
        if (cur < word(rt, buffer + 0xCu)) {
            put(rt, buffer + 8u, cur + 1u);
        } else {
            call(word(rt, word(rt, buffer) + 0x28u), {buffer});
        }
        put(rt, beg + 4u, kEof);
    }
    void set_beg(const GuestResult &r) {
        put(rt, beg, r.v0);
        put(rt, beg + 4u, r.v1);
    }

    // ctype::narrow(c, 0) with its 256-entry cache at +0x11D.
    s32 narrow(s32 c) {
        const u32 slot = ctype + (static_cast<u32>(c) & 0xFFu) + 0x11Du;
        s32 value = load_signed_byte(rt, slot);
        if (value == 0) {
            const u32 r = call(word(rt, word(rt, ctype) + 0x20u), {ctype, static_cast<u32>(c), 0u});
            if (r != 0u) rt.memory().store8(slot, static_cast<std::uint8_t>(r));
            value = seb(r);
        }
        return value;
    }
    // The first use of the widen cache: the identity table through the facet, then a check whether it is the identity.
    void widen_init() {
        const u32 table = frame.at(0xD4u);
        for (u32 i = 0u; i < 0x100u; ++i) rt.memory().store8(table + i, static_cast<std::uint8_t>(i));
        call(word(rt, word(rt, ctype) + 0x1Cu), {ctype, table, frame.at(0x1D4u), ctype + 0x1Du});
        rt.memory().store8(ctype + 0x1Cu, 1u);
        if (call(0x08B36170u, {table, ctype + 0x1Du, 0x100u}) != 0u) rt.memory().store8(ctype + 0x1Cu, 2u);
    }
    // ctype::widen(c) of a constant character.
    u32 widen_char(u32 c) {
        if (load_signed_byte(rt, ctype + 0x1Cu) != 0) return rt.memory().load8(ctype + 0x1Du + c);
        widen_init();
        return call(word(rt, word(rt, ctype) + 0x18u), {ctype, c});
    }
    // ctype::widen(first, last, to) of a constant string into the frame buffer at 0x30.
    u32 widen_string(u32 first, u32 count) {
        const u32 to = frame.at(0x30u);
        const s32 state = load_signed_byte(rt, ctype + 0x1Cu);
        if (state == 1) {
            call(0x08B79A88u, {to, first, count});
        } else {
            if (state == 0) widen_init();
            call(word(rt, word(rt, ctype) + 0x1Cu), {ctype, first, first + count, to});
        }
        return to;
    }
    void set_failbit() { put(rt, err, word(rt, err) | 4u); }

    // Parses a number into `field` (the common callee), then keeps the new begin iterator.
    void number(u32 field, u32 flag, u32 limit, u32 digits) {
        put(rt, frame.at(0u), err);
        set_beg(call_pair(0x08C5C160u, {self, beg, end, field, flag, limit, digits, io}));
    }
    // Parses a name from an array of `count` words copied from the facet's cache into the frame.
    void name(u32 field, u32 source, u32 stack_at, u32 count) {
        const u32 cache = word(rt, facet + 8u);
        for (u32 i = 0u; i < count; ++i) put(rt, frame.at(stack_at + 4u * i), word(rt, cache + source + 4u * i));
        set_beg(call_pair(0x08C5C4B4u, {self, beg, end, field, frame.at(stack_at), count, io, err}));
    }
    // A nested conversion with another format string.
    void nested(u32 sub_format) {
        set_beg(call_pair(0x08C5FBACu, {self, beg, end, io, err, tm, sub_format}));
    }

    void conversion(s32 spec);
    u32 run();
};

void Extractor::conversion(s32 spec) {
    const u32 index = static_cast<u32>(spec) - 0x41u;
    if (index >= 0x39u) {
        set_failbit();
        return;
    }
    switch (spec) {
    case 'A': name(tm + 0x18u, 0x2Cu, 0x58u, 7u); return;
    case 'a': name(tm + 0x18u, 0x48u, 0x3Cu, 7u); return;
    case 'B': name(tm + 0x10u, 0x64u, 0xA4u, 12u); return;
    case 'b':
    case 'h': name(tm + 0x10u, 0x94u, 0x74u, 12u); return;
    case 'C':
    case 'y': number(tm + 0x14u, 0u, 0x63u, 2u); return;
    case 'c': nested(word(rt, word(rt, facet + 8u) + 0x18u)); return;
    case 'x': nested(word(rt, word(rt, facet + 8u) + 0x08u)); return;
    case 'X': nested(word(rt, word(rt, facet + 8u) + 0x10u)); return;
    case 'D': nested(widen_string(0x08CB57F0u, 9u)); return;
    case 'R': nested(widen_string(0x08CB57FCu, 6u)); return;
    case 'T': nested(widen_string(0x08CB5804u, 9u)); return;
    case 'H': number(tm + 8u, 0u, 0x17u, 2u); return;
    case 'I': number(tm + 8u, 1u, 0xCu, 2u); return;
    case 'M': number(tm + 4u, 0u, 0x3Bu, 2u); return;
    case 'S': number(tm, 0u, 0x3Du, 2u); return;
    case 'd': number(tm + 0xCu, 1u, 0x1Fu, 2u); return;
    case 'm': {
        number(frame.at(0x10u), 1u, 0xCu, 2u);
        if (word(rt, err) == 0u) put(rt, tm + 0x10u, word(rt, frame.at(0x10u)) - 1u);
        return;
    }
    case 'Y': {
        number(frame.at(0x10u), 0u, 0x270Fu, 4u);
        if (word(rt, err) == 0u) put(rt, tm + 0x14u, word(rt, frame.at(0x10u)) - 0x76Cu);
        return;
    }
    case 'e': {
        const s32 c = peek_signed();
        if ((load_signed_byte(rt, word(rt, ctype + 0x18u) + (static_cast<u32>(c) & 0xFFu)) & 8) != 0) {
            advance();
            number(tm + 0xCu, 1u, 9u, 1u);
        } else {
            number(tm + 0xCu, 0xAu, 0x1Fu, 2u);
        }
        return;
    }
    case 't':
    case 'n': {
        const s32 c = peek_signed();
        if (narrow(c) == (spec == 't' ? 9 : 0xA)) {
            advance();
        } else {
            set_failbit();
        }
        return;
    }
    case 'Z': {
        s32 c = peek_signed();
        if ((load_signed_byte(rt, word(rt, ctype + 0x18u) + (static_cast<u32>(c) & 0xFFu)) & 1) == 0) {
            set_failbit();
            return;
        }
        set_beg(call_pair(0x08C5C4B4u, {self, beg, end, frame.at(0x14u), 0x08CE0E1Cu, 0xEu, io, err}));
        const u32 beg_c = peek(beg);
        const u32 end_c = peek(end);
        if ((beg_c == kEof) == (end_c == kEof)) return;
        if (word(rt, err) != 0u) return;
        if (word(rt, frame.at(0x14u)) != 0u) return;
        c = peek_signed();
        if (c != seb(widen_char(0x2Du))) {
            const s32 d = peek_signed();
            if (d != seb(widen_char(0x2Bu))) return;
        }
        number(frame.at(0x14u), 0u, 0x17u, 2u);
        number(frame.at(0x14u), 0u, 0x3Bu, 2u);
        return;
    }
    default: set_failbit(); return;
    }
}

u32 Extractor::run() {
    put(rt, beg, word(rt, ctx.gpr[5]));
    put(rt, beg + 4u, word(rt, ctx.gpr[5] + 4u));
    put(rt, end, word(rt, ctx.gpr[6]));
    put(rt, end + 4u, word(rt, ctx.gpr[6] + 4u));
    const u32 id = call(0x08B5C2B0u, {0x08D77478u});
    const u32 impl = word(rt, io + 0x6Cu);
    facet = 0u;
    if (id < word(rt, impl + 8u)) facet = word(rt, word(rt, impl + 4u) + 4u * id);
    if (facet == 0u) {
        call(0x08B5A9FCu, {});  // bad_cast: does not return
        return 0u;
    }
    ctype = call(0x08B64C80u, {io + 0x6Cu});
    const u32 length = call(0x08B35F60u, {format});
    while (true) {
        const u32 beg_c = peek(beg);
        const u32 end_c = peek(end);
        if ((beg_c == kEof) == (end_c == kEof)) break;
        if (!(fp < length)) break;
        if (word(rt, err) != 0u) break;
        const s32 ch = load_signed_byte(rt, format + fp);
        if (narrow(ch) != 0x25) {
            const s32 got = peek_signed();
            if (ch == got) {
                advance();
            } else {
                set_failbit();
            }
            ++fp;
            continue;
        }
        const u32 next = fp + 1u;
        s32 spec = narrow(load_signed_byte(rt, format + next));
        put(rt, frame.at(0x10u), 0u);
        if (spec == 0x45 || spec == 0x4F) {
            fp += 2u;
            spec = narrow(load_signed_byte(rt, format + fp));
        } else {
            fp = next;
        }
        conversion(spec);
        ++fp;
    }
    return 0u;
}

void func_08C5FBAC(Runtime &rt, AllegrexContext &ctx) {
    Extractor e(rt, ctx);
    e.run();
    ctx.gpr[2] = word(rt, e.beg);
    ctx.gpr[3] = word(rt, e.beg + 4u);
}

} // namespace
} // namespace new_batch_main_1088

#define NEW1088(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1088::name, gt::ReturnKind::kind)
NEW1088(0x08C5FBACu, func_08C5FBAC, DoubleWord);

} // namespace gt::game
