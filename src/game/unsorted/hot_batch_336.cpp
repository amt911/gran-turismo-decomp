// Hot batch 336: functions no route reaches (loops, several exits, tail calls, calls through registers; integer code only),
// drafted from `tools.disasm` with a register-level emitter (tools/batch_emit.py: one statement per instruction, labels and
// gotos for branches), read against the asm and verified by synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_336 {
namespace {

using namespace gt::game::hot;

std::uint32_t sext(std::uint32_t value, unsigned bits) {
    const unsigned shift = 32u - bits;
    return static_cast<std::uint32_t>(static_cast<std::int32_t>(value << shift) >> shift);
}
// The `lui` half of an address whose low half is added as a signed 16-bit immediate.
constexpr std::uint32_t high_half(std::uint32_t address) { return (address + 0x8000u) & 0xFFFF0000u; }
// `ext rt, rs, position, size`: the `size` bits of rs from `position`.
constexpr std::uint32_t ext(std::uint32_t value, unsigned position, unsigned size) {
    return (value >> position) & (size >= 32u ? 0xFFFFFFFFu : (1u << size) - 1u);
}
// `ins rt, rs, position, size`: rt's `size` bits from `position` replaced by rs's low bits.
constexpr std::uint32_t ins(std::uint32_t value, std::uint32_t source, unsigned position, unsigned size) {
    const std::uint32_t mask = (size >= 32u ? 0xFFFFFFFFu : (1u << size) - 1u) << position;
    return (value & ~mask) | ((source << position) & mask);
}
std::uint32_t sra(std::uint32_t value, unsigned bits) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value) >> bits); }
void jump(psprecomp::AllegrexContext &ctx, std::uint32_t target) { ctx.pc = target; }
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
// 0x08C74DD8 (a0 = the string object, a1 = the source bytes, a2 = the length): assign for a copy-on-write string whose buffer pointer is the word at +0
// (length at buffer - 0xC, use count at buffer - 4). A length above 0x3FFFFFFC calls 0x08B5ADDC(0x08CB58A0). When the source lies outside the buffer, or the buffer is shared
// (use count above 0), the tail call 0x08C74D38(object, 0, old length, source, length) is due (`tail`, made after the frame is gone). Otherwise the bytes are
// moved inside the buffer: nothing when the source starts the buffer, a single byte by hand, else 0x08B79A88 (source at or past the end of the length) or
// 0x08B79BAC (overlapping) with (buffer, source, length); then use count 0, length stored, and the terminator byte (0x08CDBE28) after the new end. v0 = the object.
bool assign_bytes(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 &old_length) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    const u32 length = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0x8u}});
    u32 buffer = word(rt, self);
    old_length = word(rt, buffer - 0xCu);
    if (length > 0x3FFFFFFCu) {
        call_set(rt, ctx, 0x08B5ADDCu, {0x08CB58A0u});
        return false;
    }
    if (source < buffer || buffer + old_length < source || static_cast<s32>(word(rt, buffer - 0x4u)) > 0) return true;
    const u32 offset = source - buffer;
    if (offset >= length || offset != 0u) {
        if (length == 1u) {
            rt.memory().store8(buffer, rt.memory().load8(source));
        } else {
            call_set(rt, ctx, offset < length ? 0x08B79BACu : 0x08B79A88u, {buffer, source, length});
        }
        buffer = word(rt, self);
    }
    put(rt, buffer - 0x4u, 0u);
    put(rt, buffer - 0xCu, length);
    rt.memory().store8(buffer + length, rt.memory().load8(0x08CDBE28u));
    ctx.gpr[2] = self;
    return false;
}
// 0x088ED560 (a0 = the object): clears and sets the object's state fields: 0x08B36018(object + 4, 0, 0xA0) first, then 0x088EF408(word +0xAC), a long run of
// field resets (words, bytes and halfwords from +0x124 up to +0x554, the two 8-byte entries at +0x128 and +0x178, the -1 markers at +0x510, +0x548, +0x54C and
// the bytes +0x52D..+0x52F), 0x088ED494(object + 0x4C0), the four-slot loop (the 8-byte pairs at +0x468 / +0x46C / +0x488 / +0x48C by 8 bytes, the byte arrays
// +0x210, +0x460, +0x4A8, +0x4AC, +0x4B0, +0x4B4, +0x4B8 by one, the words from +0x28 by 0x28) and 0x088EEEB0(word +0x118), whose result goes with the object to the tail call
// 0x088ECB0C(object, result) made after the frame is gone. Returns that result.
u32 reset_state(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 self) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    call_set(rt, ctx, 0x08B36018u, {self + 0x4u, 0u, 0xA0u});
    call_set(rt, ctx, 0x088EF408u, {word(rt, self + 0xACu)});
    put(rt, self + 0x548u, 0xFFFFFFFFu);
    put(rt, self + 0x54Cu, 0xFFFFFFFFu);
    put(rt, self + 0x530u, 0u);
    rt.memory().store8(self + 0x525u, 1u);
    put(rt, self + 0x53Cu, 0u);
    put(rt, self + 0x540u, 0u);
    put(rt, self + 0x4ECu, 0u);
    put(rt, self + 0x4F0u, 0u);
    rt.memory().store8(self + 0x524u, 1u);
    put(rt, self + 0x208u, 0u);
    put(rt, self + 0x20Cu, 0u);
    put(rt, self + 0x51Cu, 0u);
    for (const u32 offset : {0x520u, 0x522u, 0x523u, 0x554u, 0x526u, 0x527u}) rt.memory().store8(self + offset, 0u);
    put(rt, self + 0x200u, 0u);
    put(rt, self + 0x518u, 0u);
    for (const u32 offset : {0x528u, 0x529u, 0x52Au}) rt.memory().store8(self + offset, 0u);
    rt.memory().store16(self + 0x128u, 0xFFFFu);
    const u32 first_entry = self + 0x128u;
    put(rt, first_entry + 0x8u, 0u);
    put(rt, first_entry + 0xCu, 0u);
    rt.memory().store16(first_entry + 0x2u, 0u);
    put(rt, first_entry + 0x4u, 0u);
    put(rt, self + 0x500u, 0u);
    put(rt, self + 0x504u, 0u);
    rt.memory().store8(self + 0x52Bu, 0u);
    rt.memory().store8(self + 0x52Cu, 0u);
    put(rt, self + 0x124u, 0u);
    put(rt, self + 0x550u, 0u);
    const u32 second_entry = self + 0x178u;
    put(rt, second_entry + 0x8u, 0u);
    put(rt, second_entry + 0xCu, 0u);
    rt.memory().store16(second_entry + 0x2u, 0u);
    put(rt, second_entry + 0x4u, 0u);
    put(rt, self + 0x510u, 0xFFFFFFFFu);
    rt.memory().store8(self + 0x52Du, 0xFFu);
    rt.memory().store8(self + 0x52Eu, 0xFFu);
    rt.memory().store16(self + 0x178u, 0xFFFFu);
    put(rt, self + 0x4E0u, 0u);
    put(rt, self + 0x4E4u, 0u);
    rt.memory().store8(self + 0x514u, 0u);
    rt.memory().store8(self + 0x52Fu, 0xFFu);
    put(rt, self + 0x4E8u, 0u);
    call_set(rt, ctx, 0x088ED494u, {self + 0x4C0u});
    for (u32 slot = 0; slot < 4u; ++slot) {
        const u32 pair = self + 8u * slot;
        put(rt, pair + 0x468u, 0u);
        put(rt, pair + 0x46Cu, 0u);
        put(rt, pair + 0x488u, 0u);
        put(rt, pair + 0x48Cu, 0u);
        const u32 flags = self + slot;
        rt.memory().store8(flags + 0x210u, 0u);
        rt.memory().store8(flags + 0x460u, 0u);
        rt.memory().store8(flags + 0x4B0u, 0u);
        rt.memory().store8(flags + 0x4B4u, 0xFFu);
        rt.memory().store8(flags + 0x4ACu, 0u);
        rt.memory().store8(flags + 0x4B8u, 0u);
        put(rt, self + 0x28u + 0x28u * slot, 0u);
        rt.memory().store8(flags + 0x4A8u, 0xFFu);
    }
    return call_set(rt, ctx, 0x088EEEB0u, {word(rt, self + 0x118u)});
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

// 0x0881A208 (a0 = the destination string, a2 = a flag): with the flag set the destination takes 0x08A76AAC(frame +8, 0x08CF6ED8) (then 0x08A5BAD8(destination, text) when it is
// not that slot, and the slot is released, 0x08A5B9EC). Without it a reader (frame +0x30, 0x08817FCC; none: 0x08A5BB7C(0x08CA8D6C)) gives an object (0x08817A54)
// that 0x0881799C(object, &x, &y, &z) fills (three words on the frame, zeroed first), and the target (frame +0x40, 0x08A813F4; none: 0x08A5BB7C(0x08C98E8C))
// has three of its virtual +0xA0 calls made, each with a text built by 0x08B85EB8(text, format, &pad) and 0x08A6D2D4, and a value turned to text by
// 0x08A85B1C(text, one of the three words): formats 0x08C98E94, 0x08C98E9C, 0x08C98EA4. Each text is released (0x08A5B9EC; a counted string whose header is not the
// empty one 0x08D74FD0 drops its count 0x08B60804(count, -1) and, when none is left, is freed by 0x08B8542C(header, &pad)). The reader's temporary is released too.
void func_0881A208(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], flag = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x60u}, {31u, 0x6Cu}, {22u, 0x68u}, {21u, 0x64u}, {19u, 0x5Cu}, {18u, 0x58u}, {17u, 0x54u}, {16u, 0x50u}});
    ctx.gpr[20] = destination;
    if (flag != 0u) {
        const u32 slot = frame.at(0x8u);
        ctx.gpr[16] = slot;
        call_set(rt, ctx, 0x08A76AACu, {slot, 0x08CF6ED8u});
        if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
        return;
    }
    const u32 reader = frame.at(0x30u), target = frame.at(0x40u);
    ctx.gpr[17] = reader;
    call_set(rt, ctx, 0x08817FCCu, {reader});
    u32 held = word(rt, reader);
    ctx.gpr[16] = held;
    if (held == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98D6Cu});
    held = call_set(rt, ctx, 0x08817A54u, {held});
    ctx.gpr[16] = held;
    call_set(rt, ctx, 0x08A5B9ECu, {reader});
    ctx.gpr[22] = target;
    put(rt, frame.at(0x34u), 0u);
    put(rt, frame.at(0x38u), 0u);
    put(rt, frame.at(0x3Cu), 0u);
    call_set(rt, ctx, 0x0881799Cu, {held, frame.at(0x34u), frame.at(0x38u), frame.at(0x3Cu)});
    call_set(rt, ctx, 0x08A813F4u, {target});
    const u32 empty = 0x08D74FD0u;
    // the target object: reloaded, null calls the assert; v0 = its vtable
    auto target_object = [&] {
        const u32 object = word(rt, target);
        ctx.gpr[19] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98E8Cu});
        ctx.gpr[2] = word(rt, object);
        return object;
    };
    // releases the counted string at `text`
    auto release = [&](u32 text, u32 pad) {
        const u32 data = word(rt, text);
        const u32 header = data - 0xCu;
        ctx.gpr[16] = header;
        ctx.gpr[4] = data;
        if (header == empty) return;
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, pad});
    };
    struct Part {
        u32 text, temp, pad, format, word_at, scratch, free_pad;
    };
    const Part parts[3] = {
        {0x28u, 0x2Cu, 5u, 0x08C98E94u, 0x34u, 0x24u, 2u},
        {0x1Cu, 0x20u, 4u, 0x08C98E9Cu, 0x38u, 0x18u, 1u},
        {0x10u, 0x14u, 3u, 0x08C98EA4u, 0x3Cu, 0x0Cu, 0u},
    };
    ctx.gpr[21] = empty;
    for (unsigned i = 0; i < 3u; ++i) {
        const Part &part = parts[i];
        const u32 object = target_object();
        const u32 text = frame.at(part.text), temp = frame.at(part.temp), scratch = frame.at(part.scratch);
        ctx.gpr[16] = text;
        ctx.gpr[17] = temp;
        const u32 virtual_call = word(rt, word(rt, object) + 0xA0u);
        ctx.gpr[18] = virtual_call;
        call_set(rt, ctx, 0x08B85EB8u, {text, part.format, frame.at(part.pad)});
        call_set(rt, ctx, 0x08A6D2D4u, {temp, text});
        ctx.gpr[16] = scratch;
        call_set(rt, ctx, 0x08A85B1Cu, {scratch, word(rt, frame.at(part.word_at))});
        call_set(rt, ctx, virtual_call, {object, temp, scratch});
        call_set(rt, ctx, 0x08A5B9ECu, {scratch});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
        release(text, frame.at(part.free_pad));
    }
    if (destination != target) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, target)});
    call_set(rt, ctx, 0x08A5B9ECu, {target});
}
// 0x08840A0C (a0 = the destination string, a1 = a count, a2 = a value): with a count above 0, 0x0883B950(value) gives two words that 0x089312DC turns into a number n; the text
// `0x08C9C478 % n` (0x08B3767C, 0x40 bytes at frame +0x88) is formatted again with 0x08C9C4EC (0x08B37640, frame +0xC8). Two 0x3C-byte records (frame +0x108 and +0x148,
// each made by 0x08B0CE58) are filled: the first by 0x08B0EC60(record, text), the second, when the first has no error word (+0), by 0x08B10204(record, text, 1);
// both pass through a frame copy. When neither reports an error (+0, and +0x148 for the second), the number is 0x08B35254(record +0x20 after the two calls 0x088FDDF8 and 0x088FDA54,
// the second record's word at +0x20); otherwise it is 0. 0x08A85B1C(frame +0, number) formats the result, which the destination takes (0x08A5BAD8(destination, text)
// unless it is that slot), and the slot is released (0x08A5B9EC).
void func_08840A0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], count = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x230u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x21Cu}, {31u, 0x224u}, {20u, 0x220u}, {18u, 0x218u}, {17u, 0x214u}, {16u, 0x210u}});
    ctx.gpr[19] = destination;
    ctx.gpr[4] = value;
    if (static_cast<s32>(count) <= 0) return;
    const u32 pair_low = call_set(rt, ctx, 0x0883B950u, {value});
    const u32 number_source = call_set(rt, ctx, 0x089312DCu, {pair_low, ctx.gpr[3]});
    const u32 small = frame.at(0x88u), text = frame.at(0xC8u), first = frame.at(0x108u), second = frame.at(0x148u);
    ctx.gpr[16] = small;
    call_set(rt, ctx, 0x08B3767Cu, {small, 0x40u, 0x08C9C478u, number_source});
    ctx.gpr[17] = text;
    ctx.gpr[18] = first;
    call_set(rt, ctx, 0x08B37640u, {text, 0x08C9C4ECu, small});
    ctx.gpr[20] = second;
    call_set(rt, ctx, 0x08B0CE58u, {first});
    const u32 built = frame.at(0x190u);
    ctx.gpr[16] = built;
    call_set(rt, ctx, 0x08B0CE58u, {second});
    call_set(rt, ctx, 0x08B0EC60u, {built, text});
    // a 0x3C-byte record copied through the frame (+0x48 / +0x8) to its place
    auto move_record = [&](u32 from, u32 through, u32 to) {
        for (u32 offset = 0u; offset < 0x3Cu; offset += 4u) put(rt, through + offset, word(rt, from + offset));
        for (u32 offset = 0u; offset < 0x3Cu; offset += 4u) put(rt, to + offset, word(rt, through + offset));
    };
    move_record(built, frame.at(0x48u), first);
    ctx.gpr[16] = built + 0x30u;
    u32 result = 0u;
    if (word(rt, first) == 0u) {
        const u32 second_built = frame.at(0x1D0u);
        ctx.gpr[16] = second_built;
        call_set(rt, ctx, 0x08B10204u, {second_built, text, 1u});
        move_record(second_built, frame.at(0x8u), second);
        ctx.gpr[16] = second_built + 0x30u;
        if (word(rt, second) == 0u) {
            const u32 owner = word(rt, frame.at(0x168u));
            ctx.gpr[16] = owner;
            call_set(rt, ctx, 0x088FDDF8u, {owner});
            call_set(rt, ctx, 0x088FDA54u, {owner});
            result = call_set(rt, ctx, 0x08B35254u, {owner});
        }
    }
    ctx.gpr[17] = result;
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0x0u), result});
    if (destination != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0x0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
}
void func_0885B97C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); G[17] = G[4]; put(rt, frame.at(0x8u), G[31]); put(rt, frame.at(0x0u), G[16]); G[4] = word(rt, G[4] + 0x0u); G[16] = word(rt, G[4] + 0x7Cu); if (G[16] == 0u) { G[16] = word(rt, G[4] + 0x80u); goto L_0885B9B8; } G[4] = G[16]; call_here(rt, ctx, 0x088D5CF0u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); G[4] = word(rt, G[17] + 0x0u); G[16] = word(rt, G[4] + 0x80u); L_0885B9B8: ; if (G[16] == 0u) { G[16] = word(rt, G[4] + 0x84u); goto L_0885B9D8; } G[4] = G[16]; call_here(rt, ctx, 0x08894028u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); G[4] = word(rt, G[17] + 0x0u); G[16] = word(rt, G[4] + 0x84u); L_0885B9D8: ; if (G[16] == 0u) { G[16] = word(rt, G[4] + 0x88u); goto L_0885B9F8; } G[4] = G[16]; call_here(rt, ctx, 0x08893CC0u); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); G[4] = word(rt, G[17] + 0x0u); G[16] = word(rt, G[4] + 0x88u); L_0885B9F8: ; if (G[16] == 0u) { G[5] = word(rt, G[4] + 0x0u); goto L_0885BA18; } G[4] = G[16]; call_here(rt, ctx, 0x088D435Cu); G[4] = G[16]; call_here(rt, ctx, 0x08B64654u); G[4] = word(rt, G[17] + 0x0u); G[5] = word(rt, G[4] + 0x0u); L_0885BA18: ; if (G[5] == 0u) goto L_0885BA6C; G[2] = word(rt, G[5] - 0x4u); G[4] = G[2] << 6u; G[2] = G[2] << 3u; G[4] = G[4] - G[2]; G[3] = G[4] << 3u; G[3] = G[3] - G[4]; G[16] = G[5] + G[3]; if (G[5] == G[16]) goto L_0885BA60; G[16] = G[16] - 0x188u; L_0885BA48: ; G[4] = G[16]; call_here(rt, ctx, 0x0885D3A0u); G[2] = word(rt, G[17] + 0x0u); G[3] = word(rt, G[2] + 0x0u); if (G[3] != G[16]) { G[16] = G[16] - 0x188u; goto L_0885BA48; } L_0885BA60: ; G[4] = G[16] - 0x8u; call_here(rt, ctx, 0x08B61FC0u); G[4] = word(rt, G[17] + 0x0u); L_0885BA6C: ; call_here(rt, ctx, 0x08B64654u); G[4] = G[17] + 0x8u; G[31] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); tail = 0x08B28DC4u; goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_0886C44C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[31]); put(rt, frame.at(0x38u), G[22]); G[22] = G[6]; G[6] = frame.at(0x4u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); G[20] = frame.at(0xCu); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[16] = word(rt, G[4] + 0x4u); G[4] = G[20]; call_here(rt, ctx, 0x08B93CD8u); G[21] = G[16] + 0x1F4u; G[4] = G[21]; G[5] = G[20]; call_here(rt, ctx, 0x08B93E98u); G[16] = G[16] + 0x1F8u; { const bool tk = G[16] == G[2]; G[19] = G[2]; if (tk) goto L_0886C4F8; } G[4] = word(rt, frame.at(0xCu)); G[18] = G[2]; G[5] = frame.at(0x14u); G[16] = word(rt, G[4] - 0xCu); put(rt, frame.at(0x10u), G[16]); G[2] = word(rt, G[2] + 0x10u); G[17] = word(rt, G[2] - 0xCu); G[2] = frame.at(0x10u); G[3] = G[17] < G[16] ? 1u : 0u; if (G[3] == 0u) G[5] = G[2]; put(rt, frame.at(0x14u), G[17]); G[16] = G[16] - G[17]; G[6] = word(rt, G[5] + 0x0u); G[5] = word(rt, G[19] + 0x10u); call_here(rt, ctx, 0x08B36170u); if (G[2] != 0u) G[16] = G[2]; { const bool tk = static_cast<std::int32_t>(G[16]) < 0; G[2] = high_half(0x08D75298u); if (tk) goto L_0886C4F8; } G[23] = 0x08D75298u; goto L_0886C5C0; L_0886C4F8: ; G[2] = high_half(0x08D75298u); G[16] = frame.at(0x18u); G[23] = 0x08D75298u; G[2] = G[23] + 0xCu; G[4] = G[16]; G[5] = G[20]; put(rt, frame.at(0x8u), G[2]); call_here(rt, ctx, 0x08B9432Cu); G[4] = frame.at(0x1Cu); G[5] = frame.at(0x8u); call_here(rt, ctx, 0x08B9432Cu); G[4] = G[21]; G[6] = G[16]; G[5] = G[19]; call_here(rt, ctx, 0x08B94A58u); G[4] = word(rt, frame.at(0x1Cu)); G[16] = G[4] - 0xCu; { const bool tk = G[16] == G[23]; G[17] = G[2]; if (tk) goto L_0886C564; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = word(rt, frame.at(0x18u)); if (tk) goto L_0886C568; } G[4] = G[16]; G[5] = frame.at(0x3u); call_here(rt, ctx, 0x08B93554u); L_0886C564: ; G[4] = word(rt, frame.at(0x18u)); L_0886C568: ; G[16] = G[4] - 0xCu; if (G[23] == G[16]) { G[4] = word(rt, frame.at(0x8u)); goto L_0886C598; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = word(rt, frame.at(0x8u)); if (tk) goto L_0886C598; } G[4] = G[16]; G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B93554u); G[4] = word(rt, frame.at(0x8u)); L_0886C598: ; G[16] = G[4] - 0xCu; { const bool tk = G[23] == G[16]; G[18] = G[17]; if (tk) goto L_0886C5C0; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (tk) goto L_0886C5C0; } G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B93554u); L_0886C5C0: ; G[4] = G[22]; call_here(rt, ctx, 0x08B35F60u); G[16] = G[18] + 0x14u; G[4] = G[16]; G[5] = G[22]; G[6] = G[2]; call_here(rt, ctx, 0x08B945C4u); G[4] = word(rt, frame.at(0xCu)); G[16] = G[4] - 0xCu; { const bool tk = G[16] == G[23]; G[31] = word(rt, frame.at(0x40u)); if (tk) goto L_0886C610; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[31] = word(rt, frame.at(0x40u)); if (tk) goto L_0886C610; } G[4] = G[16]; G[5] = G[29]; call_here(rt, ctx, 0x08B93554u); G[31] = word(rt, frame.at(0x40u)); L_0886C610: ; G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x0887DF4C (a0 = the owner, a1 = a name string): unless the record at the owner's +0x58 has bit 1 of its halfword +0x26 set, the name is split at its last
// ':' (the prefix before it) and last '%' (the text after it), copied to frame buffers (+0, +0x20; the middle part to +0x40) with 0x08B79A88(dest, source, length).
// The prefix compared with 0x08CA0338 (0x08B35FD8 returning 0) is the case without a pattern: the middle part is formatted with 0x08B37640(+0x100,
// 0x08CA0340, middle) and, when 0x0886AF64(owner +0x60) is set, the second one with 0x08B37640(+0x180, 0x08CA0354, middle). The record gets bit 0x10000 in +0x24,
// its +8 and +4 from 0x0886B0A4(owner +0x60) / 0x0886B0C8(owner +0x60, 0), a kind byte at +0x24 (2 with the second text, else 1) and the texts
// (0x08B35F60 length, 0x08B945C4(record + 0xC or +0x10, text, length)); then 0x08BA42D0(slot) and 0x0886BC14(owner +0x60). v0 = bit 1 of the record's +0x26 (read again).
void func_0887DF4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], text = ctx.gpr[5];
    GuestFrame frame(ctx, 0x230u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x214u}, {20u, 0x210u}, {18u, 0x208u}, {31u, 0x220u}, {23u, 0x21Cu}, {22u, 0x218u}, {19u, 0x20Cu}, {17u, 0x204u}, {16u, 0x200u}});
    const u32 slot = self + 0x58u;
    ctx.gpr[21] = self;
    ctx.gpr[20] = slot;
    ctx.gpr[18] = text;
    ctx.gpr[3] = word(rt, slot);
    ctx.gpr[2] = load16(ctx.gpr[3] + 0x26u) & 0x1u;
    if (ctx.gpr[2] == 0u) {
        const u32 head = frame.at(0x0u), tail = frame.at(0x20u), middle = frame.at(0x40u);
        store8(middle, 0u);
        store8(tail, 0u);
        store8(head, 0u);
        u32 name = text;
        u32 end = 0u, skip = 0u, kept = 0u, middle_length = 0u;
        ctx.gpr[17] = 0u;
        ctx.gpr[23] = 0u;
        if (signed_byte(rt, text) != 0u) {
            u32 percent = 0xFFFFFFFFu, colon = 0xFFFFFFFFu;
            u32 index = 0u;
            for (u32 c = signed_byte(rt, text); c != 0u; c = signed_byte(rt, text + index)) {
                if (c == 0x3Au) colon = index;
                else if (c == 0x25u) percent = index;
                ++index;
            }
            ctx.gpr[19] = percent;
            ctx.gpr[16] = index;
            ctx.gpr[17] = colon;
            end = index;
            if (static_cast<s32>(colon) >= 0) {
                call_set(rt, ctx, 0x08B79A88u, {head, text, colon});
                skip = colon + 1u;
                store8(head + colon, 0u);
                kept = skip;
            }
            ctx.gpr[23] = skip;
            ctx.gpr[22] = kept;
            middle_length = end - kept;
            if (static_cast<s32>(percent) >= 0) {
                const u32 length = end - percent - 1u;
                call_set(rt, ctx, 0x08B79A88u, {tail, text + percent + 1u, length});
                store8(tail + length, 0u);
                middle_length = percent - kept;
            }
        }
        ctx.gpr[19] = middle;
        call_set(rt, ctx, 0x08B79A88u, {middle, text + skip, middle_length});
        store8(middle + middle_length, 0u);
        ctx.gpr[22] = 0u;
        u32 second = 0u;
        if (call_set(rt, ctx, 0x08B35FD8u, {head, 0x08CA0338u}) == 0u) {
            const u32 first_buffer = frame.at(0x100u);
            ctx.gpr[17] = first_buffer;
            ctx.gpr[18] = first_buffer;
            name = first_buffer;
            call_set(rt, ctx, 0x08B37640u, {first_buffer, 0x08CA0340u, middle});
            if (call_set(rt, ctx, 0x0886AF64u, {word(rt, self + 0x60u)}) != 0u) {
                const u32 second_buffer = frame.at(0x180u);
                ctx.gpr[16] = second_buffer;
                call_set(rt, ctx, 0x08B37640u, {second_buffer, 0x08CA0354u, middle});
                second = second_buffer;
                ctx.gpr[22] = second;
            }
        }
        ctx.gpr[18] = name;
        const u32 owner = word(rt, slot);
        put(rt, owner + 0x24u, word(rt, owner + 0x24u) | 0x10000u);
        ctx.gpr[16] = word(rt, slot);
        put(rt, ctx.gpr[16] + 0x8u, call_set(rt, ctx, 0x0886B0A4u, {word(rt, self + 0x60u)}));
        ctx.gpr[16] = word(rt, slot);
        put(rt, ctx.gpr[16] + 0x4u, call_set(rt, ctx, 0x0886B0C8u, {word(rt, self + 0x60u), 0u}));
        const u32 record = word(rt, slot);
        ctx.gpr[17] = record;
        if (second != 0u) {
            ctx.gpr[2] = 0x2u;
            store8(record + 0x24u, 2u);
            u32 length = call_set(rt, ctx, 0x08B35F60u, {second});
            ctx.gpr[16] = record + 0xCu;
            call_set(rt, ctx, 0x08B945C4u, {record + 0xCu, second, length});
            length = call_set(rt, ctx, 0x08B35F60u, {name});
            ctx.gpr[16] = record + 0x10u;
            call_set(rt, ctx, 0x08B945C4u, {record + 0x10u, name, length});
        } else {
            ctx.gpr[2] = 0x1u;
            store8(record + 0x24u, 1u);
            const u32 length = call_set(rt, ctx, 0x08B35F60u, {name});
            ctx.gpr[16] = record + 0xCu;
            call_set(rt, ctx, 0x08B945C4u, {record + 0xCu, name, length});
        }
        call_set(rt, ctx, 0x08BA42D0u, {slot});
        call_set(rt, ctx, 0x0886BC14u, {word(rt, self + 0x60u)});
    }
    ctx.gpr[3] = word(rt, slot);
    ctx.gpr[2] = load16(ctx.gpr[3] + 0x26u) & 0x1u;
}
void func_0888A2A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x180u); put(rt, frame.at(0x168u), G[18]); G[18] = G[5]; put(rt, frame.at(0x160u), G[16]); G[16] = G[4]; put(rt, frame.at(0x17Cu), G[31]); put(rt, frame.at(0x178u), G[22]); put(rt, frame.at(0x174u), G[21]); put(rt, frame.at(0x170u), G[20]); put(rt, frame.at(0x16Cu), G[19]); { const bool tk = G[5] != 0u; put(rt, frame.at(0x164u), G[17]); if (tk) goto L_0888A344; } G[21] = G[4] + 0x40u; G[6] = word(rt, G[21] + 0x4u); G[5] = word(rt, G[4] + 0x40u); G[3] = 0x2FA00000u; G[3] = G[3] | 0xBE83u; G[2] = G[6] - G[5]; G[2] = sra(G[2], 2u); { const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); } G[17] = high_half(0x08CC3528u); G[3] = 0x08CC3528u; put(rt, frame.at(0x150u), G[3]); G[2] = ctx.lo; { const bool tk = G[2] == 0u; put(rt, frame.at(0x154u), 0u); if (tk) goto L_0888A320; } G[4] = G[21]; call_here(rt, ctx, 0x08BAAD9Cu); G[19] = high_half(0x08CC1AA8u); goto L_0888A338; L_0888A320: ; G[5] = G[6]; G[4] = G[21]; G[6] = 0u; G[7] = frame.at(0xACu); call_here(rt, ctx, 0x08BAB0A0u); G[19] = high_half(0x08CC1AA8u); L_0888A338: ; G[2] = G[19] + sext(0x08CC1AA8u, 16u); put(rt, frame.at(0x150u), G[2]); goto L_0888A350; L_0888A344: ; G[21] = G[4] + 0x40u; G[17] = high_half(0x08CC3528u); G[19] = high_half(0x08CC1AA8u); L_0888A350: ; G[5] = G[18]; G[4] = G[16]; call_here(rt, ctx, 0x088CD73Cu); G[3] = word(rt, G[18] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2]; G[4] = G[18]; call_here(rt, ctx, callee); } G[5] = word(rt, G[16] + 0x40u); G[7] = word(rt, G[21] + 0x4u); G[6] = G[2]; G[2] = 0x2FA00000u; G[3] = G[7] - G[5]; G[3] = sra(G[3], 2u); G[2] = G[2] | 0xBE83u; { const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); } G[4] = G[17] + sext(0x08CC3528u, 16u); put(rt, frame.at(0xA4u), G[4]); G[3] = ctx.lo; G[2] = G[6] < G[3] ? 1u : 0u; { const bool tk = G[2] == 0u; put(rt, frame.at(0xA8u), 0u); if (tk) goto L_0888A3C8; } ctx.lo = G[5]; G[2] = 0xACu; G[4] = G[21]; G[6] = G[7]; G[5] = ctx.lo; call_here(rt, ctx, 0x08BAAD9Cu); G[2] = 0x2FA00000u; goto L_0888A3E0; L_0888A3C8: ; G[5] = G[7]; G[6] = G[6] - G[3]; G[4] = G[21]; G[7] = G[29]; call_here(rt, ctx, 0x08BAB0A0u); G[2] = 0x2FA00000u; L_0888A3E0: ; G[3] = G[19] + sext(0x08CC1AA8u, 16u); G[22] = G[2] | 0xBE83u; G[17] = 0u; G[19] = 0u; put(rt, frame.at(0xA4u), G[3]); goto L_0888A434; L_0888A3F8: ; G[3] = word(rt, G[18] + 0x0u); G[2] = word(rt, G[3] + 0x18u); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } G[16] = G[2]; { const bool tk = G[2] == 0u; G[4] = G[18]; if (tk) goto L_0888A434; } G[3] = word(rt, G[18] + 0x0u); G[2] = word(rt, G[3] + 0x3Cu); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } G[6] = G[2]; G[4] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x088CD9C8u); L_0888A434: ; G[3] = word(rt, G[21] + 0x0u); G[2] = word(rt, G[21] + 0x4u); G[5] = G[17]; G[20] = G[19] + G[3]; G[2] = G[2] - G[3]; G[2] = sra(G[2], 2u); { const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[22]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); } G[4] = G[18]; G[19] = G[19] + 0xACu; G[2] = ctx.lo; G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[17] = G[17] + 0x1u; if (tk) goto L_0888A3F8; } G[31] = word(rt, frame.at(0x17Cu)); G[22] = word(rt, frame.at(0x178u)); G[21] = word(rt, frame.at(0x174u)); G[20] = word(rt, frame.at(0x170u)); G[19] = word(rt, frame.at(0x16Cu)); G[18] = word(rt, frame.at(0x168u)); G[17] = word(rt, frame.at(0x164u)); G[16] = word(rt, frame.at(0x160u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_088B1F60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xC0u); put(rt, frame.at(0xACu), G[19]); G[19] = G[5]; put(rt, frame.at(0xA8u), G[18]); G[18] = G[6]; put(rt, frame.at(0xA4u), G[17]); G[17] = G[4]; put(rt, frame.at(0xB0u), G[31]); put(rt, frame.at(0xA0u), G[16]); G[2] = word(rt, G[6] + 0x0u); { const bool tk = G[2] == 0u; put(rt, G[4] + 0x8u, G[2]); if (tk) goto L_088B1FAC; } G[3] = word(rt, G[2] + 0x0u); G[4] = G[2]; G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } { const bool tk = G[2] != 0u; G[2] = 0x1u; if (tk) goto L_088B2198; } L_088B1FAC: ; store8(G[17] + 0x328u, 0u); L_088B1FB0: ; G[2] = 0x2u; store8(G[17] + 0x329u, G[2]); G[3] = word(rt, G[18] + 0x4u); put(rt, G[17] + 0xCu, G[3]); call_here(rt, ctx, 0x088BA61Cu); G[5] = G[2]; G[4] = G[17] + 0x10u; G[6] = 0x1u; call_here(rt, ctx, 0x088BADB4u); G[4] = G[18]; G[3] = G[17]; G[5] = 0u; G[6] = 0x4u; L_088B1FE4: ; G[2] = word(rt, G[4] + 0x14u); G[5] = G[5] + 0x1u; G[4] = G[4] + 0x4u; put(rt, G[3] + 0x2CCu, G[2]); { const bool tk = G[5] != G[6]; G[3] = G[3] + 0x4u; if (tk) goto L_088B1FE4; } G[2] = word(rt, G[18] + 0xCu); G[8] = word(rt, G[17] + 0x300u); G[9] = word(rt, G[17] + 0x304u); put(rt, G[17] + 0x2F8u, G[2]); G[3] = 0u; G[3] = G[3] << 16u; G[5] = word(rt, G[18] + 0x10u); G[6] = G[8] | G[9]; G[16] = G[17] + 0x300u; G[4] = G[5] >> 16u; G[3] = G[4] | G[3]; G[2] = G[5] << 16u; put(rt, G[17] + 0x2FCu, G[5]); G[4] = G[2] | 0x330Eu; { const bool tk = G[6] == 0u; G[5] = G[3]; if (tk) goto L_088B2050; } G[6] = G[8]; G[7] = G[9]; call_here(rt, ctx, 0x08B708C4u); G[4] = G[2]; G[5] = G[3]; L_088B2050: ; put(rt, G[16] + 0x8u, G[4]); G[2] = high_half(0x088B448Cu); G[2] = 0x088B448Cu; put(rt, G[16] + 0xCu, G[5]); G[16] = G[17] + 0x2DCu; G[4] = frame.at(0x70u); G[6] = word(rt, G[16] + 0x4u); G[5] = word(rt, G[17] + 0x2DCu); G[7] = frame.at(0x30u); G[6] = G[6] << 10u; G[6] = G[5] + G[6]; put(rt, frame.at(0x30u), G[2]); put(rt, frame.at(0x38u), G[19]); put(rt, frame.at(0x68u), G[19]); put(rt, frame.at(0x60u), G[2]); put(rt, frame.at(0x64u), 0u); put(rt, frame.at(0x34u), 0u); call_here(rt, ctx, 0x08BB0F28u); G[8] = high_half(0x088B4494u); G[2] = word(rt, frame.at(0x70u)); G[6] = word(rt, G[16] + 0x4u); G[5] = word(rt, G[17] + 0x2DCu); G[9] = word(rt, G[17] + 0x8u); G[3] = word(rt, frame.at(0x74u)); put(rt, frame.at(0x3Cu), G[2]); G[6] = G[6] << 10u; G[8] = 0x088B4494u; G[2] = word(rt, frame.at(0x78u)); G[6] = G[5] + G[6]; G[4] = frame.at(0x80u); G[7] = frame.at(0x18u); put(rt, frame.at(0x40u), G[3]); put(rt, frame.at(0x44u), G[2]); put(rt, frame.at(0x18u), G[8]); put(rt, frame.at(0x20u), G[9]); put(rt, frame.at(0x5Cu), G[9]); put(rt, frame.at(0x54u), G[8]); put(rt, frame.at(0x58u), 0u); put(rt, frame.at(0x1Cu), 0u); call_here(rt, ctx, 0x08BB0FF0u); G[8] = high_half(0x088B449Cu); G[2] = word(rt, frame.at(0x80u)); G[6] = word(rt, G[16] + 0x4u); G[5] = word(rt, G[17] + 0x2DCu); G[9] = word(rt, G[17] + 0xCu); G[3] = word(rt, frame.at(0x84u)); put(rt, frame.at(0x24u), G[2]); G[6] = G[6] << 10u; G[8] = 0x088B449Cu; G[2] = word(rt, frame.at(0x88u)); G[6] = G[5] + G[6]; G[7] = G[29]; G[4] = frame.at(0x90u); put(rt, frame.at(0x0u), G[8]); put(rt, frame.at(0x8u), G[9]); put(rt, frame.at(0x50u), G[9]); put(rt, frame.at(0x48u), G[8]); put(rt, frame.at(0x28u), G[3]); put(rt, frame.at(0x2Cu), G[2]); put(rt, frame.at(0x4Cu), 0u); put(rt, frame.at(0x4u), 0u); call_here(rt, ctx, 0x08BB10B8u); G[4] = G[17] + 0x310u; G[2] = word(rt, frame.at(0x90u)); G[3] = word(rt, frame.at(0x94u)); put(rt, frame.at(0xCu), G[2]); G[2] = word(rt, frame.at(0x98u)); put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(0x14u), G[2]); call_here(rt, ctx, 0x088B4100u); store8(G[17] + 0x32Au, 0u); G[5] = word(rt, G[18] + 0x8u); G[4] = word(rt, G[17] + 0x8u); call_here(rt, ctx, 0x088B1C94u); store8(G[17] + 0x32Au, G[2]); G[31] = word(rt, frame.at(0xB0u)); G[19] = word(rt, frame.at(0xACu)); G[18] = word(rt, frame.at(0xA8u)); G[17] = word(rt, frame.at(0xA4u)); G[16] = word(rt, frame.at(0xA0u)); goto exit_; L_088B2198: ; store8(G[17] + 0x328u, G[2]); goto L_088B1FB0; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_088DC48C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0x8u), G[18]); G[18] = G[4]; put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[19]); G[4] = word(rt, G[4] + 0x14u); call_here(rt, ctx, 0x088FE518u); G[4] = G[18] + 0x4u; G[16] = G[2]; call_here(rt, ctx, 0x088D5EACu); G[4] = word(rt, G[18] + 0x5D4u); { const bool tk = G[4] == 0u; G[17] = G[2]; if (tk) goto L_088DC510; } G[2] = word(rt, G[18] + 0x5D0u); if (G[2] == 0u) { G[4] = word(rt, G[18] + 0x148u); goto L_088DC514; } call_here(rt, ctx, 0x088F9F00u); G[4] = word(rt, G[18] + 0x5D0u); call_here(rt, ctx, 0x08B043D8u); G[4] = word(rt, G[18] + 0x5D0u); G[5] = G[18] + 0x5D8u; put(rt, G[18] + 0x5DCu, G[16]); put(rt, G[18] + 0x5E4u, G[16]); call_here(rt, ctx, 0x08B03A1Cu); G[5] = G[18] + 0x5E0u; G[4] = word(rt, G[18] + 0x5D0u); call_here(rt, ctx, 0x08B03A1Cu); G[4] = word(rt, G[18] + 0x5D0u); G[5] = 0x1u; call_here(rt, ctx, 0x08B03A2Cu); L_088DC510: ; G[4] = word(rt, G[18] + 0x148u); L_088DC514: ; G[5] = G[16]; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x68u); { const std::uint32_t callee = G[2]; G[6] = G[17]; call_here(rt, ctx, callee); } G[4] = word(rt, G[18] + 0x14Cu); { const bool tk = G[4] == 0u; G[5] = G[16]; if (tk) goto L_088DC548; } G[2] = word(rt, G[4] + 0x0u); G[6] = word(rt, G[18] + 0x18u); G[3] = word(rt, G[2] + 0x14u); { const std::uint32_t callee = G[3]; G[7] = G[17]; call_here(rt, ctx, callee); } L_088DC548: ; G[4] = word(rt, G[18] + 0x5CCu); { const bool tk = G[4] == 0u; G[5] = G[16]; if (tk) goto L_088DC568; } G[2] = word(rt, G[4] + 0x0u); G[6] = word(rt, G[18] + 0x18u); G[3] = word(rt, G[2] + 0xCu); { const std::uint32_t callee = G[3]; G[7] = G[17]; call_here(rt, ctx, callee); } L_088DC568: ; G[4] = word(rt, G[18] + 0x154u); { const bool tk = G[4] == 0u; G[5] = 0x3u; if (tk) goto L_088DC580; } G[5] = G[17]; call_here(rt, ctx, 0x088F4A84u); G[5] = 0x3u; L_088DC580: ; G[4] = G[18]; call_here(rt, ctx, 0x088DC1B8u); G[4] = word(rt, G[18] + 0x618u); call_here(rt, ctx, 0x088D1E14u); G[4] = word(rt, G[18] + 0x618u); call_here(rt, ctx, 0x088D1E80u); G[19] = word(rt, G[18] + 0x5C8u); if (G[19] == 0u) { G[16] = word(rt, G[18] + 0x594u); goto L_088DC5D4; } G[2] = word(rt, G[19] + 0x0u); G[4] = word(rt, G[18] + 0x14u); G[17] = word(rt, G[2] + 0x34u); call_here(rt, ctx, 0x088FE6A0u); G[4] = word(rt, G[18] + 0x14u); G[16] = G[2]; call_here(rt, ctx, 0x088FE3A4u); G[6] = G[2]; G[4] = G[19]; { const std::uint32_t callee = G[17]; G[5] = G[16]; call_here(rt, ctx, callee); } G[16] = word(rt, G[18] + 0x594u); L_088DC5D4: ; if (G[16] == 0u) { G[4] = word(rt, G[18] + 0x640u); goto L_088DC5F8; } G[4] = word(rt, G[18] + 0x14u); call_here(rt, ctx, 0x088FE6A0u); G[6] = word(rt, G[18] + 0x590u); G[4] = G[16]; G[5] = G[2]; call_here(rt, ctx, 0x088E7688u); G[4] = word(rt, G[18] + 0x640u); L_088DC5F8: ; { const bool tk = G[4] == 0u; G[16] = 0u; if (tk) goto L_088DC6AC; } G[5] = load8(G[18] + 0x5BEu); G[19] = 0xFFFFFFFFu; call_here(rt, ctx, 0x088E5C88u); G[17] = 0x48190000u; goto L_088DC690; L_088DC614: ; G[5] = G[16]; call_here(rt, ctx, 0x088E4C08u); put(rt, G[2] + 0x8u, 0u); G[6] = G[17] | 0x7FFu; G[8] = G[2]; store16(G[2] + 0x0u, G[16]); G[7] = G[2]; G[9] = 0u; store16(G[2] + 0x2u, G[19]); G[11] = 0x48190000u; G[10] = 0x10u; store8(G[2] + 0x4u, 0u); store16(G[2] + 0xCu, 0u); store16(G[2] + 0xEu, 0u); G[3] = load8(G[18] + 0x27u); if (G[3] != 0u) G[6] = 0u; put(rt, G[2] + 0x8u, G[6]); G[4] = word(rt, G[18] + 0x80u); G[5] = word(rt, G[18] + 0x84u); put(rt, G[2] + 0x78u, G[4]); put(rt, G[2] + 0x7Cu, G[5]); L_088DC668: ; G[3] = load8(G[18] + 0x27u); G[2] = G[11] | 0x7FFu; G[9] = G[9] + 0x1u; if (G[3] != 0u) G[2] = 0u; put(rt, G[7] + 0x10u, G[2]); G[7] = G[7] + 0x4u; store16(G[8] + 0x50u, 0u); { const bool tk = G[9] != G[10]; G[8] = G[8] + 0x2u; if (tk) goto L_088DC668; } G[16] = G[16] + 0x1u; L_088DC690: ; G[4] = G[18]; call_here(rt, ctx, 0x088D9F80u); G[2] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { G[4] = word(rt, G[18] + 0x640u); goto L_088DC614; } G[2] = word(rt, G[18] + 0x130u); goto L_088DC6BC; L_088DC6AC: ; G[3] = word(rt, G[18] + 0x130u); G[2] = load16(G[3] + 0x4u); store16(G[3] + 0xAu, G[2]); G[2] = word(rt, G[18] + 0x130u); L_088DC6BC: ; G[6] = 0u; G[7] = 0u; store8(G[2] + 0xFu, 0u); G[4] = G[18]; G[5] = 0x1u; G[2] = word(rt, G[18] + 0x130u); put(rt, G[2] + 0x38u, G[6]); put(rt, G[2] + 0x3Cu, G[7]); G[2] = 0x1u; G[3] = word(rt, G[18] + 0x130u); put(rt, G[3] + 0x40u, G[6]); put(rt, G[3] + 0x44u, G[7]); store8(G[18] + 0x1Eu, G[2]); put(rt, G[18] + 0xB0u, 0u); call_here(rt, ctx, 0x088D78D8u); store8(G[18] + 0x5BEu, 0u); G[31] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_088ED560(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 result = reset_state(rt, ctx, self);
    call_set(rt, ctx, 0x088ECB0Cu, {self, result});
}
void func_08920C68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); G[23] = G[6]; put(rt, frame.at(0x14u), G[21]); G[21] = G[7]; put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); G[19] = G[4]; G[20] = G[19] + 0x40u; put(rt, frame.at(0x8u), G[18]); G[18] = 0xFFFFFFFFu; put(rt, frame.at(0x4u), G[17]); G[17] = 0x2u; put(rt, frame.at(0x0u), G[16]); G[16] = G[20]; put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[30]); put(rt, frame.at(0x18u), G[22]); call_here(rt, ctx, 0x08925FE4u); G[2] = high_half(0x08CC5EE0u); G[2] = 0x08CC5EE0u; put(rt, G[19] + 0x0u, G[2]); L_08920CC0: ; G[4] = G[16]; G[17] = G[17] - 0x1u; call_here(rt, ctx, 0x08926398u); { const bool tk = G[17] != G[18]; G[16] = G[16] + 0x2Cu; if (tk) goto L_08920CC0; } G[22] = G[19] + 0xC4u; G[16] = G[22]; G[17] = 0x3u; L_08920CE0: ; G[4] = G[16]; G[17] = G[17] - 0x1u; call_here(rt, ctx, 0x08926398u); { const bool tk = G[17] != 0u; G[16] = G[16] + 0x2Cu; if (tk) goto L_08920CE0; } G[30] = G[19] + 0x148u; G[16] = G[30]; G[17] = 0x3u; L_08920D00: ; G[4] = G[16]; G[17] = G[17] - 0x1u; call_here(rt, ctx, 0x08926398u); { const bool tk = G[17] != 0u; G[16] = G[16] + 0x2Cu; if (tk) goto L_08920D00; } G[7] = G[19] + 0x30u; G[2] = G[19] + 0x10u; G[3] = G[19] + 0x20u; put(rt, G[19] + 0x8u, G[21]); G[16] = high_half(0x08CDDCC0u); G[4] = G[20]; put(rt, G[19] + 0xCu, G[23]); G[5] = 0x08CDDCC0u; G[6] = 0x1u; put(rt, G[19] + 0x10u, 0u); G[17] = high_half(0x08CDDCA8u); put(rt, G[2] + 0x8u, 0u); put(rt, G[2] + 0xCu, 0u); put(rt, G[2] + 0x4u, 0u); put(rt, G[19] + 0x20u, 0u); put(rt, G[3] + 0xCu, 0u); put(rt, G[3] + 0x4u, 0u); put(rt, G[3] + 0x8u, 0u); put(rt, G[19] + 0x30u, 0u); put(rt, G[7] + 0xCu, 0u); put(rt, G[7] + 0x4u, 0u); put(rt, G[7] + 0x8u, 0u); G[2] = word(rt, G[19] + 0x40u); G[3] = word(rt, G[2] + 0x8u); { const std::uint32_t callee = G[3]; G[18] = high_half(0x08CDDC90u); call_here(rt, ctx, callee); } G[6] = word(rt, G[19] + 0x6Cu); G[4] = G[19] + 0x6Cu; G[5] = G[16] + sext(0x08CDDCC0u, 16u); G[2] = word(rt, G[6] + 0x8u); { const std::uint32_t callee = G[2]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[3] = word(rt, G[19] + 0x98u); G[5] = G[16] + sext(0x08CDDCC0u, 16u); G[4] = G[19] + 0x98u; G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[3] = word(rt, G[19] + 0xC4u); G[4] = G[22]; G[5] = G[17] + sext(0x08CDDCA8u, 16u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[3] = word(rt, G[19] + 0xF0u); G[4] = G[19] + 0xF0u; G[5] = G[17] + sext(0x08CDDCA8u, 16u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[3] = word(rt, G[19] + 0x11Cu); G[5] = G[17] + sext(0x08CDDCA8u, 16u); G[4] = G[19] + 0x11Cu; G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[3] = word(rt, G[19] + 0x148u); G[4] = G[30]; G[5] = G[18] + sext(0x08CDDC90u, 16u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[3] = word(rt, G[19] + 0x174u); G[5] = G[18] + sext(0x08CDDC90u, 16u); G[4] = G[19] + 0x174u; G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[3] = word(rt, G[19] + 0x1A0u); G[5] = high_half(0x08CDDC78u); G[4] = G[19] + 0x1A0u; G[7] = word(rt, G[3] + 0x8u); G[5] = 0x08CDDC78u; { const std::uint32_t callee = G[7]; G[6] = 0x1u; call_here(rt, ctx, callee); } G[2] = 0x5u; put(rt, G[19] + 0x1CCu, G[2]); G[31] = word(rt, frame.at(0x24u)); G[30] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_089372C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x1F0u); G[2] = high_half(0x08CDDE0Cu); put(rt, frame.at(0x1E0u), G[30]); G[30] = G[6]; put(rt, frame.at(0x1DCu), G[23]); G[23] = G[5]; put(rt, frame.at(0x1D8u), G[22]); G[22] = frame.at(0x180u); put(rt, frame.at(0x1D4u), G[21]); G[21] = frame.at(0x40u); put(rt, frame.at(0x1D0u), G[20]); G[20] = 0u; put(rt, frame.at(0x1CCu), G[19]); G[19] = frame.at(0x1B0u); put(rt, frame.at(0x1C8u), G[18]); G[18] = frame.at(0x30u); put(rt, frame.at(0x1C4u), G[17]); G[17] = 0x08CDDE0Cu; put(rt, frame.at(0x1C0u), G[16]); G[16] = frame.at(0x80u); put(rt, frame.at(0x1E4u), G[31]); G[5] = G[23]; L_08937320: ; G[4] = G[16]; call_here(rt, ctx, 0x08B7BB6Cu); G[5] = word(rt, G[17] + 0x0u); G[4] = G[16]; call_here(rt, ctx, 0x08B7B9ACu); G[5] = G[30]; G[4] = G[16]; call_here(rt, ctx, 0x08B7B9ACu); G[4] = G[21]; call_here(rt, ctx, 0x08B0CE58u); G[4] = G[22]; G[5] = G[16]; call_here(rt, ctx, 0x08B0EC60u); G[6] = G[22]; G[7] = G[29]; L_0893735C: ; G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[6] + 0x4u); G[4] = word(rt, G[6] + 0x8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7] + 0x0u, G[2]); G[6] = G[6] + 0x10u; G[7] = G[7] + 0x10u; put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 0x8u, G[4]); { const bool tk = G[6] != G[19]; put(rt, G[7] - 0x4u, G[5]); if (tk) goto L_0893735C; } G[2] = word(rt, G[6] + 0x8u); G[3] = word(rt, G[6] + 0x0u); G[4] = word(rt, G[6] + 0x4u); G[8] = G[21]; put(rt, G[7] + 0x8u, G[2]); G[6] = G[29]; put(rt, G[7] + 0x0u, G[3]); put(rt, G[7] + 0x4u, G[4]); L_089373A8: ; G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[6] + 0x4u); G[4] = word(rt, G[6] + 0x8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[8] + 0x0u, G[2]); G[6] = G[6] + 0x10u; G[8] = G[8] + 0x10u; put(rt, G[8] - 0xCu, G[3]); put(rt, G[8] - 0x8u, G[4]); { const bool tk = G[6] != G[18]; put(rt, G[8] - 0x4u, G[5]); if (tk) goto L_089373A8; } G[2] = word(rt, G[6] + 0x8u); G[4] = word(rt, G[6] + 0x4u); G[3] = word(rt, G[6] + 0x0u); put(rt, G[8] + 0x8u, G[2]); G[7] = 0u; put(rt, G[8] + 0x0u, G[3]); put(rt, G[8] + 0x4u, G[4]); G[5] = word(rt, frame.at(0x54u)); G[2] = G[7] < G[5] ? 1u : 0u; { const bool tk = G[2] != 0u; G[4] = word(rt, frame.at(0x50u)); if (tk) goto L_08937414; } { const bool tk = G[7] != G[5]; G[3] = high_half(0x08CDDE30u); if (tk) goto L_0893741C; } G[2] = G[20] < G[4] ? 1u : 0u; if (G[2] == 0u) { G[17] = G[17] + 0x4u; goto L_08937420; } L_08937414: ; G[20] = G[4]; G[3] = high_half(0x08CDDE30u); L_0893741C: ; G[17] = G[17] + 0x4u; L_08937420: ; G[2] = G[3] + sext(0x08CDDE30u, 16u); { const bool tk = G[17] != G[2]; G[5] = G[23]; if (tk) goto L_08937320; } G[2] = G[20]; G[31] = word(rt, frame.at(0x1E4u)); G[30] = word(rt, frame.at(0x1E0u)); G[23] = word(rt, frame.at(0x1DCu)); G[22] = word(rt, frame.at(0x1D8u)); G[21] = word(rt, frame.at(0x1D4u)); G[20] = word(rt, frame.at(0x1D0u)); G[19] = word(rt, frame.at(0x1CCu)); G[18] = word(rt, frame.at(0x1C8u)); G[17] = word(rt, frame.at(0x1C4u)); G[16] = word(rt, frame.at(0x1C0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x0896FC0C (a0 = the object, a1, a2 = two arguments): needs bit 4 of the object's flags (+0x60), else v0 = 0. A temporary (frame +0x1C, made by 0x08A76AA0) is
// filled by 0x08A7BFD0(object, temporary, frame +0x18) after 0x08A69B3C(frame +0x18, 0x08CA2B3C). When it holds nothing the result is 0. Otherwise
// two more temporaries, an array of three (+0x24 to +0x30) and the arguments stored on the frame (0x08A76AAC(slot, &value): +0x14 = a1, +0xC = a2,
// +4 = the object; a slot that is not the matching array element gets 0x08A5BAD8(element, value)), are made, and the temporary's virtual +0xCC
// is called with (temporary, second, 3, array). When the second temporary holds something its virtual +0x3C is called and v0 = its result,
// else 0. Every temporary is released (0x08A5B9EC) on the way out, the array last to first.
void func_0896FC0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], first = ctx.gpr[5], second_argument = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x44u}, {19u, 0x3Cu}, {18u, 0x38u}, {17u, 0x34u}, {31u, 0x50u}, {23u, 0x4Cu}, {22u, 0x48u}, {20u, 0x40u}, {16u, 0x30u}});
    ctx.gpr[21] = second_argument;
    ctx.gpr[19] = first;
    ctx.gpr[18] = self;
    ctx.gpr[17] = 0u;
    if ((word(rt, self + 0x60u) & 0x4u) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 temp = frame.at(0x1Cu), name = frame.at(0x18u), held = frame.at(0x20u), array = frame.at(0x24u), array_end = frame.at(0x30u);
    ctx.gpr[23] = temp;
    call_set(rt, ctx, 0x08A76AA0u, {temp});
    ctx.gpr[16] = name;
    call_set(rt, ctx, 0x08A69B3Cu, {name, 0x08CA2B3Cu});
    call_set(rt, ctx, 0x08A7BFD0u, {self, temp, name});
    call_set(rt, ctx, 0x08A69D68u, {name});
    ctx.gpr[22] = held;
    if (word(rt, temp) == 0u) {
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
        ctx.gpr[17] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[20] = array;
    call_set(rt, ctx, 0x08A76AA0u, {held});
    ctx.gpr[17] = array_end;
    for (u32 element = array; element != array_end; element += 4u) {
        ctx.gpr[16] = element + 4u;
        call_set(rt, ctx, 0x08A76AA0u, {element});
    }
    // one argument stored on the frame: `slot` receives it (0x08A76AAC(slot, &value)); a temporary array element is cleared (0x08A5BAD8) when it is not the slot
    auto store_argument = [&](u32 slot, u32 value_slot, u32 value, u32 element) {
        put(rt, value_slot, value);
        ctx.gpr[16] = slot;
        call_set(rt, ctx, 0x08A76AACu, {slot, value_slot});
        if (element != slot) call_set(rt, ctx, 0x08A5BAD8u, {element, word(rt, slot)});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
    };
    store_argument(frame.at(0x14u), frame.at(0x10u), first, array);
    store_argument(frame.at(0xCu), frame.at(0x8u), second_argument, frame.at(0x28u));
    store_argument(frame.at(0x4u), frame.at(0x0u), self, frame.at(0x2Cu));
    const u32 object = word(rt, temp);
    ctx.gpr[16] = object;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA2878u});
    ctx.gpr[3] = word(rt, word(rt, object) + 0xCCu);
    call_set(rt, ctx, ctx.gpr[3], {object, held, 3u, array});
    ctx.gpr[16] = array_end;
    const u32 inner = word(rt, held);
    u32 result = 0u;
    bool keep = false;
    if (inner != 0u) {
        keep = true;
        result = call_set(rt, ctx, word(rt, word(rt, inner) + 0x3Cu), {inner});
        ctx.gpr[17] = result;
    }
    // the three array elements, last to first
    for (u32 element = array_end - 4u;; element -= 4u) {
        ctx.gpr[16] = element - 4u;
        call_set(rt, ctx, 0x08A5B9ECu, {element});
        if (element == array) break;
    }
    call_set(rt, ctx, 0x08A5B9ECu, {held});
    if (!keep) ctx.gpr[17] = 0u;
    call_set(rt, ctx, 0x08A5B9ECu, {temp});
    ctx.gpr[2] = keep ? result : 0u;
}
void func_08987F4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x38u), G[22]); G[22] = G[4]; G[4] = frame.at(0x8u); put(rt, frame.at(0x34u), G[21]); G[21] = G[5]; G[5] = G[22] + 0x4u; put(rt, frame.at(0x3Cu), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); call_here(rt, ctx, 0x08B88F44u); G[5] = high_half(0x08CA3960u); G[5] = 0x08CA3960u; G[4] = frame.at(0x4u); G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B85EB8u); G[17] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x4u)); G[2] = frame.at(0xCu); G[7] = word(rt, G[17] - 0xCu); G[8] = frame.at(0x10u); G[4] = G[17]; put(rt, frame.at(0xCu), G[7]); G[5] = G[16]; G[19] = G[16] - 0xCu; G[3] = word(rt, G[16] - 0xCu); G[6] = G[3] < G[7] ? 1u : 0u; put(rt, frame.at(0x10u), G[3]); if (G[6] == 0u) G[8] = G[2]; G[18] = G[7] - G[3]; G[6] = word(rt, G[8] + 0x0u); call_here(rt, ctx, 0x08B36170u); G[3] = high_half(0x08D74FD0u); G[20] = 0x08D74FD0u; { const bool tk = G[19] == G[20]; if (G[2] != 0u) G[18] = G[2]; if (tk) goto L_0898800C; } G[4] = G[16] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = word(rt, frame.at(0x8u)); goto L_0898800C; } G[4] = G[19]; G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); G[17] = word(rt, frame.at(0x8u)); L_0898800C: ; G[16] = G[17] - 0xCu; { const bool tk = G[20] == G[16]; G[4] = G[17] - 0x4u; if (tk) goto L_08988030; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (tk) goto L_08988030; } G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); L_08988030: ; { const bool tk = G[18] != 0u; G[16] = frame.at(0x14u); if (tk) goto L_089880B0; } G[2] = G[20] + 0xCu; G[4] = G[21]; G[5] = G[16]; put(rt, frame.at(0x14u), G[2]); call_here(rt, ctx, 0x08987A74u); G[4] = G[21]; G[5] = G[16]; call_here(rt, ctx, 0x08987A74u); G[3] = 0x101u; { const bool tk = G[2] != G[3]; G[4] = G[21]; if (tk) goto L_08988078; } G[4] = word(rt, frame.at(0x14u)); G[16] = word(rt, G[22] + 0xCu); call_here(rt, ctx, 0x08985D38u); store8(G[16] + 0x0u, G[2]); G[4] = G[21]; L_08988078: ; G[5] = 0x1u; call_here(rt, ctx, 0x0898682Cu); G[4] = word(rt, frame.at(0x14u)); G[16] = G[4] - 0xCu; { const bool tk = G[20] == G[16]; G[31] = word(rt, frame.at(0x3Cu)); if (tk) goto L_089880B4; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[31] = word(rt, frame.at(0x3Cu)); if (tk) goto L_089880B4; } G[4] = G[16]; G[5] = G[29]; call_here(rt, ctx, 0x08B8542Cu); L_089880B0: ; G[31] = word(rt, frame.at(0x3Cu)); L_089880B4: ; G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08999CB4 (a0 = the owner, a1 = a node to skip): clears the owner's five slots (+0x124, +0x128, +0x12C, +0x134, +0x130) and walks its node list (first
// at +0xF4, next by 0x08968A38) assigning nodes to them. The nodes' key (the word at 0x08A7BCC0(node)) is compared with four names on the frame
// (+4, +8, +0xC, +0x10, made by 0x08A69B3C from 0x08CA45A4, 0x08CA45B0, 0x08CA45B8, 0x08CA45C4): the first node matching +4 takes +0x128 (when empty; the text
// that 0x08A7BFD0 makes from it, if any, goes to +0x12C), the others +0x12C, +0x134, +0x130 in turn, each only while its slot is empty; a node left
// over takes +0x124 when 0x08A76F20(node, 0x08994F90()) accepts it and that slot is empty. When +0x124 ends up set, 0x08A0EE4C(frame +0x14) builds
// a value that 0x089952F4(+0x124 node, value) and 0x08999898(owner, value) take. The four names are released (0x08A69D68) last to first.
void func_08999CB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], skip = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x40u}, {19u, 0x2Cu}, {18u, 0x28u}, {31u, 0x44u}, {23u, 0x3Cu}, {21u, 0x34u}, {20u, 0x30u}, {17u, 0x24u}, {22u, 0x38u}, {16u, 0x20u}});
    const u32 name_a = frame.at(0x4u), name_b = frame.at(0x8u), name_c = frame.at(0xCu), name_d = frame.at(0x10u), temp = frame.at(0x14u);
    ctx.gpr[30] = name_a;
    ctx.gpr[19] = skip;
    ctx.gpr[18] = self;
    ctx.gpr[23] = name_b;
    ctx.gpr[21] = name_c;
    ctx.gpr[20] = name_d;
    put(rt, self + 0x124u, 0u);
    put(rt, self + 0x128u, 0u);
    put(rt, self + 0x12Cu, 0u);
    put(rt, self + 0x134u, 0u);
    put(rt, self + 0x130u, 0u);
    call_set(rt, ctx, 0x08A69B3Cu, {name_a, 0x08CA45A4u});
    call_set(rt, ctx, 0x08A69B3Cu, {name_b, 0x08CA45B0u});
    call_set(rt, ctx, 0x08A69B3Cu, {name_c, 0x08CA45B8u});
    call_set(rt, ctx, 0x08A69B3Cu, {name_d, 0x08CA45C4u});
    // does the node's key equal the name on the frame?
    auto key_is = [&](u32 node, u32 name) {
        const u32 key = word(rt, call_set(rt, ctx, 0x08A7BCC0u, {node}));
        return key == word(rt, name);
    };
    u32 node = word(rt, self + 0xF4u);
    ctx.gpr[17] = node;
    ctx.gpr[22] = temp;
    while (node != 0u) {
        ctx.gpr[17] = node;
        auto step = [&] {
            if (node == skip) return;
            if (word(rt, self + 0x128u) == 0u && key_is(node, name_a)) {
                put(rt, self + 0x128u, node);
                put(rt, self + 0x12Cu, 0u);
                call_set(rt, ctx, 0x08969160u, {temp});
                const u32 first = word(rt, self + 0x128u);
                ctx.gpr[16] = first;
                call_set(rt, ctx, 0x08A69B3Cu, {frame.at(0x0u), 0x08CA45B0u});
                call_set(rt, ctx, 0x08A7BFD0u, {first, temp, frame.at(0x0u)});
                call_set(rt, ctx, 0x08A69D68u, {frame.at(0x0u)});
                const u32 text = word(rt, temp);
                if (text != 0u) put(rt, self + 0x12Cu, text);
                call_set(rt, ctx, 0x08A5B9ECu, {temp});
                return;
            }
            if (word(rt, self + 0x12Cu) == 0u && key_is(node, name_b)) {
                put(rt, self + 0x12Cu, node);
                return;
            }
            if (word(rt, self + 0x134u) == 0u && key_is(node, name_c)) {
                put(rt, self + 0x134u, node);
                return;
            }
            if (word(rt, self + 0x130u) == 0u && key_is(node, name_d)) {
                put(rt, self + 0x130u, node);
                return;
            }
            if (word(rt, self + 0x124u) != 0u) return;
            const u32 accepted = call_set(rt, ctx, 0x08A76F20u, {node, call_set(rt, ctx, 0x08994F90u, {})});
            if (accepted != 0u) put(rt, self + 0x124u, node);
        };
        step();
        node = call_set(rt, ctx, 0x08968A38u, {node});
        ctx.gpr[17] = node;
    }
    ctx.gpr[16] = temp;
    if (word(rt, self + 0x124u) != 0u) {
        call_set(rt, ctx, 0x08A0EE4Cu, {temp});
        call_set(rt, ctx, 0x089952F4u, {word(rt, self + 0x124u), word(rt, temp)});
        call_set(rt, ctx, 0x08999898u, {self, word(rt, temp)});
        call_set(rt, ctx, 0x08A5B9ECu, {temp});
    }
    call_set(rt, ctx, 0x08A69D68u, {name_d});
    call_set(rt, ctx, 0x08A69D68u, {name_c});
    call_set(rt, ctx, 0x08A69D68u, {name_b});
    call_set(rt, ctx, 0x08A69D68u, {name_a});
}
// 0x089B1D10 (a1 = the destination, a2 = the count, a3 = the source object; nothing is done and v0 = 1 below 2): builds a vector of string values on the
// frame (+0x20 begin, +0x24 end, +0x28 capacity end) from the source's list (the object's +4, a null one calls 0x08A5BB7C(0x08CA50BC)), after a copy of the
// source (0x089A354C into +0x1C). When its virtual +0x78 says it has a count, the vector is resized to the virtual +0x7C count (erase 0x08BF9B6C or fill
// 0x08BFB68C(vector, end, n - size, &zero)) and each element i is the source's virtual +0x84(&temp, list, i), copied by 0x089A354C and stored in the vector. Otherwise the vector
// has count - 1 elements, each a copy (0x089A354C) of the source's own values in turn. 0x089AB370(frame +0xC, destination) then gives the object
// that 0x089B1CC8(object, copy, &vector) takes; the vector is destroyed (0x08BF8B48) and its storage freed (0x08A5BC54(begin, bytes, 4)).
void func_089B1D10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[5], count = ctx.gpr[6], source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x50u}, {21u, 0x44u}, {17u, 0x34u}, {31u, 0x54u}, {23u, 0x4Cu}, {22u, 0x48u}, {20u, 0x40u}, {19u, 0x3Cu}, {18u, 0x38u}, {16u, 0x30u}});
    ctx.gpr[2] = static_cast<s32>(count) < 2 ? 1u : 0u;
    ctx.gpr[30] = destination;
    ctx.gpr[21] = count;
    ctx.gpr[17] = source;
    if (static_cast<s32>(count) < 2) return;
    const u32 copy = frame.at(0x1Cu), begin_slot = frame.at(0x20u), end_slot = frame.at(0x24u), made = frame.at(0xCu);
    call_set(rt, ctx, 0x089A354Cu, {copy, source});
    const u32 list = source + 0x4u;
    ctx.gpr[23] = list;
    put(rt, begin_slot, 0u);
    put(rt, end_slot, 0u);
    put(rt, frame.at(0x28u), 0u);
    // the source's list object; null calls the assert
    auto list_object = [&](u32 where) {
        const u32 object = word(rt, where);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
        return object;
    };
    // resizes the vector to `size` elements; `zero` is the frame word whose address is the fill value
    auto resize = [&](u32 size, u32 zero) {
        const u32 begin = word(rt, begin_slot), end = word(rt, end_slot);
        const u32 now = sra(end - begin, 2u);
        put(rt, zero, 0u);
        if (size < now) {
            call_set(rt, ctx, 0x08BF9B6Cu, {begin_slot, begin + (size << 2u), end});
        } else {
            call_set(rt, ctx, 0x08BFB68Cu, {begin_slot, end, size - now, zero});
        }
    };
    u32 object = list_object(list);
    if (call_set(rt, ctx, word(rt, word(rt, object) + 0x78u), {object}) != 0u) {
        object = word(rt, source + 0x4u);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50BCu});
        const u32 n = call_set(rt, ctx, word(rt, word(rt, object) + 0x7Cu), {object});
        ctx.gpr[20] = n;
        resize(n, frame.at(0x8u));
        const u32 temp = frame.at(0x14u), result = frame.at(0x18u);
        u32 i = 0u;
        ctx.gpr[17] = made;
        if (n != 0u) {
            ctx.gpr[21] = result;
            ctx.gpr[19] = temp;
            ctx.gpr[17] = 0u;
            ctx.gpr[22] = 0x004A0000u;
            do {
                const u32 slot = word(rt, begin_slot) + (i << 2u);
                ctx.gpr[18] = slot;
                const u32 element = list_object(list);
                ctx.gpr[17] = ++i;
                call_set(rt, ctx, word(rt, word(rt, element) + 0x84u), {temp, element, i - 1u});
                call_set(rt, ctx, 0x089A354Cu, {result, temp});
                put(rt, slot, word(rt, result));
                call_set(rt, ctx, 0x08A5B9ECu, {result});
                call_set(rt, ctx, 0x08A5B9ECu, {temp});
            } while (n != i);
            ctx.gpr[17] = made;
        }
    } else {
        resize(count - 1u, frame.at(0x4u));
        u32 from = list;
        ctx.gpr[17] = from;
        const u32 temp = frame.at(0x10u);
        ctx.gpr[20] = temp;
        ctx.gpr[19] = 1u;
        ctx.gpr[18] = 0u;
        for (u32 j = 1u, offset = 0u;; ++j, offset += 4u, from += 4u) {
            const u32 base = word(rt, begin_slot);
            ctx.gpr[16] = base;
            call_set(rt, ctx, 0x089A354Cu, {temp, from});
            put(rt, base + offset, word(rt, temp));
            call_set(rt, ctx, 0x08A5B9ECu, {temp});
            if (j + 1u == count) break;
            ctx.gpr[17] = from + 4u;
        }
        ctx.gpr[17] = made;
    }
    call_set(rt, ctx, 0x089AB370u, {made, destination});
    const u32 holder = word(rt, made);
    ctx.gpr[16] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA50C4u});
    call_set(rt, ctx, 0x089B1CC8u, {holder, word(rt, copy), begin_slot});
    call_set(rt, ctx, 0x08A5B9ECu, {made});
    call_set(rt, ctx, 0x08BF8B48u, {word(rt, begin_slot), word(rt, end_slot), frame.at(0x0u)});
    const u32 begin = word(rt, begin_slot);
    const u32 capacity = sra(word(rt, frame.at(0x28u)) - begin, 2u);
    if (begin != 0u) call_set(rt, ctx, 0x08A5BC54u, {begin, capacity << 2u, 4u});
    call_set(rt, ctx, 0x08A5B9ECu, {copy});
}
void func_089C8B14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0xB0u); G[2] = high_half(0x08D74FD0u); put(rt, frame.at(0x98u), G[18]); G[18] = 0x08D74FD0u; put(rt, frame.at(0x9Cu), G[19]); G[19] = G[18] + 0xCu; put(rt, frame.at(0x94u), G[17]); G[17] = G[4]; put(rt, frame.at(0x90u), G[16]); G[16] = frame.at(0x50u); G[4] = G[16]; put(rt, frame.at(0xA4u), G[31]); put(rt, frame.at(0xA0u), G[20]); put(rt, frame.at(0x8u), G[19]); G[5] = word(rt, G[6] + 0x0u); G[6] = 0x1u; call_here(rt, ctx, 0x08B10204u); G[6] = frame.at(0x10u); G[7] = frame.at(0x80u); L_089C8B60: ; G[2] = word(rt, G[16] + 0x0u); G[3] = word(rt, G[16] + 0x4u); G[4] = word(rt, G[16] + 0x8u); G[5] = word(rt, G[16] + 0xCu); put(rt, G[6] + 0x0u, G[2]); G[16] = G[16] + 0x10u; G[6] = G[6] + 0x10u; put(rt, G[6] - 0xCu, G[3]); put(rt, G[6] - 0x8u, G[4]); { const bool tk = G[16] != G[7]; put(rt, G[6] - 0x4u, G[5]); if (tk) goto L_089C8B60; } G[2] = word(rt, G[16] + 0x8u); G[3] = word(rt, G[16] + 0x0u); G[4] = word(rt, G[16] + 0x4u); put(rt, G[6] + 0x8u, G[2]); put(rt, G[6] + 0x0u, G[3]); put(rt, G[6] + 0x4u, G[4]); G[2] = word(rt, frame.at(0x10u)); if (G[2] == 0u) { G[19] = word(rt, frame.at(0x30u)); goto L_089C8BB8; } put(rt, G[17] + 0x0u, G[19]); goto L_089C8C1C; L_089C8BB8: ; G[6] = word(rt, frame.at(0x20u)); G[16] = frame.at(0x4u); G[4] = G[16]; G[5] = G[19]; G[7] = frame.at(0x2u); G[20] = frame.at(0x8u); call_here(rt, ctx, 0x08C00AD4u); G[5] = G[16]; G[4] = G[20]; call_here(rt, ctx, 0x08B855E0u); G[4] = word(rt, frame.at(0x4u)); G[16] = G[4] - 0xCu; { const bool tk = G[18] == G[16]; G[4] = G[4] - 0x4u; if (tk) goto L_089C8C08; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (tk) goto L_089C8C08; } G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); L_089C8C08: ; G[4] = G[19]; call_here(rt, ctx, 0x08B35254u); G[4] = G[17]; G[5] = G[20]; call_here(rt, ctx, 0x08B88F44u); L_089C8C1C: ; G[4] = word(rt, frame.at(0x8u)); G[16] = G[4] - 0xCu; { const bool tk = G[18] == G[16]; G[2] = G[17]; if (tk) goto L_089C8C50; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[2] = G[17]; if (tk) goto L_089C8C50; } G[4] = G[16]; G[5] = G[29]; call_here(rt, ctx, 0x08B8542Cu); G[2] = G[17]; L_089C8C50: ; G[31] = word(rt, frame.at(0xA4u)); G[20] = word(rt, frame.at(0xA0u)); G[19] = word(rt, frame.at(0x9Cu)); G[18] = word(rt, frame.at(0x98u)); G[17] = word(rt, frame.at(0x94u)); G[16] = word(rt, frame.at(0x90u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x089D84E8 (a0 = 1 and a1 = 0xFFFF): registers the class at 0x08CF5280 through 0x08A92114(0x08CF5280, 0x08CA71C8, 0xA8, 0x089D8388, 0x089D95C4, 0x089D83AC), the one at
// 0x08CF5270 through 0x0897D534(0x08CF5270, 0x08CA71C8, 0x089D9060, 0x089D8EF8), then seven members through 0x08C045A0(destination, name, record, 0x08CF5270), each record
// being a two-word copy of a pair in 0x08C8D198..0x08C8D1C8 made in the frame. Any other a0 leaves v0 = 1, any other a1 leaves v0 = 0xFFFF; v0 is the last call's otherwise.
void func_089D84E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 phase = ctx.gpr[4];
    const u32 magic = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x48u}, {17u, 0x44u}, {16u, 0x40u}});
    ctx.gpr[2] = 1u;
    if (phase != 1u) return;
    ctx.gpr[2] = 0xFFFFu;
    if (magic != 0xFFFFu) return;
    call_set(rt, ctx, 0x08A92114u, {0x08CF5280u, 0x08CA71C8u, 0xA8u, 0x089D8388u, 0x089D95C4u, 0x089D83ACu});
    call_set(rt, ctx, 0x0897D534u, {0x08CF5270u, 0x08CA71C8u, 0x089D9060u, 0x089D8EF8u});
    struct Member {
        u32 destination;
        u32 name;
        u32 pair;
        u32 record_offset;
    };
    static constexpr Member members[] = {
        {0x08CF52FCu, 0x08CA71D8u, 0x08C8D1C8u, 0x30u}, {0x08CF52ECu, 0x08CA71E8u, 0x08C8D1C0u, 0x28u},
        {0x08CF52DCu, 0x08CA71FCu, 0x08C8D1B8u, 0x20u}, {0x08CF52CCu, 0x08CA7208u, 0x08C8D1B0u, 0x18u},
        {0x08CF52BCu, 0x08CA7214u, 0x08C8D1A8u, 0x10u}, {0x08CF52ACu, 0x08CA7220u, 0x08C8D1A0u, 0x8u},
        {0x08CF529Cu, 0x08CA7228u, 0x08C8D198u, 0x0u},
    };
    for (const Member &member : members) {
        const u32 record = frame.at(member.record_offset);
        put(rt, record, word(rt, member.pair));
        put(rt, record + 4u, word(rt, member.pair + 4u));
        call_set(rt, ctx, 0x08C045A0u, {member.destination, member.name, record, 0x08CF5270u});
    }
}
void func_089F3F30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); G[2] = high_half(0x08CC8288u); G[2] = 0x08CC8288u; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, G[4] + 0x0u, G[2]); G[2] = high_half(0x08CC84C4u); G[2] = 0x08CC84C4u; put(rt, G[16] + 0x38u, G[2]); G[4] = G[4] + 0x268u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x264u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x260u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x25Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x258u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x254u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x250u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x24Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, G[16] + 0x248u); G[2] = high_half(0x08D74FD0u); G[18] = 0x08D74FD0u; G[17] = G[4] - 0xCu; if (G[17] == G[18]) { G[4] = word(rt, G[16] + 0x244u); goto L_089F3FDC; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[16] + 0x244u); goto L_089F3FDC; } G[4] = G[17]; G[5] = frame.at(0x7u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, G[16] + 0x244u); L_089F3FDC: ; G[17] = G[4] - 0xCu; if (G[18] == G[17]) { G[4] = word(rt, G[16] + 0x240u); goto L_089F400C; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[16] + 0x240u); goto L_089F400C; } G[4] = G[17]; G[5] = frame.at(0x6u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, G[16] + 0x240u); L_089F400C: ; G[17] = G[4] - 0xCu; if (G[18] == G[17]) { G[4] = word(rt, G[16] + 0x23Cu); goto L_089F403C; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[16] + 0x23Cu); goto L_089F403C; } G[4] = G[17]; G[5] = frame.at(0x5u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, G[16] + 0x23Cu); L_089F403C: ; G[17] = G[4] - 0xCu; if (G[18] == G[17]) { G[4] = word(rt, G[16] + 0x238u); goto L_089F406C; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[16] + 0x238u); goto L_089F406C; } G[4] = G[17]; G[5] = frame.at(0x4u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, G[16] + 0x238u); L_089F406C: ; G[17] = G[4] - 0xCu; if (G[18] == G[17]) { G[4] = word(rt, G[16] + 0x234u); goto L_089F409C; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[16] + 0x234u); goto L_089F409C; } G[4] = G[17]; G[5] = frame.at(0x3u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, G[16] + 0x234u); L_089F409C: ; G[17] = G[4] - 0xCu; if (G[18] == G[17]) { G[4] = word(rt, G[16] + 0x230u); goto L_089F40CC; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[16] + 0x230u); goto L_089F40CC; } G[4] = G[17]; G[5] = frame.at(0x2u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, G[16] + 0x230u); L_089F40CC: ; G[17] = G[4] - 0xCu; if (G[18] == G[17]) { G[4] = word(rt, G[16] + 0x22Cu); goto L_089F40FC; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[16] + 0x22Cu); goto L_089F40FC; } G[4] = G[17]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = word(rt, G[16] + 0x22Cu); L_089F40FC: ; G[17] = G[4] - 0xCu; { const bool tk = G[18] == G[17]; G[4] = G[4] - 0x4u; if (tk) goto L_089F4120; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[17]; if (tk) goto L_089F4120; } G[5] = G[29]; call_here(rt, ctx, 0x08B8542Cu); L_089F4120: ; G[4] = G[16]; call_here(rt, ctx, 0x089E2430u); G[31] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A29C9C (a0 = the destination string, a1 = a count, a2 = a pair of streams; nothing is done below 2): reads a flag word (s3) and a value (t7) through the
// virtual +0x3C of the objects at +0 and +4 of the pair. A negative flag formats the fixed text 0x08CABB40 (0x08B85EB8). Otherwise the value's magnitude
// (limited to 59999999 as 5999999 and a last digit 9) is written as decimal digits into a template copied from 0x08CABB4C to frame +0x14..+0x23: the last
// three digits at +0x22, +0x21, +0x20, then two more at +0x1E and +0x1D (the latter modulo 6, so a clock-like text), the rest of the quotient by 6 going
// down from +0x1B, and with bit 1 of the flag a '-' or '+' ahead of it. The text from there is made into a string (0x08B85EB8(string, text, &pad)) and
// assigned to the destination (0x08A6D2D4, then 0x08A5BAD8(destination, text) unless it is that string); both strings are released (0x08A5B9EC and the count drop
// 0x08B60804 / free 0x08B8542C of a header that is not the empty one 0x08D74FD0).
void func_08A29C9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], count = ctx.gpr[5], pair = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x38u}, {17u, 0x34u}, {31u, 0x40u}, {19u, 0x3Cu}, {16u, 0x30u}});
    ctx.gpr[5] = static_cast<s32>(count) < 2 ? 1u : 0u;
    ctx.gpr[18] = destination;
    ctx.gpr[17] = pair;
    if (static_cast<s32>(count) < 2) return;
    // the stream at `where`: null calls the assert; its virtual +0x3C gives a word
    auto read = [&](u32 where) {
        const u32 object = word(rt, where);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAB9D8u});
        return call_set(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    };
    const u32 flags = read(pair + 0x0u);
    ctx.gpr[19] = flags;
    const u32 value = read(pair + 0x4u);
    ctx.gpr[15] = value;
    const u32 empty = 0x08D74FD0u;
    const u32 template_text = 0x08CABB4Cu;
    ctx.gpr[2] = template_text;
    const u32 w0 = word(rt, template_text), w4 = word(rt, template_text + 0x4u), w8 = word(rt, template_text + 0x8u), wc = word(rt, template_text + 0xCu);
    put(rt, frame.at(0x14u), w0);
    put(rt, frame.at(0x18u), w4);
    put(rt, frame.at(0x1Cu), w8);
    put(rt, frame.at(0x20u), wc);
    // makes a string from the text (0x08B85EB8), assigns it (0x08A6D2D4), and releases the two strings
    auto make_string = [&](u32 made, u32 held, u32 text, u32 pad, u32 free_pad) {
        ctx.gpr[16] = made;
        ctx.gpr[17] = held;
        call_set(rt, ctx, 0x08B85EB8u, {made, text, pad});
        call_set(rt, ctx, 0x08A6D2D4u, {held, made});
        if (destination != held) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, held)});
        call_set(rt, ctx, 0x08A5B9ECu, {held});
        const u32 data = word(rt, made);
        const u32 header = data - 0xCu;
        ctx.gpr[16] = header;
        if (header == empty) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, free_pad});
    };
    if (static_cast<s32>(flags) < 0) {
        make_string(frame.at(0x4u), frame.at(0x8u), 0x08CABB40u, frame.at(0x2u), frame.at(0x0u));
        return;
    }
    auto mult = [&](u32 a, u32 b) {
        const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(a)) * static_cast<s32>(b);
        ctx.lo = static_cast<u32>(product);
        ctx.hi = static_cast<u32>(static_cast<std::uint64_t>(product) >> 32u);
        return ctx.hi;
    };
    const u32 negated = 0u - value;
    const u32 magnitude = static_cast<s32>(value) > static_cast<s32>(negated) ? value : negated;
    // x / 10 by the multiply by 0x66666667 (the multiply leaves its product in lo / hi)
    auto tenth = [&](u32 x) { return sra(mult(x, 0x66666667u), 2u) - sra(x, 31u); };
    u32 head, units;
    if (static_cast<s32>(0x039386FFu) < static_cast<s32>(magnitude)) {
        head = 0x5B8D7Fu;
        units = 9u;
    } else {
        head = tenth(magnitude);
        units = magnitude - head * 10u;
    }
    store8(frame.at(0x22u), units + 0x30u);
    const u32 q2 = tenth(head);
    const u32 tens = head - q2 * 10u + 0x30u;
    const u32 q3 = tenth(q2);
    const u32 hundreds = q2 - q3 * 10u + 0x30u;
    const u32 q4 = tenth(q3);
    const u32 thousands = q3 - q4 * 10u + 0x30u;
    const u32 q5 = mult(q4, 0x2AAAAAABu) - sra(q4, 31u);
    const u32 sixes = q4 - q5 * 6u + 0x30u;
    store8(frame.at(0x21u), tens);
    store8(frame.at(0x20u), hundreds);
    store8(frame.at(0x1Eu), thousands);
    store8(frame.at(0x1Du), sixes);
    // the rest of the quotient, a digit at a time, going down from +0x1B
    u32 digit_at = frame.at(0x1Bu), rest = q5, before;
    for (;;) {
        before = digit_at - 1u;
        const u32 next = tenth(rest);
        store8(digit_at, rest - next * 10u + 0x30u);
        if (next == 0u) break;
        rest = next;
        digit_at = before;
    }
    u32 start = digit_at;
    if ((flags & 0x1u) != 0u) {
        store8(before, static_cast<s32>(value) < 0 ? 0x2Du : 0x2Bu);
        start = before;
    }
    make_string(frame.at(0xCu), frame.at(0x10u), start, frame.at(0x3u), frame.at(0x1u));
}
void func_08A39E64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); G[23] = 0x100u; put(rt, frame.at(0x18u), G[22]); G[22] = 0xAu; put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); G[20] = G[4]; G[4] = G[5]; put(rt, frame.at(0xCu), G[19]); G[19] = G[5]; G[21] = G[20] + 0x124u; put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); call_here(rt, ctx, 0x08B35F60u); G[18] = G[2]; G[2] = word(rt, G[20] + 0x20u); L_08A39EB0: ; G[4] = 0x4E20u; G[17] = G[23] - G[2]; { const bool tk = static_cast<std::int32_t>(G[17]) <= 0; G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[18]) ? 1u : 0u; if (tk) goto L_08A39F68; } { const bool tk = G[2] == 0u; G[4] = G[17]; if (tk) goto L_08A39F18; } G[18] = G[18] - G[17]; call_here(rt, ctx, 0x08B35DA0u); G[4] = G[2]; G[5] = G[19]; G[6] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08B7C830u); G[4] = G[21]; call_here(rt, ctx, 0x08B344E0u); G[5] = G[16]; G[6] = G[17]; G[4] = G[20] + 0x14u; call_here(rt, ctx, 0x08C1B60Cu); G[4] = G[21]; call_here(rt, ctx, 0x08B3457Cu); G[4] = G[16]; call_here(rt, ctx, 0x08B35254u); G[4] = 0x4E20u; G[19] = G[19] + G[17]; goto L_08A39F68; L_08A39F18: ; G[4] = G[21]; call_here(rt, ctx, 0x08B344E0u); G[5] = G[19]; G[6] = G[18]; G[4] = G[20] + 0x14u; call_here(rt, ctx, 0x08C1B60Cu); G[4] = G[21]; call_here(rt, ctx, 0x08B3457Cu); G[31] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[4] = 0x4E20u; tail = 0x08B2E2B8u; goto exit_; L_08A39F68: ; G[22] = G[22] - 0x1u; call_here(rt, ctx, 0x08B2E2B8u); if (G[22] != 0u) { G[2] = word(rt, G[20] + 0x20u); goto L_08A39EB0; } G[31] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A4C9A4 (a0 = the destination string, a1 = a count, a2 = a pair of streams): a state object on the frame (+0x14, 0x08ACCAB4) is fed from the pair (a null stream calls
// 0x08A5BB7C(0x08CAF090)): with count 1 the first stream's virtual +0x3C word goes to 0x08ACC8C0(state, 0, word); with 2 or more the first stream's virtual +0xC
// gives a text (frame +0xC) whose first three letters set flag bits ('N' = 1, second 'Y' = 0x10, third 'N' = 0x100; each longer than its position; a shared text is unshared
// with 0x08B88FF8 first) and the second stream's gives another (+0x10) for 0x08ACC8D8(state, flags, text). Then 0x08ACC93C(state) is polled (-1: 0x08B2E2B8(0x3E8) and
// 0x08B1A844, a non-zero ends it with 0x08ACC624), and its answer picks a fixed text (1: 0x08CAF0F4, 0: 0x08CAF0EC, 2: 0x08CAF0F8, 3: 0x08CAF0DC, else 0x08CAF0E4) that the
// destination takes (0x08B85EB8 builds a string, 0x08A6D2D4 assigns, 0x08A5BAD8(destination, text) unless it is that string). The strings are released
// (0x08A5B9EC; the count drop 0x08B60804, free 0x08B8542C of a header that is not the empty one 0x08D74FD0) and the state is closed (0x08ACC6BC).
void func_08A4C9A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4], count = ctx.gpr[5], pair = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x28u}, {21u, 0x34u}, {19u, 0x2Cu}, {16u, 0x20u}, {31u, 0x38u}, {20u, 0x30u}, {17u, 0x24u}});
    const u32 state = frame.at(0x14u), text_a = frame.at(0xCu), text_b = frame.at(0x10u), empty = 0x08D74FD0u;
    ctx.gpr[18] = state;
    ctx.gpr[21] = destination;
    ctx.gpr[19] = pair;
    ctx.gpr[16] = count;
    call_set(rt, ctx, 0x08ACCAB4u, {state});
    // the stream at `where`; null calls the assert; v0 = its object
    auto stream = [&](u32 where) {
        const u32 object = word(rt, where);
        ctx.gpr[16] = object;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF090u});
        return object;
    };
    // releases the counted string at `slot` (a header that is not the empty one loses a count, and is freed when none is left)
    auto release = [&](u32 slot, u32 pad) {
        const u32 data = word(rt, slot);
        const u32 header = data - 0xCu;
        ctx.gpr[16] = header;
        if (header == empty) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, pad});
    };
    // a letter of the text at frame +0xC; a shared text (negative count at header +8) is unshared first
    auto letter = [&](u32 index) {
        u32 data = word(rt, text_a);
        if (static_cast<s32>(word(rt, data - 0x4u)) >= 0) {
            call_set(rt, ctx, 0x08B88FF8u, {text_a});
            data = word(rt, text_a);
        }
        return signed_byte(rt, data + index);
    };
    ctx.gpr[20] = 0x000A0000u;
    if (static_cast<s32>(count) >= 2) {
        const u32 first = stream(pair + 0x0u);
        ctx.gpr[20] = text_a;
        call_set(rt, ctx, word(rt, word(rt, first) + 0xCu), {text_a, first});
        u32 data = word(rt, text_a);
        const u32 length = word(rt, data - 0xCu);
        ctx.gpr[16] = length;
        u32 flags = 0u;
        if (static_cast<s32>(length) > 0) {
            u32 head;
            if (static_cast<s32>(word(rt, data - 0xCu + 0x8u)) < 0) {
                head = signed_byte(rt, data);
            } else {
                call_set(rt, ctx, 0x08B88FF8u, {text_a});
                data = word(rt, text_a);
                head = signed_byte(rt, data);
            }
            flags = head == 0x4Eu ? 1u : 0u;
        }
        ctx.gpr[17] = flags;
        if (!(static_cast<s32>(length) < 2)) {
            if (letter(1u) == 0x59u) flags |= 0x10u;
            ctx.gpr[17] = flags;
        }
        if (!(static_cast<s32>(length) < 3)) {
            if (letter(2u) == 0x4Eu) flags |= 0x100u;
            ctx.gpr[17] = flags;
        }
        const u32 second = stream(pair + 0x4u);
        ctx.gpr[20] = 0x000A0000u;
        ctx.gpr[19] = empty;
        call_set(rt, ctx, word(rt, word(rt, second) + 0xCu), {text_b, second});
        call_set(rt, ctx, 0x08ACC8D8u, {state, flags, word(rt, text_b)});
        release(text_b, frame.at(0x2u));
        release(text_a, frame.at(0x1u));
    } else if (count == 1u) {
        const u32 object = stream(pair + 0x0u);
        const u32 word_value = call_set(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
        call_set(rt, ctx, 0x08ACC8C0u, {state, 0u, word_value});
    }
    // wait for the state's answer
    for (;;) {
        if (call_set(rt, ctx, 0x08ACC93Cu, {state}) != 0xFFFFFFFFu) break;
        call_set(rt, ctx, 0x08B2E2B8u, {0x3E8u});
        if (call_set(rt, ctx, 0x08B1A844u, {}) != 0u) {
            call_set(rt, ctx, 0x08ACC624u, {state});
            break;
        }
    }
    const u32 answer = call_set(rt, ctx, 0x08ACC93Cu, {state});
    u32 fixed;
    switch (answer) {
    case 1u: fixed = 0x08CAF0F4u; break;
    case 0u: fixed = 0x08CAF0ECu; break;
    case 2u: fixed = 0x08CAF0F8u; break;
    case 3u: fixed = 0x08CAF0DCu; break;
    default: fixed = 0x08CAF0E4u; break;
    }
    const u32 made = frame.at(0x4u), held = frame.at(0x8u);
    ctx.gpr[16] = made;
    ctx.gpr[17] = held;
    call_set(rt, ctx, 0x08B85EB8u, {made, fixed, frame.at(0x3u)});
    call_set(rt, ctx, 0x08A6D2D4u, {held, made});
    if (destination != held) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, held)});
    call_set(rt, ctx, 0x08A5B9ECu, {held});
    release(made, frame.at(0x0u));
    call_set(rt, ctx, 0x08ACC6BCu, {state});
}
// 0x08A705F8 (a0 = the destination string slot, a1 = a pass-through argument, a2 = the mode, a3 = the pair object): only mode 3 does anything (v0 = 3 otherwise).
// 0x08A76ABC(frame + 8, pair) builds a temporary; the pair holds two objects (+4 and +8) whose virtual +0x3C gives a first and a second length; the temporary's virtual
// +0xD8 gives its size, and when the first length is below it the second length is cut to the size left (or left alone while the size covers the sum). With
// 0x08A71E70(frame + 4, argument) a result is built through the pair's virtual +0xDC(temporary, first length) and the +0x10C handler of the new object (object, that,
// second length); a size not above the first length gives 0. Every null object calls the error function 0x08A5BB7C (0x08CB08E4, or 0x08CB08FC for the new object)
// before it is read. At the end 0x08A85B1C(frame, result) fills the frame result, which goes through 0x08A5BAD8(destination, frame word) unless the destination is the
// frame itself, then both temporaries are 0x08A5B9EC'd. v0 = the last callee's.
void func_08A705F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 mode = ctx.gpr[6];
    const u32 pair = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {20u, 0x20u}, {17u, 0x14u}, {31u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {19u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}});
    ctx.gpr[2] = 3u;
    if (mode != 3u) return;
    // A pointer read after the error call that guards it (the call never returns in practice, but the read happens as in the original).
    const auto checked = [&](u32 pointer, u32 message) {
        if (pointer == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {message});
        return pointer;
    };
    const u32 temporary = frame.at(0x8u);
    call_set(rt, ctx, 0x08A76ABCu, {temporary, pair});
    const u32 first_object = checked(word(rt, pair + 0x4u), 0x08CB08E4u);
    const u32 first_length = call_indirect(rt, ctx, word(rt, word(rt, first_object) + 0x3Cu), {first_object});
    const u32 second_object = checked(word(rt, pair + 0x8u), 0x08CB08E4u);
    u32 second_length = call_indirect(rt, ctx, word(rt, word(rt, second_object) + 0x3Cu), {second_object});
    u32 result = 0u;
    u32 holder = checked(word(rt, frame.at(0x8u)), 0x08CB08E4u);
    u32 size = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xD8u), {holder});
    if (first_length < size) {
        const u32 sum = first_length + second_length;
        holder = checked(word(rt, frame.at(0x8u)), 0x08CB08E4u);
        size = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xD8u), {holder});
        if (size < sum) {
            holder = checked(word(rt, frame.at(0x8u)), 0x08CB08E4u);
            size = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xD8u), {holder});
            second_length = size - first_length;
        }
        const u32 built = frame.at(0x4u);
        call_set(rt, ctx, 0x08A71E70u, {built, argument});
        const u32 object = checked(word(rt, frame.at(0x4u)), 0x08CB08FCu);
        const u32 handler = word(rt, word(rt, object) + 0x10Cu);
        holder = checked(word(rt, frame.at(0x8u)), 0x08CB08E4u);
        const u32 piece = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xDCu), {holder, first_length});
        result = call_indirect(rt, ctx, handler, {object, piece, second_length});
        call_set(rt, ctx, 0x08A5B9ECu, {built});
    }
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0x0u), result});
    if (destination != frame.at(0x0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0x0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}

void func_08A7ED20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x3Cu), G[23]); G[23] = G[6]; put(rt, frame.at(0x2Cu), G[19]); G[19] = G[5]; put(rt, frame.at(0x28u), G[18]); G[18] = G[7]; put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x20u), G[16]); G[2] = word(rt, G[7] + 0x0u); { const bool tk = G[2] != G[7]; G[17] = G[4]; if (tk) goto L_08A7ED74; } G[5] = G[6]; call_here(rt, ctx, 0x08A7D1F8u); G[31] = word(rt, frame.at(0x44u)); goto L_08A7EF28; L_08A7ED74: ; G[22] = frame.at(0x8u); G[5] = G[2] + 0x8u; G[4] = G[22]; G[21] = frame.at(0xCu); call_here(rt, ctx, 0x08B88F44u); G[16] = frame.at(0x4u); G[4] = G[21]; call_here(rt, ctx, 0x08A5D438u); G[4] = G[16]; G[5] = G[22]; call_here(rt, ctx, 0x08A69654u); G[4] = G[17]; G[5] = G[21]; G[6] = G[16]; call_here(rt, ctx, 0x08A7D5FCu); G[4] = G[16]; call_here(rt, ctx, 0x08A69D68u); G[2] = word(rt, frame.at(0xCu)); if (G[2] != 0u) { G[20] = frame.at(0x10u); goto L_08A7EDD0; } G[2] = high_half(0x08D74FD0u); G[30] = 0x08D74FD0u; goto L_08A7EEEC; L_08A7EDD0: ; G[4] = G[20]; call_here(rt, ctx, 0x08A76AA0u); G[16] = word(rt, frame.at(0xCu)); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A7EDF4; } G[4] = high_half(0x08CB1358u); G[4] = 0x08CB1358u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A7EDF4: ; G[5] = G[20]; G[4] = G[16]; G[2] = word(rt, G[3] + 0x1Cu); { const std::uint32_t callee = G[2]; G[6] = G[19]; call_here(rt, ctx, callee); } G[5] = word(rt, frame.at(0x10u)); { const bool tk = G[5] != 0u; G[2] = high_half(0x08D74FD0u); if (tk) goto L_08A7EE1C; } G[30] = 0x08D74FD0u; goto L_08A7EEE4; L_08A7EE1C: ; if (G[19] == G[20]) { G[16] = word(rt, G[18] + 0x0u); goto L_08A7EE30; } G[4] = G[19]; call_here(rt, ctx, 0x08A5BAD8u); G[16] = word(rt, G[18] + 0x0u); L_08A7EE30: ; G[4] = G[16]; call_here(rt, ctx, 0x08B5C094u); G[4] = word(rt, G[16] + 0x8u); G[2] = high_half(0x08D74FD0u); G[30] = 0x08D74FD0u; G[17] = G[4] - 0xCu; if (G[17] == G[30]) { G[4] = G[16]; goto L_08A7EE74; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (tk) goto L_08A7EE74; } G[4] = G[17]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); G[4] = G[16]; L_08A7EE74: ; G[5] = 0xCu; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); G[16] = word(rt, frame.at(0x10u)); if (G[16] != 0u) { G[2] = word(rt, G[16] + 0x0u); goto L_08A7EE9C; } G[4] = high_half(0x08CB1350u); G[4] = 0x08CB1350u; call_here(rt, ctx, 0x08A5BB7Cu); G[2] = word(rt, G[16] + 0x0u); L_08A7EE9C: ; G[4] = G[16]; G[5] = G[19]; G[3] = word(rt, G[2] + 0x100u); G[6] = G[23]; { const std::uint32_t callee = G[3]; G[7] = G[18]; call_here(rt, ctx, callee); } G[4] = 0xCu; G[5] = 0x4u; G[17] = word(rt, G[18] + 0x0u); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2] + 0x8u; { const bool tk = G[4] == 0u; G[16] = G[2]; if (tk) goto L_08A7EED8; } G[5] = G[22]; call_here(rt, ctx, 0x08B88F44u); L_08A7EED8: ; G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08B5C078u); L_08A7EEE4: ; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); L_08A7EEEC: ; G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = word(rt, frame.at(0x8u)); G[16] = G[4] - 0xCu; { const bool tk = G[16] == G[30]; G[31] = word(rt, frame.at(0x44u)); if (tk) goto L_08A7EF28; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[31] = word(rt, frame.at(0x44u)); if (tk) goto L_08A7EF28; } G[4] = G[16]; G[5] = G[29]; call_here(rt, ctx, 0x08B8542Cu); G[31] = word(rt, frame.at(0x44u)); L_08A7EF28: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08A8DCB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[20]); G[20] = G[4]; put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x38u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[2] = word(rt, G[5] + 0x0u); { const bool tk = G[2] == 0u; G[19] = G[5]; if (tk) goto L_08A8DE34; } G[18] = G[4] + 0x74u; G[5] = G[18]; G[4] = frame.at(0xCu); call_here(rt, ctx, 0x08B88F44u); G[16] = word(rt, G[19] + 0x0u); { const bool tk = G[16] != 0u; G[5] = G[16] + 0x10u; if (tk) goto L_08A8DD14; } G[4] = high_half(0x08CB1C98u); G[4] = 0x08CB1C98u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16] + 0x10u; L_08A8DD14: ; G[4] = G[18]; call_here(rt, ctx, 0x08B855E0u); G[4] = word(rt, G[20] + 0x74u); G[2] = word(rt, frame.at(0xCu)); G[7] = frame.at(0x14u); G[17] = word(rt, G[4] - 0xCu); G[5] = G[2]; put(rt, frame.at(0x10u), G[17]); G[16] = word(rt, G[2] - 0xCu); G[2] = frame.at(0x10u); G[3] = G[16] < G[17] ? 1u : 0u; if (G[3] == 0u) G[7] = G[2]; put(rt, frame.at(0x14u), G[16]); G[6] = word(rt, G[7] + 0x0u); call_here(rt, ctx, 0x08B36170u); { const bool tk = G[2] != 0u; G[21] = frame.at(0x4u); if (tk) goto L_08A8DD68; } { const bool tk = G[17] != G[16]; G[2] = high_half(0x08D74FD0u); if (tk) goto L_08A8DD68; } G[17] = 0x08D74FD0u; goto L_08A8DDE8; L_08A8DD68: ; G[4] = G[21]; call_here(rt, ctx, 0x08A9DD14u); G[17] = word(rt, frame.at(0x4u)); { const bool tk = G[17] != 0u; G[16] = frame.at(0x8u); if (tk) goto L_08A8DD8C; } G[4] = high_half(0x08CB1D6Cu); G[4] = 0x08CB1D6Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = frame.at(0x8u); L_08A8DD8C: ; G[5] = G[17]; G[6] = G[18]; G[4] = G[16]; call_here(rt, ctx, 0x08A9CD70u); G[4] = G[16]; call_here(rt, ctx, 0x08A67270u); G[5] = G[2]; G[4] = G[20]; call_here(rt, ctx, 0x08A8C628u); G[4] = word(rt, frame.at(0x8u)); G[2] = high_half(0x08D74FD0u); G[17] = 0x08D74FD0u; G[16] = G[4] - 0xCu; { const bool tk = G[16] == G[17]; G[4] = G[4] - 0x4u; if (tk) goto L_08A8DDE0; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (tk) goto L_08A8DDE0; } G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B8542Cu); L_08A8DDE0: ; G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); L_08A8DDE8: ; G[16] = word(rt, G[19] + 0x0u); if (G[16] != 0u) { G[4] = word(rt, frame.at(0xCu)); goto L_08A8DE04; } G[4] = high_half(0x08CB1C98u); G[4] = 0x08CB1C98u; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = word(rt, frame.at(0xCu)); L_08A8DE04: ; G[2] = word(rt, G[16] + 0x14u); G[16] = G[4] - 0xCu; { const bool tk = G[16] == G[17]; put(rt, G[20] + 0x78u, G[2]); if (tk) goto L_08A8DE34; } G[4] = G[4] - 0x4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[31] = word(rt, frame.at(0x38u)); if (tk) goto L_08A8DE38; } G[4] = G[16]; G[5] = G[29]; call_here(rt, ctx, 0x08B8542Cu); L_08A8DE34: ; G[31] = word(rt, frame.at(0x38u)); L_08A8DE38: ; G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08A9B2C0 (a0 = the destination object, a1 = the source object or 0): both hold a vector of pointers at +0x14 (begin +0, end +4). The destination vector is
// resized to the source's element count (0x08B85798 trims it, 0x08B85F30 appends the missing slots), through a temporary 0x08A76AA0(frame) destroyed by
// 0x08A5B9EC; then every source element is copied into its slot: 0x08A5BB7C(0x08CB2328) when the element is null, 0x08A77444(temp, element) and, unless
// the slot is the temporary itself, 0x08A5BAD8(slot, temporary's word at frame +4), the temporary being 0x08A5B9EC'd after each. v0 = the last callee's.
void func_08A9B2C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 other = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {31u, 0x34u}, {30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}});
    if (other == 0u) return;
    const u32 source_vector = other + 0x14u;
    const u32 dest_vector = self + 0x14u;
    const u32 count = static_cast<u32>(static_cast<s32>(word(rt, source_vector + 4u) - word(rt, source_vector)) >> 2);
    call_set(rt, ctx, 0x08A76AA0u, {frame.at(0x0u)});
    const u32 begin = word(rt, dest_vector);
    const u32 end = word(rt, dest_vector + 4u);
    const u32 size = static_cast<u32>(static_cast<s32>(end - begin) >> 2);
    if (count < size) call_set(rt, ctx, 0x08B85798u, {dest_vector, begin + (count << 2), end});
    else call_set(rt, ctx, 0x08B85F30u, {dest_vector, end, count - size, frame.at(0x0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x0u)});
    if (count == 0u) return;
    const u32 temporary = frame.at(0x4u);
    u32 offset = 0u;
    for (u32 index = 0; index < count; ++index) {
        const u32 element = word(rt, word(rt, source_vector) + offset);
        const u32 slot = word(rt, dest_vector) + offset;
        offset += 4u;
        if (element == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2328u});
        call_set(rt, ctx, 0x08A77444u, {temporary, element});
        if (slot != temporary) call_set(rt, ctx, 0x08A5BAD8u, {slot, word(rt, frame.at(0x4u))});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    }
}
void func_08AB5174(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); G[2] = high_half(0x08D74FD0u); G[19] = G[4] + 0x1Cu; put(rt, frame.at(0x18u), G[18]); G[18] = 0x08D74FD0u; G[2] = high_half(0x08CD5798u); put(rt, frame.at(0x14u), G[17]); G[2] = 0x08CD5798u; G[5] = 0xFFFFFFFFu; put(rt, frame.at(0x10u), G[16]); G[16] = G[4]; put(rt, frame.at(0x20u), G[31]); G[3] = word(rt, G[4] + 0x2Cu); put(rt, G[16] + 0x0u, G[2]); G[17] = G[3] - 0xCu; { const bool tk = G[17] == G[18]; G[4] = G[3] - 0x4u; if (tk) goto L_08AB51D8; } call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0x2u); if (tk) goto L_08AB51D8; } call_here(rt, ctx, 0x08B8542Cu); L_08AB51D8: ; G[2] = word(rt, G[16] + 0x28u); G[5] = 0xFFFFFFFFu; G[17] = G[2] - 0xCu; { const bool tk = G[18] == G[17]; G[4] = G[2] - 0x4u; if (tk) goto L_08AB5208; } call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[5] = frame.at(0x1u); if (tk) goto L_08AB5208; } call_here(rt, ctx, 0x08B8542Cu); L_08AB5208: ; G[4] = word(rt, G[16] + 0x1Cu); G[5] = word(rt, G[19] + 0x4u); G[6] = G[29]; call_here(rt, ctx, 0x08C2BF24u); G[2] = word(rt, G[16] + 0x1Cu); G[5] = word(rt, G[19] + 0x8u); G[6] = 0x4u; G[4] = G[2]; G[5] = G[5] - G[2]; G[5] = sra(G[5], 2u); { const bool tk = G[2] == 0u; G[5] = G[5] << 2u; if (tk) goto L_08AB5240; } call_here(rt, ctx, 0x08A5BC54u); L_08AB5240: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5BB5Cu); G[31] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08AC7A98 (a0 = the player): the decoder loop of an audio stream (the ring at +0x5BC, the lock at +0x8, the decode state at +0x578, the 0x400-frame output block
// the caller gets). While the running byte (+0x567) is set: a request of +0x5A4 samples is cut to 0x800 and its block (+0x20680, 4 bytes
// per channel sample) cleared (0x08B36018), or, without a request, one is taken from the ring (0x08AC7548(ring, +0x20680, &count); 2 ends the
// call, another non-zero value waits 0x2710 µs, 0x08B2E2B8). The count's samples are copied (0x08B79A88) into the output block (`out`, frame count `filled` of
// 0x400, `channels` words per frame), each time `out` is empty a new one is decoded first: 0x08B34888 / 0x08B34B2C(lock) around the decode state
// (+0x578: the block at +0x10 plus +4 x +0x14, as the multiply-add leaves it in lo), until the running byte clears or a block is found. A filled
// block ends the round (0x08B34888(lock), 0x08B24A8C(+0x578, 1), 0x08B34B24(lock)) after the ring's 0x08AC7508 says more is waiting; a part-filled one is
// cleared to its end first. The ring's 0x08AC7508 non-zero ends the whole call.
void func_08AC7A98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x30u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {31u, 0x34u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {17u, 0x14u}, {16u, 0x10u}});
    u32 &ring = ctx.gpr[30];
    u32 &out = ctx.gpr[20];
    u32 &filled = ctx.gpr[19];
    u32 &total = ctx.gpr[22];
    u32 &channels = ctx.gpr[21];
    ring = self + 0x5BCu;
    out = 0u;
    filled = 0u;
    ctx.gpr[18] = self;
    channels = word(rt, ring + 0x30u);
    const u32 block = self + 0x20680u, lock = self + 0x8u, state = self + 0x578u, remaining_slot = frame.at(0x4u);
    auto mult = [&](u32 a, u32 b) {
        const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(a)) * static_cast<s32>(b);
        ctx.lo = static_cast<u32>(product);
        ctx.hi = static_cast<u32>(static_cast<std::uint64_t>(product) >> 32u);
    };
    while (load8(self + 0x567u) != 0u) {
        ctx.gpr[6] = word(rt, self + 0x5A4u);
        put(rt, remaining_slot, 0u);
        if (static_cast<s32>(ctx.gpr[6]) > 0) {
            const u32 request = ctx.gpr[6];
            put(rt, frame.at(0x0u), 0x800u);
            const u32 chunk = word(rt, static_cast<s32>(request) < 0x801 ? self + 0x5A4u : frame.at(0x0u));
            put(rt, self + 0x5A4u, request - chunk);
            put(rt, remaining_slot, chunk);
            call_set(rt, ctx, 0x08B36018u, {block, 0u, chunk << 2u});
            total = 0u;
        } else {
            const u32 status = call_set(rt, ctx, 0x08AC7548u, {ring, block, remaining_slot});
            if (status != 0u) {
                if (status == 2u) return;
                call_set(rt, ctx, 0x08B2E2B8u, {0x2710u});
                if (call_set(rt, ctx, 0x08AC7508u, {ring}) != 0u) return;
                continue;
            }
            total = 0u;
        }
        for (;;) {
            u32 remaining = word(rt, remaining_slot);
            ctx.gpr[2] = remaining;
            if (remaining == 0u) break;
            mult(channels, total);
            if (out == 0u) {
                const u32 s0 = state;
                ctx.gpr[17] = lock;
                ctx.gpr[16] = s0;
                ctx.gpr[23] = 1u;
                bool stopped = false;
                for (;;) {
                    call_set(rt, ctx, 0x08B34888u, {lock});
                    if (word(rt, s0 + 0x18u) == word(rt, s0 + 0xCu)) {
                        store8(self + 0x594u, 1u);
                        call_set(rt, ctx, 0x08B34B2Cu, {lock});
                    }
                    ctx.lo = word(rt, s0 + 0x10u);
                    const u32 samples = word(rt, s0 + 0x4u), step = word(rt, s0 + 0x14u);
                    ctx.gpr[5] = samples;
                    ctx.gpr[3] = step;
                    // madd: hi:lo += samples * step (signed)
                    const std::uint64_t accumulated = ((static_cast<std::uint64_t>(ctx.hi) << 32u) | ctx.lo) +
                                                      static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<s32>(samples)) * static_cast<s32>(step));
                    ctx.lo = static_cast<u32>(accumulated);
                    ctx.hi = static_cast<u32>(accumulated >> 32u);
                    out = ctx.lo;
                    call_set(rt, ctx, 0x08B34B24u, {lock});
                    if (load8(self + 0x567u) == 0u) {
                        stopped = true;
                        break;
                    }
                    if (out != 0u) break;
                }
                if (stopped) break;
                ctx.gpr[2] = load8(self + 0x567u);
                remaining = word(rt, remaining_slot);
                ctx.gpr[2] = remaining;
                mult(channels, total);
            }
            const u32 room = 0x400u - filled;
            put(rt, frame.at(0x8u), room);
            const u32 source_frames = ctx.lo;
            mult(filled, channels);
            const u32 count = room < remaining ? room : remaining;
            ctx.gpr[16] = count;
            const u32 out_samples = ctx.lo;
            mult(channels, count);
            const u32 bytes = ctx.lo << 1u;
            call_set(rt, ctx, 0x08B79A88u, {out + (out_samples << 1u), self + ((source_frames << 1u) + 0x20680u), bytes});
            const u32 left = word(rt, remaining_slot) - count;
            filled += count;
            total += count;
            put(rt, remaining_slot, left);
            if (filled < 0x400u) {
                if (call_set(rt, ctx, 0x08AC7508u, {ring}) == 0u) continue;
                ctx.gpr[2] = 0x400u - filled;
                ctx.gpr[17] = lock;
                if (ctx.gpr[2] != 0u) {
                    mult(channels, filled);
                    const u32 clear_from = out + (ctx.lo << 1u);
                    mult(channels, ctx.gpr[2]);
                    call_set(rt, ctx, 0x08B36018u, {clear_from, 0u, ctx.lo << 1u});
                }
            }
            ctx.gpr[17] = lock;
            call_set(rt, ctx, 0x08B34888u, {lock});
            call_set(rt, ctx, 0x08B24A8Cu, {state, 1u});
            call_set(rt, ctx, 0x08B34B24u, {lock});
            out = 0u;
            filled = 0u;
        }
        if (call_set(rt, ctx, 0x08AC7508u, {ring}) != 0u) return;
    }
}
// 0x08ADC764 (a0 = key, a1 = optional out pointer, a2, a3, t0 = pass-through arguments): 0x08AEB808, then a table of 0x08D40630 words is made on the stack (size
// rounded up to 16 bytes below the frame) and filled by 0x08ADB39C(table), which returns the entry count (0x08AEB82C closes the section). Each entry is a record
// pointer; 0x08ADC4E0(record[+8], key, a2, a3, t0) runs on them in order and the first nonzero answer stops with v0 = 1, after storing record[+8] through the out pointer
// (kept at frame +0) when it is not null. v0 = 0 when none answers.
void func_08ADC764(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 key = ctx.gpr[4];
    const u32 out = ctx.gpr[5];
    const u32 extra_a = ctx.gpr[6];
    const u32 extra_b = ctx.gpr[7];
    const u32 extra_c = ctx.gpr[8];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x40u}, {31u, 0x44u}, {23u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {18u, 0x28u}, {17u, 0x24u}, {16u, 0x20u}});
    put(rt, frame.at(0x0u), out);
    call_set(rt, ctx, 0x08AEB808u, {});
    const u32 table_bytes = ((word(rt, 0x08D40630u) << 2) + 0xFu) & ~0xFu;
    ctx.gpr[29] -= table_bytes;
    const u32 table = ctx.gpr[29];
    const s32 count = static_cast<s32>(call_set(rt, ctx, 0x08ADB39Cu, {table}));
    call_set(rt, ctx, 0x08AEB82Cu, {});
    for (s32 index = 0; index < count; ++index) {
        const u32 record = word(rt, table + 4u * static_cast<u32>(index));
        if (call_set(rt, ctx, 0x08ADC4E0u, {word(rt, record + 0x8u), key, extra_a, extra_b, extra_c}) == 0u) continue;
        const u32 result_slot = word(rt, frame.at(0x0u));
        if (result_slot != 0u) put(rt, word(rt, frame.at(0x0u)), word(rt, record + 0x8u));
        ctx.gpr[2] = 1u;
        return;
    }
    ctx.gpr[2] = 0u;
}
void func_08AEED10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[23]); G[23] = G[4]; put(rt, frame.at(0x18u), G[22]); G[22] = G[6]; G[6] = 0u; put(rt, frame.at(0x14u), G[21]); G[21] = G[5]; G[5] = 0x1u; put(rt, frame.at(0x10u), G[20]); G[20] = G[10]; put(rt, frame.at(0xCu), G[19]); G[19] = G[9]; put(rt, frame.at(0x8u), G[18]); G[18] = G[8]; put(rt, frame.at(0x4u), G[17]); G[17] = G[7]; put(rt, frame.at(0x0u), G[16]); G[16] = high_half(0x08D41AD0u); G[16] = 0x08D41AD0u; G[4] = word(rt, G[16] + 0x4F4u); call_here(rt, ctx, 0x08C7AA20u); G[4] = word(rt, G[16] + 0x4F8u); G[5] = 0x1u; G[6] = 0u; call_here(rt, ctx, 0x08C7AA20u); G[4] = word(rt, G[16] + 0x508u); G[3] = word(rt, G[16] + 0x500u); G[5] = 0x1u; G[4] = G[4] << 5u; G[3] = G[3] + G[4]; put(rt, G[3] + 0x0u, G[23]); put(rt, G[3] + 0x4u, G[21]); put(rt, G[3] + 0x8u, G[22]); put(rt, G[3] + 0xCu, G[17]); put(rt, G[3] + 0x10u, G[18]); put(rt, G[3] + 0x14u, G[19]); put(rt, G[3] + 0x18u, G[20]); put(rt, G[3] + 0x1Cu, 0u); G[2] = word(rt, G[16] + 0x508u); G[3] = word(rt, G[16] + 0x504u); G[2] = G[2] + 0x1u; divide(ctx, G[2], G[3]); if (G[3] == 0u) {  goto L_08AEEDC8; } L_08AEEDC8: ; G[2] = word(rt, G[16] + 0x510u); G[4] = word(rt, G[16] + 0x4F8u); G[2] = G[2] + 0x1u; put(rt, G[16] + 0x510u, G[2]); G[6] = ctx.hi; put(rt, G[16] + 0x508u, G[6]); call_here(rt, ctx, 0x08C7AA08u); G[4] = word(rt, G[16] + 0x4F0u); G[5] = 0x1u; call_here(rt, ctx, 0x08C7AA08u); G[31] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08B04908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; G[19] = G[7]; put(rt, frame.at(0x8u), G[18]); G[18] = G[4]; put(rt, frame.at(0x4u), G[17]); G[17] = G[6]; put(rt, frame.at(0x0u), G[16]); G[16] = G[5]; { const bool tk = static_cast<std::int32_t>(G[6]) >= 0; put(rt, frame.at(0x10u), G[31]); if (tk) goto L_08B04980; } G[4] = word(rt, G[4] + 0x0u); call_here(rt, ctx, 0x08B01804u); G[4] = G[16] << 8u; G[3] = G[16] << 4u; G[3] = G[4] - G[3]; G[6] = G[2]; { const bool tk = G[2] == 0u; G[7] = G[19]; if (tk) goto L_08B04A24; } G[4] = word(rt, G[18] + 0x0u); G[31] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[5] = word(rt, G[4] + 0x0u); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[5] = G[5] + G[3]; tail = 0x08B046C0u; goto exit_; L_08B04980: ; if (G[2] == 0u) { G[4] = word(rt, G[4] + 0x0u); goto L_08B049D4; } G[4] = word(rt, G[4] + 0x0u); call_here(rt, ctx, 0x08B01804u); G[4] = G[16] << 8u; G[3] = G[16] << 4u; G[3] = G[4] - G[3]; G[6] = G[17]; G[7] = G[2]; { const bool tk = G[2] == 0u; G[8] = G[19]; if (tk) goto L_08B04A24; } G[4] = word(rt, G[18] + 0x0u); G[31] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[5] = word(rt, G[4] + 0x0u); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[5] = G[5] + G[3]; tail = 0x08B047D0u; goto exit_; L_08B049D4: ; G[5] = G[6]; G[6] = G[16]; call_here(rt, ctx, 0x08B01804u); G[4] = G[17] << 8u; G[3] = G[17] << 4u; G[3] = G[4] - G[3]; G[5] = G[16]; G[7] = G[2]; { const bool tk = G[2] == 0u; G[8] = G[19]; if (tk) goto L_08B04A24; } G[4] = word(rt, G[18] + 0x0u); G[31] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[6] = word(rt, G[4] + 0x0u); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[6] = G[6] + G[3]; tail = 0x08B03CE4u; goto exit_; L_08B04A24: ; G[31] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08B18534(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x330u); G[8] = G[8] & 0xFFu; put(rt, frame.at(0x320u), G[30]); G[30] = G[5]; put(rt, frame.at(0x324u), G[31]); put(rt, frame.at(0x314u), G[21]); G[21] = G[6]; put(rt, frame.at(0x310u), G[20]); G[20] = G[4]; put(rt, frame.at(0x308u), G[18]); G[18] = G[7]; put(rt, frame.at(0x304u), G[17]); put(rt, frame.at(0x2F0u), G[8]); put(rt, frame.at(0x31Cu), G[23]); put(rt, frame.at(0x318u), G[22]); put(rt, frame.at(0x30Cu), G[19]); put(rt, frame.at(0x300u), G[16]); call_here(rt, ctx, 0x08B0CE58u); G[4] = G[30]; call_here(rt, ctx, 0x08C7A850u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[17] = G[2]; if (tk) goto L_08B187A4; } G[2] = G[18]; { const bool tk = G[21] != 0u; G[3] = 0u; if (tk) goto L_08B185C0; } G[18] = 0xFFFFFFFFu; G[2] = 0xFFFFFFFFu; goto L_08B185C0; L_08B185A4: ; G[4] = G[2]; call_here(rt, ctx, 0x08B1830Cu); put(rt, G[20] + 0x0u, G[2]); goto L_08B18794; L_08B185B4: ; put(rt, G[20] + 0x10u, G[16]); put(rt, G[20] + 0x14u, G[3]); goto L_08B18794; L_08B185C0: ; put(rt, G[20] + 0x10u, G[2]); { const bool tk = G[18] == 0u; put(rt, G[20] + 0x14u, G[3]); if (tk) goto L_08B18794; } G[16] = 0u; G[22] = frame.at(0x1A8u); G[4] = G[17]; L_08B185D8: ; G[5] = frame.at(0x150u); put(rt, frame.at(0x2A8u), 0u); call_here(rt, ctx, 0x08C7A858u); if (static_cast<std::int32_t>(G[2]) < 0) goto L_08B185A4; { const bool tk = G[2] == 0u; G[3] = 0u; if (tk) goto L_08B185B4; } G[2] = high_half(0x08CB4C0Cu); G[4] = G[22]; G[5] = 0x08CB4C0Cu; call_here(rt, ctx, 0x08B35FD8u); if (G[2] == 0u) { G[16] = G[16] - 0x1u; goto L_08B18784; } G[3] = high_half(0x08CB4C10u); G[4] = G[22]; G[5] = 0x08CB4C10u; call_here(rt, ctx, 0x08B35FD8u); if (G[2] != 0u) goto L_08B1862C; G[16] = G[16] - 0x1u; goto L_08B18784; L_08B1862C: ; { const bool tk = G[21] == 0u; G[3] = word(rt, frame.at(0x150u)); if (tk) goto L_08B18784; } G[2] = G[16] << 6u; G[19] = G[21] + G[2]; G[3] = G[3] & 0xF000u; G[2] = 0x1000u; { const bool tk = G[3] == G[2]; put(rt, G[19] + 0x0u, 0u); if (tk) goto L_08B18664; } G[2] = 0x2000u; { const bool tk = G[3] != G[2]; G[2] = word(rt, frame.at(0x158u)); if (tk) goto L_08B18670; } G[4] = 0x1u; put(rt, G[19] + 0x0u, G[4]); goto L_08B1866C; L_08B18664: ; G[2] = 0x3u; put(rt, G[19] + 0x0u, G[2]); L_08B1866C: ; G[2] = word(rt, frame.at(0x158u)); L_08B18670: ; G[23] = G[19] + 0xCu; G[4] = G[23]; put(rt, G[19] + 0x8u, G[2]); G[5] = G[22]; G[6] = 0x33u; put(rt, G[19] + 0x4u, G[2]); call_here(rt, ctx, 0x08B7C830u); G[3] = word(rt, frame.at(0x2F0u)); if (G[3] == 0u) { G[16] = G[16] + 0x1u; goto L_08B18788; } G[2] = word(rt, G[19] + 0x0u); G[4] = 0x3u; { const bool tk = G[2] == G[4]; G[3] = 0x100u; if (tk) goto L_08B18784; } G[5] = G[30]; G[4] = frame.at(0x40u); G[2] = frame.at(0x4Cu); put(rt, frame.at(0x44u), G[3]); put(rt, frame.at(0x40u), G[2]); put(rt, frame.at(0x48u), 0u); store8(frame.at(0x4Cu), 0u); call_here(rt, ctx, 0x08B1CBFCu); G[5] = high_half(0x08CB4C14u); G[4] = frame.at(0x40u); G[5] = 0x08CB4C14u; call_here(rt, ctx, 0x08B1CD8Cu); G[4] = frame.at(0x40u); G[5] = G[23]; call_here(rt, ctx, 0x08B1CD8Cu); G[5] = word(rt, frame.at(0x40u)); G[4] = frame.at(0x2B0u); call_here(rt, ctx, 0x08B1837Cu); G[6] = frame.at(0x2B0u); G[7] = G[29]; L_08B186F8: ; G[2] = word(rt, G[6] + 0x0u); G[3] = word(rt, G[6] + 0x4u); G[4] = word(rt, G[6] + 0x8u); G[5] = word(rt, G[6] + 0xCu); put(rt, G[7] + 0x0u, G[2]); G[6] = G[6] + 0x10u; G[7] = G[7] + 0x10u; G[2] = frame.at(0x2E0u); put(rt, G[7] - 0xCu, G[3]); put(rt, G[7] - 0x8u, G[4]); { const bool tk = G[6] != G[2]; put(rt, G[7] - 0x4u, G[5]); if (tk) goto L_08B186F8; } G[2] = word(rt, G[6] + 0x8u); G[3] = word(rt, G[6] + 0x0u); G[4] = word(rt, G[6] + 0x4u); put(rt, G[7] + 0x8u, G[2]); put(rt, G[7] + 0x0u, G[3]); put(rt, G[7] + 0x4u, G[4]); G[2] = word(rt, frame.at(0x0u)); if (G[2] != 0u) { put(rt, G[19] + 0x8u, 0u); goto L_08B1877C; } G[2] = word(rt, frame.at(0x8u)); G[3] = word(rt, frame.at(0x10u)); G[4] = 0x1u; G[2] = G[2] & 0x1u; put(rt, G[19] + 0x0u, G[4]); { const bool tk = G[2] == 0u; put(rt, G[19] + 0x8u, G[3]); if (tk) goto L_08B18784; } G[3] = word(rt, frame.at(0x18u)); G[2] = 0x2u; put(rt, G[19] + 0x0u, G[2]); put(rt, G[19] + 0x4u, G[3]); goto L_08B18784; L_08B1877C: ; put(rt, G[19] + 0x0u, 0u); put(rt, G[19] + 0x4u, 0u); L_08B18784: ; G[16] = G[16] + 0x1u; L_08B18788: ; G[2] = G[16] < G[18] ? 1u : 0u; if (G[2] != 0u) { G[4] = G[17]; goto L_08B185D8; } L_08B18794: ; G[4] = G[17]; call_here(rt, ctx, 0x08C7A860u); G[2] = G[20]; goto L_08B187B4; L_08B187A4: ; G[4] = G[2]; call_here(rt, ctx, 0x08B1830Cu); put(rt, G[20] + 0x0u, G[2]); G[2] = G[20]; L_08B187B4: ; G[31] = word(rt, frame.at(0x324u)); G[30] = word(rt, frame.at(0x320u)); G[23] = word(rt, frame.at(0x31Cu)); G[22] = word(rt, frame.at(0x318u)); G[21] = word(rt, frame.at(0x314u)); G[20] = word(rt, frame.at(0x310u)); G[19] = word(rt, frame.at(0x30Cu)); G[18] = word(rt, frame.at(0x308u)); G[17] = word(rt, frame.at(0x304u)); G[16] = word(rt, frame.at(0x300u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08B2CF88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); G[23] = G[7]; put(rt, frame.at(0x14u), G[21]); G[21] = G[4]; put(rt, frame.at(0x10u), G[20]); G[20] = G[5]; put(rt, frame.at(0xCu), G[19]); G[19] = G[6]; put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); { const bool tk = G[5] != 0u; put(rt, frame.at(0x0u), G[16]); if (tk) goto L_08B2CFF8; } G[5] = G[6]; G[31] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[6] = G[7]; tail = 0x08B2CB18u; goto exit_; L_08B2CFF8: ; G[18] = G[5] - 0x10u; G[5] = G[18]; call_here(rt, ctx, 0x08B2C780u); { const bool tk = G[2] == 0u; G[16] = 0u; if (tk) goto L_08B2D178; } G[22] = G[21] + 0x58u; G[4] = G[22]; call_here(rt, ctx, 0x08B34D40u); G[17] = word(rt, G[18] + 0x4u); G[16] = G[19] + 0xFu; G[16] = ins(G[16], 0u, 0u, 4u); G[19] = G[16] + 0x10u; G[3] = G[17] - G[18]; G[2] = G[3] < G[19] ? 1u : 0u; { const bool tk = G[2] == 0u; G[2] = G[16] + 0x30u; if (tk) goto L_08B2D0B8; } G[2] = word(rt, G[21] + 0x8u); G[2] = G[17] < G[2] ? 1u : 0u; if (G[2] == 0u) { G[16] = 0u; goto L_08B2D12C; } G[3] = word(rt, G[17] + 0x8u); G[2] = 0xFFFFFFFFu; { const bool tk = G[3] == G[2]; G[4] = G[21]; if (tk) goto L_08B2D128; } G[5] = G[17]; call_here(rt, ctx, 0x08B2C720u); G[4] = G[21]; G[5] = G[18]; G[6] = G[17]; call_here(rt, ctx, 0x08B2C484u); G[2] = word(rt, G[18] + 0x4u); G[4] = G[2] - G[18]; G[3] = G[4] < G[19] ? 1u : 0u; if (G[3] != 0u) { G[16] = 0u; goto L_08B2D12C; } G[2] = G[16] + 0x30u; G[2] = G[4] < G[2] ? 1u : 0u; if (G[2] != 0u) { G[16] = G[20]; goto L_08B2D12C; } G[6] = G[19]; G[4] = G[21]; G[5] = G[18]; call_here(rt, ctx, 0x08B2C42Cu); G[5] = G[2]; G[4] = G[21]; call_here(rt, ctx, 0x08B2C504u); G[16] = G[20]; goto L_08B2D12C; L_08B2D0B8: ; G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[16] = G[20]; if (tk) goto L_08B2D12C; } G[4] = G[21]; G[5] = G[18]; G[6] = G[19]; call_here(rt, ctx, 0x08B2C42Cu); G[16] = word(rt, G[2] + 0x4u); G[17] = G[2]; G[2] = word(rt, G[21] + 0x8u); G[2] = G[16] < G[2] ? 1u : 0u; { const bool tk = G[2] == 0u; G[5] = G[17]; if (tk) goto L_08B2D118; } G[3] = word(rt, G[16] + 0x8u); G[2] = 0xFFFFFFFFu; { const bool tk = G[3] == G[2]; G[4] = G[21]; if (tk) goto L_08B2D118; } G[5] = G[16]; call_here(rt, ctx, 0x08B2C720u); G[4] = G[21]; G[5] = G[17]; G[6] = G[16]; call_here(rt, ctx, 0x08B2C484u); G[5] = G[17]; L_08B2D118: ; G[4] = G[21]; call_here(rt, ctx, 0x08B2C504u); G[16] = G[20]; goto L_08B2D12C; L_08B2D128: ; G[16] = 0u; L_08B2D12C: ; G[4] = G[22]; call_here(rt, ctx, 0x08B34D80u); { const bool tk = G[16] != 0u; G[2] = G[16]; if (tk) goto L_08B2D17C; } G[5] = G[19]; G[6] = G[23]; G[4] = G[21]; call_here(rt, ctx, 0x08B2CB18u); { const bool tk = G[2] == 0u; G[16] = G[2]; if (tk) goto L_08B2D16C; } G[6] = word(rt, G[18] + 0x4u); G[4] = G[2]; G[5] = G[20]; G[6] = G[6] - G[18]; G[6] = G[6] - 0x10u; call_here(rt, ctx, 0x08B79A88u); L_08B2D16C: ; G[4] = G[21]; G[5] = G[20]; call_here(rt, ctx, 0x08B2CE7Cu); L_08B2D178: ; G[2] = G[16]; L_08B2D17C: ; G[31] = word(rt, frame.at(0x20u)); G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08B3E45C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); G[2] = sext(load16(frame.at(0x48u)), 16u); put(rt, frame.at(0x24u), G[21]); G[10] = G[10] & 0xFFu; G[11] = G[11] & 0xFFu; put(rt, frame.at(0x1Cu), G[19]); G[21] = word(rt, frame.at(0x4Cu)); G[19] = load8(frame.at(0x44u)); put(rt, frame.at(0x20u), G[20]); G[20] = G[4]; put(rt, frame.at(0x14u), G[17]); G[17] = G[8]; put(rt, frame.at(0x10u), G[16]); G[16] = sext(G[5], 16u); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); G[23] = G[9]; put(rt, frame.at(0x28u), G[22]); G[22] = G[6]; put(rt, frame.at(0x18u), G[18]); G[18] = G[7]; put(rt, frame.at(0x0u), G[10]); put(rt, frame.at(0x4u), G[11]); put(rt, frame.at(0x8u), G[2]); G[30] = load16(frame.at(0x40u)); call_here(rt, ctx, 0x08B3E280u); G[5] = G[17]; G[4] = G[21]; store16(G[20] + 0x0u, G[16]); G[6] = 0x10u; store8(G[20] + 0x8u, G[19]); put(rt, G[20] + 0xCu, G[17]); call_here(rt, ctx, 0x08B2CB18u); G[4] = G[2]; G[5] = G[18]; { const bool tk = G[18] == 0u; put(rt, G[20] + 0x4u, G[2]); if (tk) goto L_08B3E504; } G[6] = word(rt, G[20] + 0xCu); call_here(rt, ctx, 0x08B79A88u); put(rt, G[20] + 0x10u, G[23]); goto L_08B3E518; L_08B3E504: ; G[6] = word(rt, G[20] + 0xCu); G[4] = G[2]; G[5] = 0u; call_here(rt, ctx, 0x08B36018u); put(rt, G[20] + 0x10u, G[23]); L_08B3E518: ; G[4] = G[21]; G[6] = 0x10u; G[2] = word(rt, frame.at(0x0u)); G[5] = word(rt, G[20] + 0xCu); store8(G[20] + 0x14u, G[2]); G[2] = word(rt, frame.at(0x4u)); store16(G[20] + 0x16u, G[30]); store8(G[20] + 0x15u, G[2]); G[2] = word(rt, frame.at(0x8u)); store16(G[20] + 0x28u, G[2]); call_here(rt, ctx, 0x08B2CB18u); G[4] = G[2]; G[6] = word(rt, G[20] + 0xCu); G[5] = 0u; put(rt, G[20] + 0x58u, G[2]); call_here(rt, ctx, 0x08B36018u); G[5] = G[22]; G[4] = G[20] + 0x34u; { const bool tk = G[22] == 0u; G[6] = 0x20u; if (tk) goto L_08B3E570; } call_here(rt, ctx, 0x08B7C830u); L_08B3E570: ; put(rt, G[20] + 0x5Cu, G[21]); G[31] = word(rt, frame.at(0x34u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B44C10 (a0 = the manager, a3 = the request, t0 = the request size, 0x58 or nothing is done and v0 = 0x58): takes a 0x50-byte record from the manager's pool
// (+0xCC, 0x08B2CB18(pool, 0x50, 0x10), constructed by 0x08B3F478) and fills it from the request (0x08B3E7FC(record, request + 8)); its two ids
// (+8, +0xC) are -1 and its state (+0x28) 0. 0x08B3EE8C(table +0x8DC, id) finding an entry frees the record (0x08B2CE7C) and ends. Else the state
// is 4 when the record's flag byte (+4) is set, or when the clock source (+0xB0, virtual +0x3C returning two words) does not give the record's (+0x38, +0x3C) and
// the manager's +0x944 is set; if neither, the table is searched with the clock (0x08B3F098(table, id, clock)): nothing found or an older record
// (compared by +0x4C then +0x48) gives state 1, otherwise the entry is copied (0x08B36018 clear, 0x08B3E7FC, 0x08B40A74(manager, id, 8, copy,
// 0x58, 1)) and the state stays. The source's virtual +0x84(0, record +0x12, 0) gives the record's +8, the table takes the record (0x08B3EE94) and, when the clock
// still matches, +0x93C = the id and +8 = -1; a record with its flag byte set also sets the manager's +0xB7 byte and calls 0x08B42710(manager, 1).
// 0x08B402AC(manager) ends it.
void func_08B44C10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], request = ctx.gpr[7];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x70u}, {16u, 0x60u}, {31u, 0x7Cu}, {23u, 0x78u}, {22u, 0x74u}, {19u, 0x6Cu}, {18u, 0x68u}, {17u, 0x64u}});
    ctx.gpr[2] = 0x58u;
    ctx.gpr[20] = self;
    ctx.gpr[16] = request;
    if (ctx.gpr[8] != 0x58u) return;
    const u32 pool = self + 0xCCu;
    ctx.gpr[18] = pool;
    const u32 record = call_set(rt, ctx, 0x08B2CB18u, {pool, 0x50u, 0x10u});
    ctx.gpr[17] = record;
    if (record != 0u) call_set(rt, ctx, 0x08B3F478u, {record});
    call_set(rt, ctx, 0x08B3E7FCu, {record, request + 0x8u});
    const u32 id = load16(record + 0x0u);
    const u32 table = self + 0x8DCu;
    ctx.gpr[19] = table;
    put(rt, record + 0xCu, 0xFFFFFFFFu);
    put(rt, record + 0x28u, 0u);
    put(rt, record + 0x8u, 0xFFFFFFFFu);
    ctx.gpr[5] = id;
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (call_set(rt, ctx, 0x08B3EE8Cu, {table, id}) != 0u) {
        call_set(rt, ctx, 0x08B2CE7Cu, {pool, record});
        return;
    }
    // the clock source's virtual +0x3C: v0 / v1 are its two words
    auto clock = [&] {
        const u32 source = word(rt, self + 0xB0u);
        return call_set(rt, ctx, word(rt, word(rt, source) + 0x3Cu), {source});
    };
    bool search = false;
    ctx.gpr[2] = 0x4u;
    if (load8(record + 0x4u) == 0u) {
        const u32 want_lo = word(rt, record + 0x38u);
        ctx.gpr[22] = want_lo;
        const u32 source = word(rt, self + 0xB0u);
        const u32 want_hi = word(rt, record + 0x3Cu);
        ctx.gpr[23] = want_hi;
        const u32 now_lo = call_set(rt, ctx, word(rt, word(rt, source) + 0x3Cu), {source});
        if (now_lo != want_lo || ctx.gpr[3] != want_hi) {
            ctx.gpr[2] = word(rt, self + 0x944u);
            search = ctx.gpr[2] == 0u;
        }
        if (!search) ctx.gpr[2] = 0x4u;
    }
    if (!search) {
        put(rt, record + 0x28u, ctx.gpr[2]);
    } else {
        const u32 now_lo = clock();
        const u32 now_hi = ctx.gpr[3];
        const u32 found = call_set(rt, ctx, 0x08B3F098u, {table, ctx.gpr[5], now_lo, now_hi});
        ctx.gpr[18] = found;
        bool copy = false;
        u32 state = 1u;
        if (found != 0u) {
            const u32 found_high = word(rt, found + 0x4Cu), record_high = word(rt, record + 0x4Cu);
            ctx.gpr[4] = found_high;
            ctx.gpr[3] = record_high;
            ctx.gpr[2] = record_high < found_high ? 1u : 0u;
            if (record_high < found_high) {
                state = 1u;
            } else {
                ctx.gpr[16] = frame.at(0x8u);
                if (found_high != record_high) {
                    copy = true;
                } else {
                    const u32 found_low = word(rt, found + 0x48u), record_low = word(rt, record + 0x48u);
                    ctx.gpr[2] = record_low < found_low ? 1u : 0u;
                    copy = !(record_low < found_low);
                }
            }
        }
        if (!copy) {
            put(rt, record + 0x28u, state);
            ctx.gpr[2] = state;
        } else {
            ctx.gpr[16] = frame.at(0x8u);
            call_set(rt, ctx, 0x08B3F478u, {frame.at(0x8u)});
            call_set(rt, ctx, 0x08B36018u, {frame.at(0x0u), 0u, 0x58u});
            call_set(rt, ctx, 0x08B3E7FCu, {frame.at(0x8u), found});
            call_set(rt, ctx, 0x08B40A74u, {self, load16(record + 0x0u), 0x8u, frame.at(0x0u), 0x58u, 1u});
        }
    }
    const u32 source = word(rt, self + 0xB0u);
    ctx.gpr[7] = 0u;
    ctx.gpr[5] = 0u;
    const u32 issued = call_set(rt, ctx, word(rt, word(rt, source) + 0x84u), {source, 0u, record + 0x12u, 0u});
    put(rt, record + 0x8u, issued);
    call_set(rt, ctx, 0x08B3EE94u, {table, record});
    const u32 later = word(rt, self + 0xB0u);
    const u32 mark_lo = word(rt, record + 0x38u);
    ctx.gpr[18] = mark_lo;
    const u32 mark_hi = word(rt, record + 0x3Cu);
    ctx.gpr[19] = mark_hi;
    const u32 later_lo = call_set(rt, ctx, word(rt, word(rt, later) + 0x3Cu), {later});
    if (later_lo == mark_lo && ctx.gpr[3] == mark_hi) {
        const u32 identifier = load16(record + 0x0u);
        ctx.gpr[2] = identifier;
        ctx.gpr[3] = 0xFFFFFFFFu;
        put(rt, self + 0x93Cu, identifier);
        put(rt, record + 0x8u, 0xFFFFFFFFu);
    }
    ctx.gpr[2] = load8(record + 0x4u);
    const bool flagged = ctx.gpr[2] != 0u;
    ctx.gpr[2] = 0x1u;
    if (flagged) {
        store8(self + 0xB7u, 1u);
        call_set(rt, ctx, 0x08B42710u, {self, 1u});
    }
    call_set(rt, ctx, 0x08B402ACu, {self});
}
// 0x08B54A64 (a0 = the pool, a1 = the limit): releases and merges the pool's block table. The table is `count` (+0x10) entries of 0x1C bytes from +0x10,
// each with a block record at +0xC (+4 = address, +8 = size, +0x10 = its owner entry, +0x18 = in use). First pass: every entry whose record exists and whose
// age (pool +8 minus the entry's +0x18) is not below the limit is released (owner cleared, 0x08B54940(pool, record)); the number released is
// the result. Second pass: the used records are kept in a table of pointers on the frame sorted by address (binary search of the first one at
// or above the address): a record that ends where the previous starts, or starts where the next ends, is merged into it (size added, then
// 0x08B548E4 and 0x08B54858 with (pool, record)), otherwise it is inserted, the following pointers moved up one place. v0 = the number released.
void func_08B54A64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], limit = ctx.gpr[5];
    GuestFrame frame(ctx, 0x820u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x818u}, {19u, 0x80Cu}, {18u, 0x808u}, {31u, 0x81Cu}, {21u, 0x814u}, {20u, 0x810u}, {17u, 0x804u}, {16u, 0x800u}});
    u32 released = 0u;
    ctx.gpr[22] = 0u;
    ctx.gpr[19] = limit;
    ctx.gpr[18] = self;
    u32 count = word(rt, self + 0x10u);
    ctx.gpr[9] = count;
    if (static_cast<s32>(count) > 0) {
        u32 entry = self + 0x10u;
        u32 passed = 0u;
        for (;;) {
            const u32 record = word(rt, entry + 0xCu);
            ctx.gpr[16] = entry;
            ctx.gpr[6] = record;
            ctx.gpr[17] = ++passed;
            ctx.gpr[5] = entry + 0xCu;
            if (record != 0u && !(word(rt, self + 0x8u) - word(rt, entry + 0x18u) < limit)) {
                put(rt, record + 0x10u, 0u);
                call_set(rt, ctx, 0x08B54940u, {self, entry + 0xCu});
                ctx.gpr[22] = ++released;
                count = word(rt, self + 0x10u);
                ctx.gpr[9] = count;
            }
            if (!(static_cast<s32>(passed) < static_cast<s32>(count))) break;
            entry += 0x1Cu;
        }
    }
    if (static_cast<s32>(count) > 0) {
        u32 kept = 0u, index = 0u;
        u32 cursor = self + 0x10u;
        u32 out = frame.at(0x0u);
        ctx.gpr[19] = 0u;
        ctx.gpr[21] = out;
        auto sync = [&](u32 record) {
            ctx.gpr[16] = record;
            ctx.gpr[17] = cursor;
            ctx.gpr[19] = kept;
            ctx.gpr[20] = index;
            ctx.gpr[21] = out;
        };
        auto append = [&](u32 record) {
            put(rt, out, record);
            ++kept;
            out += 4u;
            ++index;
        };
        auto merged = [&](u32 record) {
            ++index;
            sync(record);
            call_set(rt, ctx, 0x08B548E4u, {self, record});
            call_set(rt, ctx, 0x08B54858u, {self, record});
            count = word(rt, self + 0x10u);
            ctx.gpr[9] = count;
        };
        auto process = [&](u32 record) {
            sync(record);
            if (word(rt, record + 0x18u) == 0u) {
                ++index;
                return;
            }
            if (kept == 0u) {
                append(record);
                return;
            }
            const u32 address = word(rt, record + 0x4u);
            const u32 size = word(rt, record + 0x8u);
            u32 upper = kept - 1u, lower = 0u;
            if (static_cast<s32>(upper) > 0) {
                do {
                    const u32 sum = upper + lower;
                    const u32 middle = sra((sum >> 31u) + sum, 1u);
                    if (word(rt, word(rt, frame.at(middle << 2u)) + 0x4u) < address) lower = middle + 1u;
                    else upper = middle;
                } while (static_cast<s32>(lower) < static_cast<s32>(upper));
            }
            const u32 position = (word(rt, word(rt, frame.at(lower << 2u)) + 0x4u) < address ? 1u : 0u) + lower;
            if (static_cast<s32>(position) > 0) {
                const u32 before = word(rt, frame.at((position - 1u) << 2u));
                const u32 before_size = word(rt, before + 0x8u);
                if (address == word(rt, before + 0x4u) + before_size) {
                    put(rt, before + 0x8u, before_size + size);
                    merged(record);
                    return;
                }
            }
            if (!(static_cast<s32>(position) < static_cast<s32>(kept))) {
                append(record);
                return;
            }
            const u32 after = word(rt, frame.at(position << 2u));
            if (address + size == word(rt, after + 0x4u)) {
                const u32 after_size = word(rt, after + 0x8u);
                put(rt, after + 0x4u, address);
                put(rt, after + 0x8u, after_size + size);
                merged(record);
                return;
            }
            // insert: the pointers from `position` up move one place, the last one goes to the end
            u32 carry = record, slot = frame.at(position << 2u);
            for (u32 moved = 0u; moved != kept - position; ++moved, slot += 4u) {
                const u32 displaced = word(rt, slot);
                put(rt, slot, carry);
                carry = displaced;
            }
            ctx.gpr[16] = carry;
            append(carry);
        };
        for (;;) {
            process(cursor + 0xCu);
            if (!(static_cast<s32>(index) < static_cast<s32>(count))) break;
            cursor += 0x1Cu;
        }
    }
    ctx.gpr[2] = released;
}
void func_08B65728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); G[2] = static_cast<std::int32_t>(G[5]) < 0x8 ? 1u : 0u; G[20] = G[4] + 0x24u; put(rt, frame.at(0x14u), G[21]); G[21] = G[6] & 0xFFu; put(rt, frame.at(0xCu), G[19]); G[19] = 0x8u; put(rt, frame.at(0x8u), G[18]); G[18] = G[20]; put(rt, frame.at(0x4u), G[17]); G[17] = G[4]; put(rt, frame.at(0x0u), G[16]); G[16] = G[5]; { const bool tk = G[2] != 0u; put(rt, frame.at(0x18u), G[31]); if (tk) goto L_08B657D4; } G[2] = 0x7FFF0000u; G[2] = G[2] | 0xFFFFu; { const bool tk = G[5] == G[2]; G[19] = G[5] + 0x1u; if (tk) goto L_08B6580C; } G[4] = G[19] << 3u; call_here(rt, ctx, 0x08B6461Cu); G[18] = G[2]; G[2] = 0xFFFFFFFFu; if (G[16] == G[2]) { G[8] = word(rt, G[17] + 0x64u); goto L_08B657B0; } G[2] = G[18]; G[3] = 0u; L_08B65798: ; G[3] = G[3] + 0x1u; put(rt, G[2] + 0x0u, 0u); put(rt, G[2] + 0x4u, 0u); { const bool tk = G[19] != G[3]; G[2] = G[2] + 0x8u; if (tk) goto L_08B65798; } G[8] = word(rt, G[17] + 0x64u); L_08B657B0: ; { const bool tk = static_cast<std::int32_t>(G[8]) > 0; G[9] = word(rt, G[17] + 0x68u); if (tk) goto L_08B65858; } L_08B657B8: ; { const bool tk = G[9] == 0u; G[2] = G[16] << 3u; if (tk) goto L_08B657D8; } if (G[20] == G[9]) { put(rt, G[17] + 0x64u, G[19]); goto L_08B657DC; } G[4] = G[9]; call_here(rt, ctx, 0x08B61FC0u); put(rt, G[17] + 0x68u, 0u); L_08B657D4: ; G[2] = G[16] << 3u; L_08B657D8: ; put(rt, G[17] + 0x64u, G[19]); L_08B657DC: ; G[16] = G[18] + G[2]; put(rt, G[17] + 0x68u, G[18]); G[2] = G[16]; G[31] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; L_08B6580C: ; G[2] = word(rt, G[4] + 0x14u); G[3] = word(rt, G[4] + 0x10u); G[2] = G[2] | 0x1u; G[3] = G[2] & G[3]; { const bool tk = G[3] != 0u; put(rt, G[4] + 0x14u, G[2]); if (tk) goto L_08B65898; } { const bool tk = G[21] != 0u; G[16] = G[17] + 0x1Cu; if (tk) goto L_08B65890; } put(rt, G[4] + 0x1Cu, 0u); L_08B65830: ; G[2] = G[16]; G[31] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; L_08B65858: ; G[6] = G[18]; G[7] = 0u; G[5] = G[9]; L_08B65864: ; G[2] = word(rt, G[5] + 0x0u); G[3] = word(rt, G[5] + 0x4u); G[7] = G[7] + 0x1u; G[4] = static_cast<std::int32_t>(G[7]) < static_cast<std::int32_t>(G[8]) ? 1u : 0u; put(rt, G[6] + 0x0u, G[2]); G[5] = G[5] + 0x8u; put(rt, G[6] + 0x4u, G[3]); { const bool tk = G[4] != 0u; G[6] = G[6] + 0x8u; if (tk) goto L_08B65864; } goto L_08B657B8; L_08B65890: ; put(rt, G[4] + 0x20u, 0u); goto L_08B65830; L_08B65898: ; G[4] = high_half(0x08CB5BA8u); G[4] = 0x08CB5BA8u; call_here(rt, ctx, 0x08B5AA30u); goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B72030 (a0 = an unpacked float record: +0 the class, +4 the sign, +8 the exponent, +0x10 / +0x14 the 64-bit significand): packs it into an IEEE
// double, returned as v0 = low word, v1 = high word. Classes below 2 give a NaN (all-ones low word, fraction bits from the significand), 2 a zero,
// 4 an infinity; class 3 with a zero significand is a zero too. An exponent of 0x400 or more is an infinity; one below -0x3FE shifts the significand
// right by the difference (a part dropped is kept as a sticky bit, none of it when the shift is 0x39 or more) into a denormal; both round the significand's
// low byte to nearest-even (a low byte of 0x80 adds 0x80 only when bit 8 is set, any other adds 0x7F) before the low 8 bits are dropped. The significand
// is renormalised when the rounding carries past 0x1FFFFFFF in its high word.
void func_08B72030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 record = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}});
    u32 packed = 0u;
    const u32 kind = word(rt, record + 0x0u), low = word(rt, record + 0x10u), high = word(rt, record + 0x14u), sign = word(rt, record + 0x4u);
    ctx.gpr[3] = kind;
    ctx.gpr[10] = low;
    ctx.gpr[11] = high;
    ctx.gpr[25] = sign;
    // `ins` of a value into the packed double's high word: fraction (bits 0-19), exponent (20-30), sign (31)
    auto finish = [&](u32 fraction, u32 exponent, u32 result_low) {
        packed = ins(packed, fraction, 0u, 20u);
        packed = ins(packed, exponent, 20u, 11u);
        packed = ins(packed, sign, 31u, 1u);
        ctx.gpr[15] = packed;
        ctx.gpr[6] = result_low;
        ctx.gpr[2] = result_low;
        ctx.gpr[3] = packed;
    };
    if (kind < 2u) {
        finish((high | 0x7FFFFu) & 0xFFFFFu, 0x7FFu, 0xFFFFFFFFu);
        return;
    }
    if (kind == 4u) {
        finish(0u, 0x7FFu, 0u);
        return;
    }
    if (kind == 2u || (low | high) == 0u) {
        finish(0u, 0u, 0u);
        return;
    }
    const u32 exponent = word(rt, record + 0x8u);
    ctx.gpr[7] = exponent;
    if (static_cast<s32>(exponent) >= 0x400) {
        finish(0u, 0x7FFu, 0u);
        return;
    }
    // adds the rounding constant of the low byte (0x7F, or 0x80 for a tie with bit 8 set, or nothing for a tie without it) to a 64-bit significand
    auto round_up = [&](std::uint64_t value) {
        const u32 byte = static_cast<u32>(value) & 0xFFu;
        if (byte == 0x80u) return (static_cast<u32>(value) & 0x100u) == 0u ? value : value + 0x80u;
        return value + 0x7Fu;
    };
    if (static_cast<s32>(exponent) >= -0x3FE) {
        u32 biased = exponent + 0x3FFu;
        std::uint64_t value = round_up((static_cast<std::uint64_t>(high) << 32u) | low);
        if (0x1FFFFFFFu < static_cast<u32>(value >> 32u)) {
            value >>= 1u;
            biased = exponent + 0x400u;
        }
        const u32 value_high = static_cast<u32>(value >> 32u), value_low = static_cast<u32>(value);
        finish((value_high >> 8u) & 0xFFFFFu, biased & 0xFFFFu, (value_high << 24u) | (value_low >> 8u));
        return;
    }
    // below the normal range: shifted right into a denormal, with a sticky bit
    const u32 shift = static_cast<u32>(-0x3FE) - exponent;
    ctx.gpr[24] = shift;
    std::uint64_t value = 0u;
    if (static_cast<s32>(shift) < 0x39) {
        const std::uint64_t whole = (static_cast<std::uint64_t>(high) << 32u) | low;
        const std::uint64_t mask = (1ull << (shift & 63u)) - 1ull;
        const bool sticky = (whole & mask) != 0u;
        value = (whole >> (shift & 63u)) | (sticky ? 1u : 0u);
        ctx.gpr[16] = sticky ? 1u : 0u;
        ctx.gpr[17] = 0u;
    }
    value = round_up(value);
    const u32 value_high = static_cast<u32>(value >> 32u), value_low = static_cast<u32>(value);
    finish((value_high >> 8u) & 0xFFFFFu, 0x0FFFFFFFu < value_high ? 1u : 0u, (value_high << 24u) | (value_low >> 8u));
}
// 0x08B80990 (a0 = a year; v0 = 0 and nothing done below 1970): turns the two daylight-saving rules at 0x08CE11FC (0x1C bytes each) into times of that year.
// The days from 1970 to the start of the year are (y - 1969) / 4 + (y - 1970) x 365 - (y - 1901) / 100 + (y - 1601) / 400 (the divisions by multiplying
// by 0x51EB851F; the multiply-add leaves its 64-bit result in lo / hi); the year is remembered at 0x08D72E20. A rule whose type byte (+0) is 'J' (Julian day n,
// +0xC, with February 29 not counted: one day more in a leap year from day 60) or 'D' (day +0xC counting it) is a day number; any other is month +4, week +8, weekday +0xC: the
// first day of the month (the cumulative lengths of the month table 0x08C97588, one row of 12 words for a common year, the next for a leap year) gives the weekday of the first
// ((days + 4) mod 7; the multiply by 0x92492493), the week-th such weekday is found by stepping back from the month's end by 7. The rule's time (+0x14) is its +0x10 plus the day x 86400
// plus its +0x18. 0x08CE1234 gets 1 when the first rule's time is below the second's. v0 = 1.
void func_08B80990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 year = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
    ctx.gpr[2] = static_cast<s32>(year) < 0x7B2 ? 1u : 0u;
    ctx.gpr[12] = year;
    ctx.gpr[5] = 0u;
    if (static_cast<s32>(year) < 0x7B2) {
        ctx.gpr[2] = 0u;
        return;
    }
    auto mult = [&](u32 a, u32 b) {
        const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(a)) * static_cast<s32>(b);
        ctx.lo = static_cast<u32>(product);
        ctx.hi = static_cast<u32>(static_cast<std::uint64_t>(product) >> 32u);
        return ctx.hi;
    };
    // madd: hi:lo += a x b (signed)
    auto madd = [&](u32 a, u32 b) {
        const std::uint64_t accumulated = ((static_cast<std::uint64_t>(ctx.hi) << 32u) | ctx.lo) +
                                          static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<s32>(a)) * static_cast<s32>(b));
        ctx.lo = static_cast<u32>(accumulated);
        ctx.hi = static_cast<u32>(accumulated >> 32u);
    };
    const u32 magic = 0x51EB851Fu;
    const u32 since_1901 = year - 0x76Du, since_1601 = year - 0x641u, since_1969 = year - 0x7B1u, since_1970 = year - 0x7B2u;
    const u32 hi_year = mult(year, magic);
    const u32 hi_1901 = mult(since_1901, magic);
    const u32 hi_1601 = mult(since_1601, magic);
    u32 quarter_source = since_1969;
    if (static_cast<s32>(quarter_source) < 0) quarter_source = year - 0x7AEu;
    ctx.lo = sra(quarter_source, 2u);
    madd(since_1970, 0x16Du);
    const u32 sign_of_year = sra(year, 31u);
    const u32 centuries = sra(hi_year, 5u) - sign_of_year, quadcenturies = sra(hi_year, 7u) - sign_of_year;
    const u32 days = (ctx.lo - (sra(hi_1901, 5u) - sra(since_1901, 31u))) + (sra(hi_1601, 7u) - sra(since_1601, 31u));
    const u32 mod_400 = year - quadcenturies * 400u, mod_100 = year - centuries * 100u, mod_4 = year & 0x3u;
    put(rt, 0x08D72E20u, year);
    ctx.gpr[25] = days;
    ctx.gpr[14] = mod_400;
    ctx.gpr[15] = mod_100;
    ctx.gpr[9] = mod_4;
    const bool leap = mod_4 == 0u && (mod_100 != 0u || mod_400 == 0u);
    ctx.gpr[20] = 0x08C97588u;
    ctx.gpr[19] = 0x92492493u;
    ctx.gpr[16] = 0x15180u;
    ctx.gpr[17] = 0x08CE0000u;
    for (u32 entry = 0x08CE11FCu; entry != 0x08CE1234u; entry += 0x1Cu) {
        const u32 type = signed_byte(rt, entry);
        ctx.gpr[13] = entry;
        u32 day;
        if (type == 0x4Au) {
            const u32 number = word(rt, entry + 0xCu);
            day = days + number + (leap && !(static_cast<s32>(number) < 0x3C) ? 1u : 0u);
        } else if (type == 0x44u) {
            day = days + word(rt, entry + 0xCu);
        } else {
            const u32 month = word(rt, entry + 0x4u);
            const u32 table = 0x08C97588u + (leap ? 48u : 0u);
            ctx.gpr[24] = table;
            u32 start = days, count = 1u;
            if (!(static_cast<s32>(month) < 2)) {
                u32 at = table;
                do {
                    const u32 length = word(rt, at);
                    ++count;
                    at += 4u;
                    start += length;
                } while (month != count);
            }
            ctx.gpr[12] = start;
            const u32 first_day = start + 4u;
            const u32 quotient = sra(mult(first_day, 0x92492493u) + first_day, 2u) - sra(first_day, 31u);
            const u32 weekday = first_day - (quotient * 8u - quotient);
            const u32 target = word(rt, entry + 0xCu), week = word(rt, entry + 0x8u);
            u32 offset = target - weekday;
            if (static_cast<s32>(offset) < 0) offset += 7u;
            const u32 length = word(rt, table + (count << 2u));
            offset = offset + (week * 8u - week) - 7u;
            while (!(static_cast<s32>(offset) < static_cast<s32>(length))) offset -= 7u;
            day = offset + start;
        }
        ctx.gpr[4] = day;
        ctx.lo = word(rt, entry + 0x10u);
        madd(day, 0x15180u);
        put(rt, entry + 0x14u, ctx.lo + word(rt, entry + 0x18u));
    }
    const u32 first_time = word(rt, 0x08CE11FCu + 0x14u), second_time = word(rt, 0x08CE11FCu + 0x30u);
    put(rt, 0x08CE1234u, static_cast<s32>(first_time) < static_cast<s32>(second_time) ? 1u : 0u);
    ctx.gpr[5] = 1u;
    ctx.gpr[2] = 1u;
}
// 0x08B8BC08 (a0 = first, a1 = middle, a2 = last): a partial sort of 24-byte records ([+0] [+4] two words, [+8] a vector of 16-bit values: begin, end, capacity end) in the
// style of std::partial_sort, with the same shape as 0x08C36494. n = (middle - first) / 24 (the multiply by 0xAAAAAAAB). From 2 up the heap is made bottom up: a record is
// copied to the frame (its words at +0x20, the vector copy at +0x28: storage 0x08A5BC20(bytes, 4), elements 0x08B8A654) and put in place by
// 0x08B8B7B4(first, hole, n, &copy, vector capacity end); the copy is destroyed (0x08B8A644) and its storage freed (0x08A5BC54(begin, bytes, 4)). Then each record from middle
// to last for which 0x08B8A72C(record vector, first's vector) holds is exchanged with the first one (kept on the frame, +0x38 / +0x40; the first's words and vector
// go into the record, 0x08B8AD88) and sifted in with 0x08B8B7B4(first, 0, n, ...). 0x08B8B9E8(first, middle, s0) ends it.
void func_08B8BC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4], middle = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x90u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x80u}, {23u, 0x7Cu}, {31u, 0x84u}, {22u, 0x78u}, {21u, 0x74u}, {20u, 0x70u}, {19u, 0x6Cu}, {18u, 0x68u}, {17u, 0x64u}, {16u, 0x60u}});
    const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(sra(middle - first, 3u))) * static_cast<s32>(0xAAAAAAABu);
    ctx.lo = static_cast<u32>(product);
    ctx.hi = static_cast<u32>(static_cast<std::uint64_t>(product) >> 32u);
    const u32 count = ctx.lo;
    ctx.gpr[30] = count;
    ctx.gpr[23] = first;
    put(rt, frame.at(0x50u), middle);
    put(rt, frame.at(0x54u), last);
    ctx.gpr[2] = static_cast<s32>(count) < 2 ? 1u : 0u;
    // copies the vector of 16-bit values [begin, end) to a new one kept in the three words at `slot` (storage from 0x08A5BC20, filled by 0x08B8A654(begin, end, storage, &pad)); returns its capacity end
    auto copy_vector = [&](u32 slot, u32 begin, u32 end, u32 pad) {
        put(rt, slot + 0x0u, 0u);
        put(rt, slot + 0x4u, 0u);
        put(rt, slot + 0x8u, 0u);
        const u32 bytes = (end - begin) & ~1u;
        ctx.gpr[16] = bytes;
        const u32 storage = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
        const u32 capacity_end = storage + bytes;
        ctx.gpr[16] = capacity_end;
        put(rt, slot + 0x0u, storage);
        put(rt, slot + 0x4u, storage);
        put(rt, slot + 0x8u, capacity_end);
        const u32 filled = call_set(rt, ctx, 0x08B8A654u, {begin, end, storage, pad});
        put(rt, slot + 0x4u, filled);
        return capacity_end;
    };
    // destroys the copy at `slot` (0x08B8A644(begin, end, &pad)) and frees its storage
    auto drop_vector = [&](u32 slot, u32 pad) {
        call_set(rt, ctx, 0x08B8A644u, {word(rt, slot + 0x0u), word(rt, slot + 0x4u), pad});
        const u32 storage = word(rt, slot + 0x0u);
        const u32 bytes = sra(word(rt, slot + 0x8u) - storage, 1u) << 1u;
        if (storage != 0u) call_set(rt, ctx, 0x08A5BC54u, {storage, bytes, 4u});
    };
    if (!(static_cast<s32>(count) < 2)) {
        const u32 parent_sum = count - 2u;
        const u32 parent = sra((parent_sum >> 31u) + parent_sum, 1u);
        ctx.gpr[20] = parent;
        u32 record = first + parent * 24u;
        ctx.gpr[19] = record;
        ctx.gpr[21] = frame.at(0x28u);
        for (u32 done = 0u;; ++done) {
            ctx.gpr[22] = done;
            const u32 vector = record + 0x8u;
            put(rt, frame.at(0x24u), word(rt, record + 0x4u));
            put(rt, frame.at(0x20u), word(rt, record + 0x0u));
            const u32 begin = word(rt, record + 0x8u), end = word(rt, vector + 0x4u);
            const u32 capacity_end = copy_vector(frame.at(0x28u), begin, end, frame.at(0x5u));
            ctx.gpr[17] = vector;
            record -= 0x18u;
            ctx.gpr[19] = record;
            call_set(rt, ctx, 0x08B8B7B4u, {first, parent - done, count, frame.at(0x20u), capacity_end});
            drop_vector(frame.at(0x28u), frame.at(0x4u));
            if (done == parent) break;
        }
    }
    if (word(rt, frame.at(0x50u)) < word(rt, frame.at(0x54u))) {
        u32 record = word(rt, frame.at(0x50u));
        const u32 first_vector = first + 0x8u;
        ctx.gpr[19] = first_vector;
        do {
            ctx.gpr[17] = record;
            const u32 vector = record + 0x8u;
            ctx.gpr[18] = vector;
            if (call_set(rt, ctx, 0x08B8A72Cu, {word(rt, record + 0x8u), word(rt, record + 0xCu), word(rt, first + 0x8u), word(rt, first_vector + 0x4u)}) != 0u) {
                put(rt, frame.at(0x48u), 0u);
                put(rt, frame.at(0x3Cu), word(rt, record + 0x4u));
                put(rt, frame.at(0x38u), word(rt, record + 0x0u));
                const u32 begin = word(rt, record + 0x8u), end = word(rt, vector + 0x4u);
                put(rt, frame.at(0x40u), 0u);
                put(rt, frame.at(0x44u), 0u);
                const u32 bytes = (end - begin) & ~1u;
                ctx.gpr[16] = bytes;
                const u32 storage = call_set(rt, ctx, 0x08A5BC20u, {bytes, 4u});
                const u32 capacity_end = storage + bytes;
                ctx.gpr[16] = capacity_end;
                put(rt, frame.at(0x40u), storage);
                put(rt, frame.at(0x44u), storage);
                put(rt, frame.at(0x48u), capacity_end);
                const u32 filled = call_set(rt, ctx, 0x08B8A654u, {word(rt, record + 0x8u), word(rt, vector + 0x4u), storage, frame.at(0x3u)});
                put(rt, frame.at(0x44u), filled);
                const u32 w0 = word(rt, first + 0x0u), w4 = word(rt, first + 0x4u);
                put(rt, record + 0x0u, w0);
                put(rt, record + 0x4u, w4);
                if (vector != first_vector) call_set(rt, ctx, 0x08B8AD88u, {vector, first_vector});
                const u32 kept_low = word(rt, frame.at(0x38u)), kept_high = word(rt, frame.at(0x3Cu));
                put(rt, frame.at(0xCu), kept_high);
                put(rt, frame.at(0x8u), kept_low);
                const u32 capacity = copy_vector(frame.at(0x10u), word(rt, frame.at(0x40u)), word(rt, frame.at(0x44u)), frame.at(0x2u));
                call_set(rt, ctx, 0x08B8B7B4u, {first, 0u, count, frame.at(0x8u), capacity});
                drop_vector(frame.at(0x10u), frame.at(0x1u));
                drop_vector(frame.at(0x40u), frame.at(0x0u));
            }
            record += 0x18u;
            ctx.gpr[17] = record;
        } while (record < word(rt, frame.at(0x54u)));
    }
    call_set(rt, ctx, 0x08B8B9E8u, {first, word(rt, frame.at(0x50u)), ctx.gpr[16]});
}
void func_08B95C50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x0u), G[16]); G[16] = high_half(0x08D75088u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); G[2] = sext(load8(0x08D75088u), 8u); { const bool tk = G[2] != 0u; G[4] = 0x08D75088u; if (tk) goto L_08B95CE0; } call_here(rt, ctx, 0x08B6441Cu); { const bool tk = G[2] == 0u; G[4] = high_half(0x08D750F0u); if (tk) goto L_08B95CE0; } G[2] = sext(load8(0x08D750F0u), 8u); G[17] = 0x08D750F0u; G[4] = G[17]; { const bool tk = G[2] != 0u; G[18] = high_half(0x08D752A8u); if (tk) goto L_08B95CCC; } call_here(rt, ctx, 0x08B6441Cu); { const bool tk = G[2] == 0u; G[4] = G[17]; if (tk) goto L_08B95CCC; } G[3] = high_half(0x08CBF670u); G[5] = high_half(0x08D752B4u); G[2] = G[18] + sext(0x08D752A8u, 16u); G[3] = 0x08CBF670u; G[5] = 0x08D752B4u; put(rt, G[2] + 0x8u, G[3]); put(rt, G[2] + 0x4u, G[5]); put(rt, G[18] + sext(0x08D752A8u, 16u), 0u); call_here(rt, ctx, 0x08B64494u); L_08B95CCC: ; G[3] = G[18] + sext(0x08D752A8u, 16u); G[2] = high_half(0x08D75330u); G[4] = G[16] + sext(0x08D75088u, 16u); put(rt, 0x08D75330u, G[3]); call_here(rt, ctx, 0x08B64494u); L_08B95CE0: ; G[2] = high_half(0x08D75080u); G[3] = sext(load8(0x08D75080u), 8u); G[16] = 0x08D75080u; G[17] = high_half(0x08D75324u); { const bool tk = G[3] != 0u; G[4] = G[16]; if (tk) goto L_08B95D2C; } call_here(rt, ctx, 0x08B6441Cu); { const bool tk = G[2] == 0u; G[4] = G[16]; if (tk) goto L_08B95D2C; } G[3] = high_half(0x08CBF930u); G[5] = high_half(0x08D75330u); G[2] = G[17] + sext(0x08D75324u, 16u); G[3] = 0x08CBF930u; G[5] = 0x08D75330u; put(rt, G[2] + 0x8u, G[3]); put(rt, G[2] + 0x4u, G[5]); put(rt, G[17] + sext(0x08D75324u, 16u), 0u); call_here(rt, ctx, 0x08B64494u); L_08B95D2C: ; G[2] = G[17] + sext(0x08D75324u, 16u); G[31] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08BB2B00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); G[30] = G[5]; put(rt, frame.at(0x3Cu), G[23]); G[23] = G[4]; put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x28u), G[18]); G[18] = G[5]; put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); G[2] = word(rt, G[4] + 0x4u); put(rt, frame.at(0x10u), G[6]); G[2] = G[2] - G[6]; G[22] = sra(G[2], 2u); G[2] = G[4] + 0x4u; { const bool tk = static_cast<std::int32_t>(G[22]) <= 0; put(rt, frame.at(0x14u), G[2]); if (tk) goto L_08BB2C9C; } G[19] = G[6]; G[21] = 0u; L_08BB2B5C: ; G[3] = word(rt, G[19] + 0x0u); if (G[3] == 0u) { G[16] = word(rt, G[18] + 0x0u); goto L_08BB2B78; } G[2] = word(rt, G[3] + 0x10u); G[2] = G[2] + 0x1u; put(rt, G[3] + 0x10u, G[2]); G[16] = word(rt, G[18] + 0x0u); L_08BB2B78: ; { const bool tk = G[16] == 0u; put(rt, G[18] + 0x0u, G[3]); if (tk) goto L_08BB2C24; } G[2] = word(rt, G[16] + 0x10u); G[2] = G[2] - 0x1u; { const bool tk = G[2] != 0u; put(rt, G[16] + 0x10u, G[2]); if (tk) goto L_08BB2C24; } G[20] = word(rt, G[16] + 0xCu); call_here(rt, ctx, 0x08BB1F48u); G[3] = 0x4u; put(rt, frame.at(0x4u), G[3]); G[4] = word(rt, G[2] + 0x34u); { const bool tk = G[4] == 0u; G[17] = G[2]; if (tk) goto L_08BB2C3C; } G[2] = word(rt, G[4] + 0x10u); if (G[2] != G[3]) { G[4] = word(rt, G[17] + 0x24u); goto L_08BB2C40; } goto L_08BB2C5C; L_08BB2BC0: ; G[3] = word(rt, G[4] + 0xCu); G[2] = G[2] - 0x1u; put(rt, G[4] + 0x1Cu, G[2]); put(rt, G[20] + 0x0u, G[3]); put(rt, G[4] + 0xCu, G[20]); G[4] = G[16]; L_08BB2BD8: ; put(rt, G[16] + 0xCu, 0u); call_here(rt, ctx, 0x08BB1FF0u); call_here(rt, ctx, 0x08BB1F48u); G[3] = 0x14u; put(rt, frame.at(0x4u), G[3]); G[4] = word(rt, G[2] + 0x34u); { const bool tk = G[4] == 0u; G[17] = G[2]; if (tk) goto L_08BB2C6C; } G[2] = word(rt, G[4] + 0x10u); if (G[2] != G[3]) { G[4] = word(rt, G[17] + 0x24u); goto L_08BB2C70; } goto L_08BB2C8C; L_08BB2C10: ; G[3] = word(rt, G[4] + 0xCu); G[2] = G[2] - 0x1u; put(rt, G[4] + 0x1Cu, G[2]); put(rt, G[16] + 0x0u, G[3]); put(rt, G[4] + 0xCu, G[16]); L_08BB2C24: ; G[21] = G[21] + 0x1u; L_08BB2C28: ; G[19] = G[19] + 0x4u; { const bool tk = G[22] != G[21]; G[18] = G[18] + 0x4u; if (tk) goto L_08BB2B5C; } G[3] = word(rt, frame.at(0x14u)); goto L_08BB2CA0; L_08BB2C3C: ; G[4] = word(rt, G[17] + 0x24u); L_08BB2C40: ; G[5] = word(rt, G[17] + 0x28u); G[3] = high_half(0x08BB1B40u); G[6] = frame.at(0x4u); G[7] = 0x08BB1B40u; call_here(rt, ctx, 0x08BB1D0Cu); G[4] = word(rt, G[2] + 0x0u); put(rt, G[17] + 0x34u, G[4]); L_08BB2C5C: ; if (G[20] != 0u) { G[2] = word(rt, G[4] + 0x1Cu); goto L_08BB2BC0; } G[4] = G[16]; goto L_08BB2BD8; L_08BB2C6C: ; G[4] = word(rt, G[17] + 0x24u); L_08BB2C70: ; G[5] = word(rt, G[17] + 0x28u); G[2] = high_half(0x08BB1B40u); G[6] = frame.at(0x4u); G[7] = 0x08BB1B40u; call_here(rt, ctx, 0x08BB1D0Cu); G[4] = word(rt, G[2] + 0x0u); put(rt, G[17] + 0x34u, G[4]); L_08BB2C8C: ; if (G[16] != 0u) { G[2] = word(rt, G[4] + 0x1Cu); goto L_08BB2C10; } G[21] = G[21] + 0x1u; goto L_08BB2C28; L_08BB2C9C: ; G[3] = word(rt, frame.at(0x14u)); L_08BB2CA0: ; G[4] = G[18]; G[6] = G[29]; G[5] = word(rt, G[3] + 0x0u); call_here(rt, ctx, 0x08BB24B0u); G[4] = word(rt, frame.at(0x10u)); G[3] = word(rt, G[23] + 0x4u); G[2] = G[4] - G[30]; G[2] = ins(G[2], 0u, 0u, 2u); G[3] = G[3] - G[2]; put(rt, G[23] + 0x4u, G[3]); G[2] = G[30]; G[31] = word(rt, frame.at(0x44u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08C1FC90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); G[18] = G[5]; put(rt, frame.at(0x14u), G[17]); G[17] = G[6]; put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[7] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[4] + 0x8u); { const bool tk = G[7] == G[2]; G[16] = G[4]; if (tk) goto L_08C1FD7C; } { const bool tk = G[7] == 0u; G[3] = G[7] - 0xCu; if (tk) goto L_08C1FCE8; } G[2] = word(rt, G[3] + 0x8u); G[4] = word(rt, G[3] + 0x4u); G[3] = word(rt, G[7] - 0xCu); put(rt, G[7] + 0x8u, G[2]); put(rt, G[7] + 0x0u, G[3]); put(rt, G[7] + 0x4u, G[4]); G[7] = word(rt, G[16] + 0x4u); L_08C1FCE8: ; G[5] = G[7] - 0xCu; G[3] = G[5] - G[18]; G[2] = 0xAAAA0000u; G[3] = sra(G[3], 2u); G[2] = G[2] | 0xAAABu; { const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[3])) * static_cast<std::int32_t>(G[2]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); } G[7] = G[7] + 0xCu; put(rt, G[16] + 0x4u, G[7]); G[8] = 0u; G[7] = G[5]; G[2] = word(rt, G[17] + 0x8u); G[3] = word(rt, G[17] + 0x0u); G[4] = word(rt, G[17] + 0x4u); G[6] = ctx.lo; put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x4u), G[3]); { const bool tk = static_cast<std::int32_t>(G[6]) > 0; put(rt, frame.at(0x8u), G[4]); if (tk) goto L_08C1FD3C; } G[4] = word(rt, frame.at(0x8u)); goto L_08C1FD64; L_08C1FD38: ; G[5] = G[5] - 0xCu; L_08C1FD3C: ; G[7] = G[7] - 0xCu; G[2] = word(rt, G[7] + 0x0u); G[3] = word(rt, G[7] + 0x4u); G[4] = word(rt, G[7] + 0x8u); G[8] = G[8] + 0x1u; put(rt, G[5] + 0x0u, G[2]); put(rt, G[5] + 0x4u, G[3]); { const bool tk = G[6] != G[8]; put(rt, G[5] + 0x8u, G[4]); if (tk) goto L_08C1FD38; } G[4] = word(rt, frame.at(0x8u)); L_08C1FD64: ; G[3] = word(rt, frame.at(0xCu)); G[2] = word(rt, frame.at(0x4u)); put(rt, G[18] + 0x4u, G[4]); put(rt, G[18] + 0x0u, G[2]); put(rt, G[18] + 0x8u, G[3]); goto L_08C1FE70; L_08C1FD7C: ; G[2] = word(rt, G[4] + 0x0u); G[3] = 0xAAAA0000u; G[3] = G[3] | 0xAAABu; G[2] = G[7] - G[2]; G[2] = sra(G[2], 2u); { const auto product = static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[3]); ctx.lo = static_cast<std::uint32_t>(product); ctx.hi = static_cast<std::uint32_t>(static_cast<std::uint64_t>(product) >> 32); } G[4] = 0x15550000u; G[4] = G[4] | 0x5555u; G[2] = ctx.lo; { const bool tk = G[2] != G[4]; G[4] = high_half(0x08CB026Cu); if (tk) goto L_08C1FDB0; } G[4] = 0x08CB026Cu; call_here(rt, ctx, 0x08B5ADDCu); L_08C1FDB0: ; { const bool tk = G[2] == 0u; G[4] = 0x1u; if (tk) goto L_08C1FDC8; } G[4] = G[2] << 1u; G[2] = G[4] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFCu; if (tk) goto L_08C1FDD4; } L_08C1FDC8: ; G[3] = G[4] << 4u; G[2] = G[4] << 2u; G[20] = G[3] - G[2]; L_08C1FDD4: ; G[4] = G[20]; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = word(rt, G[16] + 0x0u); G[6] = G[2]; G[5] = G[18]; G[7] = frame.at(0x2u); G[19] = G[2]; call_here(rt, ctx, 0x08C1FC50u); { const bool tk = G[2] == 0u; G[6] = G[2]; if (tk) goto L_08C1FE18; } G[2] = word(rt, G[17] + 0x8u); G[3] = word(rt, G[17] + 0x0u); G[4] = word(rt, G[17] + 0x4u); put(rt, G[6] + 0x8u, G[2]); put(rt, G[6] + 0x0u, G[3]); put(rt, G[6] + 0x4u, G[4]); L_08C1FE18: ; G[5] = word(rt, G[16] + 0x4u); G[4] = G[18]; G[6] = G[6] + 0xCu; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C1FC50u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = G[29]; G[17] = G[2]; call_here(rt, ctx, 0x08C1FC40u); G[4] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[16] + 0x8u); G[2] = G[2] - G[4]; { const bool tk = G[4] == 0u; G[2] = sra(G[2], 2u); if (tk) goto L_08C1FE60; } G[5] = G[2] << 2u; G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08C1FE60: ; G[2] = G[19] + G[20]; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C1FE70: ; G[31] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08C2EF40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); G[2] = G[5] - G[4]; G[2] = sra(G[2], 2u); G[3] = static_cast<std::int32_t>(G[2]) < 0x11 ? 1u : 0u; put(rt, frame.at(0x40u), G[30]); G[30] = G[6]; put(rt, frame.at(0x34u), G[21]); G[21] = G[5]; put(rt, frame.at(0x2Cu), G[19]); G[19] = G[4]; put(rt, frame.at(0x24u), G[17]); G[17] = G[5]; put(rt, frame.at(0x20u), G[16]); G[16] = G[7]; put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x28u), G[18]); { const bool tk = G[3] == 0u; put(rt, frame.at(0x10u), G[4]); if (tk) goto L_08C2F0F0; } G[31] = word(rt, frame.at(0x44u)); goto L_08C2F104; L_08C2EF9C: ; if (G[30] != G[23]) { G[4] = word(rt, G[16] + 0x0u); goto L_08C2EFC4; } G[5] = G[21]; L_08C2EFA8: ; G[4] = G[19]; G[7] = G[16]; G[6] = G[21]; call_here(rt, ctx, 0x08C2EDC4u); G[31] = word(rt, frame.at(0x44u)); goto L_08C2F104; L_08C2EFC0: ; G[4] = word(rt, G[16] + 0x0u); L_08C2EFC4: ; G[3] = G[2] >> 31u; G[3] = G[3] + G[2]; G[3] = sra(G[3], 1u); G[3] = G[3] << 2u; G[2] = word(rt, G[4] + 0x8u); G[18] = G[3] + G[19]; G[4] = G[16]; G[5] = G[19]; { const std::uint32_t callee = G[2]; G[6] = G[18]; call_here(rt, ctx, callee); } G[17] = G[17] - 0x4u; G[22] = G[22] - 0x1u; G[20] = G[19]; G[4] = G[16]; G[5] = G[18]; { const bool tk = G[2] == 0u; G[6] = G[17]; if (tk) goto L_08C2F048; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } G[4] = G[16]; G[5] = G[19]; { const bool tk = G[2] != 0u; G[6] = G[17]; if (tk) goto L_08C2F088; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } if (G[2] != 0u) { G[20] = G[17]; goto L_08C2F08C; } G[5] = G[20]; goto L_08C2F090; L_08C2F048: ; G[3] = word(rt, G[16] + 0x0u); G[4] = G[16]; G[5] = G[19]; G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2]; G[6] = G[17]; call_here(rt, ctx, callee); } G[4] = G[16]; G[5] = G[18]; { const bool tk = G[2] != 0u; G[6] = G[17]; if (tk) goto L_08C2F08C; } G[3] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } if (G[2] != 0u) { G[20] = G[17]; goto L_08C2F08C; } L_08C2F088: ; G[20] = G[18]; L_08C2F08C: ; G[5] = G[20]; L_08C2F090: ; G[4] = G[29]; call_here(rt, ctx, 0x08A76ABCu); G[5] = G[21]; G[6] = G[29]; G[7] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x08C2E994u); G[17] = G[2]; G[4] = G[29]; call_here(rt, ctx, 0x08A5B9ECu); G[5] = G[21]; G[4] = G[17]; G[6] = G[22]; G[7] = G[16]; call_here(rt, ctx, 0x08C2EF40u); G[3] = word(rt, frame.at(0x10u)); G[23] = G[23] + 0x1u; G[2] = G[17] - G[3]; G[2] = sra(G[2], 2u); G[3] = static_cast<std::int32_t>(G[2]) < 0x11 ? 1u : 0u; { const bool tk = G[3] == 0u; G[21] = G[17]; if (tk) goto L_08C2EF9C; } G[31] = word(rt, frame.at(0x44u)); goto L_08C2F104; L_08C2F0F0: ; G[22] = G[6]; { const bool tk = G[6] != 0u; G[23] = 0u; if (tk) goto L_08C2EFC0; } G[5] = G[21]; goto L_08C2EFA8; L_08C2F104: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
// 0x08C36494 (a0 = first, a1 = middle, a2 = last): a partial sort of 12-byte records ([+0] a word, [+4] a counted string, [+8] a byte; strings copied by 0x08AAE214,
// released by 0x08A5B9EC) in the style of std::partial_sort. The count n = (middle - first) / 12 (the multiply by 0xAAAAAAAB); from 2 up the heap is made
// bottom up (the record at each parent index goes through 0x08C36114(first, hole, n, &value, record) with its string copied to a temporary).
// Then every record from middle to last whose key (the string's +0x14, a null string calls 0x08A5BB7C(0x08CB39CC)) is below the first record's, or
// equal with the word below it by 0x08B35FD8, is exchanged with the first (value kept on the frame) and sifted in with 0x08C36114(first, 0, n, &value, ...).
// 0x08C36358(first, middle, s0) ends it, with s0 as the last loop left it.
void func_08C36494(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4], middle = ctx.gpr[5], last = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x44u}, {30u, 0x50u}, {23u, 0x4Cu}, {20u, 0x40u}, {31u, 0x54u}, {22u, 0x48u}, {19u, 0x3Cu}, {18u, 0x38u}, {17u, 0x34u}, {16u, 0x30u}});
    const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(sra(middle - first, 2u))) * static_cast<s32>(0xAAAAAAABu);
    ctx.lo = static_cast<u32>(product);
    ctx.hi = static_cast<u32>(static_cast<std::uint64_t>(product) >> 32u);
    const u32 count = ctx.lo;
    ctx.gpr[21] = count;
    ctx.gpr[30] = last;
    ctx.gpr[23] = middle;
    ctx.gpr[20] = first;
    ctx.gpr[2] = static_cast<s32>(count) < 2 ? 1u : 0u;
    const u32 temp = frame.at(0x10u), value_slot = frame.at(0xCu), byte_slot = frame.at(0x14u);
    if (!(static_cast<s32>(count) < 2)) {
        const u32 parent_sum = count - 2u;
        const u32 parent = sra((parent_sum >> 31u) + parent_sum, 1u);
        ctx.gpr[18] = parent;
        u32 record = first + parent * 12u;
        ctx.gpr[16] = record;
        ctx.gpr[19] = temp;
        ctx.gpr[22] = value_slot;
        for (u32 done = 0u;; ++done) {
            ctx.gpr[17] = done;
            put(rt, value_slot, word(rt, record + 0x0u));
            call_set(rt, ctx, 0x08AAE214u, {temp, record + 0x4u});
            store8(byte_slot, load8(record + 0x8u));
            ctx.gpr[8] = record;
            call_set(rt, ctx, 0x08C36114u, {first, parent - done, count, value_slot, record});
            call_set(rt, ctx, 0x08A5B9ECu, {temp});
            record -= 0xCu;
            ctx.gpr[16] = record;
            if (done == parent) break;
        }
    }
    // the key of `element`'s string (s1) and of the first record's (the value read after the null checks)
    auto keys = [&](u32 element, u32 &element_key, u32 &first_key) {
        const u32 text = word(rt, element + 0x4u);
        ctx.gpr[17] = text;
        if (text == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
        const u32 head = word(rt, first + 0x4u);
        ctx.gpr[16] = head;
        element_key = word(rt, text + 0x14u);
        ctx.gpr[17] = element_key;
        if (head == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB39CCu});
        first_key = word(rt, head + 0x14u);
    };
    if (middle < last) {
        const u32 copy = frame.at(0x1Cu), hold = frame.at(0x18u), hold_byte = frame.at(0x20u);
        u32 element = middle, current = middle;
        ctx.gpr[18] = element;
        ctx.gpr[19] = current;
        ctx.gpr[22] = copy;
        do {
            bool exchange = false;
            u32 element_key, first_key;
            keys(element, element_key, first_key);
            if (static_cast<s32>(element_key) < static_cast<s32>(first_key)) {
                exchange = true;
            } else {
                keys(element, element_key, first_key);
                if (!(static_cast<s32>(first_key) < static_cast<s32>(element_key))) {
                    const u32 order = call_set(rt, ctx, 0x08B35FD8u, {word(rt, element), word(rt, first)});
                    exchange = static_cast<s32>(order) < 0;
                }
            }
            if (exchange) {
                const u32 head_word = word(rt, current);
                const u32 record = current + 0x4u;
                ctx.gpr[16] = record;
                put(rt, hold, head_word);
                call_set(rt, ctx, 0x08AAE214u, {copy, record});
                const u32 first_text = first + 0x4u;
                const u32 byte_value = load8(current + 0x8u);
                const u32 first_word = word(rt, first);
                store8(hold_byte, byte_value);
                put(rt, current, first_word);
                if (record != first_text) call_set(rt, ctx, 0x08A5BAD8u, {record, word(rt, first_text)});
                store8(current + 0x8u, load8(first + 0x8u));
                put(rt, frame.at(0x0u), word(rt, hold));
                call_set(rt, ctx, 0x08AAE214u, {frame.at(0x4u), copy});
                store8(frame.at(0x8u), load8(hold_byte));
                call_set(rt, ctx, 0x08C36114u, {first, 0u, count, frame.at(0x0u), record});
                call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
                call_set(rt, ctx, 0x08A5B9ECu, {copy});
                element = current + 0xCu;
            } else {
                element = current + 0xCu;
            }
            ctx.gpr[18] = element;
            ctx.gpr[19] = element;
            current = element;
        } while (element < last);
    }
    call_set(rt, ctx, 0x08C36358u, {first, middle, ctx.gpr[16]});
}
// 0x08C47924 (a0 = the vector, a1 = the position, a2 = the value): the single-element insert of a std::vector of 0x54-byte elements ([+0] begin, [+4] end, [+8] capacity end),
// the same shape as 0x08C1FC90. With room: the last element is copied to the end (when there is one), the value is kept on the frame (+4), the elements from the position up
// move one place up, last first (the count by the multiply by 0x3CF3CF3D) and the value is copied to the position. Full: the new size is the old one doubled (1 when empty,
// 0x030C30C3 elements being the limit, 0x08B5ADDC(0x08CB4AD4)), the storage comes from 0x08B35DA0(bytes), the elements before the position are copied there
// (0x08C47034(begin, position, storage, &frame +2)), the value is copied in, the ones after follow (0x08C47034(position, end, ...)), the old ones are destroyed
// (0x08C47024) and the old storage freed (0x08B35254). v0 = the new storage end when it grew, else the last word of the value.
void func_08C47924(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x6Cu}, {18u, 0x68u}, {17u, 0x64u}, {16u, 0x60u}, {31u, 0x78u}, {21u, 0x74u}, {20u, 0x70u}});
    ctx.gpr[19] = position;
    ctx.gpr[18] = value;
    ctx.gpr[17] = position;
    ctx.gpr[16] = self;
    // a 0x54-byte element, as the original moves it (0x50 bytes in 16-byte groups, then the last word)
    auto copy_element = [&](u32 to, u32 from) {
        for (u32 offset = 0u; offset <= 0x50u; offset += 4u) put(rt, to + offset, word(rt, from + offset));
    };
    // the number of elements in a byte distance: (distance >> 2) x 0x3CF3CF3D (the multiply leaves its 64-bit product in lo / hi)
    auto elements = [&](u32 distance) {
        const std::int64_t product = static_cast<std::int64_t>(static_cast<s32>(sra(distance, 2u))) * static_cast<s32>(0x3CF3CF3Du);
        ctx.lo = static_cast<u32>(product);
        ctx.hi = static_cast<u32>(static_cast<std::uint64_t>(product) >> 32u);
        return ctx.lo;
    };
    const u32 end = word(rt, self + 0x4u), capacity_end = word(rt, self + 0x8u);
    ctx.gpr[3] = end;
    ctx.gpr[2] = capacity_end;
    if (end != capacity_end) {
        u32 top = end;
        if (end != 0u) {
            copy_element(end, end - 0x54u);
            top = word(rt, self + 0x4u);
        }
        const u32 kept = frame.at(0x4u);
        put(rt, self + 0x4u, top + 0x54u);
        copy_element(kept, value);
        u32 last = top - 0x54u;
        const u32 count = elements(last - position);
        u32 from = last;
        if (static_cast<s32>(count) > 0) {
            for (u32 done = 1u;; ++done, last -= 0x54u) {
                from -= 0x54u;
                copy_element(last, from);
                if (done == count) break;
            }
        }
        copy_element(position, kept);
        ctx.gpr[2] = word(rt, kept + 0x50u);
        return;
    }
    const u32 begin = word(rt, self + 0x0u);
    const u32 size = elements(end - begin);
    ctx.gpr[2] = size;
    if (size == 0x030C30C3u) call_set(rt, ctx, 0x08B5ADDCu, {0x08CB4AD4u});
    u32 storage = 0u, bytes = 0u;
    bool allocate = true;
    if (size == 0u) {
        bytes = 84u;
    } else {
        const u32 doubled = size << 1u;
        ctx.gpr[3] = doubled;
        if (doubled < size) {
            bytes = 0xFFFFFFFCu;
        } else if (doubled == 0u) {
            allocate = false;
        } else {
            bytes = doubled * 84u;
        }
    }
    ctx.gpr[21] = bytes;
    ctx.gpr[20] = storage;
    u32 old_begin = begin;
    if (allocate) {
        storage = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[20] = storage;
        old_begin = word(rt, self + 0x0u);
    }
    u32 filled = call_set(rt, ctx, 0x08C47034u, {old_begin, position, storage, frame.at(0x2u)});
    ctx.gpr[8] = filled;
    if (filled != 0u) copy_element(filled, value);
    filled = call_set(rt, ctx, 0x08C47034u, {ctx.gpr[19], word(rt, self + 0x4u), filled + 0x54u, frame.at(0x1u)});
    ctx.gpr[17] = filled;
    call_set(rt, ctx, 0x08C47024u, {word(rt, self + 0x0u), word(rt, self + 0x4u), frame.at(0x0u)});
    const u32 old = word(rt, self + 0x0u);
    if (old != 0u) call_set(rt, ctx, 0x08B35254u, {old});
    ctx.gpr[2] = storage + bytes;
    put(rt, self + 0x8u, storage + bytes);
    put(rt, self + 0x4u, filled);
    put(rt, self + 0x0u, storage);
}
void func_08C52070(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); G[22] = G[5]; put(rt, frame.at(0x14u), G[21]); G[21] = G[4]; put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); { const bool tk = G[4] == G[5]; put(rt, frame.at(0x0u), G[16]); if (tk) goto L_08C5225C; } G[19] = word(rt, G[4] + 0x0u); { const bool tk = G[19] != G[4]; G[18] = word(rt, G[5] + 0x0u); if (tk) goto L_08C5224C; } G[17] = G[18]; goto L_08C5223C; L_08C520B4: ; G[3] = load8(G[18] + 0x8u); G[4] = word(rt, G[4] + 0x4u); G[17] = G[19] + 0x14u; G[20] = G[18] + 0x14u; store8(G[19] + 0x8u, G[3]); put(rt, G[16] + 0x4u, G[4]); { const bool tk = G[17] == G[20]; put(rt, G[16] + 0x8u, G[2]); if (tk) goto L_08C5213C; } G[4] = word(rt, G[16] + 0xCu); if (G[4] == 0u) { G[4] = word(rt, G[20] + 0x4u); goto L_08C520EC; } call_here(rt, ctx, 0x08B61FC0u); G[4] = word(rt, G[20] + 0x4u); L_08C520EC: ; put(rt, G[16] + 0xCu, 0u); { const bool tk = static_cast<std::int32_t>(G[4]) <= 0; put(rt, G[17] + 0x4u, G[4]); if (tk) goto L_08C5213C; } call_here(rt, ctx, 0x08B6461Cu); G[3] = word(rt, G[17] + 0x4u); { const bool tk = static_cast<std::int32_t>(G[3]) <= 0; put(rt, G[16] + 0xCu, G[2]); if (tk) goto L_08C5213C; } G[5] = 0u; G[2] = word(rt, G[20] + 0x0u); L_08C52114: ; G[3] = word(rt, G[17] + 0x0u); G[2] = G[2] + G[5]; G[4] = load8(G[2] + 0x0u); G[3] = G[3] + G[5]; G[5] = G[5] + 0x1u; store8(G[3] + 0x0u, G[4]); G[2] = word(rt, G[17] + 0x4u); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[20] + 0x0u); goto L_08C52114; } L_08C5213C: ; G[19] = word(rt, G[19] + 0x0u); L_08C52140: ; { const bool tk = G[21] == G[19]; G[18] = word(rt, G[18] + 0x0u); if (tk) goto L_08C52238; } { const bool tk = G[18] == G[22]; G[16] = G[19] + 0x8u; if (tk) goto L_08C52164; } L_08C52150: ; G[4] = G[18] + 0x8u; if (G[16] != G[4]) { G[2] = word(rt, G[4] + 0x8u); goto L_08C520B4; } G[19] = word(rt, G[19] + 0x0u); goto L_08C52140; L_08C52164: ; { const bool tk = G[21] == G[19]; G[2] = G[19]; if (tk) goto L_08C5225C; } G[5] = G[2]; L_08C52170: ; G[4] = G[21]; call_here(rt, ctx, 0x08C5201Cu); { const bool tk = G[21] != G[2]; G[5] = G[2]; if (tk) goto L_08C52170; } G[2] = G[21]; goto L_08C52260; L_08C52188: ; G[4] = 0x1Cu; call_here(rt, ctx, 0x08B35DA0u); G[5] = G[2] + 0x8u; G[18] = G[2]; { const bool tk = G[5] == 0u; G[3] = G[17] + 0x8u; if (tk) goto L_08C52218; } G[4] = load8(G[17] + 0x8u); G[2] = word(rt, G[3] + 0x8u); G[3] = word(rt, G[3] + 0x4u); G[16] = G[18] + 0x14u; put(rt, G[5] + 0x8u, G[2]); G[17] = G[17] + 0x14u; put(rt, G[5] + 0x4u, G[3]); store8(G[18] + 0x8u, G[4]); put(rt, G[16] + 0x4u, 0u); G[4] = word(rt, G[17] + 0x4u); put(rt, G[18] + 0x14u, 0u); { const bool tk = static_cast<std::int32_t>(G[4]) <= 0; put(rt, G[16] + 0x4u, G[4]); if (tk) goto L_08C52218; } call_here(rt, ctx, 0x08B6461Cu); G[3] = word(rt, G[16] + 0x4u); { const bool tk = static_cast<std::int32_t>(G[3]) <= 0; put(rt, G[18] + 0x14u, G[2]); if (tk) goto L_08C52218; } G[5] = 0u; G[2] = word(rt, G[17] + 0x0u); L_08C521F0: ; G[3] = word(rt, G[16] + 0x0u); G[2] = G[2] + G[5]; G[4] = load8(G[2] + 0x0u); G[3] = G[3] + G[5]; G[5] = G[5] + 0x1u; store8(G[3] + 0x0u, G[4]); G[2] = word(rt, G[16] + 0x4u); G[2] = static_cast<std::int32_t>(G[5]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[17] + 0x0u); goto L_08C521F0; } L_08C52218: ; G[4] = G[18]; G[5] = G[21]; call_here(rt, ctx, 0x08B5C078u); G[20] = word(rt, G[20] + 0x0u); { const bool tk = G[22] != G[20]; G[17] = G[20]; if (tk) goto L_08C52188; } G[2] = G[21]; goto L_08C52260; L_08C52238: ; G[17] = G[18]; L_08C5223C: ; { const bool tk = G[22] != G[18]; G[20] = G[18]; if (tk) goto L_08C52188; } goto L_08C52164; L_08C5224C: ; { const bool tk = G[18] != G[5]; G[16] = G[19] + 0x8u; if (tk) goto L_08C52150; } G[17] = G[18]; goto L_08C5223C; L_08C5225C: ; G[2] = G[21]; L_08C52260: ; G[31] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); goto exit_; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08C58450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[16]); G[16] = G[4]; G[6] = 0u; put(rt, frame.at(0x44u), G[17]); G[4] = G[29]; G[17] = G[5]; put(rt, frame.at(0x48u), G[31]); G[5] = G[16]; call_here(rt, ctx, 0x08C55FDCu); G[2] = load8(frame.at(0x0u)); { const bool tk = G[2] == 0u; G[2] = G[16]; if (tk) goto L_08C584FC; } G[3] = word(rt, G[16] + 0x0u); put(rt, frame.at(0x4u), 0u); G[2] = word(rt, G[3] - 0xCu); G[7] = G[16] + G[2]; G[4] = word(rt, G[7] + 0x84u); { const bool tk = G[4] == 0u; G[3] = 0xFFFFFFFFu; if (tk) goto L_08C58568; } G[2] = word(rt, G[7] + 0x78u); G[10] = word(rt, G[4] + 0x0u); put(rt, frame.at(0x1Cu), G[2]); G[5] = frame.at(0x1Cu); G[6] = frame.at(0x14u); put(rt, frame.at(0x18u), G[3]); G[8] = frame.at(0x4u); put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x14u), 0u); G[2] = word(rt, G[10] + 0xCu); { const std::uint32_t callee = G[2]; G[9] = frame.at(0x8u); call_here(rt, ctx, callee); } G[6] = word(rt, frame.at(0x4u)); put(rt, frame.at(0xCu), G[2]); G[4] = G[6] & 0x4u; put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(0x30u), G[2]); { const bool tk = G[4] == 0u; put(rt, frame.at(0x34u), G[3]); if (tk) goto L_08C58510; } G[6] = G[6] | 0x4u; L_08C584F0: ; put(rt, frame.at(0x4u), G[6]); L_08C584F4: ; { const bool tk = G[6] != 0u; G[2] = G[16]; if (tk) goto L_08C58538; } L_08C584FC: ; G[31] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); goto exit_; L_08C58510: ; G[3] = word(rt, frame.at(0x8u)); G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(0xFFFF8000u) ? 1u : 0u; if (G[2] != 0u) { G[6] = G[6] | 0x4u; goto L_08C584F0; } G[2] = 0x7FFFu; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; if (G[2] != 0u) { G[6] = G[6] | 0x4u; goto L_08C584F0; } store16(G[17] + 0x0u, G[3]); goto L_08C584F4; L_08C58538: ; G[2] = word(rt, G[16] + 0x0u); G[4] = word(rt, G[2] - 0xCu); G[4] = G[16] + G[4]; G[5] = word(rt, G[4] + 0x14u); G[5] = G[5] | G[6]; call_here(rt, ctx, 0x08C54CFCu); G[2] = G[16]; G[31] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); goto exit_; L_08C58568: ; call_here(rt, ctx, 0x08B5A9FCu); goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08C66DAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x60u), G[30]); G[30] = G[29]; G[7] = sext(G[7], 8u); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x54u), G[21]); G[21] = G[8]; put(rt, frame.at(0x50u), G[20]); G[20] = G[6]; G[22] = G[20] + 0x6Cu; put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x64u), G[31]); put(rt, frame.at(0x5Cu), G[23]); put(rt, frame.at(0x40u), G[16]); G[23] = word(rt, G[5] + 0x0u); G[5] = load8(G[5] + 0x4u); put(rt, G[30] + 0x14u, G[4]); G[4] = high_half(0x08D77484u); G[4] = 0x08D77484u; put(rt, G[30] + 0x18u, G[7]); put(rt, G[30] + 0x10u, G[5]); call_here(rt, ctx, 0x08B5C2B0u); G[4] = G[2] << 2u; G[3] = word(rt, G[20] + 0x6Cu); G[19] = G[2]; G[6] = 0u; G[2] = word(rt, G[3] + 0xCu); G[7] = 0u; put(rt, G[30] + 0x20u, G[6]); G[17] = G[2] + G[4]; G[18] = word(rt, G[17] + 0x0u); { const bool tk = G[18] == 0u; put(rt, G[30] + 0x24u, G[7]); if (tk) goto L_08C66FE0; } L_08C66E38: ; G[19] = word(rt, G[20] + 0xCu); G[22] = G[18] + 0x26u; G[5] = 0u - G[21]; G[5] = static_cast<std::uint32_t>(std::max(static_cast<std::int32_t>(G[21]), static_cast<std::int32_t>(G[5]))); G[4] = frame.at(0x14u); G[6] = G[22]; G[7] = G[19]; G[8] = 0u; call_here(rt, ctx, 0x08C5B784u); G[17] = G[2]; G[2] = load8(G[18] + 0x10u); G[16] = G[29] - G[17]; put(rt, G[30] + 0x0u, G[17]); { const bool tk = G[2] != 0u; G[9] = G[16] + 0x14u; if (tk) goto L_08C66F80; } L_08C66E78: ; G[3] = G[19] & 0x4Au; G[2] = 0x40u; { const bool tk = G[3] == G[2]; G[2] = 0x8u; if (tk) goto L_08C6704C; } { const bool tk = G[3] == G[2]; G[2] = 0x40u; if (tk) goto L_08C67050; } { const bool tk = static_cast<std::int32_t>(G[21]) <= 0; G[2] = G[19] & 0x800u; if (tk) goto L_08C66FC0; } if (G[2] != 0u) { G[2] = load8(G[22] + 0x1u); goto L_08C66F30; } G[17] = word(rt, G[30] + 0x0u); L_08C66EA4: ; G[6] = word(rt, G[20] + 0x8u); L_08C66EA8: ; G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[2] = G[6] + 0x1Eu; if (tk) goto L_08C66F54; } L_08C66EB4: ; G[7] = word(rt, G[30] + 0x10u); { const bool tk = G[7] != 0u; put(rt, G[20] + 0x8u, 0u); if (tk) goto L_08C66EE0; } G[3] = word(rt, G[23] + 0x0u); G[5] = G[9]; G[4] = G[23]; G[2] = word(rt, G[3] + 0x30u); { const std::uint32_t callee = G[2]; G[6] = G[17]; call_here(rt, ctx, callee); } { const bool tk = G[17] != G[2]; G[3] = 0x1u; if (tk) goto L_08C66EE4; } L_08C66EE0: ; G[3] = word(rt, G[30] + 0x10u); L_08C66EE4: ; store8(G[30] + 0x8u, G[3]); G[29] = G[30]; G[2] = word(rt, G[30] + 0x8u); put(rt, G[30] + 0x20u, G[23]); put(rt, G[30] + 0x24u, G[2]); G[3] = word(rt, G[30] + 0x24u); G[2] = word(rt, G[30] + 0x20u); G[31] = word(rt, frame.at(0x64u)); G[30] = word(rt, frame.at(0x60u)); G[23] = word(rt, frame.at(0x5Cu)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); goto exit_; L_08C66F30: ; G[9] = G[9] - 0x1u; G[17] = word(rt, G[30] + 0x0u); store8(G[9] + 0x0u, G[2]); G[17] = G[17] + 0x1u; G[6] = word(rt, G[20] + 0x8u); G[2] = static_cast<std::int32_t>(G[17]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; { const bool tk = G[2] == 0u; put(rt, G[30] + 0x0u, G[17]); if (tk) goto L_08C66EB4; } G[2] = G[6] + 0x1Eu; L_08C66F54: ; G[2] = ins(G[2], 0u, 0u, 4u); G[4] = word(rt, G[30] + 0x14u); G[5] = word(rt, G[30] + 0x18u); G[29] = G[29] - G[2]; G[7] = G[20]; G[8] = G[29]; G[10] = G[30]; call_here(rt, ctx, 0x08C5F858u); G[17] = word(rt, G[30] + 0x0u); G[9] = G[29]; goto L_08C66EB4; L_08C66F80: ; G[2] = G[17] << 1u; G[2] = G[2] + 0x20u; G[2] = ins(G[2], 0u, 0u, 4u); G[29] = G[29] - G[2]; G[5] = word(rt, G[18] + 0x8u); G[6] = word(rt, G[18] + 0xCu); G[7] = sext(load8(G[18] + 0x25u), 8u); G[4] = word(rt, G[30] + 0x14u); G[16] = frame.at(0x2u); G[10] = G[9]; G[8] = G[20]; G[9] = G[16]; G[11] = G[30]; call_here(rt, ctx, 0x08C59E34u); G[9] = G[16]; goto L_08C66E78; L_08C66FC0: ; { const bool tk = G[21] == 0u; G[17] = word(rt, G[30] + 0x0u); if (tk) goto L_08C66EA4; } G[2] = load8(G[18] + 0x26u); G[9] = G[9] - 0x1u; G[17] = G[17] + 0x1u; store8(G[9] + 0x0u, G[2]); put(rt, G[30] + 0x0u, G[17]); goto L_08C66EA4; L_08C66FE0: ; G[4] = 0x68u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[2] = high_half(0x08CDBAD8u); G[2] = 0x08CDBAD8u; put(rt, G[16] + 0x0u, G[2]); G[18] = G[16]; G[4] = G[16]; put(rt, G[16] + 0x4u, 0u); G[5] = G[22]; put(rt, G[16] + 0x8u, 0u); put(rt, G[16] + 0xCu, 0u); store8(G[16] + 0x10u, 0u); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0x18u, 0u); put(rt, G[16] + 0x1Cu, 0u); put(rt, G[16] + 0x20u, 0u); store8(G[16] + 0x24u, 0u); store8(G[16] + 0x25u, 0u); store8(G[16] + 0x64u, 0u); call_here(rt, ctx, 0x08C661C4u); G[4] = word(rt, G[22] + 0x0u); G[5] = G[16]; G[6] = G[19]; call_here(rt, ctx, 0x08B5C2F8u); G[18] = word(rt, G[17] + 0x0u); goto L_08C66E38; L_08C6704C: ; G[2] = 0x40u; L_08C67050: ; { const bool tk = G[3] == G[2]; G[2] = G[19] & 0x200u; if (tk) goto L_08C67094; } { const bool tk = G[2] == 0u; G[17] = word(rt, G[30] + 0x0u); if (tk) goto L_08C66EA4; } if (G[21] == 0u) { G[6] = word(rt, G[20] + 0x8u); goto L_08C66EA8; } G[2] = ext(G[19], 14u, 1u); G[2] = G[22] + G[2]; G[3] = load8(G[2] + 0x2u); G[17] = word(rt, G[30] + 0x0u); store8(G[9] - 0x1u, G[3]); G[17] = G[17] + 0x2u; G[9] = G[9] - 0x2u; G[2] = load8(G[22] + 0x4u); put(rt, G[30] + 0x0u, G[17]); store8(G[9] + 0x0u, G[2]); goto L_08C66EA4; L_08C67094: ; { const bool tk = G[2] == 0u; G[17] = word(rt, G[30] + 0x0u); if (tk) goto L_08C66EA4; } if (G[21] == 0u) { G[6] = word(rt, G[20] + 0x8u); goto L_08C66EA8; } G[17] = word(rt, G[30] + 0x0u); G[2] = load8(G[22] + 0x4u); G[9] = G[9] - 0x1u; G[17] = G[17] + 0x1u; store8(G[9] + 0x0u, G[2]); put(rt, G[30] + 0x0u, G[17]); goto L_08C66EA4; goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08C70D64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x38u), G[18]); G[18] = G[4]; G[4] = frame.at(0x10u); put(rt, frame.at(0x44u), G[21]); G[21] = G[5]; G[5] = G[18]; put(rt, frame.at(0x48u), G[31]); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x3Cu), G[19]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x30u), G[16]); call_here(rt, ctx, 0x08C70314u); G[2] = load8(frame.at(0x10u)); { const bool tk = G[2] == 0u; G[3] = word(rt, frame.at(0x14u)); if (tk) goto L_08C70E40; } G[2] = word(rt, G[18] + 0x0u); G[3] = word(rt, G[2] - 0xCu); G[16] = G[18] + G[3]; G[17] = word(rt, G[16] + 0x80u); if (G[17] == 0u) goto L_08C70EEC; G[19] = word(rt, G[16] + 0x78u); G[2] = load8(G[16] + 0x75u); { const bool tk = G[2] != 0u; G[20] = G[19] < 0x1u ? 1u : 0u; if (tk) goto L_08C70DE4; } G[4] = G[16]; G[5] = 0x20u; call_here(rt, ctx, 0x08C5477Cu); store8(G[16] + 0x74u, G[2]); G[2] = 0x1u; store8(G[16] + 0x75u, G[2]); L_08C70DE4: ; G[3] = word(rt, G[17] + 0x0u); G[7] = sext(load8(G[16] + 0x74u), 8u); store8(frame.at(0xCu), G[20]); G[4] = G[17]; G[6] = G[16]; put(rt, frame.at(0x8u), G[19]); G[8] = G[21]; G[2] = word(rt, G[3] + 0x10u); { const std::uint32_t callee = G[2]; G[5] = frame.at(0x8u); call_here(rt, ctx, callee); } put(rt, frame.at(0x24u), G[3]); G[3] = load8(frame.at(0x24u)); put(rt, frame.at(0x0u), G[2]); put(rt, frame.at(0x20u), G[2]); { const bool tk = G[3] == 0u; store8(frame.at(0x4u), G[3]); if (tk) goto L_08C70E3C; } G[2] = word(rt, G[18] + 0x0u); G[4] = word(rt, G[2] - 0xCu); G[4] = G[18] + G[4]; G[5] = word(rt, G[4] + 0x14u); G[5] = G[5] | 0x1u; call_here(rt, ctx, 0x08C54CFCu); L_08C70E3C: ; G[3] = word(rt, frame.at(0x14u)); L_08C70E40: ; G[2] = word(rt, G[3] + 0x0u); G[4] = word(rt, G[2] - 0xCu); G[3] = G[3] + G[4]; G[2] = word(rt, G[3] + 0xCu); G[2] = G[2] & 0x2000u; if (G[2] != 0u) goto L_08C70E84; L_08C70E5C: ; G[2] = G[18]; L_08C70E60: ; G[31] = word(rt, frame.at(0x48u)); G[21] = word(rt, frame.at(0x44u)); G[20] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x3Cu)); G[18] = word(rt, frame.at(0x38u)); G[17] = word(rt, frame.at(0x34u)); G[16] = word(rt, frame.at(0x30u)); goto exit_; L_08C70E84: ; call_here(rt, ctx, 0x08B62318u); { const bool tk = G[2] != 0u; G[2] = word(rt, frame.at(0x14u)); if (tk) goto L_08C70E5C; } G[3] = word(rt, G[2] + 0x0u); G[4] = word(rt, G[3] - 0xCu); G[2] = G[2] + G[4]; G[4] = word(rt, G[2] + 0x78u); { const bool tk = G[4] == 0u; G[2] = G[18]; if (tk) goto L_08C70E60; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x18u); { const std::uint32_t callee = G[2];  call_here(rt, ctx, callee); } G[3] = 0xFFFFFFFFu; if (G[2] != G[3]) { G[2] = G[18]; goto L_08C70E60; } G[4] = word(rt, frame.at(0x14u)); G[2] = word(rt, G[4] + 0x0u); G[3] = word(rt, G[2] - 0xCu); G[4] = G[4] + G[3]; G[5] = word(rt, G[4] + 0x14u); G[5] = G[5] | 0x1u; call_here(rt, ctx, 0x08C54CFCu); G[2] = G[18]; goto L_08C70E60; L_08C70EEC: ; call_here(rt, ctx, 0x08B5A9FCu); goto exit_; exit_: ; } } if (tail != 0u) jump(ctx, tail); } }
void func_08C74DD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 old_length = 0u;
    if (assign_bytes(rt, ctx, old_length)) call_set(rt, ctx, 0x08C74D38u, {ctx.gpr[4], 0u, old_length, ctx.gpr[5], ctx.gpr[6]});
}

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_336
} // namespace gt::game

#define HOT336(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_336::name, gt::ReturnKind::kind)
HOT336(0x0881A208u, func_0881A208, Word);
HOT336(0x08840A0Cu, func_08840A0C, Word);
// HOT336(0x0885B97Cu, func_0885B97C, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT336(0x0886C44Cu, func_0886C44C, Word);  // no comparable synthetic case yet
HOT336(0x0887DF4Cu, func_0887DF4C, Word);
// HOT336(0x0888A2A8u, func_0888A2A8, Word);  // no comparable synthetic case yet
// HOT336(0x088B1F60u, func_088B1F60, Word);  // no comparable synthetic case yet
// HOT336(0x088DC48Cu, func_088DC48C, Word);  // no comparable synthetic case yet
HOT336(0x088ED560u, func_088ED560, Word);
// HOT336(0x08920C68u, func_08920C68, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT336(0x089372C8u, func_089372C8, Word);  // no comparable synthetic case yet
HOT336(0x0896FC0Cu, func_0896FC0C, Word);
// HOT336(0x08987F4Cu, func_08987F4C, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT336(0x08999CB4u, func_08999CB4, Word);
HOT336(0x089B1D10u, func_089B1D10, Word);
// HOT336(0x089C8B14u, func_089C8B14, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT336(0x089D84E8u, func_089D84E8, Word);
// HOT336(0x089F3F30u, func_089F3F30, Word);  // no comparable synthetic case yet
HOT336(0x08A29C9Cu, func_08A29C9C, Word);
// HOT336(0x08A39E64u, func_08A39E64, Word);  // no comparable synthetic case yet
HOT336(0x08A4C9A4u, func_08A4C9A4, Word);
HOT336(0x08A705F8u, func_08A705F8, Word);
// HOT336(0x08A7ED20u, func_08A7ED20, Word);  // no comparable synthetic case yet
// HOT336(0x08A8DCB8u, func_08A8DCB8, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT336(0x08A9B2C0u, func_08A9B2C0, Word);
// HOT336(0x08AB5174u, func_08AB5174, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
HOT336(0x08AC7A98u, func_08AC7A98, Word);
HOT336(0x08ADC764u, func_08ADC764, Word);
// HOT336(0x08AEED10u, func_08AEED10, Word);  // no comparable synthetic case yet
// HOT336(0x08B04908u, func_08B04908, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT336(0x08B18534u, func_08B18534, Word);  // no comparable synthetic case yet
// HOT336(0x08B2CF88u, func_08B2CF88, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT336(0x08B3E45Cu, func_08B3E45C, Word);  // no comparable synthetic case yet
HOT336(0x08B44C10u, func_08B44C10, Word);
HOT336(0x08B54A64u, func_08B54A64, Word);
// HOT336(0x08B65728u, func_08B65728, Word);  // a synthetic case mismatched: to fix
HOT336(0x08B72030u, func_08B72030, Word);
HOT336(0x08B80990u, func_08B80990, Word);
HOT336(0x08B8BC08u, func_08B8BC08, Word);
// HOT336(0x08B95C50u, func_08B95C50, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT336(0x08BB2B00u, func_08BB2B00, Word);  // no comparable synthetic case yet
HOT336(0x08C1FC90u, func_08C1FC90, Word);
// HOT336(0x08C2EF40u, func_08C2EF40, Word);  // no comparable synthetic case yet
HOT336(0x08C36494u, func_08C36494, Word);
HOT336(0x08C47924u, func_08C47924, Word);
// HOT336(0x08C52070u, func_08C52070, Word);  // no comparable synthetic case yet
HOT336(0x08C58450u, func_08C58450, Word);
HOT336(0x08C66DACu, func_08C66DAC, Word);
HOT336(0x08C70D64u, func_08C70D64, Word);
HOT336(0x08C74DD8u, func_08C74DD8, Word);
