// Hot batch 313: script-binding registrations no route reaches, written by hand from `tools.disasm` and verified
// by synthetic cases: each one binds a script class's methods and properties, then registers its constants.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <utility>

namespace gt::game {
namespace hot_batch_313 {
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

// The `std::string` representation every empty string shares.
constexpr u32 kEmptyStreamText = 0x08D74FD0u;

// Drops one reference of the string whose characters start at `data` (representation at data - 0xC, count at data - 4): the shared empty
// representation is left alone, otherwise 0x08B60804 decrements the count and 0x08B8542C frees the representation (a1 = the stack word at sp)
// at zero or below. v0 / a0 / a1 are left as the original leaves them.
void drop_string_reference(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, GuestFrame &frame, u32 data) {
    const u32 representation = data - 0xCu;
    ctx.gpr[3] = data;
    ctx.gpr[2] = kEmptyStreamText;
    ctx.gpr[4] = data - 4u;
    ctx.gpr[5] = 0xFFFFFFFFu;
    if (representation == kEmptyStreamText) return;
    const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {data - 4u, 0xFFFFFFFFu}));
    ctx.gpr[4] = representation;
    ctx.gpr[5] = frame.at(0u);
    if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {representation, frame.at(0u)});
}

// Constructor-time registration of a script object (a0 = the object) in a frame of `frame_bytes`: the constructor builds the string `text` into the
// slot at sp + 4 (0x08B85EB8), has the object's virtual +0x108 take it, drops it, asks `handle_source` for a handle (released 0x08A5BA08 when set) and
// replaces the handle kept at +0x38 (the old one released by 0x08A5B930); `call` then runs one registration, `target(object, ...)`, after the other.
// `saves` are the stores of the original's prologue.
// The destructor gives the frame and the callee-saved registers back; v0 is the last call's.
class ScriptRegistration {
public:
    ScriptRegistration(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 frame_bytes, u32 text, u32 handle_source,
                       std::initializer_list<SavedSlot> saves)
        : rt_(rt), ctx_(ctx), self_(ctx.gpr[4]), frame_(ctx, frame_bytes), saved_(rt, ctx, frame_, saves) {
        const u32 slot = frame_.at(4u);
        ctx_.gpr[18] = self_;
        ctx_.gpr[16] = slot;
        ctx_.gpr[2] = word(rt_, self_);
        ctx_.gpr[17] = word(rt_, ctx_.gpr[2] + 0x108u);
        call_set(rt_, ctx_, 0x08B85EB8u, {slot, text, frame_.at(1u)});
        call_set(rt_, ctx_, ctx_.gpr[17], {self_, slot});
        drop_string_reference(rt_, ctx_, frame_, word(rt_, slot));
        const u32 handle = call_set(rt_, ctx_, handle_source, {});
        ctx_.gpr[16] = handle;
        if (handle != 0u) call_set(rt_, ctx_, 0x08A5BA08u, {handle});
        const u32 previous = word(rt_, self_ + 0x38u);
        ctx_.gpr[2] = previous;
        ctx_.gpr[4] = previous;
        if (previous != 0u) call_set(rt_, ctx_, 0x08A5B930u, {previous});
        put(rt_, self_ + 0x38u, handle);
    }

    u32 object() const { return self_; }
    u32 slot(u32 offset) { return frame_.at(offset); }

    // One call `target(object, arguments...)`; the registers a0-a3 and t0-t3 hold the arguments afterwards as the callee left a copy of them.
    u32 call(u32 target, std::initializer_list<u32> arguments) {
        unsigned reg = 4u;
        ctx_.gpr[reg++] = self_;
        for (const u32 argument : arguments) {
            if (reg <= 11u) ctx_.gpr[reg++] = argument;
        }
        const GuestResult result = call_guest(rt_, ctx_, target, {}, {});
        ctx_.gpr[2] = result.v0;
        ctx_.gpr[3] = result.v1;
        ctx_.fpr[0] = result.f0;
        return result.v0;
    }

private:
    psprecomp::Runtime &rt_;
    psprecomp::AllegrexContext &ctx_;
    u32 self_;
    GuestFrame frame_;
    SavedRegisters saved_;
};

// 0x089EF278: constructor-time registration of a script object (a0 = the object) with its text 0x08CA7EB4: 19 registration steps.
void register_089EF278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x20u, 0x08CA7EB4u, 0x089DF9E4u, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08C0B370u});
    registration.call(0x08B878E8u, {0x08CA7F3Cu, 0x089EF70Cu, 0x089EF70Cu});
    registration.call(0x08B878E8u, {0x08CA7E2Cu, 0x089EE874u, 0x089EE874u});
    registration.call(0x08B878E8u, {0x08CA7DC8u, 0x089EED04u, 0x089EED04u});
    registration.call(0x08BEEBB4u, {0x08CA7F44u, 0u, 0x089EE1E0u});
    registration.call(0x08B87108u, {0x08CA7F4Cu, 0x089EEA4Cu});
    registration.call(0x08B878E8u, {0x08CA7E68u, 0x089EFF1Cu, 0x089EFF1Cu});
    registration.call(0x08B878E8u, {0x08CA7E78u, 0x089F00BCu, 0x089F00BCu});
    registration.call(0x08B878E8u, {0x08CA7F5Cu, 0x089EF570u, 0x089EF570u});
    registration.call(0x08B878E8u, {0x08CA7E38u, 0x08C0B1D0u, 0x08C0ADECu});
    registration.call(0x08B878E8u, {0x08CA7E44u, 0x08C0B19Cu, 0x08C0ADB8u});
    registration.call(0x08B878E8u, {0x08CA7E50u, 0x08C0B168u, 0x08C0AD84u});
    registration.call(0x08B878E8u, {0x08CA7E5Cu, 0x08C0B134u, 0x08C0AD50u});
    registration.call(0x08B878E8u, {0x08CA7F68u, 0x08C0B02Cu, 0x08C0AC18u});
    registration.call(0x08B878E8u, {0x08CA7F70u, 0x08C0AFF8u, 0x08C0ABE4u});
    registration.call(0x08B878E8u, {0x08CA7F7Cu, 0x08C0AFC4u, 0x08C0ABB0u});
    registration.call(0x08B878E8u, {0x08CA7F8Cu, 0x08C0B100u, 0x08C0AD1Cu});
    registration.call(0x08B878E8u, {0x08CA7F98u, 0x089EE95Cu, 0x089EE95Cu});
    registration.call(0x08B878E8u, {0x08CA7FA0u, 0x08C0B2A4u, 0x08C0AEF0u});
}

// 0x089B837C: constructor-time registration of a script object (a0 = the object) with its text 0x08CA5920: 15 registration steps.
void register_089B837C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x30u, 0x08CA5920u, 0x089A24E4u, {{31u, 0x2Cu}, {18u, 0x28u}, {16u, 0x20u}, {17u, 0x24u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x089B6B2Cu});
    registration.call(0x08B8D51Cu, {0x08CA59F4u, 0x089B6B6Cu, 0u});
    registration.call(0x08B878E8u, {0x08CA58D0u, 0x089B6BECu, 0x089B6BECu});
    put(rt, registration.slot(0x14u), word(rt, 0x08C8C9CCu));
    put(rt, registration.slot(0x10u), word(rt, 0x08C8C9C8u));
    registration.call(0x08BFE438u, {0x08CA59FCu, registration.slot(0x10u), 0u, word(rt, 0x08C8C9C8u), word(rt, 0x08C8C9CCu)});
    registration.call(0x08B87108u, {0x08CA5A08u, 0x089B7FD8u});
    registration.call(0x08B87108u, {0x08CA5A14u, 0x089B6CE8u});
    registration.call(0x08B87108u, {0x08CA5A1Cu, 0x089B6E00u});
    registration.call(0x08B87108u, {0x08CA5A24u, 0x089B62BCu});
    registration.call(0x08B87108u, {0x08CA5A30u, 0x089B7E40u});
    put(rt, registration.slot(0x8u), word(rt, 0x08C8C9C0u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8C9C4u));
    registration.call(0x08BFE2E8u, {0x08CA5A40u, registration.slot(0x8u), word(rt, 0x08C8C9C0u), word(rt, 0x08C8C9C4u)});
    registration.call(0x08B87108u, {0x08CA5A4Cu, 0x089B7988u});
}

// 0x08A6A94C: constructor-time registration of a script object (a0 = the object) with its text 0x08CB04F4: 21 registration steps.
void register_08A6A94C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x50u, 0x08CB04F4u, 0x08A76A94u, {{31u, 0x4Cu}, {18u, 0x48u}, {16u, 0x40u}, {17u, 0x44u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08A6AB68u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08A6A308u});
    put(rt, registration.slot(0x30u), word(rt, 0x08C8EDBCu));
    put(rt, registration.slot(0x34u), word(rt, 0x08C8EDC0u));
    registration.call(0x08C2441Cu, {0x08CF6FC0u, registration.slot(0x30u), word(rt, 0x08C8EDBCu), word(rt, 0x08C8EDC0u)});
    put(rt, registration.slot(0x28u), word(rt, 0x08C8EDB4u));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C8EDB8u));
    registration.call(0x08C2441Cu, {0x08CF6F9Cu, registration.slot(0x28u), word(rt, 0x08C8EDB4u), word(rt, 0x08C8EDB8u)});
    put(rt, registration.slot(0x20u), word(rt, 0x08C8EDACu));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8EDB0u));
    registration.call(0x08C2441Cu, {0x08CF6FB8u, registration.slot(0x20u), word(rt, 0x08C8EDACu), word(rt, 0x08C8EDB0u)});
    put(rt, registration.slot(0x18u), word(rt, 0x08C8EDA4u));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8EDA8u));
    registration.call(0x08C2441Cu, {0x08CF6FBCu, registration.slot(0x18u), word(rt, 0x08C8EDA4u), word(rt, 0x08C8EDA8u)});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8ED9Cu));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8EDA0u));
    registration.call(0x08C2441Cu, {0x08CF6FA8u, registration.slot(0x10u), word(rt, 0x08C8ED9Cu), word(rt, 0x08C8EDA0u)});
    put(rt, registration.slot(0x8u), word(rt, 0x08C8ED94u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8ED98u));
    registration.call(0x08C2441Cu, {0x08CF6FB0u, registration.slot(0x8u), word(rt, 0x08C8ED94u), word(rt, 0x08C8ED98u)});
    registration.call(0x08B878E8u, {0x08CB04FCu, 0x08A69598u, 0x08A69598u});
}

// 0x08A04860: constructor-time registration of a script object (a0 = the object) with its text 0x08CA8D40: 43 registration steps.
void register_08A04860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x90u, 0x08CA8D40u, 0x08A76A94u, {{31u, 0x8Cu}, {18u, 0x88u}, {16u, 0x80u}, {17u, 0x84u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08A04254u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08A03FFCu});
    registration.call(0x08B86FB0u, {0x08CA8DC0u, 0x08A04354u});
    put(rt, registration.slot(0x78u), word(rt, 0x08C8DD10u));
    put(rt, registration.slot(0x7Cu), word(rt, 0x08C8DD14u));
    put(rt, registration.slot(0x70u), word(rt, 0x08C8DD08u));
    put(rt, registration.slot(0x74u), word(rt, 0x08C8DD0Cu));
    registration.call(0x08C103C8u, {0x08CA8DCCu, registration.slot(0x78u), registration.slot(0x70u), word(rt, 0x08C8DD14u), word(rt, 0x08C8DD0Cu)});
    put(rt, registration.slot(0x68u), word(rt, 0x08C8DD00u));
    put(rt, registration.slot(0x6Cu), word(rt, 0x08C8DD04u));
    put(rt, registration.slot(0x60u), word(rt, 0x08C8DCF8u));
    put(rt, registration.slot(0x64u), word(rt, 0x08C8DCFCu));
    registration.call(0x08C103C8u, {0x08CA8DD0u, registration.slot(0x68u), registration.slot(0x60u), word(rt, 0x08C8DD04u), word(rt, 0x08C8DCFCu)});
    put(rt, registration.slot(0x58u), word(rt, 0x08C8DCF0u));
    put(rt, registration.slot(0x5Cu), word(rt, 0x08C8DCF4u));
    put(rt, registration.slot(0x50u), word(rt, 0x08C8DCE8u));
    put(rt, registration.slot(0x54u), word(rt, 0x08C8DCECu));
    registration.call(0x08C103C8u, {0x08CA8DD4u, registration.slot(0x58u), registration.slot(0x50u), word(rt, 0x08C8DCF4u), word(rt, 0x08C8DCECu)});
    put(rt, registration.slot(0x48u), word(rt, 0x08C8DCE0u));
    put(rt, registration.slot(0x4Cu), word(rt, 0x08C8DCE4u));
    put(rt, registration.slot(0x40u), word(rt, 0x08C8DCD8u));
    put(rt, registration.slot(0x44u), word(rt, 0x08C8DCDCu));
    registration.call(0x08C103C8u, {0x08CA8DD8u, registration.slot(0x48u), registration.slot(0x40u), word(rt, 0x08C8DCE4u), word(rt, 0x08C8DCDCu)});
    put(rt, registration.slot(0x38u), word(rt, 0x08C8DCD0u));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C8DCD4u));
    put(rt, registration.slot(0x30u), word(rt, 0x08C8DCC8u));
    put(rt, registration.slot(0x34u), word(rt, 0x08C8DCCCu));
    registration.call(0x08C103C8u, {0x08CA8DDCu, registration.slot(0x38u), registration.slot(0x30u), word(rt, 0x08C8DCD4u), word(rt, 0x08C8DCCCu)});
    put(rt, registration.slot(0x28u), word(rt, 0x08C8DCC0u));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C8DCC4u));
    put(rt, registration.slot(0x20u), word(rt, 0x08C8DCB8u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8DCBCu));
    registration.call(0x08C103C8u, {0x08CA8DE0u, registration.slot(0x28u), registration.slot(0x20u), word(rt, 0x08C8DCC4u), word(rt, 0x08C8DCBCu)});
    put(rt, registration.slot(0x18u), word(rt, 0x08C8DCB0u));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8DCB4u));
    put(rt, registration.slot(0x10u), word(rt, 0x08C8DCA8u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8DCACu));
    registration.call(0x08C103C8u, {0x08CA8DE4u, registration.slot(0x18u), registration.slot(0x10u), word(rt, 0x08C8DCB4u), word(rt, 0x08C8DCACu)});
    registration.call(0x08B878E8u, {0x08CA8DE8u, 0x08A04704u, 0x08A04704u});
    registration.call(0x08B878E8u, {0x08CA8DF4u, 0x08A04570u, 0x08A04570u});
    put(rt, registration.slot(0x8u), word(rt, 0x08C8DCA0u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8DCA4u));
    registration.call(0x08C108DCu, {0x08CA8E00u, registration.slot(0x8u), 0u, word(rt, 0x08C8DCA0u), word(rt, 0x08C8DCA4u)});
}

// 0x08A08F7C: constructor-time registration of a script object (a0 = the object) with its text 0x08CA90C0: 33 registration steps.
void register_08A08F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x70u, 0x08CA90C0u, 0x08965B18u, {{31u, 0x6Cu}, {18u, 0x68u}, {16u, 0x60u}, {17u, 0x64u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08A08E58u});
    registration.call(0x08B878E8u, {0x08CA9120u, 0x08A083F0u, 0x08A082A8u});
    registration.call(0x08B87108u, {0x08CA9128u, 0x08A083F0u});
    registration.call(0x08B87108u, {0x08CA9134u, 0x08A082A8u});
    registration.call(0x08B87108u, {0x08CA9140u, 0x08A084D4u});
    registration.call(0x08B87108u, {0x08CA914Cu, 0x08A087ACu});
    put(rt, registration.slot(0x50u), word(rt, 0x08C8DEF0u));
    put(rt, registration.slot(0x54u), word(rt, 0x08C8DEF4u));
    put(rt, registration.slot(0x48u), word(rt, 0x08C8DEE8u));
    put(rt, registration.slot(0x4Cu), word(rt, 0x08C8DEECu));
    registration.call(0x08C12A8Cu, {0x08CA9158u, registration.slot(0x50u), registration.slot(0x48u), word(rt, 0x08C8DEF4u), word(rt, 0x08C8DEECu)});
    put(rt, registration.slot(0x40u), word(rt, 0x08C8DEE0u));
    put(rt, registration.slot(0x44u), word(rt, 0x08C8DEE4u));
    put(rt, registration.slot(0x38u), word(rt, 0x08C8DED8u));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C8DEDCu));
    registration.call(0x08C12794u, {0x08CA9164u, registration.slot(0x40u), registration.slot(0x38u), word(rt, 0x08C8DEE4u), word(rt, 0x08C8DEDCu)});
    put(rt, registration.slot(0x30u), word(rt, 0x08C8DED0u));
    put(rt, registration.slot(0x34u), word(rt, 0x08C8DED4u));
    put(rt, registration.slot(0x28u), word(rt, 0x08C8DEC8u));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C8DECCu));
    registration.call(0x08C12910u, {0x08CA9174u, registration.slot(0x30u), registration.slot(0x28u), word(rt, 0x08C8DED4u), word(rt, 0x08C8DECCu)});
    put(rt, registration.slot(0x20u), word(rt, 0x08C8DEC0u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8DEC4u));
    put(rt, registration.slot(0x18u), word(rt, 0x08C8DEB8u));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8DEBCu));
    registration.call(0x08C12794u, {0x08CA9184u, registration.slot(0x20u), registration.slot(0x18u), word(rt, 0x08C8DEC4u), word(rt, 0x08C8DEBCu)});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8DEB0u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8DEB4u));
    put(rt, registration.slot(0x8u), word(rt, 0x08C8DEA8u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8DEACu));
    registration.call(0x08C12794u, {0x08CA9190u, registration.slot(0x10u), registration.slot(0x8u), word(rt, 0x08C8DEB4u), word(rt, 0x08C8DEACu)});
    registration.call(0x08B878E8u, {0x08CA91A0u, 0x08A085B0u, 0x08A085B0u});
    registration.call(0x08B878E8u, {0x08CA8FB0u, 0x08A09274u, 0x08A09274u});
}

// 0x08A02D70: constructor-time registration of a script object (a0 = the object) with its text 0x08CA8B14: 28 registration steps.
void register_08A02D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x40u, 0x08CA8B14u, 0x08965B18u, {{31u, 0x3Cu}, {18u, 0x38u}, {16u, 0x30u}, {17u, 0x34u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08A02B08u});
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8DC3Cu));
    put(rt, registration.slot(0x20u), word(rt, 0x08C8DC40u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8DC44u));
    put(rt, registration.slot(0x18u), word(rt, 0x08C8DC38u));
    registration.call(0x08C0FA48u, {0x08CA8AA0u, registration.slot(0x20u), registration.slot(0x18u), word(rt, 0x08C8DC44u), word(rt, 0x08C8DC3Cu)});
    registration.call(0x08B87108u, {0x08CA8BA8u, 0x08A019A0u});
    registration.call(0x08B87108u, {0x08CA8BB4u, 0x08A018D8u});
    registration.call(0x08B87108u, {0x08CA8BC0u, 0x08A01780u});
    registration.call(0x08B87108u, {0x08CA8BCCu, 0x08A01668u});
    registration.call(0x08B87108u, {0x08CA8BD8u, 0x08A01558u});
    registration.call(0x08B87108u, {0x08CA8BE4u, 0x08A01A24u});
    registration.call(0x08B87108u, {0x08CA8BF4u, 0x08A022DCu});
    registration.call(0x08B87108u, {0x08CA8C00u, 0x08A014DCu});
    registration.call(0x08B87108u, {0x08CA8C0Cu, 0x08A01FFCu});
    registration.call(0x08B87108u, {0x08CA8C1Cu, 0x08A01EE8u});
    registration.call(0x08B87108u, {0x08CA8C2Cu, 0x08A0235Cu});
    registration.call(0x08B87108u, {0x08CA8C38u, 0x08A018D8u});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8DC30u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8DC34u));
    registration.call(0x08C100A0u, {0x08CA8C48u, registration.slot(0x10u), word(rt, 0x08C8DC30u), word(rt, 0x08C8DC34u)});
    put(rt, registration.slot(0xCu), word(rt, 0x08C8DC2Cu));
    put(rt, registration.slot(0x8u), word(rt, 0x08C8DC28u));
    registration.call(0x08C0FF04u, {0x08CA8C54u, registration.slot(0x8u), word(rt, 0x08C8DC28u), word(rt, 0x08C8DC2Cu)});
    registration.call(0x08B8D51Cu, {0x08CA8C68u, 0x08A02434u, 0u});
    registration.call(0x08B8D51Cu, {0x08CA8C70u, 0x08A024C8u, 0u});
    registration.call(0x08B8D51Cu, {0x08CA8C78u, 0x08A0255Cu, 0u});
    registration.call(0x08B8D51Cu, {0x08CA8C88u, 0x08A025F4u, 0u});
}

// 0x0880C3A8: constructor-time registration of a script object (a0 = the object) with its text 0x08C9848C: 11 registration steps.
void register_0880C3A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x20u, 0x08C9848Cu, 0x08965B18u, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08B88FB8u});
    registration.call(0x08B87108u, {0x08C984B0u, 0x0880CF40u});
    registration.call(0x08B87108u, {0x08C984C0u, 0x0880CE2Cu});
    registration.call(0x08B87108u, {0x08C984CCu, 0x0880CD18u});
    registration.call(0x08B87108u, {0x08C984DCu, 0x0880D278u});
    registration.call(0x08B87108u, {0x08C984ECu, 0x0880C134u});
    registration.call(0x08B87108u, {0x08C98500u, 0x0880C1DCu});
    registration.call(0x08B87108u, {0x08C9851Cu, 0x0880CB68u});
    registration.call(0x08B87108u, {0x08C98530u, 0x0880C9B8u});
    registration.call(0x08B87108u, {0x08C9854Cu, 0x0880C840u});
    registration.call(0x08B87108u, {0x08C98560u, 0x0880C2D4u});
}

// 0x0882506C: constructor-time registration of a script object (a0 = the object) with its text 0x08C99B4C: 71 registration steps.
void register_0882506C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x20u, 0x08C99B4Cu, 0x08A76A94u, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08825808u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08824710u});
    registration.call(0x08B87108u, {0x08C99B94u, 0x08824E88u});
    registration.call(0x08B87108u, {0x08C99BA4u, 0x08825848u});
    registration.call(0x08B87108u, {0x08C99BB0u, 0x088258D4u});
    registration.call(0x08B87108u, {0x08C99BBCu, 0x08824E38u});
    registration.call(0x08B87108u, {0x08C99BC8u, 0x08824DA0u});
    registration.call(0x08B87108u, {0x08C99BD8u, 0x08825960u});
    registration.call(0x08B87108u, {0x08C99BE8u, 0x08825AE8u});
    registration.call(0x08B87108u, {0x08C99BF0u, 0x08824D50u});
    registration.call(0x08B87108u, {0x08C99BFCu, 0x08825B74u});
    registration.call(0x08B87108u, {0x08C99C10u, 0x08824D00u});
    registration.call(0x08B87108u, {0x08C99C20u, 0x08825C00u});
    registration.call(0x08B87108u, {0x08C99C30u, 0x08824CB0u});
    registration.call(0x08B87108u, {0x08C99C38u, 0x08825FE4u});
    registration.call(0x08B87108u, {0x08C99C44u, 0x08824C60u});
    registration.call(0x08B87108u, {0x08C99C54u, 0x08826070u});
    registration.call(0x08B87108u, {0x08C99C60u, 0x08824C10u});
    registration.call(0x08B87108u, {0x08C99C70u, 0x08824BC0u});
    registration.call(0x08B87108u, {0x08C99C84u, 0x08824B70u});
    registration.call(0x08B87108u, {0x08C99C9Cu, 0x08824B20u});
    registration.call(0x08B87108u, {0x08C99CB8u, 0x088260FCu});
    registration.call(0x08B87108u, {0x08C99CD0u, 0x08824AD0u});
    registration.call(0x08B87108u, {0x08C99CE4u, 0x088261CCu});
    registration.call(0x08B87108u, {0x08C99CF8u, 0x0882629Cu});
    registration.call(0x08B87108u, {0x08C99D08u, 0x08826328u});
    registration.call(0x08B87108u, {0x08C99D24u, 0x088263B4u});
    registration.call(0x08B87108u, {0x08C99D38u, 0x08826440u});
    registration.call(0x08B87108u, {0x08C99D4Cu, 0x0882650Cu});
    registration.call(0x08B87108u, {0x08C99D64u, 0x08826628u});
    registration.call(0x08B87108u, {0x08C99D7Cu, 0x08826700u});
    registration.call(0x08B87108u, {0x08C99D94u, 0x088249F8u});
    registration.call(0x08B87108u, {0x08C99DACu, 0x088267D0u});
    registration.call(0x08B87108u, {0x08C99DBCu, 0x08824924u});
    registration.call(0x08B87108u, {0x08C99DCCu, 0x0882689Cu});
    registration.call(0x08B87108u, {0x08C99DDCu, 0x08826968u});
    registration.call(0x08B87108u, {0x08C99DF4u, 0x08826A4Cu});
    registration.call(0x08B87108u, {0x08C99E0Cu, 0x08826B80u});
    registration.call(0x08B87108u, {0x08C99E28u, 0x08826C50u});
    registration.call(0x08B87108u, {0x08C99E48u, 0x08826D9Cu});
    registration.call(0x08B87108u, {0x08C99E64u, 0x08826E68u});
    registration.call(0x08B87108u, {0x08C99E80u, 0x08826FB8u});
    registration.call(0x08B87108u, {0x08C99E9Cu, 0x088270E8u});
    registration.call(0x08B87108u, {0x08C99EB8u, 0x08827200u});
    registration.call(0x08B87108u, {0x08C99ED4u, 0x0882736Cu});
    registration.call(0x08B87108u, {0x08C99EF0u, 0x088274C0u});
    registration.call(0x08B87108u, {0x08C99F08u, 0x0882758Cu});
    registration.call(0x08B87108u, {0x08C99F24u, 0x08827658u});
    registration.call(0x08B87108u, {0x08C99F38u, 0x08827758u});
    registration.call(0x08B87108u, {0x08C99F4Cu, 0x08827890u});
    registration.call(0x08B87108u, {0x08C99F60u, 0x088279A4u});
    registration.call(0x08B87108u, {0x08C99F7Cu, 0x08827AB4u});
    registration.call(0x08B87108u, {0x08C99F94u, 0x08827B98u});
    registration.call(0x08B87108u, {0x08C99FACu, 0x08827D0Cu});
    registration.call(0x08B87108u, {0x08C99FC4u, 0x08827E7Cu});
    registration.call(0x08B87108u, {0x08C99FE0u, 0x08827F50u});
    registration.call(0x08B87108u, {0x08C99FFCu, 0x08828024u});
    registration.call(0x08B87108u, {0x08C9A018u, 0x088280F8u});
    registration.call(0x08B87108u, {0x08C9A038u, 0x088281CCu});
    registration.call(0x08B87108u, {0x08C9A058u, 0x088282A0u});
    registration.call(0x08B878E8u, {0x08C9A074u, 0x08828374u, 0x08828374u});
    registration.call(0x08B878E8u, {0x08C9A080u, 0x08828494u, 0x08828494u});
    registration.call(0x08B878E8u, {0x08C9A090u, 0x088285ACu, 0x088285ACu});
    registration.call(0x08B878E8u, {0x08C9A0A0u, 0x088286B8u, 0x088286B8u});
    registration.call(0x08B878E8u, {0x08C9A0B4u, 0x088287C4u, 0x088287C4u});
    registration.call(0x08B878E8u, {0x08C9A0CCu, 0x088288DCu, 0x088288DCu});
    registration.call(0x08B878E8u, {0x08C9A0E0u, 0x0882897Cu, 0x0882897Cu});
    registration.call(0x08B878E8u, {0x08C9A0F8u, 0x08828A1Cu, 0x08828A1Cu});
    registration.call(0x08B878E8u, {0x08C9A108u, 0x08828ABCu, 0x08828ABCu});
    registration.call(0x08B878E8u, {0x08C9A114u, 0x08828BA4u, 0x08828BA4u});
    registration.call(0x08B878E8u, {0x08C9A124u, 0x08828C8Cu, 0x08828C8Cu});
}

// 0x08A388AC: constructor-time registration of a script object (a0 = the object) with its text 0x08CAD29C: 7 registration steps.
void register_08A388AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x20u, 0x08CAD29Cu, 0x08A76A94u, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08C1B434u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08A37D50u});
    registration.call(0x08B86FB0u, {0x08CAD2C4u, 0x08A38170u});
    registration.call(0x08B87108u, {0x08CAD2D0u, 0x08A38304u});
    registration.call(0x08B878E8u, {0x08CAD2DCu, 0x08A38418u, 0x08A38418u});
    registration.call(0x08B87108u, {0x08CAD2E4u, 0x08A38540u});
    registration.call(0x08B87108u, {0x08CAD2F0u, 0x08A386E8u});
}

// 0x089C2D2C: constructor-time registration of a script object (a0 = the object) with its text 0x08CA6164: 26 registration steps.
void register_089C2D2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x60u, 0x08CA6164u, 0x08A0F03Cu, {{31u, 0x5Cu}, {18u, 0x58u}, {16u, 0x50u}, {17u, 0x54u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x089C2A88u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x089C2370u});
    registration.call(0x08B87108u, {0x08CA61BCu, 0x089C22C4u});
    registration.call(0x08B87108u, {0x08CA61C4u, 0x089C2278u});
    registration.call(0x08B878E8u, {0x08CA61D4u, 0x089C2AC8u, 0x089C2AC8u});
    registration.call(0x08B878E8u, {0x08CA61E4u, 0x089C2BB0u, 0x089C2BB0u});
    put(rt, registration.slot(0x40u), word(rt, 0x08C8CBC4u));
    put(rt, registration.slot(0x44u), word(rt, 0x08C8CBC8u));
    put(rt, registration.slot(0x38u), word(rt, 0x08C8CBBCu));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C8CBC0u));
    registration.call(0x08C001A4u, {0x08CA6114u, registration.slot(0x40u), registration.slot(0x38u), word(rt, 0x08C8CBC8u), word(rt, 0x08C8CBC0u)});
    put(rt, registration.slot(0x30u), word(rt, 0x08C8CBB4u));
    put(rt, registration.slot(0x34u), word(rt, 0x08C8CBB8u));
    put(rt, registration.slot(0x28u), word(rt, 0x08C8CBACu));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C8CBB0u));
    registration.call(0x08C00028u, {0x08CA6120u, registration.slot(0x30u), registration.slot(0x28u), word(rt, 0x08C8CBB8u), word(rt, 0x08C8CBB0u)});
    put(rt, registration.slot(0x20u), word(rt, 0x08C8CBA4u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8CBA8u));
    put(rt, registration.slot(0x18u), word(rt, 0x08C8CB9Cu));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8CBA0u));
    registration.call(0x08C00028u, {0x08CA6128u, registration.slot(0x20u), registration.slot(0x18u), word(rt, 0x08C8CBA8u), word(rt, 0x08C8CBA0u)});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8CB94u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8CB98u));
    put(rt, registration.slot(0x8u), word(rt, 0x08C8CB8Cu));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8CB90u));
    registration.call(0x08C00028u, {0x08CA6130u, registration.slot(0x10u), registration.slot(0x8u), word(rt, 0x08C8CB98u), word(rt, 0x08C8CB90u)});
}

// 0x08A9B404: constructor-time registration of a script object (a0 = the object) with its text 0x08CB2428: 31 registration steps.
void register_08A9B404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x40u, 0x08CB2428u, 0x08A76A94u, {{31u, 0x3Cu}, {18u, 0x38u}, {16u, 0x30u}, {17u, 0x34u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08A991ECu});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08A9B1C4u});
    registration.call(0x08B878E8u, {0x08CB2430u, 0x08A9B05Cu, 0x08A9B05Cu});
    registration.call(0x08B87108u, {0x08CB2438u, 0x08A9B05Cu});
    registration.call(0x08B87108u, {0x08CB2440u, 0x08A9A994u});
    registration.call(0x08B87108u, {0x08CB2448u, 0x08A9922Cu});
    put(rt, registration.slot(0x20u), word(rt, 0x08C8FA5Cu));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8FA60u));
    registration.call(0x08C2F948u, {0x08CB2450u, registration.slot(0x20u), word(rt, 0x08C8FA5Cu), word(rt, 0x08C8FA60u)});
    registration.call(0x08B87108u, {0x08CB2458u, 0x08A9AA30u});
    registration.call(0x08B87108u, {0x08CB2460u, 0x08A992ACu});
    registration.call(0x08B87108u, {0x08CB2464u, 0x08A9A310u});
    registration.call(0x08B87108u, {0x08CB246Cu, 0x08A9A09Cu});
    registration.call(0x08B87108u, {0x08CB2474u, 0x08A98F80u});
    registration.call(0x08B87108u, {0x08CB247Cu, 0x08A99864u});
    registration.call(0x08B87108u, {0x08CB2484u, 0x08A9AB10u});
    registration.call(0x08B87108u, {0x08CB248Cu, 0x08A9A448u});
    registration.call(0x08B87108u, {0x08CB2494u, 0x08A9AC60u});
    registration.call(0x08B87108u, {0x08CB249Cu, 0x08A99E1Cu});
    registration.call(0x08B87108u, {0x08CB24A4u, 0x08A99698u});
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8FA58u));
    put(rt, registration.slot(0x18u), word(rt, 0x08C8FA54u));
    registration.call(0x08C2F7ACu, {0x08CB24ACu, registration.slot(0x18u), word(rt, 0x08C8FA54u), word(rt, 0x08C8FA58u)});
    registration.call(0x08B87108u, {0x08CB24B4u, 0x08A99198u});
    registration.call(0x08B8D51Cu, {0x08CB24C4u, 0x08A9932Cu, 0u});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8FA4Cu));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8FA50u));
    registration.call(0x08C2F610u, {0x08CF6FD0u, registration.slot(0x10u), word(rt, 0x08C8FA4Cu), word(rt, 0x08C8FA50u)});
    put(rt, registration.slot(0x8u), word(rt, 0x08C8FA44u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8FA48u));
    registration.call(0x08C2F4B8u, {0x08CB24D8u, registration.slot(0x8u), word(rt, 0x08C8FA44u), word(rt, 0x08C8FA48u)});
}

// 0x08A00010: constructor-time registration of a script object (a0 = the object) with its text 0x08CA8A30: 12 registration steps.
void register_08A00010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x30u, 0x08CA8A30u, 0x08A0F03Cu, {{31u, 0x2Cu}, {18u, 0x28u}, {16u, 0x20u}, {17u, 0x24u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x089FFC9Cu});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8DB9Cu));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8DBA0u));
    put(rt, registration.slot(0x8u), word(rt, 0x08C8DB94u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8DB98u));
    registration.call(0x08C0F43Cu, {0x08CA89CCu, registration.slot(0x10u), registration.slot(0x8u), word(rt, 0x08C8DBA0u), word(rt, 0x08C8DB98u)});
    registration.call(0x08B87108u, {0x08CA8A40u, 0x089FFB74u});
    registration.call(0x08B87108u, {0x08CA8A4Cu, 0x089FFA1Cu});
    registration.call(0x08B87108u, {0x08CA8A58u, 0x089FF904u});
    registration.call(0x08B87108u, {0x08CA8A64u, 0x089FF7F4u});
    registration.call(0x08B87108u, {0x08CA8A70u, 0x089FF69Cu});
    registration.call(0x08B87108u, {0x08CA8A80u, 0x089FFCDCu});
}

// 0x0899E370: constructor-time registration of a script object (a0 = the object) with its text 0x08CA496C: 23 registration steps.
void register_0899E370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x30u, 0x08CA496Cu, 0x089A24E4u, {{31u, 0x2Cu}, {18u, 0x28u}, {16u, 0x20u}, {17u, 0x24u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x0899E668u});
    registration.call(0x08B878E8u, {0x08CA48B8u, 0x0899E6A8u, 0x0899E6A8u});
    registration.call(0x08B878E8u, {0x08CA48DCu, 0x0899E7A8u, 0x0899E7A8u});
    registration.call(0x08B878E8u, {0x08CA4978u, 0x0899E8A8u, 0x0899E8A8u});
    registration.call(0x08B878E8u, {0x08CA4988u, 0x0899E9A8u, 0x0899E9A8u});
    registration.call(0x08B878E8u, {0x08CA4990u, 0x0899EB14u, 0x0899EB14u});
    registration.call(0x08B878E8u, {0x08CA48E4u, 0x0899EC14u, 0x0899EC14u});
    registration.call(0x08B878E8u, {0x08CA48E8u, 0x0899ED14u, 0x0899ED14u});
    registration.call(0x08B878E8u, {0x08CA49A0u, 0x0899EE14u, 0x0899EE14u});
    registration.call(0x08B878E8u, {0x08CA49ACu, 0x0899EF08u, 0x0899EF08u});
    registration.call(0x08B878E8u, {0x08CA49B8u, 0x0899EFFCu, 0x0899EFFCu});
    registration.call(0x08B878E8u, {0x08CA48ECu, 0x0899F2D4u, 0x0899F2D4u});
    registration.call(0x08B878E8u, {0x08CA48F4u, 0x0899F3D4u, 0x0899F3D4u});
    registration.call(0x08B878E8u, {0x08CA4950u, 0x0899F4C8u, 0x0899F4C8u});
    registration.call(0x08B8D51Cu, {0x08CA49C0u, 0x0899F5CCu, 0u});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8C4DCu));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8C4E0u));
    put(rt, registration.slot(0x8u), word(rt, 0x08C8C4D4u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8C4D8u));
    registration.call(0x08BF44D0u, {0x08CA48ACu, registration.slot(0x10u), registration.slot(0x8u), word(rt, 0x08C8C4E0u), word(rt, 0x08C8C4D8u)});
    registration.call(0x08B87108u, {0x08CA49C8u, 0x0899DE64u});
    registration.call(0x08B87108u, {0x08CA49D4u, 0x0899DDA0u});
    registration.call(0x08B87108u, {0x08CA49E0u, 0x0899FCE8u});
}

// 0x08847C90: constructor-time registration of a script object (a0 = the object) with its text 0x08C9CBBC: 61 registration steps.
void register_08847C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x20u, 0x08C9CBBCu, 0x08A76A94u, {{31u, 0x1Cu}, {18u, 0x18u}, {16u, 0x10u}, {17u, 0x14u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08848E78u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08846E58u});
    registration.call(0x08B87108u, {0x08C9CC00u, 0x08847088u});
    registration.call(0x08B878E8u, {0x08C9CC0Cu, 0x08848EB8u, 0x08848EB8u});
    registration.call(0x08B878E8u, {0x08C9CC18u, 0x08848FC8u, 0x08848FC8u});
    registration.call(0x08B878E8u, {0x08C9CC24u, 0x088490D8u, 0x088490D8u});
    registration.call(0x08B878E8u, {0x08C9CC34u, 0x088491E8u, 0x088491E8u});
    registration.call(0x08B878E8u, {0x08C9CC40u, 0x088492F8u, 0x088492F8u});
    registration.call(0x08B878E8u, {0x08C9CC50u, 0x08849408u, 0x08849408u});
    registration.call(0x08B878E8u, {0x08C9CC64u, 0x08849518u, 0x08849518u});
    registration.call(0x08B878E8u, {0x08C9CC74u, 0x08849628u, 0x08849628u});
    registration.call(0x08B878E8u, {0x08C9CC84u, 0x08849738u, 0x08849738u});
    registration.call(0x08B878E8u, {0x08C9CC94u, 0x08849848u, 0x08849848u});
    registration.call(0x08B878E8u, {0x08C9CCA0u, 0x0884996Cu, 0x0884996Cu});
    registration.call(0x08B878E8u, {0x08C9CCACu, 0x08849A7Cu, 0x08849A7Cu});
    registration.call(0x08B878E8u, {0x08C9CCB8u, 0x08849B8Cu, 0x08849B8Cu});
    registration.call(0x08B878E8u, {0x08C9CCC4u, 0x08849D4Cu, 0x08849D4Cu});
    registration.call(0x08B878E8u, {0x08C9CCD8u, 0x08849E5Cu, 0x08849E5Cu});
    registration.call(0x08B878E8u, {0x08C9CCE8u, 0x08849F6Cu, 0x08849F6Cu});
    registration.call(0x08B878E8u, {0x08C9CD00u, 0x0884A07Cu, 0x0884A07Cu});
    registration.call(0x08B878E8u, {0x08C9CD20u, 0x0884A18Cu, 0x0884A18Cu});
    registration.call(0x08B878E8u, {0x08C9CD34u, 0x0884A29Cu, 0x0884A29Cu});
    registration.call(0x08B878E8u, {0x08C9CD44u, 0x0884A3ACu, 0x0884A3ACu});
    registration.call(0x08B878E8u, {0x08C9CD50u, 0x0884A4BCu, 0x0884A4BCu});
    registration.call(0x08B878E8u, {0x08C9CD58u, 0x0884A5CCu, 0x0884A5CCu});
    registration.call(0x08B878E8u, {0x08C9CD60u, 0x0884A6DCu, 0x0884A6DCu});
    registration.call(0x08B878E8u, {0x08C9CD6Cu, 0x0884A7ECu, 0x0884A7ECu});
    registration.call(0x08B878E8u, {0x08C9CD80u, 0x0884A8FCu, 0x0884A8FCu});
    registration.call(0x08B878E8u, {0x08C9CD90u, 0x0884AA0Cu, 0x0884AA0Cu});
    registration.call(0x08B878E8u, {0x08C9CDA4u, 0x0884AB1Cu, 0x0884AB1Cu});
    registration.call(0x08B878E8u, {0x08C9CDB4u, 0x0884AC2Cu, 0x0884AC2Cu});
    registration.call(0x08B878E8u, {0x08C9CDC8u, 0x0884AD3Cu, 0x0884AD3Cu});
    registration.call(0x08B878E8u, {0x08C9CDE0u, 0x0884AE4Cu, 0x0884AE4Cu});
    registration.call(0x08B878E8u, {0x08C9CDF8u, 0x0884AF5Cu, 0x0884AF5Cu});
    registration.call(0x08B87108u, {0x08C9CE0Cu, 0x08847144u});
    registration.call(0x08B87108u, {0x08C9CE1Cu, 0x0884B06Cu});
    registration.call(0x08B87108u, {0x08C9CE2Cu, 0x088471E4u});
    registration.call(0x08B87108u, {0x08C9CE3Cu, 0x0884B148u});
    registration.call(0x08B87108u, {0x08C9CE50u, 0x088472C0u});
    registration.call(0x08B87108u, {0x08C9CE64u, 0x0884B224u});
    registration.call(0x08B87108u, {0x08C9CE74u, 0x0884739Cu});
    registration.call(0x08B87108u, {0x08C9CE84u, 0x08847580u});
    registration.call(0x08B87108u, {0x08C9CE90u, 0x0884865Cu});
    registration.call(0x08B87108u, {0x08C9CEA8u, 0x08848398u});
    registration.call(0x08B878E8u, {0x08C9CEC4u, 0x0884B478u, 0x0884B478u});
    registration.call(0x08B87108u, {0x08C9CEDCu, 0x088475D8u});
    registration.call(0x08B87108u, {0x08C9CEF0u, 0x088476BCu});
    registration.call(0x08B878E8u, {0x08C9CF00u, 0x0884B8C0u, 0x0884B8C0u});
    registration.call(0x08B87108u, {0x08C9CF10u, 0x0884B9D0u});
    registration.call(0x08B87108u, {0x08C9CF18u, 0x0884BA94u});
    registration.call(0x08B87108u, {0x08C9CF24u, 0x0884BB28u});
    registration.call(0x08B878E8u, {0x08C9CF3Cu, 0x0884BBBCu, 0x0884BBBCu});
    registration.call(0x08B87108u, {0x08C9CF4Cu, 0x0884BCCCu});
    registration.call(0x08B87108u, {0x08C9CF58u, 0x088477A0u});
    registration.call(0x08B87108u, {0x08C9CF64u, 0x0884787Cu});
    registration.call(0x08B87108u, {0x08C9CF78u, 0x0884BDA8u});
    registration.call(0x08B87108u, {0x08C9CF8Cu, 0x08847958u});
    registration.call(0x08B87108u, {0x08C9CFA4u, 0x0884BE84u});
    registration.call(0x08B87108u, {0x08C9CFB0u, 0x0884BF60u});
    registration.call(0x08B87108u, {0x08C9CFC0u, 0x088479B0u});
    registration.call(0x08B87108u, {0x08C9CFD0u, 0x08847B38u});
}

// 0x08AB6654: constructor-time registration of a script object (a0 = the object) with its text 0x08CB40B4: 37 registration steps.
void register_08AB6654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x70u, 0x08CB40B4u, 0x08A70C44u, {{31u, 0x6Cu}, {18u, 0x68u}, {16u, 0x60u}, {17u, 0x64u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08AB63ACu});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08AB621Cu});
    registration.call(0x08B87108u, {0x08CB40C4u, 0x08AB6564u});
    put(rt, registration.slot(0x58u), word(rt, 0x08C904A0u));
    put(rt, registration.slot(0x5Cu), word(rt, 0x08C904A4u));
    registration.call(0x08C3BA48u, {0x08CB40D0u, registration.slot(0x58u), word(rt, 0x08C904A0u), word(rt, 0x08C904A4u)});
    put(rt, registration.slot(0x50u), word(rt, 0x08C90498u));
    put(rt, registration.slot(0x54u), word(rt, 0x08C9049Cu));
    registration.call(0x08C3B8ACu, {0x08CB40DCu, registration.slot(0x50u), word(rt, 0x08C90498u), word(rt, 0x08C9049Cu)});
    put(rt, registration.slot(0x48u), word(rt, 0x08C90490u));
    put(rt, registration.slot(0x4Cu), word(rt, 0x08C90494u));
    registration.call(0x08C3B710u, {0x08CB40E8u, registration.slot(0x48u), word(rt, 0x08C90490u), word(rt, 0x08C90494u)});
    put(rt, registration.slot(0x40u), word(rt, 0x08C90488u));
    put(rt, registration.slot(0x44u), word(rt, 0x08C9048Cu));
    registration.call(0x08C3B574u, {0x08CB40F0u, registration.slot(0x40u), word(rt, 0x08C90488u), word(rt, 0x08C9048Cu)});
    put(rt, registration.slot(0x38u), word(rt, 0x08C90480u));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C90484u));
    registration.call(0x08C3B3D8u, {0x08CB40FCu, registration.slot(0x38u), word(rt, 0x08C90480u), word(rt, 0x08C90484u)});
    put(rt, registration.slot(0x30u), word(rt, 0x08C90478u));
    put(rt, registration.slot(0x34u), word(rt, 0x08C9047Cu));
    registration.call(0x08C3B23Cu, {0x08CB4108u, registration.slot(0x30u), word(rt, 0x08C90478u), word(rt, 0x08C9047Cu)});
    put(rt, registration.slot(0x28u), word(rt, 0x08C90470u));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C90474u));
    registration.call(0x08C3B0A0u, {0x08CB4114u, registration.slot(0x28u), word(rt, 0x08C90470u), word(rt, 0x08C90474u)});
    put(rt, registration.slot(0x20u), word(rt, 0x08C90468u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C9046Cu));
    registration.call(0x08C3AF04u, {0x08CB4120u, registration.slot(0x20u), word(rt, 0x08C90468u), word(rt, 0x08C9046Cu)});
    put(rt, registration.slot(0x18u), word(rt, 0x08C90460u));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C90464u));
    registration.call(0x08C3AD68u, {0x08CB412Cu, registration.slot(0x18u), word(rt, 0x08C90460u), word(rt, 0x08C90464u)});
    put(rt, registration.slot(0x10u), word(rt, 0x08C90458u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C9045Cu));
    registration.call(0x08C3ABCCu, {0x08CB4138u, registration.slot(0x10u), word(rt, 0x08C90458u), word(rt, 0x08C9045Cu)});
    put(rt, registration.slot(0x8u), word(rt, 0x08C90450u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C90454u));
    registration.call(0x08C3AA30u, {0x08CB4144u, registration.slot(0x8u), word(rt, 0x08C90450u), word(rt, 0x08C90454u)});
    registration.call(0x08B878E8u, {0x08CB4154u, 0x08AB6414u, 0x08AB6414u});
}

// 0x08AB5B2C: constructor-time registration of a script object (a0 = the object) with its text 0x08CB3F4C: 39 registration steps.
void register_08AB5B2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x80u, 0x08CB3F4Cu, 0x08A701B8u, {{31u, 0x7Cu}, {18u, 0x78u}, {16u, 0x70u}, {17u, 0x74u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08AB5A68u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08AB58D8u});
    put(rt, registration.slot(0x60u), word(rt, 0x08C903C4u));
    put(rt, registration.slot(0x64u), word(rt, 0x08C903C8u));
    registration.call(0x08C39AB0u, {0x08CB3F5Cu, registration.slot(0x60u), word(rt, 0x08C903C4u), word(rt, 0x08C903C8u)});
    put(rt, registration.slot(0x58u), word(rt, 0x08C903BCu));
    put(rt, registration.slot(0x5Cu), word(rt, 0x08C903C0u));
    registration.call(0x08C39914u, {0x08CB3F68u, registration.slot(0x58u), word(rt, 0x08C903BCu), word(rt, 0x08C903C0u)});
    put(rt, registration.slot(0x50u), word(rt, 0x08C903B4u));
    put(rt, registration.slot(0x54u), word(rt, 0x08C903B8u));
    registration.call(0x08C39778u, {0x08CB3F74u, registration.slot(0x50u), word(rt, 0x08C903B4u), word(rt, 0x08C903B8u)});
    put(rt, registration.slot(0x48u), word(rt, 0x08C903ACu));
    put(rt, registration.slot(0x4Cu), word(rt, 0x08C903B0u));
    registration.call(0x08C395DCu, {0x08CB3F80u, registration.slot(0x48u), word(rt, 0x08C903ACu), word(rt, 0x08C903B0u)});
    put(rt, registration.slot(0x40u), word(rt, 0x08C903A4u));
    put(rt, registration.slot(0x44u), word(rt, 0x08C903A8u));
    registration.call(0x08C39440u, {0x08CB3F8Cu, registration.slot(0x40u), word(rt, 0x08C903A4u), word(rt, 0x08C903A8u)});
    put(rt, registration.slot(0x38u), word(rt, 0x08C9039Cu));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C903A0u));
    registration.call(0x08C392A4u, {0x08CB3F98u, registration.slot(0x38u), word(rt, 0x08C9039Cu), word(rt, 0x08C903A0u)});
    put(rt, registration.slot(0x30u), word(rt, 0x08C90394u));
    put(rt, registration.slot(0x34u), word(rt, 0x08C90398u));
    registration.call(0x08C39108u, {0x08CB3FA4u, registration.slot(0x30u), word(rt, 0x08C90394u), word(rt, 0x08C90398u)});
    put(rt, registration.slot(0x28u), word(rt, 0x08C9038Cu));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C90390u));
    registration.call(0x08C38F6Cu, {0x08CB3FB0u, registration.slot(0x28u), word(rt, 0x08C9038Cu), word(rt, 0x08C90390u)});
    put(rt, registration.slot(0x20u), word(rt, 0x08C90384u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C90388u));
    registration.call(0x08C38DD0u, {0x08CB3FBCu, registration.slot(0x20u), word(rt, 0x08C90384u), word(rt, 0x08C90388u)});
    put(rt, registration.slot(0x18u), word(rt, 0x08C9037Cu));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C90380u));
    registration.call(0x08C38C34u, {0x08CB3FC8u, registration.slot(0x18u), word(rt, 0x08C9037Cu), word(rt, 0x08C90380u)});
    put(rt, registration.slot(0x10u), word(rt, 0x08C90374u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C90378u));
    registration.call(0x08C38A98u, {0x08CB3FD4u, registration.slot(0x10u), word(rt, 0x08C90374u), word(rt, 0x08C90378u)});
    put(rt, registration.slot(0x8u), word(rt, 0x08C9036Cu));
    put(rt, registration.slot(0xCu), word(rt, 0x08C90370u));
    registration.call(0x08C388FCu, {0x08CB3FE0u, registration.slot(0x8u), word(rt, 0x08C9036Cu), word(rt, 0x08C90370u)});
    registration.call(0x08B878E8u, {0x08CB3FF0u, 0x08AB57E4u, 0x08AB57E4u});
}

// 0x089BE4BC: constructor-time registration of a script object (a0 = the object) with its text 0x08CA5CB0: 33 registration steps.
void register_089BE4BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x60u, 0x08CA5CB0u, 0x08A7BC7Cu, {{31u, 0x5Cu}, {18u, 0x58u}, {16u, 0x50u}, {17u, 0x54u}});
    registration.call(0x08B878E8u, {0x08CA5DC0u, 0x089BBC34u, 0x089BBC34u});
    registration.call(0x08B8D51Cu, {0x08CA5DC8u, 0x089BBD28u, 0u});
    registration.call(0x08B8D51Cu, {0x08CA5DD0u, 0x089BBDB0u, 0u});
    registration.call(0x08B8D51Cu, {0x08CA5DD8u, 0x089BBD28u, 0u});
    registration.call(0x08B8D51Cu, {0x08CA5DE4u, 0x089BBDB0u, 0u});
    put(rt, registration.slot(0x40u), word(rt, 0x08C8CAE0u));
    put(rt, registration.slot(0x44u), word(rt, 0x08C8CAE4u));
    registration.call(0x08BFFA28u, {0x08CA5DF0u, registration.slot(0x40u), 0u, word(rt, 0x08C8CAE0u), word(rt, 0x08C8CAE4u)});
    put(rt, registration.slot(0x38u), word(rt, 0x08C8CAD8u));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C8CADCu));
    put(rt, registration.slot(0x30u), word(rt, 0x08C8CAD0u));
    put(rt, registration.slot(0x34u), word(rt, 0x08C8CAD4u));
    registration.call(0x08BFF0CCu, {0x08CA5C70u, registration.slot(0x38u), registration.slot(0x30u), word(rt, 0x08C8CADCu), word(rt, 0x08C8CAD4u)});
    put(rt, registration.slot(0x28u), word(rt, 0x08C8CAC8u));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C8CACCu));
    put(rt, registration.slot(0x20u), word(rt, 0x08C8CAC0u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8CAC4u));
    registration.call(0x08BFEF50u, {0x08CA5C38u, registration.slot(0x28u), registration.slot(0x20u), word(rt, 0x08C8CACCu), word(rt, 0x08C8CAC4u)});
    put(rt, registration.slot(0x18u), word(rt, 0x08C8CAB8u));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8CABCu));
    put(rt, registration.slot(0x10u), word(rt, 0x08C8CAB0u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8CAB4u));
    registration.call(0x08BFEF50u, {0x08CA5C50u, registration.slot(0x18u), registration.slot(0x10u), word(rt, 0x08C8CABCu), word(rt, 0x08C8CAB4u)});
    registration.call(0x08B8D51Cu, {0x08CA5E00u, 0x089BDF0Cu, 0u});
    registration.call(0x08BE6154u, {0x08CA5E0Cu, 0u, 0x089BB504u});
    registration.call(0x08B87108u, {0x08CA5E20u, 0x089BB440u});
    registration.call(0x08B87108u, {0x08CA5E30u, 0x089BBE38u});
    registration.call(0x08B87108u, {0x08CA5E38u, 0x089BD080u});
    registration.call(0x08B87108u, {0x08CA5E44u, 0x089BDE34u});
    registration.call(0x08B87108u, {0x08CA5E58u, 0x089BD844u});
    put(rt, registration.slot(0x8u), word(rt, 0x08C8CAA8u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8CAACu));
    registration.call(0x08BFFA28u, {0x08CA5C44u, registration.slot(0x8u), 0u, word(rt, 0x08C8CAA8u), word(rt, 0x08C8CAACu)});
}

// 0x089A4648: constructor-time registration of a script object (a0 = the object) with its text 0x08CA4D28: 57 registration steps.
void register_089A4648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0xA0u, 0x08CA4D28u, 0x089A24E4u, {{31u, 0x9Cu}, {18u, 0x98u}, {16u, 0x90u}, {17u, 0x94u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x089A5D40u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x089A361Cu});
    registration.call(0x08B87108u, {0x08CA4DFCu, 0x089A358Cu});
    registration.call(0x08B87108u, {0x08CA4E08u, 0x089A3F54u});
    registration.call(0x08B878E8u, {0x08CA4E1Cu, 0x089A3F54u, 0x089A358Cu});
    put(rt, registration.slot(0x80u), word(rt, 0x08C8C6A0u));
    put(rt, registration.slot(0x84u), word(rt, 0x08C8C6A4u));
    put(rt, registration.slot(0x78u), word(rt, 0x08C8C698u));
    put(rt, registration.slot(0x7Cu), word(rt, 0x08C8C69Cu));
    registration.call(0x08BF5E10u, {0x08CA4CCCu, registration.slot(0x80u), registration.slot(0x78u), word(rt, 0x08C8C6A4u), word(rt, 0x08C8C69Cu)});
    put(rt, registration.slot(0x70u), word(rt, 0x08C8C690u));
    put(rt, registration.slot(0x74u), word(rt, 0x08C8C694u));
    put(rt, registration.slot(0x68u), word(rt, 0x08C8C688u));
    put(rt, registration.slot(0x6Cu), word(rt, 0x08C8C68Cu));
    registration.call(0x08BF6108u, {0x08CA4C28u, registration.slot(0x70u), registration.slot(0x68u), word(rt, 0x08C8C694u), word(rt, 0x08C8C68Cu)});
    put(rt, registration.slot(0x60u), word(rt, 0x08C8C680u));
    put(rt, registration.slot(0x64u), word(rt, 0x08C8C684u));
    put(rt, registration.slot(0x58u), word(rt, 0x08C8C678u));
    put(rt, registration.slot(0x5Cu), word(rt, 0x08C8C67Cu));
    registration.call(0x08BF5E10u, {0x08CA4C74u, registration.slot(0x60u), registration.slot(0x58u), word(rt, 0x08C8C684u), word(rt, 0x08C8C67Cu)});
    put(rt, registration.slot(0x50u), word(rt, 0x08C8C670u));
    put(rt, registration.slot(0x54u), word(rt, 0x08C8C674u));
    put(rt, registration.slot(0x48u), word(rt, 0x08C8C668u));
    put(rt, registration.slot(0x4Cu), word(rt, 0x08C8C66Cu));
    registration.call(0x08BF5E10u, {0x08CA4C8Cu, registration.slot(0x50u), registration.slot(0x48u), word(rt, 0x08C8C674u), word(rt, 0x08C8C66Cu)});
    put(rt, registration.slot(0x40u), word(rt, 0x08C8C660u));
    put(rt, registration.slot(0x44u), word(rt, 0x08C8C664u));
    put(rt, registration.slot(0x38u), word(rt, 0x08C8C658u));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C8C65Cu));
    registration.call(0x08BF5E10u, {0x08CA4CA0u, registration.slot(0x40u), registration.slot(0x38u), word(rt, 0x08C8C664u), word(rt, 0x08C8C65Cu)});
    put(rt, registration.slot(0x30u), word(rt, 0x08C8C650u));
    put(rt, registration.slot(0x34u), word(rt, 0x08C8C654u));
    put(rt, registration.slot(0x28u), word(rt, 0x08C8C648u));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C8C64Cu));
    registration.call(0x08BF5E10u, {0x08CA4CB4u, registration.slot(0x30u), registration.slot(0x28u), word(rt, 0x08C8C654u), word(rt, 0x08C8C64Cu)});
    put(rt, registration.slot(0x20u), word(rt, 0x08C8C640u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8C644u));
    put(rt, registration.slot(0x18u), word(rt, 0x08C8C638u));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8C63Cu));
    registration.call(0x08BF5F8Cu, {0x08CA4C38u, registration.slot(0x20u), registration.slot(0x18u), word(rt, 0x08C8C644u), word(rt, 0x08C8C63Cu)});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8C630u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8C634u));
    put(rt, registration.slot(0x8u), word(rt, 0x08C8C628u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8C62Cu));
    registration.call(0x08BF5E10u, {0x08CA4CE0u, registration.slot(0x10u), registration.slot(0x8u), word(rt, 0x08C8C634u), word(rt, 0x08C8C62Cu)});
    registration.call(0x08B87108u, {0x08CA4E2Cu, 0x089A3F54u});
    registration.call(0x08B87108u, {0x08CA4E3Cu, 0x089A358Cu});
    registration.call(0x08B87108u, {0x08CA4E4Cu, 0x089A415Cu});
    registration.call(0x08B87108u, {0x08CA4E60u, 0x089A2B94u});
    registration.call(0x08B87108u, {0x08CA4E68u, 0x089A2B9Cu});
    registration.call(0x08B87108u, {0x08CA4E70u, 0x089A2BA4u});
    registration.call(0x08B87108u, {0x08CA4E7Cu, 0x089A3FECu});
    registration.call(0x08B87108u, {0x08CA4E88u, 0x089A4D6Cu});
    registration.call(0x08BE6154u, {0x08CA4E98u, 0u, 0x089A324Cu});
    registration.call(0x08B87108u, {0x08CA4EB0u, 0x089A53B0u});
    registration.call(0x08B87108u, {0x08CA4EC8u, 0x089A43E8u});
    registration.call(0x08B87108u, {0x08CA4ED8u, 0x089A44F0u});
}

// 0x08A6F010: constructor-time registration of a script object (a0 = the object) with its text 0x08CB07CC: 52 registration steps.
void register_08A6F010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x60u, 0x08CB07CCu, 0x08A76A94u, {{31u, 0x5Cu}, {18u, 0x58u}, {16u, 0x50u}, {17u, 0x54u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x08A6D378u});
    registration.call(0x08B8A004u, {0x08CF6F24u, 0x08A6DE64u});
    registration.call(0x08B878E8u, {0x08CB07D4u, 0x08A6BF78u, 0x08A6BF78u});
    registration.call(0x08B8D51Cu, {0x08CB07DCu, 0x08A6C620u, 0u});
    registration.call(0x08B87108u, {0x08CB07E4u, 0x08A6DF48u});
    registration.call(0x08B87108u, {0x08CB07E8u, 0x08A6F704u});
    registration.call(0x08B87108u, {0x08CB07F0u, 0x08A6E0DCu});
    registration.call(0x08B87108u, {0x08CB07F8u, 0x08A6CFC0u});
    registration.call(0x08B87108u, {0x08CB0800u, 0x08A6F9ECu});
    registration.call(0x08B87108u, {0x08CB0808u, 0x08A6FBD0u});
    registration.call(0x08B87108u, {0x08CB0814u, 0x08A6D10Cu});
    registration.call(0x08B87108u, {0x08CB081Cu, 0x08A6E3D0u});
    registration.call(0x08B87108u, {0x08CB0828u, 0x08A6E3D0u});
    registration.call(0x08B87108u, {0x08CB0834u, 0x08A6E5B0u});
    registration.call(0x08B87108u, {0x08CB083Cu, 0x08A6E5B0u});
    registration.call(0x08B87108u, {0x08CB0848u, 0x08A6C6B0u});
    registration.call(0x08B87108u, {0x08CB0850u, 0x08A6C7D0u});
    registration.call(0x08B87108u, {0x08CB0858u, 0x08A6C8F0u});
    registration.call(0x08B87108u, {0x08CB0860u, 0x08A6CA10u});
    registration.call(0x08B87108u, {0x08CB086Cu, 0x08A6CB30u});
    registration.call(0x08B87108u, {0x08CB0874u, 0x08A6CC50u});
    registration.call(0x08B87108u, {0x08CB087Cu, 0x08A6CD70u});
    registration.call(0x08B87108u, {0x08CB0884u, 0x08A6CE98u});
    registration.call(0x08B87108u, {0x08CB088Cu, 0x08A6E7B8u});
    registration.call(0x08B87108u, {0x08CB089Cu, 0x08A6E974u});
    registration.call(0x08B87108u, {0x08CB08A4u, 0x08A6EB58u});
    registration.call(0x08B87108u, {0x08CB08ACu, 0x08A6ED00u});
    registration.call(0x08B87108u, {0x08CB08B4u, 0x08A6EE88u});
    put(rt, registration.slot(0x40u), word(rt, 0x08C8EE40u));
    put(rt, registration.slot(0x44u), word(rt, 0x08C8EE44u));
    registration.call(0x08C249B0u, {0x08CF6FD0u, registration.slot(0x40u), word(rt, 0x08C8EE40u), word(rt, 0x08C8EE44u)});
    put(rt, registration.slot(0x38u), word(rt, 0x08C8EE38u));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C8EE3Cu));
    registration.call(0x08C249B0u, {0x08CF6F78u, registration.slot(0x38u), word(rt, 0x08C8EE38u), word(rt, 0x08C8EE3Cu)});
    put(rt, registration.slot(0x30u), word(rt, 0x08C8EE30u));
    put(rt, registration.slot(0x34u), word(rt, 0x08C8EE34u));
    registration.call(0x08C249B0u, {0x08CF6FC0u, registration.slot(0x30u), word(rt, 0x08C8EE30u), word(rt, 0x08C8EE34u)});
    put(rt, registration.slot(0x28u), word(rt, 0x08C8EE28u));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C8EE2Cu));
    registration.call(0x08C249B0u, {0x08CF6F9Cu, registration.slot(0x28u), word(rt, 0x08C8EE28u), word(rt, 0x08C8EE2Cu)});
    put(rt, registration.slot(0x20u), word(rt, 0x08C8EE20u));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8EE24u));
    registration.call(0x08C249B0u, {0x08CF6FB8u, registration.slot(0x20u), word(rt, 0x08C8EE20u), word(rt, 0x08C8EE24u)});
    put(rt, registration.slot(0x18u), word(rt, 0x08C8EE18u));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8EE1Cu));
    registration.call(0x08C249B0u, {0x08CF6FBCu, registration.slot(0x18u), word(rt, 0x08C8EE18u), word(rt, 0x08C8EE1Cu)});
    put(rt, registration.slot(0x10u), word(rt, 0x08C8EE10u));
    put(rt, registration.slot(0x14u), word(rt, 0x08C8EE14u));
    registration.call(0x08C249B0u, {0x08CF6FA8u, registration.slot(0x10u), word(rt, 0x08C8EE10u), word(rt, 0x08C8EE14u)});
    put(rt, registration.slot(0x8u), word(rt, 0x08C8EE08u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8EE0Cu));
    registration.call(0x08C249B0u, {0x08CF6FB0u, registration.slot(0x8u), word(rt, 0x08C8EE08u), word(rt, 0x08C8EE0Cu)});
}

// 0x089DB7D0: constructor-time registration of a script object (a0 = the object) with its text 0x08CA752C: 40 registration steps.
void register_089DB7D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ScriptRegistration registration(rt, ctx, 0x70u, 0x08CA752Cu, 0x08965B18u, {{31u, 0x6Cu}, {18u, 0x68u}, {16u, 0x60u}, {17u, 0x64u}});
    registration.call(0x08B8850Cu, {0x08CF6F14u, 0x089DE970u});
    put(rt, registration.slot(0x58u), word(rt, 0x08C8D3F4u));
    put(rt, registration.slot(0x5Cu), word(rt, 0x08C8D3F8u));
    put(rt, registration.slot(0x50u), word(rt, 0x08C8D3ECu));
    put(rt, registration.slot(0x54u), word(rt, 0x08C8D3F0u));
    registration.call(0x08C06ED8u, {0x08CA7440u, registration.slot(0x58u), registration.slot(0x50u), word(rt, 0x08C8D3F8u), word(rt, 0x08C8D3F0u)});
    registration.call(0x08B878E8u, {0x08CA7538u, 0x089DC10Cu, 0x089DC10Cu});
    registration.call(0x08B878E8u, {0x08CA7544u, 0x089DF050u, 0x089DF050u});
    registration.call(0x08B878E8u, {0x08CA754Cu, 0x089DE5C0u, 0x089DE5C0u});
    registration.call(0x08B878E8u, {0x08CA7558u, 0x089DD580u, 0x089DD580u});
    put(rt, registration.slot(0x48u), word(rt, 0x08C8D3E4u));
    put(rt, registration.slot(0x4Cu), word(rt, 0x08C8D3E8u));
    put(rt, registration.slot(0x40u), word(rt, 0x08C8D3DCu));
    put(rt, registration.slot(0x44u), word(rt, 0x08C8D3E0u));
    registration.call(0x08C06D5Cu, {0x08CA74B4u, registration.slot(0x48u), registration.slot(0x40u), word(rt, 0x08C8D3E8u), word(rt, 0x08C8D3E0u)});
    registration.call(0x08B878E8u, {0x08CA74BCu, 0x089DC2C0u, 0x089DC2C0u});
    registration.call(0x08B878E8u, {0x08CA7474u, 0x089DC564u, 0x089DC564u});
    put(rt, registration.slot(0x38u), word(rt, 0x08C8D3D4u));
    put(rt, registration.slot(0x3Cu), word(rt, 0x08C8D3D8u));
    put(rt, registration.slot(0x30u), word(rt, 0x08C8D3CCu));
    put(rt, registration.slot(0x34u), word(rt, 0x08C8D3D0u));
    registration.call(0x08C06D5Cu, {0x08CA7490u, registration.slot(0x38u), registration.slot(0x30u), word(rt, 0x08C8D3D8u), word(rt, 0x08C8D3D0u)});
    registration.call(0x08B878E8u, {0x08CA7480u, 0x089DC808u, 0x089DC808u});
    put(rt, registration.slot(0x28u), word(rt, 0x08C8D3C4u));
    put(rt, registration.slot(0x2Cu), word(rt, 0x08C8D3C8u));
    put(rt, registration.slot(0x20u), word(rt, 0x08C8D3BCu));
    put(rt, registration.slot(0x24u), word(rt, 0x08C8D3C0u));
    registration.call(0x08C06BE0u, {0x08CA749Cu, registration.slot(0x28u), registration.slot(0x20u), word(rt, 0x08C8D3C8u), word(rt, 0x08C8D3C0u)});
    put(rt, registration.slot(0x14u), word(rt, 0x08C8D3B0u));
    put(rt, registration.slot(0x18u), word(rt, 0x08C8D3B4u));
    put(rt, registration.slot(0x1Cu), word(rt, 0x08C8D3B8u));
    put(rt, registration.slot(0x10u), word(rt, 0x08C8D3ACu));
    registration.call(0x08C06BE0u, {0x08CA74A8u, registration.slot(0x18u), registration.slot(0x10u), word(rt, 0x08C8D3B8u), word(rt, 0x08C8D3B0u)});
    put(rt, registration.slot(0x8u), word(rt, 0x08C8D3A4u));
    put(rt, registration.slot(0xCu), word(rt, 0x08C8D3A8u));
    registration.call(0x08C06A78u, {0x08CA7560u, registration.slot(0x8u), word(rt, 0x08C8D3A4u), word(rt, 0x08C8D3A8u)});
    registration.call(0x08B87108u, {0x08CA7568u, 0x089DEFBCu});
    registration.call(0x08B87108u, {0x08CA7570u, 0x089DEF28u});
    registration.call(0x08B87108u, {0x08CA7578u, 0x089DD4ECu});
    registration.call(0x08B87108u, {0x08CA7580u, 0x089DEE70u});
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08847C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08847C90(rt, ctx); }
void func_0882506C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_0882506C(rt, ctx); }
void func_0882C2A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08C9A2F8u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A2A110u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[16] = 0x08830000u; G[17] = 0x08830000u; G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x0882D9B8u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08C9A37Cu; G[6] = 0x0882A638u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A388u; G[6] = 0x0882A5E0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A394u; G[6] = 0x0882D9F8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A3A0u; G[6] = 0x0882DB2Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A3ACu; G[6] = 0x0882DC08u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A3C4u; G[6] = 0x0882A6D0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A3DCu; G[6] = 0x0882DCE4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A3ECu; G[6] = 0x0882DDC0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A400u; G[6] = 0x0882DE9Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A40Cu; G[6] = 0x0882A7ACu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A418u; G[6] = 0x0882A588u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A430u; G[6] = 0x0882DF7Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A448u; G[6] = 0x0882E058u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A458u; G[6] = 0x0882E134u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A464u; G[6] = 0x0882E25Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A470u; G[6] = 0x0882E390u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A480u; G[6] = 0x0882E46Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A490u; G[6] = 0x0882A84Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A4B0u; G[6] = 0x0882A530u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A4C4u; G[6] = 0x0882A4D8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A4D8u; G[6] = 0x0882A480u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A4ECu; G[6] = 0x0882A428u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A500u; G[6] = 0x0882A3D0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A518u; G[6] = 0x0882A378u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A52Cu; G[6] = 0x0882A320u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A540u; G[6] = 0x0882A954u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A54Cu; G[6] = 0x0882AA38u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A560u; G[6] = 0x0882A2D8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A570u; G[6] = 0x0882A278u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A584u; G[6] = 0x08829358u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A59Cu; G[6] = 0x08829360u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A5B0u; G[6] = 0x08829368u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A5C8u; G[6] = 0x0882AB54u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A5DCu; G[6] = 0x0882E548u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A5F8u; G[6] = 0x0882E624u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A60Cu; G[6] = 0x0882E7D4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A624u; G[6] = 0x0882AC68u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A630u; G[6] = 0x0882E8B0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A648u; G[6] = 0x0882E98Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A658u; G[6] = 0x0882AD6Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A670u; G[6] = 0x0882AE18u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A684u; G[6] = 0x0882AEB8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A698u; G[6] = 0x0882EA2Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A6ACu; G[6] = 0x0882AF64u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A6C0u; G[6] = 0x0882EA74u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A6D0u; G[6] = 0x0882AFB4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A6E0u; G[6] = 0x0882B090u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A6F4u; G[6] = 0x0882B13Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A70Cu; G[6] = 0x0882EB50u; G[7] = 0x0882EB50u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9A718u; G[6] = 0x0882EC60u; G[7] = 0x0882EC60u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9A730u; G[6] = 0x0882ED78u; G[7] = 0x0882ED78u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9A73Cu; G[6] = 0x0882B240u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A75Cu; G[6] = 0x0882D66Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A770u; G[6] = 0x0882D580u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A788u; G[6] = 0x0882EE14u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A7A4u; G[6] = 0x0882EF30u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A7C0u; G[6] = 0x0882F04Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A7D4u; G[6] = 0x0882A220u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A7ECu; G[6] = 0x0882F1B8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A800u; G[6] = 0x0882F294u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A814u; G[6] = 0x0882F370u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A82Cu; G[6] = 0x0882B31Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A848u; G[6] = 0x0882A1C8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A860u; G[6] = 0x0882A170u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A870u; G[6] = 0x0882F404u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A884u; G[6] = 0x0882A118u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A890u; G[6] = 0x0882F498u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A8A0u; G[6] = 0x0882A0C0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A8ACu; G[6] = 0x0882A068u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A8B8u; G[6] = 0x0882F52Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A8C4u; G[6] = 0x0882F5C0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A8DCu; G[6] = 0x0882B3C0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A8FCu; G[6] = 0x0882B460u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A924u; G[6] = 0x0882B50Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A93Cu; G[6] = 0x0882A010u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A950u; G[6] = 0x0882F674u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A964u; G[6] = 0x0882F708u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A97Cu; G[6] = 0x0882F7A0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A98Cu; G[6] = 0x0882F97Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A9A0u; G[6] = 0x0882FA68u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A9B4u; G[6] = 0x0882FB48u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A9C8u; G[6] = 0x08829FB8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A9D8u; G[6] = 0x08829F60u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A9E8u; G[6] = 0x08829F08u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9A9FCu; G[6] = 0x0882FBDCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AA10u; G[6] = 0x0882B5ACu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AA28u; G[6] = 0x08829EB0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AA3Cu; G[6] = 0x08829E58u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AA48u; G[6] = 0x0882FCF4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AA54u; G[6] = 0x0882FD88u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AA60u; G[6] = 0x0882FE1Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AA7Cu; G[6] = 0x0882FEB0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AA90u; G[6] = 0x0882FF98u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AAA0u; G[6] = 0x08830100u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AAB0u; G[6] = 0x0882B698u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AAC4u; G[6] = 0x08830268u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AADCu; G[6] = 0x08830350u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AAF0u; G[6] = 0x0883046Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AB00u; G[6] = 0x08829DF8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AB10u; G[6] = 0x08830554u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AB28u; G[6] = 0x0883074Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AB40u; G[6] = 0x0882B744u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AB70u; G[6] = 0x0882B834u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AB94u; G[6] = 0x0882B924u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ABB8u; G[6] = 0x08830874u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ABD4u; G[6] = 0x0882B9C4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AC08u; G[6] = 0x08830998u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AC20u; G[6] = 0x0882BAD4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AC38u; G[6] = 0x08830A78u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AC4Cu; G[6] = 0x08830BA4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AC6Cu; G[6] = 0x08829320u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AC88u; G[6] = 0x08830BFCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AC9Cu; G[6] = 0x08830D6Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ACACu; G[6] = 0x08830EF0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ACC0u; G[6] = 0x08830FCCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ACD0u; G[6] = 0x0882BBBCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ACE4u; G[6] = 0x08829D98u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ACF8u; G[6] = 0x088312C0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AD0Cu; G[6] = 0x088313DCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AD20u; G[6] = 0x088314BCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AD38u; G[6] = 0x088315ACu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AD4Cu; G[6] = 0x08829CC4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AD60u; G[6] = 0x08829C4Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AD74u; G[6] = 0x08832BB4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AD88u; G[6] = 0x08829B94u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AD9Cu; G[6] = 0x08829B34u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ADB0u; G[6] = 0x08832C48u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ADC8u; G[6] = 0x08832E2Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ADDCu; G[6] = 0x08832FC4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9ADECu; G[6] = 0x088330DCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AE00u; G[6] = 0x088331F4u; G[7] = 0x088331F4u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9AE1Cu; G[6] = 0x0883332Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AE34u; G[6] = 0x088333C0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AE4Cu; G[6] = 0x08829A78u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AE64u; G[6] = 0x08833454u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AE88u; G[6] = 0x088334D8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AE98u; G[6] = 0x08833574u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AEACu; G[6] = 0x0883365Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AEBCu; G[6] = 0x08833754u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AED0u; G[6] = 0x088299A4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AEE8u; G[6] = 0x08833850u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AEFCu; G[6] = 0x08833954u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AF14u; G[6] = 0x088339F8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AF30u; G[6] = 0x0882994Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AF48u; G[6] = 0x08833AFCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AF68u; G[6] = 0x08833B90u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AF84u; G[6] = 0x088298E8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AF9Cu; G[6] = 0x08829868u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AFACu; G[6] = 0x088297E8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AFBCu; G[6] = 0x08833C24u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AFCCu; G[6] = 0x08833CACu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AFD8u; G[6] = 0x08833D44u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9AFE4u; G[6] = 0x08833E1Cu; G[7] = 0x08833E1Cu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9AFECu; G[6] = 0x08833EB8u; G[7] = 0x08833EB8u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9B000u; G[6] = 0x0882BC68u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B020u; G[6] = 0x0882BD10u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B040u; G[6] = 0x0882BDB8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B064u; G[6] = 0x08833FCCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B078u; G[6] = 0x0882BE60u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B08Cu; G[6] = 0x08833FCCu; G[7] = 0x0882BE60u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9B09Cu; G[6] = 0x08834068u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B0B0u; G[6] = 0x0882BF00u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B0C4u; G[6] = 0x08834068u; G[7] = 0x0882BF00u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9B0D0u; G[6] = 0x08834104u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B0E8u; G[6] = 0x0882BFA0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B100u; G[6] = 0x08834104u; G[7] = 0x0882BFA0u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9B110u; G[6] = 0x088341ECu; G[7] = 0x088341ECu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9B128u; G[6] = 0x088342F4u; G[7] = 0x088342F4u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9B138u; G[6] = 0x088343FCu; G[7] = 0x088343FCu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C9B144u; G[6] = 0x088344F4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B15Cu; G[6] = 0x0882C07Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B170u; G[6] = 0x088345D4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9B180u; G[6] = 0x0882D3E4u; call_here(rt, ctx, 0x08B87108u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08A388AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08A388AC(rt, ctx); }
void func_08A00010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08A00010(rt, ctx); }
void func_0880C3A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_0880C3A8(rt, ctx); }
void func_089B837C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_089B837C(rt, ctx); }
void func_08A6A94C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08A6A94C(rt, ctx); }
void func_089C2D2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_089C2D2C(rt, ctx); }
void func_0899E370(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_0899E370(rt, ctx); }
void func_089EF278(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_089EF278(rt, ctx); }
void func_08A08F7C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08A08F7C(rt, ctx); }
void func_08A02D70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08A02D70(rt, ctx); }
void func_08AB6654(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08AB6654(rt, ctx); }
void func_08AB5B2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08AB5B2C(rt, ctx); }
void func_089BE4BC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_089BE4BC(rt, ctx); }
void func_08A9B404(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08A9B404(rt, ctx); }
void func_089A76A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x80u); put(rt, frame.at(0x7Cu), G[31]); put(rt, frame.at(0x78u), G[18]); put(rt, frame.at(0x70u), G[16]); put(rt, frame.at(0x74u), G[17]); G[5] = 0x08CA4F8Cu; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[5] = 0x08CF0000u; G[6] = 0x089A0000u; put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x089A79F4u; call_here(rt, ctx, 0x08B8850Cu); G[5] = 0x08CF0000u; G[6] = 0x089A0000u; G[4] = G[18]; G[5] = 0x08CF6F24u; G[6] = 0x089A6E24u; call_here(rt, ctx, 0x08B8A004u); G[5] = 0x08CA0000u; G[6] = 0x089A0000u; G[4] = G[18]; G[5] = 0x08CA4F9Cu; G[6] = 0x089A7C00u; call_here(rt, ctx, 0x08B87108u); G[5] = 0x08CA0000u; G[6] = 0x089B0000u; G[4] = G[18]; G[5] = 0x08CA4FACu; G[6] = 0x089A8144u; call_here(rt, ctx, 0x08B87108u); G[5] = 0x08CA0000u; G[6] = 0x089A0000u; G[4] = G[18]; G[5] = 0x08CA4FF0u; G[6] = 0x089A7EFCu; call_here(rt, ctx, 0x08B87108u); G[5] = 0x08CA0000u; G[6] = 0x089A0000u; G[4] = G[18]; G[5] = 0x08CA4FF8u; G[6] = 0x089A7E7Cu; call_here(rt, ctx, 0x08B87108u); G[5] = 0x08CA0000u; G[6] = 0x089A0000u; G[4] = G[18]; G[5] = 0x08CA5000u; G[6] = 0x089A6964u; call_here(rt, ctx, 0x08B87108u); G[4] = 0x08C90000u; G[5] = 0x08C90000u; G[2] = 0x08C8C744u; G[3] = 0x08C8C73Cu; G[8] = word(rt, G[2] + 0x4u); G[9] = word(rt, G[3] + 0x4u); G[2] = word(rt, 0x08C8C744u); G[3] = word(rt, 0x08C8C73Cu); G[5] = 0x08CA0000u; G[4] = G[18]; G[6] = frame.at(0x68u); G[7] = frame.at(0x60u); G[5] = 0x08CA5008u; put(rt, frame.at(0x68u), G[2]); put(rt, frame.at(0x6Cu), G[8]); put(rt, frame.at(0x60u), G[3]); put(rt, frame.at(0x64u), G[9]); call_here(rt, ctx, 0x08BF81FCu); G[4] = 0x08C90000u; G[5] = 0x08C90000u; G[2] = 0x08C8C734u; G[3] = 0x08C8C72Cu; G[8] = word(rt, G[2] + 0x4u); G[9] = word(rt, G[3] + 0x4u); G[2] = word(rt, 0x08C8C734u); G[3] = word(rt, 0x08C8C72Cu); G[5] = 0x08CA0000u; G[4] = G[18]; G[6] = frame.at(0x58u); G[7] = frame.at(0x50u); G[5] = 0x08CA5014u; put(rt, frame.at(0x58u), G[2]); put(rt, frame.at(0x5Cu), G[8]); put(rt, frame.at(0x50u), G[3]); put(rt, frame.at(0x54u), G[9]); call_here(rt, ctx, 0x08BF81FCu); G[4] = 0x08C90000u; G[5] = 0x08C90000u; G[2] = 0x08C8C724u; G[3] = 0x08C8C71Cu; G[8] = word(rt, G[2] + 0x4u); G[9] = word(rt, G[3] + 0x4u); G[2] = word(rt, 0x08C8C724u); G[3] = word(rt, 0x08C8C71Cu); G[5] = 0x08CA0000u; G[4] = G[18]; G[6] = frame.at(0x48u); G[7] = frame.at(0x40u); G[5] = 0x08CA501Cu; put(rt, frame.at(0x48u), G[2]); put(rt, frame.at(0x4Cu), G[8]); put(rt, frame.at(0x40u), G[3]); put(rt, frame.at(0x44u), G[9]); call_here(rt, ctx, 0x08BF8080u); G[4] = 0x08C90000u; G[5] = 0x08C90000u; G[2] = 0x08C8C714u; G[3] = 0x08C8C70Cu; G[8] = word(rt, G[2] + 0x4u); G[9] = word(rt, G[3] + 0x4u); G[2] = word(rt, 0x08C8C714u); G[3] = word(rt, 0x08C8C70Cu); G[5] = 0x08CA0000u; G[4] = G[18]; G[6] = frame.at(0x38u); G[7] = frame.at(0x30u); G[5] = 0x08CA5030u; put(rt, frame.at(0x38u), G[2]); put(rt, frame.at(0x3Cu), G[8]); put(rt, frame.at(0x30u), G[3]); put(rt, frame.at(0x34u), G[9]); call_here(rt, ctx, 0x08BF8080u); G[4] = 0x08C90000u; G[5] = 0x08C90000u; G[2] = 0x08C8C704u; G[3] = 0x08C8C6FCu; G[8] = word(rt, G[2] + 0x4u); G[9] = word(rt, G[3] + 0x4u); G[2] = word(rt, 0x08C8C704u); G[3] = word(rt, 0x08C8C6FCu); G[5] = 0x08CA0000u; G[4] = G[18]; G[6] = frame.at(0x28u); G[7] = frame.at(0x20u); G[5] = 0x08CA5040u; put(rt, frame.at(0x28u), G[2]); put(rt, frame.at(0x2Cu), G[8]); put(rt, frame.at(0x20u), G[3]); put(rt, frame.at(0x24u), G[9]); call_here(rt, ctx, 0x08BF8080u); G[4] = 0x08C90000u; G[5] = 0x08C90000u; G[2] = 0x08C8C6F4u; G[3] = 0x08C8C6ECu; G[8] = word(rt, G[2] + 0x4u); G[9] = word(rt, G[3] + 0x4u); G[2] = word(rt, 0x08C8C6F4u); G[3] = word(rt, 0x08C8C6ECu); G[5] = 0x08CA0000u; G[4] = G[18]; G[6] = frame.at(0x18u); G[7] = frame.at(0x10u); G[5] = 0x08CA5054u; put(rt, frame.at(0x18u), G[2]); put(rt, frame.at(0x1Cu), G[8]); put(rt, frame.at(0x10u), G[3]); put(rt, frame.at(0x14u), G[9]); call_here(rt, ctx, 0x08BF8080u); G[3] = 0x08C90000u; G[2] = 0x08C8C6E4u; G[9] = word(rt, G[2] + 0x4u); G[8] = word(rt, 0x08C8C6E4u); G[5] = 0x08CA0000u; G[4] = G[18]; G[6] = frame.at(0x8u); G[5] = 0x08CA5064u; G[7] = 0u; put(rt, frame.at(0x8u), G[8]); put(rt, frame.at(0xCu), G[9]); call_here(rt, ctx, 0x08BF7F14u);  G[18] = word(rt, frame.at(0x78u)); G[16] = word(rt, frame.at(0x70u)); G[17] = word(rt, frame.at(0x74u)); }  } }
void func_08A04860(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08A04860(rt, ctx); }
void func_089DB7D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_089DB7D0(rt, ctx); }
void func_089A4648(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_089A4648(rt, ctx); }
void func_08A6F010(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_08A6F010(rt, ctx); }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_313
} // namespace gt::game

#define HOT313(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_313::name, gt::ReturnKind::kind)
HOT313(0x08847C90u, func_08847C90, Word);
HOT313(0x0882506Cu, func_0882506C, Word);
// HOT313(0x0882C2A8u, func_0882C2A8, Word);  // no comparable synthetic case yet
HOT313(0x08A388ACu, func_08A388AC, Word);
HOT313(0x08A00010u, func_08A00010, Word);
HOT313(0x0880C3A8u, func_0880C3A8, Word);
HOT313(0x089B837Cu, func_089B837C, Word);
HOT313(0x08A6A94Cu, func_08A6A94C, Word);
HOT313(0x089C2D2Cu, func_089C2D2C, Word);
HOT313(0x0899E370u, func_0899E370, Word);
HOT313(0x089EF278u, func_089EF278, Word);
HOT313(0x08A08F7Cu, func_08A08F7C, Word);
HOT313(0x08A02D70u, func_08A02D70, Word);
HOT313(0x08AB6654u, func_08AB6654, Word);
HOT313(0x08AB5B2Cu, func_08AB5B2C, Word);
HOT313(0x089BE4BCu, func_089BE4BC, Word);
HOT313(0x08A9B404u, func_08A9B404, Word);
HOT313(0x089A76A4u, func_089A76A4, Word);
HOT313(0x08A04860u, func_08A04860, Word);
HOT313(0x089DB7D0u, func_089DB7D0, Word);
HOT313(0x089A4648u, func_089A4648, Word);
HOT313(0x08A6F010u, func_08A6F010, Word);
