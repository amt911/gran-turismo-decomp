// Hot batch 328: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
// 0x08B5CC1C, 0x08C78CC0 are rewritten as structured C from the disassembly (structured rewrites, batch 90) and registered: the synthetic gate compares them in the garage / challenge / dirt / manual / challenge2 scenes.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_328 {
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

// A guest call with integer arguments (a0-a3, t0-t3) and float ones ($f12, $f13, ...), and a float result in $f0.
float call_float(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 target, std::initializer_list<u32> arguments, std::initializer_list<float> floats) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    reg = 12u;
    for (const float value : floats) ctx.fpr[reg++] = value;
    const GuestResult result = call_guest(rt, ctx, target, arguments, floats);
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.f0;
}

// 0x088AA504 (a0 = object, a1 = flag): stores the flag in the word +0xC. Flag 0: clears the four 0x1C-byte entries from +0xA0 (words +8, +0xC, +0x14,
// +0x18, +0x1C) and the four 0xC-byte links from +0x114 (words +4, +8); v0 = 0x08CC2790. Flag set: the bytes +0x14..+0x17 become 0, 1, 2, 3 (the
// entries' own indices), each entry gets (index, owner) in its words +8 / +0x14 and +0xC / +0x18 and its link (index, owner), the four 0x18-byte records
// from +0x30 are cleared (words +0xC, +0x10, +0x14, +0x1C, bytes +0x18, +0x20), the word +0xA0 gets 0x123456 and the byte +0x9C 0; v0 = 0x123456.
// The original also builds the two vtable-tagged temporaries on its stack frame (dead, below the caller's sp).
void reset_entry_tables(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 flag = ctx.gpr[5];
    put(rt, self + 0xCu, flag);
    if (flag == 0u) {
        for (u32 i = 0; i < 4u; ++i) {
            const u32 entry = self + 0xA0u + 0x1Cu * i;
            put(rt, entry + 0x8u, 0u);
            put(rt, entry + 0xCu, 0u);
            put(rt, entry + 0x14u, 0u);
            put(rt, entry + 0x18u, 0u);
            put(rt, entry + 0x1Cu, 0u);
            const u32 link = self + 0x114u + 0xCu * i;
            put(rt, link + 0x4u, 0u);
            put(rt, link + 0x8u, 0u);
        }
        ctx.gpr[2] = 0x08CC2790u;
        return;
    }
    rt.memory().store8(self + 0x15u, 1u);
    rt.memory().store8(self + 0x16u, 2u);
    rt.memory().store8(self + 0x17u, 3u);
    rt.memory().store8(self + 0x14u, 0u);
    for (u32 i = 0; i < 4u; ++i) {
        const u32 entry = self + 0xA0u + 0x1Cu * i;
        const u32 index = signed_byte(rt, self + 0x14u + i);
        put(rt, entry + 0xCu, self);
        put(rt, entry + 0x8u, index);
        put(rt, entry + 0x14u, index);
        put(rt, entry + 0x18u, self);
        put(rt, entry + 0x1Cu, 0u);
        const u32 link = self + 0x114u + 0xCu * i;
        put(rt, link + 0x4u, signed_byte(rt, self + 0x14u + i));
        put(rt, link + 0x8u, self);
    }
    for (u32 i = 0; i < 4u; ++i) {
        const u32 record = self + 0x30u + 0x18u * i;
        put(rt, record + 0xCu, 0u);
        put(rt, record + 0x10u, 0u);
        put(rt, record + 0x14u, 0u);
        rt.memory().store8(record + 0x18u, 0u);
        rt.memory().store8(record + 0x20u, 0u);
        put(rt, record + 0x1Cu, 0u);
    }
    put(rt, self + 0xA0u, 0x00123456u);
    rt.memory().store8(self + 0x9Cu, 0u);
    ctx.gpr[2] = 0x00123456u;
}

// 0x0889B1FC (a0 = the object, a1 = a pass-through argument): the block at self[0x58] -> [+0x10] holds up to min(self[+0x48], 16) groups of three 0x10-byte
// records (the array at block[+0x4C], 0x38 bytes per group), the output slots at self + 0x4A0 (0x40 bytes per group). For every record of a group: kind
// (the halfword +0xC) 0 copies its three floats to the group's scratch vector; otherwise, when the kind or the index (the halfword +0xE) differs from the
// previous record's, a kind-1 record calls 0x08B56510 then 0x0889AB88(a1, index, block) and any other kind calls 0x08B568DC on the slot
// of that index (self + 0x4A0 + index * 0x40); then 0x08BAC1E8(scratch vector, record) runs. The group's three scratch vectors (frame +0, +0x10, +0x20) go to
// 0x0889B0B0(slot, ...); the frame ends with 0x08B56940. v0 is the callee's (0x10 when the count is 0, the entry v0 when the block or its member is 0).
void emit_record_groups(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x70u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x60u}, {31u, 0x64u}, {23u, 0x5Cu}, {22u, 0x58u}, {21u, 0x54u}, {20u, 0x50u}, {19u, 0x4Cu}, {18u, 0x48u}, {17u, 0x44u}, {16u, 0x40u}});
    put(rt, frame.at(0x38u), argument);
    const u32 owner = word(rt, self + 0x58u);
    if (owner == 0u) return;
    const u32 block = word(rt, owner + 0x10u);
    if (block == 0u) return;
    const u32 raw_count = word(rt, block + 0x48u);
    ctx.gpr[2] = 0x10u;
    if (raw_count == 0u) return;
    const s32 count = std::min(static_cast<s32>(raw_count), 0x10);
    call_set(rt, ctx, 0x08B569A0u, {self});
    if (count > 0) {
        u32 slot = self + 0x4A0u;
        u32 group_offset = 0u;
        u32 done = 0u;
        put(rt, frame.at(0x34u), 0u);
        put(rt, frame.at(0x30u), 0u);
        do {
            u32 record = word(rt, block + 0x4Cu) + group_offset;
            u32 mode = 0u;
            u32 last_index = 0u;
            for (u32 j = 0; j < 3u; ++j, record += 0x10u) {
                const u32 vector = frame.at(0x10u * j);
                const u32 kind = signed_half(rt, record + 0xCu);
                if (kind == 0u) {
                    put(rt, vector, word(rt, record));
                    put(rt, vector + 0x4u, word(rt, record + 0x4u));
                    put(rt, vector + 0x8u, word(rt, record + 0x8u));
                    continue;
                }
                if (kind != mode || signed_half(rt, record + 0xEu) != last_index) {
                    mode = 1u;
                    if (kind != 1u) {
                        mode = 2u;
                        call_set(rt, ctx, 0x08B568DCu, {self + (signed_half(rt, record + 0xEu) << 6) + 0x4A0u});
                    } else {
                        call_set(rt, ctx, 0x08B56510u, {});
                        call_set(rt, ctx, 0x0889AB88u, {word(rt, frame.at(0x38u)), signed_half(rt, record + 0xEu), block});
                    }
                    last_index = signed_half(rt, record + 0xEu);
                }
                call_set(rt, ctx, 0x08BAC1E8u, {vector, record});
            }
            ++done;
            put(rt, frame.at(0x34u), done);
            call_set(rt, ctx, 0x0889B0B0u, {slot, frame.at(0u), frame.at(0x10u), frame.at(0x20u)});
            slot += 0x40u;
            group_offset += 0x38u;
            put(rt, frame.at(0x30u), group_offset);
        } while (static_cast<u32>(count) != done);
    }
    call_set(rt, ctx, 0x08B56940u, {});
}


// 0x08B6BE8C (f12 = x, result f0; v0 as the original leaves it): a signed scaling of an exponential-like function on |x| by its bit pattern. Infinity or NaN
// gives x + x. The sign picks the scale f20 (0x08CB64B0 for negative, 0x08CB64B4 otherwise). |x| bits above 0x41AFFFFF: up to 0x42B17217 f20 * 0x08B6AB30(0x08B663A4(x));
// up to 0x42B2D4FC f20 * r * r with r = 0x08B6AB30(0x08B663A4(x) * the constant 0x08CB64B4); above that x * the constant 0x08CB64B8. Below it, |x| at or under
// 0x317FFFFF returns x when the constant 0x08CB64BC < x + the constant 0x08CB64B8; the rest of the range takes t = 0x08B6E920(0x08B663A4(x)) and gives
// f20 * (t + t - t*t / (t + c)) (c = 0x08CB64BC), or from |x| bits 0x3F800000 f20 * (t + t / (t + c)).
void scaled_exponential(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x8u}, {17u, 0x4u}, {52u, 0x10u}});
    const float x = ctx.fpr[12];
    const u32 magnitude = bits_of(x) & 0x7FFFFFFFu;
    float result = 0.0f;
    u32 v0 = 0u;
    if (magnitude > 0x7F7FFFFFu) {
        result = x + x;
        ctx.gpr[2] = 1u;
    } else {
        const float scale = float_at(rt, (bits_of(x) >> 31) != 0u ? 0x08CB64B0u : 0x08CB64B4u);
        const float constant_c = float_at(rt, 0x08CB64BCu);
        bool rational = false;
        v0 = 0x08CB0000u;
        if (magnitude > 0x41AFFFFFu) {
            if (magnitude <= 0x42B17217u) {
                const float reduced = call_float(rt, ctx, 0x08B663A4u, {}, {x});
                result = psp_mul(scale, call_float(rt, ctx, 0x08B6AB30u, {}, {reduced}));
                v0 = ctx.gpr[2];
            } else if (magnitude <= 0x42B2D4FCu) {
                const float reduced = call_float(rt, ctx, 0x08B663A4u, {}, {x});
                const float scaled = psp_mul(reduced, float_at(rt, 0x08CB64B4u));
                const float root = call_float(rt, ctx, 0x08B6AB30u, {}, {scaled});
                result = psp_mul(psp_mul(scale, root), root);
                v0 = ctx.gpr[2];
            } else {
                result = psp_mul(x, float_at(rt, 0x08CB64B8u));
            }
        } else if (magnitude > 0x317FFFFFu) {
            rational = true;
        } else if (constant_c < x + float_at(rt, 0x08CB64B8u)) {
            result = x;
        } else {
            rational = true;
        }
        if (rational) {
            const float reduced = call_float(rt, ctx, 0x08B663A4u, {}, {x});
            const float t = call_float(rt, ctx, 0x08B6E920u, {}, {reduced});
            if (magnitude > 0x3F7FFFFFu) {
                result = psp_mul(scale, t + t / (t + constant_c));
                ctx.gpr[2] = 1u;
            } else {
                result = psp_mul(scale, (t + t) - psp_mul(t, t) / (t + constant_c));
                ctx.gpr[2] = 0u;
            }
            v0 = ctx.gpr[2];
        }
        ctx.gpr[2] = v0;
    }
    ctx.fpr[12] = result;
    ctx.fpr[0] = result;
}

// 0x08B5CA20 (a0 = the owner): empties three tables of an owner. Stage 1 / 2: for each pointer of the tables at +4 / +0xC (the count at +8) that is set
// and where 0x08B60804(pointer + 4, -1) answers 1, the pointer's virtual slot 1 is called on it; then 0x08B61FC0(table) frees the table. Stage 3: the six
// pointers of the table at +0x10 are each freed with 0x08B61FC0 and the table itself is freed by a tail call (made after the frame is gone).
// v0 follows the registers: the entry v0 when the first table is missing, i * 4 after stage 1's loop, the last callee's result after a free, the
// compare / address leftovers otherwise.
bool empty_owner_tables(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 &tail_argument) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {31u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}});
    u32 v0 = ctx.gpr[2];
    const auto release_entries = [&](u32 table_offset) {
        // One pass over the table: an entry whose state answers 1 gets its virtual slot 1 called.
        u32 i = 0u;
        while (i < word(rt, self + 8u)) {
            const u32 entry = word(rt, word(rt, self + table_offset) + (i << 2));
            if (entry != 0u) {
                if (call_set(rt, ctx, 0x08B60804u, {entry + 4u, 0xFFFFFFFFu}) == 1u) call_indirect(rt, ctx, word(rt, word(rt, entry) + 4u), {entry});
            }
            ++i;
        }
        return i;
    };
    u32 table = word(rt, self + 4u);
    if (table != 0u) {
        if (word(rt, self + 8u) == 0u) {
            v0 = call_set(rt, ctx, 0x08B61FC0u, {table});
        } else {
            const u32 passes = release_entries(4u);
            v0 = passes << 2;
            table = word(rt, self + 4u);
            if (table != 0u) v0 = call_set(rt, ctx, 0x08B61FC0u, {table});
        }
    }
    table = word(rt, self + 0xCu);
    u32 members = 0u;
    if (table != 0u) {
        if (word(rt, self + 8u) == 0u) {
            v0 = call_set(rt, ctx, 0x08B61FC0u, {table});
        } else {
            const u32 passes = release_entries(0xCu);
            (void)passes;
            v0 = 0u;
            table = word(rt, self + 0xCu);
            if (table != 0u) v0 = call_set(rt, ctx, 0x08B61FC0u, {table});
        }
    }
    members = word(rt, self + 0x10u);
    if (members == 0u) {
        ctx.gpr[2] = v0;
        return false;
    }
    u32 offset = 0u;
    do {
        const u32 item = word(rt, members + offset);
        offset += 4u;
        if (item != 0u) {
            v0 = call_set(rt, ctx, 0x08B61FC0u, {item});
            members = word(rt, self + 0x10u);
        }
        v0 = members + offset;
    } while (offset != 0x18u);
    if (members == 0u) {
        ctx.gpr[2] = v0;
        return false;
    }
    tail_argument = members;
    return true;
}

// 0x089C5E4C (a0 = the destination object): reads a handle through 0x08A9DD14 (the string 0x08CA6398 reports a missing one), converts it with
// 0x08A9BC30 into a name, builds three temporaries on the frame with 0x08A69B3C / 0x08A7DE18 / 0x08A69D68, and assigns the results (0x08A7C0E0 /
// 0x08977AE4 fill a temporary from the constant 0x08CF6ED8; 0x08A5BAD8 copies it over the member at +0x3C / +0x40 / +0x38 unless it is the same
// address; 0x08A5B9EC releases it). v0 = the last callee's.
void rebuild_name_members(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x24u}, {19u, 0x2Cu}, {16u, 0x20u}, {31u, 0x34u}, {20u, 0x30u}, {18u, 0x28u}});
    constexpr u32 kSource = 0x08CF6ED8u;
    const u32 handle_slot = frame.at(0x14u);
    const u32 name_slot = frame.at(0x18u);
    call_set(rt, ctx, 0x08A9DD14u, {handle_slot});
    const u32 handle = word(rt, handle_slot);
    if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6398u});
    const u32 name = call_set(rt, ctx, 0x08A9BC30u, {handle});
    call_set(rt, ctx, 0x08A7C0F0u, {name_slot, name});
    call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
    // Two temporaries built from the name and a constant string each.
    for (const auto &[slot, text] : {std::pair<u32, u32>{frame.at(0x10u), 0x08CA63ACu}, std::pair<u32, u32>{frame.at(0xCu), 0x08CA63BCu}}) {
        const u32 name_object = word(rt, name_slot);
        if (name_object == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA6384u});
        call_set(rt, ctx, 0x08A69B3Cu, {slot, text});
        call_set(rt, ctx, 0x08A7DE18u, {name_object, slot});
        call_set(rt, ctx, 0x08A69D68u, {slot});
    }
    const auto assign_member = [&](u32 member, u32 temporary, u32 builder) {
        call_set(rt, ctx, builder, {temporary, kSource});
        if (member != temporary) call_set(rt, ctx, 0x08A5BAD8u, {member, word(rt, temporary)});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
    };
    assign_member(self + 0x3Cu, frame.at(0x8u), 0x08A7C0E0u);
    assign_member(self + 0x40u, frame.at(0x4u), 0x08A7C0E0u);
    assign_member(self + 0x38u, frame.at(0x0u), 0x08977AE4u);
    call_set(rt, ctx, 0x08A5BC6Cu, {});
    call_set(rt, ctx, 0x08A5B9ECu, {name_slot});
}

// 0x088BB780 (a0 = a curve object, f12 = x, result f0, v0 = 0x08CE0000): three one-time setups (a flag byte at 0x08CE3088 / 0x08CE3080 / 0x08CE3078 that
// is clear and answers 0x08B6441C(flag) with a nonzero value: 0x08CE30A0 = 0, 0x08CE309C = 0x41DE38E4, 0x08CE3098 = 0x425E38E4, each followed by
// 0x08B64494(flag)), then a piecewise-linear ramp over the thresholds t1 = 0x08CE30A0 < t2 = 0x08CE309C < t3 = 0x08CE3098 with the curve's values
// a = [+0x88], b = [+0x8C], c = [+0x90]: x <= t1 gives a, up to t2 a ramp to b, up to 125.0004 (0x42FA0001) a ramp to c, above that c.
void ramp_between_values(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 curve = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {16u, 0x0u}, {31u, 0x8u}, {55u, 0x1Cu}, {54u, 0x18u}, {53u, 0x14u}, {52u, 0x10u}});
    const float x = ctx.fpr[12];
    const float c = float_at(rt, curve + 0x90u);
    float result = float_at(rt, curve + 0x88u);
    const float b = float_at(rt, curve + 0x8Cu);
    const auto setup = [&](u32 flag, u32 target, u32 bits) {
        if (signed_byte(rt, flag) != 0u) return;
        if (call_set(rt, ctx, 0x08B6441Cu, {flag}) == 0u) return;
        put(rt, target, bits);
        call_set(rt, ctx, 0x08B64494u, {flag});
    };
    setup(0x08CE3088u, 0x08CE30A0u, 0u);
    setup(0x08CE3080u, 0x08CE309Cu, 0x41DE38E4u);
    setup(0x08CE3078u, 0x08CE3098u, 0x425E38E4u);
    const float t1 = float_at(rt, 0x08CE30A0u);
    if (!(x <= t1)) {
        const float t2 = float_at(rt, 0x08CE309Cu);
        if (x <= t2) {
            result += psp_mul(psp_mul(x - t1, b - result), 1.0f / (t2 - t1));
        } else {
            result = c;
            if (x <= std::bit_cast<float>(0x42FA0001u)) {
                const float t3 = float_at(rt, 0x08CE3098u);
                result = b + psp_mul(psp_mul(x - t2, c - b), 1.0f / (t3 - t2));
            }
        }
    }
    ctx.gpr[2] = 0x08CE0000u;
    ctx.fpr[0] = result;
}

// 0x088B723C (a0 = the owner, f12 = a value): the owner keeps a heap of up to three 0x48-byte entries from +0xA4 (their pointers in the table at +0x17C, the
// count at +0x188). With room (count < 3) the entry for the count gets f12 in its word +0x44, its pointer is appended and the count grows; when that makes 3
// the pointer table is heapified with 0x08BB1A00(table, 0, 3, table[0], 0x088B5D38). Full: the top pointer is tested with the comparator 0x088B5D38(frame
// record {0, f12} at +0x40 / +0x44, top); when it answers nonzero the last slot (+0x184) takes the top, the table is sifted with 0x08BB1A00(table, 0, 2,
// old last, comparator), the new top gets f12 in its +0x44 and 0x08BB191C(table, 2, 0, last, comparator) settles it. v0 = the entry that took the value, or 0.
void push_value_into_heap(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const float value = ctx.fpr[12];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x54u}, {52u, 0x70u}, {31u, 0x6Cu}, {22u, 0x68u}, {21u, 0x64u}, {20u, 0x60u}, {19u, 0x5Cu}, {18u, 0x58u}, {16u, 0x50u}});
    constexpr u32 kComparator = 0x088B5D38u;
    const u32 table = self + 0x17Cu;
    u32 entry = 0u;
    const u32 count = word(rt, self + 0x188u);
    if (count < 3u) {
        entry = self + 0xA4u + count * 0x48u;
        put_float(rt, entry + 0x44u, value);
        put(rt, table + (word(rt, self + 0x188u) << 2), entry);
        const u32 grown = word(rt, self + 0x188u) + 1u;
        put(rt, self + 0x188u, grown);
        if (grown == 3u) {
            // (self + 0x188 - table) / 4 = 3 slots; the first parent is (3 - 2) / 2 = 0.
            const u32 size = 3u;
            const u32 parent = (size - 2u) / 2u;
            u32 node = table + (parent << 2);
            for (u32 i = 0;; ++i) {
                call_set(rt, ctx, 0x08BB1A00u, {table, parent - i, size, word(rt, node), kComparator});
                node -= 4u;
                if (parent == i) break;
            }
        }
    } else {
        const u32 top = word(rt, table);
        put(rt, frame.at(0x40u), 0u);
        put_float(rt, frame.at(0x44u), value);
        if (call_set(rt, ctx, kComparator, {frame.at(0u), top}) != 0u) {
            const u32 last = word(rt, self + 0x184u);
            put(rt, self + 0x184u, top);
            call_set(rt, ctx, 0x08BB1A00u, {table, 0u, (self + 0x184u - table) >> 2, last, kComparator});
            entry = word(rt, table + 8u);
            put_float(rt, entry + 0x44u, value);
            call_set(rt, ctx, 0x08BB191Cu, {table, ((self + 0x188u - table) >> 2) - 1u, 0u, word(rt, self + 0x184u), kComparator});
        }
    }
    ctx.gpr[2] = entry;
}

// 0x0893CA3C (a0 = the object; a1.. unused): for each of the four slots i (0-3) with the object's records s6 = [+8], s7 = [+0xC] and the flag 0x089557BC(object):
// when the flag is set 0x08955300(object, i) gives a record whose float +0xC8 becomes 3.0; otherwise with P = 0x08955300(object, i) and r = 0x0893C91C(object, i)'s
// float, the record's +0xBC + +0xC4 grows by 0x0893C80C(object, i, 2, f12 = r) and the corrections (s6[+0x6C] * 0, s5[+0x318] * 0x08B499B8(s6[+0x700]),
// minus s5[+0x318] * s6[+0x3C], with s5 = s7 + (i / 2) * 4) give P[+0xC8] = ((f - sum) * s6[+0x6C] / P[+0xD0] - s5[+0x318] + s7[(i / 2) * 29 + 0xDB]) - r,
// f being 0x0893C80C's float result. v0 = 4.
void update_slot_records(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x20u}, {19u, 0xCu}, {18u, 0x8u}, {57u, 0x44u}, {56u, 0x40u}, {31u, 0x24u}, {23u, 0x1Cu}, {22u, 0x18u}, {21u, 0x14u}, {20u, 0x10u}, {17u, 0x4u}, {16u, 0x0u}, {55u, 0x3Cu}, {54u, 0x38u}, {53u, 0x34u}, {52u, 0x30u}});
    const float zero = 0.0f;
    const float three = 3.0f;
    const u32 block6 = word(rt, object + 0x8u);
    const u32 block7 = word(rt, object + 0xCu);
    const u32 flag = call_set(rt, ctx, 0x089557BCu, {object});
    for (u32 i = 0; i != 4u; ) {
        const u32 pair = i >> 1;
        const u32 pair_offset = pair << 2;
        const u32 row = pair_offset + block7;
        if (flag != 0u) {
            const u32 record = call_set(rt, ctx, 0x08955300u, {object, i});
            ++i;
            put_float(rt, record + 0xC8u, three);
            continue;
        }
        const u32 record = call_set(rt, ctx, 0x08955300u, {object, i});
        const float measured = call_float(rt, ctx, 0x0893C91Cu, {object, i}, {});
        float sum = float_at(rt, record + 0xBCu);
        sum = sum + float_at(rt, record + 0xC4u);
        const float adjusted = call_float(rt, ctx, 0x0893C80Cu, {object, i, 2u}, {measured});
        float target = adjusted;
        float scale = float_at(rt, row + 0x318u);
        const float curve = call_float(rt, ctx, 0x08B499B8u, {}, {float_at(rt, block6 + 0x700u)});
        const float rate = float_at(rt, block6 + 0x6Cu);
        const float scaled_curve = psp_mul(scale, curve);
        const float limit = float_at(rt, block6 + 0x3Cu);
        const float rate_zero = psp_mul(rate, zero);
        const float divisor = float_at(rt, record + 0xD0u);
        const float scaled_limit = psp_mul(scale, limit);
        sum = sum + rate_zero;
        const float again = float_at(rt, row + 0x318u);
        const u32 index = (((pair << 5) - pair_offset) + pair) << 2;
        sum = sum + scaled_curve;
        const float table_value = float_at(rt, index + block7 + 0x36Cu);
        sum = sum - scaled_limit;
        target = target - sum;
        target = psp_mul(target, rate);
        target = target / divisor;
        target = target - again;
        target = target + table_value;
        target = target - measured;
        put_float(rt, record + 0xC8u, target);
        ++i;
    }
    ctx.gpr[2] = 4u;
}

// 0x08BE03BC (a0 = the object): the destructor of an object holding three shared holders (+0x44, +0x10, +8; use count +4, weak count +8). Writes the vtable
// 0x08CC6850, releases the holder at +0x44, writes the vtable 0x08CC6838, runs 0x08B28DC4(object + 0x18), releases the holder at +0x10 and finally the one at
// +8, whose last weak-count drop is a tail call of its virtual +0xC made after the frame is gone (v0 = its vtable, $t9 = the target then).
// Returns true when that tail call is due, with the holder in `tail_holder`.
bool destroy_holder_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 &tail_holder) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x4u}, {31u, 0x8u}, {16u, 0x0u}});
    put(rt, self, 0x08CC6850u);
    const u32 first = word(rt, self + 0x44u);
    if (first != 0u) release_holder(rt, ctx, first);
    put(rt, self, 0x08CC6838u);
    call_set(rt, ctx, 0x08B28DC4u, {self + 0x18u});
    const u32 second = word(rt, self + 0x10u);
    if (second != 0u) release_holder(rt, ctx, second);
    const u32 third = word(rt, self + 8u);
    if (third == 0u) return false;
    if (!decrement_reaches_zero(rt, ctx, third + 4u)) return false;
    call_indirect(rt, ctx, word(rt, word(rt, third) + 8u), {third});
    if (!decrement_reaches_zero(rt, ctx, third + 8u)) return false;
    tail_holder = third;
    return true;
}

// Appends two command words (a 0x5C or 0x5D command and its 24-bit / 8-bit operand) to the graphics list at the write pointer.
void emit_color_words(psprecomp::Runtime &rt, u32 low24_source, u32 high8_source) {
    const u32 list = word(rt, kGuListPointer);
    put(rt, list, 0x5C000000u | (low24_source & 0x00FFFFFFu));
    put(rt, list + 4u, 0x5D000000u | (high8_source & 0xFFu));
    put(rt, kGuListPointer, list + 8u);
}

// 0x08B5358C (a0 = the record): after 0x08B59838(record), when the flags halfword +0x16 has bit 2 or 4 (4 and 16 picks the color word 0x08CE0DCC, else
// 0x08CE0DC8) two color commands are written to the graphics list; then the record (a negative flags halfword) or the one 0x08B54D28(0x08D695C8, record) finds is
// handed to 0x08B550A8; the same colors are written again from the state block (+0x244 and the byte +0x247). The word +0x1C selects 0x08B57154(0x65 / 0x66 /
// 0x67 / 0x68) for its bits 0-3 (read again after each); a nonzero (word +0x18 & 0x1E) calls 0x08B571AC; 0x08B5711C(0x1DC0) runs and the function ends with the
// tail call 0x08B57138(0xFFFF), which is `tail`, made after the frame is gone.
void write_record_state(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {17u, 0x4u}, {31u, 0x8u}});
    call_set(rt, ctx, 0x08B59838u, {self});
    const u32 flags = signed_half(rt, self + 0x16u) & 0xFFFFu;
    const bool colored = (flags & 0x14u) != 0u;
    if (colored) {
        const u32 color = word(rt, (flags & 0x10u) != 0u ? 0x08CE0DCCu : 0x08CE0DC8u);
        emit_color_words(rt, color, color >> 24);
    }
    if (static_cast<s32>(signed_half(rt, self + 0x16u)) >= 0) {
        const u32 found = call_set(rt, ctx, 0x08B54D28u, {0x08D695C8u, self});
        call_set(rt, ctx, 0x08B550A8u, {found});
    } else {
        call_set(rt, ctx, 0x08B550A8u, {word(rt, self)});
    }
    if (colored) {
        // The byte +0x247 is read after the first word is stored (the list may overlap the state block).
        const u32 low = word(rt, kGuState + 0x244u);
        const u32 list = word(rt, kGuListPointer);
        put(rt, list, 0x5C000000u | (low & 0x00FFFFFFu));
        put(rt, list + 4u, 0x5D000000u | static_cast<u32>(rt.memory().load8(kGuState + 0x247u)));
        put(rt, kGuListPointer, list + 8u);
    }
    u32 bits = word(rt, self + 0x1Cu);
    if ((bits & 0xFu) != 0u) {
        for (const u32 bit : {1u, 2u, 4u, 8u}) {
            if ((bits & bit) == 0u) continue;
            call_set(rt, ctx, 0x08B57154u, {0x64u + (bit == 1u ? 1u : bit == 2u ? 2u : bit == 4u ? 3u : 4u)});
            bits = word(rt, self + 0x1Cu);
        }
    }
    const u32 enabled = word(rt, self + 0x18u) & 0x1Eu;
    if (enabled != 0u) call_set(rt, ctx, 0x08B571ACu, {enabled});
    call_set(rt, ctx, 0x08B5711Cu, {0x1DC0u});
}

// 0x08C76E74 (a0 = a buffered input stream, a1 = a character or -1): pushes a character back. Fails with -1 unless the stream's mode word +0x2C has bit 8 and the
// byte +0x82 is clear. The previous character is the byte before the read position +8 when the position is past the start +4 (the position moves back), else the
// one the virtual +0x10 (the frame record at +0x20, then the virtual +0x24) gives (-1 from either is a failure). Then -1 as the character gives 0, the character
// matching the previous one gives itself, a stream already in pushback mode (byte +0x8C) gives -1; otherwise the pushback buffer (+0x83) takes over the stream
// bookkeeping (+0x84 / +0x88 / +0xC) and the character is stored. v0 = the answer.
void push_back_character(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 character = ctx.gpr[5];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x44u}, {16u, 0x40u}, {31u, 0x50u}, {19u, 0x4Cu}, {18u, 0x48u}});
    const auto finish = [&](u32 value) {
        ctx.gpr[2] = value;
        ctx.gpr[3] = value;
    };
    if ((word(rt, self + 0x2Cu) & 8u) == 0u || rt.memory().load8(self + 0x82u) != 0u) return finish(0xFFFFFFFFu);
    const u32 begin = word(rt, self + 4u);
    const u32 position = word(rt, self + 8u);
    const u32 pushback_active = rt.memory().load8(self + 0x8Cu);
    u32 previous;
    if (begin < position) {
        put(rt, self + 8u, position - 1u);
        previous = rt.memory().load8(position - 1u);
    } else {
        const u32 vtable = word(rt, self);
        ctx.gpr[2] = vtable;
        ctx.gpr[3] = word(rt, vtable + 0x10u);
        call_set(rt, ctx, ctx.gpr[3], {frame.at(0x20u), self, 0xFFFFFFFFu, 0xFFFFFFFFu, 1u, 0x18u});
        if (word(rt, frame.at(0x20u)) == 0xFFFFFFFFu && word(rt, frame.at(0x24u)) == 0xFFFFFFFFu) return finish(0xFFFFFFFFu);
        previous = call_indirect(rt, ctx, word(rt, word(rt, self) + 0x24u), {self});
        if (previous == 0xFFFFFFFFu) return finish(0xFFFFFFFFu);
    }
    if (character == 0xFFFFFFFFu) return finish(0u);
    if (character == previous) return finish(character);
    if (pushback_active != 0u) return finish(0xFFFFFFFFu);
    if (rt.memory().load8(self + 0x8Cu) != 0u) {
        const u32 target = word(rt, self + 8u);
        rt.memory().store8(self + 0x81u, 1u);
        rt.memory().store8(target, static_cast<std::uint8_t>(character));
        return finish(character);
    }
    const u32 read_position = word(rt, self + 8u);
    const u32 saved_limit = word(rt, self + 0xCu);
    const u32 buffer = self + 0x83u;
    put(rt, self + 0x88u, saved_limit);
    put(rt, self + 0x84u, read_position);
    put(rt, self + 0xCu, self + 0x84u);
    rt.memory().store8(self + 0x8Cu, 1u);
    put(rt, self + 4u, buffer);
    put(rt, self + 8u, buffer);
    rt.memory().store8(self + 0x81u, 1u);
    rt.memory().store8(buffer, static_cast<std::uint8_t>(character));
    return finish(character);
}

// 0x08A04570 (a0 = the destination string, a2 = mode, a3 = a pointer to a pointer): after 0x08A03E58 fills the handle on the frame (+0x10; the string
// 0x08CA8D40 reports a missing one) and mode 0, 0x08A0AD70(temporary, handle + 0x20) and 0x08A6D2D4(temporary 2, temporary) build a string that is copied over the destination
// (0x08A5BAD8, unless it is the destination itself) and released (0x08A5B9EC); mode 1 takes *a3 (0x08CA8D38 when missing), asks its virtual +0xC for a value, builds the
// second temporary with 0x08A0B0CC and copies it over the handle's +0x20 (0x089641C4 and the word +0x10). Each temporary's buffer is then released (a count test with
// 0x08B60804, the block freed with 0x08B8542C when the count reaches zero) and the handle is released. v0 = the last call's.
void convert_with_handle(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[4];
    const u32 mode = ctx.gpr[6];
    const u32 indirect = ctx.gpr[7];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x50u}, {18u, 0x48u}, {16u, 0x40u}, {17u, 0x44u}, {31u, 0x54u}, {19u, 0x4Cu}});
    const u32 handle_slot = frame.at(0x10u);
    call_set(rt, ctx, 0x08A03E58u, {handle_slot, ctx.gpr[5]});  // a1 is passed through untouched
    const auto release_buffer = [&](u32 slot, u32 scratch) {
        const u32 buffer = word(rt, slot);
        const u32 header = buffer - 0xCu;
        if (header == 0x08D74FD0u) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {buffer - 4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, scratch});
    };
    if (mode == 0u) {
        const u32 handle = word(rt, handle_slot);
        if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8D40u});
        call_set(rt, ctx, 0x08A0AD70u, {frame.at(8u), handle + 0x20u});
        call_set(rt, ctx, 0x08A6D2D4u, {frame.at(0xCu), frame.at(8u)});
        if (destination != frame.at(0xCu)) call_set(rt, ctx, 0x08A5BAD8u, {destination, word(rt, frame.at(0xCu))});
        call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0xCu)});
        release_buffer(frame.at(8u), frame.at(1u));
    } else if (mode == 1u) {
        const u32 handle = word(rt, handle_slot);
        if (handle == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8D40u});
        const u32 source = word(rt, indirect);
        if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA8D38u});
        const u32 vtable = word(rt, source);
        ctx.gpr[2] = vtable;
        ctx.gpr[3] = word(rt, vtable + 0xCu);
        call_set(rt, ctx, ctx.gpr[3], {frame.at(4u), source});
        const u32 member = handle + 0x20u;
        call_set(rt, ctx, 0x08A0B0CCu, {frame.at(0x20u), frame.at(4u)});
        if (member != frame.at(0x20u)) {
            call_set(rt, ctx, 0x089641C4u, {member, frame.at(0x20u)});
            put(rt, member + 0x10u, word(rt, frame.at(0x30u)));
        }
        release_buffer(frame.at(4u), frame.at(0u));
    }
    call_set(rt, ctx, 0x08A5B9ECu, {handle_slot});
}

// 0x0889AB88 (a0 = an object, a1 = a side index, a2 = an optional table of limits at +0x58 / +0x5C): a pair of virtuals gives a holder (+0x44) and a part
// (+0x40 with the index); both must exist. The holder's virtual +0x18 fills three floats on the frame; with a table the first limit (+0x58, or +0x5C for
// an index of 2 and up) when nonzero lowers the second float to limit - the part's virtual +0x2C if that is smaller. The three floats go to 0x08B56364;
// then the holder's virtuals +0x14 and +0x10 (times 57.2958) feed 0x08B5606C and 0x08B55FB0 (the second one signed by the index parity), the part's virtual
// +0x28 (on the part, times 0.5, signed likewise) feeds 0x08B56364 again with two zeros. v0 = the last call's (the part's, when either object is missing).
void aim_through_virtuals(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 index = ctx.gpr[5];
    const u32 table = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {52u, 0x30u}});
    const bool odd = (index & 1u) != 0u;
    const u32 holder = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x44u), {object});
    const u32 part = call_indirect(rt, ctx, word(rt, word(rt, object) + 0x40u), {object, index});
    if (holder == 0u || part == 0u) return;
    call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x18u), {holder, frame.at(0u)});
    if (table != 0u) {
        const float limit = float_at(rt, table + 0x58u + (static_cast<s32>(index) >= 2 ? 4u : 0u));
        if (limit != 0.0f) {
            const float reading = call_indirect(rt, ctx, word(rt, word(rt, part) + 0x2Cu), {part}, {}).f0;
            const float margin = limit - reading;
            if (margin < float_at(rt, frame.at(4u))) put_float(rt, frame.at(4u), margin);
        }
    }
    const float degrees = std::bit_cast<float>(0x42652EE0u);
    const float second = float_at(rt, frame.at(4u));
    const float third = float_at(rt, frame.at(8u));
    call_float(rt, ctx, 0x08B56364u, {}, {float_at(rt, frame.at(0u)), second, third});
    const float first_angle = call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x14u), {holder}, {}).f0;
    call_float(rt, ctx, 0x08B5606Cu, {}, {psp_mul(first_angle, degrees)});
    const float second_angle = psp_mul(call_indirect(rt, ctx, word(rt, word(rt, holder) + 0x10u), {holder}, {}).f0, degrees);
    call_float(rt, ctx, 0x08B55FB0u, {}, {psp_mul(odd ? -1.0f : 1.0f, second_angle)});
    const float spin = call_indirect(rt, ctx, word(rt, word(rt, part) + 0x28u), {part}, {}).f0;
    call_float(rt, ctx, 0x08B56364u, {}, {psp_mul(odd ? 0.5f : -0.5f, spin), 0.0f, 0.0f});
}

// 0x089CE23C (a0 = the object, a1 = a key): with the flag 0x10 in the word +0x14C the scale f20 = 0x089CD978 / 0x089CD19C and the offset f21 = 0x089CD910 * [+0x140] + [+0x124]
// apply when 0x089CD19C(object) is above 0 (else 1.0 and 0.0). When 0x089CE03C(object, key) accepts, the record 0x089CE1F4(object, key) names a target (its word +0xC, or
// the object's +0x154; none gives 0): its four floats (+0x10 shifted by f21, +0x14, +0x18, +0x1C, scaled by f20) go to 0x0896A9C4(target, frame ...) and the target's
// word +0xE0 gets bit 14 = (record flags +0x20 & 0x30000 == 0x30000), then 0x089687D8 / 0x08968850 get the flag bits 19 and 20. v0 = the target or 0.
void apply_record_to_target(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 key = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}, {53u, 0x24u}, {52u, 0x20u}});
    float scale = 1.0f;
    float offset = 0.0f;
    if ((word(rt, self + 0x14Cu) & 0x10u) != 0u) {
        const float basis = call_float(rt, ctx, 0x089CD19Cu, {self}, {});
        if (!(basis <= 0.0f)) {
            scale = call_float(rt, ctx, 0x089CD978u, {self}, {});
            const float divisor = call_float(rt, ctx, 0x089CD19Cu, {self}, {});
            scale = scale / divisor;
            const float gain = call_float(rt, ctx, 0x089CD910u, {self}, {});
            offset = psp_mul(gain, float_at(rt, self + 0x140u)) + float_at(rt, self + 0x124u);
        }
    }
    if (call_set(rt, ctx, 0x089CE03Cu, {self, key}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    const u32 record = call_set(rt, ctx, 0x089CE1F4u, {self, key});
    u32 target = word(rt, record + 0xCu);
    if (target == 0u) {
        target = word(rt, self + 0x154u);
        if (target == 0u) {
            ctx.gpr[2] = 0u;
            return;
        }
    }
    float a = float_at(rt, record + 0x10u);
    float b = float_at(rt, record + 0x14u);
    float c = float_at(rt, record + 0x18u);
    a = a - offset;
    b = psp_mul(scale, b);
    float d = float_at(rt, record + 0x1Cu);
    a = psp_mul(scale, a);
    c = psp_mul(scale, c);
    b = b + 0.0f;
    d = psp_mul(scale, d);
    a = offset + a;
    put_float(rt, frame.at(0xCu), a);
    put_float(rt, frame.at(8u), b);
    put_float(rt, frame.at(4u), c);
    put_float(rt, frame.at(0u), d);
    call_set(rt, ctx, 0x0896A9C4u, {target, frame.at(0xCu), frame.at(8u), frame.at(4u), frame.at(0u)});
    const u32 flags = word(rt, record + 0x20u);
    const u32 word_e0 = word(rt, target + 0xE0u);
    put(rt, target + 0xE0u, ins(word_e0, ((flags & 0x30000u) - 0x30000u) < 1u ? 1u : 0u, 14u));
    call_set(rt, ctx, 0x089687D8u, {target, (word(rt, record + 0x20u) >> 19) & 1u});
    call_set(rt, ctx, 0x08968850u, {target, (word(rt, record + 0x20u) >> 20) & 1u});
    ctx.gpr[2] = target;
}

// 0x089A7C80 (a0 = the object, a1 = a value): waits until 0x089A717C(object) answers nonzero, then until 0x089A6EA4(object) does (each retry runs 0x08B1A844 and
// 0x089C4558(1)); 0x089A6EA4 must then answer 1 and 0x089A6F38 0 (else that answer is returned). With the member +0x2C set, a vector of three strings is built on the frame
// (0x08A76AA0 for each, the pairs 0x08A76AAC(value) / 0x08A76AAC(object) copied into the second and third with 0x08A5BAD8), the member's virtual +0xCC(member, slot 0, 2, slot 1)
// runs (0x08CA4F34 reports a missing member) and the strings are released (0x08A5B9EC). v0 = 0x089A6F84(object, 1) or the answer that stopped it.
void run_object_with_strings(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 value = ctx.gpr[5];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x28u}, {16u, 0x20u}, {31u, 0x38u}, {21u, 0x34u}, {20u, 0x30u}, {19u, 0x2Cu}, {17u, 0x24u}});
    if (call_set(rt, ctx, 0x089A717Cu, {self}) == 0u) return;
    while (call_set(rt, ctx, 0x089A6EA4u, {self}) == 0u) {
        call_set(rt, ctx, 0x08B1A844u, {});
        call_set(rt, ctx, 0x089C4558u, {1u});
    }
    if (call_set(rt, ctx, 0x089A6EA4u, {self}) != 1u) return;
    if (call_set(rt, ctx, 0x089A6F38u, {self}) != 0u) return;
    if (word(rt, self + 0x2Cu) != 0u) {
        const u32 first = frame.at(0x10u);
        const u32 second = frame.at(0x14u);
        const u32 end = frame.at(0x1Cu);
        call_set(rt, ctx, 0x08A76AA0u, {first});
        for (u32 slot = second; slot != end; slot += 4u) call_set(rt, ctx, 0x08A76AA0u, {slot});
        const u32 scratch = frame.at(0xCu);
        put(rt, frame.at(8u), value);
        call_set(rt, ctx, 0x08A76AACu, {scratch, frame.at(8u)});
        if (second != scratch) call_set(rt, ctx, 0x08A5BAD8u, {second, word(rt, scratch)});
        call_set(rt, ctx, 0x08A5B9ECu, {scratch});
        const u32 scratch2 = frame.at(4u);
        put(rt, frame.at(0u), self);
        call_set(rt, ctx, 0x08A76AACu, {scratch2, frame.at(0u)});
        if (frame.at(0x18u) != scratch2) call_set(rt, ctx, 0x08A5BAD8u, {frame.at(0x18u), word(rt, scratch2)});
        call_set(rt, ctx, 0x08A5B9ECu, {scratch2});
        const u32 member = word(rt, self + 0x2Cu);
        if (member == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA4F34u});
        const u32 vtable = word(rt, member);
        ctx.gpr[2] = vtable;
        ctx.gpr[3] = word(rt, vtable + 0xCCu);
        call_set(rt, ctx, ctx.gpr[3], {member, first, 2u, second});
        for (u32 slot = end - 4u;; slot -= 4u) {
            call_set(rt, ctx, 0x08A5B9ECu, {slot});
            if (slot == second) break;
        }
        call_set(rt, ctx, 0x08A5B9ECu, {first});
    }
    call_set(rt, ctx, 0x089A6F84u, {self, 1u});
}


} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08BE03BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 holder = 0u;
    if (destroy_holder_object(rt, ctx, holder)) {
        const u32 vtable = word(rt, holder);
        ctx.gpr[2] = vtable;
        ctx.gpr[25] = word(rt, vtable + 0xCu);
        call_set(rt, ctx, ctx.gpr[25], {holder});
    }
}
// 0x08C78CC0 (a0 = the 32-byte result, a1 = the object, a2 = an 8-word request; v0 = a0): the result starts as (-1, -1, 0 x 6). The request is copied to frame + 0x18 / +0x1C (words 0 / 1) and
// frame + 0x20 ... +0x34 (words 2-7). When 0x08B61534(object + 0x24) answers 0 that is the result. Otherwise, if the object's byte +0x8C is set the object's cursor words are rotated first:
// +4 = word(+0x78), +8 = +0x84 = word(+8) xor word(+4) != 0 ? 1 : 0, plus word(+0x84), +0xC = word(+0x88), byte +0x8C = 0. Then the request words 2-7 go to frame + 0 ... +0x14 and
// 0x08C77B74(frame + 0x40, object, word0, word1, 0, frame) fills the answer at frame + 0x40 (8 words), which is copied to the result (word 1 first, then 0, then 2-7).
void func_08C78CC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4];
    const u32 object = ctx.gpr[5];
    const u32 request = ctx.gpr[6];
    GuestFrame frame(ctx, 0x80u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x6Cu}, {18u, 0x68u}, {17u, 0x64u}, {16u, 0x60u}, {31u, 0x70u}});
    ctx.gpr[17] = object;
    ctx.gpr[16] = result;
    u32 words[8];
    for (u32 i = 0; i < 8u; ++i) words[i] = word(rt, request + i * 4u);
    put(rt, frame.at(0x18u), words[0]);
    put(rt, frame.at(0x1Cu), words[1]);
    put(rt, result, 0xFFFFFFFFu);
    put(rt, result + 4u, 0xFFFFFFFFu);
    for (u32 i = 2; i < 8u; ++i) put(rt, result + i * 4u, 0u);
    const u32 first = word(rt, frame.at(0x18u));
    ctx.gpr[18] = first;
    for (u32 i = 2; i < 8u; ++i) put(rt, frame.at(0x18u + i * 4u), words[i]);
    const u32 second = word(rt, frame.at(0x1Cu));
    ctx.gpr[19] = second;
    ctx.gpr[2] = call_set(rt, ctx, 0x08B61534u, {object + 0x24u});
    if (ctx.gpr[2] == 0u) {
        ctx.gpr[2] = result;
        return;
    }
    if (load8(object + 0x8Cu) != 0u) {
        const u32 current = word(rt, object + 8u);
        const u32 previous = word(rt, object + 4u);
        const u32 base = word(rt, object + 0x84u);
        const u32 third = word(rt, object + 0x88u);
        const u32 next_pointer = word(rt, object + 0x78u);
        const u32 advanced = ((current ^ previous) != 0u ? 1u : 0u) + base;
        put(rt, object + 4u, next_pointer);
        put(rt, object + 8u, advanced);
        put(rt, object + 0xCu, third);
        put(rt, object + 0x84u, advanced);
        store8(object + 0x8Cu, 0u);
    }
    for (u32 i = 0; i < 6u; ++i) put(rt, frame.at(i * 4u), word(rt, frame.at(0x20u + i * 4u)));
    call_set(rt, ctx, 0x08C77B74u, {frame.at(0x40u), object, first, second, 0u, frame.at(0u)});
    const u32 answer_second = word(rt, frame.at(0x44u));
    u32 rest[6];
    for (u32 i = 0; i < 6u; ++i) rest[i] = word(rt, frame.at(0x48u + i * 4u));
    const u32 answer_first = word(rt, frame.at(0x40u));
    put(rt, result + 4u, answer_second);
    put(rt, result, answer_first);
    for (u32 i = 0; i < 6u; ++i) put(rt, result + 8u + i * 4u, rest[i]);
    ctx.gpr[2] = result;
}
void func_0889AB88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    aim_through_virtuals(rt, ctx);
}
void func_0893CA3C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    update_slot_records(rt, ctx);
}
void func_089CE23C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    apply_record_to_target(rt, ctx);
}
void func_08B5358C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    write_record_state(rt, ctx);
    call_set(rt, ctx, 0x08B57138u, {0xFFFFu});
}
void func_08BD44C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x80u); put(rt, frame.at(0x68u), G[18]); put(rt, frame.at(0x64u), G[17]); put(rt, frame.at(0x6Cu), G[19]); put(rt, frame.at(0x7Cu), G[31]); put(rt, frame.at(0x78u), G[22]); put(rt, frame.at(0x74u), G[21]); put(rt, frame.at(0x70u), G[20]); put(rt, frame.at(0x60u), G[16]); auto &F = ctx.fpr; G[18] = G[4] + 0xCu; G[17] = G[4]; G[4] = G[18]; G[19] = G[5]; call_here(rt, ctx, 0x08909B30u); G[2] = 0x3u; G[3] = word(rt, G[17] + 0x20u); G[5] = G[17]; { const bool tk = G[3] != G[2]; G[4] = G[19]; if (tk) goto L_08BD4520; } F[12] = float_at(rt, G[17] + 0x28u); call_here(rt, ctx, 0x08AF2978u); goto L_08BD4630; L_08BD4520: ; G[2] = 0x2u; G[6] = 0u; G[4] = frame.at(0u); { const bool tk = G[3] == G[2]; G[5] = G[19]; if (tk) goto L_08BD4544; } G[4] = G[17]; call_here(rt, ctx, 0x08AF213Cu); goto L_08BD4630; L_08BD4544: ; call_here(rt, ctx, 0x08BD2E04u); G[5] = sext(load8(G[17] + 0x24u), 8); G[4] = G[19]; G[22] = G[2]; G[16] = word(rt, G[17] + 0x4u); call_here(rt, ctx, 0x08BD2DDCu); G[4] = G[18]; G[20] = G[2]; call_here(rt, ctx, 0x089099A0u); { const bool tk = G[2] == 0u; G[3] = G[2]; if (tk) goto L_08BD462C; } G[2] = 0x1u; G[4] = G[18]; G[5] = 0u; { const bool tk = G[3] != G[2]; G[21] = word(rt, G[16] + 0x0u); if (tk) goto L_08BD45C8; } call_here(rt, ctx, 0x089099FCu); G[5] = word(rt, G[21] + 0x30u); G[3] = G[2] << 6; G[2] = G[2] << 4; G[3] = G[3] - G[2]; G[5] = G[5] + G[3]; G[4] = G[22]; G[5] = G[5] + 0x4u; call_here(rt, ctx, 0x08906304u); F[12] = F[0]; G[4] = G[19]; G[5] = G[17]; call_here(rt, ctx, 0x08AF2978u); goto L_08BD4630; L_08BD45C8: ; G[4] = G[18]; call_here(rt, ctx, 0x08909A28u); G[4] = G[18]; G[16] = G[2]; call_here(rt, ctx, 0x08909A38u); G[6] = G[2]; G[5] = G[16]; G[4] = frame.at(0x40u); G[7] = frame.at(0x10u); put(rt, frame.at(0x10u), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[20]); put(rt, frame.at(0x1Cu), G[21]); put(rt, frame.at(0x20u), G[22]); call_here(rt, ctx, 0x08BD32F8u); G[2] = word(rt, frame.at(0x40u)); G[3] = word(rt, frame.at(0x44u)); put(rt, frame.at(0x24u), G[2]); G[2] = word(rt, frame.at(0x48u)); put(rt, frame.at(0x28u), G[3]); put(rt, frame.at(0x2Cu), G[2]); G[3] = word(rt, frame.at(0x4Cu)); G[2] = word(rt, frame.at(0x50u)); put(rt, frame.at(0x30u), G[3]); put(rt, frame.at(0x34u), G[2]); L_08BD462C: ; L_08BD4630: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x68u)); G[17] = word(rt, frame.at(0x64u)); G[19] = word(rt, frame.at(0x6Cu)); G[22] = word(rt, frame.at(0x78u)); G[21] = word(rt, frame.at(0x74u)); G[20] = word(rt, frame.at(0x70u)); G[16] = word(rt, frame.at(0x60u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08BD48FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); auto &F = ctx.fpr; G[2] = 0x3u; G[21] = G[5]; G[3] = word(rt, G[4] + 0x10u); { const bool tk = G[3] != G[2]; G[18] = G[4]; if (tk) goto L_08BD4954; } F[12] = float_at(rt, G[4] + 0x18u); G[5] = G[4]; G[4] = G[21]; call_here(rt, ctx, 0x08AF2978u); goto L_08BD4A60; L_08BD4954: ; G[2] = 0x2u; if (G[3] == G[2]) { G[4] = frame.at(0x4u); goto L_08BD4970; } call_here(rt, ctx, 0x08AF213Cu); goto L_08BD4A60; L_08BD4970: ; G[6] = 0u; call_here(rt, ctx, 0x08BD2E04u); G[5] = sext(load8(G[18] + 0x14u), 8); G[4] = G[21]; G[16] = word(rt, G[18] + 0x4u); G[30] = G[2]; call_here(rt, ctx, 0x08BD2DDCu); G[17] = word(rt, G[18] + 0xCu); G[23] = G[2]; { const bool tk = static_cast<std::int32_t>(G[17]) >= 0; G[20] = word(rt, G[16] + 0x0u); if (tk) goto L_08BD4A14; } G[3] = load16(G[20] + 0x10u); G[17] = word(rt, G[20] + 0x30u); G[2] = G[3] << 6; G[3] = G[3] << 4; G[2] = G[2] - G[3]; G[22] = G[17] + G[2]; { const bool tk = G[17] == G[22]; G[19] = 0u; if (tk) goto L_08BD4A5C; } L_08BD49BC: ; G[4] = G[20]; G[5] = G[23]; G[6] = G[19]; call_here(rt, ctx, 0x089063A8u); G[5] = frame.at(0u); { const bool tk = G[2] == 0u; G[4] = G[18]; if (tk) goto L_08BD4A00; } put(rt, frame.at(0x0u), G[19]); call_here(rt, ctx, 0x08BD2EB4u); G[4] = G[30]; G[5] = G[17] + 0x4u; G[16] = G[2]; call_here(rt, ctx, 0x08906304u); F[12] = F[0]; G[4] = G[21]; G[5] = G[16]; call_here(rt, ctx, 0x08AF2978u); L_08BD4A00: ; G[17] = G[17] + 0x30u; { const bool tk = G[22] != G[17]; G[19] = G[19] + 0x1u; if (tk) goto L_08BD49BC; } goto L_08BD4A60; L_08BD4A14: ; G[5] = G[2]; G[4] = G[20]; G[6] = G[17]; call_here(rt, ctx, 0x089063A8u); { const bool tk = G[2] == 0u; if (tk) goto L_08BD4A60; } G[5] = word(rt, G[20] + 0x30u); G[3] = G[17] << 4; G[2] = G[17] << 6; G[2] = G[2] - G[3]; G[5] = G[5] + G[2]; G[4] = G[30]; G[5] = G[5] + 0x4u; call_here(rt, ctx, 0x08906304u); F[12] = F[0]; G[4] = G[21]; G[5] = G[18]; call_here(rt, ctx, 0x08AF2978u); L_08BD4A5C: ; L_08BD4A60: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x34u)); G[18] = word(rt, frame.at(0x28u)); G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C76E74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    push_back_character(rt, ctx);
}
void func_08843BD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x5Cu), G[31]); put(rt, frame.at(0x58u), G[22]); put(rt, frame.at(0x54u), G[21]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x40u), G[16]); G[20] = G[5]; G[5] = G[7]; G[17] = G[4]; { const bool tk = static_cast<std::int32_t>(G[6]) <= 0; if (tk) goto L_08843D3C; } G[21] = frame.at(0x10u); G[4] = G[21]; call_here(rt, ctx, 0x08A2D338u); G[16] = word(rt, frame.at(0x10u)); if (G[16] != 0u) { G[4] = G[16]; goto L_08843C2C; } G[4] = 0x08CA0000u; G[4] = 0x08C9C80Cu; call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; L_08843C2C: ; G[18] = frame.at(0x14u); call_here(rt, ctx, 0x08A2D1B8u); G[4] = G[18]; G[19] = G[2]; call_here(rt, ctx, 0x08B09F98u); call_here(rt, ctx, 0x08B0979Cu); G[4] = G[2]; G[5] = G[18]; call_here(rt, ctx, 0x08B09438u); { const bool tk = G[2] != 0u; G[4] = G[19]; if (tk) goto L_08843C84; } G[16] = frame.at(0xCu); G[4] = G[16]; G[5] = 0u; call_here(rt, ctx, 0x08A97A6Cu); { const bool tk = G[17] == G[16]; G[5] = word(rt, frame.at(0xCu)); if (tk) goto L_08843D20; } G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); G[4] = G[16]; goto L_08843D24; L_08843C84: ; G[5] = G[18]; call_here(rt, ctx, 0x08B096DCu); { const bool tk = G[2] == 0u; G[22] = frame.at(0x4u); if (tk) goto L_08843CBC; } G[16] = frame.at(0x8u); G[4] = G[16]; G[5] = 0u; call_here(rt, ctx, 0x08A97A6Cu); { const bool tk = G[17] == G[16]; G[5] = word(rt, frame.at(0x8u)); if (tk) goto L_08843D20; } G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); G[4] = G[16]; goto L_08843D24; L_08843CBC: ; G[5] = G[20]; G[4] = G[22]; call_here(rt, ctx, 0x088435A8u); G[16] = word(rt, frame.at(0x4u)); { const bool tk = G[16] != 0u; if (tk) goto L_08843CE0; } G[4] = 0x08CA0000u; G[4] = 0x08C9C7CCu; call_here(rt, ctx, 0x08A5BB7Cu); L_08843CE0: ; G[4] = G[16]; call_here(rt, ctx, 0x08843404u); G[4] = G[2]; G[5] = G[19]; call_here(rt, ctx, 0x088D45D8u); G[4] = G[22]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); G[5] = 0x1u; call_here(rt, ctx, 0x08A97A6Cu); { const bool tk = G[17] == frame.at(0u); G[5] = word(rt, frame.at(0x0u)); if (tk) goto L_08843D18; } G[4] = G[17]; call_here(rt, ctx, 0x08A5BAD8u); L_08843D18: ; G[4] = frame.at(0u); goto L_08843D24; L_08843D20: ; G[4] = G[16]; L_08843D24: ; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[18]; call_here(rt, ctx, 0x08B09290u); G[4] = G[21]; call_here(rt, ctx, 0x08A5B9ECu); L_08843D3C: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x50u)); G[17] = word(rt, frame.at(0x44u)); G[22] = word(rt, frame.at(0x58u)); G[21] = word(rt, frame.at(0x54u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[16] = word(rt, frame.at(0x40u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0886C798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[19] = G[4]; G[18] = G[6]; { const bool tk = G[6] != 0u; if (tk) goto L_0886C7D4; } G[5] = 0x08C90000u; G[5] = 0x08C8913Cu; G[6] = frame.at(0x4u); call_here(rt, ctx, 0x08B93CD8u); G[2] = G[19]; goto L_0886C910; L_0886C7D4: ; G[6] = sext(load8(G[6] + 0x0u), 8); G[2] = 0x2Fu; { const bool tk = G[6] != G[2]; if (tk) goto L_0886C878; } G[5] = G[18] + 0x1u; G[6] = frame.at(0x3u); call_here(rt, ctx, 0x08B93CD8u); G[2] = G[19]; goto L_0886C910; L_0886C7F8: ; G[2] = 0x3Au; { const bool tk = G[6] != G[2]; if (tk) goto L_0886C8A4; } if (static_cast<std::int32_t>(G[7]) > 0) { G[5] = G[18]; goto L_0886C8B4; } goto L_0886C8A4; L_0886C814: ; G[8] = G[8] + 0x1u; G[2] = static_cast<std::int32_t>(G[7]) < 8 ? 1u : 0u; { const bool tk = G[2] == 0u; G[6] = sext(load8(G[8] + 0x0u), 8); if (tk) goto L_0886C8C8; } G[2] = G[6] + 0xFFFFFF9Fu; G[3] = G[6] + 0xFFFFFFBFu; G[4] = G[6] + 0xFFFFFFD0u; G[3] = G[3] < 0x1Au ? 1u : 0u; G[2] = G[2] < 0x1Au ? 1u : 0u; G[3] = G[3] | G[2]; G[4] = G[4] < 0xAu ? 1u : 0u; { const bool tk = G[6] != 0u; G[4] = G[4] | G[3]; if (tk) goto L_0886C7F8; } goto L_0886C8C8; L_0886C850: ; G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[2] = G[19]; goto L_0886C910; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); G[2] = G[19]; goto L_0886C910; L_0886C878: ; { const bool tk = G[6] == 0u; G[2] = G[6] + 0xFFFFFF9Fu; if (tk) goto L_0886C8C8; } G[3] = G[6] + 0xFFFFFFBFu; G[3] = G[3] < 0x1Au ? 1u : 0u; G[2] = G[2] < 0x1Au ? 1u : 0u; G[4] = G[6] + 0xFFFFFFD0u; G[3] = G[3] | G[2]; G[4] = G[4] < 0xAu ? 1u : 0u; G[4] = G[4] | G[3]; G[8] = G[18]; G[7] = 0u; L_0886C8A4: ; { const bool tk = G[4] != 0u; G[7] = G[7] + 0x1u; if (tk) goto L_0886C814; } goto L_0886C8C8; L_0886C8B4: ; G[4] = G[19]; G[6] = frame.at(0x2u); call_here(rt, ctx, 0x08B93CD8u); G[2] = G[19]; goto L_0886C910; L_0886C8C8: ; G[4] = G[5]; call_here(rt, ctx, 0x0886B31Cu); G[16] = frame.at(0x8u); G[17] = G[2]; G[4] = G[16]; G[5] = G[18]; G[6] = frame.at(0x1u); call_here(rt, ctx, 0x08B93CD8u); G[4] = G[19]; G[6] = G[16]; G[5] = G[17]; call_here(rt, ctx, 0x08B95484u); G[4] = word(rt, frame.at(0x8u)); G[2] = 0x08D70000u; G[2] = 0x08D75298u; G[16] = G[4] + 0xFFFFFFF4u; { const bool tk = G[16] != G[2]; G[2] = G[19]; if (tk) goto L_0886C850; } L_0886C910: ; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_088AA504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    reset_entry_tables(rt, ctx);
}
void func_088B723C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    push_value_into_heap(rt, ctx);
}
void func_088BB780(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ramp_between_values(rt, ctx);
}
void func_089A7C80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    run_object_with_strings(rt, ctx);
}
void func_089C5E4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    rebuild_name_members(rt, ctx);
}
void func_08A04570(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    convert_with_handle(rt, ctx);
}
void func_08B5CA20(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    u32 tail = 0u;
    if (empty_owner_tables(rt, ctx, tail)) call_set(rt, ctx, 0x08B61FC0u, {tail});
}
// 0x08B5CC1C (a0 = the object; v0 = as the asm leaves it, see the comments): releases the object's three buffers. word(object + 4) is an array of shared holders (count word(object + 8)) and so
// is word(object + 0xC): each non-zero holder's use count (+4) is dropped with 0x08B60804(holder + 4, -1) and a last use (answer 1) calls its virtual +4 on it; then each array is freed with
// 0x08B61FC0 (when non-zero; a zero count frees it straight away). word(object + 0x10) is a table of six pointers: each non-zero one is freed with 0x08B61FC0, and the table itself ends the function
// as a tail call 0x08B61FC0(table) (made after the frame is gone).
static bool release_buffers(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 object) {
    GuestFrame frame(ctx, 0x10u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {31u, 0xCu}, {17u, 0x4u}, {16u, 0x0u}});
    ctx.gpr[18] = object;
    const auto release_holder_at = [&](u32 holder) {
        if (call_set(rt, ctx, 0x08B60804u, {holder + 4u, 0xFFFFFFFFu}) == 1u) call_indirect(rt, ctx, word(rt, word(rt, holder) + 4u), {holder});
    };
    u32 array = word(rt, object + 4u);
    ctx.gpr[4] = array;
    if (array != 0u) {
        u32 count = word(rt, object + 8u);
        u32 index = 0u;
        ctx.gpr[17] = 0u;
        if (count != 0u) {
            for (;;) {
                ctx.gpr[2] = array + 4u * index;
                const u32 holder = word(rt, ctx.gpr[2]);
                ctx.gpr[16] = holder;
                if (holder != 0u) {
                    release_holder_at(holder);
                    array = word(rt, object + 4u);
                    count = word(rt, object + 8u);
                    ctx.gpr[4] = array;
                }
                ++index;
                ctx.gpr[17] = index;
                const bool more = index < count;
                ctx.gpr[2] = index << 2;
                if (!more) break;
            }
        }
        if (array != 0u || count == 0u) call_set(rt, ctx, 0x08B61FC0u, {array});
        array = word(rt, object + 0xCu);
    } else {
        array = word(rt, object + 0xCu);
    }
    ctx.gpr[4] = array;
    u32 table;
    if (array == 0u) {
        table = word(rt, object + 0x10u);
    } else {
        u32 count = word(rt, object + 8u);
        ctx.gpr[17] = 0u;
        if (count == 0u) {
            call_set(rt, ctx, 0x08B61FC0u, {array});
            table = word(rt, object + 0x10u);
        } else {
            u32 index = 0u;
            for (;;) {
                ctx.gpr[2] = array + 4u * index;
                const u32 holder = word(rt, ctx.gpr[2]);
                ctx.gpr[16] = holder;
                if (holder != 0u) {
                    release_holder_at(holder);
                    count = word(rt, object + 8u);
                    array = word(rt, object + 0xCu);
                    ctx.gpr[4] = array;
                }
                ++index;
                ctx.gpr[17] = index;
                const bool more = index < count;
                ctx.gpr[2] = more ? 1u : 0u;
                if (!more) break;
            }
            if (array != 0u) {
                call_set(rt, ctx, 0x08B61FC0u, {array});
                table = word(rt, object + 0x10u);
            } else {
                table = word(rt, object + 0x10u);
                if (table == 0u) return false;
            }
        }
    }
    if (table == 0u) return false;
    u32 offset = 0u;
    ctx.gpr[17] = 0x18u;
    for (;;) {
        const u32 pointer = word(rt, table + offset);
        offset += 4u;
        ctx.gpr[16] = offset;
        if (pointer != 0u) {
            call_set(rt, ctx, 0x08B61FC0u, {pointer});
            table = word(rt, object + 0x10u);
        }
        ctx.gpr[2] = table + offset;
        if (offset == 0x18u) break;
    }
    if (table == 0u) return false;
    ctx.gpr[4] = table;
    return true;
}

void func_08B5CC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (release_buffers(rt, ctx, ctx.gpr[4])) jump(ctx, 0x08B61FC0u);
}
void func_08B6BE8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    scaled_exponential(rt, ctx);
}
void func_08BF15D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); auto &F = ctx.fpr; G[18] = G[5]; G[17] = G[6]; G[3] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[4] + 0x8u); { const bool tk = G[3] == G[2]; G[16] = G[4]; if (tk) goto L_08BF1680; } { const bool tk = G[3] == 0u; G[2] = G[3] + 0xFFFFFFF8u; if (tk) goto L_08BF1624; } F[0] = float_at(rt, G[3] + 0xFFFFFFF8u); put_float(rt, G[3] + 0x0u, F[0]); F[1] = float_at(rt, G[2] + 0x4u); put_float(rt, G[3] + 0x4u, F[1]); G[3] = word(rt, G[4] + 0x4u); L_08BF1624: ; G[6] = G[3] + 0xFFFFFFF8u; G[4] = G[3]; G[2] = G[6] - G[18]; G[3] = G[3] + 0x8u; put(rt, G[16] + 0x4u, G[3]); G[5] = sra(G[2], 3); F[3] = float_at(rt, G[17] + 0x4u); { const bool tk = static_cast<std::int32_t>(G[5]) <= 0; F[2] = float_at(rt, G[17] + 0x0u); if (tk) goto L_08BF1674; } G[3] = G[4] + 0xFFFFFFF0u; G[2] = G[6]; G[4] = 0u; L_08BF1654: ; F[0] = float_at(rt, G[3] + 0x0u); G[4] = G[4] + 0x1u; put_float(rt, G[2] + 0x0u, F[0]); F[1] = float_at(rt, G[3] + 0x4u); G[3] = G[3] + 0xFFFFFFF8u; put_float(rt, G[2] + 0x4u, F[1]); { const bool tk = G[5] != G[4]; G[2] = G[2] + 0xFFFFFFF8u; if (tk) goto L_08BF1654; } L_08BF1674: ; put_float(rt, G[18] + 0x4u, F[3]); put_float(rt, G[18] + 0x0u, F[2]); goto L_08BF1748; L_08BF1680: ; G[2] = word(rt, G[4] + 0x0u); G[2] = G[3] - G[2]; G[3] = 0x1FFF0000u; G[2] = sra(G[2], 3); G[3] = G[3] | 0xFFFFu; { const bool tk = G[2] != G[3]; G[4] = 0x08CA0000u; if (tk) goto L_08BF16A4; } G[4] = 0x08CA43E0u; call_here(rt, ctx, 0x08B5ADDCu); L_08BF16A4: ; { const bool tk = G[2] == 0u; G[3] = 0x1u; if (tk) goto L_08BF16BC; } G[3] = G[2] << 1; G[2] = G[3] < G[2] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFF8u; if (tk) goto L_08BF16C0; } L_08BF16BC: ; G[20] = G[3] << 3; L_08BF16C0: ; G[4] = 0x10u; G[5] = G[20]; call_here(rt, ctx, 0x08B35840u); G[4] = word(rt, G[16] + 0x0u); G[5] = G[18]; G[6] = G[2]; G[7] = frame.at(0x2u); G[19] = G[2]; call_here(rt, ctx, 0x08BF114Cu); if (G[2] == 0u) { G[5] = word(rt, G[16] + 0x4u); goto L_08BF1700; } F[0] = float_at(rt, G[17] + 0x0u); put_float(rt, G[2] + 0x0u, F[0]); F[1] = float_at(rt, G[17] + 0x4u); put_float(rt, G[2] + 0x4u, F[1]); G[5] = word(rt, G[16] + 0x4u); L_08BF1700: ; G[4] = G[18]; G[6] = G[2] + 0x8u; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08BF114Cu); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08BF1070u); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; G[2] = G[19] + G[20]; if (tk) goto L_08BF173C; } call_here(rt, ctx, 0x08B35254u); G[2] = G[19] + G[20]; L_08BF173C: ; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08BF1748: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C7A274(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x34u), G[31]); G[30] = G[7]; G[23] = G[6]; G[22] = G[4] + 0x8u; G[17] = G[4]; G[16] = G[5]; G[20] = G[16] + 0x8u; G[21] = G[16] + 0xCu; G[19] = G[16] + 0x10u; G[2] = word(rt, G[5] + 0x8u); G[10] = word(rt, G[5] + 0x4u); G[5] = word(rt, G[5] + 0x10u); G[3] = word(rt, G[2] + 0xFFFFFFF4u); put(rt, G[4] + 0x4u, 0u); G[18] = G[16] + 0x14u; G[3] = G[4] + G[3]; G[2] = word(rt, G[5] + 0xFFFFFFF4u); G[4] = word(rt, G[10] + 0xFFFFFFF4u); G[5] = word(rt, G[16] + 0xCu); G[8] = word(rt, G[16] + 0x14u); G[9] = word(rt, G[16] + 0x18u); G[2] = G[22] + G[2]; G[4] = G[17] + G[4]; put(rt, G[3] + 0x0u, G[5]); G[5] = 0u; put(rt, G[2] + 0x0u, G[8]); G[2] = G[16] + 0x4u; put(rt, G[17] + 0x0u, G[10]); put(rt, G[4] + 0x0u, G[9]); put(rt, frame.at(0x0u), G[2]); G[2] = word(rt, G[17] + 0x0u); G[3] = word(rt, G[16] + 0x1Cu); G[4] = word(rt, G[2] + 0xFFFFFFF4u); put(rt, G[17] + 0x8u, G[3]); G[4] = G[17] + G[4]; call_here(rt, ctx, 0x08C54A7Cu); G[3] = word(rt, G[16] + 0x0u); G[4] = word(rt, G[16] + 0x20u); G[5] = word(rt, G[16] + 0x24u); G[2] = word(rt, G[3] + 0xFFFFFFF4u); put(rt, G[17] + 0x0u, G[3]); G[16] = G[17] + 0xCu; G[2] = G[17] + G[2]; put(rt, G[2] + 0x0u, G[4]); G[4] = G[16]; put(rt, G[17] + 0x8u, G[5]); call_here(rt, ctx, 0x08C771D4u); G[2] = word(rt, G[17] + 0x0u); G[5] = G[16]; G[4] = word(rt, G[2] + 0xFFFFFFF4u); G[4] = G[17] + G[4]; call_here(rt, ctx, 0x08C54A7Cu); G[4] = G[16]; G[5] = G[23]; G[6] = G[30]; call_here(rt, ctx, 0x08C7835Cu); { const bool tk = G[2] == 0u; G[2] = word(rt, G[17] + 0x0u); if (tk) goto L_08C7A3C4; } G[5] = 0u; G[4] = word(rt, G[2] + 0xFFFFFFF4u); G[4] = G[17] + G[4]; call_here(rt, ctx, 0x08C54CFCu); goto exit_; L_08C7A3C4: ; G[4] = word(rt, G[2] + 0xFFFFFFF4u); G[4] = G[17] + G[4]; G[5] = word(rt, G[4] + 0x14u); G[5] = G[5] | 0x4u; call_here(rt, ctx, 0x08C54CFCu); goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0889B1FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    emit_record_groups(rt, ctx);
}
void func_08B480C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[23]); put(rt, frame.at(0x18u), G[22]); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[30]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x0u), G[16]); G[23] = G[6]; G[22] = G[7] & 0xFFu; G[20] = G[4]; G[3] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[3] + 0x14u); { const std::uint32_t tg = G[2]; G[19] = G[5]; call_here(rt, ctx, tg); } G[17] = word(rt, G[20] + 0x30u); { const bool tk = G[17] == 0u; G[18] = 0u < G[23] ? 1u : 0u; if (tk) goto L_08B48210; } G[21] = 0u < G[19] ? 1u : 0u; G[30] = G[18]; L_08B4811C: ; G[16] = word(rt, G[17] + 0x0u); G[2] = load8(G[16] + 0x30u); if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[2] = word(rt, G[16] + 0x0u); if (static_cast<std::int32_t>(G[2]) < 0) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } { const bool tk = G[22] == 0u; if (tk) goto L_08B481A4; } { const bool tk = G[21] == 0u; if (tk) goto L_08B48180; } { const bool tk = G[30] == 0u; G[4] = G[19]; if (tk) goto L_08B48168; } G[5] = G[16] + 0x4u; call_here(rt, ctx, 0x08B3F9E0u); if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[4] = G[23]; goto L_08B4818C; L_08B48168: ; G[5] = G[16] + 0x4u; call_here(rt, ctx, 0x08B3F9E0u); if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[16] = word(rt, G[16] + 0x0u); goto L_08B48214; L_08B48180: ; if (G[30] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[4] = G[23]; L_08B4818C: ; G[5] = G[16] + 0x1Au; call_here(rt, ctx, 0x08B3F9E0u); if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[16] = word(rt, G[16] + 0x0u); goto L_08B48214; L_08B481A4: ; { const bool tk = G[21] == 0u; if (tk) goto L_08B481E4; } { const bool tk = G[18] == 0u; G[4] = G[19]; if (tk) goto L_08B481CC; } G[5] = G[16] + 0x4u; call_here(rt, ctx, 0x08B3F980u); if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[4] = G[23]; goto L_08B481F0; L_08B481CC: ; G[5] = G[16] + 0x4u; call_here(rt, ctx, 0x08B3F980u); if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[16] = word(rt, G[16] + 0x0u); goto L_08B48214; L_08B481E4: ; if (G[18] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[4] = G[23]; L_08B481F0: ; G[5] = G[16] + 0x1Au; call_here(rt, ctx, 0x08B3F980u); if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B48208; } G[16] = word(rt, G[16] + 0x0u); goto L_08B48214; L_08B48208: ; { const bool tk = G[2] != 0u; G[17] = G[2]; if (tk) goto L_08B4811C; } L_08B48210: ; G[16] = 0xFFFFFFFFu; L_08B48214: ; G[3] = word(rt, G[20] + 0x0u); G[2] = word(rt, G[3] + 0x18u); { const std::uint32_t tg = G[2]; G[4] = G[20]; call_here(rt, ctx, tg); } G[2] = G[16]; goto exit_;  exit_: ; G[23] = word(rt, frame.at(0x1Cu)); G[22] = word(rt, frame.at(0x18u)); G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[30] = word(rt, frame.at(0x20u)); G[21] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x8u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B76638(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x30u), G[31]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x10u), G[16]); G[21] = G[5]; G[19] = G[4]; G[5] = G[19]; G[17] = G[6]; G[2] = word(rt, G[4] + 0x10u); G[18] = (G[2] >> 3) & 0xFFu; G[4] = G[18]; call_here(rt, ctx, 0x08B75EDCu); G[20] = G[2]; G[2] = word(rt, G[17] + 0x0u); { const bool tk = G[2] == 0u; if (tk) goto L_08B76784; } G[22] = 0u; G[23] = 0x1u; goto L_08B766DC; L_08B7669C: ; if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x0u); goto L_08B766CC; } G[4] = word(rt, G[21] + 0x0u); L_08B766A8: ; if (G[4] == 0u) { G[2] = word(rt, G[17] + 0x0u); goto L_08B766CC; } G[2] = word(rt, G[4] + 0x4u); G[3] = G[2] << 2; G[3] = G[3] + G[4]; G[2] = G[2] + 0x1u; put(rt, G[4] + 0x4u, G[2]); put(rt, G[3] + 0x8u, G[17]); G[2] = word(rt, G[17] + 0x0u); L_08B766CC: ; G[2] = G[2] + G[17]; G[3] = word(rt, G[2] + 0x4u); { const bool tk = G[3] == 0u; G[17] = G[2] + 0x4u; if (tk) goto L_08B76780; } L_08B766DC: ; G[4] = word(rt, G[17] + 0x4u); if (G[4] == 0u) { G[2] = word(rt, G[17] + 0x0u); goto L_08B766CC; } G[2] = word(rt, G[19] + 0x10u); G[2] = G[2] & 0x4u; { const bool tk = G[2] == 0u; G[2] = G[17] + 0x4u; if (tk) goto L_08B76724; } G[16] = G[2] - G[4]; { const bool tk = G[22] == G[16]; if (tk) goto L_08B76724; } G[4] = G[16]; call_here(rt, ctx, 0x08B75F68u); G[4] = G[2] & 0xFFu; G[5] = G[19]; G[18] = G[2]; call_here(rt, ctx, 0x08B75EDCu); G[20] = G[2]; G[22] = G[16]; L_08B76724: ; if (G[18] == 0u) { G[2] = word(rt, G[17] + 0x8u); goto L_08B7669C; } G[16] = G[18] & 0xFFu; G[5] = G[20]; G[6] = G[17] + 0x8u; G[7] = frame.at(0u); G[4] = G[16]; call_here(rt, ctx, 0x08B75DA0u); G[4] = G[16]; call_here(rt, ctx, 0x08B75C8Cu); G[3] = G[2]; G[2] = G[2] < 0x4u ? 1u : 0u; { const bool tk = G[2] != 0u; G[4] = 0xFFFFFFFFu; if (tk) goto L_08B767AC; } G[2] = word(rt, frame.at(0x0u)); G[2] = G[4] & G[2]; if (G[2] != 0u) { G[4] = word(rt, G[21] + 0x0u); goto L_08B766A8; } G[2] = word(rt, G[17] + 0x0u); L_08B76770: ; G[2] = G[2] + G[17]; G[3] = word(rt, G[2] + 0x4u); { const bool tk = G[3] != 0u; G[17] = G[2] + 0x4u; if (tk) goto L_08B766DC; } L_08B76780: ; L_08B76784: ; goto exit_; L_08B767AC: ; G[2] = G[3] << 3; G[2] = G[23] << (G[2] & 31u); G[4] = G[2] + 0xFFFFFFFFu; G[2] = word(rt, frame.at(0x0u)); G[2] = G[4] & G[2]; if (G[2] == 0u) { G[2] = word(rt, G[17] + 0x0u); goto L_08B76770; } G[4] = word(rt, G[21] + 0x0u); goto L_08B766A8;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_328
} // namespace gt::game

#define HOT328(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_328::name, gt::ReturnKind::kind)
HOT328(0x08BE03BCu, func_08BE03BC, Word);
HOT328(0x08C78CC0u, func_08C78CC0, Word);
HOT328(0x0889AB88u, func_0889AB88, Word);
HOT328(0x0893CA3Cu, func_0893CA3C, Word);
HOT328(0x089CE23Cu, func_089CE23C, Word);
HOT328(0x08B5358Cu, func_08B5358C, Word);
// HOT328(0x08BD44C4u, func_08BD44C4, Word);  // no comparable synthetic case yet
// HOT328(0x08BD48FCu, func_08BD48FC, Word);  // no comparable synthetic case yet
HOT328(0x08C76E74u, func_08C76E74, Word);
HOT328(0x08843BD0u, func_08843BD0, Word);
HOT328(0x0886C798u, func_0886C798, Word);
HOT328(0x088AA504u, func_088AA504, Word);
HOT328(0x088B723Cu, func_088B723C, Word);
HOT328(0x088BB780u, func_088BB780, Word);
HOT328(0x089A7C80u, func_089A7C80, Word);
HOT328(0x089C5E4Cu, func_089C5E4C, Word);
HOT328(0x08A04570u, func_08A04570, Word);
HOT328(0x08B5CA20u, func_08B5CA20, Word);
HOT328(0x08B5CC1Cu, func_08B5CC1C, Word);
HOT328(0x08B6BE8Cu, func_08B6BE8C, Word);
HOT328(0x08BF15D4u, func_08BF15D4, Word);
// HOT328(0x08C7A274u, func_08C7A274, Word);  // no comparable synthetic case yet
HOT328(0x0889B1FCu, func_0889B1FC, Word);
// HOT328(0x08B480C0u, func_08B480C0, Word);  // no comparable synthetic case yet
// HOT328(0x08B76638u, func_08B76638, Word);  // no comparable synthetic case yet
