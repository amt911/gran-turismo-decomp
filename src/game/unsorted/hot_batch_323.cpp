// Hot batch 323: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
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
namespace hot_batch_323 {
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

// The `bne v0, v1; addiu v0, v0, -1` loop the compiler emitted as a delay: it ends with v0 = -2 (and v1 = -1 as it was set up).
void delay_loop(psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0xFFFFFFFEu;
    ctx.gpr[3] = 0xFFFFFFFFu;
}

// 0x08B7634C: walks the linked records of a0 / a1 (a1 = the first node: relative links, the word +4 of a node the distance back to its owner entry, a word 0 at
// +0 ending the list): for each owner (0x08B75F68 gives its type code, 0x08B75EDC(code, a0) a selector; the type is kept in the bits 3..10 of the word
// +0x10 of a0, set when that field is all ones (0x7F8) and otherwise flagged with bit 4 when it differs) 0x08B75DA0(code, selector, node + 8, sp)
// reads a value; when it has bits within the width 0x08B75C8C gives (1, 2, 3 bytes, else all 32) it is counted and replaces the word +0 of a0 when smaller.
// v0 = how many values were counted.
void count_values_in_nodes(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4];
    u32 node = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{23u, 0x2Cu}, {19u, 0x1Cu}, {16u, 0x10u}, {31u, 0x30u}, {22u, 0x28u}, {21u, 0x24u}, {20u, 0x20u}, {18u, 0x18u}, {17u, 0x14u}});
    u32 count = 0u;
    ctx.gpr[23] = 0u;
    ctx.gpr[19] = target;
    ctx.gpr[2] = word(rt, node);
    ctx.gpr[16] = node;
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 previous_owner = 0u;
    u32 type_code = 0u;
    u32 selector = 0u;
    ctx.gpr[21] = 0u;
    ctx.gpr[20] = 0u;
    ctx.gpr[22] = 0u;
    for (;;) {
        const u32 back = word(rt, node + 4u);
        ctx.gpr[4] = back;
        if (back != 0u) {
            const u32 owner = node + 4u - back;
            ctx.gpr[18] = owner;
            u32 code = type_code & 0xFFu;
            ctx.gpr[17] = code;
            if (previous_owner != owner) {
                type_code = call_set(rt, ctx, 0x08B75F68u, {owner});
                code = type_code & 0xFFu;
                ctx.gpr[17] = code;
                ctx.gpr[20] = type_code;
                selector = call_set(rt, ctx, 0x08B75EDCu, {code, target});
                ctx.gpr[22] = selector;
                const u32 flags = word(rt, target + 0x10u);
                previous_owner = owner;
                ctx.gpr[21] = owner;
                if ((flags & 0x7F8u) == 0x7F8u) {
                    put(rt, target + 0x10u, (flags & ~0x7F8u) | ((code << 3) & 0x7F8u));
                } else if (type_code != ((flags >> 3) & 0xFFu)) {
                    put(rt, target + 0x10u, flags | 4u);
                }
            }
            call_set(rt, ctx, 0x08B75DA0u, {code, selector, node + 8u, frame.at(0u)});
            const u32 width = call_set(rt, ctx, 0x08B75C8Cu, {code});
            ctx.gpr[3] = width;
            u32 mask = 0xFFFFFFFFu;
            if (width < 4u) mask = (1u << ((width << 3) & 31u)) - 1u;
            ctx.gpr[5] = mask;
            const u32 value = word(rt, frame.at(0u));
            ctx.gpr[4] = value;
            ctx.gpr[2] = mask & value;
            if ((mask & value) != 0u) {
                ++count;
                ctx.gpr[23] = count;
                ctx.gpr[2] = word(rt, target);
                if (value < word(rt, target)) put(rt, target, value);
            }
        }
        ctx.gpr[2] = word(rt, node);
        const u32 following = word(rt, node) + node;
        ctx.gpr[2] = following;
        ctx.gpr[3] = word(rt, following + 4u);
        node = following + 4u;
        ctx.gpr[16] = node;
        if (ctx.gpr[3] == 0u) break;
    }
    ctx.gpr[2] = count;
}

// 0x08ABADE0 (a0 = the owner, a1 = the request object, a2 = a flag byte, f12 / f13 = two floats): by the kind word at a1 + 4 (0: the format 0x08CB434C, 1:
// 0x08CB4358, 2: 0x08CB4368 and the word +0x10 as the extra argument; others and a null a1 give 0) a result record is made (0x08ABAD70), the target
// 0x08AB9EF0(format) is asked for (a failure drops the record with 0x08ABA914 and gives 0), a text is formatted (0x08B3767C into sp, 0x400 bytes, with
// 0x08AD354C and the word +8) and the target's virtual +0x28(target, text, flag, extra) with the floats answers: its result is stored in the record (a0
// at +0, a1 at +0xC, the answer at +8) and v0 = the record, else the record is dropped and v0 = 0.
void make_request_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4];
    const u32 request = ctx.gpr[5];
    const u32 flag = ctx.gpr[6] & 0xFFu;
    const float first = ctx.fpr[12];
    const float second = ctx.fpr[13];
    GuestFrame frame(ctx, 0x430u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x410u}, {19u, 0x40Cu}, {16u, 0x400u}, {31u, 0x418u}, {21u, 0x414u}, {18u, 0x408u}, {17u, 0x404u}, {53u, 0x424u}, {52u, 0x420u}});
    ctx.gpr[20] = owner;
    ctx.gpr[19] = flag;
    ctx.gpr[16] = request;
    ctx.fpr[21] = second;
    ctx.fpr[20] = first;
    if (request == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 kind = word(rt, request + 4u);
    u32 format;
    u32 extra_from_request;
    if (kind == 1u) {
        format = 0x08CB4358u;
        extra_from_request = 0u;
    } else if (kind == 0u) {
        format = 0x08CB434Cu;
        extra_from_request = 0u;
    } else if (kind == 2u) {
        format = 0x08CB4368u;
        extra_from_request = 1u;
    } else {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[17] = format;
    ctx.gpr[21] = extra_from_request;
    const u32 record = call_set(rt, ctx, 0x08ABAD70u, {});
    ctx.gpr[18] = record;
    if (record == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 target = call_set(rt, ctx, 0x08AB9EF0u, {format});
    ctx.gpr[17] = target;
    if (target != 0u) {
        const u32 value = call_set(rt, ctx, 0x08AD354Cu, {});
        call_set(rt, ctx, 0x08B3767Cu, {frame.at(0u), 0x400u, 0x08CB435Cu, value, word(rt, request + 8u)});
        const u32 extra = extra_from_request != 0u ? word(rt, request + 0x10u) : 0u;
        ctx.gpr[7] = extra;
        ctx.fpr[12] = first;
        ctx.fpr[13] = second;
        const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, target) + 0x28u), {target, frame.at(0u), flag, extra});
        ctx.gpr[3] = answer;
        if (answer != 0u) {
            ctx.gpr[2] = record;
            put(rt, record, owner);
            put(rt, record + 0xCu, request);
            put(rt, record + 8u, answer);
            return;
        }
    }
    call_set(rt, ctx, 0x08ABA914u, {record});
    ctx.gpr[2] = 0u;
}

// 0x08A15DA8: appends the 4-float box a1 to the vector of 16-byte boxes at a0 + 0x3C (begin +0, end +4, capacity +8), merging it with the last one when
// there is one: x and y become the larger of the two, z and w the smaller (a pair that overlaps, z <= x or w <= y, keeps (x, y) in both slots), the result
// is built at sp and stored in place (or by 0x08C16984(vector, end, sp) when the vector is full); a null end stays null. v0 = the call's result when
// full, else the capacity word.
void append_merged_box(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 box = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {});
    const u32 vector = self + 0x3Cu;
    const u32 end = word(rt, vector + 4u);
    ctx.gpr[7] = vector;
    ctx.gpr[3] = end;
    ctx.gpr[6] = box;
    float x;
    float y;
    float z;
    float w;
    if (((end - word(rt, self + 0x3Cu)) >> 4) == 0u) {
        x = float_at(rt, box);
        y = float_at(rt, box + 4u);
        z = float_at(rt, box + 8u);
        w = float_at(rt, box + 0xCu);
        ctx.gpr[2] = word(rt, vector + 8u);
        ctx.gpr[4] = vector;
        if (end == word(rt, vector + 8u)) {
            call_set(rt, ctx, 0x08C16984u, {vector, end, box});
            return;
        }
        if (end == 0u) {
            put(rt, vector + 4u, end + 0x10u);
        } else {
            put_float(rt, end + 0xCu, w);
            put_float(rt, end, x);
            put_float(rt, end + 4u, y);
            put_float(rt, end + 8u, z);
            put(rt, vector + 4u, word(rt, vector + 4u) + 0x10u);
        }
        return;
    }
    const u32 last = word(rt, self + 0x40u) - 0x10u;
    const float box_x = float_at(rt, box);
    const float box_y = float_at(rt, box + 4u);
    const float box_z = float_at(rt, box + 8u);
    const float box_w = float_at(rt, box + 0xCu);
    x = float_at(rt, last);
    if (x < box_x) x = box_x;
    y = float_at(rt, last + 4u);
    if (y < box_y) y = box_y;
    z = float_at(rt, last + 8u);
    if (!(z < box_z)) z = box_z;
    w = float_at(rt, last + 0xCu);
    if (!(w < box_w)) w = box_w;
    if (z <= x || w <= y) {
        z = x;
        w = y;
    }
    ctx.gpr[2] = word(rt, vector + 8u);
    put_float(rt, frame.at(0u), x);
    put_float(rt, frame.at(4u), y);
    put_float(rt, frame.at(8u), z);
    put_float(rt, frame.at(0xCu), w);
    if (end == word(rt, vector + 8u)) {
        ctx.gpr[4] = vector;
        call_set(rt, ctx, 0x08C16984u, {vector, word(rt, self + 0x40u), frame.at(0u)});
        return;
    }
    if (end == 0u) {
        put(rt, vector + 4u, end + 0x10u);
    } else {
        put_float(rt, end + 0xCu, w);
        put_float(rt, end, x);
        put_float(rt, end + 4u, y);
        put_float(rt, end + 8u, z);
        put(rt, vector + 4u, word(rt, vector + 4u) + 0x10u);
    }
}

// 0x089F9860: puts a default 0x40-byte record at index a1 of the vector at a0 + 0xA8 (begin +0xA8, end +0xAC, capacity +0xB0): an index within the count of the
// virtual +0x1C8 is handed to 0x08C0D458(vector, element, element); otherwise a record (0x089D76A8 at sp, vtable 0x08CCFC90, two sub-objects 0x089FA09C at sp + 0x20 /
// sp + 0x30) is appended: built in place (copy of the eight words, 0x089FA280 for the sub-objects) or through 0x08C0D100(vector, end, sp) when full; the temporaries
// are destroyed (0x089F9B74 twice, 0x089D6684). v0 = the last call's.
void insert_default_record(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const s32 index = static_cast<s32>(ctx.gpr[5]);
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x44u}, {16u, 0x40u}, {31u, 0x58u}, {21u, 0x54u}, {20u, 0x50u}, {19u, 0x4Cu}, {18u, 0x48u}});
    ctx.gpr[17] = self;
    ctx.gpr[16] = static_cast<u32>(index);
    const u32 vector = self + 0xA8u;
    if (index >= 0) {
        const s32 count = static_cast<s32>(call_indirect(rt, ctx, word(rt, word(rt, self) + 0x1C8u), {self}));
        if (index < count) {
            const u32 element = word(rt, vector) + (static_cast<u32>(index) << 6);
            call_set(rt, ctx, 0x08C0D458u, {vector, element, element});
            return;
        }
    }
    constexpr u32 kVtable = 0x08CCFC90u;
    const u32 first_member = frame.at(0x20u);
    const u32 second_member = frame.at(0x30u);
    ctx.gpr[21] = 0x08CD0000u;
    ctx.gpr[19] = first_member;
    call_set(rt, ctx, 0x089D76A8u, {frame.at(0u)});
    ctx.gpr[20] = kVtable;
    ctx.gpr[18] = second_member;
    put(rt, frame.at(0u), kVtable);
    call_set(rt, ctx, 0x089FA09Cu, {first_member});
    call_set(rt, ctx, 0x089FA09Cu, {second_member});
    u32 end = word(rt, vector + 4u);
    ctx.gpr[16] = end;
    if (end == word(rt, vector + 8u)) {
        call_set(rt, ctx, 0x08C0D100u, {vector, end, frame.at(0u)});
    } else {
        if (end != 0u) {
            put(rt, end + 0xCu, word(rt, frame.at(0xCu)));
            put(rt, end + 4u, word(rt, frame.at(4u)));
            put(rt, end + 0x10u, word(rt, frame.at(0x10u)));
            put(rt, end + 0x14u, word(rt, frame.at(0x14u)));
            put(rt, end + 8u, word(rt, frame.at(8u)));
            put(rt, end + 0x18u, word(rt, frame.at(0x18u)));
            put(rt, end + 0x1Cu, word(rt, frame.at(0x1Cu)));
            put(rt, end, kVtable);
            call_set(rt, ctx, 0x089FA280u, {end + 0x20u, first_member});
            call_set(rt, ctx, 0x089FA280u, {end + 0x30u, second_member});
            end = word(rt, vector + 4u);
        }
        end += 0x40u;
        ctx.gpr[16] = end;
        put(rt, vector + 4u, end);
    }
    ctx.gpr[2] = kVtable;
    put(rt, frame.at(0u), kVtable);
    call_set(rt, ctx, 0x089F9B74u, {second_member});
    call_set(rt, ctx, 0x089F9B74u, {first_member});
    call_set(rt, ctx, 0x089D6684u, {frame.at(0u)});
}

// The edge scan the two polygon functions share: the polygon of the 0x48-byte record (vertex count `count`, 2-float vertices from +0) is walked edge by edge (the last
// closing on the first) and for each edge whose y difference is not zero the signed distance t = (cross terms) / (y difference) updates the largest negative
// `farthest` (edge difference below zero) or the smallest positive `nearest`. v0 = 0 afterwards.
void scan_polygon_edges(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 record, u32 count, float x, float y, float &nearest, float &farthest) {
    const u32 last = count - 1u;
    u32 vertex = 0u;
    u32 next_vertex;
    do {
        u32 following_index = 0u;
        next_vertex = count;
        if (vertex != last) {
            next_vertex = vertex + 1u;
            following_index = next_vertex;
        }
        ctx.gpr[2] = following_index;
        const u32 current = record + (vertex << 3);
        const u32 following = record + (following_index << 3);
        const float current_y = float_at(rt, current + 4u);
        const float edge_y = -(float_at(rt, following + 4u) - current_y);
        const float current_x = float_at(rt, current);
        const float rise = current_y - y;
        const float run = current_x - x;
        const float edge_x = float_at(rt, following) - current_x;
        const float sum = psp_mul(edge_y, run) + psp_mul(edge_x, rise);
        if (!(edge_y == 0.0f)) {
            const float distance = sum / edge_y;
            if (edge_y < 0.0f) {
                if (farthest < distance) farthest = distance;
            } else if (distance < nearest) {
                nearest = distance;
            }
        }
        ctx.gpr[2] = next_vertex < count ? 1u : 0u;
        ctx.gpr[9] = next_vertex;
        vertex = next_vertex;
    } while (next_vertex < count);
}

constexpr float kLargest = 3.4028234663852886e38f;

// 0x088B70DC (a0 = the object, a1 = the index, a2 = the side flag, a3 = the point, the result stored over the point's x): a negative index does nothing; a zero flag
// tail-jumps to 0x088B6F80; otherwise the polygon of record a1 (a0 + 0xA4 + 0x48 * a1, vertex count +0x40) is intersected with the point's ray (scan_polygon_edges,
// the extremes starting at +/- FLT_MAX): x + the smallest positive distance + 0.01 for a positive flag, x + the largest negative - 0.01 otherwise is stored. v0 = a1 << 6 before the
// scan, 0 after it.
void range_through_polygon_by_side(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const s32 index = static_cast<s32>(ctx.gpr[5]);
    const s32 flag = static_cast<s32>(ctx.gpr[6]);
    const u32 point = ctx.gpr[7];
    ctx.gpr[12] = static_cast<u32>(flag);
    ctx.gpr[6] = point;
    if (index < 0) return;
    ctx.gpr[2] = static_cast<u32>(index) << 6;
    if (flag == 0) {
        jump(ctx, 0x088B6F80u);
        return;
    }
    const u32 record = object + 0xA4u + (static_cast<u32>(index) << 3) + (static_cast<u32>(index) << 6);
    const u32 count = word(rt, record + 0x40u);
    ctx.gpr[3] = record;
    ctx.gpr[11] = count;
    const float x = float_at(rt, point);
    float nearest = kLargest;
    float farthest = -kLargest;
    if (count != 0u) scan_polygon_edges(rt, ctx, record, count, x, float_at(rt, point + 4u), nearest, farthest);
    const float tolerance = std::bit_cast<float>(0x3C23D70Au);
    const float result = flag > 0 ? nearest + x + tolerance : x + farthest - tolerance;
    put_float(rt, point, result);
}

// 0x088B6F80 (a0 = the object, a1 = the index, a2 = the point, the result stored over the point's x): the same intersection of the polygon of record a1 with the
// point's ray, but x moves to the nearer of the two extremes: x + the smallest positive distance + 0.01 when it is closer than the largest negative one, else x +
// that - 0.01. v0 = a1 << 6 (also for a negative index, which stores nothing), 0 after the scan.
void range_through_polygon_nearest(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const s32 index = static_cast<s32>(ctx.gpr[5]);
    const u32 point = ctx.gpr[6];
    ctx.gpr[2] = static_cast<u32>(index) << 6;
    if (index < 0) return;
    const u32 record = object + 0xA4u + (static_cast<u32>(index) << 3) + (static_cast<u32>(index) << 6);
    const u32 count = word(rt, record + 0x40u);
    ctx.gpr[3] = record;
    ctx.gpr[10] = count;
    const float x = float_at(rt, point);
    float nearest = kLargest;
    float farthest = -kLargest;
    if (count != 0u) scan_polygon_edges(rt, ctx, record, count, x, float_at(rt, point + 4u), nearest, farthest);
    const float tolerance = std::bit_cast<float>(0x3C23D70Au);
    const float result = std::fabs(nearest) < std::fabs(farthest) ? nearest + x + tolerance : x + farthest - tolerance;
    put_float(rt, point, result);
}

// 0x08B77E28: closes the stdio-like stream a0 (a null one gives 0): its flush (0x08B7807C) runs for a stream in write mode, the stream's close function (+0x2C, with the
// context +0x1C) for any with one (a negative answer gives -1), the buffer (+0x10, flag 0x80) and the pushed-back buffer (+0x30, unless it is the inline one at +0x40)
// and the second buffer (+0x44) are freed (0x08B354A4 on 0x08B796C4's record) and the flags (+0xC) cleared. v0 = 0, the flush's result or -1.
void close_stream(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 stream = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x8u}, {17u, 0x4u}});
    ctx.gpr[16] = stream;
    if (stream == 0u) {
        ctx.gpr[17] = 0u;
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 context = call_set(rt, ctx, 0x08B796C4u, {});
    if (word(rt, context + 0x38u) == 0u) {
        const u32 again = call_set(rt, ctx, 0x08B796C4u, {});
        call_set(rt, ctx, 0x08B78430u, {again});
    }
    const u32 flags = signed_half(rt, stream + 0xCu);
    ctx.gpr[2] = flags;
    ctx.gpr[17] = 0u;
    if (flags == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    u32 result = 0u;
    if ((flags & 8u) != 0u) {
        result = call_set(rt, ctx, 0x08B7807Cu, {stream});
        ctx.gpr[17] = result;
    }
    const u32 close_function = word(rt, stream + 0x2Cu);
    ctx.gpr[2] = close_function;
    if (close_function != 0u) {
        const s32 answer = static_cast<s32>(call_indirect(rt, ctx, close_function, {word(rt, stream + 0x1Cu)}));
        if (answer < 0) {
            result = 0xFFFFFFFFu;
            ctx.gpr[17] = result;
        }
    }
    const u32 now = rt.memory().load16(stream + 0xCu);
    if ((now & 0x80u) != 0u) {
        const u32 owner = call_set(rt, ctx, 0x08B796C4u, {});
        call_set(rt, ctx, 0x08B354A4u, {owner, word(rt, stream + 0x10u)});
        put(rt, stream + 0x10u, 0u);
    }
    const u32 pushed_back = word(rt, stream + 0x30u);
    if (pushed_back != 0u) {
        if (pushed_back == stream + 0x40u) {
            put(rt, stream + 0x30u, 0u);
        } else {
            const u32 owner = call_set(rt, ctx, 0x08B796C4u, {});
            call_set(rt, ctx, 0x08B354A4u, {owner, word(rt, stream + 0x30u)});
            put(rt, stream + 0x30u, 0u);
        }
    }
    const u32 second = word(rt, stream + 0x44u);
    if (second != 0u) {
        const u32 owner = call_set(rt, ctx, 0x08B796C4u, {});
        call_set(rt, ctx, 0x08B354A4u, {owner, second});
        put(rt, stream + 0x44u, 0u);
    }
    rt.memory().store16(stream + 0xCu, 0u);
    ctx.gpr[2] = result;
}

// Allegrex `wsbw`: the four bytes of a word reversed.
u32 wsbw(u32 value) {
    return ((value & 0xFFu) << 24) | ((value & 0xFF00u) << 8) | ((value & 0xFF0000u) >> 8) | ((value & 0xFF000000u) >> 24);
}

// 0x08B2A438 (a0 = the 8-byte block, a1 = the data, a2 = the key word, a3 = a flag byte, f12 / f13 = two floats): mixes the block with the data and the key
// (byte-swapped words, 0x08B29E88 / 0x08B1ECC8 / 0x08B1EDE0 / 0x08C4FF60 into the 0x9F0-byte workspace at sp + 8), runs 0x08B2A370 over the block's second word
// with f12, with a set flag a second mixing round (0x08B2A3EC) and finally over the first word with f13. v0 = the last call's.
void mix_block(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 block = ctx.gpr[4];
    const u32 data = ctx.gpr[5];
    const u32 key = ctx.gpr[6];
    const u32 round_flag = ctx.gpr[7] & 0xFFu;
    const float first = ctx.fpr[12];
    const float second = ctx.fpr[13];
    GuestFrame frame(ctx, 0xA00u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x9D4u}, {22u, 0x9E8u}, {19u, 0x9DCu}, {16u, 0x9D0u}, {31u, 0x9ECu}, {21u, 0x9E4u}, {20u, 0x9E0u}, {18u, 0x9D8u}, {53u, 0x9F4u}, {52u, 0x9F0u}});
    const u32 words = block + 8u;
    const u32 workspace = frame.at(8u);
    ctx.gpr[17] = words;
    ctx.gpr[22] = key;
    ctx.gpr[19] = block;
    ctx.gpr[16] = data;
    ctx.gpr[21] = data + 4u;
    ctx.gpr[20] = workspace;
    ctx.gpr[18] = round_flag;
    ctx.fpr[21] = second;
    ctx.fpr[20] = first;
    const u32 seed = call_set(rt, ctx, 0x08B29E88u, {0u, words, data});
    put(rt, block + 4u, wsbw(seed ^ key));
    const u32 random = wsbw(call_set(rt, ctx, 0x08B1ECC8u, {}));
    const u32 swapped = wsbw(random);
    put(rt, block, random);
    put(rt, frame.at(4u), random);
    call_set(rt, ctx, 0x08B1EDE0u, {workspace, wsbw(word(rt, block + 4u)) + swapped});
    const u32 head = word(rt, block);
    const u32 tail_word = word(rt, block + 4u);
    put(rt, frame.at(0u), wsbw(tail_word) ^ wsbw(head));
    put(rt, frame.at(4u), head);
    const u32 inner = block + 4u;
    ctx.gpr[17] = inner;
    ctx.gpr[16] = data + 8u;
    call_set(rt, ctx, 0x08C4FF60u, {words, data, workspace, frame.at(0u)});
    ctx.fpr[12] = first;
    call_set(rt, ctx, 0x08B2A370u, {inner, data + 4u, inner, 4u});
    if (round_flag != 0u) {
        call_set(rt, ctx, 0x08B1EDE0u, {workspace, key ^ wsbw(word(rt, block))});
        call_set(rt, ctx, 0x08B2A3ECu, {inner, data + 4u, workspace, 0u, 0u});
    }
    ctx.fpr[12] = second;
    call_set(rt, ctx, 0x08B2A370u, {block, data + 8u, block, 4u});
}

// 0x08AECD60: tears down the global table at 0x08D41AD0 when its flag word +0x464 is set: for each of the +0x47C records of 0x60B0 bytes (array at +0x478) the handle
// at +0x14 (when non-negative) is cleared (0x08C7AA08 with the word +0x18, 0x08C7A9D8, 0x08C7AAA0) and set to -1, 0x08AEC800(record) and 0x08AEEEF4(word +4) run, a non-negative
// word +0x2C is released (0x08AEEE50(word +4, word +0x20, word) and the byte +0x3F cleared), and the words +0x1C and +0x18 are freed (0x08C7A9E0) and set to -1;
// then the array itself is freed (0x08AD85C8) and the word at +0x474 (0x08C7A9E0) set to -1.
void tear_down_global_table(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x14u}, {31u, 0x18u}, {20u, 0x10u}, {19u, 0xCu}, {18u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
    constexpr u32 kTable = 0x08D41AD0u;
    ctx.gpr[21] = 0x08D40000u;
    ctx.gpr[3] = kTable;
    ctx.gpr[2] = word(rt, kTable + 0x464u);
    if (ctx.gpr[2] == 0u) return;
    ctx.gpr[2] = word(rt, kTable + 0x47Cu);
    ctx.gpr[18] = 0u;
    if (static_cast<s32>(ctx.gpr[2]) > 0) {
        ctx.gpr[20] = kTable;
        ctx.gpr[19] = 0xFFFFFFFFu;
        ctx.gpr[17] = 0u;
        u32 offset = 0u;
        s32 done = 0;
        do {
            const u32 record = word(rt, kTable + 0x478u) + offset;
            ctx.gpr[5] = 1u;
            ++done;
            ctx.gpr[18] = static_cast<u32>(done);
            ctx.gpr[16] = record;
            ctx.gpr[2] = word(rt, record + 0x14u);
            offset += 0x60B0u;
            ctx.gpr[17] = offset;
            if (static_cast<s32>(ctx.gpr[2]) >= 0) {
                put(rt, record + 0x10u, 0u);
                call_set(rt, ctx, 0x08C7AA08u, {word(rt, record + 0x18u), 1u});
                call_set(rt, ctx, 0x08C7A9D8u, {word(rt, record + 0x14u), 0u});
                call_set(rt, ctx, 0x08C7AAA0u, {word(rt, record + 0x14u)});
                put(rt, record + 0x14u, 0xFFFFFFFFu);
            }
            call_set(rt, ctx, 0x08AEC800u, {record});
            call_set(rt, ctx, 0x08AEEEF4u, {word(rt, record + 4u)});
            ctx.gpr[2] = word(rt, record + 0x2Cu);
            ctx.gpr[6] = ctx.gpr[2];
            if (static_cast<s32>(ctx.gpr[2]) >= 0) {
                call_set(rt, ctx, 0x08AEEE50u, {word(rt, record + 4u), word(rt, record + 0x20u), ctx.gpr[6]});
                rt.memory().store8(record + 0x3Fu, 0u);
                put(rt, record + 0x2Cu, 0xFFFFFFFFu);
            }
            ctx.gpr[2] = word(rt, record + 0x1Cu);
            ctx.gpr[4] = ctx.gpr[2];
            if (static_cast<s32>(ctx.gpr[2]) >= 0) {
                call_set(rt, ctx, 0x08C7A9E0u, {ctx.gpr[2]});
                put(rt, record + 0x1Cu, 0xFFFFFFFFu);
            }
            ctx.gpr[2] = word(rt, record + 0x18u);
            ctx.gpr[4] = ctx.gpr[2];
            if (static_cast<s32>(ctx.gpr[2]) >= 0) {
                call_set(rt, ctx, 0x08C7A9E0u, {ctx.gpr[2]});
                put(rt, record + 0x18u, 0xFFFFFFFFu);
            }
            ctx.gpr[2] = done < static_cast<s32>(word(rt, kTable + 0x47Cu)) ? 1u : 0u;
        } while (ctx.gpr[2] != 0u);
    }
    ctx.gpr[16] = kTable;
    ctx.gpr[4] = word(rt, kTable + 0x478u);
    if (ctx.gpr[4] != 0u) {
        call_set(rt, ctx, 0x08AD85C8u, {ctx.gpr[4]});
        put(rt, kTable + 0x478u, 0u);
    }
    ctx.gpr[4] = word(rt, kTable + 0x474u);
    if (static_cast<s32>(ctx.gpr[4]) >= 0) {
        call_set(rt, ctx, 0x08C7A9E0u, {ctx.gpr[4]});
        ctx.gpr[3] = 0xFFFFFFFFu;
        put(rt, kTable + 0x474u, 0xFFFFFFFFu);
    }
}

// 0x08AE8D50: applies the event a1 to the slots of the object a0 (a signed-byte count at +0x36, halfword ids from +0x38): the strength s = the halfword +0x4C, scaled by
// (halfword event +0x12 >> 12) with flag 0x200, by 0x08AD986C(halfword event +0x16) with flag 0x800; an adjustment from the halfword +0x10 (flag 0x100) plus, for a table kind 1
// (word +0x14 of *(a0 + 0x10)), (byte of that table - 0x40) * 64, scaled by the signed bytes +0x52 / +0x53 into another 0x08AD986C factor; every non-negative slot id is
// passed to 0x08AEF874(id, 5, s, 0). v0 = the count when it is not positive, else 0.
void apply_event_to_slots(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 event = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {17u, 0x4u}, {31u, 0x14u}, {20u, 0x10u}, {19u, 0xCu}, {16u, 0x0u}});
    ctx.gpr[18] = object;
    ctx.gpr[17] = event;
    const u32 flags = signed_half(rt, event + 2u);
    ctx.gpr[16] = flags;
    const u32 table = word(rt, object + 0x10u);
    ctx.gpr[20] = table;
    const auto scale = [](u32 value, u32 factor) { return static_cast<u32>(static_cast<s32>(value * factor) >> 12); };
    u32 strength = signed_half(rt, object + 0x4Cu);
    ctx.gpr[19] = strength;
    if ((flags & 0x200u) != 0u) {
        strength = scale(strength, signed_half(rt, event + 0x12u));
        ctx.gpr[19] = strength;
    }
    if ((flags & 0x800u) != 0u) {
        const u32 factor = call_set(rt, ctx, 0x08AD986Cu, {signed_half(rt, event + 0x16u)});
        strength = scale(strength, factor);
        ctx.gpr[19] = strength;
    }
    u32 adjustment = 0u;
    if ((flags & 0x100u) != 0u) adjustment = signed_half(rt, event + 0x10u);
    if (word(rt, table + 0x14u) == 1u) {
        const u32 kind = signed_byte(rt, object + 0xBu);
        adjustment += (rt.memory().load8(table + kind * 6u + 0x3Du) - 0x40u) << 6;
    }
    ctx.gpr[5] = adjustment;
    u32 count_byte;
    if (adjustment == 0u) {
        count_byte = rt.memory().load8(object + 0x36u);
    } else {
        const u32 range = static_cast<s32>(adjustment) < 0 ? signed_byte(rt, object + 0x52u) : signed_byte(rt, object + 0x53u);
        const u32 factor = call_set(rt, ctx, 0x08AD986Cu, {scale(range << 7, adjustment)});
        strength = scale(strength, factor);
        ctx.gpr[19] = strength;
        count_byte = rt.memory().load8(object + 0x36u);
    }
    s32 slots = static_cast<std::int8_t>(count_byte);
    ctx.gpr[2] = static_cast<u32>(slots);
    ctx.gpr[17] = 0u;
    if (slots <= 0) return;
    u32 cursor = object + 0x38u;
    ctx.gpr[16] = cursor;
    s32 index = 0;
    u32 id = signed_half(rt, cursor);
    for (;;) {
        ++index;
        cursor += 2u;
        ctx.gpr[17] = static_cast<u32>(index);
        ctx.gpr[16] = cursor;
        ctx.gpr[4] = id;
        if (static_cast<s32>(id) >= 0) {
            call_set(rt, ctx, 0x08AEF874u, {id, 5u, strength, 0u});
            count_byte = rt.memory().load8(object + 0x36u);
        }
        slots = static_cast<std::int8_t>(count_byte);
        const bool more = index < slots;
        ctx.gpr[2] = more ? 1u : 0u;
        if (!more) break;
        id = signed_half(rt, cursor);
        ctx.gpr[2] = id;
    }
}

// 0x089D0704: constructor (a0 = the object): base constructor 0x08994FD4, vtables 0x08CC9D60 / 0x08CC9FB8, flags +0x14C with bit 0 set and bits 1..6 cleared, floats 0.2 / 1.0 / 1.0 /
// 0.5 at +0x134 / +0x138 / +0x13C / +0x140, the words +0x12C / +0x130 = 1, +0x144 / +0x148 = 0, the empty string 0x08D74FDC at +0x150, strings 0x08969188 at +0x154 and +0x188
// (0x08CF6ED8) and 0x08A76AA0 at +0x17C / +0x180 / +0x184, 16.0 at +0x158 / +0x15C, zeros over +0x160..+0x177 and the other fields (-1 at +0x19C..+0x1A8, +0x1B4, +0x1BC; bit 24 of
// +0xE0 cleared); then tail-jumps to 0x089689C8(object, 1).
void construct_with_strings(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    bool tail = false;
    {
        GuestFrame frame(ctx, 0x10u);
        SavedRegisters saved(rt, ctx, frame, {{31u, 0x8u}, {17u, 0x4u}, {16u, 0x0u}});
        ctx.gpr[17] = 0x08CF0000u;
        ctx.gpr[16] = self;
        call_set(rt, ctx, 0x08994FD4u, {self});
        const u32 flags = (word(rt, self + 0x14Cu) | 1u) & ~0x7Eu;
        put(rt, self, 0x08CC9D60u);
        put(rt, self + 0x38u, 0x08CC9FB8u);
        put_float(rt, self + 0x134u, std::bit_cast<float>(0x3E4CCCCDu));
        put_float(rt, self + 0x140u, 0.5f);
        put(rt, self + 0x14Cu, flags);
        put(rt, self + 0x150u, 0x08D74FDCu);
        put(rt, self + 0x130u, 1u);
        put_float(rt, self + 0x13Cu, 1.0f);
        put(rt, self + 0x12Cu, 1u);
        put_float(rt, self + 0x138u, 1.0f);
        put(rt, self + 0x144u, 0u);
        put(rt, self + 0x148u, 0u);
        call_set(rt, ctx, 0x08969188u, {self + 0x154u, 0x08CF6ED8u});
        put_float(rt, self + 0x15Cu, 16.0f);
        put_float(rt, self + 0x158u, 16.0f);
        for (u32 offset = 0x160u; offset < 0x178u; offset += 4u) put(rt, self + offset, 0u);
        put(rt, self + 0x178u, 0u);
        call_set(rt, ctx, 0x08A76AA0u, {self + 0x17Cu});
        call_set(rt, ctx, 0x08A76AA0u, {self + 0x180u});
        call_set(rt, ctx, 0x08A76AA0u, {self + 0x184u});
        call_set(rt, ctx, 0x08969188u, {self + 0x188u, 0x08CF6ED8u});
        const u32 other_flags = word(rt, self + 0xE0u) & ~0x1000000u;
        put(rt, self + 0x1BCu, 0xFFFFFFFFu);
        put(rt, self + 0xE0u, other_flags);
        ctx.gpr[2] = other_flags;
        ctx.gpr[3] = 0xFFFFFFFFu;
        put(rt, self + 0x18Cu, 0u);
        put(rt, self + 0x190u, 0u);
        put(rt, self + 0x194u, 0u);
        put(rt, self + 0x198u, 0u);
        put(rt, self + 0x19Cu, 0xFFFFFFFFu);
        put(rt, self + 0x1A0u, 0xFFFFFFFFu);
        put(rt, self + 0x1A4u, 0xFFFFFFFFu);
        put(rt, self + 0x1A8u, 0xFFFFFFFFu);
        put(rt, self + 0x1B0u, 0u);
        put(rt, self + 0x1B4u, 0xFFFFFFFFu);
        put(rt, self + 0x1B8u, 0u);
        put(rt, self + 0x1C0u, 0u);
        rt.memory().store8(self + 0x1C4u, 0u);
        ctx.gpr[4] = self;
        ctx.gpr[5] = 1u;
        tail = true;
    }
    if (tail) jump(ctx, 0x089689C8u);
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
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 8u), {holder});
    if (!decrement_reaches_zero(rt, ctx, holder + 8u)) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0xCu), {holder});
}

// 0x08887678: for a non-null a2, asks the container a0 (virtual +0x24) for its count and for each index i (virtual +0x28 fills the pair (object, holder) at sp): the
// object is given a1 with a2's virtual +0x20(i) and +0x24(i) answers (when the first is non-zero, through the object's virtual +0xC), and the holder is released. v0 follows the asm.
void notify_container_entries(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 container = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 source = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {19u, 0x1Cu}, {17u, 0x14u}, {31u, 0x2Cu}, {22u, 0x28u}, {20u, 0x20u}, {18u, 0x18u}, {16u, 0x10u}});
    ctx.gpr[21] = argument;
    ctx.gpr[19] = container;
    ctx.gpr[17] = source;
    if (source == 0u) return;
    const s32 count = static_cast<s32>(call_indirect(rt, ctx, word(rt, word(rt, container) + 0x24u), {container}));
    ctx.gpr[20] = static_cast<u32>(count);
    if (count <= 0) return;
    ctx.gpr[18] = 0u;
    ctx.gpr[22] = 1u;
    for (s32 index = 0; index != count;) {
        call_indirect(rt, ctx, word(rt, word(rt, container) + 0x28u), {frame.at(0u), container, static_cast<u32>(index)});
        ctx.gpr[2] = word(rt, frame.at(0u));
        if (ctx.gpr[2] != 0u) {
            const u32 first = call_indirect(rt, ctx, word(rt, word(rt, source) + 0x20u), {source, static_cast<u32>(index)});
            ctx.gpr[16] = first;
            const u32 second = call_indirect(rt, ctx, word(rt, word(rt, source) + 0x24u), {source, static_cast<u32>(index)});
            if (first != 0u) {
                const u32 object = word(rt, frame.at(0u));
                call_indirect(rt, ctx, word(rt, word(rt, object) + 0xCu), {object, argument, first, second});
            }
        }
        const u32 holder = word(rt, frame.at(4u));
        ctx.gpr[16] = holder;
        if (holder != 0u) release_holder(rt, ctx, holder);
        ++index;
        ctx.gpr[18] = static_cast<u32>(index);
    }
}

// 0x088EDEB4: when the state word +0x4C8 of the object a0 is 2 (else v0 = 2), under the lock 0x08B344E0 / 0x08B3457C on a0 + 0x1C8 sends each of the four pending slot numbers (bytes at
// +0x4C0..+0x4C3, 0 = none) as one packet: the slot's 0x3FC-byte block (from the buffer word +0x4CC, the last one shortened to the total in +0x4D0) is copied (0x08B79A88) after a
// three-byte header (length, slot, 0) at sp + 4 and handed to the virtual +0xF4 of *(a0 + 0x538)(2, index, 7, sp, 0x400); the slot byte is rewritten and the scan stops after two
// sent packets. A pass that sent none resets the counter +0x4C8 from 2 to 0. v0 = 1, the unlock's result or 2.
void send_pending_packets(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x420u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x404u}, {16u, 0x400u}, {31u, 0x41Cu}, {22u, 0x418u}, {21u, 0x414u}, {20u, 0x410u}, {19u, 0x40Cu}, {18u, 0x408u}});
    const u32 block = self + 0x4C0u;
    ctx.gpr[17] = block;
    ctx.gpr[16] = self;
    ctx.gpr[3] = word(rt, self + 0x4C8u);
    ctx.gpr[2] = 2u;
    if (ctx.gpr[3] != 2u) return;
    const u32 lock = self + 0x1C8u;
    ctx.gpr[21] = lock;
    call_set(rt, ctx, 0x08B344E0u, {lock});
    u32 remaining = 2u;
    u32 sent = 0u;
    u32 index = 0u;
    ctx.gpr[19] = remaining;
    ctx.gpr[20] = sent;
    ctx.gpr[18] = index;
    ctx.gpr[22] = 4u;
    do {
        const u32 slot_address = block + index;
        ctx.gpr[9] = slot_address;
        const u32 slot_byte = rt.memory().load8(slot_address);
        const u32 slot = (slot_byte - 1u) & 0xFFu;
        const u32 offset = slot * 0x3FCu;
        if (slot_byte != 0u) {
            const u32 total = word(rt, block + 0x10u);
            --remaining;
            ctx.gpr[19] = remaining;
            sent = 1u;
            ctx.gpr[20] = sent;
            rt.memory().store8(slot_address, static_cast<std::uint8_t>(slot));
            u32 size = 0x3FCu;
            u32 length_byte = 0xFCu;
            if (total < offset + 0x3FCu) {
                size = total - offset;
                length_byte = size & 0xFFu;
            }
            rt.memory().store8(frame.at(1u), static_cast<std::uint8_t>(slot));
            rt.memory().store8(frame.at(0u), static_cast<std::uint8_t>(length_byte));
            rt.memory().store8(frame.at(2u), 0u);
            call_set(rt, ctx, 0x08B79A88u, {frame.at(4u), word(rt, block + 0xCu) + offset, size});
            const u32 target = word(rt, self + 0x538u);
            call_indirect(rt, ctx, word(rt, word(rt, target) + 0xF4u), {target, 2u, index, 7u, frame.at(0u), 0x400u});
            if (remaining == 0u) {
                sent = 1u;
                ctx.gpr[20] = sent;
                break;
            }
        }
        ++index;
        ctx.gpr[18] = index;
    } while (index != 4u);
    call_set(rt, ctx, 0x08B3457Cu, {lock});
    if (sent != 0u) return;
    ctx.gpr[3] = block + 8u;
    ctx.gpr[2] = word(rt, block + 8u) ^ 2u;
    ctx.gpr[2] = 0u < ctx.gpr[2] ? 1u : 0u;
    if (ctx.gpr[2] != 0u) return;
    put(rt, block + 8u, 0u);
    ctx.gpr[2] = 1u;
}

// 0x08ADDF88 (a0 = the object, a1 = a flag): fixed-point (12-bit) strength values from the halfwords +0x24 / +0x26 scaled by the words +0x2C / +0x30 of the table 0x08D40674 and the
// entry (halfword +0x20) * 8 + 0x34 / +0x38 of it, then by (halfword +0x2A * halfword +0x28) >> 7: with the flag set both go to 0x08AD8CB0(object + 0x3C, 2 / 3, value);
// otherwise the halfwords +0x40 / +0x42 of the object take them and the flag bits 4 / 8 of the words +0x3C / +0x3E are set when they changed (or were not marked). v0 follows the asm.
void store_scaled_values(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    constexpr u32 kTable = 0x08D40674u;
    const auto scale = [](u32 value, u32 factor, unsigned shift) { return static_cast<u32>(static_cast<s32>(value * factor) >> shift); };
    const u32 marks = self + 0x3Cu;
    ctx.gpr[3] = kTable;
    ctx.gpr[11] = self;
    ctx.gpr[17] = marks;
    ctx.gpr[12] = marks;
    const u32 first = scale(signed_half(rt, self + 0x24u), word(rt, kTable + 0x2Cu), 12);
    const u32 second = scale(signed_half(rt, self + 0x26u), word(rt, kTable + 0x30u), 12);
    const u32 entry = kTable + (signed_half(rt, self + 0x20u) << 3);
    u32 value_a = scale(first, word(rt, entry + 0x34u), 12);
    u32 value_b = scale(second, word(rt, entry + 0x38u), 12);
    const u32 gain = scale(signed_half(rt, self + 0x2Au), signed_half(rt, self + 0x28u), 7);
    value_b = scale(value_b, gain, 12);
    value_a = scale(value_a, gain, 12);
    ctx.gpr[7] = flag;
    ctx.gpr[16] = value_b;
    ctx.gpr[6] = value_a;
    if (flag != 0u) {
        call_set(rt, ctx, 0x08AD8CB0u, {marks, 2u, value_a});
        call_set(rt, ctx, 0x08AD8CB0u, {marks, 3u, value_b});
        return;
    }
    const u32 value_a_low = value_a;
    u32 low;
    ctx.gpr[2] = (signed_half(rt, marks + 2u) >> 2) & 1u;
    bool skip_first = false;
    if (ctx.gpr[2] == 0u) {
        low = signed_half(rt, self + 0x3Cu);
    } else {
        ctx.gpr[2] = signed_half(rt, self + 0x40u);
        if (ctx.gpr[2] == value_a_low) {
            ctx.gpr[2] = signed_half(rt, marks + 2u);
            skip_first = true;
        } else {
            low = signed_half(rt, self + 0x3Cu);
        }
    }
    if (!skip_first) {
        rt.memory().store16(self + 0x40u, static_cast<std::uint16_t>(value_a_low));
        low |= 4u;
        rt.memory().store16(self + 0x3Cu, static_cast<std::uint16_t>(low));
        ctx.gpr[2] = signed_half(rt, marks + 2u) | 4u;
        rt.memory().store16(marks + 2u, static_cast<std::uint16_t>(ctx.gpr[2]));
        ctx.gpr[2] = signed_half(rt, marks + 2u);
    }
    ctx.gpr[2] = (ctx.gpr[2] >> 3) & 1u;
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = signed_half(rt, self + 0x3Cu);
    } else {
        ctx.gpr[2] = signed_half(rt, self + 0x42u);
        if (ctx.gpr[2] == value_b) return;
        ctx.gpr[2] = signed_half(rt, self + 0x3Cu);
    }
    rt.memory().store16(self + 0x42u, static_cast<std::uint16_t>(value_b));
    ctx.gpr[2] |= 8u;
    rt.memory().store16(self + 0x3Cu, static_cast<std::uint16_t>(ctx.gpr[2]));
    ctx.gpr[3] = signed_half(rt, marks + 2u) | 8u;
    rt.memory().store16(marks + 2u, static_cast<std::uint16_t>(ctx.gpr[3]));
}

// 0x08B0562C (a0 = a vector, a1 = a second, a2 = the result): picks the unit axis least aligned with the vector's y (+4: below 0.5 in magnitude the y axis, else the x axis), builds
// the axis vector at sp, takes its product with the vector (0x08BAE264, pair -> 0x08C4ACC0 -> record -> 0x08BAE2B0 builds the result vector, 0x08BBBEF8 combines it with a1) and finally
// builds the result a2 from 0x08BAE264(a0, a1). v0 = the last call's.
void build_basis_vector(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4];
    const u32 second = ctx.gpr[5];
    const u32 result = ctx.gpr[6];
    GuestFrame frame(ctx, 0x70u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x5Cu}, {18u, 0x58u}, {17u, 0x54u}, {16u, 0x50u}, {31u, 0x64u}, {20u, 0x60u}});
    ctx.gpr[19] = result;
    ctx.gpr[18] = frame.at(0x2Cu);
    ctx.gpr[17] = second;
    ctx.gpr[16] = first;
    ctx.gpr[5] = first;
    const float y = float_at(rt, first + 4u);
    ctx.fpr[1] = y;
    u32 axis;
    u32 pair;
    u32 record;
    u32 product;
    ctx.gpr[4] = frame.at(0x20u);
    if (0.5f <= y || y <= -0.5f) {
        axis = frame.at(0x20u);
        pair = frame.at(0x10u);
        record = frame.at(0u);
        product = frame.at(0x2Cu);
        put_float(rt, axis, 1.0f);
        put(rt, axis + 4u, 0u);
        put(rt, axis + 8u, 0u);
    } else {
        ctx.gpr[20] = frame.at(0x20u);
        axis = frame.at(0x2Cu);
        pair = frame.at(0x18u);
        record = frame.at(4u);
        product = frame.at(0x20u);
        put_float(rt, axis + 4u, 1.0f);
        put(rt, axis, 0u);
        put(rt, axis + 8u, 0u);
    }
    call_set(rt, ctx, 0x08BAE264u, {axis, first});
    const u32 pair_low = ctx.gpr[2];
    const u32 pair_high = ctx.gpr[3];
    put(rt, pair + 4u, pair_high);
    put(rt, frame.at(0x44u), pair_high);
    put(rt, pair, pair_low);
    put(rt, frame.at(0x40u), pair_low);
    const u32 vector_record = call_set(rt, ctx, 0x08C4ACC0u, {pair});
    put(rt, record, vector_record);
    call_set(rt, ctx, 0x08BAE2B0u, {product, word(rt, vector_record), word(rt, vector_record + 4u)});
    call_set(rt, ctx, 0x08BBBEF8u, {second, product});
    call_set(rt, ctx, 0x08BAE264u, {first, second});
    const u32 low = ctx.gpr[2];
    const u32 high = ctx.gpr[3];
    put(rt, frame.at(0x40u), low);
    put(rt, frame.at(0x44u), high);
    put(rt, frame.at(8u), low);
    put(rt, frame.at(0xCu), high);
    call_set(rt, ctx, 0x08BAE2B0u, {result, low, high});
}

// 0x08A5BB7C: the null-handle report every checked handle load calls with its message in a0.
void report_null(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 message) { call_set(rt, ctx, 0x08A5BB7Cu, {message}); }

// 0x0881D18C: for an index a2 of 2 or more (else v0 = 1): the checked handle 0x0881BD98 fills at sp + 8 (null reports 0x08C98F48) gives an owner (0x0881B068) and 0x0881BF5C(owner, a2, a3,
// sp + 0x10, sp + 0xC) answers an entry index and a pair of words; a pair of -1 / -1 makes the text of nothing (0x08A97A6C with 0), otherwise the checked object a3[entry] (null reports
// 0x08C98EFC) has its virtual +0x3C asked and 0x0881A9CC(owner, a2, pair, the word at sp + 0xC, answer) gives the text; either text is moved into a0 (0x08A5BAD8) unless it is its own slot and released.
void format_indexed_entry(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 index = ctx.gpr[6];
    const u32 array = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x38u}, {20u, 0x30u}, {18u, 0x28u}, {31u, 0x3Cu}, {21u, 0x34u}, {19u, 0x2Cu}, {17u, 0x24u}, {16u, 0x20u}});
    ctx.gpr[2] = static_cast<s32>(index) < 2 ? 1u : 0u;
    ctx.gpr[22] = destination;
    ctx.gpr[20] = array;
    ctx.gpr[18] = index;
    if (static_cast<s32>(index) < 2) return;
    const u32 handle_slot = frame.at(8u);
    ctx.gpr[17] = handle_slot;
    call_set(rt, ctx, 0x0881BD98u, {handle_slot});
    const u32 handle = word(rt, handle_slot);
    ctx.gpr[16] = handle;
    if (handle == 0u) report_null(rt, ctx, 0x08C98F48u);
    const u32 owner = call_set(rt, ctx, 0x0881B068u, {handle});
    ctx.gpr[21] = owner;
    call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
    const u32 entry = call_set(rt, ctx, 0x0881BF5Cu, {owner, index, array, frame.at(0x10u), frame.at(0xCu)});
    const u32 first = word(rt, frame.at(0x10u));
    const u32 second = word(rt, frame.at(0x14u));
    ctx.gpr[18] = first;
    ctx.gpr[19] = second;
    ctx.gpr[3] = 0xFFFFFFFFu;
    ctx.gpr[2] = entry << 2;
    if (first == 0xFFFFFFFFu && second == 0xFFFFFFFFu) {
        const u32 slot = frame.at(4u);
        ctx.gpr[16] = slot;
        call_set(rt, ctx, 0x08A97A6Cu, {slot, 0u});
        if (destination != slot) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, slot)});
        call_set(rt, ctx, 0x08A5B9ECu, {slot});
        return;
    }
    const u32 object = word(rt, array + (entry << 2));
    ctx.gpr[16] = object;
    const u32 extra = word(rt, frame.at(0xCu));
    ctx.gpr[17] = extra;
    if (object == 0u) report_null(rt, ctx, 0x08C98EFCu);
    const u32 answer = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x3Cu), {object});
    const u32 text = call_set(rt, ctx, 0x0881A9CCu, {owner, ctx.gpr[5], first, second, extra, answer});
    call_set(rt, ctx, 0x08A97A6Cu, {frame.at(0u), text});
    if (destination != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08B819B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0xCu), G[19]); G[21] = G[4]; G[20] = G[6]; G[18] = G[5]; G[16] = sra(G[6], 5); G[2] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[5] + 0x8u); G[22] = G[16] + G[2]; G[17] = G[22] + 0x1u; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; { const bool tk = G[2] == 0u; G[5] = word(rt, G[5] + 0x4u); if (tk) goto L_08B81A14; } L_08B81A04: ; G[3] = G[3] << 1; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[17]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[5] = G[5] + 0x1u; if (tk) goto L_08B81A04; } L_08B81A14: ; G[4] = G[21]; call_here(rt, ctx, 0x08B82260u); G[19] = G[2]; { const bool tk = static_cast<std::int32_t>(G[16]) <= 0; G[5] = G[2] + 0x14u; if (tk) goto L_08B81A3C; } G[2] = 0u; L_08B81A2C: ; G[2] = G[2] + 0x1u; put(rt, G[5] + 0x0u, 0u); { const bool tk = G[16] != G[2]; G[5] = G[5] + 0x4u; if (tk) goto L_08B81A2C; } L_08B81A3C: ; G[2] = word(rt, G[18] + 0x10u); G[4] = G[18] + 0x14u; G[6] = G[20] & 0x1Fu; G[2] = G[2] << 2; { const bool tk = G[6] == 0u; G[7] = G[4] + G[2]; if (tk) goto L_08B81AD4; } G[2] = 0x20u; G[8] = G[2] - G[6]; G[3] = 0u; L_08B81A60: ; G[2] = word(rt, G[4] + 0x0u); G[2] = G[2] << (G[6] & 31u); G[2] = G[3] | G[2]; put(rt, G[5] + 0x0u, G[2]); G[5] = G[5] + 0x4u; G[3] = word(rt, G[4] + 0x0u); G[4] = G[4] + 0x4u; G[2] = G[4] < G[7] ? 1u : 0u; { const bool tk = G[2] != 0u; G[3] = G[3] >> (G[8] & 31u); if (tk) goto L_08B81A60; } { const bool tk = G[3] == 0u; put(rt, G[5] + 0x0u, G[3]); if (tk) goto L_08B81A94; } G[17] = G[22] + 0x2u; L_08B81A94: ; G[2] = G[17] + 0xFFFFFFFFu; L_08B81A98: ; put(rt, G[19] + 0x10u, G[2]); G[4] = G[21]; G[5] = G[18]; call_here(rt, ctx, 0x08B82318u); G[2] = G[19]; goto exit_; L_08B81AD4: ; G[2] = word(rt, G[4] + 0x0u); G[4] = G[4] + 0x4u; G[3] = G[4] < G[7] ? 1u : 0u; put(rt, G[5] + 0x0u, G[2]); { const bool tk = G[3] == 0u; G[5] = G[5] + 0x4u; if (tk) goto L_08B81A94; } G[2] = word(rt, G[4] + 0x0u); G[4] = G[4] + 0x4u; G[3] = G[4] < G[7] ? 1u : 0u; put(rt, G[5] + 0x0u, G[2]); { const bool tk = G[3] != 0u; G[5] = G[5] + 0x4u; if (tk) goto L_08B81AD4; } G[2] = G[17] + 0xFFFFFFFFu; goto L_08B81A98;  exit_: ; G[22] = word(rt, frame.at(0x18u)); G[21] = word(rt, frame.at(0x14u)); G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[19] = word(rt, frame.at(0xCu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C6EE0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); G[21] = G[4]; G[4] = frame.at(0x4u); G[17] = G[8]; G[16] = G[7]; G[7] = frame.at(0x3u); call_here(rt, ctx, 0x08C74968u); G[5] = G[16]; G[6] = G[17]; G[4] = frame.at(0x8u); G[7] = frame.at(0x2u); call_here(rt, ctx, 0x08C74968u); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x8u)); G[2] = word(rt, G[17] + 0xFFFFFFF4u); G[3] = word(rt, G[16] + 0xFFFFFFF4u); G[20] = G[17] + G[2]; G[19] = G[16] + G[3]; L_08C6EE70: ; G[4] = G[21]; G[5] = G[17]; G[6] = G[16]; call_here(rt, ctx, 0x08B60864u); { const bool tk = G[2] != 0u; G[18] = G[2]; if (tk) goto L_08C6EEB4; } G[4] = G[17]; call_here(rt, ctx, 0x08B35F60u); G[4] = G[16]; G[17] = G[17] + G[2]; call_here(rt, ctx, 0x08B35F60u); G[4] = G[16] + G[2]; G[2] = G[19] ^ G[4]; { const bool tk = G[20] != G[17]; G[2] = G[2] < 0x1u ? 1u : 0u; if (tk) goto L_08C6EF04; } if (G[2] == 0u) { G[18] = 0xFFFFFFFFu; goto L_08C6EEB4; } L_08C6EEB4: ; G[4] = word(rt, frame.at(0x8u)); G[19] = 0x08D70000u; G[17] = 0x08D77498u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] != G[17]; G[4] = G[4] + 0xFFFFFFFCu; if (tk) goto L_08C6EF40; } G[4] = word(rt, frame.at(0x4u)); L_08C6EED0: ; G[16] = G[4] + 0xFFFFFFF4u; if (G[17] != G[16]) { G[4] = G[4] + 0xFFFFFFFCu; goto L_08C6EF1C; } G[2] = G[18]; L_08C6EEE0: ; goto exit_; L_08C6EF04: ; { const bool tk = G[2] != 0u; G[17] = G[17] + 0x1u; if (tk) goto L_08C6EF14; } G[16] = G[4] + 0x1u; goto L_08C6EE70; L_08C6EF14: ; G[18] = 0x1u; goto L_08C6EEB4; L_08C6EF1C: ; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = G[18]; goto L_08C6EEE0; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08C74884u); G[2] = G[18]; goto L_08C6EEE0; L_08C6EF40: ; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, frame.at(0x4u)); goto L_08C6EED0; } G[4] = G[16]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08C74884u); G[4] = word(rt, frame.at(0x4u)); goto L_08C6EED0;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08885CEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); G[16] = G[4]; G[4] = word(rt, G[4] + 0x190u); if (G[4] == 0u) { G[4] = word(rt, G[16] + 0x194u); goto L_08885D28; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x2Cu); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } put(rt, G[16] + 0x190u, 0u); G[4] = word(rt, G[16] + 0x194u); L_08885D28: ; { const bool tk = G[4] == 0u; if (tk) goto L_08885D44; } G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x2Cu); { const std::uint32_t tg = G[2]; call_here(rt, ctx, tg); } put(rt, G[16] + 0x194u, 0u); L_08885D44: ; G[4] = G[16]; call_here(rt, ctx, 0x08884B98u); G[4] = word(rt, G[16] + 0x188u); G[2] = 0x08D70000u; G[18] = 0x08D75298u; G[17] = G[4] + 0xFFFFFFF4u; if (G[17] == G[18]) { G[4] = word(rt, G[16] + 0x184u); goto L_08885D88; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = word(rt, G[16] + 0x184u); goto L_08885D88; } G[4] = G[17]; G[5] = frame.at(0x1u); call_here(rt, ctx, 0x08B93554u); G[4] = word(rt, G[16] + 0x184u); L_08885D88: ; G[17] = G[4] + 0xFFFFFFF4u; if (G[18] == G[17]) { G[17] = word(rt, G[16] + 0x6Cu); goto L_08885DB8; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[17] = word(rt, G[16] + 0x6Cu); goto L_08885DB8; } G[4] = G[17]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); G[17] = word(rt, G[16] + 0x6Cu); L_08885DB8: ; { const bool tk = G[17] == 0u; G[3] = G[17] + 0x4u; if (tk) goto L_08885E1C; } L_08885DC0: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08885DC0; } { const bool tk = G[4] != G[2]; G[2] = 0x08CC0000u; if (tk) goto L_08885E20; } G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0x8u); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } G[3] = G[17] + 0x8u; L_08885DF0: ; G[4] = word(rt, G[3] + 0x0u); G[2] = G[4] + 0xFFFFFFFFu; put(rt, G[3] + 0x0u, G[2]); G[2] = 1u; { const bool tk = G[2] == 0u; G[2] = 0x1u; if (tk) goto L_08885DF0; } { const bool tk = G[4] != G[2]; G[2] = 0x08CC0000u; if (tk) goto L_08885E20; } G[3] = word(rt, G[17] + 0x0u); G[2] = word(rt, G[3] + 0xCu); { const std::uint32_t tg = G[2]; G[4] = G[17]; call_here(rt, ctx, tg); } L_08885E1C: ; G[2] = 0x08CC0000u; L_08885E20: ; G[2] = 0x08CC1798u; put(rt, G[16] + 0x50u, G[2]); G[4] = G[16] + 0x50u; call_here(rt, ctx, 0x08AF86DCu); goto exit_;  exit_: ; G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08887678(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { notify_container_entries(rt, ctx); }
void func_088B6F80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { range_through_polygon_nearest(rt, ctx); }
void func_088EDEB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { send_pending_packets(rt, ctx); }
void func_089D0704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { construct_with_strings(rt, ctx); }
void func_08A04704(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[20]); put(rt, frame.at(0x38u), G[18]); put(rt, frame.at(0x30u), G[16]); put(rt, frame.at(0x34u), G[17]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x3Cu), G[19]); G[20] = frame.at(0xCu); G[18] = G[4]; G[4] = G[20]; G[16] = G[6]; G[17] = G[7]; call_here(rt, ctx, 0x08A03E58u); { const bool tk = G[16] != 0u; G[2] = 0x1u; if (tk) goto L_08A04788; } G[16] = word(rt, frame.at(0xCu)); if (G[16] != 0u) { G[5] = word(rt, G[16] + 0x30u); goto L_08A0475C; } G[4] = 0x08CB0000u; G[4] = 0x08CA8D40u; call_here(rt, ctx, 0x08A5BB7Cu); G[5] = word(rt, G[16] + 0x30u); L_08A0475C: ; G[16] = frame.at(0x8u); G[4] = G[16]; call_here(rt, ctx, 0x08A85B1Cu); { const bool tk = G[18] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_08A04778; } G[4] = G[18]; call_here(rt, ctx, 0x08A5BAD8u); L_08A04778: ; G[4] = G[16]; call_here(rt, ctx, 0x08A5B9ECu); goto L_08A04838; L_08A04788: ; { const bool tk = G[16] != G[2]; G[19] = word(rt, frame.at(0xCu)); if (tk) goto L_08A04838; } if (G[19] != 0u) { G[16] = word(rt, G[17] + 0x0u); goto L_08A047A8; } G[4] = 0x08CB0000u; G[4] = 0x08CA8D40u; call_here(rt, ctx, 0x08A5BB7Cu); G[16] = word(rt, G[17] + 0x0u); L_08A047A8: ; if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A047C0; } G[4] = 0x08CB0000u; G[4] = 0x08CA8D38u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A047C0: ; G[4] = G[16]; G[16] = frame.at(0x4u); G[2] = word(rt, G[3] + 0x3Cu); G[18] = frame.at(0x10u); { const std::uint32_t tg = G[2]; G[17] = G[19] + 0x20u; call_here(rt, ctx, tg); } G[5] = G[2]; G[4] = G[16]; call_here(rt, ctx, 0x089F9ED4u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, 0x08A0B0CCu); { const bool tk = G[17] == G[18]; G[5] = G[18]; if (tk) goto L_08A04808; } G[4] = G[17]; call_here(rt, ctx, 0x089641C4u); G[3] = word(rt, frame.at(0x20u)); put(rt, G[17] + 0x10u, G[3]); L_08A04808: ; G[4] = word(rt, frame.at(0x4u)); G[2] = 0x08D70000u; G[2] = 0x08D74FD0u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] == G[2]; G[4] = G[4] + 0xFFFFFFFCu; if (tk) goto L_08A04838; } G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); { const bool tk = static_cast<std::int32_t>(G[2]) > 0; G[4] = G[16]; if (tk) goto L_08A04838; } G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); L_08A04838: ; G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x40u)); G[18] = word(rt, frame.at(0x38u)); G[16] = word(rt, frame.at(0x30u)); G[17] = word(rt, frame.at(0x34u)); G[19] = word(rt, frame.at(0x3Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08AD7EB0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { auto &F = ctx.fpr; G[2] = 0x1u; G[3] = G[5] << 4; G[2] = G[2] - G[5]; G[3] = G[4] + G[3]; G[2] = G[2] << 4; G[2] = G[4] + G[2]; G[9] = G[3] + 0x4u; G[3] = sext(load8(G[9] + 0x5u), 8); G[5] = sext(load8(G[2] + 0x9u), 8); G[7] = G[4]; G[4] = word(rt, G[4] + 0x24u); G[3] = G[3] << 4; G[2] = G[5] << 4; G[8] = G[4] + G[2]; G[1] = 0x3FC90000u; G[1] = G[1] | 0xFDBu; F[6] = std::bit_cast<float>(G[1]); { const bool tk = static_cast<std::int32_t>(G[5]) < 0; G[4] = G[4] + G[3]; if (tk) goto L_08AD7F7C; } G[3] = sext(load16(G[4] + 0x2u), 16); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; if (G[2] == 0u) { G[3] = sext(load16(G[4] + 0x4u), 16); goto L_08AD7F30; } G[2] = sext(load16(G[8] + 0x4u), 16); G[3] = G[3] - G[2]; G[6] = G[6] - G[2]; F[3] = std::bit_cast<float>(G[3]); F[0] = std::bit_cast<float>(G[6]); F[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[3]))); F[2] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[3] = F[2] / F[1]; goto L_08AD7F5C; L_08AD7F30: ; G[2] = static_cast<std::int32_t>(G[3]) < static_cast<std::int32_t>(G[6]) ? 1u : 0u; { const bool tk = G[2] == 0u; F[3] = psp_mul(F[6], F[6]); if (tk) goto L_08AD7F80; } G[2] = sext(load16(G[8] + 0x2u), 16); G[3] = G[2] - G[3]; G[2] = G[2] - G[6]; F[3] = std::bit_cast<float>(G[3]); F[0] = std::bit_cast<float>(G[2]); F[1] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[3]))); F[2] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[0]))); F[3] = F[2] / F[1]; L_08AD7F5C: ; F[2] = std::bit_cast<float>(0u); G[1] = 0x3FC90000u; G[1] = G[1] | 0xFDBu; F[0] = std::bit_cast<float>(G[1]); ctx.set_fpu_condition(F[2] < F[3]); { const bool tk = !ctx.fpu_condition(); F[6] = psp_mul(F[3], F[0]); if (tk) goto L_08AD8004; } L_08AD7F7C: ; F[3] = psp_mul(F[6], F[6]); L_08AD7F80: ; G[1] = 0x362E0000u; G[1] = G[1] | 0x9C4Au; F[0] = std::bit_cast<float>(G[1]); G[1] = 0x394F0000u; G[1] = G[1] | 0xB221u; F[1] = std::bit_cast<float>(G[1]); G[4] = sext(load16(G[4] + 0x6u), 16); F[0] = psp_mul(F[3], F[0]); F[5] = float_at(rt, G[7] + 0x3Cu); F[2] = std::bit_cast<float>(G[4]); F[0] = F[0] - F[1]; G[1] = 0x3C080000u; G[1] = G[1] | 0x873Eu; F[1] = std::bit_cast<float>(G[1]); F[4] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[2]))); G[1] = 0x38800000u; F[2] = std::bit_cast<float>(G[1]); F[0] = psp_mul(F[3], F[0]); F[4] = psp_mul(F[4], F[2]); F[0] = F[0] + F[1]; G[1] = 0x3E2A0000u; G[1] = G[1] | 0xAAA4u; F[1] = std::bit_cast<float>(G[1]); F[4] = psp_mul(F[4], F[5]); F[0] = psp_mul(F[3], F[0]); F[0] = F[0] - F[1]; F[3] = psp_mul(F[3], F[0]); G[1] = 0x3F800000u; F[0] = std::bit_cast<float>(G[1]); F[3] = F[3] + F[0]; F[3] = psp_mul(F[6], F[3]); F[3] = psp_mul(F[3], F[4]); F[2] = std::fabs(F[3]); L_08AD8004: ; put_float(rt, G[9] + 0xCu, F[2]); goto exit_;  exit_: ; } if (tail != 0u) jump(ctx, tail); } }
void func_08ADDF88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { store_scaled_values(rt, ctx); }
void func_08AE8D50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_event_to_slots(rt, ctx); }
void func_08AECD60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tear_down_global_table(rt, ctx); }
void func_08B0562C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { build_basis_vector(rt, ctx); }
void func_08B2A438(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { mix_block(rt, ctx); }
void func_08B77E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { close_stream(rt, ctx); }
void func_08C24AEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[18] = G[6]; G[17] = G[5]; G[3] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[4] + 0x8u); { const bool tk = G[3] == G[2]; G[16] = G[4]; if (tk) goto L_08C24B5C; } if (G[3] == 0u) { G[4] = G[3]; goto L_08C24B38; } G[2] = load8(G[3] + 0xFFFFFFFFu); store8(G[3] + 0x0u, G[2]); G[3] = word(rt, G[4] + 0x4u); G[4] = G[3]; L_08C24B38: ; G[3] = G[3] + 0x1u; put(rt, G[16] + 0x4u, G[3]); G[6] = G[4] + 0xFFFFFFFFu; G[6] = G[6] - G[17]; G[16] = load8(G[18] + 0x0u); G[4] = G[4] - G[6]; call_here(rt, ctx, 0x08B79BACu); store8(G[17] + 0x0u, G[16]); goto L_08C24C28; L_08C24B5C: ; G[2] = word(rt, G[4] + 0x0u); G[4] = G[3] - G[2]; G[3] = 0xFFFFFFFFu; { const bool tk = G[4] != G[3]; if (tk) goto L_08C24B7C; } G[4] = 0x08CB0000u; G[4] = 0x08CB09FCu; call_here(rt, ctx, 0x08B5ADDCu); L_08C24B7C: ; { const bool tk = G[4] == 0u; G[3] = 0x1u; if (tk) goto L_08C24BA0; } G[3] = G[4] << 1; G[2] = G[3] < G[4] ? 1u : 0u; if (G[2] == 0u) { G[20] = G[3]; goto L_08C24BA4; } G[3] = 0xFFFFFFFFu; G[20] = 0xFFFFFFFFu; goto L_08C24BA4; L_08C24BA0: ; G[20] = G[3]; L_08C24BA4: ; G[4] = G[3]; G[5] = 0x4u; call_here(rt, ctx, 0x08A5BC20u); G[4] = word(rt, G[16] + 0x0u); G[6] = G[2]; G[5] = G[17]; G[7] = frame.at(0x2u); G[19] = G[2]; call_here(rt, ctx, 0x08C24A38u); { const bool tk = G[2] == 0u; G[6] = G[2]; if (tk) goto L_08C24BD8; } G[2] = load8(G[18] + 0x0u); store8(G[6] + 0x0u, G[2]); L_08C24BD8: ; G[5] = word(rt, G[16] + 0x4u); G[4] = G[17]; G[6] = G[6] + 0x1u; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08C24A38u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08C24A18u); G[4] = word(rt, G[16] + 0x0u); G[2] = word(rt, G[16] + 0x8u); { const bool tk = G[4] == 0u; G[5] = G[2] - G[4]; if (tk) goto L_08C24C18; } G[6] = 0x4u; call_here(rt, ctx, 0x08A5BC54u); L_08C24C18: ; G[2] = G[19] + G[20]; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08C24C28: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0881D18C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { format_indexed_entry(rt, ctx); }
void func_088B70DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { range_through_polygon_by_side(rt, ctx); }
void func_089F9860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { insert_default_record(rt, ctx); }
void func_08A15DA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { append_merged_box(rt, ctx); }
void func_08ABADE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_request_record(rt, ctx); }
void func_08AC6FE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x2Cu), G[31]); G[18] = G[4]; G[17] = G[6]; G[16] = G[5]; call_here(rt, ctx, 0x08B344E0u); G[2] = 0x1u; store8(G[18] + 0x28u, G[2]); G[4] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08C7AB50u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[16] = 0u; if (tk) goto L_08AC711C; } put(rt, G[18] + 0x2Cu, G[2]); G[4] = G[2]; call_here(rt, ctx, 0x08C7AB80u); { const bool tk = G[2] != 0u; G[16] = 0u; if (tk) goto L_08AC711C; } G[4] = word(rt, G[18] + 0x2Cu); G[16] = frame.at(0x4u); G[5] = G[16]; call_here(rt, ctx, 0x08C7AB68u); { const bool tk = G[2] != 0u; G[2] = word(rt, frame.at(0x4u)); if (tk) goto L_08AC7118; } G[3] = frame.at(0u); put(rt, frame.at(0x0u), 0u); G[2] = static_cast<std::int32_t>(G[2]) < 0 ? 1u : 0u; if (G[2] == 0u) G[3] = G[16]; G[5] = word(rt, G[3] + 0x0u); G[4] = word(rt, G[18] + 0x2Cu); G[3] = 0x2u; G[2] = G[5] >> 31; G[2] = G[2] + G[5]; G[2] = sra(G[2], 1); put(rt, G[18] + 0x30u, G[3]); G[5] = frame.at(0x8u); put(rt, G[18] + 0x38u, G[2]); call_here(rt, ctx, 0x08C7AB78u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[2] = word(rt, frame.at(0x8u)); if (tk) goto L_08AC7118; } G[4] = word(rt, G[18] + 0x2Cu); G[5] = frame.at(0xCu); put(rt, G[18] + 0x34u, G[2]); G[6] = frame.at(0x10u); G[7] = frame.at(0x14u); put(rt, frame.at(0xCu), 0u); put(rt, frame.at(0x10u), 0u); put(rt, frame.at(0x14u), 0u); call_here(rt, ctx, 0x08C7AB70u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[5] = word(rt, frame.at(0x10u)); if (tk) goto L_08AC7118; } { const bool tk = static_cast<std::int32_t>(G[5]) < 0; G[4] = word(rt, frame.at(0xCu)); if (tk) goto L_08AC70F8; } G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[5]) ? 1u : 0u; { const bool tk = G[2] != 0u; G[3] = word(rt, frame.at(0x14u)); if (tk) goto L_08AC70FC; } { const bool tk = static_cast<std::int32_t>(G[3]) < 0; G[2] = static_cast<std::int32_t>(G[4]) < static_cast<std::int32_t>(G[3]) ? 1u : 0u; if (tk) goto L_08AC70FC; } if (G[2] != 0u) { G[16] = 0x1u; goto L_08AC7100; } G[4] = word(rt, G[18] + 0x2Cu); G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08C7AB60u); G[5] = word(rt, frame.at(0x10u)); G[4] = word(rt, frame.at(0xCu)); G[3] = word(rt, frame.at(0x14u)); goto L_08AC70FC; L_08AC70F8: ; G[3] = word(rt, frame.at(0x14u)); L_08AC70FC: ; G[16] = 0x1u; L_08AC7100: ; put(rt, G[18] + 0x3Cu, G[4]); put(rt, G[18] + 0x40u, G[5]); put(rt, G[18] + 0x44u, G[3]); store8(G[18] + 0x28u, 0u); goto L_08AC711C; L_08AC7118: ; G[16] = 0u; L_08AC711C: ; G[4] = G[18]; call_here(rt, ctx, 0x08B3457Cu); G[2] = G[16]; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B032A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); auto &F = ctx.fpr; G[3] = G[4] + 0xE0u; G[20] = G[8]; G[19] = G[7]; G[18] = G[5]; G[17] = G[6]; put_float(rt, frame.at(0x3Cu), F[23]); F[23] = std::bit_cast<float>(0u); put_float(rt, frame.at(0x38u), F[22]); G[1] = 0x3F800000u; F[22] = std::bit_cast<float>(G[1]); put_float(rt, frame.at(0x34u), F[21]); put_float(rt, frame.at(0x30u), F[20]); G[2] = word(rt, G[4] + 0x1FCu); G[2] = (G[2] >> 12) & 0xFu; G[2] = G[2] << 4; G[2] = G[2] + 0xE0u; G[16] = G[4] + G[2]; { const bool tk = G[16] == G[3]; put(rt, frame.at(0x0u), G[3]); if (tk) goto L_08B033BC; } F[21] = F[23]; L_08B03314: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08C45D84u); G[4] = G[2]; G[5] = G[18]; call_here(rt, ctx, 0x08C46364u); G[4] = frame.at(0u); F[20] = F[0]; call_here(rt, ctx, 0x08C45D7Cu); G[4] = G[2]; G[5] = G[17]; call_here(rt, ctx, 0x08BBBEC4u); ctx.set_fpu_condition(F[0] == F[21]); { const bool tk = !ctx.fpu_condition(); F[1] = -F[20]; if (tk) goto L_08B03368; } ctx.set_fpu_condition(F[21] <= F[20]); if (!ctx.fpu_condition()) { G[2] = word(rt, frame.at(0x0u)); goto L_08B033B0; } G[2] = 0u; goto L_08B033D8; L_08B03368: ; ctx.set_fpu_condition(F[0] < F[21]); { const bool tk = !ctx.fpu_condition(); F[0] = F[1] / F[0]; if (tk) goto L_08B03390; } ctx.set_fpu_condition(F[23] < F[0]); if (ctx.fpu_condition()) { F[23] = F[0]; goto L_08B033A0; } goto L_08B033A0; L_08B03390: ; ctx.set_fpu_condition(F[0] < F[22]); if (ctx.fpu_condition()) { F[22] = F[0]; goto L_08B033A0; } L_08B033A0: ; ctx.set_fpu_condition(F[22] <= F[23]); { const bool tk = ctx.fpu_condition(); G[2] = word(rt, frame.at(0x0u)); if (tk) goto L_08B033D4; } L_08B033B0: ; G[2] = G[2] + 0x10u; { const bool tk = G[16] != G[2]; put(rt, frame.at(0x0u), G[2]); if (tk) goto L_08B03314; } L_08B033BC: ; if (G[19] != 0u) { put_float(rt, G[19] + 0x0u, F[23]); goto L_08B033C4; } L_08B033C4: ; { const bool tk = G[20] == 0u; G[2] = 0x1u; if (tk) goto L_08B033D8; } put_float(rt, G[20] + 0x0u, F[22]); goto L_08B033D8; L_08B033D4: ; G[2] = 0u; L_08B033D8: ; F[23] = float_at(rt, frame.at(0x3Cu)); F[22] = float_at(rt, frame.at(0x38u)); F[21] = float_at(rt, frame.at(0x34u)); F[20] = float_at(rt, frame.at(0x30u)); goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B62CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x30u), G[31]); G[19] = G[5]; G[9] = G[19]; G[18] = G[6]; G[5] = G[7]; G[6] = 0x6u; G[17] = G[7]; G[16] = G[4]; G[7] = G[18]; G[10] = G[16]; G[11] = frame.at(0u); G[2] = word(rt, G[4] + 0x0u); put(rt, frame.at(0x4u), 0u); G[4] = word(rt, G[2] + 0xFFFFFFFCu); G[8] = word(rt, G[2] + 0xFFFFFFF8u); G[2] = 0x10u; G[3] = word(rt, G[4] + 0x0u); put(rt, frame.at(0x10u), G[2]); G[8] = G[16] + G[8]; G[2] = word(rt, G[3] + 0x1Cu); put(rt, frame.at(0x0u), 0u); put(rt, frame.at(0x8u), 0u); { const std::uint32_t tg = G[2]; put(rt, frame.at(0xCu), 0u); call_here(rt, ctx, tg); } G[6] = word(rt, frame.at(0x0u)); { const bool tk = G[6] == 0u; G[7] = 0x6u; if (tk) goto L_08B62DA8; } G[5] = word(rt, frame.at(0xCu)); G[2] = G[5] & 0x6u; { const bool tk = G[2] == G[7]; G[4] = G[6]; if (tk) goto L_08B62D7C; } G[3] = word(rt, frame.at(0x8u)); G[2] = word(rt, frame.at(0x4u)); G[2] = G[3] & G[2]; G[2] = G[2] & 0x6u; if (G[2] != G[7]) { G[3] = G[3] & 0x5u; goto L_08B62D9C; } L_08B62D7C: ; L_08B62D80: ; G[2] = G[4]; goto exit_; L_08B62D9C: ; G[2] = 0x4u; { const bool tk = G[3] != G[2]; if (tk) goto L_08B62DCC; } L_08B62DA8: ; L_08B62DAC: ; G[4] = 0u; G[2] = G[4]; goto exit_; L_08B62DCC: ; { const bool tk = G[5] != 0u; G[3] = G[5] & 0x6u; if (tk) goto L_08B62DF4; } { const bool tk = static_cast<std::int32_t>(G[17]) < 0; G[2] = G[6] + G[17]; if (tk) goto L_08B62E08; } { const bool tk = G[16] == G[2]; G[3] = 0x6u; if (tk) goto L_08B62DE8; } L_08B62DE4: ; G[3] = 0x1u; L_08B62DE8: ; G[5] = G[3]; put(rt, frame.at(0xCu), G[3]); G[3] = G[5] & 0x6u; L_08B62DF4: ; G[2] = 0x6u; { const bool tk = G[3] != G[2]; if (tk) goto L_08B62DAC; } G[4] = word(rt, frame.at(0x0u)); goto L_08B62D80; L_08B62E08: ; G[2] = 0xFFFFFFFEu; { const bool tk = G[17] == G[2]; G[4] = G[18]; if (tk) goto L_08B62DE4; } G[3] = word(rt, G[18] + 0x0u); G[5] = G[17]; G[2] = word(rt, G[3] + 0x20u); G[7] = G[19]; { const std::uint32_t tg = G[2]; G[8] = G[16]; call_here(rt, ctx, tg); } G[3] = G[2]; goto L_08B62DE8;  exit_: ; G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B7634C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { count_values_in_nodes(rt, ctx); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_323
} // namespace gt::game

#define HOT323(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_323::name, gt::ReturnKind::kind)
// HOT323(0x08B819B4u, func_08B819B4, Word);  // no comparable synthetic case yet
HOT323(0x08C6EE0Cu, func_08C6EE0C, Word);
// HOT323(0x08885CECu, func_08885CEC, Word);  // no comparable synthetic case yet
HOT323(0x08887678u, func_08887678, Word);
HOT323(0x088B6F80u, func_088B6F80, Word);
HOT323(0x088EDEB4u, func_088EDEB4, Word);
HOT323(0x089D0704u, func_089D0704, Word);
HOT323(0x08A04704u, func_08A04704, Word);
HOT323(0x08AD7EB0u, func_08AD7EB0, Word);
HOT323(0x08ADDF88u, func_08ADDF88, Word);
HOT323(0x08AE8D50u, func_08AE8D50, Word);
HOT323(0x08AECD60u, func_08AECD60, Word);
HOT323(0x08B0562Cu, func_08B0562C, Word);
HOT323(0x08B2A438u, func_08B2A438, Word);
HOT323(0x08B77E28u, func_08B77E28, Word);
HOT323(0x08C24AECu, func_08C24AEC, Word);
HOT323(0x0881D18Cu, func_0881D18C, Word);
HOT323(0x088B70DCu, func_088B70DC, Word);
HOT323(0x089F9860u, func_089F9860, Word);
HOT323(0x08A15DA8u, func_08A15DA8, Word);
HOT323(0x08ABADE0u, func_08ABADE0, Word);
// HOT323(0x08AC6FE0u, func_08AC6FE0, Word);  // no comparable synthetic case yet
HOT323(0x08B032A8u, func_08B032A8, Word);
// HOT323(0x08B62CD4u, func_08B62CD4, Word);  // no comparable synthetic case yet
HOT323(0x08B7634Cu, func_08B7634C, Word);
