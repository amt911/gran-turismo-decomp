// Hot batch 221: fifty-five-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_221 {
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
    if (!(value >= -2217483648.0f && value < 2217483648.0f)) return 0x7FFFFFFF;
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

void func_08A06B1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x50u), G[31]); G[16] = 0x08D74FD0u; G[2] = G[16] + 0xCu; G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[3] = 0xFFFFFFFFu; G[19] = frame.at(0x20u); G[18] = G[4]; G[4] = G[5]; G[6] = word(rt, G[5]); put(rt, frame.at(8u), G[2]); G[2] = 0x08CCE190u; put(rt, frame.at(0xCu), G[3]); put(rt, frame.at(0x30u), G[3]); put_float(rt, frame.at(0x20u), ctx.fpr[0]); put(rt, frame.at(4u), G[2]); put_float(rt, frame.at(0x2Cu), ctx.fpr[0]); put_float(rt, frame.at(0x28u), ctx.fpr[0]); put_float(rt, frame.at(0x24u), ctx.fpr[0]); put(rt, frame.at(0x10u), G[19]); G[2] = word(rt, G[6] + 8u); G[5] = frame.at(4u); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(8u)); G[2] = 0x08CCD990u; G[17] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(4u), G[2]); G[4] = G[3] - 4u; if (G[17] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[18]; G[5] = G[19]; call_here(rt, ctx, 0x08A05640u); G[16] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); } }
// A two-mode dispose helper over a shared holder: mode 0 releases the target's reference inside the holder
// fetched by 0x08A70338 (skipping the self-assignment) and drops the temporary; mode 1 instead copies the
// value into the holder's +0x14 buffer through 0x08A71E70 and then drops both the target's reference and the
// temporary. Any other mode returns 1 untouched. A null holder raises the assert at 0x08CB0928.
// A two-mode dispose helper over a shared holder: mode 0 releases the target's reference inside the holder
// fetched by 0x08A70338 (skipping the self-assignment) and drops the temporary; mode 1 instead copies the
// value into the holder's +0x14 buffer through 0x08A71E70 and then drops both the target's reference and the
// temporary. Any other mode returns 1 untouched. A null holder raises the assert at 0x08CB0928.
// A two-mode dispose helper over a shared holder: mode 0 releases the target's reference inside the holder
// fetched by 0x08A70338 (skipping the self-assignment) and drops the temporary; mode 1 instead copies the
// value into the holder's +0x14 buffer through 0x08A71E70 and then drops both the target's reference and the
// temporary. Any other mode returns 1 untouched. A null holder raises the assert at 0x08CB0928.
void func_08A7051C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4], argument = ctx.gpr[5], mode = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    ctx.gpr[17] = target;
    ctx.gpr[16] = value;
    if (mode == 0u) {
        const u32 temporary = frame.at(0x8u);
        ctx.gpr[18] = temporary;
        call_set(rt, ctx, 0x08A70338u, {temporary, argument});
        const u32 holder = word(rt, temporary);
        ctx.gpr[16] = holder;
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0928u});
        if (target != holder + 0x14u) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, holder + 0x14u)});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        return;
    }
    if (mode != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 temporary = frame.at(0x4u);
    ctx.gpr[18] = temporary;
    call_set(rt, ctx, 0x08A70338u, {temporary, argument});
    const u32 holder = word(rt, temporary);
    ctx.gpr[17] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0928u});
    call_set(rt, ctx, 0x08A71E70u, {frame.at(0u), value});
    if (holder + 0x14u != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {holder + 0x14u, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}
void func_08A06BF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x50u), G[31]); G[16] = 0x08D74FD0u; G[2] = G[16] + 0xCu; G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[3] = 0xFFFFFFFFu; G[19] = frame.at(0x20u); G[18] = G[4]; G[4] = G[5]; G[6] = word(rt, G[5]); put(rt, frame.at(8u), G[2]); G[2] = 0x08CCE190u; put(rt, frame.at(0xCu), G[3]); put(rt, frame.at(0x30u), G[3]); put_float(rt, frame.at(0x20u), ctx.fpr[0]); put(rt, frame.at(4u), G[2]); put_float(rt, frame.at(0x2Cu), ctx.fpr[0]); put_float(rt, frame.at(0x28u), ctx.fpr[0]); put_float(rt, frame.at(0x24u), ctx.fpr[0]); put(rt, frame.at(0x10u), G[19]); G[2] = word(rt, G[6] + 8u); G[5] = frame.at(4u); call_here(rt, ctx, G[2]); G[3] = word(rt, frame.at(8u)); G[2] = 0x08CCD990u; G[17] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; put(rt, frame.at(4u), G[2]); G[4] = G[3] - 4u; if (G[17] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[17]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[18]; G[5] = G[19]; call_here(rt, ctx, 0x08A05490u); G[16] = word(rt, frame.at(0x40u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); } }
// A two-mode dispose helper over a shared holder: mode 0 releases the target's reference inside the holder
// fetched by 0x08A70338 (skipping the self-assignment) and drops the temporary; mode 1 instead copies the
// value into the holder's +0x14 buffer through 0x08A71E70 and then drops both the target's reference and the
// temporary. Any other mode returns 1 untouched. A null holder raises the assert at 0x08CB0928.
// A two-mode dispose helper over a shared holder: mode 0 releases the target's reference inside the holder
// holder fetched by 0x08A70F28 (skipping the self-assignment) and drops the temporary; mode 1 instead copies the
// value into the holder's +0x14 buffer through 0x08A71E70 and then drops both the target's reference and the
// temporary. Any other mode returns 1 untouched. A null holder raises the assert at 0x08CB0928.
// A two-mode dispose helper over a shared holder: mode 0 releases the target's reference inside the holder
// holder fetched by 0x08A70F28 (skipping the self-assignment) and drops the temporary; mode 1 instead copies the
// value into the holder's +0x14 buffer through 0x08A71E70 and then drops both the target's reference and the
// temporary. Any other mode returns 1 untouched. A null holder raises the assert at 0x08CB0928.
// A two-mode dispose helper over a shared holder: mode 0 releases the target's reference inside the holder
// holder fetched by 0x08A70F28 (skipping the self-assignment) and drops the temporary; mode 1 instead copies the
// value into the holder's +0x14 buffer through 0x08A71E70 and then drops both the target's reference and the
// temporary. Any other mode returns 1 untouched. A null holder raises the assert at 0x08CB0928.
void func_08A71294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 target = ctx.gpr[4], argument = ctx.gpr[5], mode = ctx.gpr[6], value = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {16u, 0x10u}, {31u, 0x1Cu}, {18u, 0x18u}});
    ctx.gpr[17] = target;
    ctx.gpr[16] = value;
    if (mode == 0u) {
        const u32 temporary = frame.at(0x8u);
        ctx.gpr[18] = temporary;
        call_set(rt, ctx, 0x08A70F28u, {temporary, argument});
        const u32 holder = word(rt, temporary);
        ctx.gpr[16] = holder;
        if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0A14u});
        if (target != holder + 0x14u) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, holder + 0x14u)});
        call_set(rt, ctx, 0x08A5B9ECu, {temporary});
        return;
    }
    if (mode != 1u) {
        ctx.gpr[2] = 1u;
        return;
    }
    const u32 temporary = frame.at(0x4u);
    ctx.gpr[18] = temporary;
    call_set(rt, ctx, 0x08A70F28u, {temporary, argument});
    const u32 holder = word(rt, temporary);
    ctx.gpr[17] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB0A14u});
    call_set(rt, ctx, 0x08A71E70u, {frame.at(0u), value});
    if (holder + 0x14u != frame.at(0u)) call_set(rt, ctx, 0x08A5BAD8u, {holder + 0x14u, word(rt, frame.at(0u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}
// Publishes a record into a container: the first call returns a handle (zero stops the work), the second a
// second pointer (also fatal when zero); the record buffer at frame + 4 is filled through three helpers and
// then released, dropping its counted header unless it is the shared empty one, and the scratch buffer at
// frame + 8 is released too.
void func_08820D1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 owner = ctx.gpr[4], argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{31u, 0x24u}, {20u, 0x20u}, {18u, 0x18u}, {17u, 0x14u}, {16u, 0x10u}, {19u, 0x1Cu}});
    ctx.gpr[20] = argument;
    ctx.gpr[18] = owner;
    const u32 handle = call_set(rt, ctx, 0x08820B14u, {owner, argument});
    const u32 second = call_set(rt, ctx, 0x08820AC0u, {owner, argument});
    const u32 record = frame.at(0x4u), scratch = frame.at(0x8u);
    ctx.gpr[19] = record;
    ctx.gpr[16] = handle;
    ctx.gpr[17] = second;
    if (handle == 0u) return;
    if (second == 0u) return;
    call_set(rt, ctx, 0x08B85EB8u, {record, owner, frame.at(0x1u)});
    call_set(rt, ctx, 0x0899C764u, {scratch, handle, second, record, argument});
    call_set(rt, ctx, 0x089A9324u, {handle, scratch});
    call_set(rt, ctx, 0x08A5B9ECu, {scratch});
    const u32 data = word(rt, record);
    const u32 header = data - 0xCu;
    ctx.gpr[2] = 0x08D74FD0u;
    ctx.gpr[16] = header;
    if (header != 0x08D74FD0u) {
        const u32 count = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(count) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
}
// Sets the flag byte at +0x5BE of the object behind the +4 slot (when the count argument is positive): resolves
// the slot with 0x088297A8 and 0x088294D0 (asserting the inner pointer), releases the slot, turns it into the
// target with 0x0885EDF8, and stores the inverse of the +0x30 virtual method's answer of the node at the flag
// source (asserted). The +0 temporary is then handed to 0x08A85B1C, moved into the object and released.
void func_0882E548(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 object = ctx.gpr[4], count = ctx.gpr[6], flag_source = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{17u, 0x14u}, {19u, 0x1Cu}, {18u, 0x18u}, {31u, 0x20u}, {16u, 0x10u}});
    const u32 slot = frame.at(0x4u), temporary = frame.at(0u);
    ctx.gpr[17] = slot;
    ctx.gpr[19] = object;
    ctx.gpr[18] = flag_source;
    if (static_cast<s32>(count) <= 0) return;
    call_set(rt, ctx, 0x088297A8u, {slot, ctx.gpr[5]});
    const u32 holder = word(rt, slot);
    ctx.gpr[16] = holder;
    if (holder == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2F8u});
    const u32 inner = call_set(rt, ctx, 0x088294D0u, {holder});
    ctx.gpr[16] = inner;
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    const u32 target = call_set(rt, ctx, 0x0885EDF8u, {inner});
    ctx.gpr[17] = target;
    const u32 node = word(rt, flag_source);
    ctx.gpr[16] = node;
    if (node == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08C9A2B4u});
    const u32 state = call_set(rt, ctx, word(rt, word(rt, node) + 0x30u), {node});
    store8(target + 0x5BEu, state ^ 1u);
    call_set(rt, ctx, 0x08A85B1Cu, {temporary, 0u});
    if (object != temporary) call_set(rt, ctx, 0x08A5BAD8u, {object, word(rt, temporary)});
    call_set(rt, ctx, 0x08A5B9ECu, {temporary});
}
void func_0882E8B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x20u), G[31]); G[19] = frame.at(8u); G[17] = G[4]; G[4] = G[19]; G[18] = frame.at(4u); call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(8u)); G[4] = 0x08C9A2F8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[16] = G[2]; G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[16]; call_here(rt, ctx, 0x0885E710u); G[4] = G[2]; call_here(rt, ctx, 0x08917A98u); G[4] = G[18]; G[5] = 0u; if (G[2] != 0u) { call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != G[18]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); } else { G[4] = frame.at(0u); G[5] = 1u; call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[17]; if (G[17] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08867D0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put_float(rt, frame.at(0x30u), ctx.fpr[20]); G[21] = G[4]; G[4] += 0x600u; G[19] = G[5]; call_here(rt, ctx, 0x08893788u); G[20] = word(rt, G[2] + 0x263Cu); if (static_cast<std::int32_t>(G[20]) > 0) { G[16] = G[2]; G[18] = 0u; G[17] = G[2] + 0x230Cu; do { G[3] = word(rt, G[19]); G[5] = word(rt, G[16] + 0x22F4u); G[4] = G[19]; G[2] = word(rt, G[3] + 0x1Cu); ctx.fpr[20] = float_at(rt, G[16] + 0x22C4u); G[18] += 1u; call_here(rt, ctx, G[2]); G[4] = G[2]; G[5] = frame.at(0u); G[16] += 4u; if (G[2] != 0u) { G[2] = word(rt, G[2]); G[3] = word(rt, G[2] + 0x10u); call_here(rt, ctx, G[3]); ctx.fpr[13] = ctx.fpr[20]; G[4] = G[21]; G[5] = frame.at(0u); G[1] = 0x42480000u; ctx.fpr[12] = std::bit_cast<float>(G[1]); G[1] = 0x3F800000u; ctx.fpr[14] = std::bit_cast<float>(G[1]); G[6] = G[17]; call_here(rt, ctx, 0x088679D4u); } G[17] += 0x80u; } while (G[20] != G[18]); } G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } ctx.fpr[20] = f20; }
void func_088782F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[16]); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x2Cu), G[31]); put(rt, frame.at(0x28u), G[18]); G[16] = frame.at(4u); G[17] = G[4]; G[4] = G[16]; call_here(rt, ctx, 0x08AB85F0u); G[4] = G[16]; call_here(rt, ctx, 0x08AB856Cu); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x088755B4u); G[4] = G[16]; G[18] = G[2]; call_here(rt, ctx, 0x08AB856Cu); G[6] = G[2]; G[2] = word(rt, frame.at(0xCu)); G[5] = 0x08CA0088u; G[4] = G[17]; if (static_cast<std::int32_t>(G[2]) > 0) { call_here(rt, ctx, 0x088756F0u); G[4] = G[2]; G[5] = 2u; if (G[2] != 0u) { call_here(rt, ctx, 0x0886A9A0u); G[4] = G[2]; G[5] = G[18]; if (G[2] != 0u) call_here(rt, ctx, 0x08881D28u); } } G[3] = word(rt, frame.at(4u)); G[2] = 0x08D75298u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B93554u); } G[16] = word(rt, frame.at(0x20u)); G[17] = word(rt, frame.at(0x24u)); G[18] = word(rt, frame.at(0x28u)); } }
void func_0887FA54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[17]); put(rt, frame.at(0x34u), G[31]); put(rt, frame.at(0x30u), G[20]); put(rt, frame.at(0x2Cu), G[19]); put(rt, frame.at(0x28u), G[18]); put(rt, frame.at(0x20u), G[16]); G[17] = G[4]; G[4] = 0x60u; G[20] = G[5]; G[19] = G[6]; G[18] = G[17] + 4u; call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[2] = 0x08CC1218u; put(rt, G[16], G[2]); G[4] = 0x10u; put(rt, G[17], G[16]); call_here(rt, ctx, 0x08B62344u); G[3] = 0x08CC1258u; G[7] = 1u; put(rt, G[2] + 4u, G[7]); G[6] = G[16]; G[5] = G[16]; put(rt, G[2] + 8u, G[7]); G[4] = G[18]; put(rt, G[2] + 0xCu, G[16]); put(rt, G[2], G[3]); put(rt, G[17] + 4u, G[2]); call_here(rt, ctx, 0x08B8E380u); G[5] = G[19]; G[4] = word(rt, G[20] + 0x74u); G[16] = word(rt, G[17]); G[4] += 4u; call_here(rt, ctx, 0x088854A8u); put(rt, frame.at(4u), G[3]); G[4] = G[16] + 0x10u; put(rt, frame.at(0x10u), G[2]); put(rt, frame.at(0u), G[2]); put(rt, frame.at(0x14u), G[3]); put(rt, G[16] + 0x50u, G[2]); G[2] = word(rt, frame.at(4u)); put(rt, G[16] + 0x54u, G[2]); call_here(rt, ctx, 0x08B4E648u); G[2] = G[17]; G[17] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x30u)); G[19] = word(rt, frame.at(0x2Cu)); G[18] = word(rt, frame.at(0x28u)); G[16] = word(rt, frame.at(0x20u)); } }
void func_088A2BF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = G[5] << 2; G[3] = G[5] << 7; G[3] -= G[2]; G[2] = 0x08CC27E8u; G[19] = G[4]; G[18] = G[5]; put(rt, G[4], G[2]); G[4] = G[3] + 4u; call_here(rt, ctx, 0x08B6461Cu); put(rt, G[2], G[18]); G[20] = G[2] + 4u; if (G[18] != 0u) { G[16] = G[20]; G[17] = 0u; do { G[4] = G[16]; G[17] += 1u; call_here(rt, ctx, 0x088A0F44u); G[16] += 0x7Cu; } while (G[18] != G[17]); } G[4] = 0x40u; put(rt, G[19] + 4u, G[20]); call_here(rt, ctx, 0x08B62344u); G[16] = G[2]; G[6] = word(rt, G[19] + 4u); G[4] = G[2]; G[5] = G[18]; call_here(rt, ctx, 0x088A10B4u); put(rt, G[19] + 8u, G[16]); G[4] = G[18]; call_here(rt, ctx, 0x08B6461Cu); put(rt, G[19] + 0xCu, G[2]); if (static_cast<std::int32_t>(G[18]) > 0) { G[3] = 0u; G[4] = 0xFFFFFFFFu; do { G[2] = word(rt, G[19] + 0xCu); G[2] += G[3]; G[3] += 1u; store8(G[2], G[4]); } while (G[18] != G[3]); } G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } }
void func_088B0418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0xCu), G[31]); G[18] = G[6]; G[17] = G[4]; G[16] = G[5]; call_here(rt, ctx, 0x08AC0640u); G[2] = 0x088B0C18u; G[3] = 0x08CC2FC0u; put(rt, G[17] + 0x30u, G[2]); put(rt, G[17], G[3]); put(rt, G[17] + 0x20u, 0u); put(rt, G[17] + 0x24u, 0u); put(rt, G[17] + 0x28u, 0u); put(rt, G[17] + 0x2Cu, 0u); put(rt, G[17] + 0x34u, 0u); G[2] = word(rt, G[16]); put(rt, G[17] + 0x38u, G[2]); G[2] = word(rt, G[16] + 4u); put(rt, G[17] + 0x3Cu, G[2]); if (G[2] != 0u) { G[3] = G[2] + 4u; G[2] = word(rt, G[3]) + 1u; put(rt, G[3], G[2]); G[2] = 1u; } G[2] = word(rt, G[18]); put(rt, G[17] + 0x48u, G[2]); G[2] = word(rt, G[18] + 4u); put(rt, G[17] + 0x4Cu, G[2]); if (G[2] != 0u) { G[6] = G[2] + 4u; G[2] = word(rt, G[6]) + 1u; put(rt, G[6], G[2]); G[2] = 1u; } G[2] = 0x08D752A4u; put(rt, G[17] + 0x58u, G[2]); put(rt, G[17] + 0x5Cu, 0u); put(rt, G[17] + 0x50u, 0u); put(rt, G[17] + 0x54u, 0u); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } }
void func_088DB3CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x0u), G[16]); G[3] = G[5] << 5; G[2] = G[5] << 7; G[2] -= G[3]; G[2] -= G[5]; G[2] <<= 3; G[5] <<= 3; G[17] = G[4]; G[5] += G[17]; G[3] = 0u; G[20] = 0xFFFFFFFFu; G[4] = word(rt, G[4] + 0xC0u); put(rt, G[5] + 0x5Cu, G[3]); G[18] = G[4] + G[2]; G[2] = 0u; G[4] = G[18]; put(rt, G[5] + 0x58u, G[2]); call_here(rt, ctx, 0x088E7F14u); G[19] = G[2]; G[4] = G[17]; if (G[2] != G[20]) { call_here(rt, ctx, 0x088D9F80u); G[4] = G[17]; G[16] = G[2]; call_here(rt, ctx, 0x088DAA8Cu); G[16] = static_cast<std::int32_t>(G[2]) < static_cast<std::int32_t>(G[16]) ? 1u : 0u; G[4] = G[18]; G[5] = 0xFFFFFFFFu; if (G[16] != 0u) { G[2] = word(rt, G[17] + 0x130u); G[2] = G[19] + G[2]; store8(G[2] + 0x50u, G[20]); } tail = 0x088E7EBCu; } G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail != 0u) jump(ctx, tail); }
void func_08975A54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x54u), G[31]); put(rt, frame.at(0x50u), G[20]); put(rt, frame.at(0x4Cu), G[19]); put(rt, frame.at(0x48u), G[18]); put(rt, frame.at(0x44u), G[17]); put(rt, frame.at(0x40u), G[16]); G[19] = frame.at(4u); G[18] = G[4]; G[20] = G[18] + 0xA8u; G[17] = 0x08CCDE00u; call_here(rt, ctx, 0x089D8E2Cu); G[2] = 0x08CCCF88u; put(rt, G[18], G[2]); G[2] = 0x08CCD174u; put(rt, G[18] + 0x38u, G[2]); G[5] = 0u; G[4] = 0x10u; put(rt, G[18] + 0xA8u, 0u); put(rt, G[20] + 4u, 0u); put(rt, G[20] + 8u, 0u); call_here(rt, ctx, 0x08B35840u); G[16] = G[2]; put(rt, G[18] + 0xA8u, G[2]); G[4] = G[19]; put(rt, G[20] + 4u, G[2]); put(rt, G[20] + 8u, G[2]); call_here(rt, ctx, 0x089D76A8u); G[4] = G[16]; G[6] = G[19]; G[7] = frame.at(0u); G[5] = 0u; put(rt, frame.at(4u), G[17]); put(rt, frame.at(0x24u), 0u); put(rt, frame.at(0x28u), 0u); put(rt, frame.at(0x2Cu), 0u); put(rt, frame.at(0x30u), 0u); call_here(rt, ctx, 0x08BE6674u); G[4] = G[19]; put(rt, frame.at(4u), G[17]); call_here(rt, ctx, 0x089D6684u); G[2] = word(rt, G[18] + 0xA8u); put(rt, G[20] + 4u, G[2]); put(rt, G[18] + 0xB4u, 0u); G[20] = word(rt, frame.at(0x50u)); G[19] = word(rt, frame.at(0x4Cu)); G[18] = word(rt, frame.at(0x48u)); G[17] = word(rt, frame.at(0x44u)); G[16] = word(rt, frame.at(0x40u)); } }
void func_089C0EA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); G[20] = frame.at(0xCu); G[18] = G[4]; G[4] = G[20]; G[19] = frame.at(4u); G[17] = frame.at(8u); call_here(rt, ctx, 0x089BEE4Cu); G[16] = word(rt, frame.at(0xCu)); G[4] = 0x08CA5EB4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[5] = G[16]; G[4] = G[19]; call_here(rt, ctx, 0x089BFE58u); G[4] = G[17]; G[5] = G[19]; call_here(rt, ctx, 0x08A6D2D4u); G[4] = G[18]; if (G[18] != G[17]) { G[5] = word(rt, frame.at(8u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[17]; call_here(rt, ctx, 0x08A5B9ECu); G[3] = word(rt, frame.at(4u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[5] = 0xFFFFFFFFu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[19] = word(rt, frame.at(0x1Cu)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_089C1440(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x1Cu), G[19]); G[20] = G[4] + 0x2B0u; G[18] = G[4]; G[4] = G[20]; G[17] = frame.at(8u); G[16] = G[6]; call_here(rt, ctx, 0x08B344E0u); G[5] = G[16]; G[4] = G[17]; G[6] = frame.at(1u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[17]; G[4] = G[18] + 0x230u; call_here(rt, ctx, 0x08B855E0u); G[3] = word(rt, frame.at(8u)); G[2] = 0x08D74FD0u; G[16] = G[3] - 0xCu; G[19] = frame.at(4u); G[4] = G[3] - 4u; G[5] = 0xFFFFFFFFu; G[17] = G[18] + 0x264u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[19]; G[5] = G[18] + 0x22Cu; call_here(rt, ctx, 0x089C5680u); G[4] = G[17]; if (G[17] != G[19]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[19]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08B3457Cu); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0x1Cu)); } }
void func_089C1F00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); put_float(rt, frame.at(0x10u), ctx.fpr[20]); G[17] = G[4]; G[16] = G[5]; bool ne; ctx.fpr[1] = float_at(rt, G[17] + 0u); ctx.fpr[0] = float_at(rt, G[16] + 0u); ne = !(ctx.fpr[1] == ctx.fpr[0]); ctx.fpr[20] = ctx.fpr[12]; if (!ne) { ctx.fpr[1] = float_at(rt, G[17] + 4u); ctx.fpr[0] = float_at(rt, G[16] + 4u); ne = !(ctx.fpr[1] == ctx.fpr[0]); if (!ne) { ctx.fpr[1] = float_at(rt, G[17] + 8u); ctx.fpr[0] = float_at(rt, G[16] + 8u); ne = !(ctx.fpr[1] == ctx.fpr[0]); if (!ne) { ctx.fpr[1] = float_at(rt, G[17] + 0xCu); ctx.fpr[0] = float_at(rt, G[16] + 0xCu); ne = !(ctx.fpr[1] == ctx.fpr[0]); } } } if (ne) { G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x089C1EACu); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[17] + 4u; G[5] = G[16] + 4u; call_here(rt, ctx, 0x089C1EACu); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[17] + 8u; G[5] = G[16] + 8u; call_here(rt, ctx, 0x089C1EACu); ctx.fpr[12] = ctx.fpr[20]; G[4] = G[17] + 0xCu; G[5] = G[16] + 0xCu; tail = 0x089C1EACu; } G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } ctx.fpr[20] = f20; if (tail != 0u) jump(ctx, tail); }
void func_089CC4C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); G[18] = 0x08D74FD0u; G[17] = G[4]; G[3] = word(rt, G[4] + 0x14u); G[2] = 0x08CCF240u; put(rt, G[17], G[2]); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[18]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(2u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, G[17] + 0x10u); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(1u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[2] = word(rt, G[17] + 0xCu); G[5] = 0xFFFFFFFFu; G[16] = G[2] - 0xCu; G[4] = G[2] - 4u; if (G[18] != G[16]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } G[4] = G[17]; call_here(rt, ctx, 0x08A5BB5Cu); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_221
} // namespace gt::game

#define HOT221(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_221::name, gt::ReturnKind::kind)
HOT221(0x08A06B1Cu, func_08A06B1C, Word);
HOT221(0x08A7051Cu, func_08A7051C, Word);
// HOT221(0x08A06BF8u, func_08A06BF8, Word);  // no comparable synthetic case yet
HOT221(0x08A71294u, func_08A71294, Word);
HOT221(0x08820D1Cu, func_08820D1C, Word);
HOT221(0x0882E548u, func_0882E548, Word);
// HOT221(0x0882E8B0u, func_0882E8B0, Word);  // no comparable synthetic case yet
HOT221(0x08867D0Cu, func_08867D0C, Word);
HOT221(0x088782F8u, func_088782F8, Word);
HOT221(0x0887FA54u, func_0887FA54, Word);
HOT221(0x088A2BF8u, func_088A2BF8, Word);
HOT221(0x088B0418u, func_088B0418, Word);
HOT221(0x088DB3CCu, func_088DB3CC, Word);
HOT221(0x08975A54u, func_08975A54, Word);
// HOT221(0x089C0EA8u, func_089C0EA8, Word);  // no comparable synthetic case yet
// HOT221(0x089C1440u, func_089C1440, Word);  // no comparable synthetic case yet
HOT221(0x089C1F00u, func_089C1F00, Word);
HOT221(0x089CC4C0u, func_089CC4C0, Word);
