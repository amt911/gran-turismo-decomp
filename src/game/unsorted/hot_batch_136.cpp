// Hot batch 136: more of the code only the routes from the user's save reach (car physics of the save's cars), written
// by hand from `tools.disasm`; role words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game {
namespace hot_batch_136 {
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


// Releases the shared string whose data pointer sits at `slot` (libstdc++ copy-on-write: rep 0xC before the data, count
// at -4): unless it is the empty rep 0x08D74FD0, func_08B60804(&count, -1) steps the count and func_08B8542C(rep, alloc)
// frees it when the old count was not positive. `alloc` is the stack byte the original passes as the allocator.
void release_string(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t slot, std::uint32_t alloc) {
    const std::uint32_t text = word(rt, slot);
    const std::uint32_t rep = text - 0xCu;
    if (rep == 0x08D74FD0u) return;
    if (static_cast<std::int32_t>(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}).v0) > 0) return;
    call_set(rt, ctx, 0x08B8542Cu, {rep, alloc});
}

} // namespace

// The force of wheel a1's suspension at travel f12 (0 when the car type's table at 0x08CF3440 + 24 * type, type the
// byte +0x140E of the state *(a0 + 0xC), says the axle has none). Per axle (a1 >> 1) the state holds a 116-byte block
// at +0x360: below its rest point +0 a bump term -(d * d * k), k at +0x3C; the spring func_08958410(a0, a1, f12) times
// +0x14 subtracted; and, while the travel past +0x10 plus the spring stays negative, a damper half (table +8, scaled by
// x / -(table +4) when x is beyond -(table +4), times the axle's +0x13D0).
void func_089584F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t state = word(rt, ctx.gpr[4] + 0xCu);
    const std::uint32_t type = rt.memory().load8(state + 0x140Eu);
    const std::uint32_t table = 0x08CF3440u + (type << 5u) - (type << 3u);
    const auto axle = static_cast<std::uint32_t>(static_cast<std::int32_t>(ctx.gpr[5]) >> 1);
    const std::uint32_t block = state + ((((axle << 5u) - (axle << 2u)) + axle) << 2u) + 0x360u;
    const float travel = ctx.fpr[12];
    if (rt.memory().load8(table) == 0u) {
        ctx.fpr[0] = 0.0f;
        return;
    }
    float bump = 0.0f;
    const float below = travel - float_at(rt, block);
    if (below < 0.0f) bump = -psp_mul(psp_mul(below, below), float_at(rt, block + 0x3Cu));
    const float offset = travel - float_at(rt, block + 0x10u);
    const float spring = call_set(rt, ctx, 0x08958410u, {}, {travel}).f0;
    const float x = offset + spring;
    float damper = 0.0f;
    if (x < 0.0f) {
        const float limit = -float_at(rt, table + 4u);
        float rate = float_at(rt, table + 8u);
        const float scaled = psp_mul(x, rate);
        if (limit < x) rate = scaled / limit;
        damper = psp_mul(psp_mul(rate, float_at(rt, state + (axle << 2u) + 0x13D0u)), 0.5f);
    }
    ctx.fpr[0] = bump + (damper - psp_mul(spring, float_at(rt, block + 0x14u)));
}

// Three parameters of entry a1 of the byte tables at a0 + 0x1C8..0x1E4 (a car's settings): *a3 = 5 * byte +0x1C8
// thousandths (10 when 0), times the percentage byte +0x1E4 when it is neither 0 nor 100; *t0 = that * byte +0x1CC * 0.05,
// less half the signed byte +0x1E2 in units of 0.0254 when it is not 0 (bounded so the sum keeps 0.0254, and the result
// at least 0.0127); *a2, returned, = byte +0x1CA * 0.0254 * 0.05 plus *t0 before that adjustment.
void func_089514A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t entry = ctx.gpr[5] + ctx.gpr[4];
    const std::uint32_t total_out = ctx.gpr[6], base_out = ctx.gpr[7], part_out = ctx.gpr[8];
    const float inch = std::bit_cast<float>(0x3CD013A9u), twentieth = std::bit_cast<float>(0x3D4CCCCDu);
    const std::uint32_t steps = rt.memory().load8(entry + 0x1C8u) * 5u;
    const float units = steps == 0u ? 10.0f : static_cast<float>(static_cast<std::int32_t>(steps));
    const float base = psp_mul(units, std::bit_cast<float>(0x3A83126Fu));
    put_float(rt, base_out, base);
    const float width = static_cast<float>(rt.memory().load8(entry + 0x1CCu));
    const float extra = psp_mul(psp_mul(static_cast<float>(rt.memory().load8(entry + 0x1CAu)), inch), twentieth);
    const float part = psp_mul(psp_mul(base, width), twentieth);
    put_float(rt, part_out, part);
    put_float(rt, total_out, extra + part);
    const auto shift = static_cast<std::int8_t>(rt.memory().load8(entry + 0x1E2u));
    if (shift != 0) {
        float half = psp_mul(psp_mul(static_cast<float>(shift), inch), 0.5f);
        if (extra + half < inch) half = inch - extra;
        const float floor = std::bit_cast<float>(0x3C5013A9u);
        const float adjusted = float_at(rt, part_out) - half;
        put_float(rt, part_out, adjusted < floor ? floor : adjusted);
    }
    const std::uint32_t percent = rt.memory().load8(entry + 0x1E4u);
    if (percent != 0u && percent != 100u)
        put_float(rt, base_out, psp_mul(float_at(rt, base_out), static_cast<float>(static_cast<std::int32_t>(percent)) / 100.0f));
    ctx.fpr[0] = float_at(rt, total_out);
}

// Fills the 16-bit table at a2 (entries 1..n, n = byte +0x155) from a car's settings at a0: wheel data from
// func_089515C4(self, &b0, &b1, &b2, &b3, &w0, &w1) and func_089514A4(self, b2, &radius, &x, &y) (a3 non-zero first
// clears byte +0x156). A gear table row (0x08CDE070 + 6 * row, row 0 below 5 gears, n - 5 below 8, else 2) gives three
// ratios interpolated by (a1 - 150) / 250; the base is 100 * byte +0xBD rpm in rad/s times the radius over a1 km/h in
// m/s, over lh +0x150 thousandths (times lh +0x152 thousandths when byte +0x154 is 1). Entry n, n-1 .. 1 get 1000 * the
// running value (the first one also times the first ratio), each step multiplying it by the second and that by the
// third; entry 0 = (3 * entry 1 + entry 2) / 4, rounded toward zero.
void func_089494D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t speed = ctx.gpr[5];
    const std::uint32_t out = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    if ((ctx.gpr[7] & 0xFFu) != 0u) rt.memory().store8(self + 0x156u, 0u);
    call_set(rt, ctx, 0x089515C4u, {self, frame.at(0u), frame.at(1u), frame.at(2u), frame.at(3u), frame.at(0x10u), frame.at(0x14u)});
    call_set(rt, ctx, 0x089514A4u, {self, rt.memory().load8(frame.at(2u)), frame.at(4u), frame.at(8u), frame.at(0xCu)});
    const auto gears = static_cast<std::int32_t>(rt.memory().load8(self + 0x155u));
    const std::uint32_t row = gears < 5 ? 0u : (gears < 8 ? static_cast<std::uint32_t>(gears - 5) : 2u);
    const std::uint32_t ratios = 0x08CDE070u + row * 6u;
    const auto byte_of = [&](std::uint32_t at) { return static_cast<float>(static_cast<std::int32_t>(rt.memory().load8(at))); };
    const auto half_of = [&](std::uint32_t at) {
        return static_cast<float>(static_cast<std::int32_t>(static_cast<std::int16_t>(rt.memory().load16(at))));
    };
    const float thousandth = std::bit_cast<float>(0x3A83126Fu);
    const float meters = psp_mul(static_cast<float>(static_cast<std::int32_t>(speed)), std::bit_cast<float>(0x3E8E38E4u));
    const float blend = static_cast<float>(static_cast<std::int32_t>(speed - 0x96u)) / 250.0f;
    float divisor = psp_mul(half_of(self + 0x150u), thousandth);
    if (rt.memory().load8(self + 0x154u) == 1u) divisor = psp_mul(divisor, psp_mul(half_of(self + 0x152u), thousandth));
    const float r1 = 200.0f / byte_of(ratios + 1u), r3 = 200.0f / byte_of(ratios + 3u), r5 = 200.0f / byte_of(ratios + 5u);
    const float r4 = 200.0f / byte_of(ratios + 4u), r0 = 200.0f / byte_of(ratios + 0u), r2 = 200.0f / byte_of(ratios + 2u);
    const std::uint32_t rpm = rt.memory().load8(self + 0xBDu) * 100u;
    float base = psp_mul(static_cast<float>(static_cast<std::int32_t>(rpm)), std::bit_cast<float>(0x3DD67750u));
    base = psp_mul(base, float_at(rt, frame.at(4u))) / meters;
    const float first = r5 + psp_mul(blend, r4 - r5);
    float second = r1 + psp_mul(blend, r0 - r1);
    const float third = r3 + psp_mul(blend, r2 - r3);
    float value = base / divisor;
    std::uint32_t slot = out + static_cast<std::uint32_t>(gears) * 2u;
    for (std::int32_t i = 0; i < gears; ++i) {
        if (gears - i == 1) value = psp_mul(value, first);
        const std::uint32_t whole = ctx.fpu_float_to_word(psp_mul(value, 1000.0f), 1u);
        value = psp_mul(value, second);
        second = psp_mul(second, third);
        rt.memory().store16(slot, static_cast<std::uint16_t>(whole));
        slot -= 2u;
    }
    const std::int32_t one = static_cast<std::int16_t>(rt.memory().load16(out + 2u));
    const std::int32_t two = static_cast<std::int16_t>(rt.memory().load16(out + 4u));
    const std::int32_t sum = one * 3 + two;
    const std::int32_t average = (sum < 0 ? sum + 3 : sum) >> 2;
    rt.memory().store16(out, static_cast<std::uint16_t>(average));
    ctx.gpr[2] = static_cast<std::uint32_t>(average);
}

// Classifies a car's drivetrain from bytes +0x128 / +0x129 into *a1 (returned), *a2, *a3 and *t0, with two factors:
// *t1 (percent byte +0x12A, adjusted per class) and *t2 (g, 9.8 times ten times byte +0x12B, or a default). Class 2
// splits by +0x129; its case 5 calls func_089475F0 with a0 still holding that case's address (the jr's target
// register), as the original does.
void func_089515C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t kind = ctx.gpr[5], flag = ctx.gpr[6], driven = ctx.gpr[7], mode = ctx.gpr[8];
    const std::uint32_t factor = ctx.gpr[9], weight = ctx.gpr[10];
    const float g = std::bit_cast<float>(0x411CCCCDu);
    auto& memory = rt.memory();
    const auto set8 = [&](std::uint32_t at, std::uint32_t v) { memory.store8(at, static_cast<std::uint8_t>(v)); };
    const auto weight_from = [&](std::uint32_t fallback) {
        std::uint32_t value = memory.load8(self + 0x12Bu);
        if (value == 0u) value = fallback;
        put_float(rt, weight, psp_mul(static_cast<float>(static_cast<std::int32_t>(value * 10u)), g));
    };
    set8(kind, 1u);
    set8(flag, 0u);
    set8(driven, 0u);
    set8(mode, 0u);
    put(rt, factor, 0u);
    put(rt, weight, 0u);
    ctx.gpr[5] = self;
    const std::uint32_t outer = memory.load8(self + 0x128u);
    if (outer == 1u) {
        set8(kind, 1u);
        set8(driven, 0u);
    } else if (outer == 2u) {
        put_float(rt, factor, psp_mul(static_cast<float>(memory.load8(self + 0x12Au)), std::bit_cast<float>(0x3C23D70Au)));
        set8(driven, 1u);
        switch (memory.load8(self + 0x129u)) {
        case 0u: set8(kind, 2u); set8(driven, 0u); set8(mode, 2u); break;
        case 2u:
            set8(kind, 4u);
            set8(mode, 1u);
            if (!(memory.load8(self + 0x12Au) < 0x32u)) put_float(rt, factor, 1.0f);
            weight_from(8u);
            break;
        case 3u: {
            set8(kind, 5u);
            set8(mode, 2u);
            put_float(rt, factor, psp_mul(float_at(rt, factor), std::bit_cast<float>(0x3EB851EBu)));
            const std::uint32_t six = memory.load8(self + 0x158u) == 6u ? 1u : 0u;
            set8(flag, six);
            const float share = static_cast<float>(memory.load8(0x08CDE0E4u + six)) / 100.0f;
            const float mass = psp_mul(static_cast<float>(static_cast<std::int32_t>(memory.load8(0x08CDE0E0u + six) * 10u)), g);
            put_float(rt, factor, psp_mul(share, float_at(rt, factor)));
            put_float(rt, weight, mass);
            break;
        }
        case 4u: {
            set8(kind, 6u);
            set8(mode, 1u);
            const float angle = float_at(rt, factor);
            if (!(angle == 0.0f))
                put_float(rt, factor, 1.0f / psp_mul(psp_mul(angle, 100.0f), std::bit_cast<float>(0x3C8EFA35u)));
            weight_from(15u);
            break;
        }
        case 5u:
            set8(kind, 7u);
            set8(mode, 2u);
            call_set(rt, ctx, 0x089475F0u, {0x08951808u, self});
            break;
        case 6u: set8(kind, 9u); set8(driven, 0u); set8(mode, 1u); break;
        case 7u: set8(kind, 8u); set8(mode, 2u); break;
        case 8u:
            set8(kind, 10u);
            set8(mode, 2u);
            put_float(rt, weight, psp_mul(static_cast<float>(memory.load8(self + 0x12Bu)), g));
            break;
        case 9u: set8(kind, 11u); set8(driven, 0u); set8(mode, 1u); weight_from(15u); break;
        default: set8(kind, 3u); set8(mode, 2u); break;  // 1 and 10+
        }
    } else if (outer < 5u) {  // 0, 3, 4
        set8(kind, 0u);
        set8(driven, 1u);
    }
    ctx.gpr[2] = memory.load8(kind);
}

// The state byte +0x778 of the object *(a0 + 8) (rows of 28 bytes at 0x08CF334C, set up by func_08954AEC) steps by its
// float +0x6C4 against the thresholds 0x08CF3384 / row +0x70 / +0x3C / 0x08CF33A4 / 0x08CF33C0 / 0x08CF33DC, some steps
// only while its +0x538 is above 0.25 or exactly 0. A change marks the four channels in +0x779; each marked channel
// (+0x77C, +0x780, +0x784, +0x788, rows +8 / +0xC / +0x14 / +0x18, rates 0x08CF33F4 / +0x3F8 / +0x3FC twice) moves
// toward the new row by rate * func_0895549C(a0) per call and drops its mark on arrival.
void func_08954B78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t object = word(rt, self + 8u);
    const std::uint32_t state_at = object + 0x778u, mask_at = object + 0x779u;
    constexpr std::uint32_t kRows = 0x08CF334Cu;
    const float pedal = float_at(rt, object + 0x538u);
    const float value = float_at(rt, object + 0x6C4u);
    const bool pressed = 0.25f < pedal;
    const bool released = pedal == 0.0f;
    const std::uint32_t old = rt.memory().load8(state_at);
    const auto set_state = [&](std::uint32_t v) { rt.memory().store8(state_at, static_cast<std::uint8_t>(v)); };
    switch (old) {
    case 0u: if (float_at(rt, 0x08CF3384u) <= value) set_state(2u); break;
    case 2u:
        if (float_at(rt, kRows + 0x70u) <= value) set_state(4u);
        else if (value < float_at(rt, kRows + 0x3Cu)) set_state(0u);
        else if (pressed) set_state(3u);
        break;
    case 3u: if (value < float_at(rt, 0x08CF33A4u) || released) set_state(2u); break;
    case 4u:
        if (value < float_at(rt, 0x08CF33C0u)) set_state(2u);
        else if (pressed) set_state(5u);
        break;
    case 5u: if (value < float_at(rt, 0x08CF33DCu) || released) set_state(4u); break;
    default: break;
    }
    const std::uint32_t state = rt.memory().load8(state_at);
    if (state != old) rt.memory().store8(mask_at, 0x0Fu);
    if (rt.memory().load8(mask_at) == 0u) return;
    const std::uint32_t row = (state << 5u) - (state << 2u) + kRows;
    const float dt = call_set(rt, ctx, 0x0895549Cu, {self}).f0;
    static constexpr std::uint32_t kTarget[4] = {8u, 0xCu, 0x14u, 0x18u};
    static constexpr std::uint32_t kRate[4] = {0xA8u, 0xACu, 0xB0u, 0xB0u};
    for (std::uint32_t channel = 0u; channel < 4u; ++channel) {
        const std::uint32_t mask = rt.memory().load8(mask_at);
        const std::uint32_t bit = (1u << channel) & 0xFFu;
        if ((mask & bit) == 0u) continue;
        const float target = float_at(rt, row + kTarget[channel]);
        const float step = psp_mul(float_at(rt, kRows + kRate[channel]), dt);
        const std::uint32_t slot = object + 0x77Cu + channel * 4u;
        const float current = float_at(rt, slot);
        bool arrived = false;
        if (current < target) {
            const float next = current + step;
            put_float(rt, slot, next);
            arrived = target <= next;
        } else if (target < current) {
            const float next = current - step;
            put_float(rt, slot, next);
            arrived = next <= target;
        } else {
            rt.memory().store8(mask_at, static_cast<std::uint8_t>(bit ^ mask));
            continue;
        }
        if (arrived) {
            put_float(rt, slot, target);
            rt.memory().store8(mask_at, static_cast<std::uint8_t>(bit ^ rt.memory().load8(mask_at)));
        }
    }
}

// Reads a versioned record from a stream (reached only from the save routes): version = func_08A71C1C(stream) (stored at +0x44; stream = the object at +4, which reports
// 0x08CB30B0 through func_08A5BB7C when null, each time it is fetched).
// - Below 13: func_08AA9D84(self) (below 12; the count is then +0x44) or func_08AAA2C0(self) and a count read; that many
//   pairs: a key string (func_08C33360(&key, stream)) and a number; a key the map at +8 lacks (func_08C316C8 finds its
//   end +0xC) gets a new 12-byte node (vtable 0x08CD5368, +8 the number), stored at +0x14 of the entry the hinted
//   insert func_08C335FC makes, or of the lower_bound entry (func_08C31D6C) when its key is not greater (libstdc++
//   compare: memcmp of the shorter length, then the length difference).
// - 13: func_08AAA2C0(self) and a number read and dropped.
// - 14 and up: func_08AAA2C0(self); a position func_08C31354(stream) to seek to (virtual 0x114) after saving the current
//   one (virtual 0x118); n strings read into the vector at +0x38, resized first (func_08C311E0 erases the tail,
//   func_08C30F28 appends empties); then the saved position is restored.
// Then func_08AA9B68(self, a1) and func_08AA97D4(self).
void func_08AAB0B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    const auto stream = [&] {
        const std::uint32_t object = word(rt, self + 4u);
        ctx.gpr[4] = 0x08CB30B0u;
        if (object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB30B0u});
        return object;
    };
    const auto number = [&] { return call_set(rt, ctx, 0x08A71C1Cu, {stream()}).v0; };
    const auto virtual_call = [&](std::uint32_t object, std::uint32_t slot, std::initializer_list<std::uint32_t> args) {
        ctx.gpr[3] = word(rt, object);
        ctx.gpr[2] = word(rt, ctx.gpr[3] + slot);
        return call_set(rt, ctx, ctx.gpr[2], args).v0;
    };
    const std::uint32_t version = number();
    put(rt, self + 0x44u, version);
    if (version < 0xDu) {
        std::uint32_t count;
        if (version < 0xCu) {
            call_set(rt, ctx, 0x08AA9D84u, {self});
            count = word(rt, self + 0x44u);
        } else {
            call_set(rt, ctx, 0x08AAA2C0u, {self});
            count = number();
        }
        const std::uint32_t map = self + 8u, end = self + 0xCu, key = frame.at(0xCu);
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint32_t source = stream();
            call_set(rt, ctx, 0x08C33360u, {key, source});
            const std::uint32_t value = number();
            const std::uint32_t found = call_set(rt, ctx, 0x08C316C8u, {map, key}).v0;
            if (found == end) {
                const std::uint32_t node = call_set(rt, ctx, 0x08B62344u, {0xCu}).v0;
                put(rt, frame.at(0x20u), node);
                put(rt, node + 8u, value);
                put(rt, node + 4u, 0u);
                put(rt, node, 0x08CD5368u);
                const std::uint32_t position = call_set(rt, ctx, 0x08C31D6Cu, {map, key}).v0;
                std::uint32_t entry = position;
                bool insert = position == found;
                if (!insert) {
                    const std::uint32_t mine = word(rt, key);
                    const std::uint32_t theirs = word(rt, position + 0x10u);
                    const std::uint32_t my_length = word(rt, mine - 0xCu), their_length = word(rt, theirs - 0xCu);
                    put(rt, frame.at(0x10u), my_length);
                    put(rt, frame.at(0x14u), their_length);
                    const std::uint32_t shorter = their_length < my_length ? their_length : my_length;
                    const std::uint32_t compared = call_set(rt, ctx, 0x08B36170u, {mine, theirs, shorter}).v0;
                    const std::uint32_t order = compared != 0u ? compared : my_length - their_length;
                    insert = static_cast<std::int32_t>(order) < 0;
                }
                if (insert) {
                    call_set(rt, ctx, 0x08B88F44u, {frame.at(0x18u), key});
                    put(rt, frame.at(0x1Cu), 0u);
                    entry = call_set(rt, ctx, 0x08C335FCu, {map, position, frame.at(0x18u)}).v0;
                    release_string(rt, ctx, frame.at(0x18u), frame.at(3u));
                }
                put(rt, entry + 0x14u, word(rt, frame.at(0x20u)));
            }
            release_string(rt, ctx, key, frame.at(2u));
        }
    } else if (version < 0xEu) {
        call_set(rt, ctx, 0x08AAA2C0u, {self});
        number();
    } else {
        call_set(rt, ctx, 0x08AAA2C0u, {self});
        const std::uint32_t target = call_set(rt, ctx, 0x08C31354u, {stream()}).v0;
        std::uint32_t object = stream();
        const std::uint32_t saved = virtual_call(object, 0x118u, {object});
        object = stream();
        virtual_call(object, 0x114u, {object, target});
        const std::uint32_t count = number();
        const std::uint32_t vector = self + 0x38u;
        const std::uint32_t begin = word(rt, vector), size = (word(rt, vector + 4u) - begin) >> 2u;
        put(rt, frame.at(4u), 0x08D74FDCu);
        if (count < size)
            call_set(rt, ctx, 0x08C311E0u, {vector, begin + count * 4u, word(rt, self + 0x3Cu)});
        else
            call_set(rt, ctx, 0x08C30F28u, {vector, word(rt, self + 0x3Cu), count - size, frame.at(4u)});
        release_string(rt, ctx, frame.at(4u), frame.at(1u));
        for (std::uint32_t i = 0; i < count; ++i) {
            const std::uint32_t slot = word(rt, self + 0x38u) + i * 4u;
            const std::uint32_t source = stream();
            call_set(rt, ctx, 0x08C33360u, {frame.at(8u), source});
            call_set(rt, ctx, 0x08B855E0u, {slot, frame.at(8u)});
            release_string(rt, ctx, frame.at(8u), frame.at(0u));
        }
        object = stream();
        virtual_call(object, 0x114u, {object, saved});
    }
    call_set(rt, ctx, 0x08AA9B68u, {self, out});
    call_set(rt, ctx, 0x08AA97D4u, {self});
}

} // namespace hot_batch_136
} // namespace gt::game

#define HOT136(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_136::name, gt::ReturnKind::kind)
HOT136(0x089584F4u, func_089584F4, Float);
HOT136(0x089514A4u, func_089514A4, Float);
HOT136(0x089494D4u, func_089494D4, Word);
HOT136(0x089515C4u, func_089515C4, Word);
HOT136(0x08954B78u, func_08954B78, Void);
HOT136(0x08AAB0B4u, func_08AAB0B4, Word);
