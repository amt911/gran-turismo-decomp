// Hot batch 208: forty-three-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_208 {
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
// A `jal` with a0.. loaded from `arguments` first (the registers the original sets before it); v0 / v1 / f0 as the callee left them.
u32 call_args(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    call_here(rt, ctx, target);
    return ctx.gpr[2];
}
// A `lw v0, slot(vtable); jalr v0` call: v0 holds the target when the callee starts.
u32 call_virtual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    ctx.gpr[2] = target;
    call_here(rt, ctx, target);
    return ctx.gpr[2];
}
// `a0 = message; if (reference == 0) call 0x08A5BB7C`: the failure handler of a null checked reference. a0 holds the message either way.
void check_reference(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 reference, u32 message) {
    ctx.gpr[4] = message;
    if (reference == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
}
// Four wrappers share this body. With a2 > 0: 0x088297A8 fills the text pointer at frame + 0 (0x088294D0 converts the pointer it
// holds, 0x08A5B9EC releases the temporary), `convert` makes an argument of the conversion; the object a3 points at (checked)
// answers its virtual at `slot`, and `forward` gets both. With a2 <= 0 nothing happens and v0 is what it was on entry.
void forward_converted_text(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 convert, u32 slot, u32 forward) {
    GuestFrame frame(ctx, 0x20u);
    const u32 text = frame.at(0u);
    const u32 holder = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        call_args(rt, ctx, 0x088297A8u, {text});
        const u32 source = word(rt, text);
        check_reference(rt, ctx, source, 0x08C9A2F8u);
        const u32 converted = call_args(rt, ctx, 0x088294D0u, {source});
        call_args(rt, ctx, 0x08A5B9ECu, {text});
        const u32 argument = call_args(rt, ctx, convert, {converted});
        const u32 target = word(rt, holder);
        check_reference(rt, ctx, target, 0x08C9A2B4u);
        const u32 result = call_virtual(rt, ctx, word(rt, word(rt, target) + slot), {target});
        call_args(rt, ctx, forward, {argument, result});
    }
}
// A counted string from [begin, end) (a0, a1): the shared empty representation `empty_rep` when the range is empty, else a new one
// from `allocate` (length, 0) holding the characters, its length at +0, 0 at +8 and the terminator byte kept at `terminator`.
// Returns the character pointer (representation + 0xC).
u32 make_counted_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 empty_rep, u32 allocate, u32 terminator) {
    GuestFrame frame(ctx, 0x20u);
    const u32 begin = ctx.gpr[4];
    const u32 end = ctx.gpr[5];
    const u32 length = end - begin;
    u32 text = empty_rep + 0xCu;
    if (begin != end) {
        const u32 representation = call_args(rt, ctx, allocate, {length, 0u});
        text = representation + 0xCu;
        ctx.gpr[2] = 1u;  // v0 holds the constant 1 the length is compared with
        if (length != 1u) call_args(rt, ctx, 0x08B79A88u, {text, begin, length});
        else rt.memory().store8(text, rt.memory().load8(begin));
        put(rt, representation, length);
        put(rt, representation + 8u, 0u);
        rt.memory().store8(text + length, rt.memory().load8(terminator));
    }
    ctx.gpr[2] = text;
    return text;
}
// With a1 >= 2: a0 = the virtual +0x3C of the object a2 points at (checked), a1 = the same for the object at a2 + 4 (checked; made 0 / 1
// when `normalize`), then the jump to `target`. With a1 < 2 nothing happens.
void tail_after_two_virtuals(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, bool normalize) {
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 holder = ctx.gpr[6];
        ctx.gpr[5] = static_cast<s32>(ctx.gpr[5]) < 2 ? 1u : 0u;
        if (ctx.gpr[5] == 0u) {
            const u32 first = word(rt, holder);
            check_reference(rt, ctx, first, 0x08C9BA2Cu);
            const u32 left = call_virtual(rt, ctx, word(rt, word(rt, first) + 0x3Cu), {first});
            const u32 second = word(rt, holder + 4u);
            check_reference(rt, ctx, second, 0x08C9BA2Cu);
            const u32 right = call_virtual(rt, ctx, word(rt, word(rt, second) + 0x3Cu), {second});
            ctx.gpr[5] = normalize ? (right != 0u ? 1u : 0u) : right;
            ctx.gpr[4] = left;
            tail = true;
        }
    }
    if (tail) jump(ctx, target);
}
// Drops one use of the counted object `shared` (use count at +4; at zero its virtual +8 runs and the weak count at +8 drops, at zero
// its virtual +0xC runs). v0 is the constant 1 the counts are compared with when a count does not reach zero.
void release_counted(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 shared) {
    const u32 uses = word(rt, shared + 4u);
    put(rt, shared + 4u, uses - 1u);
    ctx.gpr[2] = 1u;
    if (uses == 1u) {
        call_virtual(rt, ctx, word(rt, word(rt, shared) + 8u), {shared});
        const u32 weak = word(rt, shared + 8u);
        put(rt, shared + 8u, weak - 1u);
        ctx.gpr[2] = 1u;
        if (weak == 1u) call_virtual(rt, ctx, word(rt, word(rt, shared) + 0xCu), {shared});
    }
}
// Destructor body: the vtable becomes `first`, the counted object at `offset` (when set) loses a use, the vtable becomes `last`, and
// the base destructor `base` runs as a tail call with a0 = the object.
void destroy_with_shared_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 offset, u32 first, u32 last, u32 base) {
    const u32 self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        const u32 shared = word(rt, self + offset);
        put(rt, self, first);
        if (shared != 0u) release_counted(rt, ctx, shared);
        ctx.gpr[2] = last;
        put(rt, self, last);
        ctx.gpr[4] = self;
    }
    jump(ctx, base);
}
// `count` (a1) records of 12 bytes at a0, each built from the same range at a2 (begin at +0, end at +4): a block of the range's length
// rounded down to 32 bytes from 0x08B35840 (16, size), then 0x08C0D7F8 (begin, end, block, frame + 0) fills it. The range pointer
// does not move between records, as in the original.
void build_blocks(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x30u);
    const u32 count = ctx.gpr[5];
    const u32 range = ctx.gpr[6];
    if (count != 0u) {
        u32 record = ctx.gpr[4];
        u32 done = 0u;
        do {
            const u32 size = (word(rt, range + 4u) - word(rt, range)) & ~0x1Fu;
            put(rt, record + 4u, 0u);
            put(rt, record, 0u);
            ++done;
            put(rt, record + 8u, 0u);
            const u32 block = call_args(rt, ctx, 0x08B35840u, {0x10u, size});
            put(rt, record, block);
            put(rt, record + 4u, block);
            put(rt, record + 8u, block + size);
            const u32 filled = call_args(rt, ctx, 0x08C0D7F8u, {word(rt, range), word(rt, range + 4u), block, frame.at(0u)});
            put(rt, record + 4u, filled);
            record += 0xCu;
        } while (count != done);
    }
}
// Asks the object a0 (virtual +0x108) for 8 bytes at frame + 0 and joins them into a 64-bit value (v1:v0): byte i shifted by 8 * i,
// or by 8 * (7 - i) when the object's flag at +0x24 is set; 0 when it did not deliver 8 bytes.
void read_eight_bytes(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 self = ctx.gpr[4];
    const u32 buffer = frame.at(0u);
    const u32 delivered = call_virtual(rt, ctx, word(rt, word(rt, self) + 0x108u), {self, buffer, 8u});
    std::uint64_t value = 0u;
    if (delivered == 8u) {
        const bool swapped = word(rt, self + 0x24u) != 0u;
        for (u32 i = 0u; i < 8u; ++i) {
            const std::uint64_t byte = rt.memory().load8(buffer + i);
            value |= byte << ((swapped ? 7u - i : i) * 8u);
        }
    }
    ctx.gpr[2] = static_cast<u32>(value);
    ctx.gpr[3] = static_cast<u32>(value >> 32);
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0882AD6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    forward_converted_text(rt, ctx, 0x0885E710u, 0x30u, 0x08917ACCu);
}
void func_088375E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_after_two_virtuals(rt, ctx, 0x0892B904u, true);
}
void func_08B956D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    make_counted_string(rt, ctx, 0x08D75298u, 0x08B93B20u, 0x08CBF760u);
}
void func_08BC04A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    destroy_with_shared_member(rt, ctx, 8u, 0x08CC51D0u, 0x08CC2548u, 0x08B64654u);
}
void func_08BEB9E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[3] = word(rt, G[5] + 0x18u); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[4] = G[6] + G[2]; G[2] = G[3] & 1u; G[5] = word(rt, G[5] + 0x14u); if (G[2] != 0u) { G[2] = word(rt, G[4]); G[2] += G[5]; G[2] = word(rt, G[2]); } else G[2] = G[5]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[2] = word(rt, G[16] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[16] + 0x1Cu); G[4] = G[3] + G[5]; } call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08B88F44u); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08C0E1AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    build_blocks(rt, ctx);
}
void func_08C39C2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    read_eight_bytes(rt, ctx);
}
void func_08BF863C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[3] = word(rt, G[5] + 0x18u); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[4] = G[6] + G[2]; G[2] = G[3] & 1u; G[5] = word(rt, G[5] + 0x14u); if (G[2] != 0u) { G[2] = word(rt, G[4]); G[2] += G[5]; G[2] = word(rt, G[2]); } else G[2] = G[5]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[2] = word(rt, G[16] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[16] + 0x1Cu); G[4] = G[3] + G[5]; } call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08BFDE18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[3] = word(rt, G[5] + 0x18u); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[4] = G[6] + G[2]; G[2] = G[3] & 1u; G[5] = word(rt, G[5] + 0x14u); if (G[2] != 0u) { G[2] = word(rt, G[4]); G[2] += G[5]; G[2] = word(rt, G[2]); } else G[2] = G[5]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[2] = word(rt, G[16] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[16] + 0x1Cu); G[4] = G[3] + G[5]; } call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A76ABCu); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08C0632C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[3] = word(rt, G[5] + 0x18u); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[4] = G[6] + G[2]; G[2] = G[3] & 1u; G[5] = word(rt, G[5] + 0x14u); if (G[2] != 0u) { G[2] = word(rt, G[4]); G[2] += G[5]; G[2] = word(rt, G[2]); } else G[2] = G[5]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[2] = word(rt, G[16] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[16] + 0x1Cu); G[4] = G[3] + G[5]; } call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08B88F44u); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_08C07D78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[4]; G[16] = G[5]; G[3] = word(rt, G[5] + 0x18u); G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[3]) >> 1); G[4] = G[6] + G[2]; G[2] = G[3] & 1u; G[5] = word(rt, G[5] + 0x14u); if (G[2] != 0u) { G[2] = word(rt, G[4]); G[2] += G[5]; G[2] = word(rt, G[2]); } else G[2] = G[5]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[2] = word(rt, G[16] + 0x20u); G[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 1); G[2] &= 1u; G[4] = G[5] + G[3]; if (G[2] != 0u) { G[3] = word(rt, G[16] + 0x1Cu); G[2] = word(rt, G[4]); G[2] += G[3]; G[2] = word(rt, G[2]); } else { G[2] = word(rt, G[16] + 0x1Cu); G[4] = G[3] + G[5]; } call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08B88F44u); G[2] = G[17]; } G[16] = s0; G[17] = s1; }
void func_0882AEB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    forward_converted_text(rt, ctx, 0x0885E710u, 0x30u, 0x08917970u);
}
void func_0882B090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    forward_converted_text(rt, ctx, 0x0885E710u, 0x3Cu, 0x08917AC0u);
}
void func_0882BBBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    forward_converted_text(rt, ctx, 0x0885EDF8u, 0x3Cu, 0x088E0B78u);
}
void func_08BEEC3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    make_counted_string(rt, ctx, 0x08D74FD0u, 0x08B8543Cu, 0x08CB7430u);
}
void func_08C308D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[2] = 0x08D74FD0u; G[3] = G[5]; G[17] = G[5] - G[4]; G[16] = G[4]; G[4] = G[17]; G[5] = 0u; G[18] = G[2] + 0xCu; if (G[16] != G[3]) { call_here(rt, ctx, 0x08B8543Cu); G[18] = G[2] + 0xCu; G[19] = G[2]; G[2] = 1u; G[5] = G[16]; G[6] = G[17]; G[4] = G[18]; if (G[17] != G[2]) call_here(rt, ctx, 0x08B79A88u); else { G[2] = load8(G[16]); store8(G[19] + 0xCu, G[2]); } G[3] = 0x08CB0000u; G[4] = load8(0x08CB7430u); G[2] = G[18] + G[17]; put(rt, G[19], G[17]); put(rt, G[19] + 8u, 0u); store8(G[2], G[4]); } G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08BC05FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    destroy_with_shared_member(rt, ctx, 8u, 0x08CC4388u, 0x08CC2548u, 0x08B64654u);
}
void func_08837738(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_after_two_virtuals(rt, ctx, 0x0892B954u, true);
}
void func_08C0E30C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    build_blocks(rt, ctx);
}
void func_08C39E0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    read_eight_bytes(rt, ctx);
}
void func_08BC789C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    destroy_with_shared_member(rt, ctx, 0x14u, 0x08CC5258u, 0x08CBF488u, 0x08B64654u);
}
void func_0881EF0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = frame.at(0u); G[17] = frame.at(4u); call_here(rt, ctx, 0x0881BD98u); G[4] = 0x08C98F48u; G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x0881B068u); G[16] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0881AA7Cu); G[6] = G[2]; G[4] = G[17]; G[7] = 0x10u; G[5] = 0u; call_here(rt, ctx, 0x08A950D0u); G[2] = word(rt, frame.at(4u)); G[4] = G[16]; G[5] = word(rt, G[2] + 0x14u); call_here(rt, ctx, 0x0881B324u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_0882B698(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 text = frame.at(0u);
    const u32 argument = ctx.gpr[5];
    const u32 holder = ctx.gpr[7];
    if (static_cast<s32>(ctx.gpr[6]) > 0) {
        const u32 target = word(rt, holder);
        check_reference(rt, ctx, target, 0x08C9A2B4u);
        const u32 result = call_virtual(rt, ctx, word(rt, word(rt, target) + 0x3Cu), {target});
        call_args(rt, ctx, 0x088297A8u, {text, argument});
        const u32 source = word(rt, text);
        check_reference(rt, ctx, source, 0x08C9A2F8u);
        const u32 converted = call_args(rt, ctx, 0x088294D0u, {source});
        const u32 value = call_args(rt, ctx, 0x0885EDF8u, {converted});
        call_args(rt, ctx, 0x08A5B9ECu, {text});
        call_args(rt, ctx, 0x088D6C00u, {value, result});
    }
}
void func_0883768C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[5] = static_cast<std::int32_t>(G[5]) < 2 ? 1u : 0u; G[17] = G[6]; if (G[5] == 0u) { G[16] = word(rt, G[6]); G[4] = 0x08C9BA2Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); G[4] = 0x08C9BA2Cu; G[18] = G[2]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[18]; tail = true; } } G[16] = s0; G[17] = s1; G[18] = s2; if (tail) jump(ctx, 0x0892B92Cu); }
void func_0886A658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBF680u; G[17] = G[4]; put(rt, G[4], G[2]); G[4] = 0x4244u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[4] = G[2]; call_here(rt, ctx, 0x0886A5C8u); put(rt, G[17] + 4u, G[16]); G[3] = 0x08D70000u; G[2] = 0x08D70000u; G[5] = sext(load8(0x08D752D0u), 8); G[16] = 0x08D752D0u; G[18] = 0x08D752D8u; G[4] = G[16]; G[17] += 4u; if (G[5] == 0u) { call_here(rt, ctx, 0x08B6441Cu); G[5] = 0x08D70000u; G[18] = 0x08D752D8u; G[4] = G[16]; if (G[2] != 0u) { G[18] = 0x08D752D8u; G[2] = 0x08CBF690u; put(rt, 0x08D752D8u, G[2]); call_here(rt, ctx, 0x08B64494u); } } put(rt, G[17] + 4u, G[18]); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088853F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = G[5]; G[16] = G[4]; G[4] = G[5]; G[5] = 0x08884EE8u; put(rt, G[16] + 0x18Cu, 0u); put(rt, G[16] + 0x40u, G[2]); if (G[2] != 0u) { call_here(rt, ctx, 0x08BA6000u); put(rt, G[16] + 0x44u, G[2]); G[6] = 0x08C89A00u; G[4] = word(rt, G[2]); G[7] = 2u; G[5] = G[2]; call_here(rt, ctx, 0x08AF93DCu); G[3] = word(rt, G[16] + 0x44u); G[2] = G[16] + 0x50u; put(rt, G[3] + 4u, G[2]); G[5] = word(rt, G[16] + 0x78u); G[4] = word(rt, G[16] + 0x44u); G[5] <<= 20; G[5] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[5]) >> 20); call_here(rt, ctx, 0x08AF8FF0u); G[4] = G[16]; put(rt, G[16] + 0x16Cu, 0u); G[5] = 1u; call_here(rt, ctx, 0x08884DA0u); G[4] = G[16]; call_here(rt, ctx, 0x0888531Cu); G[4] = G[16]; tail = true; } } G[16] = s0; if (tail) jump(ctx, 0x08884EF8u); }
void func_088875CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = word(rt, G[4]); if (G[4] != 0u) { G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); } G[16] = word(rt, G[16] + 4u); if (G[16] != 0u) { G[3] = G[16] + 4u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 8u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = G[16] + 8u; G[4] = word(rt, G[3]); G[2] = G[4] - 1u; put(rt, G[3], G[2]); G[2] = 1u; if (G[4] == G[2]) { G[2] = word(rt, G[16]); G[4] = G[16]; G[25] = word(rt, G[2] + 0xCu); tail = true; } } } } G[16] = s0; if (tail) jump(ctx, G[25]); }
void func_088CD62C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[2] = word(rt, G[4] + 0x2Cu); G[17] = G[4]; if (static_cast<std::int32_t>(G[2]) >= 0) { G[2] = load8(G[4] + 0x635u); if (G[2] != 0u) { call_here(rt, ctx, 0x08B344E0u); G[4] = G[17]; call_here(rt, ctx, 0x088CD5DCu); store8(G[17] + 0x635u, 0u); G[4] = G[17]; call_here(rt, ctx, 0x08B3457Cu); } G[4] = G[17]; call_here(rt, ctx, 0x08B344E0u); ctx.fpr[0] = float_at(rt, G[17] + 0x610u); G[3] = G[17] + 0xB50u; G[2] = word(rt, G[17] + 0x5DCu); put_float(rt, G[3] + 4u, ctx.fpr[0]); G[4] = G[17]; G[16] = 0u; ctx.fpr[0] = float_at(rt, G[17] + 0x614u); put(rt, G[17] + 0xB50u, G[2]); G[18] = 0x24u; put_float(rt, G[3] + 8u, ctx.fpr[0]); call_here(rt, ctx, 0x08B3457Cu); G[5] = G[16]; do { G[4] = G[17]; G[16] += 1u; call_here(rt, ctx, 0x088CB9B0u); G[5] = G[16]; } while (G[16] != G[18]); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088D07F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); put_float(rt, frame.at(0x20u), ctx.fpr[20]); ctx.fpr[20] = ctx.fpr[12]; G[17] = G[4]; G[4] = G[6]; G[19] = G[5]; G[18] = G[6]; G[16] = G[19]; call_here(rt, ctx, 0x088FB028u); const bool lt = ctx.fpr[0] < ctx.fpr[20]; G[4] = G[18]; ctx.fpr[1] = ctx.fpr[20]; if (lt) { G[16] = G[19] + 1u; call_here(rt, ctx, 0x088FB028u); ctx.fpr[1] = ctx.fpr[20] - ctx.fpr[0]; } ctx.fpr[0] = 0.0f; const bool neg = ctx.fpr[20] < ctx.fpr[0]; G[4] = G[18]; if (neg) { G[16] -= 1u; call_here(rt, ctx, 0x088FB028u); ctx.fpr[1] = ctx.fpr[20] + ctx.fpr[0]; } put(rt, G[17] + 0x10u, G[16]); put_float(rt, G[17], ctx.fpr[1]); put_float(rt, G[17] + 4u, ctx.fpr[1]); put(rt, G[17] + 0x1Cu, G[16]); put(rt, G[17] + 0x18u, G[16]); put(rt, G[17] + 0x14u, G[16]); ctx.fpr[20] = float_at(rt, frame.at(0x20u)); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_088D0CCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4]; G[4] = G[6]; G[20] = 1u; call_here(rt, ctx, 0x088DCD28u); G[4] = G[2]; G[3] = word(rt, G[2]); G[16] = G[2]; G[2] = word(rt, G[3] + 0x8Cu); call_here(rt, ctx, G[2]); G[19] = G[2]; if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = 0u; G[3] = word(rt, G[16]); do { G[5] = G[17]; G[2] = word(rt, G[3] + 0x90u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (G[2] != 0u) { G[2] = word(rt, G[18] + 0x5Cu); G[2] += 1u; put(rt, G[18] + 0x5Cu, G[2]); } else G[20] = 0u; G[17] += 1u; if (G[19] == G[17]) break; G[3] = word(rt, G[16]); } while (true); } store8(G[18] + 0x5Bu, G[20]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_088E649C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[17] = G[5] & 0xFFu; G[16] = G[4]; G[4] = word(rt, G[4]); G[4] += 0xECu; call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x50u); G[5] = 0xFFFFFFFFu; call_here(rt, ctx, G[2]); if (G[2] != 0u) store8(G[2] + 0xAu, G[17]); G[4] = word(rt, G[16]); G[4] += 0xECu; call_here(rt, ctx, 0x08B3457Cu); G[2] = word(rt, G[16] + 4u); G[4] = G[2] + 0xECu; if (G[2] != 0u) { call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[16] + 4u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x50u); G[5] = 0xFFFFFFFFu; call_here(rt, ctx, G[2]); if (G[2] != 0u) store8(G[2] + 0xAu, G[17]); G[4] = word(rt, G[16] + 4u); G[4] += 0xECu; tail = true; } } G[16] = s0; G[17] = s1; if (tail) jump(ctx, 0x08B3457Cu); }
void func_088E7004(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x10u); G[18] = G[5]; G[17] = G[4]; G[2] = word(rt, G[4]); G[16] = word(rt, G[2] + 0x548u); G[4] = G[2] + 0xECu; G[3] = 0u; if (static_cast<std::int32_t>(G[16]) >= 0) { call_here(rt, ctx, 0x08B344E0u); G[4] = word(rt, G[17]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0x50u); G[5] = G[16]; call_here(rt, ctx, G[2]); G[4] = word(rt, G[17]); if (G[2] == 0u) { G[4] += 0xECu; call_here(rt, ctx, 0x08B3457Cu); G[3] = 0u; } else { G[16] = word(rt, G[2] + 0x38u); G[4] += 0xECu; call_here(rt, ctx, 0x08B3457Cu); call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; G[5] = G[16]; G[6] = G[18]; call_here(rt, ctx, 0x08B09B5Cu); G[3] = G[2] != 0u ? 1u : 0u; } } G[2] = G[3]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_088F8688(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[3] = float_at(rt, G[6] + 0x40u); ctx.fpr[4] = float_at(rt, G[6] + 0x50u); ctx.fpr[1] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 8u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[3]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[4]); ctx.fpr[3] = float_at(rt, G[6] + 0x60u); ctx.fpr[4] = float_at(rt, G[6] + 0x70u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[3]); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2]; ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[4]; put_float(rt, G[4], ctx.fpr[1]); ctx.fpr[3] = float_at(rt, G[6] + 0x44u); ctx.fpr[4] = float_at(rt, G[6] + 0x54u); ctx.fpr[0] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[5] + 4u); ctx.fpr[1] = float_at(rt, G[5] + 8u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[3]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[4]); ctx.fpr[3] = float_at(rt, G[6] + 0x64u); ctx.fpr[4] = float_at(rt, G[6] + 0x74u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[3]); ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[2]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[4]; put_float(rt, G[4] + 4u, ctx.fpr[0]); ctx.fpr[3] = float_at(rt, G[6] + 0x48u); ctx.fpr[4] = float_at(rt, G[6] + 0x58u); ctx.fpr[1] = float_at(rt, G[5]); ctx.fpr[2] = float_at(rt, G[5] + 4u); ctx.fpr[0] = float_at(rt, G[5] + 8u); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[3]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[4]); ctx.fpr[3] = float_at(rt, G[6] + 0x68u); ctx.fpr[4] = float_at(rt, G[6] + 0x78u); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[2]; ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[3]); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[4]; put_float(rt, G[4] + 8u, ctx.fpr[1]); }
void func_08926564(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x14u); ctx.fpr[3] = float_at(rt, G[4] + 0xCu); ctx.fpr[6] = float_at(rt, G[4] + 0x18u); ctx.fpr[7] = psp_mul(ctx.fpr[0], ctx.fpr[12]); ctx.fpr[5] = float_at(rt, G[4] + 0x1Cu); ctx.fpr[11] = 0.0f; G[2] = 0x08CE0000u; ctx.fpr[1] = float_at(rt, 0x08CDDD68u); G[1] = 0x3F800000u; ctx.fpr[8] = std::bit_cast<float>(G[1]); G[2] = 5u; G[1] = 0x3C88AB7Cu; ctx.fpr[10] = std::bit_cast<float>(G[1]); G[1] = 0x3727C5ACu; ctx.fpr[9] = std::bit_cast<float>(G[1]); for (;;) { const bool big = ctx.fpr[10] <= ctx.fpr[1]; ctx.fpr[4] = psp_mul(ctx.fpr[6], ctx.fpr[3]); G[1] = 0x3C88AB7Cu; ctx.fpr[2] = std::bit_cast<float>(G[1]); G[2] -= 1u; if (!big) ctx.fpr[2] = ctx.fpr[1]; if (ctx.fpr[11] < ctx.fpr[1]) ctx.fpr[8] = ctx.fpr[2] / ctx.fpr[1]; ctx.fpr[0] = psp_mul(ctx.fpr[8], ctx.fpr[7]); ctx.fpr[1] = ctx.fpr[1] - ctx.fpr[2]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[4]; const bool done = ctx.fpr[1] < ctx.fpr[9]; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[5]; ctx.fpr[0] = psp_mul(ctx.fpr[2], ctx.fpr[0]); ctx.fpr[0] = ctx.fpr[3] + ctx.fpr[0]; ctx.fpr[3] = ctx.fpr[0]; put_float(rt, G[4] + 0xCu, ctx.fpr[0]); if (done || G[2] == 0u) return; } }
void func_08929F14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x20u); G[2] = 1u; if (G[4] == G[2]) { G[4] = 0x08CF2910u; G[2] = 0xFFFFu; if (G[5] == G[2]) { call_here(rt, ctx, 0x08929C64u); G[4] = 0x08CF290Cu; call_here(rt, ctx, 0x08929D88u); G[4] = 0x08CF2900u; call_here(rt, ctx, 0x0892A7D0u); } } else { const bool nz = G[4] != 0u; G[16] = 0x08CF0000u; if (!nz) { G[2] = 0xFFFFu; G[3] = 0x08CF2900u; G[6] = frame.at(0u); if (G[5] == G[2]) { G[4] = word(rt, 0x08CF2900u); G[5] = word(rt, G[3] + 4u); call_here(rt, ctx, 0x08BDD878u); G[2] = word(rt, 0x08CF2900u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08B35254u); G[4] = 0x08CF290Cu; call_here(rt, ctx, 0x08929DF0u); G[4] = 0x08CF2910u; call_here(rt, ctx, 0x08929CB0u); } } } } G[16] = s0; }
void func_0893C870(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; const std::uint32_t s5 = G[21]; { GuestFrame frame(ctx, 0x20u); G[21] = 4u; G[19] = G[4]; G[18] = G[5]; G[17] = 1u; G[16] = 0u; G[20] = word(rt, G[4] + 0xCu); do { G[5] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x08955300u); G[4] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[16]) >> 1); G[5] = G[4] << 2; G[3] = G[4] << 5; G[3] -= G[5]; G[3] += G[4]; G[3] <<= 2; G[3] += G[20]; ctx.fpr[1] = float_at(rt, G[2] + 0xC8u); ctx.fpr[0] = float_at(rt, G[3] + 0x364u); G[16] += 1u; if (ctx.fpr[1] < ctx.fpr[0]) G[18] |= G[17]; G[17] <<= 1; } while (G[16] != G[21]); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; G[21] = s5; }
void func_08949DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[3] = G[4]; G[5] &= 0xFFu; G[1] = 0x42C80000u; ctx.fpr[3] = std::bit_cast<float>(G[1]); G[1] = 0x41A00000u; ctx.fpr[2] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x10u), ctx.fpr[20]); G[2] = word(rt, G[3] + 8u); G[3] = sext(load16(G[2] + 0x58Eu), 16); G[17] = G[2] + 0x594u; G[16] = G[2] + 0x598u; ctx.fpr[0] = std::bit_cast<float>(G[3]); ctx.fpr[1] = static_cast<float>(static_cast<std::int32_t>(G[3])); if (G[5] == 0u) { G[1] = 0x447A0000u; ctx.fpr[3] = std::bit_cast<float>(G[1]); G[1] = 0x42480000u; ctx.fpr[2] = std::bit_cast<float>(G[1]); } ctx.fpr[20] = float_at(rt, G[17]); ctx.fpr[0] = float_at(rt, G[16]); ctx.fpr[20] = ctx.fpr[1] - ctx.fpr[20]; ctx.fpr[0] = psp_mul(ctx.fpr[2], ctx.fpr[0]); ctx.fpr[20] = psp_mul(ctx.fpr[3], ctx.fpr[20]); ctx.fpr[20] = ctx.fpr[20] - ctx.fpr[0]; call_here(rt, ctx, 0x0895549Cu); ctx.fpr[2] = float_at(rt, G[16]); ctx.fpr[20] = psp_mul(ctx.fpr[20], ctx.fpr[0]); ctx.fpr[2] = ctx.fpr[2] + ctx.fpr[20]; put_float(rt, G[16], ctx.fpr[2]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); ctx.fpr[1] = float_at(rt, G[17]); ctx.fpr[1] = ctx.fpr[1] + ctx.fpr[0]; put_float(rt, G[17], ctx.fpr[1]); ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } G[16] = s0; G[17] = s1; }
void func_089C9528(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CBB888u; G[17] = G[4] + 0x14u; G[16] = G[4]; put(rt, G[4], G[2]); G[4] = G[17]; G[5] = word(rt, G[17] + 8u); call_here(rt, ctx, 0x08C012FCu); G[2] = G[16] + 0x18u; put(rt, G[17] + 0x10u, G[2]); G[4] = G[16]; put(rt, G[17] + 0xCu, G[2]); put(rt, G[17] + 8u, 0u); put(rt, G[17] + 0x14u, 0u); call_here(rt, ctx, 0x089C946Cu); G[4] = G[16] + 0x48u; G[5] = word(rt, G[4] + 8u); call_here(rt, ctx, 0x08C01C34u); G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x3Cu; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x38u; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08C00980u); G[5] = word(rt, G[17] + 8u); G[4] = G[17]; call_here(rt, ctx, 0x08C012FCu); G[4] = G[16]; call_here(rt, ctx, 0x08A780B0u); G[4] = G[16]; G[5] = 0x80u; G[6] = 4u; } G[16] = s0; G[17] = s1; jump(ctx, 0x08A5BC54u); }
void func_08A13F1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD0418u; G[5] = 0u; G[16] = G[4]; G[17] = G[16] + 0x10u; put(rt, G[4], G[2]); G[2] = 0x08D00000u; G[4] = word(rt, 0x08CFFAACu); call_here(rt, ctx, 0x08ACF184u); G[2] = 1u; store8(G[16] + 0x41u, G[2]); G[4] = G[16]; call_here(rt, ctx, 0x08A1371Cu); G[4] = G[17]; call_here(rt, ctx, 0x08B28C74u); G[4] = word(rt, G[16] + 0x3Cu); call_here(rt, ctx, 0x08B1B99Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A12AE0u); G[4] = G[16] + 0x240u; call_here(rt, ctx, 0x08ACEF04u); G[4] = G[16] + 0x200u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x178u; call_here(rt, ctx, 0x08A19DD4u); G[4] = G[16] + 0x140u; call_here(rt, ctx, 0x08A19DD4u); G[4] = G[16] + 0xA0u; call_here(rt, ctx, 0x08B241A0u); G[4] = G[16] + 0x5Cu; call_here(rt, ctx, 0x08C15D30u); G[4] = G[17]; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16]; } G[16] = s0; G[17] = s1; jump(ctx, 0x089C16C8u); }
void func_08A142C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[18] = G[4] + 0xACu; G[17] = G[4]; G[4] = G[18]; G[19] = G[5]; G[16] = G[17] + 0x108u; call_here(rt, ctx, 0x08B34888u); G[4] = word(rt, G[17] + 0x108u); G[3] = 1u; if (G[4] != 0u) { G[4] = G[18]; call_here(rt, ctx, 0x08B34B2Cu); G[4] = word(rt, G[17] + 0x108u); G[3] = 1u; } G[5] = word(rt, G[16]); G[2] = G[3]; put(rt, G[16], G[2]); G[2] = 1u; G[4] = word(rt, G[17] + 0x108u); G[5] = word(rt, G[19]); G[6] = 0x1Fu; G[4] = G[17] + 0x18Du; call_here(rt, ctx, 0x08B7C830u); G[4] = G[18]; call_here(rt, ctx, 0x08B34B24u); G[4] = G[17] + 0xD8u; call_here(rt, ctx, 0x08B28C74u); G[4] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; jump(ctx, 0x08B28C44u); }
void func_08A29780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[5] = static_cast<std::int32_t>(G[5]) < 2 ? 1u : 0u; G[17] = G[6]; if (G[5] == 0u) { G[16] = word(rt, G[6]); G[4] = 0x08CAB9D8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[16] = word(rt, G[17] + 4u); G[4] = 0x08CAB9D8u; G[18] = G[2]; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[2] <<= 10; G[4] = G[18] + G[2]; tail = true; } } G[16] = s0; G[17] = s1; G[18] = s2; if (tail) jump(ctx, 0x08AB9894u); }
void func_08A2D434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[16] = G[5]; G[19] = G[4]; call_here(rt, ctx, 0x08B0979Cu); G[4] = frame.at(0u); G[5] = G[16]; G[18] = G[2]; call_here(rt, ctx, 0x08A2D338u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CAC2A8u; G[17] = frame.at(4u); if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x08A2D1B8u); G[5] = G[2]; G[4] = G[18]; call_here(rt, ctx, 0x08B0A034u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[19]; if (G[19] != G[17]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A812D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[5] = 0x08CB15ECu; G[18] = G[4]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = frame.at(0u); G[16] = G[2]; call_here(rt, ctx, 0x08A86BDCu); G[2] = 0x08CD4658u; put(rt, G[16], G[2]); G[2] = 0x08A80F14u; G[4] = 0x18u; G[5] = 4u; put(rt, G[16] + 0x14u, G[2]); call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[16]; G[17] = G[2]; call_here(rt, ctx, 0x08A86FA8u); G[5] = frame.at(4u); G[4] = G[18]; put(rt, frame.at(4u), G[17]); call_here(rt, ctx, 0x08A76AB4u); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A69D68u); G[2] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A84048(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x30u); G[19] = G[5]; G[18] = word(rt, G[7]); G[16] = word(rt, G[4] + 0x18u); G[17] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CB168Cu; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 0x4Cu); G[4] = G[18]; call_here(rt, ctx, G[2]); G[3] = ~G[2]; G[4] = G[16] >> 1; G[4] >>= (G[3] & 31u); G[7] = G[17] << (G[2] & 31u); G[5] = G[2] & 0x20u; G[6] = G[16] << (G[2] & 31u); G[7] = G[4] | G[7]; if (G[5] != 0u) G[7] = G[6]; G[4] = frame.at(0u); if (G[5] != 0u) G[6] = 0u; call_here(rt, ctx, 0x08A83160u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08A95718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = 0x08D74FD0u; G[2] = G[18] + 0xCu; G[17] = G[4]; G[7] = frame.at(1u); G[4] = frame.at(4u); put(rt, G[17], G[2]); G[2] = word(rt, G[5] + 0x1Cu); G[6] = G[2]; if (G[2] != 0u) { G[5] = word(rt, G[5] + 0x14u); call_here(rt, ctx, 0x08C00AD4u); G[16] = word(rt, frame.at(4u)); G[4] = G[16]; call_here(rt, ctx, 0x08B35F60u); G[6] = G[2]; G[5] = G[16]; G[4] = G[17]; call_here(rt, ctx, 0x08B8A4C0u); G[3] = word(rt, frame.at(4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[2] = G[17]; } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08A9AF28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; G[5] += 1u; { GuestFrame frame(ctx, 0x20u); G[2] = G[5] < 2u ? 1u : 0u; G[18] = G[4]; G[16] = 1u; if (G[2] == 0u) { G[16] <<= 1; for (;;) { G[2] = G[16] < G[5] ? 1u : 0u; if (G[2] == 0u) break; G[16] <<= 1; } } G[4] = frame.at(0u); G[17] = G[18] + 0x14u; call_here(rt, ctx, 0x08A76AA0u); G[4] = word(rt, G[18] + 0x14u); G[7] = word(rt, G[17] + 4u); G[2] = G[7] - G[4]; G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) >> 2); G[3] = G[16] < G[2] ? 1u : 0u; G[5] = G[7]; if (G[3] != 0u) { G[5] = G[16] << 2; G[5] = G[4] + G[5]; G[6] = G[7]; G[4] = G[17]; call_here(rt, ctx, 0x08B85798u); } else { G[6] = G[16] - G[2]; G[4] = G[17]; G[7] = frame.at(0u); call_here(rt, ctx, 0x08B85F30u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AA6CBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; { GuestFrame frame(ctx, 0x20u); G[19] = G[5] & 0xFFu; G[18] = G[4]; G[17] = G[6]; G[16] = G[8]; G[3] = word(rt, G[6]); G[7] -= G[3]; G[2] = G[8] < G[7] ? 1u : 0u; if (G[2] == 0u) G[16] = G[7]; G[5] = G[16]; call_here(rt, ctx, 0x08A951B8u); if (G[16] != 0u) { G[6] = G[16] - 1u; G[7] = 0u; do { G[4] = word(rt, G[18]); G[2] = word(rt, G[17]); G[5] = G[7]; G[3] = word(rt, G[4] + 0x14u); G[2] += G[7]; G[4] = load8(G[2]); if (G[19] != 0u) G[5] = G[6]; G[3] += G[5]; G[7] += 1u; store8(G[3], G[4]); G[6] -= 1u; } while (G[16] != G[7]); } G[3] = word(rt, G[17]); G[2] = G[18]; G[3] += G[16]; put(rt, G[17], G[3]); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; }
void func_08AB4864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[17] = frame.at(4u); G[6] = frame.at(1u); G[5] = 0x08C902BCu; G[18] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B85EB8u); G[4] = 0x28u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x08AB481Cu); G[4] = G[18]; G[5] = frame.at(8u); put(rt, frame.at(8u), G[16]); call_here(rt, ctx, 0x08A76AB4u); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08AC4B78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = float_at(rt, G[4] + 0x25A8u); ctx.fpr[1] = 0.0f; G[1] = 0x3F800000u; ctx.fpr[2] = std::bit_cast<float>(G[1]); const bool up = ctx.fpr[1] < ctx.fpr[0]; ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[12]); if (up) { ctx.fpr[3] = float_at(rt, G[4] + 0x25A0u); ctx.fpr[3] = ctx.fpr[3] + ctx.fpr[0]; const bool top = ctx.fpr[2] <= ctx.fpr[3]; put_float(rt, G[4] + 0x25A0u, ctx.fpr[3]); if (top) { ctx.fpr[3] = ctx.fpr[2]; put_float(rt, G[4] + 0x25A8u, ctx.fpr[1]); put_float(rt, G[4] + 0x25A0u, ctx.fpr[2]); } } else { ctx.fpr[0] = float_at(rt, G[4] + 0x25ACu); const bool down = ctx.fpr[1] < ctx.fpr[0]; ctx.fpr[12] = psp_mul(ctx.fpr[0], ctx.fpr[12]); if (down) { ctx.fpr[3] = float_at(rt, G[4] + 0x25A0u); ctx.fpr[3] = ctx.fpr[3] - ctx.fpr[12]; const bool bot = ctx.fpr[3] <= ctx.fpr[1]; put_float(rt, G[4] + 0x25A0u, ctx.fpr[3]); if (bot) { ctx.fpr[3] = ctx.fpr[1]; put_float(rt, G[4] + 0x25ACu, ctx.fpr[1]); put_float(rt, G[4] + 0x25A0u, ctx.fpr[1]); store8(G[4] + 0x25B2u, 0u); } } else ctx.fpr[3] = float_at(rt, G[4] + 0x25A0u); } ctx.fpr[0] = float_at(rt, G[4] + 0x2598u); ctx.fpr[1] = float_at(rt, G[4] + 0x259Cu); ctx.fpr[2] = float_at(rt, G[4] + 0x25A4u); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[1]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[3]); ctx.fpr[0] = psp_mul(ctx.fpr[0], ctx.fpr[2]); }
void func_08AC4ED4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x20u); G[17] = G[4] + 8u; G[16] = G[4]; G[4] = G[17]; put_float(rt, frame.at(0x10u), ctx.fpr[20]); ctx.fpr[20] = ctx.fpr[12]; call_here(rt, ctx, 0x08B344E0u); ctx.fpr[1] = 0.0f; ctx.fpr[0] = float_at(rt, G[16] + 0x25A0u); bool reset = ctx.fpr[0] <= ctx.fpr[1]; if (!reset) { G[2] = load8(G[16] + 0x25B1u); G[1] = 0x3C800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); const bool big = ctx.fpr[0] < ctx.fpr[20]; G[1] = 0x42800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); if (G[2] != 0u) reset = true; else { put_float(rt, G[16] + 0x25A8u, ctx.fpr[1]); if (big) { G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[20]; } put_float(rt, G[16] + 0x25ACu, ctx.fpr[0]); } } if (reset) { store8(G[16] + 0x25B1u, 0u); put(rt, G[16] + 0x25A0u, 0u); store8(G[16] + 0x25B2u, 0u); put(rt, G[16] + 0x25A8u, 0u); put(rt, G[16] + 0x25ACu, 0u); } G[4] = G[17]; ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } G[16] = s0; G[17] = s1; jump(ctx, 0x08B3457Cu); }
void func_08AC6658(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[16] = G[4]; G[4] = G[4] + 8u; call_here(rt, ctx, 0x08B28DCCu); G[4] = G[16] + 0x30u; call_here(rt, ctx, 0x08B28D34u); G[4] = G[16] + 0x460u; call_here(rt, ctx, 0x08AC646Cu); G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); store8(G[16] + 0x25B1u, 0u); G[4] = G[16] + 0x25C0u; G[5] = G[16] + 0x6A00u; store8(G[16] + 0x25B2u, 0u); G[6] = 2u; put_float(rt, G[16] + 0x25A4u, ctx.fpr[0]); store8(G[16] + 0x25B3u, 0u); put_float(rt, G[16] + 0x259Cu, ctx.fpr[0]); put(rt, G[16] + 0x2598u, 0u); put(rt, G[16] + 0x25A0u, 0u); put(rt, G[16] + 0x25A8u, 0u); put(rt, G[16] + 0x25ACu, 0u); store8(G[16] + 0x25B0u, 0u); store8(G[16] + 0x25B4u, 0u); put(rt, G[16] + 0x25B8u, 0u); put(rt, G[16] + 0x25BCu, 0u); call_here(rt, ctx, 0x08C3CC64u); G[2] = 0xFFFFFFFFu; put(rt, G[16] + 0x25E0u, G[2]); G[4] = G[16] + 0x5Cu; G[5] = 0u; store8(G[16] + 0x25DCu, 0u); G[6] = 0x400u; put(rt, G[16] + 0x25E4u, 0u); call_here(rt, ctx, 0x08B36018u); put(rt, G[16] + 4u, 0u); put(rt, G[16], 0u); } G[16] = s0; }
void func_08AD83B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x830u); G[16] = G[5]; G[5] = 0x2Fu; G[20] = G[4]; G[4] = G[16]; put(rt, frame.at(0x828u), G[10]); put(rt, frame.at(0x82Cu), G[11]); G[19] = G[7]; G[18] = G[6]; G[17] = frame.at(0x400u); put(rt, frame.at(0x820u), G[8]); put(rt, frame.at(0x824u), G[9]); call_here(rt, ctx, 0x08B7C9E4u); G[3] = G[2] + 1u; if (G[2] != 0u) G[16] = G[3]; G[7] = G[16]; G[8] = G[18]; G[9] = G[19]; G[6] = G[20]; G[4] = frame.at(0u); G[5] = 0x08C9084Cu; call_here(rt, ctx, 0x08B37640u); G[6] = frame.at(0x820u); G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B373ECu); G[4] = G[17]; call_here(rt, ctx, 0x08AD8464u); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08AD9958(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08D40000u; G[5] = 0x08D405D0u; G[4] <<= 4; G[3] = word(rt, G[5] + 0x18u); G[16] = G[3] + G[4]; G[2] = word(rt, G[16] + 4u); G[17] = G[5]; if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = word(rt, G[16] + 0xCu); do { G[3] = word(rt, G[16]); G[2] <<= 4; G[3] += G[2]; G[7] = word(rt, G[3] + 0xCu); G[4] = word(rt, G[3]); G[5] = word(rt, G[3] + 4u); G[6] = word(rt, G[3] + 8u); call_here(rt, ctx, 0x08AD98F0u); call_here(rt, ctx, 0x08AEB808u); G[2] = word(rt, G[16] + 0xCu); G[3] = word(rt, G[17] + 0x1Cu); G[2] += 1u; if (G[3] == 0u) { rt.unsupported(0x08AD99C8u, 0x000001CDu, "special? not lowered yet"); return; } if (G[2] == 0x80000000u && G[3] == 0xFFFFFFFFu) { ctx.lo = 0x80000000u; ctx.hi = 0u; } else { ctx.lo = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) / static_cast<std::int32_t>(G[3])); ctx.hi = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[2]) % static_cast<std::int32_t>(G[3])); } G[2] = word(rt, G[16] + 4u); G[2] -= 1u; put(rt, G[16] + 4u, G[2]); G[4] = ctx.hi; put(rt, G[16] + 0xCu, G[4]); call_here(rt, ctx, 0x08AEB82Cu); G[2] = word(rt, G[16] + 4u); if (static_cast<std::int32_t>(G[2]) <= 0) break; G[2] = word(rt, G[16] + 0xCu); } while (true); } } G[16] = s0; G[17] = s1; }
void func_08ADC8DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = G[5]; G[5] = G[7]; G[16] = G[6]; call_here(rt, ctx, 0x08ADB278u); G[7] = G[2]; G[8] = 0u; bool zero = true; if (G[2] != 0u) { G[2] = word(rt, G[2] + 0x3Cu); G[8] = 0u; if (G[2] != 0u) { zero = false; G[3] = word(rt, G[7] + 0x40u); G[6] = 0u; if (static_cast<std::int32_t>(G[3]) > 0) { G[4] = G[2]; G[5] = G[17]; for (;;) { G[2] = word(rt, G[4] + 8u) >> 28; G[2] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[16]) >> (G[2] & 31u)); G[2] &= 1u; G[6] += 1u; if (G[2] != 0u) { G[8] += 1u; if (G[17] != 0u) { G[3] = word(rt, G[4] + 0xCu); G[2] = word(rt, G[7] + 0x14u); G[2] += G[3]; put(rt, G[5], G[2]); G[3] = word(rt, G[7] + 0x40u); } G[5] += 4u; } G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; G[4] += 8u; if (G[2] == 0u) break; } } } } G[2] = zero ? 0u : G[8]; } G[16] = s0; G[17] = s1; }
void func_08B0603C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[18] = G[5]; G[5] = G[6] + 0x38u; G[17] = G[4]; G[16] = G[6]; put_float(rt, frame.at(0x18u), ctx.fpr[22]); ctx.fpr[22] = ctx.fpr[12]; put_float(rt, frame.at(0x14u), ctx.fpr[21]); ctx.fpr[21] = ctx.fpr[13]; put_float(rt, frame.at(0x10u), ctx.fpr[20]); call_here(rt, ctx, 0x08BBBEC4u); G[4] = G[17] + 0xCu; G[5] = G[16] + 0x50u; ctx.fpr[20] = ctx.fpr[0]; call_here(rt, ctx, 0x08BBBEC4u); ctx.fpr[1] = float_at(rt, G[18] + 0x30u); const bool lt = ctx.fpr[1] < ctx.fpr[21]; ctx.fpr[20] = ctx.fpr[20] + ctx.fpr[0]; if (!lt) ctx.fpr[1] = ctx.fpr[21]; ctx.fpr[1] = -ctx.fpr[1]; ctx.fpr[0] = float_at(rt, G[18] + 0x2Cu); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[22]); ctx.fpr[0] = psp_mul(ctx.fpr[20], ctx.fpr[0]); if (!(ctx.fpr[0] < ctx.fpr[1])) ctx.fpr[0] = ctx.fpr[1]; ctx.fpr[0] = ctx.fpr[0] + ctx.fpr[20]; ctx.fpr[22] = float_at(rt, frame.at(0x18u)); ctx.fpr[21] = float_at(rt, frame.at(0x14u)); ctx.fpr[20] = float_at(rt, frame.at(0x10u)); } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B16BB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x20u); G[20] = G[5]; G[16] = G[4]; G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xA0u); G[17] = word(rt, G[5] + 0xA0u); call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[19] = G[2]; G[5] = G[17]; G[2] = word(rt, G[3] + 0xA0u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08B17A98u); G[4] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x08B17A98u); G[6] = word(rt, G[16]); G[4] = G[16]; G[5] = G[17]; G[3] = word(rt, G[6] + 0xB4u); G[6] = G[2]; call_here(rt, ctx, G[3]); put(rt, G[20] + 0xA8u, G[2]); G[4] = G[19]; call_here(rt, ctx, 0x08B17AA8u); G[4] = G[18]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; jump(ctx, 0x08B17AA8u); }
void func_08B1E390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; { GuestFrame frame(ctx, 0x10u); G[2] = G[5] & 0xFFu; G[6] = 0xAu; G[5] = 0u; G[16] = G[4]; G[3] = G[2]; if (G[4] != 0u) { call_here(rt, ctx, 0x08B7DF7Cu); G[3] = 1u; if (G[2] == 0u) { static constexpr std::uint32_t strs[4] = {0x08CB5374u, 0x08CB537Cu, 0x08CB5380u, 0x08CB5388u}; bool hit = false; for (int i = 0; i < 4; ++i) { G[5] = strs[i]; G[4] = G[16]; call_here(rt, ctx, 0x08B7B8C4u); if (G[2] == 0u) { hit = true; break; } } G[3] = hit ? 1u : 0u; } } G[2] = G[3]; } G[16] = s0; }
void func_08B217AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; const std::uint32_t s5 = G[21]; const std::uint32_t s6 = G[22]; { GuestFrame frame(ctx, 0x20u); G[3] = G[6] + G[8]; G[22] = G[5]; G[21] = G[7]; G[20] = G[8]; G[19] = G[4]; G[2] = load8(G[4] + 4u); G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; G[18] = G[6]; if (G[2] != 0u) store8(G[4] + 4u, G[3]); if (static_cast<std::int32_t>(G[18]) > 0) { G[17] = 0u; G[5] = G[21] + G[17]; do { G[4] = G[22]; call_here(rt, ctx, 0x08B21410u); G[16] = G[17] + G[20]; G[4] = G[19]; G[17] += 1u; G[5] = G[16]; G[6] = G[2]; call_here(rt, ctx, 0x08B21758u); G[5] = G[21] + G[17]; } while (G[18] != G[17]); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; G[21] = s5; G[22] = s6; }
void func_08B222B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; bool tail = false; { GuestFrame frame(ctx, 0x10u); G[2] = 0x08CD9538u; G[18] = G[6]; G[17] = G[5]; G[16] = G[4]; put(rt, G[4], G[2]); G[4] += 4u; call_here(rt, ctx, 0x08B28DCCu); G[5] = G[16]; G[4] = G[16] + 0x34u; put(rt, G[16] + 0x2Cu, G[17]); put(rt, G[16] + 0x30u, G[18]); call_here(rt, ctx, 0x08C4FB18u); G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08C4FB30u); G[3] = word(rt, G[16] + 0x30u); G[2] = 0x08D68B7Cu; G[5] = G[16]; G[4] = G[3]; put(rt, G[16] + 0x60u, G[2]); put(rt, G[16] + 0x5Cu, G[2]); put(rt, G[16] + 0x64u, 0u); put(rt, G[16] + 0x68u, 0u); put(rt, G[16] + 0x6Cu, 0u); tail = G[3] != 0u; } G[16] = s0; G[17] = s1; G[18] = s2; if (tail) jump(ctx, 0x08B221B4u); }
void func_08B33950(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[17] = 0x08D70000u; G[16] = G[4]; G[4] = 0x08D69214u; call_here(rt, ctx, 0x08B34D40u); G[4] = G[16]; call_here(rt, ctx, 0x08B33898u); G[16] = G[2]; if (static_cast<std::int32_t>(G[2]) >= 0) { G[2] = 0x08D70000u; G[3] = word(rt, 0x08D6925Cu); G[5] = G[3] - 1u; G[4] = static_cast<std::int32_t>(G[16]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; put(rt, 0x08D6925Cu, G[5]); if (G[4] != 0u) { G[3] = G[16] + 1u; G[3] <<= 2; G[2] = G[16] << 2; G[7] = G[2] - G[3]; G[2] = 0x08D6921Cu; G[6] = G[5] - G[16]; G[4] = G[3] + G[2]; G[5] = 0u; do { G[3] = word(rt, G[4]); G[2] = G[4] + G[7]; G[5] += 1u; put(rt, G[2], G[3]); G[4] += 4u; } while (G[5] != G[6]); G[16] += G[5]; } } G[4] = 0x08D69214u; call_here(rt, ctx, 0x08B34D80u); G[2] = ~G[16]; G[2] >>= 31; } G[16] = s0; G[17] = s1; }
void func_08B3F098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[8] = word(rt, G[4] + 0x24u); G[3] = 0u; if (G[8] == 0u) { G[2] = 0u; return; } G[2] = load8(G[4] + 0x20u); G[9] = 0u; G[5] = G[4]; G[3] = 0u; if (G[2] == 0u) { for (;;) { G[3] += 1u; if (G[8] == G[3]) { G[3] = 0u; G[2] = 0u; return; } G[2] = load8(G[5] + 0x21u); G[5] += 1u; if (G[2] != 0u) break; } G[9] = G[3]; if (static_cast<std::int32_t>(G[3]) < 0) { G[3] = 0u; G[2] = 0u; return; } } G[2] = G[9] << 2; G[2] += G[4]; G[4] = word(rt, G[2]); if (G[4] == 0u) { G[3] = 0u; G[2] = 0u; return; } G[3] = word(rt, G[4]); for (;;) { G[2] = word(rt, G[3] + 0x38u); if (G[2] == G[6]) { G[2] = word(rt, G[3] + 0x3Cu); if (G[2] == G[7]) { G[2] = G[3]; return; } } G[4] = word(rt, G[4] + 8u); if (G[4] == 0u) { G[3] = 0u; G[2] = 0u; return; } G[3] = word(rt, G[4]); } }
void func_08B43370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[17] = sext(G[5] & 0xFFFFu, 16); G[2] = static_cast<std::int32_t>(G[17]) < 0x20 ? 1u : 0u; G[18] = sext(G[6] & 0xFFFFu, 16); G[16] = G[4]; if (G[2] == 0u || static_cast<std::int32_t>(G[18]) < 0) G[2] = 0u; else { G[3] = word(rt, G[4]); put(rt, frame.at(0u), 0u); put(rt, frame.at(4u), 0u); put(rt, frame.at(8u), 0u); G[2] = word(rt, G[3] + 0xA0u); call_here(rt, ctx, G[2]); G[2] ^= 1u; store8(frame.at(0u), G[2]); G[4] = G[16]; G[5] = 0x4E2Bu; G[6] = frame.at(0u); G[7] = 0xCu; G[8] = 1u; G[9] = 1u; G[10] = 0u; G[11] = 0u; G[2] = 0xFFFFFFFFu; put(rt, frame.at(4u), G[2]); store16(frame.at(8u), G[17]); store16(frame.at(0xAu), G[18]); call_here(rt, ctx, 0x08B42A90u); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08B59F6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[2] = 1u; G[17] = G[4]; if (G[5] != 0u) { G[16] = load8(G[4] + 0x202u); G[2] = 0xFFu; G[4] = G[16] << 4; bool found = false; if (G[16] != G[2]) { G[2] = G[4] + G[17]; G[3] = word(rt, G[2] + 8u); found = G[5] == G[3]; while (!found) { G[16] = load8(G[2] + 3u); G[2] = 0xFFu; G[4] = G[16] << 4; G[3] = G[4] + G[17]; if (G[16] == G[2]) break; G[2] = word(rt, G[3] + 8u); if (G[5] == G[2]) { found = true; break; } G[2] = G[4] + G[17]; } } if (found) { G[6] = G[16]; G[4] = G[17]; G[5] = G[17] + 0x202u; call_here(rt, ctx, 0x08B59BC0u); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08B59D6Cu); G[2] = 1u; } else G[2] = 0u; } } G[16] = s0; G[17] = s1; }
void func_08B8D470(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x30u); G[18] = frame.at(8u); G[20] = G[4]; G[4] = G[18]; G[19] = G[7]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x08A98220u); G[2] = 0x08CBEB68u; put(rt, G[17], G[2]); G[16] = frame.at(4u); G[4] = G[18]; put(rt, G[17] + 0x14u, G[19]); call_here(rt, ctx, 0x08A69D68u); G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[17]); call_here(rt, ctx, 0x08A5D414u); G[4] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x08A7C5C8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08BEEB08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x30u); G[18] = frame.at(8u); G[20] = G[4]; G[4] = G[18]; G[19] = G[8]; call_here(rt, ctx, 0x08A69B3Cu); G[4] = 0x18u; G[5] = 4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = G[2]; G[5] = G[18]; G[17] = G[2]; call_here(rt, ctx, 0x08A98220u); G[2] = 0x08CCE118u; put(rt, G[17], G[2]); G[16] = frame.at(4u); G[4] = G[18]; put(rt, G[17] + 0x14u, G[19]); call_here(rt, ctx, 0x08A69D68u); G[4] = G[16]; G[5] = frame.at(0u); put(rt, frame.at(0u), G[17]); call_here(rt, ctx, 0x08A5D414u); G[4] = G[20]; G[5] = G[16]; call_here(rt, ctx, 0x08A7C5C8u); G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }
void func_08B951FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; { GuestFrame frame(ctx, 0x20u); G[7] = G[5]; G[17] = G[4]; G[5] = frame.at(0u); G[6] = word(rt, G[4]); G[3] = G[6] - 0xCu; G[2] = word(rt, G[3] + 4u); G[4] = G[3]; bool go = true; if (G[2] == G[7]) { G[2] = word(rt, G[3] + 8u); if (static_cast<std::int32_t>(G[2]) <= 0) go = false; } if (go) { G[6] = word(rt, G[6] - 0xCu); G[2] = G[7] < G[6] ? 1u : 0u; if (G[2] != 0u) G[7] = G[6]; G[6] = G[7] - G[6]; call_here(rt, ctx, 0x08B94288u); G[3] = word(rt, G[17]); G[18] = G[2]; G[2] = 0x08D75298u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B93554u); } put(rt, G[17], G[18]); } } G[16] = s0; G[17] = s1; G[18] = s2; }
void func_08BBB988(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; { GuestFrame frame(ctx, 0x10u); G[5] <<= 2; G[16] = G[6]; G[2] = word(rt, G[4] + 4u); G[4] = word(rt, G[4] + 0x1Cu); G[17] = G[2] + G[5]; G[5] = sext(load16(G[17]), 16); call_here(rt, ctx, 0x08B03BB4u); G[3] = load16(G[2] + 0xE2u); G[4] = load16(G[17] + 2u); G[5] = G[2] + 0xB0u; G[3] &= 1u; G[4] <<= 17; if (G[3] == 0u) G[5] = 0u; G[4] = static_cast<std::uint32_t>(static_cast<std::int32_t>(G[4]) >> 17); G[3] = G[2] + 0x50u; G[6] = G[2] + 0x20u; G[7] = G[2] + 0x38u; put(rt, G[16] + 0x14u, G[3]); store16(G[16], G[4]); put(rt, G[16] + 8u, G[6]); put(rt, G[16] + 0xCu, G[5]); put(rt, G[16] + 0x10u, G[7]); store16(G[16] + 2u, 0u); put(rt, G[16] + 4u, G[2]); G[3] = load16(G[2] + 0xE2u) & 4u; G[8] = 0u; if (G[3] != 0u) { G[2] = load16(G[17] + 2u) & 0x8000u; if (G[2] != 0u) G[8] = 1u; } G[2] = G[8]; } G[16] = s0; G[17] = s1; }
void func_08BD5D54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; const std::uint32_t s5 = G[21]; { GuestFrame frame(ctx, 0x30u); G[21] = G[4] + 0x98u; G[2] = word(rt, G[4] + 0x98u); G[19] = G[2]; if (G[2] != G[21]) { G[20] = word(rt, G[5]); G[18] = G[2]; for (;;) { G[2] = word(rt, G[18] + 8u); G[5] = G[20]; G[17] = word(rt, G[2] - 0xCu); G[2] = frame.at(4u); put(rt, frame.at(0u), G[17]); G[16] = word(rt, G[20] - 0xCu); G[3] = G[16] < G[17] ? 1u : 0u; put(rt, frame.at(4u), G[16]); if (G[3] == 0u) G[2] = frame.at(0u); G[6] = word(rt, G[2]); G[4] = word(rt, G[18] + 8u); call_here(rt, ctx, 0x08B36170u); if (G[2] == 0u) { G[2] = G[19]; if (G[17] == G[16]) break; } G[19] = word(rt, G[18]); if (G[21] == G[19]) break; G[18] = G[19]; } } G[2] = G[19]; } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; G[21] = s5; }
void func_08BDA6D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const std::uint32_t s0 = G[16]; const std::uint32_t s1 = G[17]; const std::uint32_t s2 = G[18]; const std::uint32_t s3 = G[19]; const std::uint32_t s4 = G[20]; { GuestFrame frame(ctx, 0x30u); G[19] = word(rt, G[6]); G[18] = G[4]; G[16] = G[5]; store8(frame.at(0u), G[7]); for (;;) { for (;;) { G[20] = word(rt, G[18]); G[5] = G[19]; G[4] = G[20]; call_here(rt, ctx, 0x08B35FD8u); if (static_cast<std::int32_t>(G[2]) >= 0) break; G[18] += 8u; } G[16] -= 8u; for (;;) { G[17] = word(rt, G[16]); G[4] = G[19]; G[5] = G[17]; call_here(rt, ctx, 0x08B35FD8u); if (static_cast<std::int32_t>(G[2]) >= 0) break; G[16] -= 8u; } G[2] = G[18] < G[16] ? 1u : 0u; if (G[2] == 0u) { G[2] = G[18]; break; } G[2] = word(rt, G[16] + 4u); G[4] = word(rt, G[18] + 4u); put(rt, G[18], G[17]); put(rt, G[18] + 4u, G[2]); G[18] += 8u; put(rt, G[16] + 4u, G[4]); put(rt, G[16], G[20]); } } G[16] = s0; G[17] = s1; G[18] = s2; G[19] = s3; G[20] = s4; }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_208
} // namespace gt::game

#define HOT208(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_208::name, gt::ReturnKind::kind)
HOT208(0x0882AD6Cu, func_0882AD6C, Word);
HOT208(0x088375E0u, func_088375E0, Word);
HOT208(0x08B956D8u, func_08B956D8, Word);
HOT208(0x08BC04A8u, func_08BC04A8, Word);
// HOT208(0x08BEB9E4u, func_08BEB9E4, Word);  // no comparable synthetic case yet
HOT208(0x08C0E1ACu, func_08C0E1AC, Word);
HOT208(0x08C39C2Cu, func_08C39C2C, DoubleWord);
// HOT208(0x08BF863Cu, func_08BF863C, Word);  // no comparable synthetic case yet
// HOT208(0x08BFDE18u, func_08BFDE18, Word);  // no comparable synthetic case yet
// HOT208(0x08C0632Cu, func_08C0632C, Word);  // no comparable synthetic case yet
// HOT208(0x08C07D78u, func_08C07D78, Word);  // no comparable synthetic case yet
HOT208(0x0882AEB8u, func_0882AEB8, Word);
HOT208(0x0882B090u, func_0882B090, Word);
HOT208(0x0882BBBCu, func_0882BBBC, Word);
HOT208(0x08BEEC3Cu, func_08BEEC3C, Word);
HOT208(0x08C308D4u, func_08C308D4, Word);
HOT208(0x08BC05FCu, func_08BC05FC, Word);
HOT208(0x08837738u, func_08837738, Word);
HOT208(0x08C0E30Cu, func_08C0E30C, Word);
HOT208(0x08C39E0Cu, func_08C39E0C, DoubleWord);
HOT208(0x08BC789Cu, func_08BC789C, Word);
// HOT208(0x0881EF0Cu, func_0881EF0C, Word);  // no comparable synthetic case yet
HOT208(0x0882B698u, func_0882B698, Word);
HOT208(0x0883768Cu, func_0883768C, Word);
HOT208(0x0886A658u, func_0886A658, Word);
HOT208(0x088853F0u, func_088853F0, Word);
HOT208(0x088875CCu, func_088875CC, Word);
HOT208(0x088CD62Cu, func_088CD62C, Word);
HOT208(0x088D07F0u, func_088D07F0, Word);
// HOT208(0x088D0CCCu, func_088D0CCC, Word);  // no comparable synthetic case yet
// HOT208(0x088E649Cu, func_088E649C, Word);  // no comparable synthetic case yet
HOT208(0x088E7004u, func_088E7004, Word);
HOT208(0x088F8688u, func_088F8688, Word);
HOT208(0x08926564u, func_08926564, Word);
HOT208(0x08929F14u, func_08929F14, Word);
HOT208(0x0893C870u, func_0893C870, Word);
HOT208(0x08949DE0u, func_08949DE0, Word);
// HOT208(0x089C9528u, func_089C9528, Word);  // no comparable synthetic case yet
// HOT208(0x08A13F1Cu, func_08A13F1C, Word);  // no comparable synthetic case yet
// HOT208(0x08A142C8u, func_08A142C8, Word);  // no comparable synthetic case yet
HOT208(0x08A29780u, func_08A29780, Word);
// HOT208(0x08A2D434u, func_08A2D434, Word);  // no comparable synthetic case yet
HOT208(0x08A812D4u, func_08A812D4, Word);
HOT208(0x08A84048u, func_08A84048, Word);
HOT208(0x08A95718u, func_08A95718, Word);
// HOT208(0x08A9AF28u, func_08A9AF28, Word);  // no comparable synthetic case yet
HOT208(0x08AA6CBCu, func_08AA6CBC, Word);
HOT208(0x08AB4864u, func_08AB4864, Word);
HOT208(0x08AC4B78u, func_08AC4B78, Float);
HOT208(0x08AC4ED4u, func_08AC4ED4, Word);
HOT208(0x08AC6658u, func_08AC6658, Word);
// HOT208(0x08AD83B8u, func_08AD83B8, Word);  // no comparable synthetic case yet
HOT208(0x08AD9958u, func_08AD9958, Word);
HOT208(0x08ADC8DCu, func_08ADC8DC, Word);
HOT208(0x08B0603Cu, func_08B0603C, Float);
HOT208(0x08B16BB0u, func_08B16BB0, Word);
HOT208(0x08B1E390u, func_08B1E390, Word);
HOT208(0x08B217ACu, func_08B217AC, Word);
HOT208(0x08B222B0u, func_08B222B0, Word);
HOT208(0x08B33950u, func_08B33950, Word);
HOT208(0x08B3F098u, func_08B3F098, Word);
HOT208(0x08B43370u, func_08B43370, Word);
// HOT208(0x08B59F6Cu, func_08B59F6C, Word);  // no comparable synthetic case yet
HOT208(0x08B8D470u, func_08B8D470, Word);
HOT208(0x08BEEB08u, func_08BEEB08, Word);
HOT208(0x08B951FCu, func_08B951FC, Word);
// HOT208(0x08BBB988u, func_08BBB988, Word);  // no comparable synthetic case yet
// HOT208(0x08BD5D54u, func_08BD5D54, Word);  // no comparable synthetic case yet
// HOT208(0x08BDA6D8u, func_08BDA6D8, Word);  // no comparable synthetic case yet
