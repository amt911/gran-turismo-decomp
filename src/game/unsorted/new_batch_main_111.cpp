// New batch main-111 (main session): re-drafts of functions whose earlier drafts mismatched the original in the synthetic gate.
// Structured C written from `tools/disasm`; delay slots are read as "before the branch / call". Callee-saved registers the
// original keeps values in are set where the asm sets them (a callee may read them) and the entry values are put back at the end;
// `v0` (and `f0` for the float ones) ends as the asm leaves it. Draft, not yet verified by the differential gate.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_111 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;
using u16 = std::uint16_t;
using s16 = std::int16_t;
using u8 = std::uint8_t;
using s8 = std::int8_t;

// A guest call; afterwards v0/v1/f0 hold the callee's, as after the original's `jal`.
GuestResult call_keep(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}
u32 call(Runtime &rt, AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments = {}) {
    return call_keep(rt, ctx, target, arguments).v0;
}

u8 byte_at(Runtime &rt, u32 address) { return rt.memory().load8(address); }
u16 half_at(Runtime &rt, u32 address) { return rt.memory().load16(address); }
s8 sbyte_at(Runtime &rt, u32 address) { return static_cast<s8>(rt.memory().load8(address)); }
s16 shalf_at(Runtime &rt, u32 address) { return static_cast<s16>(rt.memory().load16(address)); }
void put_byte(Runtime &rt, u32 address, u32 value) { rt.memory().store8(address, static_cast<u8>(value)); }
void put_half(Runtime &rt, u32 address, u32 value) { rt.memory().store16(address, static_cast<u16>(value)); }
s32 swd(Runtime &rt, u32 address) { return static_cast<s32>(word(rt, address)); }

// s0-s7 and fp are callee-saved: the body sets them where the original does and the entry values come back at the end.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx), fp_(ctx.gpr[30]) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
};

// The low half of `value * factor`, as a signed 32-bit word, divided by 100 (the compiler's magic 0x51EB851F).
s32 hundredth_of(u32 value, u32 factor) { return static_cast<s32>(value * factor) / 100; }

// The pair 0x0892CF9C / 0x0892D1C0 (a1 = the sums, a2 = the source, no frame): the 16 bytes at the sums' +0xC0 each take
// one source byte added, the byte +0xBD takes another, and the halfwords +0x10A and +0x10C become the low half of
// (their value times a source halfword) / 100. v0 = the first quotient (before the store truncates it).
void add_source_to_sums(Runtime &rt, AllegrexContext &ctx, u32 first_byte, u32 second_byte, u32 first_half, u32 second_half) {
    const u32 sums = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    for (u32 offset = 0xC0u; offset < 0xD0u; ++offset)
        put_byte(rt, sums + offset, byte_at(rt, sums + offset) + byte_at(rt, source + first_byte));
    put_byte(rt, sums + 0xBDu, byte_at(rt, sums + 0xBDu) + byte_at(rt, source + second_byte));
    const u32 first_old = half_at(rt, sums + 0x10Au);
    const u32 second_old = half_at(rt, sums + 0x10Cu);
    const u32 first = static_cast<u32>(hundredth_of(first_old, half_at(rt, source + first_half)));
    put_half(rt, sums + 0x10Au, first);
    const u32 second = static_cast<u32>(hundredth_of(second_old, half_at(rt, source + second_half)));
    put_half(rt, sums + 0x10Cu, second);
    ctx.gpr[2] = first;
}
void func_0892CF9C(Runtime &rt, AllegrexContext &ctx) { add_source_to_sums(rt, ctx, 0x7u, 0x8u, 0x0u, 0x2u); }
void func_0892D1C0(Runtime &rt, AllegrexContext &ctx) { add_source_to_sums(rt, ctx, 0x9u, 0xAu, 0x4u, 0x6u); }

// 0x0893B160 (a0 = this, a1 = key, a2 = out scale, a3 = out value): the entry 0x08955300(this) gives a mode byte at +0x10C
// (zero: out value = the float at 0x08CF29D8, out scale = 0, v0 = 0x08CF0000); otherwise the float at this[+0xC] + 0x328 +
// 4 * (key >> 1) (the `sra` sits in the delay slot of the mode test, so v0 = key >> 1 when the branch is taken) picks an axis
// from the curve at 0x08CF2990 (+0x34 below the lower edge, +0x38 above the upper, interpolated between), then a scale from
// |entry+0x34| / 100 squared times +0x3C times the axis, capped at +0x50, and a value interpolated over +0x40..+0x44.
void func_0893B160(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kCurve = 0x08CF2990u;
    constexpr u32 kFlat = 0x08CF29D8u;
    const float lower_edge = std::bit_cast<float>(0x3E0A3D71u);
    const float upper_edge = std::bit_cast<float>(0x3EB5C290u);
    const float edge_span = std::bit_cast<float>(0x3E6147AFu);
    const float hundred = std::bit_cast<float>(0x42C80000u);
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 out_scale = ctx.gpr[6];
    const u32 out_value = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    ctx.gpr[16] = key;                       // s0
    ctx.gpr[17] = word(rt, self + 0xCu);     // s1 (the delay slot of the call)
    ctx.gpr[18] = out_value;                 // s2
    ctx.gpr[19] = out_scale;                 // s3
    const u32 entry = call(rt, ctx, 0x08955300u, {self});
    ctx.gpr[3] = entry;
    const u32 table = ctx.gpr[17];
    if (byte_at(rt, entry + 0x10Cu) == 0u) {
        ctx.gpr[2] = 0x08CF0000u;
        const float flat = float_at(rt, kFlat);
        ctx.fpr[0] = flat;
        put_float(rt, out_value, flat);
        put(rt, out_scale, 0u);
        return;
    }
    const u32 index = static_cast<u32>(static_cast<s32>(key) >> 1);
    const float input = float_at(rt, table + index * 4u + 0x328u);
    const float low = float_at(rt, kCurve + 0x34u);
    const float high = float_at(rt, kCurve + 0x38u);
    float axis;
    if (input < lower_edge) {
        axis = low;
    } else if (upper_edge < input) {
        axis = high;
    } else {
        axis = low + ((input - lower_edge) * (high - low)) / edge_span;
    }
    float distance = float_at(rt, entry + 0x34u);
    if (distance < 0.0f) distance = -distance;
    const float normalized = distance / hundred;
    const float gain = axis * ((normalized * normalized) * float_at(rt, kCurve + 0x3Cu));
    const float upper = float_at(rt, kCurve + 0x40u);
    float value;
    if (distance < upper) {
        value = float_at(rt, kCurve + 0x48u);
    } else {
        const float lower = float_at(rt, kCurve + 0x44u);
        if (lower < distance) {
            value = float_at(rt, kCurve + 0x4Cu);
        } else {
            const float start = float_at(rt, kCurve + 0x48u);
            const float end = float_at(rt, kCurve + 0x4Cu);
            value = start + ((distance - upper) * (end - start)) / (lower - upper);
        }
    }
    float scale = gain;
    const float cap = float_at(rt, kCurve + 0x50u);
    if (cap < scale) scale = cap;
    ctx.gpr[2] = kCurve;
    ctx.fpr[0] = value;
    put_float(rt, out_value, value);
    put_float(rt, out_scale, scale);
}

// 0x08959C9C (a0 = the two pointers 0x08959180 needs, a1 = the packet whose first word is its id, a2 = the new id): with the
// packet's id neither 0 nor -1 (else v0 = 1), 0x08959180(a0[0], byte of a0[8]) prepares the record; when the record's id at +4
// differs from the packet's (else v0 = the packet's id) the 0x34 bytes at +4 move to +0x3C (the previous blob), the record's
// +0x70 takes the old +0x38, +0x38 gets the new id, the packet's 0x34 bytes go to +4, and a -1 id at +0 is replaced. Then the
// state at +0xA0: 0 returns -1, 3 returns 3, 1 becomes 2 (returns 2), anything else accumulates the counters at +0xA4..+0xD0
// (signed compares throughout); v0 = the new +0xA4 count.
void func_08959C9C(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 packet = ctx.gpr[5];
    const u32 id = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[17] = id;      // s1
    ctx.gpr[16] = packet;  // s0
    if (word(rt, packet) + 1u < 2u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 block = word(rt, owner + 8u);
    ctx.gpr[2] = block;
    const u32 record = call(rt, ctx, 0x08959180u, {word(rt, owner), byte_at(rt, block)});
    if (word(rt, record + 4u) == word(rt, packet)) {
        ctx.gpr[2] = word(rt, packet);
        return;
    }
    put(rt, record + 0x70u, word(rt, record + 0x38u));
    for (u32 i = 0u; i < 0x30u; i += 4u) put(rt, record + 0x3Cu + i, word(rt, record + 4u + i));
    put(rt, record + 0x3Cu + 0x30u, word(rt, record + 0x34u));
    put(rt, record + 0x38u, id);
    for (u32 i = 0u; i < 0x30u; i += 4u) put(rt, record + 4u + i, word(rt, packet + i));
    put(rt, record + 0x34u, word(rt, packet + 0x30u));
    if (word(rt, record) == 0xFFFFFFFFu) put(rt, record, id);
    const u32 state = word(rt, record + 0xA0u);
    if (state == 0u) {
        ctx.gpr[2] = 0xFFFFFFFFu;
        return;
    }
    if (state == 3u) {
        ctx.gpr[2] = 3u;
        return;
    }
    if (state == 1u) {
        put(rt, record + 0xA0u, 2u);
        ctx.gpr[2] = 2u;
        return;
    }
    const s32 first = swd(rt, record + 4u);
    const s32 limit = swd(rt, record + 0x3Cu);
    const s32 available = swd(rt, record + 0x38u);
    const s32 previous = swd(rt, record + 0x70u);
    if (first < limit && available > 0 && previous > 0 && first > 0 && limit > 0)
        put(rt, record + 0xA8u, word(rt, record + 0xA8u) + 1u);
    s32 left = available - previous;
    if (left < 0) left = 0;
    put(rt, record + 0xACu, word(rt, record + 0xACu) + static_cast<u32>(left));
    if (swd(rt, record + 0xB0u) < left) put(rt, record + 0xB0u, static_cast<u32>(left));
    if (left >= 0x258) put(rt, record + 0xB4u, word(rt, record + 0xB4u) + 1u);
    s32 moved = static_cast<s32>(id - static_cast<u32>(first));
    if (moved < 0) moved = 0;
    put(rt, record + 0xB8u, word(rt, record + 0xB8u) + static_cast<u32>(moved));
    if (swd(rt, record + 0xBCu) < moved) put(rt, record + 0xBCu, static_cast<u32>(moved));
    if (moved >= 0x5DC) put(rt, record + 0xC0u, word(rt, record + 0xC0u) + 1u);
    const u32 count = word(rt, record + 0xA4u);
    if (count != 0u) {
        const s32 earlier = swd(rt, record + 0xCCu);
        put(rt, record + 0xC4u, word(rt, record + 0xC4u) + static_cast<u32>(earlier));
        if (swd(rt, record + 0xC8u) < earlier) put(rt, record + 0xC8u, static_cast<u32>(earlier));
        if (earlier >= 0x708) put(rt, record + 0xD0u, word(rt, record + 0xD0u) + 1u);
    }
    put(rt, record + 0xA4u, count + 1u);
    put(rt, record + 0xCCu, static_cast<u32>(moved));
    ctx.gpr[2] = count + 1u;
}

// 0x0895F194 (a0 = the object whose word at +8 is the track data, a1 = the segment index, a2 = the out xyz, a3 = a float out):
// samples the 12-byte point records at data + 0x610 (xyz at +8/+0xC/+0x10). With the state byte at +0x67A equal to 20 it copies
// the record of (byte +0x678 - index, +5 when negative) and returns (4 - index) * 20 / 80; with index 0 it runs
// 0x0895F174(object, out) and returns 1.0 (float out = (20 - state) / 60); for index 1..4 it copies the record of
// (byte +0x678 - index + 1, +6 when negative) and returns ((4 - index) * 20 + state) / 80; for index 5 and up it
// interpolates the records of (byte +0x678 - index + 2, +7 when negative) and (byte +0x679) by state / 20 and returns 0
// (float out = state / 60). The copying cases set the float out to 0x3EAAAAAB. v0 ends as the last `sll v0, slot, 2`.

// 0x0895FA54 (a0 = a parameter block, a1 = a selector; -1 acts as 0): fills the block from the tables at 0x08CDE508. The byte count at
// +0x23 selects how many rows of the two speed tables at 0x08CDE52C (doubled, * 0.2777) / 0x08CDE534 (* 0.001) go to +0xC / +0x2C,
// then 0x08946734 gets them; the byte at +0x34 gives the factor at +0x4C; the bytes at +0x14..+0x19 (two rows of three, * 0.005) and
// +0x11/+0x17/+0x1D go to +0x50.. / +0x68..; the bytes +0x1E (/ 10), +0x1F (100 - x) / 100, +0x20, +0x21 (1 / (x * 10)) and +0x22
// (/ 100) go to +0x134..+0x144; a 24-row pass fills +0x74, +0xD4, +0x148, +0x1A8, +0x208 (stride 4) and +0x268, +0x280 (stride 1); a
// selector of 3 maps row 5 of that pass to row 15. v0 = the last row's byte +0xC8; s2 holds 0x08CE0000 (the `lui` of the table
// addresses) while the body runs.
void func_0895FA54(Runtime &rt, AllegrexContext &ctx) {
    constexpr u32 kTable = 0x08CDE508u;
    constexpr u32 kSpeeds = 0x08CDE534u;
    constexpr u32 kPairs = 0x08CDE558u;
    const u32 object = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[18] = 0x08CE0000u;  // s2
    ctx.gpr[17] = ctx.gpr[5] == 0xFFFFFFFFu ? 0u : ctx.gpr[5];  // s1
    ctx.gpr[16] = object;       // s0
    const u32 selector = ctx.gpr[17];
    const float per_row = std::bit_cast<float>(0x3E8E38E4u);
    const float thousandth = std::bit_cast<float>(0x3A83126Fu);
    const float hundredth = std::bit_cast<float>(0x3C23D70Au);
    const float five_thousandths = std::bit_cast<float>(0x3BA3D70Au);
    const float tenth = std::bit_cast<float>(0x3DCCCCCDu);
    const float two_pi = std::bit_cast<float>(0x40C90FDBu);

    const u32 rows = byte_at(rt, kTable + 0x23u);
    for (u32 i = 0u; i < rows; ++i) {
        put_float(rt, object + 0xCu + 4u * i, static_cast<float>(static_cast<s32>(byte_at(rt, kSpeeds - 8u + i) << 1)) * per_row);
        put_float(rt, object + 0x2Cu + 4u * i, static_cast<float>(static_cast<s32>(byte_at(rt, kSpeeds + i))) * thousandth);
    }
    ctx.gpr[5] = rows;
    call(rt, ctx, 0x08946734u, {object, rows, object + 0xCu, object + 0x2Cu, 0u, 0u});

    float factor = 1.0f;
    const u32 hundredths = byte_at(rt, kTable + 0x34u);
    if (hundredths != 0u) factor = static_cast<float>(static_cast<s32>(hundredths)) / 100.0f + 1.0f;
    put_float(rt, object + 0x4Cu, factor);

    for (u32 i = 0u; i < 2u; ++i) {
        const u32 row = kTable + 0xCu + i;
        put_float(rt, object + 0x50u + 4u * i, static_cast<float>(byte_at(rt, row)) * five_thousandths);
        put_float(rt, object + 0x58u + 4u * i, static_cast<float>(byte_at(rt, row + 6u)) * five_thousandths);
        put_float(rt, object + 0x60u + 4u * i, static_cast<float>(byte_at(rt, row + 0xCu)) * five_thousandths);
    }
    put_float(rt, object + 0x68u, static_cast<float>(byte_at(rt, kTable + 0x11u)) * five_thousandths);
    put_float(rt, object + 0x6Cu, static_cast<float>(byte_at(rt, kTable + 0x17u)) * five_thousandths);
    put_float(rt, object + 0x70u, static_cast<float>(byte_at(rt, kTable + 0x1Du)) * five_thousandths);

    const u32 tenths = byte_at(rt, kTable + 0x1Eu);
    put_float(rt, object + 0x134u, tenths != 0u ? static_cast<float>(static_cast<s32>(tenths)) / 10.0f : 1.0f);
    const u32 percent = byte_at(rt, kTable + 0x1Fu);
    put_float(rt, object + 0x138u, percent != 0u ? static_cast<float>(100 - static_cast<s32>(percent)) / 100.0f : 0.0f);
    for (u32 i = 0u; i < 2u; ++i) {
        const u32 value = byte_at(rt, kTable + 0x20u + i);
        put_float(rt, object + 0x13Cu + 4u * i, value != 0u ? 1.0f / static_cast<float>(static_cast<s32>(value * 10u)) : 0.0f);
    }
    const u32 permille = byte_at(rt, kTable + 0x22u);
    put_float(rt, object + 0x144u, permille != 0u ? static_cast<float>(static_cast<s32>(permille)) / 100.0f : 1.0f);

    u32 last = 0u;
    for (u32 i = 0u; i < 24u; ++i) {
        const u32 row = (selector == 3u && i == 5u) ? 0xFu : i;
        const u32 first = byte_at(rt, kPairs + i - 0x18u);
        put_float(rt, object + 0x74u + 4u * i, first != 0u ? static_cast<float>(static_cast<s32>(first)) / 100.0f : 1.0f);
        const u32 second = byte_at(rt, kPairs + i);
        put_float(rt, object + 0xD4u + 4u * i,
                  second != 0u ? 1.0f / static_cast<float>(static_cast<s32>(second * 10u)) : 0.0f);
        put_float(rt, object + 0x148u + 4u * i, static_cast<float>(shalf_at(rt, kTable + row * 2u + 0x80u)) * hundredth);
        put_float(rt, object + 0x1A8u + 4u * i, static_cast<float>(byte_at(rt, kTable + row + 0x68u)) * thousandth);
        put_float(rt, object + 0x208u + 4u * i, two_pi / (static_cast<float>(byte_at(rt, kTable + row + 0xB0u)) * tenth));
        last = byte_at(rt, kTable + row + 0xC8u);
        put_byte(rt, object + 0x268u + i, last);
        put_byte(rt, object + 0x280u + i, byte_at(rt, kTable + row + 0xE0u));
    }
    ctx.gpr[2] = last;
}

// The list walkers (a0 = self): `prepare(self)`, then `head(0)` answers the head of a linked list (next at +0x0C); for every node
// 0x08B85EB8(&text, node[0], &flag) and 0x08B88F44(&name, &text) build two temporaries, the node is stored at name + 4 (the delay
// slot of the next call, which reads it) and 0x08BE9D90(self, &name) consumes it; then `name` and `text` are released: when the
// temporary's header (word - 0xC) is not the shared empty 0x08D74FD0, 0x08B60804(word - 4, -1) drops a reference and a
// non-positive answer goes on to 0x08B8542C(word - 0xC, &byte). The v0/v1 of the consumer are also parked in the frame.
void walk_list(Runtime &rt, AllegrexContext &ctx, u32 prepare, u32 head) {
    constexpr u32 kEmpty = 0x08D74FD0u;
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    const u32 text = frame.at(0x4u);
    const u32 flag = frame.at(0x2u);
    const u32 name = frame.at(0x10u);
    ctx.gpr[21] = self;   // s5
    call_keep(rt, ctx, prepare, {self});
    u32 node = call(rt, ctx, head, {0u});
    ctx.gpr[17] = node;   // s1
    ctx.gpr[20] = kEmpty; // s4
    ctx.gpr[19] = text;   // s3
    ctx.gpr[22] = flag;   // s6
    ctx.gpr[18] = name;   // s2
    if (node == 0u) return;
    for (;;) {
        call_keep(rt, ctx, 0x08B85EB8u, {text, word(rt, node), flag});
        call_keep(rt, ctx, 0x08B88F44u, {name, text});
        put(rt, name + 4u, node);
        const GuestResult consumed = call_keep(rt, ctx, 0x08BE9D90u, {self, name});
        put(rt, frame.at(0x24u), consumed.v1);
        put(rt, frame.at(0x8u), consumed.v0);
        put_byte(rt, frame.at(0xCu), byte_at(rt, frame.at(0x24u)));
        put(rt, frame.at(0x20u), consumed.v0);
        const u32 first = word(rt, name);
        ctx.gpr[16] = first - 0xCu;  // s0
        if (first - 0xCu != kEmpty) {
            call_keep(rt, ctx, 0x08B60804u, {first - 4u, 0xFFFFFFFFu});
            if (static_cast<s32>(ctx.gpr[2]) <= 0) call_keep(rt, ctx, 0x08B8542Cu, {first - 0xCu, frame.at(1u)});
        }
        const u32 second = word(rt, text);
        ctx.gpr[16] = second - 0xCu;
        if (second - 0xCu != kEmpty) {
            call_keep(rt, ctx, 0x08B60804u, {second - 4u, 0xFFFFFFFFu});
            if (static_cast<s32>(ctx.gpr[2]) <= 0) call_keep(rt, ctx, 0x08B8542Cu, {second - 0xCu, frame.at(0u)});
        }
        node = word(rt, node + 0xCu);
        ctx.gpr[17] = node;
        if (node == 0u) return;
    }
}
void func_089A4AE4(Runtime &rt, AllegrexContext &ctx) { walk_list(rt, ctx, 0x08A0DF1Cu, 0x08BF5A9Cu); }
void func_089B8580(Runtime &rt, AllegrexContext &ctx) { walk_list(rt, ctx, 0x08A0DF1Cu, 0x08BFDEDCu); }
void func_089B9B48(Runtime &rt, AllegrexContext &ctx) { walk_list(rt, ctx, 0x089BCCDCu, 0x08BFE5A4u); }
void func_089D0A5C(Runtime &rt, AllegrexContext &ctx) { walk_list(rt, ctx, 0x089958A0u, 0x08C02400u); }
void func_089F88A0(Runtime &rt, AllegrexContext &ctx) { walk_list(rt, ctx, 0x089D7BE0u, 0x08C0C34Cu); }
void func_089FFE38(Runtime &rt, AllegrexContext &ctx) { walk_list(rt, ctx, 0x08A10020u, 0x08C0EF40u); }

constexpr u32 kEmptyString = 0x08D74FD0u;
// The compiler's string release: header = pointer - 0xC (kept in the callee-saved register `header_register`); unless it is the
// shared empty one, 0x08B60804(pointer - 4, -1) drops a reference and a non-positive answer goes on to 0x08B8542C(header, marker).
void release_string(Runtime &rt, AllegrexContext &ctx, unsigned header_register, u32 pointer, u32 marker) {
    const u32 header = pointer - 0xCu;
    ctx.gpr[header_register] = header;
    if (header == kEmptyString) return;
    call_keep(rt, ctx, 0x08B60804u, {pointer - 4u, 0xFFFFFFFFu});
    if (static_cast<s32>(ctx.gpr[2]) <= 0) call_keep(rt, ctx, 0x08B8542Cu, {header, marker});
}

// 0x089A54A0 (a0 = this, a1 = a key, a2 = an output): nothing (v0 = 0) when the output's first word is null; with this + 0x16C as
// the map, 0x08BF63A8(map) must answer this + 0x170 (the end), else v0 is its answer. 0x08BF6308(map, key) answers the record;
// when it is the end, or when the key's block (word [key]) compares smaller than the record's block (word +0x10) through 0x08B36170
// (the difference of their counts at -0xC, the call's non-zero answer replacing it), a local pair is built (0x08A5BAB8(&local,
// 0x08CF6ED8), 0x08B88F44(&other, key), 0x089FE2F0(&third, &local)) and 0x08BF76A8(map, record, &other) inserts, then the temporaries
// are released and the record becomes the insertion's answer. The record's word +0x14 is finally assigned the output's word unless
// that is the same address.
void func_089A54A0(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const u32 output = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[20] = output;  // s4
    ctx.gpr[19] = key;     // s3
    ctx.gpr[2] = word(rt, output);
    ctx.gpr[16] = self;    // s0
    if (ctx.gpr[2] == 0u) return;
    const u32 map = self + 0x16Cu;
    ctx.gpr[18] = map;     // s2
    call_keep(rt, ctx, 0x08BF63A8u, {map});
    ctx.gpr[16] = self + 0x170u;
    if (ctx.gpr[2] != self + 0x170u) return;
    const u32 found = call(rt, ctx, 0x08BF6308u, {map, key});
    ctx.gpr[22] = found;   // s6
    u32 target = found + 0x14u;  // a0 at the final compare
    bool insert = found == self + 0x170u;
    if (!insert) {
        ctx.gpr[21] = found;  // s5
        const u32 key_block = word(rt, key);
        const u32 key_count = word(rt, key_block - 0xCu);
        ctx.gpr[16] = key_count;
        put(rt, frame.at(8u), key_count);
        const u32 record_block = word(rt, found + 0x10u);
        const u32 record_count = word(rt, record_block - 0xCu);
        ctx.gpr[17] = record_count;
        const u32 smaller = record_count < key_count ? record_count : key_count;
        put(rt, frame.at(0xCu), record_count);
        s32 difference = static_cast<s32>(key_count - record_count);
        ctx.gpr[16] = static_cast<u32>(difference);
        const u32 compared = call(rt, ctx, 0x08B36170u, {key_block, word(rt, found + 0x10u), smaller});
        if (compared != 0u) difference = static_cast<s32>(compared);
        ctx.gpr[16] = static_cast<u32>(difference);
        insert = difference < 0;
    }
    if (insert) {
        const u32 local = frame.at(4u);
        const u32 other = frame.at(0x10u);
        const u32 third = frame.at(0x14u);
        ctx.gpr[21] = local;  // s5
        ctx.gpr[17] = other;  // s1
        call_keep(rt, ctx, 0x08A5BAB8u, {local, 0x08CF6ED8u});
        ctx.gpr[16] = third;  // s0
        call_keep(rt, ctx, 0x08B88F44u, {other, key});
        call_keep(rt, ctx, 0x089FE2F0u, {third, local});
        const u32 inserted = call(rt, ctx, 0x08BF76A8u, {map, found, other});
        ctx.gpr[17] = inserted;
        call_keep(rt, ctx, 0x08A5B9ECu, {third});
        release_string(rt, ctx, 16u, word(rt, other), frame.at(0u));
        call_keep(rt, ctx, 0x08A5B9ECu, {local});
        ctx.gpr[21] = inserted;
        target = inserted + 0x14u;
    }
    if (output != target) call_keep(rt, ctx, 0x08A5BAD8u, {target, word(rt, output)});
}

// 0x089DA994 (selector = a0, id = a1; only compared, never dereferenced): any selector but 1 answers v0 = 0xFFFF (the `ori` sits in
// the delay slot of the selector test) except selector 0 with id 0xFFFF, which destroys 0x08CF5400 with 0x08B28DC4 and tail-calls the
// same destructor for 0x08CF5428. Selector 1 with id 0xFFFF constructs the statics 0x08CF5428 / 0x08CF5400 (0x08B28DCC), hands the
// 0x1C0-byte descriptor 0x08CF531C to 0x08A92114, copies eleven words 0x08C8CC28 down to 0x08C8CC00 into every other word of
// 0x08CDEAB8, passes that table to 0x08B2095C, registers a callback pair with 0x0897D534 and then twelve (name, text, pair) entries
// through 0x08C0645C (each pair is copied into a frame slot first); v0 is the last registration's result. s1 = 0x08CA0000 and
// s0 = 0x08CF0000 while the body runs.
void func_089DA994(Runtime &rt, AllegrexContext &ctx) {
    const u32 selector = ctx.gpr[4];
    const u32 id = ctx.gpr[5];
    {
        SavedRegisters saved(ctx);
        GuestFrame frame(ctx, 0x70u);
        if (selector != 1u) {
            ctx.gpr[2] = 0xFFFFu;
            if (selector != 0u || id != 0xFFFFu) return;
            call_keep(rt, ctx, 0x08B28DC4u, {0x08CF5400u});
        } else {
            ctx.gpr[2] = 0xFFFFu;
            if (id != 0xFFFFu) return;
            call_keep(rt, ctx, 0x08B28DCCu, {0x08CF5428u});
            call_keep(rt, ctx, 0x08B28DCCu, {0x08CF5400u});
            ctx.gpr[17] = 0x08CA0000u;
            call_keep(rt, ctx, 0x08A92114u, {0x08CF531Cu, 0x08CA7434u, 0x1C0u, 0x089DA8C4u, 0x089DBB5Cu, 0x089DA8E8u});
            for (u32 index = 0u; index != 0xBu; ++index)
                put(rt, 0x08CDEAB8u + 8u * index, word(rt, 0x08C8CC28u - 4u * index));
            ctx.gpr[16] = 0x08CF0000u;
            call_keep(rt, ctx, 0x08B2095Cu, {0x08CF53F8u, 0x08CDEAB8u, 0x0Bu});
            call_keep(rt, ctx, 0x0897D534u, {0x08CF530Cu, 0x08CA7434u, 0x089DC0ECu, 0x089DB78Cu});
            struct Registration {
                u32 name;
                u32 text;
                u32 pair;
            };
            static constexpr Registration kRegistrations[] = {
                {0x08CF53E8u, 0x08CA7440u, 0x08C8D364u}, {0x08CF53D8u, 0x08CA744Cu, 0x08C8D35Cu},
                {0x08CF53C8u, 0x08CA7454u, 0x08C8D354u}, {0x08CF53B8u, 0x08CA745Cu, 0x08C8D34Cu},
                {0x08CF53A8u, 0x08CA7468u, 0x08C8D344u}, {0x08CF5398u, 0x08CA7474u, 0x08C8D33Cu},
                {0x08CF5388u, 0x08CA7480u, 0x08C8D334u}, {0x08CF5378u, 0x08CA7490u, 0x08C8D32Cu},
                {0x08CF5368u, 0x08CA749Cu, 0x08C8D324u}, {0x08CF5358u, 0x08CA74A8u, 0x08C8D31Cu},
                {0x08CF5348u, 0x08CA74B4u, 0x08C8D314u}, {0x08CF5338u, 0x08CA74BCu, 0x08C8D30Cu},
            };
            for (u32 index = 0u; index != 0xCu; ++index) {
                const Registration &entry = kRegistrations[index];
                const u32 slot = frame.at(0x58u - 8u * index);
                put(rt, slot, word(rt, entry.pair));
                put(rt, slot + 4u, word(rt, entry.pair + 4u));
                call_keep(rt, ctx, 0x08C0645Cu, {entry.name, entry.text, slot, 0x08CF530Cu});
            }
            return;
        }
    }
    tail_call(ctx, 0x08B28DC4u, {0x08CF5428u});
}

// 0x089DBCE8 (a0 = this): the vtable words 0x08CBC738 (at +0) and 0x08CBC970 (at +0x38) are stored, this + 0x13C and + 0x138 are
// released (0x08A5B9EC), the counted block at this + 0x134 is freed, then three ranges of two 0xC-byte elements — the range
// ending at this + 0x134 through 0x08C06684, the ones ending at +0x11C and +0x104 through 0x08BF0BF0 — are destroyed from the last
// element down (`release(word, word + 4, &marker)`, then 0x08B35254 for the element's first word when it is not null);
// 0x0896BC0C(this) finishes.
void destroy_elements(Runtime &rt, AllegrexContext &ctx, u32 last, u32 first, u32 marker, u32 release) {
    for (u32 element = last;; element -= 0xCu) {
        ctx.gpr[16] = element;  // s0
        call_keep(rt, ctx, release, {word(rt, element), word(rt, element + 4u), marker});
        const u32 held = word(rt, element);
        ctx.gpr[2] = held;
        if (held != 0u) call_keep(rt, ctx, 0x08B35254u, {held});
        if (element == first) break;
    }
}
void func_089DBCE8(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = self;  // s3
    put(rt, self, 0x08CBC738u);
    put(rt, self + 0x38u, 0x08CBC970u);
    call_keep(rt, ctx, 0x08A5B9ECu, {self + 0x13Cu});
    call_keep(rt, ctx, 0x08A5B9ECu, {self + 0x138u});
    ctx.gpr[17] = self + 0x11Cu;  // s1
    release_string(rt, ctx, 16u, word(rt, self + 0x134u), frame.at(3u));
    ctx.gpr[18] = frame.at(2u);   // s2
    destroy_elements(rt, ctx, self + 0x128u, self + 0x11Cu, frame.at(2u), 0x08C06684u);
    ctx.gpr[18] = self + 0x104u;
    ctx.gpr[17] = frame.at(1u);
    destroy_elements(rt, ctx, self + 0x110u, self + 0x104u, frame.at(1u), 0x08BF0BF0u);
    ctx.gpr[17] = self + 0xECu;
    destroy_elements(rt, ctx, self + 0xF8u, self + 0xECu, frame.at(0u), 0x08BF0BF0u);
    call_keep(rt, ctx, 0x0896BC0Cu, {self});
}

// 0x08A14778 (a0 = owner): builds the 0x1B0-byte object (0x08A5BC20(0x1B0, 4), constructed by 0x08A2A154, vtable word 0x08CD04B0),
// the member at +0x18 (0x08A1A914) and two at +0xAC and +0xD8 (0x08B28D34); the inline copies put the 0x80 bytes at 0x08C8E2B0 at
// +0x10D and the eight words at 0x08C8E290 at +0x18D (unaligned: swl/swr pairs), then +0x10D gets 0x80 zero bytes and +0x18D 0x20
// (0x08B36018), 0x80 bytes are copied from 0x08CA9A84 (0x08B7C830) and the member +0x14 points at +0x18. 0x08A2A218(owner, &object)
// finishes; v0 is its answer.
void func_08A14778(Runtime &rt, AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[19] = owner;  // s3
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x1B0u, 4u});
    ctx.gpr[16] = object;  // s0
    call_keep(rt, ctx, 0x08A2A154u, {object});
    const u32 member = object + 0x18u;
    ctx.gpr[18] = member;  // s2
    put(rt, object, 0x08CD04B0u);
    call_keep(rt, ctx, 0x08A1A914u, {member});
    call_keep(rt, ctx, 0x08B28D34u, {object + 0xACu});
    call_keep(rt, ctx, 0x08B28D34u, {object + 0xD8u});
    const u32 text = object + 0x10Du;
    ctx.gpr[17] = text;  // s1
    put(rt, object + 0x104u, 0u);
    put(rt, object + 0x108u, 0u);
    put_byte(rt, object + 0x10Cu, 0u);
    for (u32 i = 0u; i < 0x80u; ++i) put_byte(rt, text + i, byte_at(rt, 0x08C8E2B0u + i));
    for (u32 i = 0u; i < 0x20u; ++i) put_byte(rt, object + 0x18Du + i, byte_at(rt, 0x08C8E290u + i));
    call_keep(rt, ctx, 0x08B36018u, {text, 0u, 0x80u});
    call_keep(rt, ctx, 0x08B36018u, {object + 0x18Du, 0u, 0x20u});
    call_keep(rt, ctx, 0x08B7C830u, {text, 0x08CA9A84u, 0x80u});
    put(rt, object + 0x14u, member);
    put(rt, frame.at(0u), object);
    call_keep(rt, ctx, 0x08A2A218u, {owner, frame.at(0u)});
}

// 0x08A243FC (a0 = a destination object, a1 = a count, a2 = two objects): nothing when the count (signed) is below 2 (`slti a1, a1, 2`
// + `bnez`); else 0x08A6C410(&first, a2) and 0x08A6C410(&second, a2 + 4) then 0x08ABB9E8(0) answer a record; the first's +0xC virtual
// fills a local, the second's +0xC virtual a second local, 0x08ABBAE0(record, first-local, second-local) answers another whose
// 0x08A97A6C(&result, record) the destination is assigned from (0x08A5BAD8, unless it is the result's own address), and both
// locals' strings and the three temporaries are released.
void func_08A243FC(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const s32 count = static_cast<s32>(ctx.gpr[5]);
    const u32 objects = ctx.gpr[6];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    ctx.gpr[19] = destination;  // s3
    ctx.gpr[16] = objects;      // s0
    if (count < 2) return;
    const u32 first_slot = frame.at(0x10u);
    const u32 second_slot = frame.at(0x14u);
    ctx.gpr[21] = first_slot;   // s5
    call_keep(rt, ctx, 0x08A6C410u, {first_slot, objects});
    ctx.gpr[20] = second_slot;  // s4
    call_keep(rt, ctx, 0x08A6C410u, {second_slot, objects + 4u});
    u32 result = call(rt, ctx, 0x08ABB9E8u, {0u});
    const u32 first = word(rt, first_slot);
    ctx.gpr[16] = first;
    ctx.gpr[18] = result;       // s2
    if (first == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAACD8u});
    ctx.gpr[3] = word(rt, first);
    call_indirect(rt, ctx, word(rt, word(rt, first) + 0xCu), {frame.at(0xCu), first});
    const u32 second = word(rt, second_slot);
    ctx.gpr[16] = second;
    ctx.gpr[17] = word(rt, frame.at(0xCu));  // s1
    if (second == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAACD8u});
    ctx.gpr[3] = word(rt, second);
    call_indirect(rt, ctx, word(rt, word(rt, second) + 0xCu), {frame.at(8u), second});
    result = call(rt, ctx, 0x08ABBAE0u, {result, ctx.gpr[17], word(rt, frame.at(8u))});
    ctx.gpr[18] = result;
    ctx.gpr[17] = kEmptyString;
    release_string(rt, ctx, 16u, word(rt, frame.at(8u)), frame.at(1u));
    release_string(rt, ctx, 16u, word(rt, frame.at(0xCu)), frame.at(0u));
    const u32 local = frame.at(4u);
    ctx.gpr[16] = local;
    call_keep(rt, ctx, 0x08A97A6Cu, {local, result});
    if (destination != local) call_keep(rt, ctx, 0x08A5BAD8u, {destination, word(rt, local)});
    call_keep(rt, ctx, 0x08A5B9ECu, {local});
    call_keep(rt, ctx, 0x08A5B9ECu, {second_slot});
    call_keep(rt, ctx, 0x08A5B9ECu, {first_slot});
}

// 0x08A4037C (a0 = this): 0x08B85EB8(&name, 0x08CADA50, &flag) fills a string slot (the delay slot loads the virtual at
// word(vtable + 0x108) into s1), the virtual runs with (this, &name — the slot's address, not its content), the string is released,
// a replacement from 0x08A76A94 takes the place of the object held at +0x38 (0x08A5BA08 on the new one, 0x08A5B930 on the old one) and
// eight handlers are registered on this.
void func_08A4037C(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x20u);
    const u32 slot = frame.at(4u);
    ctx.gpr[18] = self;  // s2
    ctx.gpr[16] = slot;  // s0
    const u32 table = word(rt, self);
    ctx.gpr[2] = table;
    const u32 handler = word(rt, table + 0x108u);
    ctx.gpr[17] = handler;  // s1
    call_keep(rt, ctx, 0x08B85EB8u, {slot, 0x08CADA50u, frame.at(1u)});
    ctx.gpr[2] = handler;
    call_keep(rt, ctx, handler, {self, slot});
    release_string(rt, ctx, 16u, word(rt, slot), frame.at(0u));
    const u32 replacement = call(rt, ctx, 0x08A76A94u);
    ctx.gpr[16] = replacement;
    if (replacement != 0u) call_keep(rt, ctx, 0x08A5BA08u, {replacement});
    const u32 held = word(rt, self + 0x38u);
    ctx.gpr[2] = held;
    if (held != 0u) call_keep(rt, ctx, 0x08A5B930u, {held});
    put(rt, self + 0x38u, replacement);
    call_keep(rt, ctx, 0x08B8850Cu, {self, 0x08CF6F14u, 0x08A40294u});
    call_keep(rt, ctx, 0x08B87108u, {self, 0x08CADA94u, 0x08A3F7ACu});
    call_keep(rt, ctx, 0x08B878E8u, {self, 0x08CADA9Cu, 0x08A3FC30u, 0x08A40A54u});
    call_keep(rt, ctx, 0x08B8D51Cu, {self, 0x08CADAA8u, 0x08A3FCB0u, 0u});
    call_keep(rt, ctx, 0x08B8D51Cu, {self, 0x08CADAB0u, 0x08A3FD58u, 0u});
    call_keep(rt, ctx, 0x08B8D51Cu, {self, 0x08CADAB8u, 0x08A3FE08u, 0u});
    call_keep(rt, ctx, 0x08B86FB0u, {self, 0x08CADAC0u, 0x08A3F6C4u});
    call_keep(rt, ctx, 0x08B86FB0u, {self, 0x08CADAD8u, 0x08A3F258u});
}

// 0x08A58394 (a0 = self, a1 = an owner): 0x08A76ACC(self), vtable 0x08CB7730 at self (the delay slot of the next call),
// 0x08A52A98(self + 0x14, owner), the virtual at +0x120 of the object [owner] answers a value, 0x08A5BC20(0x18, 4) allocates a
// block (the delay slot keeps the virtual's answer in s0), 0x08A56018(block, 0, answer, 0, 0) builds it and the *block* (the
// allocation's v0, copied into s1 in the delay slot) goes into the frame; 0x08A5BAB8(self + 0x18, &slot) and self + 0x1C = 0.
void func_08A58394(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 owner = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[18] = self;         // s2
    ctx.gpr[19] = self + 0x18u; // s3
    ctx.gpr[16] = owner;        // s0
    call_keep(rt, ctx, 0x08A76ACCu, {self});
    put(rt, self, 0x08CB7730u);
    call_keep(rt, ctx, 0x08A52A98u, {self + 0x14u, owner});
    const u32 object = word(rt, owner);
    ctx.gpr[16] = object;
    if (object == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CAF798u});
    call_indirect(rt, ctx, word(rt, word(rt, object) + 0x120u), {object});
    ctx.gpr[16] = ctx.gpr[2];  // s0 = the virtual's answer (the delay slot of the allocation call)
    const u32 answer = ctx.gpr[2];
    const u32 block = call(rt, ctx, 0x08A5BC20u, {0x18u, 4u});
    call_keep(rt, ctx, 0x08A56018u, {block, 0u, answer, 0u, 0u});
    ctx.gpr[17] = block;  // s1 (the delay slot sees the allocation's v0)
    put(rt, frame.at(0u), block);
    call_keep(rt, ctx, 0x08A5BAB8u, {self + 0x18u, frame.at(0u)});
    put(rt, self + 0x1Cu, 0u);
}

// 0x08A9C340 (a0 = this): when this's byte +0x7C is already set the function returns with that byte as v0; else it is set,
// 0x08A920E0(0) answers a list whose nodes (next at +0x10) get their +4 virtuals and then their +8 virtuals called (no arguments set),
// 0x08AA0AA4(&local) answers an object that replaces this + 0x34 when it is not already that address, 0x08A5BC20(0x38, 4) answers an
// object that 0x08A7E914 initializes with 0x08CF6F0C and this + 0x38 takes (0x08A76AB4(&second, &object) before the assignment),
// then 0x08A7BE18(this[+0x38], this[+0x34]) and 0x08A7BFD0(this[+0x34], this + 0x3C / + 0x40, 0x08CF6F38 / 0x08CF6FEC) run
// (0x08A5BB7C reporting a missing object at 0x08CB2570).
void func_08A9C340(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x30u);
    ctx.gpr[18] = self;  // s2
    const u32 flag = byte_at(rt, self + 0x7Cu);
    ctx.gpr[2] = flag;
    if (flag != 0u) return;
    put_byte(rt, self + 0x7Cu, 1u);
    const u32 list = call(rt, ctx, 0x08A920E0u, {0u});
    ctx.gpr[17] = list;  // s1
    if (list != 0u) {
        for (u32 node = list; node != 0u; node = word(rt, node + 0x10u)) {
            ctx.gpr[16] = node;
            call_indirect(rt, ctx, word(rt, node + 4u), {});
        }
        for (u32 node = list; node != 0u; node = word(rt, node + 0x10u)) {
            ctx.gpr[16] = node;
            call_indirect(rt, ctx, word(rt, node + 8u), {});
        }
    }
    const u32 local = frame.at(8u);
    ctx.gpr[16] = local;
    ctx.gpr[17] = self + 0x34u;
    call_keep(rt, ctx, 0x08AA0AA4u, {local});
    if (self + 0x34u != local) call_keep(rt, ctx, 0x08A5BAD8u, {self + 0x34u, word(rt, local)});
    call_keep(rt, ctx, 0x08A5B9ECu, {local});
    const u32 object = call(rt, ctx, 0x08A5BC20u, {0x38u, 4u});
    ctx.gpr[17] = frame.at(4u);
    ctx.gpr[16] = object;
    ctx.gpr[19] = self + 0x38u;  // s3
    call_keep(rt, ctx, 0x08A7E914u, {object, 0x08CF6F0Cu});
    put(rt, frame.at(0u), object);
    call_keep(rt, ctx, 0x08A76AB4u, {frame.at(4u), frame.at(0u)});
    if (self + 0x38u != frame.at(4u)) call_keep(rt, ctx, 0x08A5BAD8u, {self + 0x38u, word(rt, frame.at(4u))});
    call_keep(rt, ctx, 0x08A5B9ECu, {frame.at(4u)});
    u32 held = word(rt, self + 0x38u);
    ctx.gpr[16] = held;
    if (held == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
    call_keep(rt, ctx, 0x08A7BE18u, {held, word(rt, self + 0x34u)});
    held = word(rt, self + 0x34u);
    ctx.gpr[16] = held;
    if (held == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
    call_keep(rt, ctx, 0x08A7BFD0u, {held, self + 0x3Cu, 0x08CF6F38u});
    held = word(rt, self + 0x34u);
    ctx.gpr[16] = held;
    if (held == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CB2570u});
    call_keep(rt, ctx, 0x08A7BFD0u, {held, self + 0x40u, 0x08CF6FECu});
}

// 0x08ABEF54 (a0 = this): the vtable word 0x08CD6390 is stored, 0x08ABF6C0(this + 0x144) and 0x08B28DC4(this + 0x11C) run, then
// sixteen entries (0x10 bytes apart, downwards from this + 0x110 / +0x114 / +0x118): the +0x110 word is cleared whatever happens
// (the delay slot of the test of the +0x118 word); when the +0x118 word is set and the +0x114 array pointer is not null, the array
// of 0x20-byte elements (their count in the word before it) is destroyed from the last element down through the first virtual of
// each (the loop stops when the element is the array start), its storage (array - 4) is freed (0x08B61FC0), the +0x118 block too
// when set, and both words are cleared. The vtable word 0x08CD63D8 is stored last and stays in v0.

// 0x08AC9B54 (a0 = this, a1 = a pointer to a 64-bit key whose low word is read and whose high word is its sign, `sra 31`):
// 0x08AC99A4(key) answers a record whose +0x28 is the answer when found; otherwise 0x08AC99F4(key) answers a new one, 0x08B62344(0x14)
// a block that 0x08AD5590 sets up and 0x08AD5524(block, a1) fills: failing, the block is released through its virtual +4 and
// 0x08AC9240(record) runs, v0 = 0; succeeding, 0x08AD542C(block), 0x08ADC988(block[+0x10], &local) answers an entry that goes to
// 0x08AD36AC(record, entry), the block is appended to the record's list at +0x30 (0x08C3D12C when it is full), 0x08AC963C(record,
// block[+0x10], &local, entry, 0, 0) runs and the record gets this at +0x28 and itself at +0x2C. v0 = record + 0x28, or 0.
void func_08AC9B54(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x830u);
    ctx.gpr[19] = self;      // s3
    ctx.gpr[17] = argument;  // s1
    const u32 low = word(rt, argument);
    const u32 sign = static_cast<u32>(static_cast<s32>(low) >> 31);
    const u32 existing = call(rt, ctx, 0x08AC99A4u, {low, sign});
    ctx.gpr[16] = existing;  // s0
    ctx.gpr[2] = existing + 0x28u;
    if (existing != 0u) return;
    const u32 key = word(rt, argument);
    const u32 record = call(rt, ctx, 0x08AC99F4u, {key, static_cast<u32>(static_cast<s32>(key) >> 31)});
    ctx.gpr[16] = record;
    const u32 block = call(rt, ctx, 0x08B62344u, {0x14u});
    ctx.gpr[18] = block;     // s2
    call_keep(rt, ctx, 0x08AD5590u, {block});
    if (call(rt, ctx, 0x08AD5524u, {block, argument}) == 0u) {
        if (block != 0u) {
            ctx.gpr[3] = word(rt, block);
            call_indirect(rt, ctx, word(rt, word(rt, block) + 4u), {block});
        }
        call_keep(rt, ctx, 0x08AC9240u, {record});
        ctx.gpr[2] = 0u;
        return;
    }
    call_keep(rt, ctx, 0x08AD542Cu, {block});
    const u32 payload = word(rt, block + 0x10u);
    ctx.gpr[20] = payload;       // s4
    ctx.gpr[21] = frame.at(4u);  // s5
    const u32 entry = call(rt, ctx, 0x08ADC988u, {payload, frame.at(4u)});
    ctx.gpr[17] = entry;         // s1
    call_keep(rt, ctx, 0x08AD36ACu, {record, entry});
    put(rt, frame.at(0u), block);
    const u32 list = record + 0x30u;
    const u32 end = word(rt, list + 4u);
    if (end == word(rt, list + 8u)) {
        call_keep(rt, ctx, 0x08C3D12Cu, {list, end, frame.at(0u)});
    } else if (end == 0u) {
        put(rt, list + 4u, 4u);  // `beql $a1, $zero` runs `$a1 += 4` and skips the store
    } else {
        put(rt, end, block);
        put(rt, list + 4u, word(rt, list + 4u) + 4u);
    }
    call_keep(rt, ctx, 0x08AC963Cu, {record, payload, frame.at(4u), entry, 0u, 0u});
    if (record == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[2] = record + 0x28u;
    put(rt, record + 0x28u, self);
    put(rt, record + 0x2Cu, record);
}

// 0x08AD17C4 (a0 = this, a1 = four floats): the four floats fill the four rows of a local 4x4 matrix; each row goes to 0x08B4A6A0
// with f12 = its component * -2 (f20 holds -2 and is restored at the end); the diagonal terms get 1 added, the three top-right terms
// and the bottom row's first three are zeroed, and the result (also copied to this + 0x40) goes to 0x08B4E994(this, this + 0x40,
// this + 0x80 + this[+0x1C0] * 80). v0 holds this[+0x1C0] * 64 at that call (the callee does not set it, so it is also the answer).
void func_08AD17C4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 source = ctx.gpr[5];
    SavedRegisters saved(ctx);
    const float saved_f20 = ctx.fpr[20];
    GuestFrame frame(ctx, 0x60u);
    const u32 matrix = self + 0x40u;
    ctx.gpr[18] = self;    // s2
    ctx.gpr[17] = source;  // s1
    ctx.gpr[16] = matrix;  // s0
    ctx.fpr[20] = -2.0f;
    const float w = float_at(rt, source);
    const float x = float_at(rt, source + 4u);
    const float y = float_at(rt, source + 8u);
    const float z = float_at(rt, source + 0xCu);
    for (u32 row = 0u; row < 0x40u; row += 0x10u) {
        put_float(rt, frame.at(row), w);
        put_float(rt, frame.at(row + 4u), x);
        put_float(rt, frame.at(row + 8u), y);
        put_float(rt, frame.at(row + 0xCu), z);
    }
    const auto call_row = [&](u32 row, float component) {
        const GuestResult result = call_guest(rt, ctx, 0x08B4A6A0u, {frame.at(row)}, {psp_mul(component, ctx.fpr[20])});
        ctx.gpr[2] = result.v0;
        ctx.gpr[3] = result.v1;
        ctx.fpr[0] = result.f0;
    };
    call_row(0x00u, w);
    call_row(0x10u, float_at(rt, source + 4u));
    call_row(0x20u, float_at(rt, source + 8u));
    call_row(0x30u, float_at(rt, source + 0xCu));
    const float one = 1.0f;
    const float m00 = float_at(rt, frame.at(0x00u)) + one;
    const float m11 = float_at(rt, frame.at(0x14u)) + one;
    const float m22 = float_at(rt, frame.at(0x28u)) + one;
    put_float(rt, matrix + 0x00u, m00);
    put_float(rt, matrix + 0x04u, float_at(rt, frame.at(0x04u)));
    put_float(rt, matrix + 0x08u, float_at(rt, frame.at(0x08u)));
    put_float(rt, matrix + 0x10u, float_at(rt, frame.at(0x10u)));
    put_float(rt, matrix + 0x14u, m11);
    put_float(rt, matrix + 0x18u, float_at(rt, frame.at(0x18u)));
    put_float(rt, matrix + 0x20u, float_at(rt, frame.at(0x20u)));
    put_float(rt, matrix + 0x24u, float_at(rt, frame.at(0x24u)));
    put_float(rt, matrix + 0x28u, m22);
    put_float(rt, matrix + 0x2Cu, 0.0f);
    put_float(rt, matrix + 0x30u, float_at(rt, frame.at(0x30u)));
    put_float(rt, matrix + 0x34u, float_at(rt, frame.at(0x34u)));
    put_float(rt, frame.at(0x00u), m00);
    put_float(rt, frame.at(0x14u), m11);
    put_float(rt, frame.at(0x28u), m22);
    put_float(rt, frame.at(0x3Cu), one);
    put(rt, frame.at(0x0Cu), 0u);
    put(rt, frame.at(0x1Cu), 0u);
    put(rt, frame.at(0x2Cu), 0u);
    put_float(rt, matrix + 0x0Cu, 0.0f);
    put_float(rt, matrix + 0x1Cu, 0.0f);
    put_float(rt, matrix + 0x38u, float_at(rt, frame.at(0x38u)));
    put_float(rt, matrix + 0x3Cu, one);
    const u32 index = word(rt, self + 0x1C0u);
    ctx.gpr[2] = index << 6;
    call_keep(rt, ctx, 0x08B4E994u, {self, matrix, self + index * 80u + 0x80u});
    ctx.fpr[20] = saved_f20;
}

// 0x08AE1D6C (a0 = an argument the callback gets, a1 = a limit kept aside, a2 = an end position, a3 = a table, t0 = a count,
// t1 = a start position, t2 = a length, t3 = a flags pointer that may be null): when a1 | a2 is zero both become -1; while the
// position is below the length (signed; the loop is skipped when the length is not positive), the span is the rest (capped,
// `min`, by a2 - start when a2 is not negative), every table entry (0xC bytes apart, its word +4) at or after the start and before
// start + span is reported through 0x08AE0CD8(a0, index) while the table, the span and the count are all positive; the start then
// becomes start + span, except that when a2 is not negative and start + span >= a2 it becomes the a1 limit and the flags word (when
// there is one) gets bit 8 set. v0 = the final start.
void func_08AE1D6C(Runtime &rt, AllegrexContext &ctx) {
    const u32 callback_argument = ctx.gpr[4];
    u32 limit = ctx.gpr[5];
    u32 end = ctx.gpr[6];
    const u32 table = ctx.gpr[7];
    const s32 count = static_cast<s32>(ctx.gpr[8]);
    u32 start = ctx.gpr[9];
    const s32 length = static_cast<s32>(ctx.gpr[10]);
    const u32 flags = ctx.gpr[11];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x50u);
    ctx.gpr[30] = static_cast<u32>(count);  // fp
    ctx.gpr[22] = static_cast<u32>(length); // s6
    ctx.gpr[21] = end;                      // s5
    ctx.gpr[20] = start;                    // s4
    if ((limit | end) == 0u) {
        limit = 0xFFFFFFFFu;
        end = 0xFFFFFFFFu;
        ctx.gpr[21] = end;
    }
    ctx.gpr[23] = 0u;  // s7
    if (length > 0) {
        s32 position = 0;
        const bool end_is_set = static_cast<s32>(end) >= 0;
        const bool has_table = table != 0u;
        const bool has_count = count > 0;
        do {
            s32 span = length - position;
            if (end_is_set) {
                const s32 room = static_cast<s32>(end - start);
                if (room < span) span = room;
            }
            ctx.gpr[18] = static_cast<u32>(span);  // s2
            const u32 span_end = start + static_cast<u32>(span);
            ctx.gpr[19] = span_end;                // s3
            if (has_table && span > 0 && has_count) {
                for (s32 index = 0; index < count; ++index) {
                    const s32 value = swd(rt, table + 4u + static_cast<u32>(index) * 0xCu);
                    if (value >= static_cast<s32>(start) && value < static_cast<s32>(span_end)) {
                        ctx.gpr[17] = static_cast<u32>(index) + 1u;  // s1
                        ctx.gpr[16] = table + 4u + (static_cast<u32>(index) + 1u) * 0xCu;
                        call_keep(rt, ctx, 0x08AE0CD8u, {callback_argument, static_cast<u32>(index)});
                    }
                }
            }
            start = span_end;
            if (end_is_set && static_cast<s32>(span_end) >= static_cast<s32>(end)) {
                start = limit;
                if (flags != 0u) put(rt, flags, word(rt, flags) | 0x100u);
            }
            ctx.gpr[20] = start;
            position += span;
            ctx.gpr[23] = static_cast<u32>(position);
        } while (position < length);
    }
    ctx.gpr[2] = start;
}

// 0x08AF2D20: rotates the three words of the record 0x08AF296C() answers through the three frame words at sp + 4 / +8 / +0xC (the
// record's words are replaced by the older frame words: 0, 0 and 0 on the first round), then 0x08C3F8BC(first, second) runs and a
// nonzero first word is released (0x08B35254); v0 = the first word (loaded just before the test) when it is zero, else the
// release's answer.
void func_08AF2D20(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    const u32 object = call(rt, ctx, 0x08AF296Cu, {});
    ctx.gpr[6] = frame.at(0u);
    put(rt, frame.at(8u), 0u);
    put(rt, frame.at(4u), 0u);
    put(rt, frame.at(0xCu), 0u);
    const u32 first = word(rt, object);
    put(rt, frame.at(4u), first);
    put(rt, object, 0u);
    const u32 second = word(rt, object + 4u);
    const u32 older_second = word(rt, frame.at(8u));
    put(rt, frame.at(8u), second);
    put(rt, object + 4u, older_second);
    const u32 third = word(rt, object + 8u);
    const u32 older_third = word(rt, frame.at(0xCu));
    put(rt, frame.at(0xCu), third);
    put(rt, object + 8u, older_third);
    call_keep(rt, ctx, 0x08C3F8BCu, {word(rt, frame.at(4u)), word(rt, frame.at(8u))});
    const u32 pending = word(rt, frame.at(4u));
    ctx.gpr[2] = pending;
    if (pending != 0u) call_keep(rt, ctx, 0x08B35254u, {pending});
}

// 0x089CA118 (a0 = a destination object, a1 = a key, a2 = a mode, a3 = a holder): 0x089B395C(&local, 0x08CF6ED8) runs first; a mode
// of 0 or below only releases that local. Else 0x089C5850(&other, key) answers into `other`; the count is 1, or for mode 2 the
// 0x30 virtual of the object at holder[+4]; the source is `other`'s word (a missing one is reported at 0x08CA63A0), the holder's
// object [holder] (missing: 0x08CA6304) fills a third local through its +0xC virtual, 0x089C9F24(source, &local, &third, count) runs,
// the third local's string, the destination assignment (0x08A5BAD8 from the local, unless it is the local itself), `other` and the
// local are released.
void func_089CA118(Runtime &rt, AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    const s32 mode = static_cast<s32>(ctx.gpr[6]);
    const u32 holder = ctx.gpr[7];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x40u);
    const u32 local = frame.at(8u);
    const u32 other = frame.at(0xCu);
    const u32 third = frame.at(4u);
    ctx.gpr[20] = local;        // s4
    ctx.gpr[17] = key;          // s1
    ctx.gpr[21] = destination;  // s5
    ctx.gpr[16] = static_cast<u32>(mode);  // s0
    ctx.gpr[19] = holder;       // s3
    call_keep(rt, ctx, 0x089B395Cu, {local, 0x08CF6ED8u});
    ctx.gpr[23] = other;        // s7
    if (mode > 0) {
        call_keep(rt, ctx, 0x089C5850u, {other, key});
        u32 count = 1u;
        ctx.gpr[22] = count;    // s6
        if (mode == 2) {
            const u32 counted = word(rt, holder + 4u);
            ctx.gpr[16] = counted;
            if (counted == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
            ctx.gpr[3] = word(rt, counted);
            count = call_indirect(rt, ctx, word(rt, word(rt, counted) + 0x30u), {counted});
            ctx.gpr[22] = count;
        }
        const u32 source = word(rt, other);
        ctx.gpr[18] = source;   // s2
        ctx.gpr[17] = word(rt, holder);
        if (source == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA63A0u});
        const u32 object = word(rt, holder);
        ctx.gpr[17] = object;
        if (object == 0u) call_keep(rt, ctx, 0x08A5BB7Cu, {0x08CA6304u});
        ctx.gpr[16] = third;
        ctx.gpr[3] = word(rt, object);
        call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {third, object});
        call_keep(rt, ctx, 0x089C9F24u, {source, local, third, count});
        release_string(rt, ctx, 16u, word(rt, third), frame.at(0u));
        if (destination != local) call_keep(rt, ctx, 0x08A5BAD8u, {destination, word(rt, local)});
        call_keep(rt, ctx, 0x08A5B9ECu, {other});
    }
    call_keep(rt, ctx, 0x08A5B9ECu, {local});
}

} // namespace
} // namespace new_batch_main_111
} // namespace gt::game

#define NEW111(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_111::name, gt::ReturnKind::kind)
NEW111(0x0892CF9Cu, func_0892CF9C, Word);
NEW111(0x0892D1C0u, func_0892D1C0, Word);
NEW111(0x0893B160u, func_0893B160, Word);
NEW111(0x08959C9Cu, func_08959C9C, Word);
NEW111(0x0895FA54u, func_0895FA54, Word);
NEW111(0x089A4AE4u, func_089A4AE4, Word);
NEW111(0x089A54A0u, func_089A54A0, Word);
NEW111(0x089B8580u, func_089B8580, Word);
NEW111(0x089B9B48u, func_089B9B48, Word);
NEW111(0x089CA118u, func_089CA118, Word);
NEW111(0x089D0A5Cu, func_089D0A5C, Word);
NEW111(0x089DA994u, func_089DA994, Word);
NEW111(0x089DBCE8u, func_089DBCE8, Word);
NEW111(0x089F88A0u, func_089F88A0, Word);
NEW111(0x089FFE38u, func_089FFE38, Word);
NEW111(0x08A14778u, func_08A14778, Word);
NEW111(0x08A243FCu, func_08A243FC, Word);
NEW111(0x08A4037Cu, func_08A4037C, Word);
NEW111(0x08A58394u, func_08A58394, Word);
NEW111(0x08A9C340u, func_08A9C340, Word);
NEW111(0x08AC9B54u, func_08AC9B54, Word);
NEW111(0x08AD17C4u, func_08AD17C4, Word);
NEW111(0x08AE1D6Cu, func_08AE1D6C, Word);
NEW111(0x08AF2D20u, func_08AF2D20, Word);
