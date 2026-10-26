// Hot batch 135: functions only the routes that start from the user's finished save reach (spec
// docs/superpowers/specs/2026-10-23-savedata-routes-design.md), written by hand from `tools.disasm`; role words describe
// access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_135 {
namespace {

using namespace gt::game::hot;

inline void take(psprecomp::AllegrexContext &ctx, const GuestResult &result) {
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 of the callee stay in the registers.
inline GuestResult call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                            std::initializer_list<std::uint32_t> arguments, std::initializer_list<float> floats = {}) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, floats);
    take(ctx, result);
    return result;
}

// Assigns the string `local` to *out (kAssign) unless out is the local itself, then destroys the local: the common
// ending of the string getters below.
void assign_and_destroy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t out, std::uint32_t local) {
    ctx.gpr[4] = out;
    if (out != local) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, local)});
    call_set(rt, ctx, 0x08A5B9ECu, {local});
}


// Releases the shared string whose data pointer sits at `slot` (the libstdc++ copy-on-write layout: the rep 0xC bytes
// before the data, its count at -4): unless it is the empty rep 0x08D74FD0, func_08B60804(&count, -1) steps the count
// and func_08B8542C(rep, frame) frees it when the old count was not positive. (As in hot_batch_128.)
void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t frame_base) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    ctx.gpr[4] = text - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (rep == 0x08D74FD0u) return;
    if (static_cast<std::int32_t>(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}).v0) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {rep, frame_base});
}

} // namespace

// A constant: 0x65.
void func_088A57F4(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = 0x65u; }

// The object 8 bytes before the pointer.
void func_08B2A3B0(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = ctx.gpr[4] - 8u; }

// A float constant from .rodata (0x08CF33CC); v0 keeps the address's high half, as the lui leaves it.
void func_08954AE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0x00010000u;
    ctx.fpr[0] = float_at(rt, 0x08CF33CCu);
}

// Virtual slot 0x110 of the object `*a0` points to, with a2 = 1 (a tail jump: a0 and a1 pass through).
void func_08AA19C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x110u);
    ctx.gpr[25] = ctx.gpr[2];
    ctx.gpr[6] = 1u;
    tail_call(ctx, ctx.gpr[25], {});
}

// Clears the word +0xEC and the byte +0xF0, then tail-jumps to func_08895AC4(self + 0xB4); v0 = self.
void func_0889605C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = self;
    put(rt, self + 0xECu, 0u);
    rt.memory().store8(self + 0xF0u, 0u);
    tail_call(ctx, 0x08895AC4u, {self + 0xB4u});
}

// The entry `*a0` + 0x38 + 0x180 * (signed byte +8 of `*a0`): (b << 9) - (b << 7) = 0x180 * b.
void func_088A65E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t base = word(rt, ctx.gpr[4]);
    const std::uint32_t index = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(rt.memory().load8(base + 8u))));
    ctx.gpr[3] = base;
    ctx.gpr[4] = index << 7u;
    ctx.gpr[2] = (index << 9u) - (index << 7u) + base + 0x38u;
}

// Sets the byte +0xF0, clears the word +0xF4, then tail-jumps on the word +0xEC: zero to func_08895BFC(self + 0xB4),
// otherwise func_08894E24(self + 0xB4). v0 = 1, v1 = the word, a1 = self.
void func_088963C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t state = word(rt, self + 0xECu);
    ctx.gpr[3] = state;
    ctx.gpr[5] = self;
    ctx.gpr[2] = 1u;
    rt.memory().store8(self + 0xF0u, 1u);
    put(rt, self + 0xF4u, 0u);
    tail_call(ctx, state != 0u ? 0x08894E24u : 0x08895BFCu, {self + 0xB4u});
}

// func_08C24C48(a0, &local) and the local's first byte.
void func_08C24C50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08C24C48u, {ctx.gpr[4], frame.at(0u)});
    ctx.gpr[2] = rt.memory().load8(frame.at(0u));
}

// func_08C3134C(a0, &local) and the local's word.
void func_08C31354(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08C3134Cu, {ctx.gpr[4], frame.at(0u)});
    ctx.gpr[2] = word(rt, frame.at(0u));
}

// The 8-byte entry self + 0x354 + 8 * func_0881AD9C(self), or 0 when the index is negative.
void func_0881AEC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t index = call_set(rt, ctx, 0x0881AD9Cu, {self}).v0;
    const std::uint32_t entry = (index << 3u) + self;
    ctx.gpr[3] = entry;
    ctx.gpr[4] = static_cast<std::int32_t>(index) < 0 ? 0u : entry + 0x354u;
    ctx.gpr[2] = ctx.gpr[4];
}

// Stores the float virtual slot 0x9C of the object at +0xB04 of a1 returns (called with a1 = 0) into *a0.
void func_08899FA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[4];
    const std::uint32_t object = word(rt, ctx.gpr[5] + 0xB04u);
    ctx.gpr[6] = object;
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x9Cu);
    const GuestResult result = call_set(rt, ctx, ctx.gpr[2], {object, 0u});
    put(rt, out, std::bit_cast<std::uint32_t>(result.f0));
}

// A variable-length number from the reader func_08C24C50(a0): the first byte's leading 1 bits after the top one say
// how many more bytes follow (UTF-8 style); each byte adds 8 bits and the mask keeps the value bits.
void func_08A71C1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t reader = ctx.gpr[4];
    std::uint32_t mask = 0x7Fu;
    std::uint32_t value = call_set(rt, ctx, 0x08C24C50u, {reader}).v0;
    if ((value & 0x80u) != 0u) {
        std::uint32_t lead = value;
        std::uint32_t more = 0u;
        do {
            const std::uint32_t next = call_set(rt, ctx, 0x08C24C50u, {reader}).v0;
            lead <<= 1u;
            ctx.gpr[4] = value << 8u;
            ctx.gpr[3] = (mask >> 1u) << 8u;
            more = lead & 0x80u;
            ctx.gpr[5] = more;
            value = (value << 8u) | next;
            mask = ctx.gpr[3] | 0xFFu;
        } while (more != 0u);
    }
    ctx.gpr[2] = mask & value;
}

// The state byte at +0x778 of the object `*(a0 + 8)` points to, from its float +0x6C4 against two thresholds of the
// table at 0x08CF334C (+0x70: 4, +0x38: 2, else 0); byte +0x779 cleared and the state's 28-byte table row (0x08CF334C +
// 28 * state) copied: words +8, +0xC, +0x14, +0x18 to +0x77C..+0x788.
void func_08954AEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    constexpr std::uint32_t kTable = 0x08CF334Cu;
    const std::uint32_t object = word(rt, ctx.gpr[4] + 8u);
    const std::uint32_t out = object + 0x778u;
    const float value = float_at(rt, object + 0x6C4u);
    ctx.fpr[1] = value;
    if (float_at(rt, kTable + 0x70u) <= value) {
        rt.memory().store8(out, 4u);
    } else if (float_at(rt, kTable + 0x38u) <= value) {
        rt.memory().store8(out, 2u);
    } else {
        rt.memory().store8(out, 0u);
    }
    const std::uint32_t state = rt.memory().load8(out);
    rt.memory().store8(out + 1u, 0u);
    const std::uint32_t row = (state << 5u) - (state << 2u) + kTable;
    put(rt, out + 0x4u, word(rt, row + 0x8u));
    put(rt, out + 0x8u, word(rt, row + 0xCu));
    put(rt, out + 0xCu, word(rt, row + 0x14u));
    put(rt, out + 0x10u, word(rt, row + 0x18u));
    ctx.fpr[0] = float_at(rt, row + 0x14u);
    ctx.fpr[1] = float_at(rt, row + 0x18u);
    ctx.gpr[2] = row;
    ctx.gpr[3] = state << 2u;
    ctx.gpr[4] = kTable;
    ctx.gpr[5] = out;
}

// f12 unless the object at +8 has signed byte +3 != 2 and byte +0xE == 0 and func_089445D8(self, 2) > 0: then the
// float at 0x08CF2AB0 + 4 * (n / 5), n = func_089444F8(self, 2, 0, 0) clamped to 0 (below 5) .. 45 (from 51), minus 5.
void func_089448D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    float result = ctx.fpr[12];
    const std::uint32_t object = word(rt, self + 8u);
    const auto kind = static_cast<std::int8_t>(rt.memory().load8(object + 3u));
    if (kind != 2 && rt.memory().load8(object + 0xEu) == 0u) {
        const auto count = static_cast<std::int32_t>(call_set(rt, ctx, 0x089445D8u, {self, 2u}).v0);
        ctx.gpr[4] = self;
        ctx.gpr[5] = 2u;
        ctx.gpr[6] = 0u;
        ctx.gpr[7] = 0u;
        if (count > 0) {
            const auto n = static_cast<std::int32_t>(call_set(rt, ctx, 0x089444F8u, {self, 2u, 0u, 0u}).v0);
            const std::int32_t clamped = n < 5 ? 0 : (n < 0x33 ? n - 5 : 0x2D);
            result = float_at(rt, 0x08CF2AB0u + 4u * static_cast<std::uint32_t>(clamped / 5));
        }
    }
    ctx.fpr[0] = result;
}

// Reads 4 bytes through the stream's virtual slot 0x110 (self, &local, 4) and stores them in *a1 as a big-endian word
// (zero first, as the original). Two functions with this exact body: 0x08AA1B84 and 0x08AA1C2C.
void read_big_endian_word(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[6] = 4u;
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x110u);
    call_set(rt, ctx, ctx.gpr[2], {ctx.gpr[4], frame.at(0u), 4u});
    put(rt, out, 0u);
    std::uint32_t value = 0u;
    for (std::uint32_t i = 0; i < 4u; ++i) value = (value << 8u) | rt.memory().load8(frame.at(i));
    put(rt, out, value);
    ctx.gpr[2] = value;
    ctx.gpr[4] = value;
    ctx.gpr[3] = frame.at(4u);
    ctx.gpr[5] = frame.at(4u);
}

// The object at +4 (func_08A5BB7C reports 0x08CB30B0 when it is null), then a tail jump to `target`(object, a1).
void forward_checked(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target) {
    const std::uint32_t argument = ctx.gpr[5];
    const std::uint32_t object = word(rt, ctx.gpr[4] + 4u);
    ctx.gpr[4] = 0x08CB30B0u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    tail_call(ctx, target, {object, argument});
}
void func_08AA9288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C31344u); }
void func_08AA91F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C3134Cu); }

// Constructor: vtable 0x08D74FDC, then func_08C313B4(a1, self); returns self.
void func_08C33360(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, 0x08D74FDCu);
    call_set(rt, ctx, 0x08C313B4u, {ctx.gpr[5], self});
    ctx.gpr[2] = self;
}

// A string built by func_08A76AA0 into a local; assigned to *a2 (kAssign) unless a2 is that local, then destroyed.
void func_08C32874(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    assign_and_destroy(rt, ctx, out, frame.at(0u));
}

// Virtual slot 0x164 of the object at +0x14 (func_08A5BB7C reports 0x08C97DC4 when it is null).
void func_08B86D30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4] + 0x14u);
    ctx.gpr[4] = 0x08C97DC4u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C97DC4u});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x164u);
    call_set(rt, ctx, ctx.gpr[2], {object});
}

// When the word +0x48 is set: func_08B2BB74(self), then a tail jump to func_08B2BB44(self).
void func_08B2BC5C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    ctx.gpr[2] = word(rt, self + 0x48u);
    if (ctx.gpr[2] == 0u) return;
    call_set(rt, ctx, 0x08B2BB74u, {self});
    tail_call(ctx, 0x08B2BB44u, {self});
}

// A new 0x20-byte object (func_08A5BC20(0x20, 4), constructed by func_08A44460) handed to func_08A76AB4(self, &it).
void func_08A44CCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x20u, 4u}).v0;
    call_set(rt, ctx, 0x08A44460u, {object});
    put(rt, frame.at(0u), object);
    call_set(rt, ctx, 0x08A76AB4u, {self, frame.at(0u)});
}

// Constructor: base func_08A719F0(self, a1), vtable 0x08CD3B28, a string at +0x1C (func_08A76AA0), func_08A94CE8 at
// +0x20, the word +0x24 cleared, then a tail jump to func_08A93CC8(self, a1).
void func_08A93D24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t argument = ctx.gpr[5];
    call_set(rt, ctx, 0x08A719F0u, {self});
    put(rt, self, 0x08CD3B28u);
    ctx.gpr[2] = 0x08CD3B28u;
    call_set(rt, ctx, 0x08A76AA0u, {self + 0x1Cu});
    call_set(rt, ctx, 0x08A94CE8u, {self + 0x20u});
    put(rt, self + 0x24u, 0u);
    tail_call(ctx, 0x08A93CC8u, {self, argument});
}

// A new 0x28-byte object (func_08A5BC20(0x28, 4), constructed by func_08A93D24(it, a1)) handed to
// func_08A71E10(self, &it).
void func_08A9411C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x28u, 4u}).v0;
    call_set(rt, ctx, 0x08A93D24u, {object, argument});
    put(rt, frame.at(0u), object);
    call_set(rt, ctx, 0x08A71E10u, {self, frame.at(0u)});
}

// func_08817A54(a0, a1); then virtual slot 0xDC(object, 0) of the object *a1 points to (func_08A5BB7C reports
// 0x08C98D34 when it is null); a tail jump to func_08817C2C(first result, second result).
void func_08817EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[5];
    const std::uint32_t first = call_set(rt, ctx, 0x08817A54u, {}).v0;
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[4] = 0x08C98D34u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C98D34u});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xDCu);
    const std::uint32_t second = call_set(rt, ctx, ctx.gpr[2], {object, 0u}).v0;
    tail_call(ctx, 0x08817C2Cu, {first, second});
}

// *target = virtual slot 0x3C of the object *a2 (when there is one); target is the word +0x1C of a1, or else the word
// +0xC of the object at +0x18 (func_08A5BB7C reports 0x08CAF9AC when that is null).
void func_08A598FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t owner = ctx.gpr[5];
    const std::uint32_t source_holder = ctx.gpr[6];
    std::uint32_t target = word(rt, owner + 0x1Cu);
    ctx.gpr[2] = target;
    if (target == 0u) {
        const std::uint32_t object = word(rt, owner + 0x18u);
        ctx.gpr[4] = 0x08CAF9ACu;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF9ACu});
        target = word(rt, object + 0xCu);
    }
    const std::uint32_t source = word(rt, source_holder);
    ctx.gpr[3] = source;
    ctx.gpr[4] = source;
    if (source == 0u) return;
    ctx.gpr[3] = word(rt, source);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    put(rt, target, call_set(rt, ctx, ctx.gpr[2], {source}).v0);
}

// When func_08891568(self, a1) says so: on the object *self + 0x14, func_08891D60(it, 1), func_08891D28(it, a1 & 0xFF)
// and a tail jump to func_08891984(it).
void func_08891E08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t flag = ctx.gpr[5] & 0xFFu;
    const std::uint32_t ok = call_set(rt, ctx, 0x08891568u, {}).v0;
    ctx.gpr[5] = 1u;
    if (ok == 0u) return;
    call_set(rt, ctx, 0x08891D60u, {word(rt, self) + 0x14u, 1u});
    call_set(rt, ctx, 0x08891D28u, {word(rt, self) + 0x14u, flag});
    tail_call(ctx, 0x08891984u, {word(rt, self) + 0x14u});
}

// A string local filled by func_08A77C54(*(self + 4), &it, 0x08CF6FE0, 0, 0); its body pointer (func_08A5BB7C reports
// 0x08CAF878 when null) gets func_08A5BA28; the string is destroyed and the pointer returned.
void func_08A58BB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    call_set(rt, ctx, 0x08A77C54u, {word(rt, self + 4u), frame.at(0u), 0x08CF6FE0u, 0u, 0u});
    const std::uint32_t body = word(rt, frame.at(0u));
    ctx.gpr[4] = 0x08CAF878u;
    if (body == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF878u});
    call_set(rt, ctx, 0x08A5BA28u, {body});
    const std::uint32_t result = word(rt, frame.at(0u));
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    ctx.gpr[2] = result;
}

// n = func_08B2A680(a2, a3, t0, 0) with f12 = 0x3EA770CA, f13 = 0x3F5BC524 (bit patterns); n < 0 gives 0, otherwise
// func_08B79A88(self, a2 + 8, n) and 1.
void func_08B2A7EC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t source = ctx.gpr[6];
    ctx.fpr[12] = std::bit_cast<float>(0x3EA770CAu);
    ctx.fpr[13] = std::bit_cast<float>(0x3F5BC524u);
    const auto n = static_cast<std::int32_t>(
        call_set(rt, ctx, 0x08B2A680u, {source, ctx.gpr[7], ctx.gpr[8], 0u}, {ctx.fpr[12], ctx.fpr[13]}).v0);
    std::uint32_t result = 0u;
    if (n >= 0) {
        call_set(rt, ctx, 0x08B79A88u, {self, source + 8u, static_cast<std::uint32_t>(n)});
        result = 1u;
    } else {
        ctx.gpr[4] = self;
        ctx.gpr[5] = source + 8u;
        ctx.gpr[6] = static_cast<std::uint32_t>(n);
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}


// h = func_08AA9AC8(self, func_08AA8F38(self, a1)); with h, its virtual slot 0xC(h, self, out); without, *out gets an
// empty string (func_08A76AA0).
void func_08AA9B68(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t key = call_set(rt, ctx, 0x08AA8F38u, {}).v0;
    const std::uint32_t handler = call_set(rt, ctx, 0x08AA9AC8u, {self, key}).v0;
    ctx.gpr[4] = handler;
    ctx.gpr[5] = self;
    ctx.gpr[6] = out;
    if (handler != 0u) {
        ctx.gpr[2] = word(rt, handler);
        ctx.gpr[3] = word(rt, ctx.gpr[2] + 0xCu);
        call_set(rt, ctx, ctx.gpr[3], {handler, self, out});
        return;
    }
    call_set(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
    assign_and_destroy(rt, ctx, out, frame.at(0u));
}

// *a1 = the string func_08A97A6C makes from (self's word +0x14 differs from virtual slot 0x4C of the object *a3)
// (func_08A5BB7C reports 0x08CB00F8 when that object is null).
void func_08A60168(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[7]);
    const std::uint32_t out = ctx.gpr[5];
    const std::uint32_t mine = word(rt, ctx.gpr[4] + 0x14u);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[4] = 0x08CB00F8u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB00F8u});
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x4Cu);
    const std::uint32_t theirs = call_set(rt, ctx, ctx.gpr[2], {object}).v0;
    call_set(rt, ctx, 0x08A97A6Cu, {frame.at(0u), (mine ^ theirs) != 0u ? 1u : 0u});
    assign_and_destroy(rt, ctx, out, frame.at(0u));
}

// Locals at +4 (built by func_08A9411C(it, a1)), +0xC (func_08AA9960(it, &+4)) and +8 (a string from func_08A76AAC with
// self at +0); func_08AAB0B4(&+0xC, &+8), then the locals are torn down in reverse.
void func_08A773B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x80u);
    const std::uint32_t holder = frame.at(4u), text = frame.at(8u), list = frame.at(0xCu);
    call_set(rt, ctx, 0x08A9411Cu, {holder});
    call_set(rt, ctx, 0x08AA9960u, {list, holder});
    put(rt, frame.at(0u), self);
    call_set(rt, ctx, 0x08A76AACu, {text, frame.at(0u)});
    call_set(rt, ctx, 0x08AAB0B4u, {list, text});
    call_set(rt, ctx, 0x08A5B9ECu, {text});
    call_set(rt, ctx, 0x08AA96B4u, {list});
    call_set(rt, ctx, 0x08A5B9ECu, {holder});
}

// key = func_088A5EE0(a1) (v0, v1) and func_088A6164(a1); i = func_0881AD9C(self, a1, key.v0, key.v1, that) indexes
// the 8-byte entries at self + 0x354: with i >= 0, func_088A6C18(entry, a1) and 1, otherwise 0.
void func_0881AFA0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t item = ctx.gpr[5];
    const GuestResult key = call_set(rt, ctx, 0x088A5EE0u, {item});
    const std::uint32_t extra = call_set(rt, ctx, 0x088A6164u, {item}).v0;
    ctx.gpr[8] = extra;
    ctx.gpr[4] = self;
    ctx.gpr[6] = key.v0;
    ctx.gpr[7] = key.v1;
    const auto index = static_cast<std::int32_t>(call_set(rt, ctx, 0x0881AD9Cu, {}).v0);
    const std::uint32_t entry = (static_cast<std::uint32_t>(index) << 3u) + self + 0x354u;
    std::uint32_t result = 0u;
    if (index >= 0) {
        call_set(rt, ctx, 0x088A6C18u, {entry, item});
        result = 1u;
    } else {
        ctx.gpr[4] = entry;
        ctx.gpr[5] = item;
    }
    ctx.gpr[3] = result;
    ctx.gpr[2] = result;
}

// key = `lookup`(a1, &key); with no object *a2 yet, *a2 gets the string `make`(&local, key) (kAssign unless a2 is the
// local); with one, its virtual slot 0xF4(object, &string) gets `make`(&string, key). Two functions, two pairs.
void string_from_key(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t lookup, std::uint32_t make) {
    const std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, lookup, {ctx.gpr[5], frame.at(8u)});
    const std::uint32_t object = word(rt, out);
    if (object == 0u) {
        call_set(rt, ctx, make, {frame.at(4u), word(rt, frame.at(8u))});
        assign_and_destroy(rt, ctx, out, frame.at(4u));
        return;
    }
    const std::uint32_t method = word(rt, word(rt, object) + 0xF4u);
    ctx.gpr[2] = word(rt, object);
    call_set(rt, ctx, make, {frame.at(0u), word(rt, frame.at(8u))});
    call_set(rt, ctx, method, {object, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}
void func_08C325E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_from_key(rt, ctx, 0x08C31A58u, 0x08BE9040u); }
void func_08C32350(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { string_from_key(rt, ctx, 0x08C319D8u, 0x08BF3BD0u); }

// Writes `length` bytes from `source` at `offset` in the growable buffer (data +0x14, capacity +0x18): when
// offset + length passes the capacity, a new block of that size (func_08B2B5E0) gets the old bytes (func_08B79A88) and
// func_08A44598(buffer, block, length, 1) adopts it; then a tail jump to func_08B79A88(data + offset, source, length).
void func_08A44614(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t buffer = ctx.gpr[4];
    const std::uint32_t offset = ctx.gpr[5];
    const std::uint32_t source = ctx.gpr[6];
    const std::uint32_t length = ctx.gpr[7];
    const std::uint32_t needed = offset + length;
    const std::uint32_t capacity = word(rt, buffer + 0x18u);
    ctx.gpr[3] = capacity;
    ctx.gpr[4] = needed;
    ctx.gpr[2] = capacity < needed ? 1u : 0u;
    if (capacity < needed) {
        const std::uint32_t block = call_set(rt, ctx, 0x08B2B5E0u, {needed}).v0;
        call_set(rt, ctx, 0x08B79A88u, {block, word(rt, buffer + 0x14u), word(rt, buffer + 0x18u)});
        call_set(rt, ctx, 0x08A44598u, {buffer, block, length, 1u});
    }
    tail_call(ctx, 0x08B79A88u, {word(rt, buffer + 0x14u) + offset, source, length});
}

// v = func_08837498(self, 0); the object *a1 (func_08A5BB7C reports 0x08C9BA2C when null) fills the local at +4 through
// its virtual slot 0x9C(&+4, object, &+0) from an empty string at +0 (func_08A85B1C); func_08837DB0(v, &+4, 0).
void func_088382DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t value = call_set(rt, ctx, 0x08837498u, {ctx.gpr[4], 0u}).v0;
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[4] = 0x08C9BA2Cu;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9BA2Cu});
    ctx.gpr[2] = word(rt, object);
    const std::uint32_t method = word(rt, ctx.gpr[2] + 0x9Cu);
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), 0u});
    call_set(rt, ctx, method, {frame.at(4u), object, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call_set(rt, ctx, 0x08837DB0u, {value, frame.at(4u), 0u});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
}

// A string of n bytes read from the stream: n = func_08A71C1C(stream) (nothing when 0), the string *a1 sized by
// func_08B86674(a1, n, 0) and made unique (func_08B88FF8) unless its count (word before the body) is negative, then a
// tail jump to the stream's virtual slot 0x110(stream, body, n).
void func_08AA1F40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t stream = ctx.gpr[4];
    const std::uint32_t text = ctx.gpr[5];
    const std::uint32_t n = call_set(rt, ctx, 0x08A71C1Cu, {stream}).v0;
    ctx.gpr[4] = text;
    ctx.gpr[5] = n;
    ctx.gpr[6] = 0u;
    if (n == 0u) return;
    call_set(rt, ctx, 0x08B86674u, {text, n, 0u});
    std::uint32_t body = word(rt, text);
    ctx.gpr[3] = word(rt, stream);
    const std::uint32_t method = word(rt, ctx.gpr[3] + 0x110u);
    ctx.gpr[2] = word(rt, body - 4u);
    ctx.gpr[4] = text;
    if (static_cast<std::int32_t>(ctx.gpr[2]) >= 0) {
        call_set(rt, ctx, 0x08B88FF8u, {text});
        body = word(rt, text);
    }
    ctx.gpr[25] = method;
    tail_call(ctx, method, {stream, body, n});
}

// The object at +0x14 of a1 (func_08A5BB7C reports 0x08CAF798 when null) runs its virtual slot 0x148(self, it, a1,
// &local) on a local the object *a2 fills through its virtual slot 0x5C(&local, it) (0x08CAF778 reported when null);
// the local is destroyed (func_08A69D68) and self returned.
void func_08A56F80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    const std::uint32_t source_holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t target = word(rt, owner + 0x14u);
    ctx.gpr[4] = 0x08CAF798u;
    if (target == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
    const std::uint32_t method = word(rt, word(rt, target) + 0x148u);
    const std::uint32_t source = word(rt, source_holder);
    ctx.gpr[4] = 0x08CAF778u;
    if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF778u});
    ctx.gpr[3] = word(rt, source);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x5Cu);
    call_set(rt, ctx, ctx.gpr[2], {frame.at(0u), source});
    call_set(rt, ctx, method, {self, target, owner, frame.at(0u)});
    call_set(rt, ctx, 0x08A69D68u, {frame.at(0u)});
    ctx.gpr[2] = self;
}

// Two calls through C++ member-function pointers (function or vtable offset, and an adjustment whose low bit marks a
// virtual one): the first (+0x14, +0x18) on the object *a2, the second (+0x1C, +0x20) on what the first returns, with a
// string local built from t0 (func_08A76ABC).
void func_08C2626C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object_holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    call_set(rt, ctx, 0x08A76ABCu, {frame.at(0u), ctx.gpr[8]});
    const std::uint32_t adjust = word(rt, self + 0x18u);
    const std::uint32_t pointer = word(rt, self + 0x14u);
    const std::uint32_t object = word(rt, object_holder) + static_cast<std::uint32_t>(static_cast<std::int32_t>(adjust) >> 1);
    ctx.gpr[3] = adjust & 1u;
    std::uint32_t function = pointer;
    if ((adjust & 1u) != 0u) {
        ctx.gpr[2] = word(rt, object) + pointer;
        function = word(rt, ctx.gpr[2]);
    }
    const std::uint32_t result = call_set(rt, ctx, function, {object, adjust, pointer, function}).v0;
    const std::uint32_t adjust2 = word(rt, self + 0x20u);
    const std::uint32_t object2 = result + static_cast<std::uint32_t>(static_cast<std::int32_t>(adjust2) >> 1);
    std::uint32_t function2 = word(rt, self + 0x1Cu);
    ctx.gpr[3] = static_cast<std::uint32_t>(static_cast<std::int32_t>(adjust2) >> 1);
    if ((adjust2 & 1u) != 0u) {
        ctx.gpr[3] = function2;
        function2 = word(rt, word(rt, object2) + function2);
    }
    ctx.gpr[2] = function2;
    call_set(rt, ctx, function2, {object2, frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

// func_08C32AD4(a1, a2), except when the object *a2 exists and its virtual slot 0xEC returns the word at 0x08CDF7C4:
// then func_08C32AD4(a1, &empty string), the object's virtual slot 0xF4(object, &string) (0x08CB30B8 reported when the
// object is gone by then), and the string destroyed.
void func_08C32FCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t first = ctx.gpr[5];
    const std::uint32_t holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[4] = first;
    ctx.gpr[5] = holder;
    const std::uint32_t object = word(rt, holder);
    ctx.gpr[2] = object;
    if (object != 0u) {
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xECu);
        const std::uint32_t kind = call_set(rt, ctx, ctx.gpr[2], {object}).v0;
        ctx.gpr[6] = word(rt, 0x08CDF7C4u);
        ctx.gpr[4] = frame.at(0u);
        ctx.gpr[5] = holder;
        if (kind == ctx.gpr[6]) {
            call_set(rt, ctx, 0x08A76AA0u, {frame.at(0u)});
            call_set(rt, ctx, 0x08C32AD4u, {first, frame.at(0u)});
            const std::uint32_t current = word(rt, holder);
            ctx.gpr[4] = 0x08CB30B8u;
            if (current == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
            ctx.gpr[3] = word(rt, current);
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xF4u);
            call_set(rt, ctx, ctx.gpr[2], {current, frame.at(0u)});
            call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
            return;
        }
        ctx.gpr[4] = first;
    }
    call_set(rt, ctx, 0x08C32AD4u, {first, holder});
}

// XORs `length` bytes at `data` in place with two generators: per 32-bit word (the word read and written byte-reversed,
// wsbw), word = x ^ (y + word), x = func_08B1EEF0(a2), y = func_08B3A37C(a3); then per remaining byte the same with
// bytes.
void func_08C4FE98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t data = ctx.gpr[4];
    std::uint32_t length = ctx.gpr[5];
    const std::uint32_t first = ctx.gpr[6];
    const std::uint32_t second = ctx.gpr[7];
    const auto swap = [](std::uint32_t v) { return __builtin_bswap32(v); };
    if (length >= 4u) {
        do {
            const std::uint32_t x = call_set(rt, ctx, 0x08B1EEF0u, {first}).v0;
            const std::uint32_t y = call_set(rt, ctx, 0x08B3A37Cu, {second}).v0;
            const std::uint32_t value = swap(word(rt, data));
            put(rt, data, swap(x ^ (y + value)));
            ctx.gpr[3] = value;
            ctx.gpr[2] = y + value;
            length -= 4u;
            data += 4u;
        } while (length >= 4u);
        ctx.gpr[4] = 0u;
    }
    if (length == 0u) return;
    const std::uint32_t end = data + length;
    do {
        const std::uint32_t x = call_set(rt, ctx, 0x08B1EEF0u, {first}).v0;
        const std::uint32_t y = call_set(rt, ctx, 0x08B3A37Cu, {second}).v0;
        const std::uint32_t value = rt.memory().load8(data);
        ctx.gpr[3] = value;
        ctx.gpr[2] = (y + value) ^ x;
        rt.memory().store8(data, static_cast<std::uint8_t>(ctx.gpr[2]));
        ++data;
    } while (data != end);
}

// n = virtual slot 0xD8 of the object *a1, p = its virtual slot 0xDC(object, 0) (func_08A5BB7C reports 0x08CB211C when
// the object is null, each time); func_08A948A8(self, n), then with both p and n a tail jump to
// func_08B79A88(*(self + 0x14), p, n).
void func_08A94BE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    const auto object_of = [&]() {
        const std::uint32_t object = word(rt, holder);
        ctx.gpr[4] = 0x08CB211Cu;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB211Cu});
        ctx.gpr[3] = word(rt, object);
        return object;
    };
    std::uint32_t object = object_of();
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xD8u);
    const std::uint32_t n = call_set(rt, ctx, ctx.gpr[2], {object}).v0;
    object = object_of();
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xDCu);
    const std::uint32_t p = call_set(rt, ctx, ctx.gpr[2], {object, 0u}).v0;
    call_set(rt, ctx, 0x08A948A8u, {self, n});
    ctx.gpr[5] = p;
    ctx.gpr[6] = n;
    if (p == 0u || n == 0u) return;
    tail_call(ctx, 0x08B79A88u, {word(rt, self + 0x14u), p, n});
}

// Below version 0xE (word +0x44): func_08C313B4(object at +4, a1). From 0xE on: a string of func_08A71C1C(object) bytes
// made by func_08AA95E8(&local, self, n), handed to func_08B855E0(a1, &local) and released. The object at +4 reports
// 0x08CB30B0 through func_08A5BB7C when null.
void func_08AA99DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t version = word(rt, self + 0x44u);
    const std::uint32_t object = word(rt, self + 4u);
    ctx.gpr[4] = 0x08CB30B0u;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
    if (version < 0xEu) {
        call_set(rt, ctx, 0x08C313B4u, {object, out});
        return;
    }
    const std::uint32_t n = call_set(rt, ctx, 0x08A71C1Cu, {object}).v0;
    call_set(rt, ctx, 0x08AA95E8u, {frame.at(4u), self, n});
    call_set(rt, ctx, 0x08B855E0u, {out, frame.at(4u)});
    ctx.gpr[3] = word(rt, frame.at(4u));
    ctx.gpr[2] = 0x08D74FD0u;
    release_string(rt, ctx, frame.at(4u), frame.at(0u));
}

// A key string (empty, then func_08C32AAC(a1, &key)); with no object *a2, *a2 gets func_08A6D2D4(&local, &key) (kAssign
// unless a2 is the local); with one, its virtual slot 0xF4(object, &string) gets func_08A6D2D4(&string, &key). The key
// is released.
void func_08C33268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    const std::uint32_t key = frame.at(0xCu);
    put(rt, key, 0x08D74FDCu);
    call_set(rt, ctx, 0x08C32AACu, {ctx.gpr[5], key});
    const std::uint32_t object = word(rt, out);
    if (object == 0u) {
        call_set(rt, ctx, 0x08A6D2D4u, {frame.at(8u), key});
        assign_and_destroy(rt, ctx, out, frame.at(8u));
    } else {
        const std::uint32_t method = word(rt, word(rt, object) + 0xF4u);
        call_set(rt, ctx, 0x08A6D2D4u, {frame.at(4u), key});
        call_set(rt, ctx, method, {object, frame.at(4u)});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    }
    ctx.gpr[2] = word(rt, key);
    release_string(rt, ctx, key, frame.at(0u));
}

// A script native over a 16-bit field of the current entry (result a0, argc a2, argv a3). The context comes from
// func_088547C4(&local) (its object, func_08A5BB7C reporting 0x08C9D3BC when null, passed to func_08854678 and the local
// released). With arguments it sets the field of func_088A6608(context) to virtual slot 0x3C of the first argument
// (0x08C9D394 reported when null); without, it returns the field of func_088A65E8(context) as a value
// (func_08A85B1C(&local, field), kAssign to the result unless that is the local, the local released).
void half_field_native(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t offset, bool is_signed) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    const auto context_of = [&](std::uint32_t local) {
        call_set(rt, ctx, 0x088547C4u, {local});
        const std::uint32_t object = word(rt, local);
        ctx.gpr[4] = 0x08C9D3BCu;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9D3BCu});
        const std::uint32_t context = call_set(rt, ctx, 0x08854678u, {object}).v0;
        call_set(rt, ctx, 0x08A5B9ECu, {local});
        return context;
    };
    if (argc > 0) {
        const std::uint32_t context = context_of(frame.at(8u));
        const std::uint32_t entry = call_set(rt, ctx, 0x088A6608u, {context}).v0;
        const std::uint32_t argument = word(rt, argv);
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9D394u});
        ctx.gpr[3] = word(rt, argument);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
        const std::uint32_t value = call_set(rt, ctx, ctx.gpr[2], {argument}).v0;
        rt.memory().store16(entry + offset, static_cast<std::uint16_t>(value));
        return;
    }
    const std::uint32_t context = context_of(frame.at(4u));
    const std::uint32_t entry = call_set(rt, ctx, 0x088A65E8u, {context}).v0;
    const std::uint16_t raw = rt.memory().load16(entry + offset);
    const std::uint32_t field = is_signed ? static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(raw))) : raw;
    call_set(rt, ctx, 0x08A85B1Cu, {frame.at(0u), field});
    assign_and_destroy(rt, ctx, result, frame.at(0u));
}
void func_08851760(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x15Au, true); }
void func_08851864(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x158u, false); }

// *result (built empty by func_08A76AA0) = func_08A57B54 over the entry the map at a1 + 0x38 finds for a3
// (func_08C1D1AC; the end is a1 + 0x3C, then nothing more): the entry's object +0x14 (func_08A5BB7C reports
// 0x08CAF594 when null), its word +0x10 plus a base (a2's word +0x1C, or else the word +0xC of the object at a2 + 0x18,
// 0x08CAF540 reported when null), and a2 + 0x18. Returns result.
void func_08A54A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t owner = ctx.gpr[5];
    const std::uint32_t source = ctx.gpr[6];
    const std::uint32_t key = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    call_set(rt, ctx, 0x08A76AA0u, {result});
    const std::uint32_t found = call_set(rt, ctx, 0x08C1D1ACu, {owner + 0x38u, key}).v0;
    if (found == owner + 0x3Cu) {
        ctx.gpr[2] = result;
        return;
    }
    const auto object_at = [&](std::uint32_t slot) {
        const std::uint32_t object = word(rt, slot);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF594u});
        return object;
    };
    const std::uint32_t offset = word(rt, object_at(found + 0x14u) + 0x10u);
    std::uint32_t base = word(rt, source + 0x1Cu);
    ctx.gpr[2] = base;
    if (base == 0u) {
        const std::uint32_t holder = word(rt, source + 0x18u);
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF540u});
        base = word(rt, holder + 0xCu);
        ctx.gpr[2] = base;
    }
    const std::uint32_t object = object_at(found + 0x14u);
    call_set(rt, ctx, 0x08A57B54u, {frame.at(0u), object + 0xCu, source + 0x18u, base + offset});
    assign_and_destroy(rt, ctx, result, frame.at(0u));
    ctx.gpr[2] = result;
}

// Seconds (a0:a1, 64-bit) to the date and time: t = seconds + 12 h; v0 = func_08B1B420(t / 86400) and
// v1 = func_08B1B6A8(t % 86400) (the 64-bit division routines func_08B6F518 and func_08B6FBA4). The original packs them
// through stack bytes field by field; every bit lands where it was.
void func_08B1B740(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = ctx.gpr[4] + 0xA8C0u;
    const std::uint32_t high = ctx.gpr[5] + (low < ctx.gpr[4] ? 1u : 0u);
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t days = call_set(rt, ctx, 0x08B6F518u, {low, high, 0x15180u, 0u}).v0;
    const std::uint32_t date = call_set(rt, ctx, 0x08B1B420u, {days}).v0;
    const std::uint32_t seconds = call_set(rt, ctx, 0x08B6FBA4u, {low, high, 0x15180u, 0u}).v0;
    const std::uint32_t time = call_set(rt, ctx, 0x08B1B6A8u, {seconds}).v0;
    ctx.gpr[2] = date;
    ctx.gpr[3] = time;
}

// The object a source local holds after `fill`(&local) (func_08A5BB7C reports `report` when null), passed to `use`; the
// local is released. Shared by the script natives below.
std::uint32_t use_filled(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t local, std::uint32_t fill,
                         std::initializer_list<std::uint32_t> fill_arguments, std::uint32_t report, std::uint32_t use) {
    call_set(rt, ctx, fill, fill_arguments);
    const std::uint32_t object = word(rt, local);
    ctx.gpr[4] = report;
    if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {report});
    const std::uint32_t value = call_set(rt, ctx, use, {object}).v0;
    call_set(rt, ctx, 0x08A5B9ECu, {local});
    return value;
}

// Script native (result a0, argc a2, argv a3): with arguments, the result is the bool func_0881AFA0(owner, entry) made a
// value by func_08A97A6C; owner from func_0881BD98 / func_0881B068 (0x08C98F48 reported when null), entry from
// func_088547C4(argv) / func_08854678 (0x08C98F58).
void func_0881EE14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[4] = frame.at(8u);
    if (argc <= 0) return;
    const std::uint32_t owner = use_filled(rt, ctx, frame.at(8u), 0x0881BD98u, {frame.at(8u)}, 0x08C98F48u, 0x0881B068u);
    const std::uint32_t entry = use_filled(rt, ctx, frame.at(4u), 0x088547C4u, {frame.at(4u), argv}, 0x08C98F58u, 0x08854678u);
    const std::uint32_t found = call_set(rt, ctx, 0x0881AFA0u, {owner, entry}).v0;
    call_set(rt, ctx, 0x08A97A6Cu, {frame.at(0u), found});
    assign_and_destroy(rt, ctx, result, frame.at(0u));
}

// Script native (result a0, argc a2, argv a3): with arguments, func_0881BF5C(owner, argc, argv, &key, &extra) parses a
// key; when both its words are -1 the result is false (func_08A97A6C(&local, 0)), otherwise the entry
// func_0881AEC8(owner) finds for that key (a2, a3, t0 as the parser left them) made a value by func_08857EE4(&local,
// entry, 0). Owner as in func_0881EE14.
void func_0881E7D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    if (argc <= 0) return;
    const std::uint32_t owner = use_filled(rt, ctx, frame.at(8u), 0x0881BD98u, {frame.at(8u)}, 0x08C98F48u, 0x0881B068u);
    call_set(rt, ctx, 0x0881BF5Cu, {owner, static_cast<std::uint32_t>(argc), argv, frame.at(0x10u), frame.at(0xCu)});
    ctx.gpr[6] = word(rt, frame.at(0x10u));
    ctx.gpr[7] = word(rt, frame.at(0x14u));
    ctx.gpr[8] = word(rt, frame.at(0xCu));
    ctx.gpr[2] = 0xFFFFFFFFu;
    if (ctx.gpr[6] == 0xFFFFFFFFu && ctx.gpr[7] == 0xFFFFFFFFu) {
        call_set(rt, ctx, 0x08A97A6Cu, {frame.at(4u), 0u});
        assign_and_destroy(rt, ctx, result, frame.at(4u));
        return;
    }
    const std::uint32_t entry = call_set(rt, ctx, 0x0881AEC8u, {owner}).v0;
    call_set(rt, ctx, 0x08857EE4u, {frame.at(0u), entry, 0u});
    assign_and_destroy(rt, ctx, result, frame.at(0u));
}

// Moves a selection (self +0x1C must be 1; nothing and 1 otherwise) from `from` (a1) to `to` (a2), on the list at +0x14
// and the view at +4. to < 0: a tail jump to func_088DA964(view, from, to). to == from: a tail jump to
// func_08891E08(list, 0). Otherwise func_08891E08(list, 1), func_08892678(list), and the row
// func_08891488(func_08891644(list)): a tail jump to func_088DA8DC(view, row) when it is >= 0; when not, and
// func_08891624(list) and the item func_088DA788(view, from) has a positive +0x14 and func_088915A8(list), a tail jump
// to func_088DA8DC(view, to).
void func_0888BAF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t from = ctx.gpr[5];
    const auto to = static_cast<std::int32_t>(ctx.gpr[6]);
    ctx.gpr[2] = 1u;
    ctx.gpr[3] = word(rt, self + 0x1Cu);
    if (ctx.gpr[3] != 1u) return;
    if (to < 0) {
        tail_call(ctx, 0x088DA964u, {word(rt, self + 4u)});
        return;
    }
    const auto list = [&] { return word(rt, self + 0x14u); };
    ctx.gpr[4] = list();
    if (static_cast<std::uint32_t>(to) == from) {
        tail_call(ctx, 0x08891E08u, {list(), 0u});
        return;
    }
    call_set(rt, ctx, 0x08891E08u, {list(), 1u});
    call_set(rt, ctx, 0x08892678u, {list()});
    const std::uint32_t cursor = call_set(rt, ctx, 0x08891644u, {list()}).v0;
    const auto row = static_cast<std::int32_t>(call_set(rt, ctx, 0x08891488u, {cursor}).v0);
    ctx.gpr[5] = static_cast<std::uint32_t>(row);
    if (row >= 0) {
        tail_call(ctx, 0x088DA8DCu, {word(rt, self + 4u), static_cast<std::uint32_t>(row)});
        return;
    }
    if (call_set(rt, ctx, 0x08891624u, {list()}).v0 == 0u) return;
    const std::uint32_t item = call_set(rt, ctx, 0x088DA788u, {word(rt, self + 4u), from}).v0;
    ctx.gpr[3] = word(rt, item + 0x14u);
    if (static_cast<std::int32_t>(ctx.gpr[3]) <= 0) return;
    if (call_set(rt, ctx, 0x088915A8u, {list()}).v0 == 0u) return;
    tail_call(ctx, 0x088DA8DCu, {word(rt, self + 4u), static_cast<std::uint32_t>(to)});
}

// Copies `length` bytes at `offset` of the data (the word +0x1C, or else the word +0xC of the object at +0x18,
// func_08A5BB7C reporting 0x08CAF7A0 when null) to `destination`, when offset is below the size virtual slot 0x120 of
// the object at +0x14 gives (0x08CAF798 reported when null; asked again each time); the length is cut to the size: a
// tail jump to func_08B79A88(data + offset, destination, length).
void func_08A58794(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t offset = ctx.gpr[5];
    const std::uint32_t destination = ctx.gpr[6];
    std::uint32_t length = ctx.gpr[7];
    const auto size = [&] {
        const std::uint32_t object = word(rt, self + 0x14u);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x120u);
        return call_set(rt, ctx, ctx.gpr[2], {object}).v0;
    };
    ctx.gpr[2] = offset < size() ? 1u : 0u;
    if (ctx.gpr[2] == 0u) return;
    if (size() < offset + length) length = size() - offset;
    std::uint32_t data = word(rt, self + 0x1Cu);
    if (data == 0u) {
        const std::uint32_t holder = word(rt, self + 0x18u);
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAF7A0u});
        data = word(rt, holder + 0xCu);
    }
    ctx.gpr[2] = data;
    tail_call(ctx, 0x08B79A88u, {data + offset, destination, length});
}

// Formats 64-bit seconds (a0:a1) into the buffer (a2, a3 bytes): 0 copies the text at 0x08C906BC (func_08B7C830);
// otherwise the date and time func_08B1B740 gives (year in the date's low half, month and day in its two high bytes;
// hour and minute in the time's two low bytes) go to func_08B3767C (an snprintf) with the format 0x08CB468C (year,
// month, day, hour, minute) or, when func_08B49688(the word at 0x08D69524) says so, 0x08CB46A8 (day, month, year,
// hour, minute).
void func_08ABE778(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t low = ctx.gpr[4], high = ctx.gpr[5];
    const std::uint32_t buffer = ctx.gpr[6];
    const std::uint32_t size = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[2] = low | high;
    ctx.gpr[6] = size;
    if ((low | high) == 0u) {
        tail_call(ctx, 0x08B7C830u, {buffer, 0x08C906BCu});
        return;
    }
    const GuestResult when = call_set(rt, ctx, 0x08B1B740u, {low, high});
    const std::uint32_t date = when.v0, time = when.v1;
    const auto year = static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int16_t>(date & 0xFFFFu)));
    const std::uint32_t month = (date >> 16u) & 0xFFu, day = date >> 24u;
    const std::uint32_t hour = time & 0xFFu, minute = (time >> 8u) & 0xFFu;
    put(rt, frame.at(0u), date);
    put(rt, frame.at(4u), time);
    put(rt, frame.at(0x10u), date);
    put(rt, frame.at(0x14u), time);
    ctx.gpr[5] = 0x00090000u;
    ctx.gpr[6] = date;
    ctx.gpr[7] = time;
    const std::uint32_t day_first = call_set(rt, ctx, 0x08B49688u, {word(rt, 0x08D69524u)}).v0;
    if (day_first != 0u)
        tail_call(ctx, 0x08B3767Cu, {buffer, size, 0x08CB46A8u, day, month, year, hour, minute});
    else
        tail_call(ctx, 0x08B3767Cu, {buffer, size, 0x08CB468Cu, year, month, day, hour, minute});
}

// Script native (result a0, argc a1, argv a2): the first argument as a date string. Its value is 64-bit (virtual slot
// 0x40) when func_08A76F20(argument, func_08A82A5C()) says so, else 32-bit sign-extended (slot 0x3C); func_08ABE778
// formats it into a 128-byte buffer, which becomes a string (func_08B85EB8) and a value (func_08A6D2D4) assigned to the
// result. The argument reports 0x08CAA480 through func_08A5BB7C when null.
void func_08A1FA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    GuestFrame frame(ctx, 0xA0u);
    if (argc <= 0) return;
    const auto argument = [&] {
        const std::uint32_t object = word(rt, argv);
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAA480u});
        return object;
    };
    const std::uint32_t first = argument();
    const std::uint32_t type = call_set(rt, ctx, 0x08A82A5Cu, {}).v0;
    const std::uint32_t is_long = call_set(rt, ctx, 0x08A76F20u, {first, type}).v0;
    const std::uint32_t object = argument();
    ctx.gpr[3] = word(rt, object);
    std::uint32_t low = 0u, high = 0u;
    if (is_long != 0u) {
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x40u);
        const GuestResult value = call_set(rt, ctx, ctx.gpr[2], {object});
        low = value.v0;
        high = value.v1;
    } else {
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
        low = call_set(rt, ctx, ctx.gpr[2], {object}).v0;
        high = static_cast<std::uint32_t>(static_cast<std::int32_t>(low) >> 31);
    }
    call_set(rt, ctx, 0x08ABE778u, {low, high, frame.at(0xCu), 0x80u});
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(4u), frame.at(0xCu), frame.at(1u)});
    call_set(rt, ctx, 0x08A6D2D4u, {frame.at(8u), frame.at(4u)});
    assign_and_destroy(rt, ctx, result, frame.at(8u));
    release_string(rt, ctx, frame.at(4u), frame.at(0u));
}

// x = (float +8) * 6: func_08B53844 gets its fraction (x minus its truncation) in f12, and the truncation is returned.
void func_0889A57C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const float six = std::bit_cast<float>(0x40C00000u);
    const float scaled = psp_mul(float_at(rt, ctx.gpr[4] + 8u), six);
    const std::uint32_t whole = ctx.fpu_float_to_word(scaled, 1u);
    const float fraction = scaled - static_cast<float>(static_cast<std::int32_t>(whole));
    ctx.fpr[12] = fraction;
    call_set(rt, ctx, 0x08B53844u, {}, {fraction});
    ctx.gpr[2] = whole;
}

// out = virtual slot 0x9C(out, object, &key) of `object`, the key a new 0x18-byte node (func_08A5BC20(0x18, 4),
// func_08A76ACC, vtable 0x08CB6B88, a copy of the name at +0x14 via func_08A6972C) held by func_08A76AB4(&key, &node);
// the name is a temporary func_08A69B3C(&tmp, text) destroyed with func_08A69D68. tmp, holder and key are stack slots.
void attribute_by_name(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t object, std::uint32_t text,
                       std::uint32_t out, std::uint32_t tmp, std::uint32_t holder, std::uint32_t key) {
    const std::uint32_t method = word(rt, word(rt, object) + 0x9Cu);
    call_set(rt, ctx, 0x08A69B3Cu, {tmp, text});
    const std::uint32_t node = call_set(rt, ctx, 0x08A5BC20u, {0x18u, 4u}).v0;
    call_set(rt, ctx, 0x08A76ACCu, {node});
    put(rt, node, 0x08CB6B88u);
    call_set(rt, ctx, 0x08A6972Cu, {node + 0x14u, tmp});
    put(rt, holder, node);
    call_set(rt, ctx, 0x08A76AB4u, {key, holder});
    call_set(rt, ctx, 0x08A69D68u, {tmp});
    call_set(rt, ctx, method, {out, object, key});
    call_set(rt, ctx, 0x08A5B9ECu, {key});
}

// Reads the attribute named at 0x08C9E55C of the object *a1; when its virtual slot 0x3C is 0x65 (func_088A57F4), the
// attribute named at 0x08C9E564 goes, through its virtual slot 0xDC(it, 0), to func_088A6DE4(func_08854678(self), ...).
// The object and the attributes report 0x08C9E14C through func_08A5BB7C when null.
void func_088584A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t holder = ctx.gpr[5];
    GuestFrame frame(ctx, 0x50u);
    const auto checked = [&](std::uint32_t object) {
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9E14Cu});
        return object;
    };
    const std::uint32_t first = frame.at(0x18u), second = frame.at(0x1Cu);
    attribute_by_name(rt, ctx, checked(word(rt, holder)), 0x08C9E55Cu, first, frame.at(8u), frame.at(0xCu), frame.at(0x14u));
    const std::uint32_t value = checked(word(rt, first));
    ctx.gpr[3] = word(rt, value);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const std::uint32_t kind = call_set(rt, ctx, ctx.gpr[2], {value}).v0;
    const std::uint32_t wanted = call_set(rt, ctx, 0x088A57F4u, {}).v0;
    if (kind == wanted) {
        attribute_by_name(rt, ctx, checked(word(rt, holder)), 0x08C9E564u, second, frame.at(0u), frame.at(4u), frame.at(0x10u));
        const std::uint32_t context = call_set(rt, ctx, 0x08854678u, {self}).v0;
        const std::uint32_t other = checked(word(rt, second));
        ctx.gpr[3] = word(rt, other);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xDCu);
        const std::uint32_t data = call_set(rt, ctx, ctx.gpr[2], {other, 0u}).v0;
        call_set(rt, ctx, 0x088A6DE4u, {context, data});
        call_set(rt, ctx, 0x08A5B9ECu, {second});
    }
    call_set(rt, ctx, 0x08A5B9ECu, {first});
}

// Reads n = func_08C319D8(stream, &n) key/value string pairs (func_08C32AD4(stream, &string) each) into the container
// *a2. Without one, *a2 gets a new one (func_08A813F4, kAssign unless a2 is the local) and every pair is added with its
// virtual slot 0xA0(container, &key, &value). With one, its virtual slot 0x90 decides: zero skips the pairs (read and
// dropped); otherwise each key is looked up with slot 0xA4(&found, container, &key) into the value, which is read in
// place when it was there and added with slot 0xA0 when not. The container reports 0x08CB30B8 through func_08A5BB7C
// when null, each time it is fetched.
void func_08C32D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t stream = ctx.gpr[5];
    const std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t count_slot = frame.at(8u);
    call_set(rt, ctx, 0x08C319D8u, {stream, count_slot});
    const auto count = [&] { return word(rt, count_slot); };
    const auto container = [&] {
        const std::uint32_t object = word(rt, out);
        ctx.gpr[4] = 0x08CB30B8u;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B8u});
        ctx.gpr[3] = word(rt, object);
        return object;
    };
    const auto read = [&](std::uint32_t text) { call_set(rt, ctx, 0x08C32AD4u, {stream, text}); };
    const auto empty = [&](std::uint32_t text) { call_set(rt, ctx, 0x08A76AA0u, {text}); };
    const auto destroy = [&](std::uint32_t text) { call_set(rt, ctx, 0x08A5B9ECu, {text}); };
    const std::uint32_t existing = word(rt, out);
    if (existing == 0u) {
        call_set(rt, ctx, 0x08A813F4u, {frame.at(4u)});
        assign_and_destroy(rt, ctx, out, frame.at(4u));
        ctx.gpr[2] = count();
        if (ctx.gpr[2] == 0u) return;
        const std::uint32_t key = frame.at(0xCu), value = frame.at(0x10u);
        for (std::uint32_t i = 0; i < count(); ++i) {
            empty(key);
            empty(value);
            read(key);
            read(value);
            const std::uint32_t object = container();
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xA0u);
            call_set(rt, ctx, ctx.gpr[2], {object, key, value});
            destroy(value);
            destroy(key);
        }
        return;
    }
    ctx.gpr[3] = word(rt, existing);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x90u);
    const std::uint32_t merge = call_set(rt, ctx, ctx.gpr[2], {existing}).v0;
    ctx.gpr[2] = count();
    if (ctx.gpr[2] == 0u) return;
    if (merge == 0u) {
        const std::uint32_t key = frame.at(0xCu), value = frame.at(0x10u);
        for (std::uint32_t i = 0; i < count(); ++i) {
            empty(key);
            empty(value);
            read(key);
            read(value);
            destroy(value);
            destroy(key);
        }
        return;
    }
    const std::uint32_t found = frame.at(0u), value = frame.at(0xCu), key = frame.at(0x10u);
    for (std::uint32_t i = 0; i < count(); ++i) {
        empty(key);
        empty(value);
        read(key);
        std::uint32_t object = container();
        ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xA4u);
        call_set(rt, ctx, ctx.gpr[2], {found, object, key});
        call_set(rt, ctx, 0x08A5BAD8u, {value, word(rt, found)});
        destroy(found);
        if (word(rt, value) != 0u) {
            read(value);
        } else {
            read(value);
            object = container();
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xA0u);
            call_set(rt, ctx, ctx.gpr[2], {object, key, value});
        }
        destroy(value);
        destroy(key);
    }
}

// Batch 158: more members of the 0x08AA91F8 family (tools.family_rows), verified by synthetic cases.
void func_08AA8F88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C313ACu); }
void func_08AA9028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C3139Cu); }
void func_08AA90B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C3138Cu); }
void func_08AA9158(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C3137Cu); }
void func_08AA9318(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C31334u); }
void func_08AA93A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C31324u); }
void func_08AA9438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C24C48u); }
void func_08AA94C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C3130Cu); }
void func_08AA9558(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { forward_checked(rt, ctx, 0x08C312FCu); }
// Batch 160: more members of the 0x08851864 family (tools.family_rows), verified by synthetic cases.
void func_08851968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x134u, false); }
void func_08851A6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x136u, false); }
void func_08852EBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x108u, false); }
void func_08852FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x10Au, false); }
void func_088530C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x10Cu, false); }
void func_088531C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x10Eu, false); }
void func_088532CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x110u, false); }
void func_088533D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x112u, false); }
void func_088534D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x114u, false); }
void func_088535D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x116u, false); }
void func_088536DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x118u, false); }
void func_088537E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x11Au, false); }
void func_088538E4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x11Cu, false); }
void func_088539E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x11Eu, false); }
void func_08853AEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { half_field_native(rt, ctx, 0x120u, false); }
} // namespace hot_batch_135
} // namespace gt::game

#define HOT135(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_135::name, gt::ReturnKind::kind)
HOT135(0x088A57F4u, func_088A57F4, Word);
HOT135(0x08B2A3B0u, func_08B2A3B0, Word);
HOT135(0x08954AE0u, func_08954AE0, Float);
HOT135(0x08AA19C8u, func_08AA19C8, Void);
HOT135(0x0889605Cu, func_0889605C, Word);
HOT135(0x088A65E8u, func_088A65E8, Word);
HOT135(0x088963C4u, func_088963C4, Word);
HOT135(0x08C24C50u, func_08C24C50, Word);
HOT135(0x08C31354u, func_08C31354, Word);
HOT135(0x0881AEC8u, func_0881AEC8, Word);
HOT135(0x08899FA8u, func_08899FA8, Void);
HOT135(0x08A71C1Cu, func_08A71C1C, Word);
HOT135(0x08954AECu, func_08954AEC, Void);
HOT135(0x089448D4u, func_089448D4, Float);
HOT135(0x08AA1B84u, read_big_endian_word, Word);
HOT135(0x08AA1C2Cu, read_big_endian_word, Word);
HOT135(0x08AA9288u, func_08AA9288, Void);
HOT135(0x08AA91F8u, func_08AA91F8, Void);
HOT135(0x08C33360u, func_08C33360, Word);
HOT135(0x08C32874u, func_08C32874, Void);
HOT135(0x08B86D30u, func_08B86D30, Word);
HOT135(0x08B2BC5Cu, func_08B2BC5C, Word);
HOT135(0x08A44CCCu, func_08A44CCC, Word);
HOT135(0x08A93D24u, func_08A93D24, Word);
HOT135(0x08A9411Cu, func_08A9411C, Word);
HOT135(0x08817EBCu, func_08817EBC, Word);
HOT135(0x08A598FCu, func_08A598FC, Void);
HOT135(0x08891E08u, func_08891E08, Word);
HOT135(0x08A58BB8u, func_08A58BB8, Word);
HOT135(0x08B2A7ECu, func_08B2A7EC, Word);
HOT135(0x08AA9B68u, func_08AA9B68, Word);
HOT135(0x08A60168u, func_08A60168, Word);
HOT135(0x08A773B8u, func_08A773B8, Word);
HOT135(0x0881AFA0u, func_0881AFA0, Word);
HOT135(0x08C325E0u, func_08C325E0, Word);
HOT135(0x08C32350u, func_08C32350, Word);
HOT135(0x08A44614u, func_08A44614, Word);
HOT135(0x088382DCu, func_088382DC, Word);
HOT135(0x08AA1F40u, func_08AA1F40, Word);
HOT135(0x08A56F80u, func_08A56F80, Word);
HOT135(0x08C2626Cu, func_08C2626C, Word);
HOT135(0x08C32FCCu, func_08C32FCC, Word);
HOT135(0x08C4FE98u, func_08C4FE98, Void);
HOT135(0x08A94BE8u, func_08A94BE8, Word);
HOT135(0x08AA99DCu, func_08AA99DC, Word);
HOT135(0x08C33268u, func_08C33268, Word);
HOT135(0x08851760u, func_08851760, Void);
HOT135(0x08851864u, func_08851864, Void);
HOT135(0x08A54A58u, func_08A54A58, Word);
HOT135(0x08B1B740u, func_08B1B740, DoubleWord);
HOT135(0x0881EE14u, func_0881EE14, Void);
HOT135(0x0881E7D0u, func_0881E7D0, Void);
HOT135(0x0888BAF0u, func_0888BAF0, Word);
HOT135(0x08A58794u, func_08A58794, Word);
HOT135(0x08ABE778u, func_08ABE778, Word);
HOT135(0x08A1FA54u, func_08A1FA54, Void);
HOT135(0x0889A57Cu, func_0889A57C, Word);
HOT135(0x088584A0u, func_088584A0, Word);
HOT135(0x08C32D1Cu, func_08C32D1C, Word);
// HOT135(0x08AA8F88u, func_08AA8F88, Void);  // no comparable synthetic case yet
// HOT135(0x08AA9028u, func_08AA9028, Void);  // no comparable synthetic case yet
// HOT135(0x08AA90B8u, func_08AA90B8, Void);  // no comparable synthetic case yet
// HOT135(0x08AA9158u, func_08AA9158, Void);  // no comparable synthetic case yet
HOT135(0x08AA9318u, func_08AA9318, Void);
// HOT135(0x08AA93A8u, func_08AA93A8, Void);  // no comparable synthetic case yet
// HOT135(0x08AA9438u, func_08AA9438, Void);  // no comparable synthetic case yet
HOT135(0x08AA94C8u, func_08AA94C8, Void);
// HOT135(0x08AA9558u, func_08AA9558, Void);  // no comparable synthetic case yet
// HOT135(0x08851968u, func_08851968, Void);  // no comparable synthetic case yet
// HOT135(0x08851A6Cu, func_08851A6C, Void);  // no comparable synthetic case yet
// HOT135(0x08852EBCu, func_08852EBC, Void);  // no comparable synthetic case yet
// HOT135(0x08852FC0u, func_08852FC0, Void);  // no comparable synthetic case yet
// HOT135(0x088530C4u, func_088530C4, Void);  // no comparable synthetic case yet
// HOT135(0x088531C8u, func_088531C8, Void);  // no comparable synthetic case yet
// HOT135(0x088532CCu, func_088532CC, Void);  // no comparable synthetic case yet
// HOT135(0x088533D0u, func_088533D0, Void);  // no comparable synthetic case yet
// HOT135(0x088534D4u, func_088534D4, Void);  // no comparable synthetic case yet
// HOT135(0x088535D8u, func_088535D8, Void);  // no comparable synthetic case yet
// HOT135(0x088536DCu, func_088536DC, Void);  // no comparable synthetic case yet
// HOT135(0x088537E0u, func_088537E0, Void);  // no comparable synthetic case yet
// HOT135(0x088538E4u, func_088538E4, Void);  // no comparable synthetic case yet
// HOT135(0x088539E8u, func_088539E8, Void);  // no comparable synthetic case yet
// HOT135(0x08853AECu, func_08853AEC, Void);  // no comparable synthetic case yet
