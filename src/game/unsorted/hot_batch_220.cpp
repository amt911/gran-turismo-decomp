// Hot batch 220: fifty-four- and fifty-five-instruction functions no route reaches, written by hand from `tools.disasm` and verified by
// synthetic cases (calls that set up an argument or adjust the result, guarded tail calls, fills and copies, copy loops and calls that keep a0 in s0).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_220 {
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
    if (!(value >= -2207483648.0f && value < 2207483648.0f)) return 0x7FFFFFFF;
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

// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order): what the callee
// does not take stays as the caller had it; v0 / v1 / f0 afterwards are the callee's.
std::uint32_t call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t target, std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, target, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

using u32 = std::uint32_t;
using s32 = std::int32_t;

// 0x0882DB2C and twenty copies: for a count a2 > 0, builds an object into the slot at sp + 4 with `build`, asks it for a value with
// `query` (null message `text_object`), releases the slot (0x08A5B9EC), asks the member *a3 (virtual +`method`, null message
// `text_member`) for a value, converts the pair with `convert` and stores the result through `store` into the slot at sp, which is
// copied into a0 when it is not the slot itself (0x08A5BAD8) and released.
void convert_object_and_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 build, u32 text_object, u32 query, u32 text_member,
                               u32 method, u32 convert, u32 store) {
    const u32 target = ctx.gpr[4];
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 outer = frame.at(4u);
    const u32 result = frame.at(0u);
    ctx.gpr[4] = outer;
    if (static_cast<s32>(count) <= 0) return;
    call_here(rt, ctx, build);
    const u32 object = word(rt, outer);
    ctx.gpr[4] = text_object;
    if (object == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    const u32 object_value = call_set(rt, ctx, query, {object});
    call_set(rt, ctx, 0x08A5B9ECu, {outer});
    const u32 member = word(rt, holder);
    ctx.gpr[4] = text_member;
    if (member == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    ctx.gpr[3] = word(rt, member);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + method);
    const u32 member_value = call_set(rt, ctx, ctx.gpr[2], {member});
    call_set(rt, ctx, convert, {object_value, member_value});
    call_set(rt, ctx, store, {result, ctx.gpr[2]});
    ctx.gpr[4] = target;
    if (target != result) call_set(rt, ctx, 0x08A5BAD8u, {target, word(rt, result)});
    call_set(rt, ctx, 0x08A5B9ECu, {result});
}

// 0x0882A6D0 and two copies: for a count a2 >= 2, builds an object into the slot at sp, asks it for a value with `query` (null message
// `text_object`), releases the slot, asks the members *a3 and *(a3 + 4) (virtual +0x3C, null message `text_member`) for a value each
// and calls `finish` with the object's value and both.
void apply_object_and_member_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 build, u32 text_object, u32 query,
                                  u32 text_member, u32 finish) {
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    const u32 slot = frame.at(0u);
    ctx.gpr[6] = static_cast<s32>(count) < 2 ? 1u : 0u;
    ctx.gpr[4] = slot;
    if (static_cast<s32>(count) < 2) return;
    call_here(rt, ctx, build);
    const u32 object = word(rt, slot);
    ctx.gpr[4] = text_object;
    if (object == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    const u32 object_value = call_set(rt, ctx, query, {object});
    ctx.gpr[4] = slot;
    call_set(rt, ctx, 0x08A5B9ECu, {slot});
    const u32 first = word(rt, holder);
    ctx.gpr[4] = text_member;
    if (first == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    ctx.gpr[3] = word(rt, first);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const u32 first_value = call_set(rt, ctx, ctx.gpr[2], {first});
    const u32 second = word(rt, holder + 4u);
    ctx.gpr[4] = text_member;
    if (second == 0u) call_here(rt, ctx, 0x08A5BB7Cu);
    ctx.gpr[3] = word(rt, second);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
    const u32 second_value = call_set(rt, ctx, ctx.gpr[2], {second});
    call_set(rt, ctx, finish, {object_value, first_value, second_value});
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

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_0882DB2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885FC2Cu, 0x08A97A6Cu); }
void func_089D0D4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); G[20] = frame.at(4u); G[19] = G[4]; G[4] = G[20]; G[16] = G[6]; G[18] = 1u; G[17] = G[7]; call_here(rt, ctx, 0x089CD62Cu); if (static_cast<std::int32_t>(G[16]) > 0) { G[16] = word(rt, G[17]); G[4] = 0x08CA697Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; } G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA69B8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x089CFAB8u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_0882A6D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_object_and_member_pair(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x08860AD0u); }
void func_0882B240(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_object_and_member_pair(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x0885E740u); }
void func_0884787C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { apply_object_and_member_pair(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x088D4D04u); }
void func_0882DC08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885E5A0u, 0x08A85B1Cu); }
void func_0882DCE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885E5BCu, 0x08A85B1Cu); }
void func_0882DDC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885E61Cu, 0x08A85B1Cu); }
void func_0882DF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[19] = frame.at(4u); G[20] = G[4]; G[4] = G[19]; G[18] = G[7]; if (static_cast<std::int32_t>(G[6]) > 0) { call_here(rt, ctx, 0x088297A8u); G[16] = word(rt, frame.at(4u)); G[4] = 0x08C9A2F8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; call_here(rt, ctx, 0x088294D0u); G[4] = G[19]; G[17] = G[2]; call_here(rt, ctx, 0x08A5B9ECu); G[16] = word(rt, G[18]); G[4] = 0x08C9A2B4u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[5] = G[2]; G[4] = G[17]; call_here(rt, ctx, 0x0885E65Cu); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[20]; if (G[20] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[19] = word(rt, frame.at(0x1Cu)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_0882E058(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885E570u, 0x08A85B1Cu); }
void func_0882E390(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885E4C4u, 0x08A85B1Cu); }
void func_0882E46C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885E4D0u, 0x08A85B1Cu); }
void func_0882E7D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885F80Cu, 0x08A97A6Cu); }
void func_0882EA74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885E6D0u, 0x08A85B1Cu); }
void func_0882F1B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885E81Cu, 0x08A97A6Cu); }
void func_0882F294(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x3Cu, 0x0885EA28u, 0x08A97A6Cu); }
void func_08830EF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088297A8u, 0x08C9A2F8u, 0x088294D0u, 0x08C9A2B4u, 0x30u, 0x0885FE4Cu, 0x08A97A6Cu); }
void func_08839690(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x08C9BB94u, 0x3Cu, 0x08931990u, 0x08A85B1Cu); }
void func_08839968(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x08838FBCu, 0x08C9BBF8u, 0x08838DC4u, 0x08C9BB94u, 0x3Cu, 0x08931AFCu, 0x08A85B1Cu); }
void func_0884B06C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x3Cu, 0x088D4E24u, 0x08A85B1Cu); }
void func_0884B148(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x3Cu, 0x088D61C4u, 0x08A85B1Cu); }
void func_0884BCCC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x3Cu, 0x088D6218u, 0x08A85B1Cu); }
void func_0884BE84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x3Cu, 0x088D636Cu, 0x08A85B1Cu); }
void func_0884BF60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x08847008u, 0x08C9CBBCu, 0x08846E50u, 0x08C9CB80u, 0x3Cu, 0x088D6294u, 0x08A85B1Cu); }
void func_08855F30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { convert_object_and_member(rt, ctx, 0x088547C4u, 0x08C9E188u, 0x08854678u, 0x08C9E14Cu, 0x3Cu, 0x088A5DB8u, 0x08A97A6Cu); }
void func_089D0E28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); G[20] = frame.at(4u); G[19] = G[4]; G[4] = G[20]; G[16] = G[6]; G[18] = 1u; G[17] = G[7]; call_here(rt, ctx, 0x089CD62Cu); if (static_cast<std::int32_t>(G[16]) > 0) { G[16] = word(rt, G[17]); G[4] = 0x08CA697Cu; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; } G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA69B8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x089CF9D4u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A97A6Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_089E9544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); G[20] = frame.at(4u); G[19] = G[4]; G[4] = G[20]; G[16] = G[6]; G[18] = 1u; G[17] = G[7]; call_here(rt, ctx, 0x089E5E50u); if (static_cast<std::int32_t>(G[16]) > 0) { G[16] = word(rt, G[17]); G[4] = 0x08CA78D8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; } G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA7A10u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x089E80A4u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_089E9620(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); G[20] = frame.at(4u); G[19] = G[4]; G[4] = G[20]; G[16] = G[6]; G[18] = 1u; G[17] = G[7]; call_here(rt, ctx, 0x089E5E50u); if (static_cast<std::int32_t>(G[16]) > 0) { G[16] = word(rt, G[17]); G[4] = 0x08CA78D8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; } G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA7A10u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x089E7F74u); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_089E96FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); G[20] = frame.at(4u); G[19] = G[4]; G[4] = G[20]; G[16] = G[6]; G[18] = 1u; G[17] = G[7]; call_here(rt, ctx, 0x089E5E50u); if (static_cast<std::int32_t>(G[16]) > 0) { G[16] = word(rt, G[17]); G[4] = 0x08CA78D8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; } G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA7A10u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x089E7E4Cu); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } }
void func_089E97D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); G[20] = frame.at(4u); G[19] = G[4]; G[4] = G[20]; G[16] = G[6]; G[18] = 1u; G[17] = G[7]; call_here(rt, ctx, 0x089E5E50u); if (static_cast<std::int32_t>(G[16]) > 0) { G[16] = word(rt, G[17]); G[4] = 0x08CA78D8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[18] = G[2]; } G[16] = word(rt, frame.at(4u)); G[4] = 0x08CA7A10u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[16]; G[5] = G[18]; call_here(rt, ctx, 0x089E7D1Cu); G[5] = G[2]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A85B1Cu); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[4] = G[20]; call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[16] = word(rt, frame.at(0x10u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); } }
// 0x089E2788 (a0 = the object, a1 = the destination, a2 = a count, a3 = a holder whose word +0 is a source; v0 = the last call's): only
// with a positive count, the source (null: 0x08A5BB7C(0x08CA7770)) has its virtual +0xC called with (sp + 8, source), then
// 0x089E1104(sp + 4, destination) makes a value (null: 0x08A5BB7C(0x08CA77A4)) that 0x089E1C08(value, sp + 8, word(sp + 8 - 0xC)) uses;
// 0x08A5B9EC(sp + 4) frees the value and the counted string at sp + 8 is released (empty header 0x08D74FD0: no count; else
// 0x08B60804(data - 4, -1) and, at zero or below, 0x08B8542C(header, sp)).
void func_089E2788(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 destination = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{18u, 0x18u}, {17u, 0x14u}, {31u, 0x1Cu}, {16u, 0x10u}});
    ctx.gpr[18] = frame.at(0x4u);
    ctx.gpr[17] = destination;
    if (static_cast<s32>(count) <= 0) return;
    const u32 source = word(rt, holder);
    ctx.gpr[16] = source;
    if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA7770u});
    call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {frame.at(0x8u), source});
    call_set(rt, ctx, 0x089E1104u, {frame.at(0x4u), destination});
    const u32 made = word(rt, frame.at(0x4u));
    ctx.gpr[16] = made;
    if (made == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CA77A4u});
    const u32 text = word(rt, frame.at(0x8u));
    call_set(rt, ctx, 0x089E1C08u, {made, text, word(rt, text - 0xCu)});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x4u)});
    const u32 data = word(rt, frame.at(0x8u));
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    ctx.gpr[2] = 0x08D74FD0u;
    if (header != 0x08D74FD0u) {
        const u32 remaining = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(remaining) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
}
void func_08A208CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; const float f21 = ctx.fpr[21]; { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[20]); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x14u), G[31]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put_float(rt, frame.at(0x24u), ctx.fpr[21]); put_float(rt, frame.at(0x20u), ctx.fpr[20]); G[20] = G[6] & 0xFFu; G[19] = G[5]; G[18] = G[4]; ctx.fpr[21] = ctx.fpr[13]; ctx.fpr[20] = ctx.fpr[12]; ctx.fpr[12] = 0.0f; G[17] = word(rt, G[4] + 0x18u); G[16] = word(rt, G[4] + 0x14u); G[5] = G[17]; bool go = true; if (G[17] != 0u) { G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x34u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); G[5] = G[17]; G[2] = word(rt, G[3] + 0x38u); G[4] = G[16]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16]); } else { G[2] = 0u; if (G[16] == 0u) go = false; else G[3] = word(rt, G[16]); } if (go) { ctx.fpr[12] = ctx.fpr[20]; ctx.fpr[13] = ctx.fpr[21]; ctx.fpr[14] = float_at(rt, G[18] + 0x1Cu); G[2] = word(rt, G[3] + 0x2Cu); G[4] = G[16]; G[5] = G[19]; G[6] = G[20]; G[7] = 0u; call_here(rt, ctx, G[2]); put(rt, G[18] + 0x18u, G[2]); G[2] = G[2] != 0u ? 1u : 0u; } G[20] = word(rt, frame.at(0x10u)); G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } ctx.fpr[20] = f20; ctx.fpr[21] = f21; }
void func_08A33330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { G[6] = 0x31B5D43Bu; { const std::uint64_t p = static_cast<std::uint64_t>(G[5]) * G[6]; ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(p >> 32); } G[2] = 0x05265C00u; G[3] = 0x76Cu; store16(G[4] + 0x14u, G[3]); G[6] = ctx.hi; G[6] >>= 24; ctx.lo = G[5]; { const std::uint64_t acc = ((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) - static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(G[6])) * static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(acc); ctx.hi = static_cast<std::uint32_t>(acc >> 32); } G[5] = 0x95217CB1u; G[2] = 0x0036EE80u; G[7] = ctx.lo; { const std::uint64_t p = static_cast<std::uint64_t>(G[7]) * G[5]; ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(p >> 32); } G[3] = 0xEA60u; store8(G[4] + 0x17u, G[6]); G[5] = ctx.hi >> 21; ctx.lo = G[7]; { const std::uint64_t acc = ((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) - static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(G[5])) * static_cast<std::int32_t>(G[2])); ctx.lo = static_cast<std::uint32_t>(acc); ctx.hi = static_cast<std::uint32_t>(acc >> 32); } G[2] = 0x45E7B273u; store8(G[4] + 0x18u, G[5]); G[8] = ctx.lo; { const std::uint64_t p = static_cast<std::uint64_t>(G[8]) * G[2]; ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(p >> 32); } store8(G[4] + 0x16u, 0u); G[2] = ctx.hi >> 14; ctx.lo = G[8]; { const std::uint64_t acc = ((static_cast<std::uint64_t>(ctx.hi) << 32) | ctx.lo) - static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(G[2])) * static_cast<std::int32_t>(G[3])); ctx.lo = static_cast<std::uint32_t>(acc); ctx.hi = static_cast<std::uint32_t>(acc >> 32); } G[3] = 0x10624DD3u; store8(G[4] + 0x19u, G[2]); G[6] = ctx.lo; { const std::uint64_t p = static_cast<std::uint64_t>(G[6]) * G[3]; ctx.lo = static_cast<std::uint32_t>(p); ctx.hi = static_cast<std::uint32_t>(p >> 32); } G[3] = ctx.hi >> 6; G[5] = G[3] << 2; G[2] = G[3] << 7; G[2] -= G[5]; G[2] += G[3]; G[2] <<= 3; G[6] -= G[2]; G[5] = G[6] << 2; G[2] = G[6] << 7; G[2] -= G[5]; G[2] += G[6]; G[2] <<= 3; put(rt, G[4] + 0x1Cu, G[2]); store8(G[4] + 0x1Au, G[3]); }
void func_08A52EC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x24u), G[31]); put(rt, frame.at(0x10u), G[16]); G[20] = G[4]; G[19] = 0xFFFFFFFFu; G[18] = frame.at(4u); G[17] = G[5]; if (static_cast<std::int32_t>(G[6]) > 0) { G[16] = word(rt, G[7]); G[4] = 0x08CAF4B8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x3Cu); G[4] = G[16]; call_here(rt, ctx, G[2]); G[19] = G[2]; } G[4] = frame.at(0u); G[5] = G[17]; call_here(rt, ctx, 0x08A52A18u); G[16] = word(rt, frame.at(0u)); G[4] = 0x08CAF4A8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[4] = G[18]; G[5] = G[16]; G[6] = G[19]; G[7] = 0u; G[8] = 0u; call_here(rt, ctx, 0x08A52D3Cu); G[4] = G[20]; if (G[20] != G[18]) { G[5] = word(rt, frame.at(4u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = G[18]; call_here(rt, ctx, 0x08A5B9ECu); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[20] = word(rt, frame.at(0x20u)); G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A58EF0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x1Cu), G[31]); G[18] = G[6]; G[17] = G[4]; G[2] = word(rt, G[5] + 0x1Cu); G[16] = G[2]; if (G[2] == 0u) { G[16] = word(rt, G[5] + 0x18u); if (G[16] == 0u) { G[4] = 0x08CAF864u; call_here(rt, ctx, 0x08A5BB7Cu); } G[16] = word(rt, G[16] + 0xCu); } G[2] = word(rt, G[18]); if (G[2] != 0u) { G[2] = word(rt, G[16]); if (G[2] == 0u) { G[4] = word(rt, G[17] + 0x38u); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 8u); call_here(rt, ctx, G[2]); put(rt, G[16], G[2]); } G[5] = word(rt, G[17] + 0x38u); G[6] = G[2]; G[3] = word(rt, G[5]); G[2] = word(rt, G[3] + 0x10u); G[4] = frame.at(0u); call_here(rt, ctx, G[2]); G[16] = word(rt, frame.at(0u)); if (G[16] == 0u) { G[4] = 0x08CAF878u; call_here(rt, ctx, 0x08A5BB7Cu); } G[3] = word(rt, G[16]); G[4] = G[16]; G[2] = word(rt, G[3] + 0xF4u); G[5] = G[18]; call_here(rt, ctx, G[2]); G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); } G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A5EEA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[19] = G[5]; G[18] = word(rt, G[7]); G[20] = word(rt, G[4] + 0x18u); G[21] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CB0080u; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[18]; call_here(rt, ctx, G[2]); G[4] = ~G[2]; G[6] = G[20] >> 1; G[6] >>= (G[4] & 31u); G[17] = G[21] << (G[2] & 31u); G[4] = G[2] & 0x20u; G[5] = 0u; G[17] |= G[6]; G[7] = 0u; G[6] = 0u; G[16] = G[20] << (G[2] & 31u); call_here(rt, ctx, 0x08B6EE4Cu); G[2] ^= 1u; if (G[2] != 0u) { G[17] = G[16]; G[16] = 0u; } G[4] = frame.at(0u); G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08A5EF7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); G[19] = G[5]; G[18] = word(rt, G[7]); G[20] = word(rt, G[4] + 0x18u); G[21] = word(rt, G[4] + 0x1Cu); G[4] = 0x08CB0080u; if (G[18] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[18]); G[2] = word(rt, G[3] + 0x50u); G[4] = G[18]; call_here(rt, ctx, G[2]); G[4] = ~G[2]; G[6] = G[21] << 1; G[6] <<= (G[4] & 31u); G[16] = G[20] >> (G[2] & 31u); G[4] = G[2] & 0x20u; G[5] = 0u; G[16] |= G[6]; G[7] = 0u; G[6] = 0u; G[17] = G[21] >> (G[2] & 31u); call_here(rt, ctx, 0x08B6EE4Cu); G[2] ^= 1u; if (G[2] != 0u) { G[16] = G[17]; G[17] = 0u; } G[4] = frame.at(0u); G[6] = G[16]; G[7] = G[17]; call_here(rt, ctx, 0x08A5DEE8u); G[4] = G[19]; if (G[19] != frame.at(0u)) { G[5] = word(rt, frame.at(0u)); call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[19] = word(rt, frame.at(0x1Cu)); G[21] = word(rt, frame.at(0x24u)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08AA3850(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x40u); put(rt, frame.at(0x24u), G[21]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x28u), G[31]); put(rt, frame.at(0x20u), G[20]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x10u), G[16]); put_float(rt, frame.at(0x30u), ctx.fpr[20]); G[21] = G[4]; G[19] = G[5]; G[1] = 0x7F7FFFFFu; ctx.fpr[20] = std::bit_cast<float>(G[1]); if (static_cast<std::int32_t>(G[5]) > 0) { G[17] = G[6]; G[18] = 0u; G[20] = 0x08CB0000u; G[16] = word(rt, G[17]); for (;;) { G[18] += 1u; G[17] += 4u; G[4] = 0x08CB2BB8u; if (G[16] == 0u) call_here(rt, ctx, 0x08A5BB7Cu); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x54u); G[4] = G[16]; call_here(rt, ctx, G[2]); if (!(ctx.fpr[20] < ctx.fpr[0])) ctx.fpr[20] = ctx.fpr[0]; if (G[19] == G[18]) break; G[16] = word(rt, G[17]); } } ctx.fpr[12] = ctx.fpr[20]; G[4] = frame.at(0u); call_here(rt, ctx, 0x08A87EECu); G[5] = word(rt, frame.at(0u)); if (G[21] != frame.at(0u)) { G[4] = G[21]; call_here(rt, ctx, 0x08A5BAD8u); } G[4] = frame.at(0u); call_here(rt, ctx, 0x08A5B9ECu); G[21] = word(rt, frame.at(0x24u)); G[19] = word(rt, frame.at(0x1Cu)); G[20] = word(rt, frame.at(0x20u)); G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } ctx.fpr[20] = f20; }
// 0x08AA48E0 (a0 = the object, a1 = a mode word, a2 = a holder whose word +0 is a source; v0 = 1 or the last call's): only mode 1 does
// anything: the source (null: 0x08A5BB7C(0x08CB2CD4)) has its virtual +0xC called with (sp + 4, source), 0x08AA43CC(sp + 4) makes a value
// that 0x08A97A6C(sp + 8, value) stores at sp + 8; when the object and sp + 8 differ 0x08A5BAD8(object, sp + 8) moves it,
// 0x08A5B9EC(sp + 8) frees it and the counted string at sp + 4 is released (empty header 0x08D74FD0: no count; else
// 0x08B60804(data - 4, -1) and, at zero or below, 0x08B8542C(header, sp)).
void func_08AA48E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 mode = ctx.gpr[5];
    const u32 holder = ctx.gpr[6];
    GuestFrame frame(ctx, 0x30u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0x1Cu}, {18u, 0x18u}, {17u, 0x14u}, {31u, 0x20u}, {16u, 0x10u}});
    ctx.gpr[2] = 1u;
    ctx.gpr[19] = frame.at(0x4u);
    ctx.gpr[18] = frame.at(0x8u);
    ctx.gpr[17] = self;
    if (mode != 1u) return;
    const u32 source = word(rt, holder);
    ctx.gpr[16] = source;
    if (source == 0u) call_set(rt, ctx, 0x08A5BB7Cu, {0x08CB2CD4u});
    call_indirect(rt, ctx, word(rt, word(rt, source) + 0xCu), {frame.at(0x4u), source});
    const u32 value = call_set(rt, ctx, 0x08AA43CCu, {frame.at(0x4u)});
    call_set(rt, ctx, 0x08A97A6Cu, {frame.at(0x8u), value});
    if (self != frame.at(0x8u)) call_set(rt, ctx, 0x08A5BAD8u, {self, word(rt, frame.at(0x8u))});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0x8u)});
    const u32 data = word(rt, frame.at(0x4u));
    const u32 header = data - 0xCu;
    ctx.gpr[16] = header;
    ctx.gpr[2] = 0x08D74FD0u;
    if (header != 0x08D74FD0u) {
        const u32 remaining = call_set(rt, ctx, 0x08B60804u, {data - 0x4u, 0xFFFFFFFFu});
        if (static_cast<s32>(remaining) <= 0) call_set(rt, ctx, 0x08B8542Cu, {header, frame.at(0u)});
    }
}
void func_08AB4090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x14u), G[17]); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x10u), G[16]); G[18] = 0x08D74FD0u; G[2] = G[18] + 0xCu; G[3] = G[5] + 0x14u; G[17] = G[4]; put(rt, G[4], G[2]); G[2] = word(rt, G[3] + 4u); G[2] = static_cast<std::int32_t>(G[6]) < static_cast<std::int32_t>(G[2]) ? 1u : 0u; G[8] = G[5]; if (G[2] != 0u) { G[3] = word(rt, G[3] + 8u); G[5] = word(rt, G[5] + 0x24u); G[2] = G[6] << 3; G[3] += G[2]; G[4] = word(rt, G[5] - 0xCu); G[6] = word(rt, G[3]); G[2] = word(rt, G[3] + 4u); G[5] = G[8] + 0x24u; G[4] = G[4] < G[6] ? 1u : 0u; G[7] = G[2] - G[6]; if (G[4] != 0u) { G[4] = 0x08CB3D90u; call_here(rt, ctx, 0x08B5AEDCu); } G[16] = frame.at(4u); G[4] = G[16]; call_here(rt, ctx, 0x08B8D96Cu); G[4] = G[17]; G[5] = G[16]; call_here(rt, ctx, 0x08B855E0u); G[4] = word(rt, frame.at(4u)); G[16] = G[4] - 0xCu; if (G[16] != G[18]) { G[4] -= 4u; G[5] = 0xFFFFFFFFu; call_here(rt, ctx, 0x08B60804u); if (static_cast<std::int32_t>(G[2]) <= 0) { G[4] = G[16]; G[5] = frame.at(0u); call_here(rt, ctx, 0x08B8542Cu); } } } G[2] = G[17]; G[18] = word(rt, frame.at(0x18u)); G[17] = word(rt, frame.at(0x14u)); G[16] = word(rt, frame.at(0x10u)); } }
void func_08ACC504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); put(rt, frame.at(0x8u), G[31]); G[17] = G[4] + 0x60u; G[16] = G[4]; G[4] = G[17]; call_here(rt, ctx, 0x08B34888u); call_here(rt, ctx, 0x08C7B1C8u); G[3] = G[2]; G[2] = load8(G[16] + 0x8Cu); do { if (G[2] != 0u) { G[2] = G[3] - 1u; G[2] = G[2] < 4u ? 1u : 0u; if (G[2] == 0u) { store8(G[16] + 0x8Cu, 0u); G[4] = G[16] + 0x98u; call_here(rt, ctx, 0x08C7B170u); put(rt, G[16] + 0x90u, G[2]); if (G[2] == 0u) { G[2] = 1u; store8(G[16] + 0x8Du, G[2]); } G[4] = G[17]; break; } } G[2] = 2u; if (G[3] == G[2]) { G[4] = 1u; call_here(rt, ctx, 0x08C7B1B8u); G[4] = G[17]; break; } G[2] = 3u; if (G[3] == G[2]) { call_here(rt, ctx, 0x08C7B1A0u); G[4] = G[17]; break; } G[4] = G[17]; if (G[3] == 0u) { G[2] = load8(G[16] + 0x8Du); if (G[2] != 0u) { G[2] = word(rt, G[16] + 0x2D8u); store8(G[16] + 0x8Du, 0u); G[4] = G[17]; put(rt, G[16] + 0x94u, G[2]); call_here(rt, ctx, 0x08B34CA0u); G[4] = G[17]; } } } while (false); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B34B24u); }
void func_08ACE4F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x10u); put(rt, frame.at(0xCu), G[31]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x0u), G[16]); G[2] = 0x08CD6B28u; G[16] = G[4]; G[17] = G[16] + 0x420u; put(rt, G[4], G[2]); call_here(rt, ctx, 0x08ACDD14u); G[18] = G[16] + 0x334u; call_here(rt, ctx, 0x08C7AD80u); G[4] = G[16] + 0x2Cu; call_here(rt, ctx, 0x08B2EE2Cu); G[4] = G[16] + 0x18u; call_here(rt, ctx, 0x08B2EE2Cu); G[4] = G[16] + 4u; call_here(rt, ctx, 0x08B2EE2Cu); G[2] = G[16] + 0x510u; if (G[17] != 0u && G[17] != G[2]) { G[4] = G[16] + 0x4A0u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16] + 0x474u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x448u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[17]; call_here(rt, ctx, 0x08B28DC4u); } if (G[18] != 0u && G[18] != G[17]) { G[4] = G[16] + 0x3B8u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16] + 0x38Cu; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[16] + 0x360u; call_here(rt, ctx, 0x08B28BE0u); G[4] = G[18]; call_here(rt, ctx, 0x08B28BE0u); } G[4] = G[16] + 0x164u; call_here(rt, ctx, 0x08B28DC4u); G[4] = G[16] + 0x40u; call_here(rt, ctx, 0x08B0C808u); G[4] = G[16]; G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } jump(ctx, 0x08B64654u); }
void func_08AD230C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; { GuestFrame frame(ctx, 0x60u); put(rt, frame.at(0x40u), G[16]); put(rt, frame.at(0x48u), G[31]); put(rt, frame.at(0x44u), G[17]); put_float(rt, frame.at(0x50u), ctx.fpr[20]); G[1] = 0x40A00000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); G[16] = G[4]; G[1] = 0x3F800000u; ctx.fpr[20] = std::bit_cast<float>(G[1]); G[17] = G[16] + 0x20u; put(rt, G[4] + 0x78u, G[5]); G[4] = frame.at(0x24u); put_float(rt, G[16] + 0x70u, ctx.fpr[0]); put_float(rt, G[16] + 0x60u, ctx.fpr[20]); put_float(rt, G[16] + 0x64u, ctx.fpr[20]); put_float(rt, G[16] + 0x74u, ctx.fpr[20]); store8(G[16] + 0x5Au, 0u); store8(G[16] + 0x5Bu, 0u); store8(G[16] + 0x5Cu, 0u); put(rt, G[16] + 0x68u, 0u); put(rt, G[16] + 0x6Cu, 0u); call_here(rt, ctx, 0x08AD5EBCu); ctx.fpr[1] = 0.0f; G[4] = word(rt, frame.at(0x24u)); G[5] = word(rt, frame.at(0x28u)); G[2] = G[16] + 0x2Cu; G[3] = G[16] + 0x38u; G[1] = 0x4C088344u; ctx.fpr[0] = std::bit_cast<float>(G[1]); put_float(rt, G[16] + 0x20u, ctx.fpr[1]); put_float(rt, G[17] + 4u, ctx.fpr[0]); put_float(rt, G[17] + 8u, ctx.fpr[1]); put_float(rt, G[16] + 0x2Cu, ctx.fpr[1]); put_float(rt, G[2] + 8u, ctx.fpr[1]); put_float(rt, G[2] + 4u, ctx.fpr[1]); put_float(rt, G[16] + 0x38u, ctx.fpr[1]); put_float(rt, G[3] + 8u, ctx.fpr[1]); put_float(rt, G[3] + 4u, ctx.fpr[1]); put_float(rt, G[17] + 0x34u, ctx.fpr[1]); put(rt, G[17] + 0x24u, G[4]); put(rt, G[17] + 0x28u, G[5]); put_float(rt, G[17] + 0x30u, ctx.fpr[20]); put_float(rt, G[17] + 0x2Cu, ctx.fpr[1]); put(rt, G[16] + 0x14u, 0u); store8(G[16] + 0x58u, 0u); store8(G[16] + 0x59u, 0u); G[16] = word(rt, frame.at(0x40u)); G[17] = word(rt, frame.at(0x44u)); } ctx.fpr[20] = f20; }
void func_08AD77F0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { const float f20 = ctx.fpr[20]; const float f21 = ctx.fpr[21]; const float f22 = ctx.fpr[22]; const float f23 = ctx.fpr[23]; { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[31]); put_float(rt, frame.at(0x2Cu), ctx.fpr[23]); put_float(rt, frame.at(0x24u), ctx.fpr[21]); put_float(rt, frame.at(0x28u), ctx.fpr[22]); put_float(rt, frame.at(0x20u), ctx.fpr[20]); ctx.fpr[23] = ctx.fpr[12]; ctx.fpr[12] = ctx.fpr[13]; ctx.fpr[21] = ctx.fpr[14]; G[5] = frame.at(4u); G[16] = G[4]; G[4] = frame.at(0u); ctx.fpr[22] = ctx.fpr[13]; call_here(rt, ctx, 0x08B498ACu); G[1] = 0x3EB17218u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[21] = psp_mul(ctx.fpr[21], ctx.fpr[0]); ctx.fpr[20] = float_at(rt, frame.at(0u)); ctx.fpr[22] = psp_mul(ctx.fpr[22], ctx.fpr[21]); ctx.fpr[12] = ctx.fpr[22] / ctx.fpr[20]; call_here(rt, ctx, 0x08B67C70u); ctx.fpr[20] = psp_mul(ctx.fpr[20], ctx.fpr[0]); G[1] = 0x3F800000u; ctx.fpr[0] = std::bit_cast<float>(G[1]); ctx.fpr[3] = float_at(rt, frame.at(4u)); G[1] = 0xC0000000u; ctx.fpr[1] = std::bit_cast<float>(G[1]); ctx.fpr[4] = ctx.fpr[20] + ctx.fpr[0]; ctx.fpr[2] = ctx.fpr[0] - ctx.fpr[20]; ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[1]); ctx.fpr[1] = -ctx.fpr[20]; ctx.fpr[0] = ctx.fpr[0] / ctx.fpr[4]; ctx.fpr[23] = psp_mul(ctx.fpr[23], ctx.fpr[0]); ctx.fpr[2] = psp_mul(ctx.fpr[2], ctx.fpr[0]); ctx.fpr[3] = psp_mul(ctx.fpr[3], ctx.fpr[0]); ctx.fpr[0] = 0.0f; ctx.fpr[20] = psp_mul(ctx.fpr[20], ctx.fpr[23]); ctx.fpr[1] = psp_mul(ctx.fpr[1], ctx.fpr[23]); ctx.fpr[0] = psp_mul(ctx.fpr[23], ctx.fpr[0]); put_float(rt, G[16] + 0x10u, ctx.fpr[2]); put_float(rt, G[16], ctx.fpr[20]); put_float(rt, G[16] + 4u, ctx.fpr[0]); put_float(rt, G[16] + 8u, ctx.fpr[1]); put_float(rt, G[16] + 0xCu, ctx.fpr[3]); G[16] = word(rt, frame.at(0x10u)); } ctx.fpr[20] = f20; ctx.fpr[21] = f21; ctx.fpr[22] = f22; ctx.fpr[23] = f23; }
void func_08AF4CEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { GuestFrame frame(ctx, 0x70u); put(rt, frame.at(0x6Cu), G[31]); put(rt, frame.at(0x68u), G[18]); put(rt, frame.at(0x64u), G[17]); put(rt, frame.at(0x60u), G[16]); G[18] = G[4]; G[17] = frame.at(0x20u); G[4] = G[17]; G[3] = word(rt, G[5]); G[2] = word(rt, G[3] + 0xCu); G[16] = G[5]; call_here(rt, ctx, G[2]); G[4] = G[17]; call_here(rt, ctx, 0x08B507B8u); G[3] = word(rt, G[16]); G[2] = word(rt, G[3] + 0x10u); G[4] = G[16]; call_here(rt, ctx, G[2]); ctx.fpr[12] = ctx.fpr[0]; G[4] = frame.at(0u); ctx.fpr[1] = float_at(rt, frame.at(0x44u)); ctx.fpr[0] = float_at(rt, frame.at(0x40u)); put_float(rt, frame.at(0x4u), ctx.fpr[1]); put_float(rt, frame.at(0u), ctx.fpr[0]); ctx.fpr[1] = float_at(rt, frame.at(0x4Cu)); ctx.fpr[0] = float_at(rt, frame.at(0x48u)); put_float(rt, frame.at(0xCu), ctx.fpr[1]); put_float(rt, frame.at(0x8u), ctx.fpr[0]); ctx.fpr[1] = float_at(rt, frame.at(0x54u)); ctx.fpr[0] = float_at(rt, frame.at(0x50u)); put_float(rt, frame.at(0x14u), ctx.fpr[1]); put_float(rt, frame.at(0x10u), ctx.fpr[0]); ctx.fpr[1] = float_at(rt, frame.at(0x5Cu)); ctx.fpr[0] = float_at(rt, frame.at(0x58u)); put_float(rt, frame.at(0x1Cu), ctx.fpr[1]); put_float(rt, frame.at(0x18u), ctx.fpr[0]); call_here(rt, ctx, 0x08B4A6A0u); G[4] = frame.at(0x10u); G[6] = frame.at(0u); G[5] = G[4]; call_here(rt, ctx, 0x08B4A8D4u); ctx.fpr[2] = float_at(rt, frame.at(0x18u)); ctx.fpr[1] = float_at(rt, frame.at(0x10u)); ctx.fpr[0] = float_at(rt, frame.at(0x14u)); put_float(rt, G[18] + 8u, ctx.fpr[2]); G[2] = G[18]; put_float(rt, G[18] + 4u, ctx.fpr[0]); put_float(rt, G[18], ctx.fpr[1]); G[18] = word(rt, frame.at(0x68u)); G[17] = word(rt, frame.at(0x64u)); G[16] = word(rt, frame.at(0x60u)); } }
// 0x08B09C78 (a0 = the object, a1 = a second key; v0 = 1 when the change was accepted, 0 otherwise): with 0x08B344E0(0x08D473C0) holding a
// lock, the object's word +0xD4 block's byte +0x29 set asks 0x08B09530 twice (the block and the key): when both answer the same, the
// block's byte +0x28 is cleared, 0x08B3457C(0x08D473C0) releases the lock and v0 = 1. Otherwise, with +0x29 still set the lock is only
// released (v0 = 0); with it clear 0x08B09298(block, key) runs, +0x29 takes 1, +0x28 is cleared, the lock is released and v0 = 1.
void func_08B09C78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(rt, ctx, frame, {{19u, 0xCu}, {16u, 0x0u}, {20u, 0x10u}, {18u, 0x8u}, {31u, 0x14u}, {17u, 0x4u}});
    ctx.gpr[19] = 0x70000u;
    ctx.gpr[16] = self;
    ctx.gpr[20] = argument;
    call_set(rt, ctx, 0x08B344E0u, {0x08D473C0u});
    const u32 field = word(rt, self) + 0xD4u;
    ctx.gpr[18] = field;
    u32 result = 0u;
    bool handled = false;
    if (load8(field + 0x29u) != 0u) {
        const u32 first = call_set(rt, ctx, 0x08B09530u, {field});
        ctx.gpr[16] = first;
        if (call_set(rt, ctx, 0x08B09530u, {argument}) == first) {
            store8(field + 0x28u, 0u);
            call_set(rt, ctx, 0x08B3457Cu, {0x08D473C0u});
            result = 1u;
            handled = true;
        }
    }
    if (!handled) {
        if (load8(field + 0x29u) != 0u) {
            call_set(rt, ctx, 0x08B3457Cu, {0x08D473C0u});
        } else {
            call_set(rt, ctx, 0x08B09298u, {field, argument});
            store8(field + 0x29u, 1u);
            store8(field + 0x28u, 0u);
            call_set(rt, ctx, 0x08B3457Cu, {0x08D473C0u});
            result = 1u;
        }
    }
    ctx.gpr[2] = result;
}
void func_08B0E784(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { std::uint32_t tail = 0u; { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0xCu), G[19]); put(rt, frame.at(0x8u), G[18]); put(rt, frame.at(0x4u), G[17]); put(rt, frame.at(0x10u), G[31]); put(rt, frame.at(0x0u), G[16]); G[19] = G[4] + 4u; G[18] = G[4]; G[4] = G[19]; G[17] = G[5] & 0xFFu; call_here(rt, ctx, 0x08B34888u); G[2] = word(rt, G[18] + 0xFCu); if (G[2] != 0u) { G[16] = G[2]; do { G[4] = word(rt, G[16]); G[3] = word(rt, G[4]); G[2] = word(rt, G[3] + 0xCu); G[5] = G[17]; call_here(rt, ctx, G[2]); G[3] = word(rt, G[16] + 8u); G[16] = G[3]; } while (G[3] != 0u); G[17] = 0u; } else if (G[17] != 0u) { G[3] = word(rt, G[18]); G[17] = load8(G[18] + 0x68u); store8(G[18] + 0x68u, 0u); G[2] = word(rt, G[3] + 0x24u); G[4] = G[18]; call_here(rt, ctx, G[2]); } G[4] = G[19]; call_here(rt, ctx, 0x08B34B24u); if (G[17] != 0u) { G[4] = G[18]; tail = 0x08B0DEB0u; } G[19] = word(rt, frame.at(0xCu)); G[18] = word(rt, frame.at(0x8u)); G[17] = word(rt, frame.at(0x4u)); G[16] = word(rt, frame.at(0x0u)); } if (tail != 0u) jump(ctx, tail); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_220
} // namespace gt::game

#define HOT220(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_220::name, gt::ReturnKind::kind)
HOT220(0x0882DB2Cu, func_0882DB2C, Word);
// HOT220(0x089D0D4Cu, func_089D0D4C, Word);  // no comparable synthetic case yet
HOT220(0x0882A6D0u, func_0882A6D0, Word);
HOT220(0x0882B240u, func_0882B240, Word);
HOT220(0x0884787Cu, func_0884787C, Word);
HOT220(0x0882DC08u, func_0882DC08, Word);
HOT220(0x0882DCE4u, func_0882DCE4, Word);
HOT220(0x0882DDC0u, func_0882DDC0, Word);
HOT220(0x0882DF7Cu, func_0882DF7C, Word);
HOT220(0x0882E058u, func_0882E058, Word);
HOT220(0x0882E390u, func_0882E390, Word);
HOT220(0x0882E46Cu, func_0882E46C, Word);
HOT220(0x0882E7D4u, func_0882E7D4, Word);
HOT220(0x0882EA74u, func_0882EA74, Word);
HOT220(0x0882F1B8u, func_0882F1B8, Word);
HOT220(0x0882F294u, func_0882F294, Word);
HOT220(0x08830EF0u, func_08830EF0, Word);
HOT220(0x08839690u, func_08839690, Word);
HOT220(0x08839968u, func_08839968, Word);
HOT220(0x0884B06Cu, func_0884B06C, Word);
HOT220(0x0884B148u, func_0884B148, Word);
HOT220(0x0884BCCCu, func_0884BCCC, Word);
HOT220(0x0884BE84u, func_0884BE84, Word);
HOT220(0x0884BF60u, func_0884BF60, Word);
HOT220(0x08855F30u, func_08855F30, Word);
// HOT220(0x089D0E28u, func_089D0E28, Word);  // no comparable synthetic case yet
// HOT220(0x089E9544u, func_089E9544, Word);  // no comparable synthetic case yet
// HOT220(0x089E9620u, func_089E9620, Word);  // no comparable synthetic case yet
// HOT220(0x089E96FCu, func_089E96FC, Word);  // no comparable synthetic case yet
// HOT220(0x089E97D8u, func_089E97D8, Word);  // no comparable synthetic case yet
HOT220(0x089E2788u, func_089E2788, Word);
HOT220(0x08A208CCu, func_08A208CC, Word);
HOT220(0x08A33330u, func_08A33330, Word);
// HOT220(0x08A52EC4u, func_08A52EC4, Word);  // no comparable synthetic case yet
// HOT220(0x08A58EF0u, func_08A58EF0, Word);  // no comparable synthetic case yet
// HOT220(0x08A5EEA4u, func_08A5EEA4, Word);  // no comparable synthetic case yet
HOT220(0x08A5EF7Cu, func_08A5EF7C, Word);
HOT220(0x08AA3850u, func_08AA3850, Word);
HOT220(0x08AA48E0u, func_08AA48E0, Word);
HOT220(0x08AB4090u, func_08AB4090, Word);
// HOT220(0x08ACC504u, func_08ACC504, Word);  // no comparable synthetic case yet
// HOT220(0x08ACE4F4u, func_08ACE4F4, Word);  // no comparable synthetic case yet
HOT220(0x08AD230Cu, func_08AD230C, Word);
HOT220(0x08AD77F0u, func_08AD77F0, Word);
HOT220(0x08AF4CECu, func_08AF4CEC, Word);
HOT220(0x08B09C78u, func_08B09C78, Word);
// HOT220(0x08B0E784u, func_08B0E784, Word);  // no comparable synthetic case yet
