// Hot batch 327: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_327 {
namespace {

using namespace gt::game::hot;

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }
// `ins value, source, position, 1`: the bit at `position` replaced by source's lowest bit.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position) {
    return (value & ~(1u << position)) | ((source & 1u) << position);
}
// The `lui` half of an address whose `lo` is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
std::uint32_t bits_of(float value) { return std::bit_cast<std::uint32_t>(value); }
// `trunc.w.s` as the recompiled baseline does it: NaN and +inf give 0x7FFFFFFF, -inf 0x80000000, finite values
// truncate toward zero and clamp to the int32 range.
std::uint32_t trunc_to_word(float value) {
    if (std::isnan(value)) return 0x7FFFFFFFu;
    const double t = std::trunc(static_cast<double>(value));
    if (t >= 2147483647.0) return 0x7FFFFFFFu;
    if (t <= -2147483648.0) return 0x80000000u;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(t));
}
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
// Allegrex `trunc.w.s`: toward zero; NaN and out of range give 0x7FFFFFFF.
std::int32_t trunc_w_s(float value) {
    if (!(value >= -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}
// `div`: lo the quotient, hi the remainder, both toward zero; INT_MIN / -1 gives INT_MIN and 0 (no host trap), and a
// zero divisor leaves hi = the dividend and lo = -1 (dividend >= 0) or 1, as Allegrex does.
void divide(psprecomp::AllegrexContext &ctx, std::uint32_t dividend, std::uint32_t divisor) {
    const auto a = static_cast<std::int32_t>(dividend);
    const auto b = static_cast<std::int32_t>(divisor);
    if (b == 0) { ctx.lo = a >= 0 ? 0xFFFFFFFFu : 1u; ctx.hi = dividend; return; }
    if (b == -1) { ctx.lo = static_cast<std::uint32_t>(0u - dividend); ctx.hi = 0u; return; }
    ctx.lo = static_cast<std::uint32_t>(a / b);
    ctx.hi = static_cast<std::uint32_t>(a % b);
}
// A `jal` with the registers as they stand (the caller's frame already open); v0 / v1 / f0 back as the callee left them.
void call_here(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// The same inside a frame of `bytes` (`addiu sp, sp, -bytes; sw ra; jal; ...; jr ra`).
void call_with(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::uint32_t bytes) {
    GuestFrame frame(ctx, bytes);
    call_here(rt, ctx, target);
}

using u32 = std::uint32_t;
using s32 = std::int32_t;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
u32 call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// Where the prologue of a function stores a callee-saved register: `reg` 0-31 is a general register, 32-63 a floating one (32 + its number).
struct SavedSlot {
    unsigned reg;
    u32 offset;
};

// The callee-saved registers (s0-s7, fp, f20-f31) a function keeps values in, and the stores of its prologue: the rewrite stores them where the asm
// does (a callee can read the stack below its caller's frame), sets the registers as the asm does because callees can read them too, and gives
// the caller's values back when it returns.
class SavedRegisters {
public:
    SavedRegisters(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, std::initializer_list<SavedSlot> slots) : ctx_(ctx) {
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
    psprecomp::AllegrexContext &ctx_;
    u32 gpr_[8];
    u32 fp_;
    float fpr_[12];
};

u32 signed_byte(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int8_t>(rt.memory().load8(address))));
}
u32 signed_half(psprecomp::Runtime &rt, u32 address) {
    return static_cast<u32>(static_cast<s32>(static_cast<std::int16_t>(rt.memory().load16(address))));
}

u32 wsbw(u32 value) { return ((value & 0xFFu) << 24) | ((value & 0xFF00u) << 8) | ((value & 0xFF0000u) >> 8) | (value >> 24); }
// A `lw v1, 0(object); lw v0, offset(v1); jalr v0` call: v1 = the vtable until the callee sets it.
u32 call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 object, u32 offset, std::initializer_list<u32> arguments) {
    const u32 table = word(rt, object);
    ctx.gpr[3] = table;
    return call_indirect(rt, ctx, word(rt, table + offset), arguments);
}

// The `bne v0, v1; addiu v0, v0, -1` loop the compiler emitted as a delay: it ends with v0 = -2 (and v1 = -1 as it was set up).
void delay_loop(psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0xFFFFFFFEu;
    ctx.gpr[3] = 0xFFFFFFFFu;
}


// 0x08A5BB7C: the null-handle report every checked handle load calls with its message in a0.
void report_null(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 message) { call_set(rt, ctx, 0x08A5BB7Cu, {message}); }

// Drops one reference of the string whose characters start at `data` (representation at data - 0xC, count at data - 4): the shared empty
// representation `empty` is left alone, otherwise 0x08B60804 decrements the count and `release` frees the representation (a1 = the stack
// address sp + release_slot) at zero or below. v0 is the shared representation's address until a call replaces it.
void drop_string_reference(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, u32 data, u32 empty, u32 release,
                           unsigned representation_register = 16u, u32 release_slot = 0u) {
    const u32 representation = data - 0xCu;
    ctx.gpr[2] = empty;
    ctx.gpr[4] = data - 4u;
    if (representation == empty) return;
    ctx.gpr[representation_register] = representation;
    const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
    ctx.gpr[4] = representation;
    if (remaining <= 0) call_set(rt, ctx, release, {representation, frame.at(release_slot)});
}

// ll / sc decrement of the word at `address` (nothing else runs here, so the store always succeeds and v0 is 1 afterwards): true when it was 1.
bool decrement_reaches_zero(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 address) {
    const u32 old = word(rt, address);
    put(rt, address, old - 1u);
    ctx.gpr[2] = 1u;
    return old == 1u;
}

// Drops one use of a shared holder (use count +4, weak count +8): the last use calls the virtual +8, the last weak count the virtual +0xC.
void release_holder(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 holder) {
    if (!decrement_reaches_zero(rt, ctx, holder + 4u)) return;
    call_virtual(rt, ctx, holder, 8u, {holder});
    if (!decrement_reaches_zero(rt, ctx, holder + 8u)) return;
    call_virtual(rt, ctx, holder, 0xCu, {holder});
}

// 0x08B764AC (a0 = the target, a1 = the first node, a2 = the limit): walks the linked records the same way as 0x08B7634C (relative links, the word +4 the distance back to the
// owner entry, a word 0 at +0 ending the list); for every record with an owner the type code and selector (0x08B75F68 / 0x08B75EDC, re-asked when the owner changes and flag 4 of the
// word +0x10 of a0 is set) read the record's value and extent (0x08B75DA0 into sp and sp + 4; without a type code the words +8 / +0xC); a non-zero value
// whose offset from the limit is within the extent (a2 - value < extent, unsigned) returns the record. v0 = that record, 0 when none.
void find_node_covering(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    u32 node = ctx.gpr[5];
    const u32 limit = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {31u, 0x2Cu}, {22u, 0x28u}, {16u, 0x10u}});
    ctx.gpr[19] = target;
    ctx.gpr[17] = node;
    ctx.gpr[21] = limit;
    u32 code = (word(rt, target + 0x10u) >> 3) & 0xFFu;
    ctx.gpr[18] = code;
    u32 selector = call_set(rt, ctx, 0x08B75EDCu, {code, target});
    ctx.gpr[20] = selector;
    if (word(rt, node) == 0u) {
        ctx.gpr[17] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    u32 previous_owner = 0u;
    ctx.gpr[22] = 0u;
    for (;;) {
        const u32 back = word(rt, node + 4u);
        if (back != 0u) {
            if ((word(rt, target + 0x10u) & 4u) != 0u) {
                const u32 owner = node + 4u - back;
                ctx.gpr[16] = owner;
                if (previous_owner != owner) {
                    const u32 type = call_set(rt, ctx, 0x08B75F68u, {owner});
                    selector = call_set(rt, ctx, 0x08B75EDCu, {type & 0xFFu, target});
                    code = type;
                    ctx.gpr[18] = code;
                    ctx.gpr[20] = selector;
                    previous_owner = owner;
                    ctx.gpr[22] = owner;
                }
            }
            u32 value;
            u32 extent;
            bool candidate;
            if (code == 0u) {
                value = word(rt, node + 8u);
                extent = word(rt, node + 0xCu);
                put(rt, frame.at(0u), value);
                put(rt, frame.at(4u), extent);
                candidate = value != 0u;
            } else {
                const u32 kind = code & 0xFFu;
                ctx.gpr[16] = kind;
                const u32 fetched = call_set(rt, ctx, 0x08B75DA0u, {kind, selector, node + 8u, frame.at(0u)});
                call_set(rt, ctx, 0x08B75DA0u, {kind & 0xFu, 0u, fetched, frame.at(4u)});
                const u32 width = call_set(rt, ctx, 0x08B75C8Cu, {kind});
                u32 mask = 0xFFFFFFFFu;
                if (width < 4u) mask = (1u << ((width << 3) & 31u)) - 1u;
                value = word(rt, frame.at(0u));
                extent = word(rt, frame.at(4u));
                candidate = (mask & value) != 0u;
            }
            if (candidate && (limit - value) < extent) {
                ctx.gpr[2] = node;
                return;
            }
        }
        const u32 following = word(rt, node) + node;
        if (word(rt, following + 4u) == 0u) {
            ctx.gpr[17] = 0u;
            ctx.gpr[2] = 0u;
            return;
        }
        node = following + 4u;
        ctx.gpr[17] = node;
    }
}

// The single-precision float of a soft-float value (class, sign, exponent, mantissa with seven guard bits), rounded to nearest even as 0x08B72564 does it: classes 0 and 1 give an infinity
// with the mantissa bits kept (a NaN), 4 an infinity, 2 zero, the rest a number whose exponent below -126 is shifted into a denormal and from 128 up is an infinity.
struct PackedFloat {
    u32 bits;
    u32 mantissa;
};
PackedFloat pack_soft_float(u32 kind, u32 sign, s32 exponent, u32 mantissa) {
    u32 field = 0u;
    u32 exponent_field = 0u;
    if (kind < 2u) {
        field = (mantissa | 0x000FFFFFu) & 0x7FFFFFu;
        exponent_field = 0xFFu;
    } else if (kind == 4u) {
        exponent_field = 0xFFu;
    } else if (kind == 2u || mantissa == 0u) {
        field = 0u;
    } else if (exponent < -126) {
        const u32 shift = static_cast<u32>(-126 - exponent);
        u32 value = 0u;
        u32 low = 0u;
        if (static_cast<s32>(shift) < 26) {
            const u32 sticky = (mantissa & ((1u << (shift & 31u)) - 1u)) != 0u ? 1u : 0u;
            value = (mantissa >> (shift & 31u)) | sticky;
            low = value & 0x7Fu;
        }
        if (low == 0x40u) {
            if ((value & 0x80u) != 0u) value += 0x40u;
        } else {
            value += 0x3Fu;
        }
        exponent_field = 0x3FFFFFFFu < value ? 1u : 0u;
        field = (value >> 7) & 0x7FFFFFu;
    } else if (exponent >= 128) {
        exponent_field = 0xFFu;
    } else {
        u32 biased = static_cast<u32>(exponent) + 0x7Fu;
        if ((mantissa & 0x7Fu) == 0x40u) {
            if ((mantissa & 0x80u) != 0u) mantissa += 0x40u;
        } else {
            mantissa += 0x3Fu;
        }
        if (static_cast<s32>(mantissa) < 0) {
            mantissa >>= 1;
            biased = static_cast<u32>(exponent) + 0x80u;
        }
        field = (mantissa >> 7) & 0x7FFFFFu;
        exponent_field = biased & 0xFFu;
    }
    return {field | (exponent_field << 23) | ((sign & 1u) << 31), field};
}

// 0x08B72564 (a0 = the soft-float value {class, sign, exponent, mantissa}): the float in f0 (see pack_soft_float); v0 = its mantissa bits.
void soft_float_to_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const PackedFloat packed = pack_soft_float(word(rt, self), word(rt, self + 4u), static_cast<s32>(word(rt, self + 8u)), word(rt, self + 0xCu));
    ctx.fpr[0] = std::bit_cast<float>(packed.bits);
    ctx.gpr[2] = packed.mantissa;
    ctx.gpr[6] = packed.bits;
}

// 0x08A50758 (a0 = the container, a1 = the key string, a2 = the context): a non-empty key first removes the entry whose string (0x08B35FD8 on **entry) equals it from the tree at +0x14
// (count +0x28: 0x08A4FC60 / 0x08B64654 on its value, 0x08B5F444 / 0x08B35254 on the node, count - 1); then a new entry is made (0x08B62344(0x14), 0x08A4FD08) and, when 0x08A4FB80 and
// 0x08A4FA18 accept it, inserted (0x08C1CFD0) and v0 = 1; otherwise it is destroyed and v0 = 0.
void replace_entry_by_key(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 context = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x38u}, {20u, 0x30u}, {18u, 0x28u}, {31u, 0x3Cu}, {21u, 0x34u}, {19u, 0x2Cu}, {17u, 0x24u}, {16u, 0x20u}});
    ctx.gpr[22] = context;
    ctx.gpr[20] = self;
    ctx.gpr[18] = key;
    ctx.gpr[4] = 0u;
    ctx.gpr[2] = 0u;
    if (key == 0u || signed_byte(rt, key) == 0u) return;
    const u32 tree = self + 0x14u;
    ctx.gpr[21] = tree;
    if (word(rt, tree + 0x14u) != 0u) {
        put(rt, frame.at(0u), 0u);
        u32 node = word(rt, tree + 0xCu);
        const u32 end = self + 0x18u;
        ctx.gpr[19] = end;
        ctx.gpr[16] = node;
        while (end != node) {
            const u32 value = word(rt, node + 0x10u);
            ctx.gpr[17] = value;
            if (call_set(rt, ctx, 0x08B35FD8u, {word(rt, value), key}) == 0u) {
                if (value != 0u) {
                    call_set(rt, ctx, 0x08A4FC60u, {value, tree + 4u});
                    call_set(rt, ctx, 0x08B64654u, {value});
                }
                put(rt, frame.at(4u), node);  // the asm stores the node in the delay slot of the 0x08B5F444 call
                const u32 removed = call_set(rt, ctx, 0x08B5F444u, {node, tree + 4u});
                call_set(rt, ctx, 0x08B35254u, {removed});
                put(rt, tree + 0x14u, word(rt, tree + 0x14u) - 1u);
                break;
            }
            node = call_set(rt, ctx, 0x08B5F16Cu, {node});
            ctx.gpr[16] = node;
        }
    }
    const u32 entry = call_set(rt, ctx, 0x08B62344u, {0x14u});
    ctx.gpr[16] = entry;
    call_set(rt, ctx, 0x08A4FD08u, {entry, key});
    bool accepted = false;
    put(rt, frame.at(0u), entry);  // the asm keeps the new entry in sp + 0, which 0x08A4FA18 and the insert read back
    if (call_set(rt, ctx, 0x08A4FB80u, {entry, word(rt, self), context}) != 0u) {
        accepted = call_set(rt, ctx, 0x08A4FA18u, {word(rt, frame.at(0u)), word(rt, self), context}) != 0u;
    }
    if (accepted) {
        const u32 position = call_set(rt, ctx, 0x08C1CFD0u, {tree, frame.at(0u)});
        const u32 second = ctx.gpr[3];
        put(rt, frame.at(0x14u), second);
        const u32 flag = rt.memory().load8(frame.at(0x14u));
        put(rt, frame.at(4u), position);
        rt.memory().store8(frame.at(8u), static_cast<std::uint8_t>(flag));
        put(rt, frame.at(0x10u), position);
        ctx.gpr[4] = 1u;
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[4] = 0u;
    if (entry != 0u) {
        call_set(rt, ctx, 0x08A4FC60u, {entry, word(rt, self)});
        call_set(rt, ctx, 0x08B64654u, {entry});
        ctx.gpr[4] = 0u;
    }
    ctx.gpr[2] = 0u;
}

// 0x088FDDF8 (a0 = a big-endian object with the magic 0x47636966): rebases its pointers by the distance of the object from the base offset in +4 (itself then stored as the address): the
// table at +0x14 (count +0x10; each non-zero entry plus the delta, then 0x088FD9F0(entry, delta) is called with the entry), and the tables at +0x1C and +0x24 (count +0x18, entries rebased the
// same way). v0 = the magic for another object, else 0.
void rebase_big_endian_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}});
    constexpr u32 kMagic = 0x47636966u;
    ctx.gpr[2] = kMagic;
    ctx.gpr[16] = self;
    if (wsbw(word(rt, self)) != kMagic) return;
    const u32 delta = self - wsbw(word(rt, self + 4u));
    ctx.gpr[17] = delta;
    ctx.gpr[18] = 0u;
    put(rt, self + 4u, wsbw(self));
    put(rt, self + 0x14u, wsbw(wsbw(word(rt, self + 0x14u)) + delta));
    const auto rebase_entry = [&](u32 entry_address) {
        const u32 value = wsbw(word(rt, entry_address));
        if (value != 0u) put(rt, entry_address, wsbw(delta + value));
    };
    const u32 first_count = wsbw(word(rt, self + 0x10u));
    for (u32 index = 0u; index < first_count; ++index) {
        const u32 entry_address = wsbw(word(rt, self + 0x14u)) + index * 4u;
        rebase_entry(entry_address);
        call_set(rt, ctx, 0x088FD9F0u, {wsbw(word(rt, wsbw(word(rt, self + 0x14u)) + index * 4u)), delta});
    }
    put(rt, self + 0x1Cu, wsbw(delta + wsbw(word(rt, self + 0x1Cu))));
    for (u32 index = 0u; index < wsbw(word(rt, self + 0x18u)); ++index) rebase_entry(wsbw(word(rt, self + 0x1Cu)) + index * 4u);
    put(rt, self + 0x24u, wsbw(delta + wsbw(word(rt, self + 0x24u))));
    for (u32 index = 0u; index < wsbw(word(rt, self + 0x18u)); ++index) rebase_entry(wsbw(word(rt, self + 0x24u)) + index * 4u);
    ctx.gpr[2] = 0u;
}

// 0x088FB3D8 (a0 = the object, a1 / a2 = two values, a3 = a depth): a depth of 3 or more gives v0 = 1; otherwise 0x088FB2D0 compares the pairs and, for as long as neither slot word (sp, sp + 4) has a
// bit in common with the other, the first set flag of the slot words picks a float of the object (+8, +0x14 for flags 1, 2 with mode 2; +4, +0x10 for 4, 8 with mode 1; +0, +0xC for 0x10, 0x20 with mode
// 0) that 0x088FB338(value, a1, a2, mode) applies; the slot is refreshed by 0x088FB2D0(object, value, depth). v0 = 1 when the words end up disjoint and both empty, else 0.
void reduce_overlapping_flags(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const s32 depth = static_cast<s32>(ctx.gpr[7]);
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {31u, 0x28u}, {21u, 0x24u}, {16u, 0x10u}});
    ctx.gpr[2] = depth < 3 ? 1u : 0u;
    ctx.gpr[20] = static_cast<u32>(depth);
    ctx.gpr[19] = second;
    ctx.gpr[18] = first;
    ctx.gpr[17] = object;
    if (depth >= 3) {
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[6] = static_cast<u32>(depth);
    const u32 first_result = call_set(rt, ctx, 0x088FB2D0u, {ctx.gpr[4], ctx.gpr[5], static_cast<u32>(depth)});
    put(rt, frame.at(0u), first_result);  // the asm stores each result in its slot word (sp, sp + 4) before reading them back
    const u32 answer = call_set(rt, ctx, 0x088FB2D0u, {object, second, static_cast<u32>(depth)});
    ctx.gpr[3] = answer;
    u32 low = word(rt, frame.at(0u));
    put(rt, frame.at(4u), answer);
    ctx.gpr[4] = low;
    ctx.gpr[2] = answer & low;
    if (ctx.gpr[2] != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = answer | low;
    for (;;) {
        ctx.gpr[16] = first;
        if (ctx.gpr[2] == 0u) {
            ctx.gpr[2] = 1u;
            return;
        }
        u32 start = first;
        u32 slot = frame.at(0u);
        if (low == 0u) {
            start = second;
            slot = frame.at(4u);
        }
        ctx.gpr[16] = start;
        ctx.gpr[21] = slot;
        const u32 flags = word(rt, slot);
        ctx.gpr[3] = flags;
        static constexpr struct {
            u32 flag;
            u32 offset;
            u32 mode;
        } kRules[] = {{1u, 8u, 2u}, {2u, 0x14u, 2u}, {4u, 4u, 1u}, {8u, 0x10u, 1u}, {0x10u, 0u, 0u}, {0x20u, 0xCu, 0u}};
        for (const auto &rule : kRules) {
            if ((flags & rule.flag) == 0u) continue;
            ctx.fpr[12] = float_at(rt, object + rule.offset);
            call_set(rt, ctx, 0x088FB338u, {start, first, second, rule.mode});
            break;
        }
        ctx.gpr[4] = object;
        const u32 refreshed = call_set(rt, ctx, 0x088FB2D0u, {object, start, static_cast<u32>(depth)});
        put(rt, slot, refreshed);
        low = word(rt, frame.at(0u));
        const u32 high = word(rt, frame.at(4u));
        ctx.gpr[4] = low;
        ctx.gpr[3] = high;
        ctx.gpr[2] = high & low;
        if (ctx.gpr[2] != 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
        ctx.gpr[2] = high | low;
    }
}

// 0x08B46FB0 (a0 = the object, a1 / a2 / a3 = the arguments passed on): with a non-zero byte +0x8C, 0x08C7AE10(object + 0x70, 0, 0x2000, 0) opens a handle (negative gives v0 = 0), the object's virtual
// +0xA0 gives a slot whose word +0 keeps it, the slot's block (+4) is set up from object + 0x70 (0x08B3F8C0), its bytes +0x30 / +0x32 set and the packet at sp + 6 assembled (0x08B36018, 0x08B3FA98,
// 0x08B3F8C0, 0x08B3FA3C, 0x08B3F8A0, 0x08B3FA54) and handed to 0x08C7AE48(handle, size, length, a1, a2, 1000000, 1); the slot is passed to the virtual +0xA8, marked -1 first when it failed. v0 = 1 on success.
void start_transfer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 first = ctx.gpr[5];
    const u32 second = ctx.gpr[6];
    const u32 flags = ctx.gpr[7] & 0xFFFFu;
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{23u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {18u, 0x28u}, {17u, 0x24u}, {31u, 0x40u}, {20u, 0x30u}, {19u, 0x2Cu}, {16u, 0x20u}});
    ctx.gpr[23] = first;
    ctx.gpr[22] = second;
    ctx.gpr[21] = flags;
    ctx.gpr[18] = object;
    const u32 stream = object + 0x70u;
    ctx.gpr[17] = stream;
    ctx.gpr[2] = rt.memory().load8(object + 0x8Cu);
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 handle = call_set(rt, ctx, 0x08C7AE10u, {stream, 0u, 0x2000u, 0u});
    const u32 packet = frame.at(6u);
    ctx.gpr[20] = packet;
    ctx.gpr[16] = handle;
    if (static_cast<s32>(handle) < 0) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 slot = call_indirect(rt, ctx, word(rt, word(rt, object) + 0xA0u), {object, 0u});
    ctx.gpr[19] = slot;
    put(rt, slot, handle);
    call_set(rt, ctx, 0x08B3F8C0u, {slot + 4u, stream});
    rt.memory().store8(slot + 0x30u, 1u);
    rt.memory().store8(slot + 0x32u, 1u);
    call_set(rt, ctx, 0x08B36018u, {frame.at(0u), 0xFFu, 6u});
    call_set(rt, ctx, 0x08B3FA98u, {packet});
    call_set(rt, ctx, 0x08B3F8C0u, {packet, frame.at(0u)});
    call_set(rt, ctx, 0x08B3FA3Cu, {packet, flags});
    const u32 stored = word(rt, slot);
    ctx.gpr[17] = stored;
    const u32 size = call_set(rt, ctx, 0x08B3F8A0u, {packet});
    ctx.gpr[16] = size;
    const u32 length = call_set(rt, ctx, 0x08B3FA54u, {packet});
    const s32 result = static_cast<s32>(call_set(rt, ctx, 0x08C7AE48u, {stored, size, length & 0xFFFFu, first, second, 0x000F4240u, 1u}));
    if (result >= 0) {
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0xA8u), {object, slot});
        ctx.gpr[2] = 1u;
        return;
    }
    put(rt, slot, 0xFFFFFFFFu);
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0xA8u), {object, slot});
    ctx.gpr[2] = 0u;
}

// 0x08B43E44 (a0 = the object, a1 = a kind, a2 = an argument, a3 = a halfword, t0 = the payload, t1 = its length): when the object's virtual +0xA4 allows it and the kind is 0 or 2 with a payload of at most 0x400 bytes, a
// message record (halfwords a3 and the length, then the payload, 0x08B79A88) is built at sp + 0x10 and under the lock at +0x12C (0x08B344E0 / 0x08B3457C) delivered: kind 0 through 0x08B42A90(object, 0x4E2C, record, length + 0x14,
// 1, 1, 0, 0), kind 2 through the record found by 0x08B3EE8C(object + 0x8DC) and 0x08B40950(object, record, 0x4E2C, sp, length + 0x14, 1). v0 = 1 when delivered or the kind is not 0 / 2, else 0.
void send_message_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 kind = ctx.gpr[5];
    const u32 argument = ctx.gpr[6];
    const u32 halfword = ctx.gpr[7];
    const u32 payload = ctx.gpr[8];
    const u32 length = ctx.gpr[9];
    GuestFrame frame(ctx, 0x450u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x434u}, {20u, 0x430u}, {19u, 0x42Cu}, {18u, 0x428u}, {17u, 0x424u}, {16u, 0x420u}, {31u, 0x444u}, {30u, 0x440u}, {23u, 0x43Cu}, {22u, 0x438u}});
    ctx.gpr[21] = argument;
    ctx.gpr[20] = halfword;
    ctx.gpr[19] = payload;
    ctx.gpr[18] = kind;
    ctx.gpr[17] = length;
    ctx.gpr[16] = object;
    if (call_indirect(rt, ctx, word(rt, word(rt, object) + 0xA4u), {object}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[22] = 2u;
    call_set(rt, ctx, 0x08B36018u, {frame.at(0u), 0u, 0x414u});
    if (kind != 2u && kind != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    if (!(length < 0x401u)) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 lock = object + 0x12Cu;
    ctx.gpr[23] = lock;
    const u32 total = length + 0x14u;
    ctx.gpr[30] = total;
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x90u), {object});
    rt.memory().store16(frame.at(0x10u), static_cast<std::uint16_t>(halfword));
    rt.memory().store16(frame.at(0x12u), static_cast<std::uint16_t>(length));
    call_set(rt, ctx, 0x08B79A88u, {frame.at(0x14u), payload, length});
    call_set(rt, ctx, 0x08B344E0u, {lock});
    bool delivered = true;
    if (kind == 0u) {
        delivered = call_set(rt, ctx, 0x08B42A90u, {object, 0x4E2Cu, frame.at(0u), total, 1u, 1u, 0u, 0u}) != 0u;
    } else if (kind == 2u) {
        const u32 record = call_set(rt, ctx, 0x08B3EE8Cu, {object + 0x8DCu, argument});
        delivered = false;
        if (record != 0u) delivered = call_set(rt, ctx, 0x08B40950u, {object, record, 0x4E2Cu, frame.at(0u), total, 1u}) != 0u;
    }
    call_set(rt, ctx, 0x08B3457Cu, {lock});
    ctx.gpr[2] = delivered ? 1u : 0u;
}

// 0x08AE9798 (a0 = {+0 first record, +4 last record, +8 count, +0xC cursor}, a1 = a key): the records are 0x38 bytes in the table whose address is the word 0x08D40A3C. The first loop takes up to `count`
// records from the cursor (wrapping to the first after the last) and returns the word +0 of the first one whose flag bit 0 (+4) is clear. When all are set: a key >= 0 picks, among the records from first to last
// whose signed halfword +6 is >= the key, the one with the smallest value (ties: the larger word +0xC); a key < 0 the one with the largest value above the key. v0 = its word +0, -1 when none.
void pick_record_by_priority(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 key = static_cast<s32>(ctx.gpr[5]);
    const u32 first = word(rt, self);
    const s32 last = static_cast<s32>(word(rt, self + 4u));
    const s32 count = static_cast<s32>(word(rt, self + 8u));
    s32 cursor = static_cast<s32>(word(rt, self + 0xCu));
    if (count > 0) {
        const u32 table = word(rt, 0x08D40A3Cu);
        for (s32 taken = 0; taken < count;) {
            const u32 record = table + static_cast<u32>(cursor) * 0x38u;
            const u32 flags = rt.memory().load8(record + 4u) & 1u;
            ++cursor;
            ++taken;
            if (last < cursor) cursor = static_cast<s32>(first);
            if (flags == 0u) {
                ctx.gpr[2] = word(rt, record);
                return;
            }
        }
    }
    s32 best_value = -1;
    u32 best_word = 0xFFFFFFFFu;
    s32 best_second = -1;
    if (last < static_cast<s32>(first)) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    const u32 table = word(rt, 0x08D40A3Cu);
    for (s32 index = static_cast<s32>(first); index <= last; ++index) {
        const u32 record = table + static_cast<u32>(index) * 0x38u;
        const s32 value = static_cast<s32>(signed_half(rt, record + 6u));
        if (key >= 0) {
            if (value < key) continue;
            const s32 second = static_cast<s32>(word(rt, record + 0xCu));
            const bool better = static_cast<s32>(best_word) < 0 || value < best_value || (value == best_value && best_second < second);
            if (!better) continue;
            best_word = word(rt, record);
            best_value = value;
            best_second = second;
        } else {
            if (!(key < value)) continue;
            if (static_cast<s32>(best_word) >= 0 && !(best_value < value)) continue;
            best_word = word(rt, record);
            best_value = value;
        }
    }
    ctx.gpr[2] = best_word;
    if (static_cast<s32>(best_word) < 0) ctx.gpr[2] = 0xFFFFFFFFu;
}

// 0x088F360C (a0 passed on, a1 = the array of 16-byte records, a2 = the first index, a3 = the last): a quicksort by the float at +8 of each record (pivot = the middle one, records swapped as four words); the
// left part is sorted by a recursive call, the loop goes on with the right part. v0 = first + last of the final range, v1 = the last scan count (or the callee's), f0 the last float read.
void sort_records_by_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 base = ctx.gpr[5];
    s32 low = static_cast<s32>(ctx.gpr[6]);
    const s32 high = static_cast<s32>(ctx.gpr[7]);
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {31u, 0x20u}, {16u, 0x10u}});
    if (high < 0) return;
    const auto record = [&](s32 index) { return base + static_cast<u32>(index) * 16u; };
    do {
        const float pivot = float_at(rt, record((low + high) >> 1) + 8u);
        s32 i = low;
        s32 j = high;
        for (;;) {
            ctx.fpr[0] = float_at(rt, record(i) + 8u);
            while (ctx.fpr[0] < pivot) {
                ++i;
                ctx.fpr[0] = float_at(rt, record(i) + 8u);
            }
            ctx.fpr[0] = float_at(rt, record(j) + 8u);
            u32 passes = 0u;
            if (pivot < ctx.fpr[0]) {
                u32 address = record(j) - 8u;
                bool again;
                do {
                    ctx.fpr[0] = float_at(rt, address);
                    ++passes;
                    again = pivot < ctx.fpr[0];
                    address -= 16u;
                } while (again);
            }
            ctx.gpr[3] = passes;
            j -= static_cast<s32>(passes);
            if (!(i < j)) break;
            const u32 left = record(i);
            const u32 right = record(j);
            const u32 w0 = word(rt, left);
            const u32 w4 = word(rt, left + 4u);
            const u32 w8 = word(rt, left + 8u);
            const u32 wc = word(rt, left + 0xCu);
            put(rt, frame.at(4u), w4);
            put(rt, frame.at(0xCu), wc);
            put(rt, frame.at(0u), w0);
            put(rt, frame.at(8u), w8);
            put(rt, left + 4u, word(rt, right + 4u));
            put(rt, left + 0xCu, word(rt, right + 0xCu));
            put(rt, left + 8u, word(rt, right + 8u));
            const u32 r0 = word(rt, right);
            put(rt, left, r0);
            put(rt, right + 0xCu, wc);
            put(rt, right, w0);
            put(rt, right + 4u, w4);
            put(rt, right + 8u, w8);
            ctx.gpr[3] = w0;
            ++i;
            --j;
        }
        if (low < i - 1) call_set(rt, ctx, 0x088F360Cu, {object, base, static_cast<u32>(low), static_cast<u32>(i - 1)});
        low = j + 1;
        ctx.gpr[2] = static_cast<u32>(low + high);
    } while (low < high);
}

// 0x08B441C4 (a0 = the object, a2 = a key for the list at +0x8DC, a3 = a record, t0 = the kind (0xC, otherwise v0 = 0xC), t2 / t3 = two words): a node of the list (0x08B3F144) gets the two words (+0x38 / +0x3C),
// state 4 (+0x28) and the halfword +8 of the record (or its own first halfword) as its second; the virtual +0x98 gives the halfword kept at object + 0xBD0; the node is sent to the others (0x08B40950 / 0x08B42A90
// with the 0x58-byte block at sp + 0x10) and every other owner in the list gets the same notice. v0 = the last call's result.
void announce_list_node(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 key = ctx.gpr[6];
    const u32 record = ctx.gpr[7];
    const u32 kind = ctx.gpr[8];
    const u32 low = ctx.gpr[10];
    const u32 high = ctx.gpr[11];
    GuestFrame frame(ctx, 0x90u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x80u}, {19u, 0x7Cu}, {17u, 0x74u}, {16u, 0x70u}, {31u, 0x88u}, {21u, 0x84u}, {18u, 0x78u}});
    ctx.gpr[2] = 0xCu;
    if (kind != 0xCu) return;
    const u32 list = object + 0x8DCu;
    ctx.gpr[21] = list;
    const u32 node = call_set(rt, ctx, 0x08B3F144u, {list, key});
    ctx.gpr[18] = node;
    if (node == 0u) return;
    put(rt, node + 0x38u, low);
    put(rt, node + 0x3Cu, high);
    put(rt, node + 0x28u, 4u);
    const u32 half = rt.memory().load16(record + 8u);
    rt.memory().store16(node + 2u, static_cast<std::uint16_t>(half == 0u ? rt.memory().load16(node) : half));
    const u32 handle_slot = frame.at(0x18u);
    const u32 block = frame.at(0x10u);
    const u32 halfword = call_virtual(rt, ctx, object, 0x98u, {object});
    rt.memory().store16(object + 0xBD0u, static_cast<std::uint16_t>(halfword));
    put(rt, frame.at(0u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0xCu), 0u);
    call_set(rt, ctx, 0x08B40950u, {object, node, 0x4E22u, frame.at(0u), 0x10u, 1u});
    call_set(rt, ctx, 0x08B3F478u, {handle_slot});
    call_set(rt, ctx, 0x08B36018u, {block, 0u, 0x58u});
    call_set(rt, ctx, 0x08B3E7FCu, {handle_slot, node});
    call_set(rt, ctx, 0x08B42A90u, {object, 0x4E24u, block, 0x58u, 0u, 1u, 0u, 0u});
    u32 cursor = call_set(rt, ctx, 0x08B3F3C8u, {list, node});
    if (cursor == 0u) return;
    ctx.gpr[2] = word(rt, cursor);
    for (;;) {
        const u32 owner = ctx.gpr[2];
        if (owner != 0u && owner != node) {
            call_set(rt, ctx, 0x08B3E7FCu, {handle_slot, owner});
            call_set(rt, ctx, 0x08B40950u, {object, node, 0x4E24u, block, 0x58u, 1u});
        }
        cursor = word(rt, cursor + 8u);
        if (cursor == 0u) return;
        ctx.gpr[2] = word(rt, cursor);
    }
}

// 0x08915E94 (a0 = the object, a1 = a source, a2 = a flag byte): f0 = 1.0 for a zero flag, else 0.0, stored at +0xC (+0x1C and +0x2C cleared); with the flag set 0x088FBA1C(source, object + 0x10 / + 0x20, sp) is read twice into
// {non-zero flag byte, float} records at sp + 8 / sp + 0x10; when both are non-zero the two floats (sp + 0xC, sp + 0x14) are ordered so the distance between them is
// at most half of 0x088FB028(source): the lower goes to +0x1C and the higher to +0x2C, or the other way round when the span is shorter than that half.
void init_float_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x30u}, {19u, 0x2Cu}, {31u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}, {53u, 0x44u}, {52u, 0x40u}, {22u, 0x4u}});
    ctx.fpr[0] = flag != 0u ? 0.0f : 1.0f;
    put_float(rt, object + 0xCu, ctx.fpr[0]);
    put(rt, object + 0x1Cu, 0u);
    put(rt, object + 0x2Cu, 0u);
    if (flag == 0u) return;
    for (u32 index = 0; index < 2u; ++index) {
        put(rt, frame.at(0u), 0u);
        call_set(rt, ctx, 0x088FBA1Cu, {source, object + 0x10u + 0x10u * index, frame.at(0u)});
        const u32 record = frame.at(8u) + 8u * index;
        rt.memory().store8(record, static_cast<std::uint8_t>(ctx.fpr[0] != 0.0f ? 1u : 0u));
        put_float(rt, record + 4u, ctx.fpr[0]);
    }
    ctx.gpr[2] = rt.memory().load8(frame.at(8u));
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = rt.memory().load8(frame.at(0x10u));
    if (ctx.gpr[2] == 0u) return;
    const float first = float_at(rt, frame.at(0xCu));
    const float second = float_at(rt, frame.at(0x14u));
    if (first <= second && second <= first) {
        put_float(rt, object + 0x2Cu, second);
        put_float(rt, object + 0x1Cu, first);
        return;
    }
    call_set(rt, ctx, 0x088FB028u, {source});
    ctx.fpr[0] = psp_mul(ctx.fpr[0], 0.5f);
    const bool shorter = first <= second ? ctx.fpr[0] < second - first : first - second < ctx.fpr[0];
    if (!shorter) {
        put_float(rt, object + 0x2Cu, second);
        put_float(rt, object + 0x1Cu, first);
    } else {
        ctx.fpr[0] = second;
        put_float(rt, object + 0x2Cu, first);
        put_float(rt, object + 0x1Cu, second);
    }
}

// 0x08BE0238 (a0 = the object): destructor of an object with three shared holders: the vtable word +0 goes to 0x08CC6850, the holder at +0x44 is released, the vtable word to 0x08CC6838, 0x08B28DC4(object + 0x18) runs, then
// the holders at +0x10 and +8 are released; the object is then handed to 0x08B64654 as a tail call.
void release_shared_members(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
        ctx.gpr[17] = self;
        u32 holder = word(rt, self + 0x44u);
        ctx.gpr[16] = holder;
        put(rt, self, 0x08CC6850u);
        if (holder != 0u) release_holder(rt, ctx, holder);
        ctx.gpr[2] = 0x08CC6838u;
        put(rt, self, 0x08CC6838u);
        call_set(rt, ctx, 0x08B28DC4u, {self + 0x18u});
        holder = word(rt, self + 0x10u);
        ctx.gpr[16] = holder;
        if (holder != 0u) release_holder(rt, ctx, holder);
        holder = word(rt, self + 8u);
        ctx.gpr[16] = holder;
        if (holder != 0u) release_holder(rt, ctx, holder);
        ctx.gpr[4] = self;
    }
    call_set(rt, ctx, 0x08B64654u, {self});
}

// 0x088AC8F0 (a0 = the object): the step (+0x18 - +4) times the scale at +0x434 is put through 0x08B4A560 on the vector at +0x2C (copied from +0x3C / +0x40), its two scaled components (+0x44 / +0x48) go to +0x34 / +0x38, the
// vector at +0x1C (copied from +8 / +0xC) is combined by 0x08B4AC0C, the box at +0x4C..+0x58 is the corners (+8 or +0x1C and +0xC or +0x20, widened by +0x3C4, ordered by the signs of +0x2C and +0x30) and
// its extents go to +0x5C / +0x60; 0x088AB798(object, object + 0x18, 0) runs and three flag bytes (+0x439 .. +0x43B) are cleared.
void update_scaled_box(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}, {52u, 0x20u}});
    const u32 vector_a = self + 0x2Cu;
    const u32 vector_b = self + 0x1Cu;
    float step = float_at(rt, self + 0x18u) - float_at(rt, self + 4u);
    const float scale = float_at(rt, self + 0x434u);
    const float first = float_at(rt, self + 0x3Cu);
    const float second = float_at(rt, self + 0x40u);
    step = psp_mul(step, scale);
    put_float(rt, vector_a, first);
    put_float(rt, vector_a + 4u, second);
    ctx.fpr[12] = step;
    ctx.fpr[20] = step;
    call_set(rt, ctx, 0x08B4A560u, {vector_a});
    const float scaled_y = psp_mul(step, float_at(rt, self + 0x48u));
    const float scaled_x = psp_mul(step, float_at(rt, self + 0x44u));
    ctx.fpr[20] = scaled_y;
    put_float(rt, vector_b, float_at(rt, self + 8u));
    put_float(rt, self + 0x34u, scaled_x);
    put_float(rt, self + 0x38u, scaled_y);
    put_float(rt, vector_b + 4u, float_at(rt, self + 0xCu));
    call_set(rt, ctx, 0x08B4AC0Cu, {vector_b, vector_b, vector_a});
    put_float(rt, self + 0x24u, float_at(rt, self + 0x10u) + float_at(rt, self + 0x34u));
    put_float(rt, self + 0x28u, float_at(rt, self + 0x14u) + float_at(rt, self + 0x38u));
    const float sign_x = float_at(rt, self + 0x2Cu);
    const float low_x = float_at(rt, self + 8u);
    const float high_y = float_at(rt, self + 0xCu);
    const float low_y = float_at(rt, self + 0x20u);
    const float high_x = float_at(rt, self + 0x1Cu);
    const float margin = float_at(rt, self + 0x3C4u);
    float corner_a;
    float corner_b;
    if (sign_x < 0.0f) {
        corner_a = high_x - margin;
        corner_b = low_x + margin;
    } else {
        corner_a = low_x - margin;
        corner_b = high_x + margin;
    }
    float corner_c;
    float corner_d;
    if (float_at(rt, self + 0x30u) < 0.0f) {
        corner_c = high_y + margin;
        corner_d = low_y - margin;
    } else {
        corner_c = low_y + margin;
        corner_d = high_y - margin;
    }
    put_float(rt, self + 0x4Cu, corner_a);
    put_float(rt, self + 0x50u, corner_d);
    put_float(rt, self + 0x54u, corner_b);
    put_float(rt, self + 0x58u, corner_c);
    put_float(rt, self + 0x5Cu, float_at(rt, self + 0x54u) - float_at(rt, self + 0x4Cu));
    put_float(rt, self + 0x60u, float_at(rt, self + 0x58u) - float_at(rt, self + 0x50u));
    ctx.gpr[3] = self + 0x4Cu;
    call_set(rt, ctx, 0x088AB798u, {self, self + 0x18u, 0u});
    rt.memory().store8(self + 0x43Bu, 0u);
    rt.memory().store8(self + 0x439u, 0u);
    rt.memory().store8(self + 0x43Au, 0u);
}

// 0x08B6759C (f12 = a value): 0x08B6B238's float is the result unless the mode word 0x08C96698 is -1 (then nothing else happens) or 0x08B6ECB0 accepts the value or it is above zero; otherwise a record
// at sp + 0 / 0x18 (the double of 0x08B70F28(value), a constant pair that is negated for a non-zero mode, and the code 0x21 / 0x22 sent through the object 0x08B77D40 returns when mode 2 or 0x08B6E524 refuses)
// is passed to 0x08B71EC8, whose float is the result.
void apply_constant_mode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float input = ctx.fpr[12];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x30u}, {53u, 0x44u}, {52u, 0x40u}, {31u, 0x34u}});
    ctx.fpr[21] = input;
    call_set(rt, ctx, 0x08B6B238u, {});
    const float base = ctx.fpr[0];
    ctx.fpr[20] = base;
    ctx.gpr[2] = 0x08C90000u;
    const u32 mode = word(rt, 0x08C96698u);
    ctx.gpr[16] = mode;
    ctx.gpr[3] = 0xFFFFFFFFu;
    if (mode == 0xFFFFFFFFu) {
        ctx.fpr[0] = base;
        return;
    }
    ctx.fpr[12] = input;
    const u32 refused = call_set(rt, ctx, 0x08B6ECB0u, {});
    ctx.fpr[0] = 0.0f;
    if (refused != 0u || !(input <= 0.0f)) {
        ctx.fpr[0] = base;
        return;
    }
    ctx.fpr[12] = input;
    put(rt, frame.at(4u), 0x08CB5E6Cu);
    put(rt, frame.at(0x20u), 0u);
    const u32 low = call_set(rt, ctx, 0x08B70F28u, {});
    const u32 high = ctx.gpr[3];
    put(rt, frame.at(8u), low);
    put(rt, frame.at(0xCu), high);
    put(rt, frame.at(0x10u), low);
    put(rt, frame.at(0x14u), high);
    if (mode == 0u) {
        put(rt, frame.at(0x18u), word(rt, 0x08CB6610u));
        put(rt, frame.at(0x1Cu), word(rt, 0x08CB6614u));
    } else {
        put(rt, frame.at(0x18u), word(rt, 0x08C96690u));
        put(rt, frame.at(0x1Cu), word(rt, 0x08C96694u) ^ 0x80000000u);
    }
    u32 code = 0u;
    if (input != 0.0f) {
        put(rt, frame.at(0u), 1u);
        if (mode == 2u) code = 0x21u;
    } else {
        put(rt, frame.at(0u), 2u);
        if (mode == 2u) code = 0x22u;
    }
    if (code == 0u && call_set(rt, ctx, 0x08B6E524u, {frame.at(0u)}) == 0u) code = 0x21u;
    if (code != 0u) put(rt, call_set(rt, ctx, 0x08B77D40u, {}), code);
    if (word(rt, frame.at(0x20u)) != 0u) {
        const u32 sink = call_set(rt, ctx, 0x08B77D40u, {});
        put(rt, sink, word(rt, frame.at(0x20u)));
    }
    call_set(rt, ctx, 0x08B71EC8u, {word(rt, frame.at(0x18u)), word(rt, frame.at(0x1Cu))});
    ctx.fpr[20] = ctx.fpr[0];
}

// 0x08926784 (a0 = the object, f12 = a new angle): unless the byte 0x08CF28AC is set (then f0 = the float at +4), the object's angle at +4 is moved on by the wrapped difference to the new one (the new one is kept at +8, the old
// +8 at +0x10), 0x08926564 is called with that difference and the angle then advances by (+0xC * the float 0x08CDDD68), wrapped into [-pi, pi]; f0 = the angle.
void turn_towards_heading(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float angle = ctx.fpr[12];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}});
    constexpr float kPi = std::bit_cast<float>(0x40490FDBu);
    constexpr float kTwoPi = std::bit_cast<float>(0x40C90FDBu);
    ctx.gpr[2] = 0x08CF0000u;
    ctx.gpr[16] = self;
    const u32 flag = rt.memory().load8(0x08CF28ACu);
    ctx.gpr[3] = flag;
    if (flag != 0u) {
        ctx.fpr[0] = float_at(rt, self + 4u);
        return;
    }
    const float previous = float_at(rt, self + 8u);
    float delta = angle - float_at(rt, self + 4u);
    put_float(rt, self + 8u, angle);
    put_float(rt, self + 0x10u, previous);
    bool wrapped = false;
    if (kPi < delta) {
        delta -= kTwoPi;
        wrapped = true;
    } else if (delta < -kPi) {
        delta += kTwoPi;
        wrapped = true;
    }
    if (wrapped) {
        if (kPi < delta) delta -= kTwoPi;
        else if (delta < -kPi) delta += kTwoPi;
    }
    ctx.fpr[12] = delta;
    call_set(rt, ctx, 0x08926564u, {self});
    ctx.gpr[2] = 0x08CE0000u;
    const float rate = float_at(rt, 0x08CDDD68u);
    float heading = float_at(rt, self + 4u) + psp_mul(float_at(rt, self + 0xCu), rate);
    put_float(rt, self + 4u, heading);
    if (kPi < heading) {
        heading -= kTwoPi;
        put_float(rt, self + 4u, heading);
    } else if (heading < -kPi) {
        heading += kTwoPi;
        put_float(rt, self + 4u, heading);
    }
    ctx.fpr[0] = heading;
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_088C6B34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); auto &F = ctx.fpr; G[21] = G[5]; G[5] = 0u; G[20] = G[6]; G[19] = G[7]; G[18] = G[4]; put_float(rt, frame.at(0x30u), F[20]); F[20] = F[12]; G[16] = word(rt, G[4] + 0x0u); call_here(rt, ctx, 0x088C50C4u); F[0] = float_at(rt, G[2] + 0x14u); ctx.set_fpu_condition(F[20] < F[0]); { const bool tk = !ctx.fpu_condition(); G[2] = G[16] + 0xFFFFFFFFu; if (tk) goto L_088C6BA4; } G[3] = 0x1u; put(rt, G[19] + 0x0u, G[2]); put(rt, G[20] + 0x0u, 0u); goto L_088C6C84; L_088C6BA4: ; G[17] = G[16] + 0xFFFFFFFFu; G[4] = G[18]; G[5] = G[17]; call_here(rt, ctx, 0x088C50C4u); F[0] = float_at(rt, G[2] + 0x14u); ctx.set_fpu_condition(F[0] <= F[20]); { const bool tk = !ctx.fpu_condition(); G[3] = 0x2u; if (tk) goto L_088C6C68; } put(rt, G[19] + 0x0u, G[17]); put(rt, G[20] + 0x0u, 0u); goto L_088C6C84; L_088C6BD4: ; G[3] = G[22] & 0x1u; L_088C6BD8: ; G[2] = 0u - G[22]; if (G[3] != 0u) G[2] = G[22]; G[17] = G[17] + G[2]; G[4] = G[18]; G[22] = G[22] + 0x1u; G[3] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[16]) ? 1u : 0u; G[2] = G[17] - G[16]; { const bool tk = static_cast<std::int32_t>(G[17]) < 0; G[21] = G[16] + G[17]; if (tk) goto L_088C6C04; } G[21] = G[17]; if (G[3] == 0u) G[21] = G[2]; L_088C6C04: ; { const bool tk = G[21] == 0u; G[23] = G[21] + 0xFFFFFFFFu; if (tk) goto L_088C6C54; } G[2] = G[23] + G[16]; G[3] = static_cast<std::int32_t>(G[23]) < 0 ? 1u : 0u; if (G[3] != 0u) G[23] = G[2]; G[5] = G[23]; call_here(rt, ctx, 0x088C50C4u); F[0] = float_at(rt, G[2] + 0x14u); G[4] = G[18]; ctx.set_fpu_condition(F[20] < F[0]); { const bool tk = ctx.fpu_condition(); G[5] = G[21]; if (tk) goto L_088C6C54; } call_here(rt, ctx, 0x088C50C4u); F[0] = float_at(rt, G[2] + 0x14u); ctx.set_fpu_condition(F[0] <= F[20]); if (!ctx.fpu_condition()) { put(rt, G[19] + 0x0u, G[23]); goto L_088C6C7C; } L_088C6C54: ; G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[22]) ? 1u : 0u; if (G[2] == 0u) { G[3] = G[22] & 0x1u; goto L_088C6BD8; } G[3] = 0u; goto L_088C6C84; L_088C6C68: ; G[17] = G[21]; { const bool tk = static_cast<std::int32_t>(G[16]) >= 0; G[22] = 0u; if (tk) goto L_088C6BD4; } G[3] = 0u; goto L_088C6C84; L_088C6C7C: ; G[3] = 0x3u; put(rt, G[20] + 0x0u, G[21]); L_088C6C84: ; F[20] = float_at(rt, frame.at(0x30u)); G[2] = G[3]; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088F360C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { sort_records_by_float(rt, ctx); }
void func_08AE9798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { pick_record_by_priority(rt, ctx); }
void func_08B43E44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { send_message_record(rt, ctx); }
void func_08B441C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { announce_list_node(rt, ctx); }
void func_08B46FB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { start_transfer(rt, ctx); }
void func_08B6759C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_constant_mode(rt, ctx); }
void func_08BE0238(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { release_shared_members(rt, ctx); }
void func_08872940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x4Cu), G[31]); auto &F = ctx.fpr; G[6] = 0u; G[17] = G[5]; G[18] = G[17] + 0x5Cu; G[16] = G[4]; put_float(rt, frame.at(0x50u), F[20]); G[4] = word(rt, G[5] + 0x8u); G[5] = 0x08CA0000u; G[5] = 0x08C9F8A0u; call_here(rt, ctx, 0x0886B920u); G[3] = word(rt, G[17] + 0x5Cu); G[4] = G[2]; G[3] = word(rt, G[3] + 0x10u); G[2] = G[3] < 0x2u ? 1u : 0u; { const bool tk = G[2] == 0u; G[2] = 0x2u; if (tk) goto L_08872998; } if (G[4] == 0u) { put(rt, G[16] + 0x0u, 0u); goto L_088729C4; } L_08872998: ; { const bool tk = G[3] == G[2]; G[2] = static_cast<std::int32_t>(G[3]) < 3 ? 1u : 0u; if (tk) goto L_08872A00; } { const bool tk = G[2] == 0u; G[2] = 0x3u; if (tk) goto L_088729B8; } if (static_cast<std::int32_t>(G[3]) >= 0) { G[3] = word(rt, G[4] + 0x0u); goto L_088729D0; } put(rt, G[16] + 0x0u, 0u); goto L_088729C4; L_088729B8: ; { const bool tk = G[3] == G[2]; G[2] = G[17] + 0x2Cu; if (tk) goto L_08872A20; } put(rt, G[16] + 0x0u, 0u); L_088729C4: ; put(rt, G[16] + 0x4u, 0u); put(rt, G[16] + 0x8u, 0u); goto L_08872A94; L_088729D0: ; G[6] = 0x08CA0000u; G[6] = 0x08C9F8A4u; G[2] = word(rt, G[3] + 0x28u); { const std::uint32_t tg = G[2]; G[5] = frame.at(0u); call_here(rt, ctx, tg); } F[1] = float_at(rt, frame.at(0x34u)); F[2] = float_at(rt, frame.at(0x38u)); F[0] = float_at(rt, frame.at(0x30u)); put_float(rt, G[16] + 0x4u, F[1]); put_float(rt, G[16] + 0x0u, F[0]); put_float(rt, G[16] + 0x8u, F[2]); goto L_08872A94; L_08872A00: ; G[2] = G[17] + 0x2Cu; F[0] = float_at(rt, G[2] + 0x8u); F[1] = float_at(rt, G[17] + 0x2Cu); F[2] = float_at(rt, G[2] + 0x4u); put_float(rt, G[16] + 0x8u, F[0]); put_float(rt, G[16] + 0x0u, F[1]); put_float(rt, G[16] + 0x4u, F[2]); goto L_08872A94; L_08872A20: ; F[0] = float_at(rt, G[2] + 0x8u); F[2] = float_at(rt, G[2] + 0x4u); F[1] = float_at(rt, G[17] + 0x2Cu); G[2] = G[17] + 0x20u; put_float(rt, G[16] + 0x8u, F[0]); G[5] = G[16]; G[6] = frame.at(0u); put_float(rt, G[16] + 0x0u, F[1]); G[4] = G[16]; put_float(rt, G[16] + 0x4u, F[2]); F[1] = float_at(rt, G[2] + 0x8u); F[2] = float_at(rt, G[2] + 0x4u); F[0] = float_at(rt, G[17] + 0x20u); put_float(rt, frame.at(0x8u), F[1]); put_float(rt, frame.at(0x4u), F[2]); put_float(rt, frame.at(0x0u), F[0]); call_here(rt, ctx, 0x08B4AA68u); G[4] = G[16]; G[2] = word(rt, G[17] + 0x5Cu); F[20] = float_at(rt, G[2] + 0x14u); call_here(rt, ctx, 0x08B4A720u); F[0] = std::sqrt(F[0]); G[4] = G[16]; F[12] = F[20] / F[0]; call_here(rt, ctx, 0x08B4A5E8u); G[4] = G[16]; G[5] = G[16]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B4AA34u); L_08872A94: ; G[6] = word(rt, G[18] + 0x0u); G[4] = G[16]; G[5] = G[16]; G[6] = G[6] + 0x18u; call_here(rt, ctx, 0x08B4AA34u); G[2] = G[16]; F[20] = float_at(rt, frame.at(0x50u)); goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08872F28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[30]); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); auto &F = ctx.fpr; G[22] = G[5]; G[17] = G[4] + 0x20u; G[16] = G[4]; G[23] = G[16] + 0x2Cu; put_float(rt, frame.at(0x30u), F[20]); G[30] = G[16] + 0x38u; F[12] = float_at(rt, G[4] + 0x20u); call_here(rt, ctx, 0x08B70F28u); F[12] = float_at(rt, G[17] + 0x4u); G[20] = G[2]; G[21] = G[3]; call_here(rt, ctx, 0x08B70F28u); F[12] = float_at(rt, G[17] + 0x8u); G[18] = G[2]; G[19] = G[3]; call_here(rt, ctx, 0x08B70F28u); G[5] = 0x08CA0000u; G[4] = G[22]; G[6] = G[20]; G[7] = G[21]; G[8] = G[18]; G[9] = G[19]; G[5] = 0x08C9F8F4u; G[10] = G[2]; G[11] = G[3]; F[20] = float_at(rt, G[16] + 0x10u); call_here(rt, ctx, 0x08B96B54u); F[12] = float_at(rt, G[16] + 0x2Cu); call_here(rt, ctx, 0x08B70F28u); F[12] = float_at(rt, G[23] + 0x4u); G[20] = G[2]; G[21] = G[3]; call_here(rt, ctx, 0x08B70F28u); F[12] = float_at(rt, G[23] + 0x8u); G[18] = G[2]; G[19] = G[3]; call_here(rt, ctx, 0x08B70F28u); G[5] = 0x08CA0000u; G[4] = G[22]; G[6] = G[20]; G[7] = G[21]; G[8] = G[18]; G[9] = G[19]; G[5] = 0x08C9F910u; G[10] = G[2]; G[11] = G[3]; call_here(rt, ctx, 0x08B96B54u); F[12] = float_at(rt, G[16] + 0x38u); call_here(rt, ctx, 0x08B70F28u); F[12] = float_at(rt, G[30] + 0x4u); G[18] = G[2]; G[19] = G[3]; call_here(rt, ctx, 0x08B70F28u); F[12] = float_at(rt, G[30] + 0x8u); G[16] = G[2]; G[17] = G[3]; call_here(rt, ctx, 0x08B70F28u); G[5] = 0x08CA0000u; G[4] = G[22]; G[6] = G[18]; G[7] = G[19]; G[5] = 0x08C9F92Cu; G[8] = G[16]; G[9] = G[17]; G[10] = G[2]; G[11] = G[3]; call_here(rt, ctx, 0x08B96B54u); F[12] = F[20]; call_here(rt, ctx, 0x08B70F28u); G[5] = 0x08CA0000u; G[4] = G[22]; F[20] = float_at(rt, frame.at(0x30u)); G[5] = 0x08C9F948u; G[6] = G[2]; G[7] = G[3]; tail = 0x08B96B54u; goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088AC8F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { update_scaled_box(rt, ctx); }
void func_088BE83C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); auto &F = ctx.fpr; G[17] = G[4]; put_float(rt, frame.at(0x18u), F[22]); F[22] = F[13]; put_float(rt, frame.at(0x14u), F[21]); F[21] = F[12]; put_float(rt, frame.at(0x10u), F[20]); G[4] = word(rt, G[4] + 0x0u); G[16] = word(rt, G[17] + 0x24u); call_here(rt, ctx, 0x088B1A0Cu); G[2] = G[2] + 0xFFFFFFFEu; G[2] = G[2] < 0x2u ? 1u : 0u; { const bool tk = G[2] != 0u; G[3] = G[17] + 0x4u; if (tk) goto L_088BE88C; } G[2] = word(rt, G[3] + 0x4u); F[20] = float_at(rt, G[2] + 0x6C4u); goto L_088BE8AC; L_088BE88C: ; G[2] = word(rt, G[3] + 0x4u); F[0] = float_at(rt, G[2] + 0x6D0u); F[12] = float_at(rt, G[2] + 0x6CCu); F[0] = psp_mul(F[0], F[0]); F[12] = psp_mul(F[12], F[12]); F[12] = F[12] + F[0]; call_here(rt, ctx, 0x08B49D7Cu); F[20] = F[0]; L_088BE8AC: ; G[2] = 0x08CE0000u; F[13] = float_at(rt, 0x08CE304Cu); F[15] = F[22]; F[12] = F[20]; G[1] = 0x3F400000u; F[14] = std::bit_cast<float>(G[1]); F[13] = F[21] - F[13]; call_here(rt, ctx, 0x088BB914u); G[2] = load8(G[16] + 0x12u); F[2] = F[0]; G[2] = G[2] + 0xFFFFFFFDu; G[2] = G[2] & 0xFFu; G[2] = G[2] < 0x2u ? 1u : 0u; { const bool tk = G[2] != 0u; put_float(rt, G[16] + 0x28u, F[0]); if (tk) goto L_088BE924; } G[3] = load8(G[16] + 0xA5u); G[2] = G[3] + 0xFFFFFFFEu; G[2] = G[2] < 0x2u ? 1u : 0u; { const bool tk = G[2] != 0u; F[1] = F[21]; if (tk) goto L_088BE928; } G[2] = 0x9u; { const bool tk = G[3] == G[2]; G[2] = 0x4u; if (tk) goto L_088BE928; } { const bool tk = G[3] == G[2]; F[0] = std::bit_cast<float>(0u); if (tk) goto L_088BE92C; } G[1] = 0x40B10000u; G[1] = G[1] | 0xC71Du; F[0] = std::bit_cast<float>(G[1]); F[1] = F[21] + F[0]; goto L_088BE928; L_088BE924: ; F[1] = F[21]; L_088BE928: ; F[0] = std::bit_cast<float>(0u); L_088BE92C: ; ctx.set_fpu_condition(F[2] == F[0]); if (!ctx.fpu_condition()) { F[1] = float_at(rt, G[16] + 0x28u); goto L_088BE978; } ctx.set_fpu_condition(F[1] < F[20]); if (!ctx.fpu_condition()) { F[1] = float_at(rt, G[16] + 0x28u); goto L_088BE978; } G[3] = word(rt, G[17] + 0x14u); G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); G[2] = load8(G[3] + 0x1u); if (G[2] == 0u) { put_float(rt, G[16] + 0x2Cu, F[0]); goto L_088BE974; } F[12] = F[21]; G[4] = G[17]; call_here(rt, ctx, 0x088BCE94u); put_float(rt, G[16] + 0x2Cu, F[0]); L_088BE974: ; F[1] = float_at(rt, G[16] + 0x28u); L_088BE978: ; G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); ctx.set_fpu_condition(F[1] == F[0]); { const bool tk = ctx.fpu_condition(); F[0] = std::bit_cast<float>(0u); if (tk) goto L_088BE9A4; } ctx.set_fpu_condition(F[1] == F[0]); { const bool tk = ctx.fpu_condition(); G[2] = 0xCu; if (tk) goto L_088BE9A4; } store16(G[16] + 0x38u, G[2]); L_088BE9A4: ; F[22] = float_at(rt, frame.at(0x18u)); F[21] = float_at(rt, frame.at(0x14u)); F[20] = float_at(rt, frame.at(0x10u)); goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088BE9C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x10u), G[20]); auto &F = ctx.fpr; G[21] = G[5]; G[19] = G[4]; G[16] = G[4] + 0x4u; put_float(rt, frame.at(0x24u), F[21]); put_float(rt, frame.at(0x20u), F[20]); G[4] = word(rt, G[16] + 0x8u); G[20] = word(rt, G[19] + 0x18u); G[4] = G[4] + 0x4u; call_here(rt, ctx, 0x088D5910u); G[4] = word(rt, G[16] + 0x8u); G[5] = word(rt, G[16] + 0xCu); G[17] = G[2]; call_here(rt, ctx, 0x088DF13Cu); G[3] = load8(G[20] + 0x98u); { const bool tk = G[3] == 0u; G[18] = word(rt, G[2] + 0x18u); if (tk) goto L_088BEB0C; } { const bool tk = static_cast<std::int32_t>(G[17]) <= 0; G[2] = 0u; if (tk) goto L_088BEB20; } { const bool tk = static_cast<std::int32_t>(G[18]) <= 0; if (tk) goto L_088BEB24; } G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[18]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = 0u; if (tk) goto L_088BEB24; } goto L_088BEB14; L_088BEA48: ; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x14u); { const std::uint32_t tg = G[2]; G[18] = G[18] + 0xFFFFFFFFu; call_here(rt, ctx, tg); } F[1] = std::bit_cast<float>(G[17]); F[20] = F[0]; G[4] = word(rt, G[16] + 0x8u); F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[1]))); G[5] = word(rt, G[16] + 0xCu); F[21] = psp_mul(F[20], F[0]); call_here(rt, ctx, 0x088DF13Cu); F[0] = std::bit_cast<float>(G[18]); F[12] = std::bit_cast<float>(0u); F[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); ctx.set_fpu_condition(F[12] < F[21]); F[0] = float_at(rt, G[2] + 0x0u); F[20] = psp_mul(F[20], F[1]); { const bool tk = !ctx.fpu_condition(); F[0] = F[20] + F[0]; if (tk) goto L_088BEA98; } F[12] = F[0] / F[21]; L_088BEA98: ; G[17] = word(rt, G[19] + 0x14u); G[4] = G[17] + 0x27Cu; call_here(rt, ctx, 0x089468C0u); G[5] = word(rt, G[16] + 0xCu); G[4] = word(rt, G[16] + 0x8u); F[20] = F[0]; call_here(rt, ctx, 0x088DA1C8u); G[3] = G[2] + 0x1u; G[2] = static_cast<std::int32_t>(G[3]) < 2 ? 1u : 0u; { const bool tk = G[2] != 0u; F[0] = std::bit_cast<float>(G[3]); if (tk) goto L_088BEAD4; } F[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[0] = float_at(rt, G[17] + 0x288u); F[0] = F[0] / F[1]; F[20] = psp_mul(F[20], F[0]); L_088BEAD4: ; G[2] = load8(G[20] + 0x99u); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[17] + 0x2B0u; if (tk) goto L_088BEB00; } G[2] = G[2] + 0xFFFFFFFFu; F[0] = std::bit_cast<float>(G[2]); G[1] = 0x42C80000u; F[12] = std::bit_cast<float>(G[1]); F[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[12] = F[1] / F[12]; call_here(rt, ctx, 0x089468C0u); F[20] = psp_mul(F[20], F[0]); L_088BEB00: ; G[2] = 0x1u; put_float(rt, G[21] + 0x0u, F[20]); goto L_088BEB20; L_088BEB0C: ; G[2] = 0u; goto L_088BEB20; L_088BEB14: ; if (G[21] != 0u) { G[4] = word(rt, G[19] + 0x1Cu); goto L_088BEA48; } G[2] = 0x1u; L_088BEB20: ; L_088BEB24: ; F[21] = float_at(rt, frame.at(0x24u)); F[20] = float_at(rt, frame.at(0x20u)); goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[20] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088C66E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x38u), G[31]); auto &F = ctx.fpr; G[1] = 0x43FA0000u; F[0] = std::bit_cast<float>(G[1]); G[17] = G[4]; G[16] = G[5]; put_float(rt, frame.at(0x48u), F[22]); F[22] = F[14]; put_float(rt, frame.at(0x44u), F[21]); F[21] = F[13]; put_float(rt, frame.at(0x40u), F[20]); F[20] = F[12]; F[1] = float_at(rt, G[5] + 0x18u); ctx.set_fpu_condition(F[0] < F[1]); { const bool tk = ctx.fpu_condition(); G[5] = G[6]; if (tk) goto L_088C6848; } G[1] = 0xC3FA0000u; F[0] = std::bit_cast<float>(G[1]); ctx.set_fpu_condition(F[1] < F[0]); { const bool tk = ctx.fpu_condition(); F[0] = F[21]; if (tk) goto L_088C684C; } F[12] = F[13]; G[4] = G[16]; G[6] = frame.at(0x4u); G[7] = 0u; call_here(rt, ctx, 0x088C64ACu); G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0x20u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } { const bool tk = G[2] == 0u; F[1] = float_at(rt, frame.at(0x8u)); if (tk) goto L_088C6848; } F[0] = float_at(rt, frame.at(0x4u)); G[8] = word(rt, G[16] + 0x38u); F[1] = -F[1]; G[9] = word(rt, G[16] + 0x3Cu); put_float(rt, frame.at(0x18u), F[0]); G[4] = G[2]; F[12] = F[20]; F[0] = float_at(rt, frame.at(0xCu)); put_float(rt, frame.at(0x20u), F[1]); G[5] = frame.at(0x18u); put(rt, frame.at(0x10u), G[8]); put(rt, frame.at(0x14u), G[9]); put_float(rt, frame.at(0x1Cu), F[0]); G[2] = word(rt, G[2] + 0x0u); G[3] = word(rt, G[2] + 0xCu); { const std::uint32_t tg = G[3]; G[6] = frame.at(0u); call_here(rt, ctx, tg); } G[5] = word(rt, G[17] + 0x0u); G[4] = G[17]; G[2] = word(rt, G[5] + 0x14u); { const std::uint32_t tg = G[2]; F[20] = F[20] - F[0]; call_here(rt, ctx, tg); } F[1] = F[0]; G[1] = 0x3F000000u; F[0] = std::bit_cast<float>(G[1]); F[0] = psp_mul(F[1], F[0]); ctx.set_fpu_condition(F[0] < F[20]); { const bool tk = !ctx.fpu_condition(); if (tk) goto L_088C67F4; } F[20] = F[20] - F[1]; goto L_088C6810; L_088C67F4: ; G[1] = 0xBF000000u; F[0] = std::bit_cast<float>(G[1]); F[0] = psp_mul(F[1], F[0]); ctx.set_fpu_condition(F[20] < F[0]); if (ctx.fpu_condition()) { F[20] = F[20] + F[1]; goto L_088C6810; } L_088C6810: ; F[0] = F[20] / F[22]; G[1] = 0x3F800000u; F[1] = std::bit_cast<float>(G[1]); F[21] = F[21] + F[0]; ctx.set_fpu_condition(F[1] < F[21]); { const bool tk = !ctx.fpu_condition(); F[0] = std::bit_cast<float>(0u); if (tk) goto L_088C6838; } F[21] = F[1]; goto L_088C6848; L_088C6838: ; ctx.set_fpu_condition(F[21] < F[0]); if (ctx.fpu_condition()) { F[21] = F[0]; goto L_088C6848; } L_088C6848: ; F[0] = F[21]; L_088C684C: ; F[22] = float_at(rt, frame.at(0x48u)); F[21] = float_at(rt, frame.at(0x44u)); F[20] = float_at(rt, frame.at(0x40u)); goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088FB3D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reduce_overlapping_flags(rt, ctx); }
void func_088FDDF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rebase_big_endian_object(rt, ctx); }
void func_08915E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { init_float_pair(rt, ctx); }
void func_08926784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { turn_towards_heading(rt, ctx); }
void func_089AFC78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x44u), G[21]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x48u), G[31]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); G[21] = 0x08CF0000u; G[20] = G[4]; G[4] = 0x08CF4A04u; G[18] = G[5]; G[17] = frame.at(0x10u); call_here(rt, ctx, 0x08B344E0u); G[4] = G[18]; call_here(rt, ctx, 0x08A5BA28u); G[2] = word(rt, G[18] + 0x138u); G[6] = G[20] + 0x18u; G[4] = G[20]; G[2] = G[2] | 0x200u; G[5] = G[20] + 0x30u; put(rt, G[18] + 0x138u, G[2]); call_here(rt, ctx, 0x089AFA10u); G[2] = 0x08CD0000u; G[16] = frame.at(0x14u); G[2] = 0x08CCFD88u; put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x24u), 0u); put(rt, frame.at(0x14u), 0u); put(rt, frame.at(0x18u), 0u); put(rt, frame.at(0x1Cu), G[16]); put(rt, frame.at(0x20u), G[16]); call_here(rt, ctx, 0x089B9E60u); G[5] = frame.at(0u); G[4] = G[17]; put(rt, frame.at(0x0u), G[2]); call_here(rt, ctx, 0x08BF9104u); G[5] = G[2]; { const bool tk = G[16] == G[2]; G[19] = G[2]; if (tk) goto L_089AFD34; } G[3] = word(rt, frame.at(0x0u)); G[2] = word(rt, G[2] + 0x10u); G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] == 0u; if (tk) goto L_089AFD50; } G[4] = G[17]; goto L_089AFD3C; L_089AFD34: ; G[3] = word(rt, frame.at(0x0u)); G[4] = G[17]; L_089AFD3C: ; G[6] = frame.at(0x4u); put(rt, frame.at(0x4u), G[3]); put(rt, frame.at(0x8u), 0u); call_here(rt, ctx, 0x08BF99BCu); G[19] = G[2]; L_089AFD50: ; G[4] = 0xCu; call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CD0000u; G[3] = 0x08CCEA88u; put(rt, G[2] + 0x0u, G[3]); G[3] = 0x08C00000u; G[17] = frame.at(0xCu); G[3] = 0x08BF8A7Cu; G[16] = 0x08CD0000u; put(rt, G[2] + 0x4u, G[3]); G[16] = 0x08CCEA78u; G[4] = G[20]; put(rt, G[2] + 0x8u, 0u); G[5] = G[17]; put(rt, G[19] + 0x14u, G[2]); put(rt, frame.at(0xCu), G[16]); put(rt, frame.at(0x28u), G[18]); call_here(rt, ctx, 0x089A931Cu); G[4] = G[18]; G[3] = word(rt, G[18] + 0x0u); G[2] = word(rt, G[3] + 0x164u); { const std::uint32_t tg = G[2]; G[5] = G[20]; call_here(rt, ctx, tg); } G[5] = G[17]; G[4] = G[20]; call_here(rt, ctx, 0x089A9314u); G[4] = G[18]; call_here(rt, ctx, 0x08A5B970u); call_here(rt, ctx, 0x08A5BC6Cu); G[4] = G[17]; put(rt, frame.at(0xCu), G[16]); call_here(rt, ctx, 0x089FD82Cu); G[4] = 0x08CF4A04u; call_here(rt, ctx, 0x08B3457Cu); goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x44u)); G[20] = word(rt, frame.at(0x40u)); G[18] = word(rt, frame.at(0x38u)); G[19] = word(rt, frame.at(0x3Cu)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08A50758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { replace_entry_by_key(rt, ctx); }
void func_08AAAB6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); G[17] = G[5]; G[22] = word(rt, G[4] + 0x4u); { const bool tk = G[22] != 0u; G[16] = G[4]; if (tk) goto L_08AAABAC; } G[4] = 0x08CB0000u; G[4] = 0x08CB30B0u; call_here(rt, ctx, 0x08A5BB7Cu); L_08AAABAC: ; G[20] = frame.at(0x4u); G[5] = G[17]; G[6] = frame.at(0x2u); G[4] = G[20]; G[21] = G[16] + 0x8u; call_here(rt, ctx, 0x08B85EB8u); G[4] = G[21]; G[5] = G[20]; call_here(rt, ctx, 0x08C31D6Cu); G[19] = G[2]; G[2] = G[16] + 0xCu; { const bool tk = G[2] == G[19]; G[4] = word(rt, frame.at(0x4u)); if (tk) goto L_08AAAC2C; } G[5] = frame.at(0xCu); G[18] = G[19]; G[16] = word(rt, G[4] + 0xFFFFFFF4u); put(rt, frame.at(0x8u), G[16]); G[2] = word(rt, G[19] + 0x10u); G[17] = word(rt, G[2] + 0xFFFFFFF4u); G[2] = frame.at(0x8u); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[5] = G[2]; put(rt, frame.at(0xCu), G[17]); G[16] = G[16] - G[17]; G[6] = word(rt, G[5] + 0x0u); G[5] = word(rt, G[19] + 0x10u); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; { const bool tk = static_cast<std::int32_t>(G[16]) < 0; G[2] = 0x08D70000u; if (tk) goto L_08AAAC2C; } G[19] = 0x08D74FD0u; goto L_08AAAC8C; L_08AAAC2C: ; G[16] = frame.at(0x10u); G[4] = G[16]; G[5] = G[20]; call_here(rt, ctx, 0x08B88F44u); G[4] = G[21]; G[5] = G[19]; G[6] = G[16]; put(rt, frame.at(0x14u), 0u); call_here(rt, ctx, 0x08C335FCu); G[16] = G[2]; G[4] = word(rt, frame.at(0x10u)); G[2] = 0x08D70000u; G[19] = 0x08D74FD0u; G[17] = G[4] + 0xFFFFFFF4u; { const bool tk = G[17] == G[19]; G[18] = G[16]; if (tk) goto L_08AAAC8C; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = word(rt, G[18] + 0x14u); goto L_08AAAC90; } G[4] = G[17]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); L_08AAAC8C: ; G[2] = word(rt, G[18] + 0x14u); L_08AAAC90: ; G[4] = G[22]; G[5] = word(rt, G[2] + 0x4u); call_here(rt, ctx, 0x08A71B00u); G[4] = word(rt, frame.at(0x4u)); G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[19]; if (tk) goto L_08AAACD0; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; if (tk) goto L_08AAACD0; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); L_08AAACD0: ; goto exit_;  exit_: ; G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B72564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { soft_float_to_float(rt, ctx); }
void func_089795AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x34u), G[17]); auto &F = ctx.fpr; G[20] = frame.at(0x14u); G[19] = G[4]; G[4] = G[20]; call_here(rt, ctx, 0x08978000u); G[16] = word(rt, frame.at(0x14u)); if (G[16] != 0u) { G[4] = G[16]; goto L_089795F4; } G[4] = 0x08CA0000u; G[4] = 0x08CA2F94u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_089795F4: ; G[5] = frame.at(0x18u); G[6] = frame.at(0x1Cu); G[7] = frame.at(0x20u); G[8] = frame.at(0x24u); G[18] = frame.at(0x28u); call_here(rt, ctx, 0x089772F4u); G[4] = G[18]; G[5] = 0x4u; call_here(rt, ctx, 0x08A98C1Cu); G[2] = word(rt, frame.at(0x28u)); F[12] = float_at(rt, frame.at(0x18u)); G[16] = frame.at(0x10u); G[17] = word(rt, G[2] + 0x14u); G[4] = G[16]; call_here(rt, ctx, 0x08A87EECu); { const bool tk = G[17] == G[16]; G[5] = word(rt, frame.at(0x10u)); if (tk) goto L_08979640; } G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); L_08979640: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x28u)); F[12] = float_at(rt, frame.at(0x1Cu)); G[17] = frame.at(0xCu); G[3] = word(rt, G[2] + 0x14u); G[4] = G[17]; G[16] = G[3] + 0x4u; call_here(rt, ctx, 0x08A87EECu); { const bool tk = G[16] == G[17]; G[5] = word(rt, frame.at(0xCu)); if (tk) goto L_08979674; } G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); L_08979674: ; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x28u)); F[12] = float_at(rt, frame.at(0x20u)); G[17] = frame.at(0x8u); G[3] = word(rt, G[2] + 0x14u); G[4] = G[17]; G[16] = G[3] + 0x8u; call_here(rt, ctx, 0x08A87EECu); { const bool tk = G[16] == G[17]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_089796A8; } G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); L_089796A8: ; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[2] = word(rt, frame.at(0x28u)); F[12] = float_at(rt, frame.at(0x24u)); G[17] = frame.at(0x4u); G[3] = word(rt, G[2] + 0x14u); G[4] = G[17]; G[16] = G[3] + 0xCu; call_here(rt, ctx, 0x08A87EECu); { const bool tk = G[16] == G[17]; G[5] = word(rt, frame.at(0x4u)); if (tk) goto L_089796DC; } G[4] = G[16]; call_here(rt, ctx, 0x08A5BAD8u); L_089796DC: ; G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); G[5] = G[18]; call_here(rt, ctx, 0x08A983E8u); { const bool tk = G[19] == frame.at(0u); G[5] = word(rt, frame.at(0x0u)); if (tk) goto L_08979700; } G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); L_08979700: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[16] = word(rt, frame.at(0x30u)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B764AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { find_node_covering(rt, ctx); }
void func_08B8B4A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x20u), G[16]); G[22] = G[6] + 0x8u; G[21] = G[6]; G[18] = G[5]; G[17] = G[4]; goto L_08B8B4E0; L_08B8B4DC: ; G[17] = G[17] + 0x18u; L_08B8B4E0: ; G[20] = G[17] + 0x8u; G[4] = word(rt, G[17] + 0x8u); G[5] = word(rt, G[20] + 0x4u); G[6] = word(rt, G[21] + 0x8u); G[7] = word(rt, G[22] + 0x4u); call_here(rt, ctx, 0x08B8A72Cu); if (G[2] != 0u) { G[17] = G[17] + 0x18u; goto L_08B8B4E0; } G[18] = G[18] + 0xFFFFFFE8u; L_08B8B504: ; G[19] = G[18] + 0x8u; G[6] = word(rt, G[18] + 0x8u); G[4] = word(rt, G[21] + 0x8u); G[5] = word(rt, G[22] + 0x4u); G[7] = word(rt, G[19] + 0x4u); call_here(rt, ctx, 0x08B8A72Cu); if (G[2] != 0u) { G[18] = G[18] + 0xFFFFFFE8u; goto L_08B8B504; } G[2] = G[17] < G[18] ? 1u : 0u; { const bool tk = G[2] == 0u; G[5] = 0x4u; if (tk) goto L_08B8B604; } G[2] = word(rt, G[17] + 0x0u); G[3] = word(rt, G[17] + 0x4u); put(rt, frame.at(0x8u), G[2]); put(rt, frame.at(0xCu), G[3]); G[16] = word(rt, G[20] + 0x4u); G[2] = word(rt, G[17] + 0x8u); put(rt, frame.at(0x18u), 0u); G[16] = G[16] - G[2]; G[16] = (G[16] & ~0x1u) | ((0u << 0) & 0x1u); G[4] = G[16]; put(rt, frame.at(0x14u), 0u); put(rt, frame.at(0x10u), 0u); call_here(rt, ctx, 0x08A5BC20u); G[16] = G[2] + G[16]; put(rt, frame.at(0x10u), G[2]); G[6] = G[2]; G[7] = frame.at(0x1u); put(rt, frame.at(0x14u), G[2]); G[4] = word(rt, G[17] + 0x8u); G[5] = word(rt, G[20] + 0x4u); put(rt, frame.at(0x18u), G[16]); call_here(rt, ctx, 0x08B8A654u); G[4] = word(rt, G[18] + 0x0u); G[5] = word(rt, G[18] + 0x4u); put(rt, frame.at(0x14u), G[2]); put(rt, G[17] + 0x0u, G[4]); { const bool tk = G[20] == G[19]; put(rt, G[17] + 0x4u, G[5]); if (tk) goto L_08B8B5AC; } G[4] = G[20]; G[5] = G[19]; call_here(rt, ctx, 0x08B8AD88u); L_08B8B5AC: ; G[2] = word(rt, frame.at(0x8u)); G[3] = word(rt, frame.at(0xCu)); G[5] = frame.at(0x10u); put(rt, G[18] + 0x0u, G[2]); { const bool tk = G[19] == G[5]; put(rt, G[18] + 0x4u, G[3]); if (tk) goto L_08B8B5CC; } G[4] = G[19]; call_here(rt, ctx, 0x08B8AD88u); L_08B8B5CC: ; G[4] = word(rt, frame.at(0x10u)); G[5] = word(rt, frame.at(0x14u)); G[6] = frame.at(0u); call_here(rt, ctx, 0x08B8A644u); G[4] = word(rt, frame.at(0x10u)); G[2] = word(rt, frame.at(0x18u)); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 1); if (tk) goto L_08B8B4DC; } G[5] = G[2] << 1; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); G[17] = G[17] + 0x18u; goto L_08B8B4E0; L_08B8B604: ; G[2] = G[17]; goto exit_;  exit_: ; G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_327
} // namespace gt::game

#define HOT327(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_327::name, gt::ReturnKind::kind)
HOT327(0x088C6B34u, func_088C6B34, Word);
HOT327(0x088F360Cu, func_088F360C, Word);
HOT327(0x08AE9798u, func_08AE9798, Word);
HOT327(0x08B43E44u, func_08B43E44, Word);
HOT327(0x08B441C4u, func_08B441C4, Word);
HOT327(0x08B46FB0u, func_08B46FB0, Word);
HOT327(0x08B6759Cu, func_08B6759C, Word);
HOT327(0x08BE0238u, func_08BE0238, Word);
// HOT327(0x08872940u, func_08872940, Word);  // no comparable synthetic case yet
HOT327(0x08872F28u, func_08872F28, Word);
HOT327(0x088AC8F0u, func_088AC8F0, Word);
HOT327(0x088BE83Cu, func_088BE83C, Word);
// HOT327(0x088BE9C4u, func_088BE9C4, Word);  // no comparable synthetic case yet
HOT327(0x088C66E4u, func_088C66E4, Word);
HOT327(0x088FB3D8u, func_088FB3D8, Word);
HOT327(0x088FDDF8u, func_088FDDF8, Word);
HOT327(0x08915E94u, func_08915E94, Word);
HOT327(0x08926784u, func_08926784, Word);
// HOT327(0x089AFC78u, func_089AFC78, Word);  // no comparable synthetic case yet
HOT327(0x08A50758u, func_08A50758, Word);
// HOT327(0x08AAAB6Cu, func_08AAAB6C, Word);  // no comparable synthetic case yet
HOT327(0x08B72564u, func_08B72564, Word);
// HOT327(0x089795ACu, func_089795AC, Word);  // no comparable synthetic case yet
HOT327(0x08B764ACu, func_08B764AC, Word);
HOT327(0x08B8B4A4u, func_08B8B4A4, Word);
