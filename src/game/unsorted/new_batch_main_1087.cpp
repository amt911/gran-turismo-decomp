// New batch main 1087 (main session): one large function, written by hand from `tools/disasm` as structured C.
// DRAFT: not verified by the differential gate yet.
//
// 0x089868A8 is a token scanner over a stream (a0) that appends to a shared string (a1: a pointer to the string body, with
// length / capacity / refcount at -0xC / -0x8 / -0x4). Return values read off the asm: -1 end of input, 0x100 name,
// 0x101 number, 0x102 string literal, otherwise the character itself. Quirks kept on purpose:
// - after a run of decimal digits only 'E' starts an exponent ('e' is not tested on that path; it is after a fraction);
// - a hex / binary prefix with no digit behind it pushes both characters back and rescans the '0' as a decimal.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <array>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_1087 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

class SavedRegisters {
public:
    SavedRegisters(AllegrexContext &ctx, std::initializer_list<unsigned> registers) : ctx_(ctx), count_(registers.size()) {
        unsigned index = 0u;
        for (const unsigned reg : registers) {
            registers_[index] = reg;
            saved_[index] = ctx.gpr[reg];
            ++index;
        }
    }
    ~SavedRegisters() {
        for (unsigned index = 0u; index < count_; ++index) ctx_.gpr[registers_[index]] = saved_[index];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    unsigned count_;
    std::array<unsigned, 10> registers_{};
    std::array<u32, 10> saved_{};
};

constexpr u32 kEnd = 0xFFFFFFFFu;
constexpr u32 kName = 0x100u;
constexpr u32 kNumber = 0x101u;
constexpr u32 kString = 0x102u;

bool is_space(u32 c) { return c == 0x20u || c == 0xCu || c == 0xAu || c == 0xDu || c == 9u || c == 0xBu; }
bool is_digit(u32 c) { return c - 0x30u < 10u; }
bool is_alpha(u32 c) { return c - 0x41u < 26u || c - 0x61u < 26u; }
bool is_hex(u32 c) { return c - 0x41u < 6u || c - 0x30u < 10u || c - 0x61u < 6u; }

class Scanner {
public:
    Scanner(Runtime &rt, AllegrexContext &ctx, u32 stream, u32 text) : rt_(rt), ctx_(ctx), stream_(stream), text_(text) {}

    u32 scan() {
        u32 c;
        do {
            c = get();
        } while (is_space(c));
        if (c == kEnd) return kEnd;
        call(0x08B8634Cu, {text_, 0u, word(rt_, word(rt_, text_) - 0xCu), 0u});
        ctx_.gpr[23] = 0x4B0000u;  // s7, set in the call's delay slot
        if (is_alpha(c) || c == '_') return name(c);
        if (c == 0x27u) return quoted();
        if (c == '+' || c == '-') {
            push(c);
            c = get();
            if (!is_digit(c) && c != '.') {
                unget(c);
                if (static_cast<s32>(word(rt_, word(rt_, text_) - 4u)) >= 0) call(0x08B88FF8u, {text_});
                c = static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt_.memory().load8(word(rt_, text_)))));
            }
        }
        if (c == '0') {
            c = get();
            if (c == 'x' || c == 'X') {
                u32 v = get();
                if (is_hex(v)) {
                    do {
                        push(v);
                        v = get();
                    } while (is_hex(v));
                    unget(v);
                    return kNumber;
                }
                unget(v);
                unget(c);
                return decimal(0x30u);
            }
            if (c == 'b' || c == 'B') {
                u32 v = get();
                if (v - 0x30u < 2u) {
                    do {
                        push(v);
                        v = get();
                    } while (v - 0x30u < 2u);
                    unget(v);
                    return kNumber;
                }
                unget(v);
                unget(c);
                return decimal(0x30u);
            }
            if (c - 0x30u < 8u) {
                do {
                    push(c);
                    c = get();
                } while (c - 0x30u < 8u);
                unget(c);
                return kNumber;
            }
            unget(c);
            return decimal(0x30u);
        }
        if (is_digit(c)) return decimal(c);
        if (c == '.') return leading_dot();
        if (c == '"') return string_literal();
        // anything else (a lone sign included): the text becomes that one character
        const u32 len = word(rt_, word(rt_, text_) - 0xCu);
        call(0x08BEFC38u, {text_, 0u, len, 1u, static_cast<u32>(static_cast<std::int32_t>(static_cast<std::int8_t>(c)))});
        return c;
    }

private:
    u32 get() { return call(0x089865DCu, {stream_}); }
    u32 raw() { return call(0x089EC9C8u, {stream_}); }
    void unget(u32 c) { call(0x089EC958u, {stream_, c}); }

    u32 call(u32 target, std::initializer_list<u32> arguments) {
        unsigned reg = 4u;
        for (const u32 argument : arguments) {
            if (reg <= 11u) ctx_.gpr[reg++] = argument;
        }
        ctx_.gpr[2] = call_guest(rt_, ctx_, target, arguments);
        return ctx_.gpr[2];
    }

    // The inlined string push_back: grow when the capacity is short or the body is shared, store the byte, set the length,
    // clear the refcount and write the terminator.
    void push(u32 c) {
        u32 body = word(rt_, text_);
        u32 len = word(rt_, body - 0xCu);
        const u32 grown = len + 1u;
        if (word(rt_, body - 8u) < grown || static_cast<s32>(word(rt_, body - 4u)) > 0) {
            call(0x08B864ECu, {text_, grown});
            body = word(rt_, text_);
            len = word(rt_, body - 0xCu);
        }
        const std::uint8_t terminator = rt_.memory().load8(0x08CB7430u);
        rt_.memory().store8(body + len, static_cast<std::uint8_t>(c));
        const u32 body2 = word(rt_, text_);
        put(rt_, body2 - 4u, 0u);
        put(rt_, body2 - 0xCu, grown);
        rt_.memory().store8(body2 + grown, terminator);
    }

    u32 name(u32 c) {
        for (;;) {
            push(c);
            c = get();
            if (c == kEnd) break;
            if (is_alpha(c) || is_digit(c) || c == '_') continue;
            break;
        }
        unget(c);
        return kName;
    }

    u32 quoted() {
        for (;;) {
            const u32 v = raw();
            if (v == kEnd) return kEnd;
            if (v == 0x27u) return kName;
            push(v);
        }
    }

    // after an exponent letter `e` was read: optional sign, then digits; otherwise everything read is pushed back
    u32 exponent(u32 e) {
        u32 c = get();
        if (c == '+' || c == '-') {
            u32 d = get();
            if (is_digit(d)) {
                push(e);
                push(c);
                do {
                    push(d);
                    d = get();
                } while (is_digit(d));
                unget(d);
            } else {
                unget(d);
                unget(c);
                unget(e);
            }
        } else if (is_digit(c)) {
            push(e);
            do {
                push(c);
                c = get();
            } while (is_digit(c));
            unget(c);
        } else {
            unget(c);
            unget(e);
        }
        return kNumber;
    }

    u32 decimal(u32 c) {
        do {
            push(c);
            c = get();
        } while (is_digit(c));
        if (c == '.') {
            u32 f = get();
            if (is_digit(f)) {
                push('.');
                do {
                    push(f);
                    f = get();
                } while (is_digit(f));
            }
            if (f == 'e' || f == 'E') return exponent(f);
            unget(f);
            return kNumber;
        }
        if (c == 'E') return exponent(c);  // 'e' is not tested here (compiler quirk of the original)
        unget(c);
        return kNumber;
    }

    u32 leading_dot() {
        u32 f = get();
        if (!is_digit(f)) {
            unget(f);
            const u32 len = word(rt_, word(rt_, text_) - 0xCu);
            call(0x08BEFC38u, {text_, 0u, len, 1u, 0x2Eu});
            return 0x2Eu;
        }
        push('.');
        do {
            push(f);
            f = get();
        } while (is_digit(f));
        if (f == 'e' || f == 'E') return exponent(f);
        unget(f);
        return kNumber;
    }

    u32 string_literal() {
        for (;;) {
            u32 a = raw();
            if (a == kEnd) return kEnd;
            if (a == '"') return kString;
            u32 ch = 0u;
            bool mapped = false;
            if (a == '\\') {
                a = raw();
                if (a - 0x61u < 0x16u) {
                    mapped = true;
                    switch (a) {
                    case 'a': ch = 7u; break;
                    case 'b': ch = 8u; break;
                    case 'f': ch = 0xCu; break;
                    case 'n': ch = 0xAu; break;
                    case 'r': ch = 0xDu; break;
                    case 't': ch = 9u; break;
                    case 'v': ch = 0xBu; break;
                    default: mapped = false; break;
                    }
                }
                if (!mapped) {
                    if (a == kEnd) return kEnd;
                    if (a == 0u) continue;
                    ch = a;
                }
            } else {
                if (a == 0u) continue;
                ch = a;
            }
            push(ch);
        }
    }

    Runtime &rt_;
    AllegrexContext &ctx_;
    u32 stream_;
    u32 text_;
};

void func_089868A8(Runtime &rt, AllegrexContext &ctx) {
    const u32 stream = ctx.gpr[4];
    const u32 text = ctx.gpr[5];
    u32 result;
    {
        SavedRegisters saved(ctx, {16u, 17u, 18u, 19u, 20u, 21u, 22u, 23u});
        GuestFrame frame(ctx, 0x30u);
        ctx.gpr[18] = stream;
        ctx.gpr[19] = text;
        Scanner scanner(rt, ctx, stream, text);
        result = scanner.scan();
        ctx.gpr[16] = result;
        ctx.gpr[2] = result;
    }
    ctx.gpr[2] = result;
}

} // namespace
} // namespace new_batch_main_1087
} // namespace gt::game

#define NEW1087(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1087::name, gt::ReturnKind::kind)
NEW1087(0x089868A8u, func_089868A8, Word);
