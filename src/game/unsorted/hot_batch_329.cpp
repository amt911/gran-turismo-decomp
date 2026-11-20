// Hot batch 329: functions no route reaches (arbitrary control flow: several exits, tail calls, loops and FPU arithmetic),
// drafted from `tools.disasm` with a register-level emitter (pipeline/bodyg.py: labels and gotos over the same register-level operations as bodyx), read against the asm and verified
// by synthetic cases.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <initializer_list>
#include <cmath>
#include <cstdint>
#include <utility>

namespace gt::game {
namespace hot_batch_329 {
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
} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

// 0x08B8D028 / 0x08C4F48C share this body: std::vector<T>::_M_insert_aux(a0 = the vector, a1 = the position, a2 = the value) for 2-byte
// (`bytes` 2) and 8-byte elements. With room, the last element is copied one place on, the rest moved back and the value stored at the
// position; full, the storage doubles (0x08B35DA0, at least one element; `limit` elements gives a length error 0x08B5ADDC(message)), the two
// halves are copied around the value (`copy`), the old ones destroyed (`destroy`) and freed (0x08B35254).
struct InsertAux {
    u32 bytes, shift, limit, message, copy, destroy;
};
void insert_aux(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const InsertAux &k) {
    const u32 vec = ctx.gpr[4], position = ctx.gpr[5], value = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {31u, 0x24u}, {20u, 0x20u}, {19u, 0x1Cu}});
    ctx.gpr[18] = position;
    ctx.gpr[17] = value;
    ctx.gpr[16] = vec;
    // One element from `from` to `to`, in the order the asm copies it (2 bytes: low then high; 8 bytes: the high word first).
    auto move_one = [&](u32 from, u32 to) {
        if (k.bytes == 2u) {
            store8(to + 0x0u, load8(from + 0x0u));
            store8(to + 0x1u, load8(from + 0x1u));
        } else {
            const u32 hi = word(rt, from + 0x4u), lo = word(rt, from + 0x0u);
            put(rt, to + 0x4u, hi);
            put(rt, to + 0x0u, lo);
        }
    };
    u32 finish = word(rt, vec + 0x4u);
    if (finish != word(rt, vec + 0x8u)) {
        if (finish != 0u) {
            move_one(finish - k.bytes, finish);
            finish = word(rt, vec + 0x4u);
        }
        const u32 last = finish - k.bytes;
        const u32 count = sra(last - position, k.shift);
        put(rt, vec + 0x4u, finish + k.bytes);
        u32 v0, v1;
        if (k.bytes == 2u) {
            v0 = load8(value + 0x0u);
            v1 = load8(value + 0x1u);
        } else {
            v0 = word(rt, value + 0x0u);
            v1 = word(rt, value + 0x4u);
        }
        u32 to = last, from = last;
        ctx.gpr[2] = last - position;  // v0 when nothing moves; else the last word (8 bytes) or high byte (2 bytes) moved
        for (u32 i = 0u; static_cast<s32>(count) > 0 && i < count; ++i) {
            from -= k.bytes;
            move_one(from, to);
            ctx.gpr[2] = k.bytes == 2u ? load8(from + 0x1u) : word(rt, from + 0x0u);
            to -= k.bytes;
        }
        if (k.bytes == 2u) {
            store8(position + 0x0u, v0);
            store8(position + 0x1u, v1);
        } else {
            put(rt, position + 0x0u, v0);
            put(rt, position + 0x4u, v1);
        }
        return;
    }
    const u32 size = sra(finish - word(rt, vec + 0x0u), k.shift);
    if (size == k.limit) call_set(rt, ctx, 0x08B5ADDCu, {k.message});
    u32 bytes = 0u, storage = 0u;
    bool allocate = true;
    if (size == 0u) {
        bytes = 1u << k.shift;
    } else {
        const u32 doubled = size << 1;
        if (doubled < size) {
            bytes = 0u - k.bytes;
        } else if (doubled == 0u) {
            allocate = false;
        } else {
            bytes = doubled << k.shift;
        }
    }
    ctx.gpr[20] = bytes;
    ctx.gpr[19] = 0u;
    if (allocate) {
        storage = call_set(rt, ctx, 0x08B35DA0u, {bytes});
        ctx.gpr[19] = storage;
    }
    u32 slot = call_set(rt, ctx, k.copy, {word(rt, vec + 0x0u), position, storage, frame.at(0x2u)});
    if (slot != 0u) move_one(value, slot);
    const u32 end = call_set(rt, ctx, k.copy, {position, word(rt, vec + 0x4u), slot + k.bytes, frame.at(0x1u)});
    ctx.gpr[17] = end;
    call_set(rt, ctx, k.destroy, {word(rt, vec + 0x0u), word(rt, vec + 0x4u), frame.at(0u)});
    const u32 old = word(rt, vec + 0x0u);
    if (old != 0u) call_set(rt, ctx, 0x08B35254u, {old});
    ctx.gpr[2] = storage + bytes;
    put(rt, vec + 0x8u, storage + bytes);
    put(rt, vec + 0x4u, end);
    put(rt, vec + 0x0u, storage);
}

// 0x08C7A470 (a0 = the stream, a1 = a file name, a2 = the open mode): a file stream constructor with a virtual base. The base at +0xB0
// (0x08B65660) and the virtual-base offsets from the vtable table 0x08CDC420 are set, the stream base initialised (0x08C54A7C), the final
// vtables 0x08CDC454 / 0x08CDC468 / 0x08CDC47C set, the file buffer at +0xC built (0x08C771D4) and attached, and 0x08C7835C opens the file:
// failure sets bit 2 (failbit) of the state through 0x08C54CFC.
void func_08C7A470(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], name = ctx.gpr[5], mode = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x8u}, {21u, 0x14u}, {16u, 0x0u}, {22u, 0x18u}, {20u, 0x10u}, {19u, 0xCu}, {17u, 0x4u}, {31u, 0x1Cu}});
    const u32 ios = self + 0xB0u, table = 0x08CDC420u;
    ctx.gpr[18] = ios;
    ctx.gpr[21] = 0x08CE0000u;
    ctx.gpr[16] = self;
    ctx.gpr[22] = 0x08CE0000u;
    ctx.gpr[20] = mode;
    ctx.gpr[19] = name;
    ctx.gpr[17] = self + 0x8u;
    call_set(rt, ctx, 0x08B65660u, {ios});
    const u32 v8 = word(rt, table + 0x8u), v4 = word(rt, table + 0x4u), v10 = word(rt, table + 0x10u);
    store8(ios + 0x74u, 0u);
    put(rt, self + 0x4u, 0u);
    const u32 at1 = self + word(rt, v8 - 0xCu), at2 = self + 0x8u + word(rt, v10 - 0xCu), at3 = self + word(rt, v4 - 0xCu);
    const u32 vC = word(rt, table + 0xCu), v14 = word(rt, table + 0x14u), v18 = word(rt, table + 0x18u);
    put(rt, self + 0xB0u, 0x08CDB088u);
    put(rt, at1, vC);
    put(rt, at2, v14);
    put(rt, self + 0x0u, v4);
    put(rt, at3, v18);
    const u32 base = self + word(rt, word(rt, self) - 0xCu);
    put(rt, self + 0x8u, word(rt, table + 0x1Cu));
    put(rt, ios + 0x70u, 0u);
    store8(ios + 0x75u, 0u);
    put(rt, ios + 0x78u, 0u);
    put(rt, ios + 0x7Cu, 0u);
    put(rt, ios + 0x80u, 0u);
    put(rt, ios + 0x84u, 0u);
    ctx.gpr[2] = table;
    ctx.gpr[10] = 0x08CDB088u;
    ctx.gpr[11] = v4;
    ctx.gpr[7] = vC;
    ctx.gpr[8] = v14;
    ctx.gpr[9] = v18;
    call_set(rt, ctx, 0x08C54A7Cu, {base, 0u, word(rt, table + 0x1Cu)});
    put(rt, self + 0x0u, 0x08CDC454u);
    put(rt, self + 0xB0u, 0x08CDC47Cu);
    put(rt, self + 0x8u, 0x08CDC468u);
    const u32 buffer = self + 0xCu;
    ctx.gpr[17] = buffer;
    call_set(rt, ctx, 0x08C771D4u, {buffer});
    call_set(rt, ctx, 0x08C54A7Cu, {ios, buffer});
    const u32 opened = call_set(rt, ctx, 0x08C7835Cu, {buffer, name, mode});
    const u32 vtable = word(rt, self);
    ctx.gpr[2] = vtable;
    const u32 stream = self + word(rt, vtable - 0xCu);
    call_set(rt, ctx, 0x08C54CFCu, {stream, opened == 0u ? (word(rt, stream + 0x14u) | 0x4u) : 0u});
}
// 0x0898DC1C (a0 = the object, f12 = a limit): eases +0x4B8 towards the integer +0x4BC (x 0.2 a call), lays out (0x0898D9C0) and walks the
// entries from +0x220 to +0x224 (index in the frame, next by 0x0898D9E8(object, &index, 0x80, 1)) advancing a position by each entry's width
// (+0x428 byte x scale); entries that end past -width and start before the limit are drawn (0x08A4D738 on the global 0x08CF68E8 at the
// position) and their value (0x0898DB58) added to +0x4B8 (and to +0x4BC when the flag +0x4C0 is set).
void func_0898DC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {31u, 0x18u}, {17u, 0x14u}, {54u, 0x28u}, {53u, 0x24u}, {52u, 0x20u}});
    auto &F = ctx.fpr;
    auto int_float = [](u32 bits) { return static_cast<float>(static_cast<s32>(bits)); };
    ctx.gpr[16] = self;
    F[0] = 0.0f;
    F[2] = float_at(rt, self + 0x4B8u);
    ctx.set_fpu_condition(F[0] < F[2]);
    F[22] = F[12];
    if (ctx.fpu_condition()) {
        F[0] = std::bit_cast<float>(0x3E4CCCCDu);
        F[1] = F[2] + psp_mul(int_float(word(rt, self + 0x4BCu)) - F[2], F[0]);
        put_float(rt, self + 0x4B8u, F[1]);
    }
    ctx.gpr[17] = 0x08CF0000u;
    call_set(rt, ctx, 0x0898D9C0u, {self});
    const u32 global = word(rt, 0x08CF68E8u);
    const float base = float_at(rt, global + 0x68u), scale_g = float_at(rt, global + 0x24u);
    F[5] = scale_g;
    F[2] = int_float(word(rt, self + 0x4A8u));
    F[0] = base + 14.0f;
    F[3] = float_at(rt, self + 0x4B8u);
    F[1] = 1.0f;
    F[4] = psp_mul(base, scale_g);
    F[21] = psp_mul(F[0], scale_g);
    F[3] = F[3] + F[1];
    u32 index = word(rt, self + 0x220u);
    ctx.gpr[2] = word(rt, self + 0x224u);
    ctx.gpr[3] = index;
    F[2] = psp_mul(F[21], F[2]);
    F[3] = psp_mul(F[21], F[3]);
    put(rt, frame.at(0x0u), index);
    F[2] = (F[22] - F[2]) + F[3];
    F[20] = F[2] + F[4];
    while (index != word(rt, self + 0x224u)) {
        F[13] = F[20];
        F[0] = psp_mul(F[21], int_float(signed_byte(rt, index + self + 0x428u)));
        F[1] = -F[0];
        ctx.set_fpu_condition(F[20] <= F[1]);
        F[12] = 0.0f;
        if (!ctx.fpu_condition()) {
            ctx.set_fpu_condition(F[22] <= F[20]);
            if (ctx.fpu_condition()) return;
            call_set(rt, ctx, 0x08A4D738u, {word(rt, 0x08CF68E8u)});
            const u32 value = call_set(rt, ctx, 0x0898DB58u, {self, word(rt, frame.at(0x0u))});
            F[0] = float_at(rt, self + 0x4B8u) + int_float(value);
            const u32 now = word(rt, frame.at(0x0u));
            put_float(rt, self + 0x4B8u, F[0]);
            if (load8(self + 0x4C0u) != 0u) put(rt, self + 0x4BCu, value + word(rt, self + 0x4BCu));
            F[0] = psp_mul(F[21], int_float(signed_byte(rt, now + self + 0x428u)));
        }
        F[20] = F[20] + F[0];
        call_set(rt, ctx, 0x0898D9E8u, {self, frame.at(0u), 0x80u, 1u});
        index = word(rt, frame.at(0x0u));
        ctx.gpr[3] = index;
    }
}
// 0x089D9B90 (a0 = the object, a1 = an argument; v0 = 1 when the position was clamped to the limit, else 0): the wanted position f21 is the getter 0x089A8DFC(argument) when the byte at object + 0x8D is set,
// else float(object + 0x90) + 0x089A8E8C(argument). With f20 = 0x089D8410(object) and f23 = float(object + 0x9C): when f23 - f20 is not below f21 nothing moves (v0 = 0). Otherwise
// f22 = 0x089D83F4(object), f12 = f21 - f22, f13 = f23 - (f20 + f22) and the word at object + 0x80 (v1) is the step count. With no steps (v1 <= 0) f21 = f22 (plus 0x08B6728C() when f13 is above 0).
// Otherwise the step at object + 0xA0 advances by one (byte +0x8D clear) or becomes trunc(f12 / f13) (f13 above 0, else v1); below v1 the position is f22 plus 0x08B6728C() (f13 above 0) or
// 0x089D83F4(object) alone; at v1 or above it is clamped to f23 (v0 = 1) when that is below f21, and +0xA0 becomes v1. Last 0x089D9B88(object, f12 = f21).
void func_089D9B90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x8u}, {17u, 0x4u}, {55u, 0x1Cu}, {54u, 0x18u}, {53u, 0x14u}, {52u, 0x10u}});
    ctx.gpr[16] = object;
    float f21 = 0.0f, f20 = 0.0f, f22 = 0.0f;
    if (load8(object + 0x8Du) != 0u) {
        call_set(rt, ctx, 0x089A8DFCu, {argument});
        f21 = ctx.fpr[0];
    } else {
        f20 = float_at(rt, object + 0x90u);
        call_set(rt, ctx, 0x089A8E8Cu, {argument});
        f21 = f20 + ctx.fpr[0];
    }
    call_set(rt, ctx, 0x089D8410u, {object});
    const float f23 = float_at(rt, object + 0x9Cu);
    f20 = ctx.fpr[0];
    ctx.fpr[20] = f20;
    ctx.fpr[23] = f23;
    u32 clamped = 0u;
    ctx.gpr[17] = 0u;
    if (f23 - f20 < f21) {
        call_set(rt, ctx, 0x089D83F4u, {object});
        f22 = ctx.fpr[0];
        ctx.fpr[22] = f22;
        const float f12 = f21 - f22;
        const float f13 = f23 - (f20 + f22);
        const s32 steps = static_cast<s32>(word(rt, object + 0x80u));
        ctx.gpr[3] = static_cast<u32>(steps);
        if (steps <= 0) {
            f21 = f22;
            if (0.0f < f13) {
                call_set(rt, ctx, 0x08B6728Cu, {});
                f21 = f22 + ctx.fpr[0];
            }
        } else {
            s32 step;
            if (load8(object + 0x8Du) == 0u) {
                step = static_cast<s32>(word(rt, object + 0xA0u)) + 1;
                put(rt, object + 0xA0u, static_cast<u32>(step));
            } else if (0.0f < f13) {
                step = trunc_w_s(f12 / f13);
                put(rt, object + 0xA0u, static_cast<u32>(step));
            } else {
                step = steps;
                put(rt, object + 0xA0u, static_cast<u32>(steps));
            }
            ctx.gpr[2] = step < steps ? 1u : 0u;
            if (step < steps) {
                if (0.0f < f13) {
                    call_set(rt, ctx, 0x08B6728Cu, {});
                    f20 = ctx.fpr[0];
                    ctx.fpr[20] = f20;
                    call_set(rt, ctx, 0x089D83F4u, {object});
                    f21 = f20 + ctx.fpr[0];
                } else {
                    call_set(rt, ctx, 0x089D83F4u, {object});
                    f21 = ctx.fpr[0];
                }
            } else {
                put(rt, object + 0xA0u, static_cast<u32>(steps));
                if (f23 < f21) {
                    f21 = f23;
                    clamped = 1u;
                    ctx.gpr[17] = 1u;
                }
            }
        }
    }
    ctx.fpr[21] = f21;
    ctx.fpr[12] = f21;
    {
        // the last call takes the position in f12
        const GuestResult done = call_guest(rt, ctx, 0x089D9B88u, {object}, {f21});
        ctx.gpr[2] = done.v0;
        ctx.gpr[3] = done.v1;
        ctx.fpr[0] = done.f0;
    }
    ctx.gpr[2] = clamped;
}
void func_08AD9404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); auto &F = ctx.fpr; G[17] = 0x08D40000u; G[2] = word(rt, 0x08D405D0u); G[16] = 0x08D405D0u; { const bool tk = G[2] != 0u; if (tk) goto L_08AD9478; } G[3] = 0x08CE0000u; G[2] = word(rt, 0x08CDFB3Cu); G[2] = G[2] & 0x1u; { const bool tk = G[2] != 0u; G[2] = 0x08CE0000u; if (tk) goto L_08AD9454; } L_08AD943C: ; L_08AD9440: ; goto exit_; L_08AD9454: ; G[4] = word(rt, 0x08CDFB40u); G[5] = 0x08C90000u; G[7] = 0x08C90000u; G[5] = 0x08C9085Cu; G[7] = 0x08C909B8u; G[6] = 0x335u; call_here(rt, ctx, 0x08AD83B8u); goto L_08AD9440; L_08AD9478: ; G[18] = 0x08CE0000u; G[2] = word(rt, 0x08CDFB3Cu); G[3] = 0x1u; put(rt, G[16] + 0x4u, G[3]); G[2] = G[2] & 0x4u; { const bool tk = G[2] != 0u; G[4] = 0x08C90000u; if (tk) goto L_08AD9590; } L_08AD9494: ; G[4] = 0u; call_here(rt, ctx, 0x08ADFC20u); F[12] = std::bit_cast<float>(0u); call_here(rt, ctx, 0x08AD96D8u); call_here(rt, ctx, 0x08ADBAD0u); call_here(rt, ctx, 0x08AEB320u); call_here(rt, ctx, 0x08AEA0A4u); call_here(rt, ctx, 0x08ADDF3Cu); call_here(rt, ctx, 0x08AE2EB4u); call_here(rt, ctx, 0x08AE687Cu); call_here(rt, ctx, 0x08ADB4BCu); call_here(rt, ctx, 0x08AD9FF0u); call_here(rt, ctx, 0x08AEB3BCu); G[4] = word(rt, G[16] + 0x28u); { const bool tk = G[4] != 0u; if (tk) goto L_08AD9580; } G[4] = word(rt, G[16] + 0x24u); L_08AD9500: ; { const bool tk = G[4] != 0u; if (tk) goto L_08AD9570; } G[4] = word(rt, G[16] + 0x18u); L_08AD950C: ; { const bool tk = G[4] != 0u; if (tk) goto L_08AD9560; } G[2] = word(rt, 0x08CDFB3Cu); L_08AD9518: ; put(rt, 0x08D405D0u, 0u); G[2] = G[2] & 0x4u; put(rt, G[16] + 0x28u, 0u); put(rt, G[16] + 0x4u, 0u); put(rt, G[16] + 0x8u, 0u); put(rt, G[16] + 0xCu, 0u); put(rt, G[16] + 0x10u, 0u); put(rt, G[16] + 0x14u, 0u); put(rt, G[16] + 0x18u, 0u); put(rt, G[16] + 0x1Cu, 0u); put(rt, G[16] + 0x20u, 0u); { const bool tk = G[2] == 0u; put(rt, G[16] + 0x24u, 0u); if (tk) goto L_08AD943C; } G[4] = 0x08C90000u; G[4] = 0x08C909F4u; call_here(rt, ctx, 0x08AD8464u); goto L_08AD9440; L_08AD9560: ; call_here(rt, ctx, 0x08AD85C8u); G[2] = word(rt, 0x08CDFB3Cu); goto L_08AD9518; L_08AD9570: ; call_here(rt, ctx, 0x08AEB8C8u); G[4] = word(rt, G[16] + 0x18u); goto L_08AD950C; L_08AD9580: ; call_here(rt, ctx, 0x08AEB8C8u); G[4] = word(rt, G[16] + 0x24u); goto L_08AD9500; L_08AD9590: ; G[4] = 0x08C909D0u; call_here(rt, ctx, 0x08AD8464u); goto L_08AD9494;  exit_: ; G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); G[18] = word(rt, frame.at(0x8u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08C7835C (a0 = the file buffer, a1 = the name, a2 = the mode): basic_filebuf::open. A file already open (+0x24, 0x08B61534) or a failed
// open (0x08B615E0 with permissions 0x1B4) gives 0; otherwise the buffer pointers are reset from +0x30..+0x44 (after 0x08C76AA4) and, in append
// mode (bit 1), the position is moved to the end through the virtual +0x10 (seekoff(0, end)): a result of -1 closes it again
// (0x08C78130) and gives 0. v0 = the buffer on success.
void func_08C7835C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4], name = ctx.gpr[5], mode = ctx.gpr[6];
    GuestFrame frame(ctx, 0x60u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x44u}, {19u, 0x4Cu}, {18u, 0x48u}, {16u, 0x40u}, {31u, 0x50u}});
    const u32 file = self + 0x24u;
    ctx.gpr[17] = file;
    ctx.gpr[19] = mode;
    ctx.gpr[18] = name;
    ctx.gpr[16] = self;
    if (call_set(rt, ctx, 0x08B61534u, {file}) != 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x08B615E0u, {file, name, mode, 0x1B4u});
    if (call_set(rt, ctx, 0x08B61534u, {file}) == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    call_set(rt, ctx, 0x08C76AA4u, {self});
    const u32 start = word(rt, self + 0x78u);
    u32 w[6];
    for (u32 i = 0u; i < 6u; ++i) w[i] = word(rt, self + 0x30u + 4u * i);
    put(rt, self + 0xCu, start);
    for (u32 i = 0u; i < 6u; ++i) put(rt, self + 0x60u + 4u * i, w[i]);
    put(rt, self + 0x2Cu, mode);
    store8(self + 0x81u, 0u);
    store8(self + 0x82u, 0u);
    put(rt, self + 0x4u, start);
    put(rt, self + 0x8u, start);
    put(rt, self + 0x14u, 0u);
    put(rt, self + 0x10u, 0u);
    put(rt, self + 0x18u, 0u);
    for (u32 i = 0u; i < 6u; ++i) put(rt, self + 0x48u + 4u * i, w[i]);
    ctx.gpr[2] = start;
    for (u32 i = 0u; i < 6u; ++i) ctx.gpr[3 + i] = w[i];
    ctx.gpr[9] = mode & 0x2u;
    if ((mode & 0x2u) == 0u) {
        ctx.gpr[2] = self;
        return;
    }
    const u32 seek = word(rt, word(rt, self) + 0x10u);
    ctx.gpr[2] = word(rt, self);
    ctx.gpr[3] = seek;
    call_set(rt, ctx, seek, {frame.at(0x20u), self, 0u, 0u, 2u, mode});
    for (u32 i = 0u; i < 6u; ++i) put(rt, frame.at(0x8u + 4u * i), word(rt, frame.at(0x28u + 4u * i)));
    const u32 position = word(rt, frame.at(0x20u)), state = word(rt, frame.at(0x24u));
    put(rt, frame.at(0x0u), position);
    put(rt, frame.at(0x4u), state);
    ctx.gpr[2] = 0xFFFFFFFFu;
    ctx.gpr[3] = word(rt, frame.at(0x3Cu));
    ctx.gpr[4] = position;
    ctx.gpr[5] = state;
    if (position != 0xFFFFFFFFu || state != position) {
        ctx.gpr[2] = self;
        return;
    }
    call_set(rt, ctx, 0x08C78130u, {self});
    ctx.gpr[2] = 0u;
}
void func_08A5E4E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[6] = 0u; G[21] = G[5]; G[20] = G[7]; G[7] = 0u; G[17] = word(rt, G[4] + 0x1Cu); G[16] = word(rt, G[4] + 0x18u); G[5] = G[17]; G[4] = G[16]; call_here(rt, ctx, 0x08B6EE4Cu); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[16]; if (tk) goto L_08A5E540; } G[5] = G[17]; call_here(rt, ctx, 0x08B6EE80u); G[18] = G[2]; G[19] = G[3]; goto L_08A5E580; L_08A5E540: ; G[6] = G[17] << 31; G[2] = G[16] >> 1; G[2] = G[6] | G[2]; G[3] = G[17] >> 1; G[4] = G[16] & 0x1u; G[5] = 0u; G[4] = G[4] | G[2]; G[5] = G[5] | G[3]; call_here(rt, ctx, 0x08B6EE80u); G[4] = G[2]; G[5] = G[3]; G[6] = G[2]; G[7] = G[3]; call_here(rt, ctx, 0x08B71328u); G[18] = G[2]; G[19] = G[3]; L_08A5E580: ; G[16] = word(rt, G[20] + 0x0u); if (G[16] != 0u) { G[3] = word(rt, G[16] + 0x0u); goto L_08A5E59C; } G[4] = 0x08CB0000u; G[4] = 0x08CB0080u; call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16] + 0x0u); L_08A5E59C: ; G[2] = word(rt, G[3] + 0x50u); { const std::uint32_t tg = G[2]; G[4] = G[16]; call_here(rt, ctx, tg); } G[4] = G[2]; G[5] = G[3]; G[6] = 0u; G[7] = 0u; G[16] = G[2]; G[17] = G[3]; call_here(rt, ctx, 0x08B6EE4Cu); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; G[4] = G[16]; if (tk) goto L_08A5E5DC; } G[5] = G[17]; call_here(rt, ctx, 0x08B6EE80u); G[6] = G[2]; goto L_08A5E618; L_08A5E5DC: ; G[6] = G[17] << 31; G[2] = G[16] >> 1; G[2] = G[6] | G[2]; G[3] = G[17] >> 1; G[4] = G[16] & 0x1u; G[5] = 0u; G[4] = G[4] | G[2]; G[5] = G[5] | G[3]; call_here(rt, ctx, 0x08B6EE80u); G[4] = G[2]; G[5] = G[3]; G[6] = G[2]; G[7] = G[3]; call_here(rt, ctx, 0x08B71328u); G[6] = G[2]; L_08A5E618: ; G[7] = G[3]; G[4] = G[18]; G[5] = G[19]; call_here(rt, ctx, 0x08B6683Cu); G[4] = G[2]; G[5] = G[3]; call_here(rt, ctx, 0x08B6F1B8u); G[6] = G[2]; G[4] = frame.at(0u); G[7] = G[3]; call_here(rt, ctx, 0x08A5DEE8u); { const bool tk = G[21] == frame.at(0u); G[5] = word(rt, frame.at(0x0u)); if (tk) goto L_08A5E654; } G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); L_08A5E654: ; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B81F40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x2Cu), G[31]); G[5] = 0x1u; G[17] = G[7]; G[22] = G[9]; G[21] = G[8]; G[16] = G[6]; call_here(rt, ctx, 0x08B82260u); G[3] = G[17]; G[3] = (G[3] & ~0x80000000u) | ((0u << 31) & 0x80000000u); G[4] = G[17]; G[18] = G[2]; G[4] = (G[4] & ~0xFFF00000u) | ((0u << 20) & 0xFFF00000u); G[19] = G[3] >> 20; put(rt, frame.at(0x4u), G[4]); { const bool tk = G[19] == 0u; G[20] = G[18] + 0x14u; if (tk) goto L_08B81FAC; } G[2] = 0x00100000u; G[4] = G[4] | G[2]; put(rt, frame.at(0x4u), G[4]); L_08B81FAC: ; { const bool tk = G[16] == 0u; G[4] = frame.at(0u); if (tk) goto L_08B8203C; } put(rt, frame.at(0x0u), G[16]); call_here(rt, ctx, 0x08B815E4u); { const bool tk = G[2] != 0u; G[5] = G[2]; if (tk) goto L_08B820A4; } G[2] = word(rt, frame.at(0x0u)); G[16] = 0x2u; put(rt, G[18] + 0x14u, G[2]); G[2] = 0x1u; G[4] = word(rt, frame.at(0x4u)); if (G[4] == 0u) G[16] = G[2]; put(rt, G[20] + 0x4u, G[4]); put(rt, G[18] + 0x10u, G[16]); L_08B81FE4: ; { const bool tk = G[19] != 0u; G[2] = G[5] + G[19]; if (tk) goto L_08B82064; } L_08B81FEC: ; G[2] = G[16] << 2; G[2] = G[2] + G[20]; G[4] = word(rt, G[2] + 0xFFFFFFFCu); G[3] = G[5] + 0xFFFFFBCEu; put(rt, G[21] + 0x0u, G[3]); call_here(rt, ctx, 0x08B815DCu); G[16] = G[16] << 5; G[16] = G[16] - G[2]; put(rt, G[22] + 0x0u, G[16]); G[2] = G[18]; goto exit_; L_08B8203C: ; G[4] = frame.at(0x4u); call_here(rt, ctx, 0x08B815E4u); G[5] = G[2] + 0x20u; G[2] = word(rt, frame.at(0x4u)); G[3] = 0x1u; put(rt, G[18] + 0x10u, G[3]); G[16] = 0x1u; { const bool tk = G[19] == 0u; put(rt, G[18] + 0x14u, G[2]); if (tk) goto L_08B81FEC; } G[2] = G[5] + G[19]; L_08B82064: ; G[3] = 0x35u; G[2] = G[2] + 0xFFFFFBCDu; G[3] = G[3] - G[5]; put(rt, G[21] + 0x0u, G[2]); G[2] = G[18]; put(rt, G[22] + 0x0u, G[3]); goto exit_; L_08B820A4: ; G[3] = word(rt, frame.at(0x4u)); G[4] = word(rt, frame.at(0x0u)); G[2] = 0u - G[2]; G[3] = G[3] << (G[2] & 31u); G[3] = G[3] | G[4]; put(rt, G[18] + 0x14u, G[3]); G[16] = 0x2u; G[2] = 0x1u; G[4] = word(rt, frame.at(0x4u)); G[4] = G[4] >> (G[5] & 31u); if (G[4] == 0u) G[16] = G[2]; put(rt, frame.at(0x4u), G[4]); put(rt, G[18] + 0x10u, G[16]); put(rt, G[20] + 0x4u, G[4]); goto L_08B81FE4;  exit_: ; G[17] = word(rt, frame.at(0x14u)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B8D028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    insert_aux(rt, ctx, InsertAux{2u, 1u, 0x7FFFFFFFu, 0x08C9BA58u, 0x08B8CFF0u, 0x08B8CFE0u});
}
void func_08B8D1C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); G[18] = G[5]; G[17] = G[6]; G[7] = word(rt, G[4] + 0x4u); G[2] = word(rt, G[4] + 0x8u); { const bool tk = G[7] == G[2]; G[16] = G[4]; if (tk) goto L_08B8D274; } { const bool tk = G[7] == 0u; G[3] = G[7] + 0xFFFFFFFEu; if (tk) goto L_08B8D218; } G[2] = load8(G[7] + 0xFFFFFFFEu); store8(G[7] + 0x0u, G[2]); G[3] = load8(G[3] + 0x1u); store8(G[7] + 0x1u, G[3]); G[7] = word(rt, G[4] + 0x4u); L_08B8D218: ; G[5] = G[7] + 0xFFFFFFFEu; G[2] = G[5] - G[18]; G[7] = G[7] + 0x2u; put(rt, G[16] + 0x4u, G[7]); G[8] = sra(G[2], 1); G[4] = G[5]; G[9] = load8(G[17] + 0x0u); G[6] = load8(G[17] + 0x1u); { const bool tk = static_cast<std::int32_t>(G[8]) > 0; G[7] = 0u; if (tk) goto L_08B8D24C; } store8(G[18] + 0x0u, G[9]); goto L_08B8D26C; L_08B8D248: ; G[5] = G[5] + 0xFFFFFFFEu; L_08B8D24C: ; G[4] = G[4] + 0xFFFFFFFEu; G[3] = load8(G[4] + 0x0u); G[7] = G[7] + 0x1u; store8(G[5] + 0x0u, G[3]); G[2] = load8(G[4] + 0x1u); { const bool tk = G[8] != G[7]; store8(G[5] + 0x1u, G[2]); if (tk) goto L_08B8D248; } store8(G[18] + 0x0u, G[9]); L_08B8D26C: ; store8(G[18] + 0x1u, G[6]); goto L_08B8D348; L_08B8D274: ; G[4] = word(rt, G[4] + 0x0u); G[2] = 0x7FFF0000u; G[2] = G[2] | 0xFFFFu; G[3] = G[7] - G[4]; G[3] = sra(G[3], 1); { const bool tk = G[3] != G[2]; if (tk) goto L_08B8D29C; } G[4] = 0x08CA0000u; G[4] = 0x08C9BA58u; call_here(rt, ctx, 0x08B5ADDCu); L_08B8D29C: ; { const bool tk = G[3] == 0u; G[5] = 0x1u; if (tk) goto L_08B8D2C0; } G[5] = G[3] << 1; G[2] = G[5] < G[3] ? 1u : 0u; { const bool tk = G[2] != 0u; G[20] = 0xFFFFFFFEu; if (tk) goto L_08B8D2C4; } G[19] = 0u; { const bool tk = G[5] == 0u; G[20] = 0u; if (tk) goto L_08B8D2D4; } L_08B8D2C0: ; G[20] = G[5] << 1; L_08B8D2C4: ; G[4] = G[20]; call_here(rt, ctx, 0x08B35DA0u); G[4] = word(rt, G[16] + 0x0u); G[19] = G[2]; L_08B8D2D4: ; G[6] = G[19]; G[5] = G[18]; G[7] = frame.at(0x2u); call_here(rt, ctx, 0x08B8CFA8u); { const bool tk = G[2] == 0u; G[6] = G[2]; if (tk) goto L_08B8D2FC; } G[2] = load8(G[17] + 0x0u); store8(G[6] + 0x0u, G[2]); G[3] = load8(G[17] + 0x1u); store8(G[6] + 0x1u, G[3]); L_08B8D2FC: ; G[5] = word(rt, G[16] + 0x4u); G[4] = G[18]; G[6] = G[6] + 0x2u; G[7] = frame.at(0x1u); call_here(rt, ctx, 0x08B8CFA8u); G[4] = word(rt, G[16] + 0x0u); G[5] = word(rt, G[16] + 0x4u); G[6] = frame.at(0u); G[17] = G[2]; call_here(rt, ctx, 0x08B8CF98u); G[4] = word(rt, G[16] + 0x0u); { const bool tk = G[4] == 0u; G[2] = G[19] + G[20]; if (tk) goto L_08B8D33C; } call_here(rt, ctx, 0x08B35254u); G[2] = G[19] + G[20]; L_08B8D33C: ; put(rt, G[16] + 0x8u, G[2]); put(rt, G[16] + 0x4u, G[17]); put(rt, G[16] + 0x0u, G[19]); L_08B8D348: ; goto exit_;  exit_: ; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B943A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x1Cu), G[19]); G[22] = G[7]; G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; G[4] = word(rt, G[4] + 0x0u); G[2] = word(rt, G[4] + 0xFFFFFFF4u); G[6] = G[4] + 0xFFFFFFF4u; G[7] = word(rt, G[6] + 0x4u); G[3] = G[2] + G[22]; G[20] = G[3] - G[18]; G[2] = G[2] - G[5]; G[3] = G[7] < G[20] ? 1u : 0u; { const bool tk = G[3] != 0u; G[21] = G[2] - G[18]; if (tk) goto L_08B94408; } G[2] = word(rt, G[6] + 0x8u); { const bool tk = static_cast<std::int32_t>(G[2]) <= 0; if (tk) goto L_08B94528; } L_08B94408: ; G[5] = G[7]; G[4] = G[20]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08B93B20u); G[3] = G[2]; { const bool tk = G[16] == 0u; G[19] = G[2] + 0xCu; if (tk) goto L_08B94448; } G[2] = 0x1u; { const bool tk = G[16] != G[2]; G[5] = word(rt, G[17] + 0x0u); if (tk) goto L_08B9443C; } G[2] = sext(load8(G[5] + 0x0u), 8); store8(G[3] + 0xCu, G[2]); goto L_08B94448; L_08B9443C: ; G[4] = G[19]; G[6] = G[16]; call_here(rt, ctx, 0x08B79A88u); L_08B94448: ; if (G[21] == 0u) { G[4] = word(rt, G[17] + 0x0u); goto L_08B94484; } G[2] = word(rt, G[17] + 0x0u); G[3] = G[19] + G[16]; G[4] = G[3] + G[22]; G[2] = G[16] + G[2]; G[3] = 0x1u; { const bool tk = G[21] != G[3]; G[5] = G[2] + G[18]; if (tk) goto L_08B94478; } G[2] = sext(load8(G[5] + 0x0u), 8); store8(G[4] + 0x0u, G[2]); goto L_08B94480; L_08B94478: ; G[6] = G[21]; call_here(rt, ctx, 0x08B79A88u); L_08B94480: ; G[4] = word(rt, G[17] + 0x0u); L_08B94484: ; G[2] = 0x08D70000u; G[2] = 0x08D75298u; G[16] = G[4] + 0xFFFFFFF4u; if (G[16] == G[2]) { G[4] = G[19]; goto L_08B944BC; } G[4] = G[4] + 0xFFFFFFFCu; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) > 0) { G[4] = G[19]; goto L_08B944BC; } G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B93554u); G[4] = G[19]; L_08B944BC: ; put(rt, G[17] + 0x0u, G[19]); goto L_08B944F0; L_08B944C4: ; G[2] = 0x1u; G[5] = G[3] + G[18]; { const bool tk = G[21] != G[2]; G[4] = G[3] + G[22]; if (tk) goto L_08B944E4; } G[2] = sext(load8(G[5] + 0x0u), 8); store8(G[4] + 0x0u, G[2]); G[4] = word(rt, G[17] + 0x0u); goto L_08B944F0; L_08B944E4: ; G[6] = G[21]; call_here(rt, ctx, 0x08B79BACu); G[4] = word(rt, G[17] + 0x0u); L_08B944F0: ; G[2] = G[4] + G[20]; L_08B944F4: ; put(rt, G[4] + 0xFFFFFFFCu, 0u); L_08B944F8: ; put(rt, G[4] + 0xFFFFFFF4u, G[20]); store8(G[2] + 0x0u, 0u); goto exit_; L_08B94528: ; { const bool tk = G[21] == 0u; G[2] = G[4] + G[20]; if (tk) goto L_08B944F4; } { const bool tk = G[22] != G[18]; G[3] = G[4] + G[16]; if (tk) goto L_08B944C4; } put(rt, G[4] + 0xFFFFFFFCu, 0u); goto L_08B944F8;  exit_: ; G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08BB27BC (a0 = first, a1 = last, a2 = a function): for_each over an array of intrusive pointers. Each element is copied into the frame
// (reference count +0x10 up), the function called with it, and the copy dropped: at zero the object's child (+0xC) and then the object itself
// (after 0x08BB1FF0) go back to the pool 0x08BB1F48's free lists for 4- and 0x14-byte blocks (the cached bucket +0x34, else found by
// 0x08BB1D0C(+0x24, +0x28, &size, 0x08BB1B40)). v0 = the function.
void func_08BB27BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 first = ctx.gpr[4], last = ctx.gpr[5], function = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{21u, 0x24u}, {20u, 0x20u}, {18u, 0x18u}, {31u, 0x2Cu}, {22u, 0x28u}, {19u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[21] = last;
    ctx.gpr[20] = function;
    ctx.gpr[18] = first;
    // The pool's bucket for `size`-byte blocks; `block` (when not 0) goes on its free list.
    auto give_back = [&](u32 size, u32 block) {
        const u32 pool = call_set(rt, ctx, 0x08BB1F48u, {});
        ctx.gpr[17] = pool;
        ctx.gpr[3] = size;
        put(rt, frame.at(0x4u), size);
        u32 bucket = word(rt, pool + 0x34u);
        if (bucket == 0u || word(rt, bucket + 0x10u) != size) {
            const u32 found = call_set(rt, ctx, 0x08BB1D0Cu, {word(rt, pool + 0x24u), word(rt, pool + 0x28u), frame.at(0x4u), 0x08BB1B40u});
            bucket = word(rt, found);
            put(rt, pool + 0x34u, bucket);
        }
        ctx.gpr[4] = bucket;
        if (block == 0u) return;
        ctx.gpr[2] = word(rt, bucket + 0x1Cu) - 1u;
        put(rt, bucket + 0x1Cu, ctx.gpr[2]);
        ctx.gpr[3] = word(rt, bucket + 0xCu);
        put(rt, block, ctx.gpr[3]);
        put(rt, bucket + 0xCu, block);
    };
    ctx.gpr[22] = 0x08BB0000u;
    for (u32 at = first; at != last; at += 4u) {
        ctx.gpr[18] = at;
        const u32 element = word(rt, at);
        ctx.gpr[3] = element;
        put(rt, frame.at(0x0u), element);
        if (element != 0u) put(rt, element + 0x10u, word(rt, element + 0x10u) + 1u);
        call_set(rt, ctx, function, {frame.at(0u)});
        const u32 held = word(rt, frame.at(0x0u));
        ctx.gpr[16] = held;
        if (held == 0u) continue;
        const u32 uses = word(rt, held + 0x10u) - 1u;
        ctx.gpr[2] = uses;
        put(rt, held + 0x10u, uses);
        if (uses != 0u) continue;
        const u32 child = word(rt, held + 0xCu);
        ctx.gpr[19] = child;
        give_back(4u, child);
        put(rt, held + 0xCu, 0u);
        call_set(rt, ctx, 0x08BB1FF0u, {held});
        give_back(0x14u, held);
    }
    ctx.gpr[18] = last;
    ctx.gpr[2] = function;
}
void func_08C4F48C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    insert_aux(rt, ctx, InsertAux{8u, 3u, 0x1FFFFFFFu, 0x08CB539Cu, 0x08C4F17Cu, 0x08C4F16Cu});
}
// 0x088F0D34 (a0 = the object): a constructor. vtable 0x08CC3A50, the fields +0x4..+0x20 cleared, the member at +0xBC built (0x088AFAF0), a
// 16-byte node (0x08B62344) holding a 0x54-byte object (0x088F1310) and an 8-byte one (0x088F0EF4) linked at +0xCC, the shared static
// 0x08D76000 (vtable 0x08CC3B00, built once behind guard 0x08D75FF8) at +0xD0, and the 1- and 2-byte buffers at +0xD4 / +0xDC (the second
// filled with 0xFFFF through 0x08BB8760).
void func_088F0D34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {31u, 0x24u}, {20u, 0x20u}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}});
    ctx.gpr[19] = self;
    put(rt, self + 0x0u, 0x08CC3A50u);
    for (u32 at = 0x4u; at <= 0x1Cu; at += 4u) put(rt, self + at, 0u);
    put(rt, self + 0x20u, 0u);
    ctx.gpr[3] = self + 0x4u;
    ctx.gpr[2] = 0xFFFFFFFEu;  // a count-down delay loop from 3 that leaves -2
    call_set(rt, ctx, 0x088AFAF0u, {self + 0xBCu});
    store8(self + 0xC8u, 8u);
    ctx.gpr[18] = 1u;
    ctx.gpr[20] = self + 0xCCu;
    put(rt, self + 0xC4u, 0u);
    store8(self + 0xCBu, 0u);
    const u32 node = call_set(rt, ctx, 0x08B62344u, {0x10u});
    put(rt, node + 0x0u, 0u);
    ctx.gpr[16] = node;
    put(rt, node + 0x4u, 1u);
    u32 made = call_set(rt, ctx, 0x08B6461Cu, {0x54u});
    ctx.gpr[17] = made;
    call_set(rt, ctx, 0x088F1310u, {made});
    put(rt, node + 0x0u, made);
    put(rt, node + 0xCu, 1u);
    put(rt, node + 0x8u, 0u);
    made = call_set(rt, ctx, 0x08B6461Cu, {0x8u});
    ctx.gpr[17] = made;
    call_set(rt, ctx, 0x088F0EF4u, {made});
    put(rt, node + 0x8u, made);
    put(rt, self + 0xCCu, node);
    const u32 guard = 0x08D75FF8u, shared = 0x08D76000u;
    ctx.gpr[17] = guard;
    ctx.gpr[3] = 0x08D70000u;
    ctx.gpr[4] = guard;
    if (signed_byte(rt, guard) == 0u && call_set(rt, ctx, 0x08B6441Cu, {guard}) != 0u) {
        put(rt, shared, 0x08CC3B00u);
        call_set(rt, ctx, 0x08B64494u, {guard});
    } else {
        ctx.gpr[2] = 0x08D70000u;
    }
    ctx.gpr[18] = shared;
    ctx.gpr[16] = 1u;
    put(rt, self + 0xD0u, shared);
    ctx.gpr[17] = self + 0xDCu;
    put(rt, self + 0xD8u, 1u);
    put(rt, self + 0xD4u, 0u);
    const u32 one = call_set(rt, ctx, 0x08B6461Cu, {1u});
    put(rt, self + 0xD4u, one);
    put(rt, self + 0xDCu, 0u);
    put(rt, self + 0xE0u, 1u);
    const u32 two = call_set(rt, ctx, 0x08B6461Cu, {2u});
    put(rt, self + 0xE8u, 0u);
    store8(self + 0xC9u, 0u);
    store8(self + 0xCAu, 0u);
    const u32 count = word(rt, self + 0xE0u);
    put(rt, self + 0xDCu, two);
    put(rt, frame.at(0x0u), 0xFFFFFFFFu);
    ctx.gpr[2] = 0xFFFFFFFFu;
    call_set(rt, ctx, 0x08BB8760u, {two, two + (count << 1), frame.at(0u)});
    ctx.gpr[2] = word(rt, self + 0xDCu);  // v0: the buffer it also stores at +0xE4
    put(rt, self + 0xE4u, ctx.gpr[2]);
}
// 0x0898DDB8 (a0 = the object): after the layout (0x0898D9C0), the 128-entry ring from +0x220 to +0x224 is walked (0x0898DB58 per entry, next
// by 0x0898D9E8(object, &index, 0x80, 1)). When the content (+0x4AC) is shorter than the room (+0x4A8), it instead walks back from the newest
// entry adding widths (+0x428 bytes) while they fit, positions the drawing (0x08A4D738 on 0x08CF68E8: x 0, y from +0x4B0 and the space left)
// and draws that many entries forward from there.
void func_0898DDB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x10u}, {31u, 0x20u}, {19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}});
    auto &F = ctx.fpr;
    ctx.gpr[16] = self;
    call_set(rt, ctx, 0x0898D9C0u, {self});
    const s32 room = static_cast<s32>(word(rt, self + 0x4A8u));
    auto next = [&](u32 step) {
        call_set(rt, ctx, 0x0898D9E8u, {self, frame.at(0u), 0x80u, step});
        return word(rt, frame.at(0x0u));
    };
    if (!(static_cast<s32>(word(rt, self + 0x4ACu)) < room)) {
        u32 index = word(rt, self + 0x220u);
        put(rt, frame.at(0x0u), index);
        while (index != word(rt, self + 0x224u)) {
            call_set(rt, ctx, 0x0898DB58u, {self, index});
            index = next(1u);
        }
        ctx.gpr[2] = word(rt, self + 0x224u);  // v0: the end it compared with
        return;
    }
    const u32 newest_end = word(rt, self + 0x224u);
    u32 last = newest_end - 1u;
    if (static_cast<s32>(last) < 0) last = newest_end + 0x7Fu;
    put(rt, frame.at(0x0u), last);
    u32 at = word(rt, frame.at(0x0u));
    const u32 start = word(rt, self + 0x220u);
    u32 stop = start - 1u;
    if (static_cast<s32>(stop) < 0) stop = start + 0x7Fu;
    ctx.gpr[17] = stop;
    ctx.gpr[19] = at;
    ctx.gpr[2] = at + self;  // v0 when the ring is empty
    if (stop == at) return;
    u32 used = signed_byte(rt, at + self + 0x428u);
    ctx.gpr[18] = used;
    while (static_cast<s32>(used) < static_cast<s32>(word(rt, self + 0x4ACu))) {
        at = next(0xFFFFFFFFu);
        ctx.gpr[19] = at;
        if (stop == at) {
            at = 0u;
            ctx.gpr[19] = 0u;
            break;
        }
        used += signed_byte(rt, at + self + 0x428u);
        ctx.gpr[18] = used;
    }
    ctx.gpr[2] = 0x08CF0000u;  // v0 when nothing fits
    if (static_cast<s32>(used) <= 0) return;
    const u32 global = word(rt, 0x08CF68E8u);
    const u32 left = word(rt, self + 0x4ACu) - used;
    F[13] = psp_mul(psp_mul(float_at(rt, global + 0x68u) + 14.0f, float_at(rt, global + 0x24u)), static_cast<float>(static_cast<s32>(left)));
    F[13] = F[13] + float_at(rt, self + 0x4B0u);
    F[12] = 0.0f;
    F[0] = float_at(rt, self + 0x4B0u);
    ctx.gpr[17] = 0u;
    call_set(rt, ctx, 0x08A4D738u, {global});
    put(rt, frame.at(0x0u), at);
    u32 index = at;
    for (u32 drawn = 0u; drawn != used;) {
        ++drawn;
        ctx.gpr[17] = drawn;
        call_set(rt, ctx, 0x0898DB58u, {self, index});
        index = next(1u);
    }
}
// 0x089A4EFC (a0 = the owner, a1 = a key string, a2 = a pointer to a shared value): stores the value under the key in the owner's map at +0x154
// unless the key already holds a live one. The map is searched (0x08BF6204); a missing key is placed by lower_bound (0x08BF6164) and a string
// compare (length first, then 0x08B36170), and inserted when absent (0x08BF70B4 with a copy of the key, 0x08B88F44, released after). The
// node's value slot (+0x14) gets the value with its reference taken (0x08A5BA08) and the old one released (0x08A5B930).
void func_089A4EFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4], key = ctx.gpr[5], value_at = ctx.gpr[6];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{20u, 0x30u}, {19u, 0x2Cu}, {16u, 0x20u}, {31u, 0x3Cu}, {22u, 0x38u}, {21u, 0x34u}, {18u, 0x28u}, {17u, 0x24u}});
    ctx.gpr[20] = key;
    ctx.gpr[19] = value_at;
    ctx.gpr[16] = owner;
    if (word(rt, value_at) == 0u) return;
    const u32 map = owner + 0x154u, end = owner + 0x158u;
    ctx.gpr[18] = map;
    const u32 found = call_set(rt, ctx, 0x08BF6204u, {map, key});
    ctx.gpr[16] = end;
    u32 slot;
    if (found != end) {
        slot = found + 0x14u;
        ctx.gpr[17] = slot;
        const u32 held = word(rt, slot);
        ctx.gpr[3] = held;
        if (held != 0u && word(rt, held + 0x4u) != 0u) return;
    } else {
        const u32 hint = call_set(rt, ctx, 0x08BF6164u, {map, key});
        ctx.gpr[22] = hint;
        u32 node = hint;
        bool insert = hint == end;
        if (!insert) {
            const u32 text = word(rt, key);
            ctx.gpr[21] = hint;
            const u32 length = word(rt, text - 0xCu);
            put(rt, frame.at(0x4u), length);
            const u32 node_text = word(rt, hint + 0x10u);
            const u32 node_length = word(rt, node_text - 0xCu);
            put(rt, frame.at(0x8u), node_length);
            const u32 shorter = node_length < length ? node_length : length;
            ctx.gpr[17] = node_length;
            u32 order = length - node_length;
            const u32 compared = call_set(rt, ctx, 0x08B36170u, {text, node_text, shorter});
            if (compared != 0u) order = compared;
            ctx.gpr[16] = order;
            insert = static_cast<s32>(order) < 0;
        }
        if (insert) {
            const u32 copy = frame.at(0xCu);
            ctx.gpr[16] = copy;
            call_set(rt, ctx, 0x08B88F44u, {copy, key});
            put(rt, frame.at(0x10u), 0u);
            node = call_set(rt, ctx, 0x08BF70B4u, {map, hint, copy});
            ctx.gpr[17] = node;
            const u32 pending = word(rt, frame.at(0x10u));
            if (pending != 0u) call_set(rt, ctx, 0x08A5B930u, {pending});
            const u32 data = word(rt, frame.at(0xCu)), header = data - 0xCu;
            ctx.gpr[2] = 0x08D74FD0u;
            ctx.gpr[16] = header;
            ctx.gpr[4] = data;
            if (header != 0x08D74FD0u) {
                const u32 left = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
                if (static_cast<s32>(left) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
            }
        }
        ctx.gpr[21] = node;
        slot = node + 0x14u;
        ctx.gpr[17] = slot;
    }
    const u32 value = word(rt, value_at);
    ctx.gpr[2] = value;
    u32 kept = 0u;
    if (value != 0u) {
        kept = value;
        call_set(rt, ctx, 0x08A5BA08u, {value});
    }
    ctx.gpr[16] = kept;
    const u32 old = word(rt, slot);
    ctx.gpr[4] = old;
    if (old != 0u) call_set(rt, ctx, 0x08A5B930u, {old});
    put(rt, slot, kept);
}
// 0x08A2C9A4 (a0 = the result, a1 = the argument count, a2 = the arguments): with two arguments, their strings (virtual +0xC into the frame)
// are combined in a scratch object (0x08C1A21C, 0x08C1A0F0); a non-empty result (0x08C1A0C4) becomes a string (0x08B85EB8) and a value
// (0x08A6D2D4) assigned to the result (0x08A5BAD8). The scratch object ends (0x08C1A2D4) and every string is released (shared count
// 0x08B60804, freed by 0x08B8542C unless it is the empty string 0x08D74FD0).
void func_08A2C9A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 result = ctx.gpr[4], count = ctx.gpr[5], arguments = ctx.gpr[6];
    GuestFrame frame(ctx, 0x50u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x3Cu}, {17u, 0x34u}, {31u, 0x40u}, {18u, 0x38u}, {16u, 0x30u}});
    ctx.gpr[5] = static_cast<s32>(count) < 2 ? 1u : 0u;
    ctx.gpr[19] = result;
    ctx.gpr[17] = arguments;
    if (static_cast<s32>(count) < 2) return;
    auto text_of = [&](u32 argument, u32 out) {
        ctx.gpr[16] = argument;
        if (argument == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CAC140u});
        const u32 method = word(rt, word(rt, argument) + 0xCu);
        ctx.gpr[3] = word(rt, argument);
        ctx.gpr[2] = method;
        call_set(rt, ctx, method, {out, argument});
    };
    const u32 empty = 0x08D74FD0u;
    auto release = [&](u32 slot, u32 tag) {
        const u32 data = word(rt, slot), header = data - 0xCu;
        ctx.gpr[4] = data;
        ctx.gpr[16] = header;
        if (header == empty) return;
        if (static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu})) > 0) return;
        call_set(rt, ctx, 0x08B8542Cu, {header, tag});
    };
    text_of(word(rt, arguments + 0x0u), frame.at(0xCu));
    const u32 scratch = frame.at(0x14u);
    ctx.gpr[18] = scratch;
    text_of(word(rt, arguments + 0x4u), frame.at(0x10u));
    call_set(rt, ctx, 0x08C1A21Cu, {scratch, word(rt, frame.at(0xCu))});
    call_set(rt, ctx, 0x08C1A0F0u, {scratch, word(rt, frame.at(0x10u))});
    const u32 combined = call_set(rt, ctx, 0x08C1A0C4u, {scratch});
    ctx.gpr[16] = frame.at(0x4u);
    if (combined != 0u) {
        const u32 text = frame.at(0x4u), value = frame.at(0x8u);
        ctx.gpr[17] = value;
        call_set(rt, ctx, 0x08B85EB8u, {text, combined, frame.at(0x3u)});
        call_set(rt, ctx, 0x08A6D2D4u, {value, text});
        if (result != value) call_set(rt, ctx, 0x08A5BAD8u, {result, word(rt, value)});
        call_set(rt, ctx, 0x08A5B9ECu, {value});
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[17] = empty;
        release(text, frame.at(0x2u));
    } else {
        ctx.gpr[2] = 0x08D70000u;
        ctx.gpr[17] = empty;
    }
    call_set(rt, ctx, 0x08C1A2D4u, {scratch});
    release(frame.at(0x10u), frame.at(0x1u));
    release(frame.at(0xCu), frame.at(0u));
}
// 0x08AD1374 (a0 = the state, f12 = the time step): a level follower in decibels. A zero rate (+0xC) sets the level (+0x4) to 1.0. Otherwise the
// target is 0x08B84A2C(+0x0, -rate) x +0x8 (at most 1000), the level rises with the attack (+0x14) or falls with the release (+0x10) speed:
// both are converted to dB (20 log10 via 0x08B847A8 x 0.30103, floored at -120), mixed by min(speed x step, 1), and turned back
// (0x08B84954 of dB x 0.05 x ln 10); the result is capped at +0x1C / +0x0.
void func_08AD1374(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 state = ctx.gpr[4];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{16u, 0x0u}, {31u, 0x4u}, {57u, 0x24u}, {56u, 0x20u}, {55u, 0x1Cu}, {54u, 0x18u}, {53u, 0x14u}, {52u, 0x10u}});
    auto &F = ctx.fpr;
    ctx.gpr[16] = state;
    F[0] = 0.0f;
    F[13] = float_at(rt, state + 0xCu);
    ctx.set_fpu_condition(F[13] == F[0]);
    F[25] = F[12];
    if (ctx.fpu_condition()) {
        F[0] = 1.0f;
        put_float(rt, state + 0x4u, 1.0f);
        return;
    }
    F[12] = float_at(rt, state + 0x0u);
    F[1] = 1.0f;
    ctx.set_fpu_condition(F[0] <= F[12]);
    if (ctx.fpu_condition()) {
        F[13] = -F[13];
        call_set(rt, ctx, 0x08B84A2Cu, {state});
        F[2] = 1000.0f;
        F[1] = psp_mul(F[0], float_at(rt, state + 0x8u));
        ctx.set_fpu_condition(F[1] < F[2]);
        if (!ctx.fpu_condition()) F[1] = F[2];
    }
    F[0] = float_at(rt, state + 0x4u);
    ctx.set_fpu_condition(F[0] < F[1]);
    F[24] = ctx.fpu_condition() ? float_at(rt, state + 0x14u) : float_at(rt, state + 0x10u);
    F[22] = std::bit_cast<float>(0x3E9A209Bu);
    F[23] = 20.0f;
    F[21] = -120.0f;
    F[12] = F[1];
    call_set(rt, ctx, 0x08B847A8u, {state});
    F[0] = psp_mul(F[0], F[22]);
    F[20] = psp_mul(F[0], F[23]);
    ctx.set_fpu_condition(F[20] < F[21]);
    if (ctx.fpu_condition()) F[20] = F[21];
    F[12] = float_at(rt, state + 0x4u);
    call_set(rt, ctx, 0x08B847A8u, {state});
    F[0] = psp_mul(psp_mul(F[0], F[22]), F[23]);
    ctx.set_fpu_condition(F[0] < F[21]);
    if (ctx.fpu_condition()) F[0] = F[21];
    F[12] = psp_mul(F[24], F[25]);
    F[2] = 1.0f;
    ctx.set_fpu_condition(F[12] < F[2]);
    F[1] = F[20] - F[0];
    if (!ctx.fpu_condition()) F[12] = F[2];
    F[12] = psp_mul(F[12], F[1]);
    F[1] = std::bit_cast<float>(0x40135D8Eu);
    F[12] = F[0] + F[12];
    F[0] = std::bit_cast<float>(0x3D4CCCCDu);
    F[12] = psp_mul(psp_mul(F[12], F[0]), F[1]);
    call_set(rt, ctx, 0x08B84954u, {state});
    F[2] = F[0];
    F[1] = float_at(rt, state + 0x1Cu) / float_at(rt, state + 0x0u);
    F[0] = float_at(rt, state + 0x0u);
    ctx.set_fpu_condition(F[2] < F[1]);
    if (!ctx.fpu_condition()) F[2] = F[1];
    put_float(rt, state + 0x4u, F[2]);
}
// 0x08B130D4 (a0 = unused, a1 = a table, a2 = a string, a3 = its length): binary search of the table's entries (0x08B13038(table, i), the
// entry's length read by 0x08B12268 from its first byte on) for the string, comparing each string byte with the entry byte mapped through
// the case table 0x08C92BC0, then by length. Found: v0 = that entry. The table's two first bytes (through the case table) give the count
// (bits 1-11) and, in bit 0, whether a miss returns the entry just before the insertion point instead of 0.
void func_08B130D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 table = ctx.gpr[5], text = ctx.gpr[6], length = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(rt, ctx, frame, {{30u, 0x30u}, {23u, 0x2Cu}, {22u, 0x28u}, {21u, 0x24u}, {18u, 0x18u}, {31u, 0x34u}, {20u, 0x20u}, {19u, 0x1Cu}, {17u, 0x14u}, {16u, 0x10u}});
    const u32 fold = 0x08C92BC0u;
    ctx.gpr[23] = text;
    ctx.gpr[22] = 0x08C90000u;
    ctx.gpr[21] = table;
    ctx.gpr[18] = length;
    const u32 header = load8(fold + load8(table + 0x0u)) | (load8(fold + load8(table + 0x1u)) << 8);
    ctx.gpr[30] = header;
    u32 hi = (header >> 1) & 0x7FFu, lo = 0u, found = 0u;
    if (hi == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    ctx.gpr[19] = hi;
    ctx.gpr[20] = lo;
    for (;;) {
        const u32 mid = (lo + hi) >> 1;
        ctx.gpr[16] = mid;
        found = call_set(rt, ctx, 0x08B13038u, {table, mid});
        ctx.gpr[17] = found;
        put(rt, frame.at(0x0u), found + 0x1u);
        const u32 entry_length = call_set(rt, ctx, 0x08B12268u, {frame.at(0u)});
        const u32 common = length < entry_length ? length : entry_length;
        s32 difference = 0;
        u32 i = 0u;
        for (; i < common; ++i) {
            u32 at = word(rt, frame.at(0x0u));
            const u32 entry_byte = load8(fold + load8(at));
            put(rt, frame.at(0x0u), at + 0x1u);
            difference = static_cast<s32>(load8(text + i) - entry_byte);
            if (difference != 0) break;
        }
        if (i == common) {
            difference = static_cast<s32>(length - entry_length);
            if (difference == 0) {
                ctx.gpr[2] = found;
                return;
            }
        }
        if (difference >= 0) lo = mid + 1u;
        else hi = mid;
        ctx.gpr[19] = hi;
        ctx.gpr[20] = lo;
        if (!(lo < hi)) break;
    }
    found = 0u;
    if (lo != 0u && (header & 0x1u) != 0u) found = call_set(rt, ctx, 0x08B13038u, {table, lo - 1u});
    ctx.gpr[17] = found;
    ctx.gpr[2] = found;
}
void func_08B5CE18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[20] = G[5]; G[18] = G[4]; G[5] = word(rt, G[5] + 0x8u); put(rt, G[4] + 0x0u, G[6]); G[4] = G[5] << 2; put(rt, G[18] + 0x4u, 0u); put(rt, G[18] + 0x8u, G[5]); put(rt, G[18] + 0xCu, 0u); put(rt, G[18] + 0x10u, 0u); call_here(rt, ctx, 0x08B6461Cu); G[5] = word(rt, G[18] + 0x8u); { const bool tk = G[5] == 0u; put(rt, G[18] + 0x4u, G[2]); if (tk) goto L_08B5CEB0; } G[16] = 0u; G[2] = word(rt, G[20] + 0x4u); L_08B5CE74: ; G[3] = G[16] << 2; G[4] = word(rt, G[18] + 0x4u); G[2] = G[3] + G[2]; G[2] = word(rt, G[2] + 0x0u); G[3] = G[3] + G[4]; { const bool tk = G[2] == 0u; put(rt, G[3] + 0x0u, G[2]); if (tk) goto L_08B5CEA0; } G[4] = G[2] + 0x4u; G[5] = 0x1u; call_here(rt, ctx, 0x08B6085Cu); G[5] = word(rt, G[18] + 0x8u); L_08B5CEA0: ; G[16] = G[16] + 0x1u; G[2] = G[16] < G[5] ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[20] + 0x4u); goto L_08B5CE74; } L_08B5CEB0: ; G[4] = G[5] << 2; call_here(rt, ctx, 0x08B6461Cu); G[5] = word(rt, G[18] + 0x8u); { const bool tk = G[5] == 0u; put(rt, G[18] + 0xCu, G[2]); if (tk) goto L_08B5CF08; } G[16] = 0u; G[2] = word(rt, G[20] + 0xCu); L_08B5CECC: ; G[4] = G[16] << 2; G[3] = word(rt, G[18] + 0xCu); G[2] = G[2] + G[4]; G[2] = word(rt, G[2] + 0x0u); G[3] = G[3] + G[4]; { const bool tk = G[2] == 0u; put(rt, G[3] + 0x0u, G[2]); if (tk) goto L_08B5CEF8; } G[4] = G[2] + 0x4u; G[5] = 0x1u; call_here(rt, ctx, 0x08B6085Cu); G[5] = word(rt, G[18] + 0x8u); L_08B5CEF8: ; G[16] = G[16] + 0x1u; G[2] = G[16] < G[5] ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[20] + 0xCu); goto L_08B5CECC; } L_08B5CF08: ; G[4] = 0x18u; call_here(rt, ctx, 0x08B6461Cu); G[4] = G[2]; put(rt, G[18] + 0x10u, G[2]); G[3] = 0u; G[5] = 0x18u; L_08B5CF20: ; G[2] = G[3] + G[4]; G[3] = G[3] + 0x4u; { const bool tk = G[3] != G[5]; put(rt, G[2] + 0x0u, 0u); if (tk) goto L_08B5CF20; } G[16] = 0u; G[21] = 0x18u; G[2] = word(rt, G[20] + 0x10u); L_08B5CF3C: ; G[2] = G[2] + G[16]; G[4] = word(rt, G[2] + 0x0u); { const bool tk = G[4] == 0u; if (tk) goto L_08B5CF9C; } call_here(rt, ctx, 0x08B35F60u); G[3] = word(rt, G[18] + 0x10u); G[19] = G[2] + 0x1u; G[4] = G[19]; G[17] = G[3] + G[16]; call_here(rt, ctx, 0x08B6461Cu); G[3] = word(rt, G[18] + 0x10u); G[6] = word(rt, G[20] + 0x10u); put(rt, G[17] + 0x0u, G[2]); G[3] = G[3] + G[16]; G[6] = G[6] + G[16]; G[5] = word(rt, G[6] + 0x0u); G[4] = word(rt, G[3] + 0x0u); G[16] = G[16] + 0x4u; G[6] = G[19]; call_here(rt, ctx, 0x08B79A88u); if (G[16] != G[21]) { G[2] = word(rt, G[20] + 0x10u); goto L_08B5CF3C; } L_08B5CF9C: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[21] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08B5CFFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x18u), G[31]); put(rt, frame.at(0x14u), G[21]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[20] = G[5]; G[18] = G[4]; G[5] = word(rt, G[5] + 0x8u); put(rt, G[4] + 0x0u, G[6]); G[4] = G[5] << 2; put(rt, G[18] + 0x4u, 0u); put(rt, G[18] + 0x8u, G[5]); put(rt, G[18] + 0xCu, 0u); put(rt, G[18] + 0x10u, 0u); call_here(rt, ctx, 0x08B6461Cu); G[5] = word(rt, G[18] + 0x8u); { const bool tk = G[5] == 0u; put(rt, G[18] + 0x4u, G[2]); if (tk) goto L_08B5D094; } G[16] = 0u; G[2] = word(rt, G[20] + 0x4u); L_08B5D058: ; G[3] = G[16] << 2; G[4] = word(rt, G[18] + 0x4u); G[2] = G[3] + G[2]; G[2] = word(rt, G[2] + 0x0u); G[3] = G[3] + G[4]; { const bool tk = G[2] == 0u; put(rt, G[3] + 0x0u, G[2]); if (tk) goto L_08B5D084; } G[4] = G[2] + 0x4u; G[5] = 0x1u; call_here(rt, ctx, 0x08B6085Cu); G[5] = word(rt, G[18] + 0x8u); L_08B5D084: ; G[16] = G[16] + 0x1u; G[2] = G[16] < G[5] ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[20] + 0x4u); goto L_08B5D058; } L_08B5D094: ; G[4] = G[5] << 2; call_here(rt, ctx, 0x08B6461Cu); G[5] = word(rt, G[18] + 0x8u); { const bool tk = G[5] == 0u; put(rt, G[18] + 0xCu, G[2]); if (tk) goto L_08B5D0EC; } G[16] = 0u; G[2] = word(rt, G[20] + 0xCu); L_08B5D0B0: ; G[4] = G[16] << 2; G[3] = word(rt, G[18] + 0xCu); G[2] = G[2] + G[4]; G[2] = word(rt, G[2] + 0x0u); G[3] = G[3] + G[4]; { const bool tk = G[2] == 0u; put(rt, G[3] + 0x0u, G[2]); if (tk) goto L_08B5D0DC; } G[4] = G[2] + 0x4u; G[5] = 0x1u; call_here(rt, ctx, 0x08B6085Cu); G[5] = word(rt, G[18] + 0x8u); L_08B5D0DC: ; G[16] = G[16] + 0x1u; G[2] = G[16] < G[5] ? 1u : 0u; if (G[2] != 0u) { G[2] = word(rt, G[20] + 0xCu); goto L_08B5D0B0; } L_08B5D0EC: ; G[4] = 0x18u; call_here(rt, ctx, 0x08B6461Cu); G[4] = G[2]; put(rt, G[18] + 0x10u, G[2]); G[3] = 0u; G[5] = 0x18u; L_08B5D104: ; G[2] = G[3] + G[4]; G[3] = G[3] + 0x4u; { const bool tk = G[3] != G[5]; put(rt, G[2] + 0x0u, 0u); if (tk) goto L_08B5D104; } G[16] = 0u; G[21] = 0x18u; G[2] = word(rt, G[20] + 0x10u); L_08B5D120: ; G[2] = G[2] + G[16]; G[4] = word(rt, G[2] + 0x0u); { const bool tk = G[4] == 0u; if (tk) goto L_08B5D180; } call_here(rt, ctx, 0x08B35F60u); G[3] = word(rt, G[18] + 0x10u); G[19] = G[2] + 0x1u; G[4] = G[19]; G[17] = G[3] + G[16]; call_here(rt, ctx, 0x08B6461Cu); G[3] = word(rt, G[18] + 0x10u); G[6] = word(rt, G[20] + 0x10u); put(rt, G[17] + 0x0u, G[2]); G[3] = G[3] + G[16]; G[6] = G[6] + G[16]; G[5] = word(rt, G[6] + 0x0u); G[4] = word(rt, G[3] + 0x0u); G[16] = G[16] + 0x4u; G[6] = G[19]; call_here(rt, ctx, 0x08B79A88u); if (G[16] != G[21]) { G[2] = word(rt, G[20] + 0x10u); goto L_08B5D120; } L_08B5D180: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x8u)); G[21] = word(rt, frame.at(0x14u)); G[19] = word(rt, frame.at(0xCu)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
// 0x08B81D9C (a0 = a big integer: word count +0x10, words from +0x14; a1 = where the bit length of its top word goes): the top 53 bits as a
// double in [1, 2) (v0 = low word, v1 = high word with exponent 0x3FF), like dtoa's b2d; 0x08B815DC counts the top word's leading zeros.
void func_08B81D9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 big = ctx.gpr[4], bits_out = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{22u, 0x18u}, {20u, 0x10u}, {17u, 0x4u}, {16u, 0x0u}, {31u, 0x1Cu}, {21u, 0x14u}, {19u, 0xCu}, {18u, 0x8u}});
    const u32 first = big + 0x14u, end = first + (word(rt, big + 0x10u) << 2);
    u32 at = end - 0x4u;
    const u32 top = word(rt, at);
    const u32 zeros = call_set(rt, ctx, 0x08B815DCu, {top});
    put(rt, bits_out, 0x20u - zeros);
    u32 high, low;
    if (static_cast<s32>(zeros) < 11) {
        const u32 shift = 11u - zeros;
        high = (top >> (shift & 31u)) | 0x3FF00000u;
        const u32 next = first < at ? word(rt, end - 0x8u) >> (shift & 31u) : 0u;
        low = next | (top << ((zeros + 0x15u) & 31u));
    } else {
        u32 next = 0u;
        if (first < at) {
            at = end - 0x8u;
            next = word(rt, at);
        }
        const u32 k = zeros - 11u;
        if (k == 0u) {
            high = top | 0x3FF00000u;
            low = next;
        } else {
            const u32 back = 0x20u - k;
            high = ((top << (k & 31u)) | (next >> (back & 31u))) | 0x3FF00000u;
            const u32 after = first < at ? word(rt, at - 0x4u) >> (back & 31u) : 0u;
            low = after | (next << (k & 31u));
        }
    }
    ctx.gpr[2] = low;
    ctx.gpr[3] = high;
}
void func_08BB295C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[21] = G[5]; G[20] = G[6]; G[18] = G[4]; { const bool tk = G[4] == G[5]; if (tk) goto L_08BB2AD4; } G[22] = 0x08BB0000u; L_08BB2994: ; G[3] = word(rt, G[18] + 0x10u); { const bool tk = G[3] == 0u; put(rt, frame.at(0x0u), G[3]); if (tk) goto L_08BB29AC; } G[2] = word(rt, G[3] + 0x10u); G[2] = G[2] + 0x1u; put(rt, G[3] + 0x10u, G[2]); L_08BB29AC: ; { const std::uint32_t tg = G[20]; G[4] = frame.at(0u); call_here(rt, ctx, tg); } G[16] = word(rt, frame.at(0x0u)); { const bool tk = G[16] == 0u; if (tk) goto L_08BB2A64; } G[2] = word(rt, G[16] + 0x10u); G[2] = G[2] + 0xFFFFFFFFu; { const bool tk = G[2] != 0u; put(rt, G[16] + 0x10u, G[2]); if (tk) goto L_08BB2A64; } G[19] = word(rt, G[16] + 0xCu); call_here(rt, ctx, 0x08BB1F48u); G[3] = 0x4u; put(rt, frame.at(0x4u), G[3]); G[4] = word(rt, G[2] + 0x34u); { const bool tk = G[4] == 0u; G[17] = G[2]; if (tk) goto L_08BB2A7C; } G[2] = word(rt, G[4] + 0x10u); if (G[2] != G[3]) { G[4] = word(rt, G[17] + 0x24u); goto L_08BB2A80; } goto L_08BB2A98; L_08BB2A00: ; G[3] = word(rt, G[4] + 0xCu); G[2] = G[2] + 0xFFFFFFFFu; put(rt, G[4] + 0x1Cu, G[2]); put(rt, G[19] + 0x0u, G[3]); put(rt, G[4] + 0xCu, G[19]); G[4] = G[16]; L_08BB2A18: ; put(rt, G[16] + 0xCu, 0u); call_here(rt, ctx, 0x08BB1FF0u); call_here(rt, ctx, 0x08BB1F48u); G[3] = 0x14u; put(rt, frame.at(0x4u), G[3]); G[4] = word(rt, G[2] + 0x34u); { const bool tk = G[4] == 0u; G[17] = G[2]; if (tk) goto L_08BB2AA8; } G[2] = word(rt, G[4] + 0x10u); if (G[2] != G[3]) { G[4] = word(rt, G[17] + 0x24u); goto L_08BB2AAC; } goto L_08BB2AC4; L_08BB2A50: ; G[3] = word(rt, G[4] + 0xCu); G[2] = G[2] + 0xFFFFFFFFu; put(rt, G[4] + 0x1Cu, G[2]); put(rt, G[16] + 0x0u, G[3]); put(rt, G[4] + 0xCu, G[16]); L_08BB2A64: ; G[4] = G[18]; call_here(rt, ctx, 0x08B5F16Cu); { const bool tk = G[2] != G[21]; G[18] = G[2]; if (tk) goto L_08BB2994; } G[2] = G[20]; goto L_08BB2AD8; L_08BB2A7C: ; G[4] = word(rt, G[17] + 0x24u); L_08BB2A80: ; G[5] = word(rt, G[17] + 0x28u); G[6] = frame.at(0x4u); G[7] = 0x08BB1B40u; call_here(rt, ctx, 0x08BB1D0Cu); G[4] = word(rt, G[2] + 0x0u); put(rt, G[17] + 0x34u, G[4]); L_08BB2A98: ; if (G[19] != 0u) { G[2] = word(rt, G[4] + 0x1Cu); goto L_08BB2A00; } G[4] = G[16]; goto L_08BB2A18; L_08BB2AA8: ; G[4] = word(rt, G[17] + 0x24u); L_08BB2AAC: ; G[5] = word(rt, G[17] + 0x28u); G[6] = frame.at(0x4u); G[7] = 0x08BB1B40u; call_here(rt, ctx, 0x08BB1D0Cu); G[4] = word(rt, G[2] + 0x0u); put(rt, G[17] + 0x34u, G[4]); L_08BB2AC4: ; if (G[16] != 0u) { G[2] = word(rt, G[4] + 0x1Cu); goto L_08BB2A50; } goto L_08BB2A64; L_08BB2AD4: ; G[2] = G[20]; L_08BB2AD8: ; goto exit_;  exit_: ; G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[22] = word(rt, frame.at(0x28u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_08C50034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x30u), G[30]); put(rt, frame.at(0x2Cu), G[23]); put(rt, frame.at(0x28u), G[22]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); auto &F = ctx.fpr; G[20] = G[5] + 0xFFFFFFFFu; G[2] = 0u; G[30] = G[4]; G[23] = G[7]; G[22] = G[6]; G[21] = G[5]; G[16] = 0u; put(rt, frame.at(0x4u), 0u); put(rt, frame.at(0x8u), 0u); { const bool tk = G[20] == 0u; put(rt, frame.at(0xCu), 0u); if (tk) goto L_08C50098; } G[16] = G[20] << 1; G[4] = G[16]; call_here(rt, ctx, 0x08B35DA0u); L_08C50098: ; G[3] = G[2]; G[4] = G[2]; G[5] = G[20]; G[2] = G[2] + G[16]; G[6] = frame.at(0x2u); G[7] = frame.at(0x1u); put(rt, frame.at(0xCu), G[2]); put(rt, frame.at(0x4u), G[3]); put(rt, frame.at(0x8u), G[3]); store16(frame.at(0x2u), 0u); call_here(rt, ctx, 0x08C4FD44u); G[3] = word(rt, frame.at(0x4u)); G[2] = G[16] + G[3]; { const bool tk = G[20] == 0u; put(rt, frame.at(0x8u), G[2]); if (tk) goto L_08C50144; } G[2] = G[20] << 1; G[17] = G[2] + 0xFFFFFFFEu; G[16] = G[21]; G[18] = 0u; goto L_08C500EC; L_08C500E8: ; G[3] = word(rt, frame.at(0x4u)); L_08C500EC: ; G[4] = G[22]; G[19] = G[17] + G[3]; call_here(rt, ctx, 0x08B1EF70u); F[2] = std::bit_cast<float>(G[16]); G[3] = G[16] & 0x1u; G[2] = G[16] >> 1; F[1] = F[0]; G[18] = G[18] + 0x1u; G[17] = G[17] + 0xFFFFFFFEu; G[3] = G[3] | G[2]; { const bool tk = static_cast<std::int32_t>(G[16]) >= 0; F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[2]))); if (tk) goto L_08C50128; } F[2] = std::bit_cast<float>(G[3]); F[0] = static_cast<float>(static_cast<std::int32_t>(bits_of(F[2]))); F[0] = F[0] + F[0]; L_08C50128: ; F[0] = psp_mul(F[1], F[0]); G[16] = G[16] + 0xFFFFFFFFu; F[0] = std::bit_cast<float>(trunc_to_word(F[0])); G[2] = bits_of(F[0]); { const bool tk = G[20] != G[18]; store16(G[19] + 0x0u, G[2]); if (tk) goto L_08C500E8; } G[3] = word(rt, frame.at(0x4u)); L_08C50144: ; G[2] = G[21] < 0x2u ? 1u : 0u; G[16] = 0x1u; { const bool tk = G[2] != 0u; G[17] = 0u; if (tk) goto L_08C50184; } G[2] = G[3] + G[17]; goto L_08C50164; L_08C5015C: ; G[3] = word(rt, frame.at(0x4u)); G[2] = G[3] + G[17]; L_08C50164: ; G[6] = load16(G[2] + 0x0u); G[5] = G[16]; G[4] = G[30]; { const std::uint32_t tg = G[23]; G[16] = G[16] + 0x1u; call_here(rt, ctx, tg); } { const bool tk = G[21] != G[16]; G[17] = G[17] + 0x2u; if (tk) goto L_08C5015C; } G[3] = word(rt, frame.at(0x4u)); L_08C50184: ; G[5] = word(rt, frame.at(0x8u)); G[4] = G[3]; G[6] = frame.at(0u); call_here(rt, ctx, 0x08C4FC40u); G[4] = word(rt, frame.at(0x4u)); { const bool tk = G[4] == 0u; if (tk) goto L_08C501AC; } call_here(rt, ctx, 0x08B35254u); L_08C501AC: ; goto exit_;  exit_: ; G[20] = word(rt, frame.at(0x20u)); G[30] = word(rt, frame.at(0x30u)); G[23] = word(rt, frame.at(0x2Cu)); G[22] = word(rt, frame.at(0x28u)); G[21] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_089275B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[19] = G[4]; G[18] = 0u; G[17] = G[5]; call_here(rt, ctx, 0x089275A8u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088DA1C8u); G[4] = G[19]; G[16] = G[2]; call_here(rt, ctx, 0x089273D8u); G[4] = G[2]; G[5] = 0x20u; call_here(rt, ctx, 0x08925C7Cu); G[4] = G[19]; call_here(rt, ctx, 0x089273D8u); G[4] = G[2]; G[5] = 0x4u; call_here(rt, ctx, 0x08925BCCu); G[4] = G[19]; call_here(rt, ctx, 0x089273D8u); G[3] = word(rt, G[2] + 0x0u); G[4] = G[2]; G[2] = word(rt, G[3] + 0x28u); { const std::uint32_t tg = G[2]; G[5] = 0x1u; call_here(rt, ctx, tg); } G[3] = 0xFFFFFFFFu; G[4] = G[19]; if (G[2] != 0u) G[18] = G[3]; call_here(rt, ctx, 0x089273D8u); G[3] = word(rt, G[2] + 0x0u); G[4] = G[2]; G[2] = word(rt, G[3] + 0x28u); { const std::uint32_t tg = G[2]; G[5] = 0x2u; call_here(rt, ctx, tg); } G[3] = 0x1u; if (G[2] != 0u) G[18] = G[3]; G[16] = G[18] + G[16]; { const bool tk = static_cast<std::int32_t>(G[16]) >= 0; if (tk) goto L_08927670; } G[4] = G[17]; call_here(rt, ctx, 0x088DA168u); G[16] = G[2] + 0xFFFFFFFFu; L_08927670: ; G[4] = G[17]; call_here(rt, ctx, 0x088DA168u); G[2] = G[2] + 0xFFFFFFFFu; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[16]) ? 1u : 0u; G[4] = G[19]; if (G[2] != 0u) G[16] = 0u; call_here(rt, ctx, 0x089275A8u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088DA1C8u); { const bool tk = G[16] != G[2]; G[5] = G[16]; if (tk) goto L_089276D8; } G[3] = 0u; goto L_08927738; L_089276A8: ; G[16] = G[16] + G[18]; { const bool tk = static_cast<std::int32_t>(G[16]) >= 0; if (tk) goto L_089276C0; } call_here(rt, ctx, 0x088DA168u); G[16] = G[2] + 0xFFFFFFFFu; L_089276C0: ; G[4] = G[17]; call_here(rt, ctx, 0x088DA168u); G[2] = G[2] + 0xFFFFFFFFu; G[2] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[16]) ? 1u : 0u; if (G[2] != 0u) G[16] = 0u; G[5] = G[16]; L_089276D8: ; G[4] = G[17]; call_here(rt, ctx, 0x088DA170u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088E1ADCu); { const bool tk = G[2] != 0u; G[4] = G[17]; if (tk) goto L_089276A8; } G[4] = G[19]; call_here(rt, ctx, 0x089275A8u); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088DA1C8u); { const bool tk = G[2] == G[16]; G[4] = G[17]; if (tk) goto L_08927734; } G[5] = G[16]; call_here(rt, ctx, 0x088DA170u); G[5] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08927440u); G[2] = 0x1u; G[3] = 0x1u; store8(G[19] + 0x1FC5u, G[2]); goto L_08927738; L_08927734: ; G[3] = 0u; L_08927738: ; G[2] = G[3]; goto exit_;  exit_: ; G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } } if (tail != 0u) jump(ctx, tail); } }
void func_0892B268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { std::uint32_t tail = 0u; { { GuestFrame frame(ctx, 0x50u); put(rt, frame.at(0x40u), G[30]); put(rt, frame.at(0x3Cu), G[23]); put(rt, frame.at(0x44u), G[31]); put(rt, frame.at(0x38u), G[22]); put(rt, frame.at(0x34u), G[21]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x20u), G[16]); auto &F = ctx.fpr; G[2] = G[6] + 0x2Cu; G[1] = 0x3DCC0000u; G[1] = G[1] | 0xCCCDu; F[0] = std::bit_cast<float>(G[1]); G[30] = G[6]; G[23] = G[4]; put(rt, frame.at(0x10u), G[2]); put_float(rt, G[2] + 0x4u, F[0]); put(rt, frame.at(0x14u), G[5]); G[21] = word(rt, G[4] + 0x0u); G[18] = word(rt, G[4] + 0x4u); { const bool tk = G[18] == G[21]; G[4] = G[21]; if (tk) goto L_0892B34C; } G[17] = G[21]; L_0892B2CC: ; G[2] = load8(G[4] + 0x0u); G[3] = 0x10u; { const bool tk = G[2] != G[3]; G[20] = load8(G[4] + 0x1u); if (tk) goto L_0892B340; } G[4] = G[20]; call_here(rt, ctx, 0x0892B240u); { const bool tk = static_cast<std::int32_t>(G[2]) < 0; G[22] = G[2]; if (tk) goto L_0892B340; } { const bool tk = G[21] == G[18]; G[16] = G[21]; if (tk) goto L_0892B340; } G[2] = load8(G[16] + 0x1u); L_0892B2F8: ; G[19] = load8(G[16] + 0x0u); G[16] = G[16] + 0x2u; { const bool tk = G[20] == G[2]; G[4] = G[2]; if (tk) goto L_0892B338; } call_here(rt, ctx, 0x0892B240u); { const bool tk = G[22] != G[2]; if (tk) goto L_0892B338; } { const bool tk = G[19] == 0u; G[2] = word(rt, frame.at(0x10u)); if (tk) goto L_0892B338; } G[1] = 0x3F000000u; F[0] = std::bit_cast<float>(G[1]); put_float(rt, G[2] + 0x4u, F[0]); G[21] = word(rt, G[23] + 0x0u); G[18] = word(rt, G[23] + 0x4u); goto L_0892B34C; L_0892B338: ; if (G[16] != G[18]) { G[2] = load8(G[16] + 0x1u); goto L_0892B2F8; } L_0892B340: ; G[17] = G[17] + 0x2u; { const bool tk = G[17] != G[18]; G[4] = G[17]; if (tk) goto L_0892B2CC; } L_0892B34C: ; { const bool tk = G[21] == G[18]; G[16] = G[21]; if (tk) goto L_0892B3E0; } G[2] = 0x08CC0000u; G[3] = 0x08CC0000u; G[22] = 0x08CC6338u; G[21] = 0x08CC6538u; G[20] = 0x2u; G[19] = 0x20u; G[18] = 0x1u; put(rt, frame.at(0x0u), G[20]); L_0892B374: ; put(rt, frame.at(0x4u), G[19]); G[4] = word(rt, frame.at(0x14u)); G[5] = load8(G[16] + 0x1u); G[17] = load8(G[16] + 0x0u); call_here(rt, ctx, 0x08B21590u); G[3] = G[2]; G[2] = G[2] << 1; { const bool tk = G[17] != G[18]; G[2] = G[2] + G[21]; if (tk) goto L_0892B3B4; } G[2] = G[3] << 1; G[3] = sext(load16(G[30] + 0x4u), 16); { const bool tk = G[3] != 0u; G[2] = G[2] + G[22]; if (tk) goto L_0892B3D0; } G[2] = sext(load16(G[2] + 0x0u), 16); G[8] = 0u - G[2]; goto L_0892B3B8; L_0892B3B4: ; G[8] = load16(G[2] + 0x0u); L_0892B3B8: ; G[7] = G[17]; G[4] = 0u; G[5] = frame.at(0u); G[6] = G[30]; G[9] = 0u; call_here(rt, ctx, 0x0892AE14u); L_0892B3D0: ; G[2] = word(rt, G[23] + 0x4u); G[16] = G[16] + 0x2u; if (G[16] != G[2]) { put(rt, frame.at(0x0u), G[20]); goto L_0892B374; } L_0892B3E0: ; goto exit_;  exit_: ; G[30] = word(rt, frame.at(0x40u)); G[23] = word(rt, frame.at(0x3Cu)); G[22] = word(rt, frame.at(0x38u)); G[21] = word(rt, frame.at(0x34u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[17] = word(rt, frame.at(0x24u)); G[16] = word(rt, frame.at(0x20u)); } } if (tail != 0u) jump(ctx, tail); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_329
} // namespace gt::game

#define HOT329(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_329::name, gt::ReturnKind::kind)
HOT329(0x08C7A470u, func_08C7A470, Word);
HOT329(0x0898DC1Cu, func_0898DC1C, Word);
HOT329(0x089D9B90u, func_089D9B90, Word);
// HOT329(0x08AD9404u, func_08AD9404, Word);  // no comparable synthetic case yet
HOT329(0x08C7835Cu, func_08C7835C, Word);
// HOT329(0x08A5E4E0u, func_08A5E4E0, Word);  // no comparable synthetic case yet
HOT329(0x08B81F40u, func_08B81F40, Word);
HOT329(0x08B8D028u, func_08B8D028, Word);
// HOT329(0x08B8D1C8u, func_08B8D1C8, Word);  // no comparable synthetic case yet
HOT329(0x08B943A0u, func_08B943A0, Word);
HOT329(0x08BB27BCu, func_08BB27BC, Word);
HOT329(0x08C4F48Cu, func_08C4F48C, Word);
HOT329(0x088F0D34u, func_088F0D34, Word);
HOT329(0x0898DDB8u, func_0898DDB8, Word);
HOT329(0x089A4EFCu, func_089A4EFC, Word);
HOT329(0x08A2C9A4u, func_08A2C9A4, Word);
HOT329(0x08AD1374u, func_08AD1374, Word);
HOT329(0x08B130D4u, func_08B130D4, Word);
// HOT329(0x08B5CE18u, func_08B5CE18, Word);  // no comparable synthetic case yet
HOT329(0x08B5CFFCu, func_08B5CFFC, Word);
HOT329(0x08B81D9Cu, func_08B81D9C, Word);
HOT329(0x08BB295Cu, func_08BB295C, Word);
HOT329(0x08C50034u, func_08C50034, Word);
// HOT329(0x089275B0u, func_089275B0, Word);  // no comparable synthetic case yet
// HOT329(0x0892B268u, func_0892B268, Word);  // no comparable synthetic case yet
