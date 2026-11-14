// Hot batch 312: script-binding registrations and a few other functions no route reaches, written by hand from
// `tools.disasm` and verified by synthetic cases (binding tables that register the methods of a script class, an
// index accessor, a line reader and a flag-array builder).
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <utility>

namespace gt::game {
namespace hot_batch_312 {
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

// The callee-saved registers (s0-s7, f20-f31): a function that sets them leaves the caller's values back on return.
class SavedRegisters {
public:
    explicit SavedRegisters(psprecomp::AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0; i < 8; ++i) gpr_[i] = ctx.gpr[16 + i];
    }
    ~SavedRegisters() {
        for (unsigned i = 0; i < 8; ++i) ctx_.gpr[16 + i] = gpr_[i];
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    psprecomp::AllegrexContext &ctx_;
    u32 gpr_[8];
};

// The `std::string` representation every empty string shares.
constexpr u32 kEmptyStreamText = 0x08D74FD0u;

// 0x08A5BB7C: the null-handle report every checked handle load calls with its message in a0.
void report_null(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 message) { call_set(rt, ctx, 0x08A5BB7Cu, {message}); }

// Drops one reference of the string whose characters start at `data` (representation at data - 0xC, count at data - 4): the shared
// empty representation is left alone, otherwise 0x08B60804 decrements the count and 0x08B8542C frees the representation at zero or
// below. v0 / a0 / a1 are left as the original leaves them.
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

// One registration call of a script object, `target(object, name, ...)`: with a callback, optionally with a third argument (the same callback, zero or
// a value), after setting s0 or, for a record, with a copy of the member-pointer record `callback` (two words) stored in the frame at `extra`.
enum class StepKind { Callback, SameThird, ZeroThird, Third, SetS0, Record };

struct Step {
    u32 target;
    StepKind kind;
    u32 name;
    u32 callback;
    u32 extra;
};

// Constructor-time registration of a script object (a0 = the object) in a frame of `frame_bytes`: builds the string `text` into the slot at sp + 4
// (0x08B85EB8), has the object's virtual +0x108 take it, drops it, asks 0x08A76A94 for a handle (released 0x08A5BA08 when set), replaces the handle
// kept at +0x38 (old one 0x08A5B930) and then runs the registration `steps`. v0 = the last call's.
void register_script_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, u32 frame_bytes, u32 text, const Step *steps, std::size_t count) {
    const u32 self = ctx.gpr[4];
    GuestFrame frame(ctx, frame_bytes);
    SavedRegisters saved(ctx);
    const u32 slot = frame.at(4u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = slot;
    ctx.gpr[2] = word(rt, self);
    ctx.gpr[17] = word(rt, ctx.gpr[2] + 0x108u);
    call_set(rt, ctx, 0x08B85EB8u, {slot, text, frame.at(1u)});
    call_set(rt, ctx, ctx.gpr[17], {self, slot});
    drop_string_reference(rt, ctx, frame, word(rt, slot));
    const u32 handle = call_set(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = handle;
    if (handle != 0u) call_set(rt, ctx, 0x08A5BA08u, {handle});
    const u32 previous = word(rt, self + 0x38u);
    ctx.gpr[2] = previous;
    ctx.gpr[4] = previous;
    if (previous != 0u) call_set(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, handle);
    for (std::size_t i = 0; i < count; ++i) {
        const Step &step = steps[i];
        switch (step.kind) {
        case StepKind::Callback: call_set(rt, ctx, step.target, {self, step.name, step.callback}); break;
        case StepKind::SameThird: call_set(rt, ctx, step.target, {self, step.name, step.callback, step.callback}); break;
        case StepKind::ZeroThird: call_set(rt, ctx, step.target, {self, step.name, step.callback, 0u}); break;
        case StepKind::Third: call_set(rt, ctx, step.target, {self, step.name, step.callback, step.extra}); break;
        case StepKind::SetS0:
            ctx.gpr[16] = step.extra;
            call_set(rt, ctx, step.target, {self, step.name, step.callback});
            break;
        case StepKind::Record: {
            const u32 first = word(rt, step.callback);
            const u32 second = word(rt, step.callback + 4u);
            const u32 record_slot = frame.at(step.extra);
            ctx.gpr[3] = 0x08C90000u;
            ctx.gpr[2] = step.callback;
            put(rt, record_slot, first);
            put(rt, record_slot + 4u, second);
            call_set(rt, ctx, step.target, {self, step.name, record_slot, first, second});
            break;
        }
        }
    }
}

constexpr Step kSteps_08A45188[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x08A4509Cu, 0u},
    {0x08B8A004u, StepKind::Callback, 0x08CF6F24u, 0x08A449F8u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08CADE5Cu, 0x08A44ADCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADE64u, 0x08A44B64u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADE6Cu, 0x08A45330u, 0u},
    {0x08C1C854u, StepKind::Record, 0x08CF6FC0u, 0x08C8E86Cu, 0x10u},
    {0x08C1C854u, StepKind::Record, 0x08CF6F9Cu, 0x08C8E864u, 0x8u},
};
constexpr Step kSteps_08837BEC[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x08837A2Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BAC0u, 0x08838990u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BAC8u, 0x088387DCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BAD4u, 0x088381F8u, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C9BAE0u, 0x08837A6Cu, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C9BAF0u, 0x088375E0u, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C9BB00u, 0x08837AECu, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C9BB0Cu, 0x0883768Cu, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C9BB18u, 0x08837B6Cu, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C9BB28u, 0x08837738u, 0u},
};
constexpr Step kSteps_08A3D1D0[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x08A3D0A4u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD760u, 0x08A3D018u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD768u, 0x08A3CFCCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD770u, 0x08A3CF80u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD778u, 0x08A3CF34u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD780u, 0x08A3CEE8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD788u, 0x08A3CE9Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD790u, 0x08A3CE50u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD798u, 0x08A3CE04u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD7A0u, 0x08A3CDB8u, 0u},
};
constexpr Step kSteps_08A58118[] = {
    {0x08B87108u, StepKind::Callback, 0x08CAF810u, 0x08A57BD4u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAF81Cu, 0x08A57930u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAF828u, 0x08A57CB0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAF834u, 0x08A57894u, 0u},
    {0x08B878E8u, StepKind::SameThird, 0x08CAF840u, 0x08A57D74u, 0u},
    {0x08B878E8u, StepKind::SameThird, 0x08CAF848u, 0x08A57D74u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08CAF850u, 0x08A57E50u, 0u},
    {0x08C1E370u, StepKind::Record, 0x08CAF858u, 0x08C8EA10u, 0x8u},
};
constexpr Step kSteps_0884CDB8[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x0884CB58u, 0u},
    {0x08B8A004u, StepKind::Callback, 0x08CF6F24u, 0x0884D1C8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D0CCu, 0x0884D1E0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D0D8u, 0x0884CF78u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D0E4u, 0x0884CF78u, 0u},
    {0x08B878E8u, StepKind::SameThird, 0x08C9D0F0u, 0x0884CF78u, 0u},
    {0x08B878E8u, StepKind::SameThird, 0x08C9D0F8u, 0x0884C9D0u, 0u},
    {0x08B878E8u, StepKind::SameThird, 0x08C9D100u, 0x0884D1E0u, 0u},
    {0x08B878E8u, StepKind::SameThird, 0x08C9D108u, 0x0884C8E8u, 0u},
};
constexpr Step kSteps_08818A04[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x0881A6C4u, 0u},
    {0x08B8A004u, StepKind::Callback, 0x08CF6F24u, 0x08818064u, 0u},
    {0x08B878E8u, StepKind::Third, 0x08C98DD4u, 0x0881A208u, 0x0881A208u},
    {0x08B87108u, StepKind::Callback, 0x08C98DDCu, 0x08818560u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C98DE8u, 0x088193E8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C98DF4u, 0x088185F4u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C98E04u, 0x08819170u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C98E14u, 0x0881829Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C98E28u, 0x08818F28u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C98E3Cu, 0x0881800Cu, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C98E44u, 0x08818680u, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C98E50u, 0x08818C38u, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C98E5Cu, 0x088187ACu, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C98E6Cu, 0x0881888Cu, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08C98E7Cu, 0x08818948u, 0u},
};
constexpr Step kSteps_08839D78[] = {
    {0x08B8850Cu, StepKind::SetS0, 0x08CF6F14u, 0x0883A344u, 0x08840000u},
    {0x08B8A004u, StepKind::Callback, 0x08CF6F24u, 0x08839FDCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BC3Cu, 0x08839FDCu, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9BC48u, 0x0883916Cu, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9BC50u, 0x08839204u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9BC5Cu, 0x08839298u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9BC6Cu, 0x0883932Cu, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9BC74u, 0x08839440u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9BC7Cu, 0x088394D4u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9BC8Cu, 0x08839568u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9BC98u, 0x088395FCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BCA8u, 0x08839690u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BCB4u, 0x0883976Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BCC8u, 0x088398D4u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BCDCu, 0x08839968u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9BCF0u, 0x08839A44u, 0u},
};
constexpr Step kSteps_08A33D28[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x08C1B03Cu, 0u},
    {0x08B8A004u, StepKind::Callback, 0x08CF6F24u, 0x08A33140u, 0u},
    {0x08B878E8u, StepKind::Third, 0x08CACD5Cu, 0x08C1B288u, 0x08C1B008u},
    {0x08B878E8u, StepKind::Third, 0x08CACD6Cu, 0x08C1B254u, 0x08C1AFD4u},
    {0x08B878E8u, StepKind::Third, 0x08CACD74u, 0x08C1B220u, 0x08C1AFA0u},
    {0x08B878E8u, StepKind::Third, 0x08CACD7Cu, 0x08C1B1ECu, 0x08C1AF6Cu},
    {0x08B878E8u, StepKind::Third, 0x08CACD80u, 0x08C1B1B8u, 0x08C1AF38u},
    {0x08B878E8u, StepKind::Third, 0x08CACD88u, 0x08C1B184u, 0x08C1AF04u},
    {0x08B878E8u, StepKind::Third, 0x08CACD90u, 0x08C1B150u, 0x08C1AED0u},
    {0x08B87108u, StepKind::Callback, 0x08CACD98u, 0x08A33FACu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACDA0u, 0x08A3397Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACDA8u, 0x08A33AA8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACDB0u, 0x08A33BD4u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACDBCu, 0x08A33644u, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08CACDC8u, 0x08A33C70u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08CACDD8u, 0x08C1B11Cu, 0u},
};
constexpr Step kSteps_08A42028[] = {
    {0x08B8850Cu, StepKind::SetS0, 0x08CF6F14u, 0x08A43248u, 0x08A40000u},
    {0x08B87108u, StepKind::Callback, 0x08CADC74u, 0x08A41BA8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADC84u, 0x08A41CB0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADC94u, 0x08A41B28u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADCA4u, 0x08A41E4Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADCB4u, 0x08A43724u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADCBCu, 0x08A43398u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADCCCu, 0x08A42E10u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CADCD4u, 0x08A42E10u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08CADCE0u, 0x08A417A4u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08CADCE8u, 0x08A4163Cu, 0u},
    {0x08B878E8u, StepKind::Third, 0x08CADCF0u, 0x08A41F4Cu, 0x08A41824u},
};
constexpr Step kSteps_08A35C30[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x08A36CECu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACF68u, 0x08A3676Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACF70u, 0x08A355FCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACF78u, 0x08A36608u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACF80u, 0x08A3650Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACF8Cu, 0x08A36438u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACF98u, 0x08A36364u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACFA4u, 0x08A35648u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACFB0u, 0x08A35B8Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACFC0u, 0x08A36240u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACFCCu, 0x08A36104u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACFD8u, 0x08A368A4u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACFE8u, 0x08A35694u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CACFF4u, 0x08A356E0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD000u, 0x08A358F0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAD010u, 0x08A35EECu, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08CAD024u, 0x08A35978u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08CAD02Cu, 0x08A359DCu, 0u},
    {0x08B878E8u, StepKind::Third, 0x08CAD034u, 0x08A35A40u, 0x08A35774u},
    {0x08B878E8u, StepKind::Third, 0x08CAD03Cu, 0x08A35AA4u, 0x08A35AA4u},
};
constexpr Step kSteps_08A24038[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x08A25258u, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08CAAD4Cu, 0x08A243FCu, 0u},
    {0x08B86FB0u, StepKind::Callback, 0x08CAAD58u, 0x08A242F8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAAD68u, 0x08A23E10u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAAD74u, 0x08A2505Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAAD7Cu, 0x08A23DC4u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAAD84u, 0x08A23D78u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAAD8Cu, 0x08A24B0Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CAADA0u, 0x08A24A54u, 0u},
    {0x08B878E8u, StepKind::Third, 0x08CAADB4u, 0x08C19B74u, 0x08C197F8u},
    {0x08B878E8u, StepKind::Third, 0x08CAADC8u, 0x08C19B40u, 0x08C197C4u},
    {0x08B878E8u, StepKind::Third, 0x08CAADDCu, 0x08C19A24u, 0x08C196C0u},
    {0x08B878E8u, StepKind::Third, 0x08CAADE8u, 0x08C199F0u, 0x08C1968Cu},
    {0x08B878E8u, StepKind::Third, 0x08CAADF8u, 0x08C199BCu, 0x08C19658u},
    {0x08B878E8u, StepKind::Third, 0x08CAAE08u, 0x08C19988u, 0x08C19624u},
    {0x08B878E8u, StepKind::Third, 0x08CAAE14u, 0x08C19954u, 0x08C195F0u},
    {0x08B878E8u, StepKind::Third, 0x08CAAE20u, 0x08C19920u, 0x08C195BCu},
    {0x08B878E8u, StepKind::Third, 0x08CAAE30u, 0x08C198ECu, 0x08C19588u},
};
constexpr Step kSteps_089C7710[] = {
    {0x08B8850Cu, StepKind::SetS0, 0x08CF6F14u, 0x089CA680u, 0x089D0000u},
    {0x08B87108u, StepKind::Callback, 0x08CA64A8u, 0x089C7EC0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA64B8u, 0x089C80DCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA64CCu, 0x089C80DCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA64E0u, 0x089C40E4u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA64F4u, 0x089CA474u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA6500u, 0x089CA474u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA6510u, 0x089CA118u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA651Cu, 0x089C5D80u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA652Cu, 0x089C7314u, 0u},
    {0x08B878E8u, StepKind::Third, 0x08CA653Cu, 0x089C6CB0u, 0x089C6CB0u},
    {0x08B878E8u, StepKind::Third, 0x08CA6548u, 0x089C6DA4u, 0x089C6DA4u},
    {0x08B878E8u, StepKind::Third, 0x08CA6554u, 0x089C6E98u, 0x089C6E98u},
    {0x08B878E8u, StepKind::Third, 0x08CA6560u, 0x089C6F94u, 0x089C6F94u},
    {0x08B87108u, StepKind::Callback, 0x08CA6570u, 0x089C7090u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA6584u, 0x089C5CB8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA6594u, 0x089C8EE0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA659Cu, 0x089C7418u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA65ACu, 0x089C7508u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA65BCu, 0x089C7610u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA65CCu, 0x089C9238u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08CA65D8u, 0x089C40ECu, 0u},
};
constexpr Step kSteps_088429A0[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x08843330u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C5A8u, 0x08841AD0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C5B4u, 0x08841BD4u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C5C4u, 0x08841C70u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C5D0u, 0x08841D04u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C5DCu, 0x08841D9Cu, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C5ECu, 0x0884308Cu, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C5FCu, 0x08841E34u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C604u, 0x08841ECCu, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C618u, 0x08841F80u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C624u, 0x08842018u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C634u, 0x088420B0u, 0u},
    {0x08B8D51Cu, StepKind::ZeroThird, 0x08C9C644u, 0x08842148u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C658u, 0x088421FCu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C66Cu, 0x08842290u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C680u, 0x08842324u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C694u, 0x088423B8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C6A4u, 0x088424D8u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C6B8u, 0x08842604u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C6C8u, 0x08842704u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C6D4u, 0x08842804u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C6E4u, 0x08842908u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9C6F0u, 0x08842CB8u, 0u},
};
constexpr Step kSteps_0884E9D4[] = {
    {0x08B8850Cu, StepKind::Callback, 0x08CF6F14u, 0x0884F47Cu, 0u},
    {0x08B8A004u, StepKind::Callback, 0x08CF6F24u, 0x0884DB04u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D1D8u, 0x0884DA48u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D1E4u, 0x0884D9F0u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D1F0u, 0x0884D950u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D1FCu, 0x0884DD14u, 0u},
    {0x08B878E8u, StepKind::Third, 0x08C9D210u, 0x0884DDA8u, 0x0884DDA8u},
    {0x08B878E8u, StepKind::Third, 0x08C9D218u, 0x0884DEA0u, 0x0884DEA0u},
    {0x08B878E8u, StepKind::Third, 0x08C9D224u, 0x0884F138u, 0x0884F138u},
    {0x08B878E8u, StepKind::Third, 0x08C9D234u, 0x0884EFACu, 0x0884EFACu},
    {0x08B878E8u, StepKind::Third, 0x08C9D23Cu, 0x0884DF90u, 0x0884DF90u},
    {0x08B878E8u, StepKind::Third, 0x08C9D244u, 0x0884E088u, 0x0884E088u},
    {0x08B878E8u, StepKind::Third, 0x08C9D250u, 0x0884E180u, 0x0884E180u},
    {0x08B878E8u, StepKind::Third, 0x08C9D25Cu, 0x0884E278u, 0x0884E278u},
    {0x08B878E8u, StepKind::Third, 0x08C9D268u, 0x0884E370u, 0x0884E370u},
    {0x08B87108u, StepKind::Callback, 0x08C9D27Cu, 0x0884E468u, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D290u, 0x0884D8B0u, 0u},
    {0x08B878E8u, StepKind::Third, 0x08C9D2A4u, 0x0884E4FCu, 0x0884E4FCu},
    {0x08B878E8u, StepKind::Third, 0x08C9D2B0u, 0x0884E5F4u, 0x0884E5F4u},
    {0x08B878E8u, StepKind::Third, 0x08C9D2C8u, 0x0884E6ECu, 0x0884E6ECu},
    {0x08B878E8u, StepKind::Third, 0x08C9D2D8u, 0x0884E7E4u, 0x0884E7E4u},
    {0x08B87108u, StepKind::Callback, 0x08C9D2E8u, 0x0884ED44u, 0u},
    {0x08B878E8u, StepKind::Third, 0x08C9D2FCu, 0x0884E8DCu, 0x0884E8DCu},
    {0x08B87108u, StepKind::Callback, 0x08C9D31Cu, 0x0884D81Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D324u, 0x0884D77Cu, 0u},
    {0x08B87108u, StepKind::Callback, 0x08C9D330u, 0x0884D6A0u, 0u},
};

// 0x08B78180: reads a line (up to a1 - 1 characters, newline included) into the buffer a0 from the chunk reader a2 (pointer at +0, bytes
// left at +4; 0x08B7B0CC refills when it is empty, 0x08B79998 finds the newline, 0x08B79A88 copies) and NUL-terminates it; returns the
// buffer, or 0 when nothing could be read (or the size is below 2).
void read_line(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 buffer = ctx.gpr[4];
    const u32 size = ctx.gpr[5];
    const u32 reader = ctx.gpr[6];
    GuestFrame frame(ctx, 0x20u);
    SavedRegisters saved(ctx);
    ctx.gpr[2] = static_cast<s32>(size) < 2 ? 1u : 0u;
    u32 result = buffer;
    ctx.gpr[17] = reader;
    if (static_cast<s32>(size) < 2) {
        result = 0u;
    } else {
        u32 room = size - 1u;
        u32 cursor = buffer;
        for (;;) {
            u32 available = word(rt, reader + 4u);
            ctx.gpr[4] = reader;
            if (available == 0u) {
                if (call_set(rt, ctx, 0x08B7B0CCu, {reader}) != 0u) {
                    if (result == cursor) result = 0u;
                    else rt.memory().store8(cursor, 0u);
                    break;
                }
                available = word(rt, reader + 4u);
            }
            const u32 data = word(rt, reader);
            ctx.gpr[2] = room < available ? 1u : 0u;
            if (room < available) available = room;
            const u32 found = call_set(rt, ctx, 0x08B79998u, {data, 10u, available});
            if (found != 0u) {
                const u32 length = found + 1u - data;
                ctx.gpr[2] = word(rt, reader + 4u) - length;
                put(rt, reader, found + 1u);
                put(rt, reader + 4u, ctx.gpr[2]);
                call_set(rt, ctx, 0x08B79A88u, {cursor, data, length});
                rt.memory().store8(cursor + length, 0u);
                break;
            }
            const u32 remaining = word(rt, reader + 4u);
            const u32 position = word(rt, reader);
            const u32 target = cursor;
            cursor += available;
            ctx.gpr[2] = remaining - available;
            put(rt, reader, position + available);
            put(rt, reader + 4u, remaining - available);
            call_set(rt, ctx, 0x08B79A88u, {target, data, available});
            room -= available;
            if (room == 0u) {
                rt.memory().store8(cursor, 0u);
                break;
            }
        }
    }
    ctx.gpr[2] = result;
}

// 0x089B7E40: for a count a2 >= 2 builds the object behind the handle slots (0x089AB370 from the holder a3, 0x08A98674 from its next element,
// null message 0x08CA59EC), allocates one byte per element of its list (the list at +0x14 .. +0x18, 4-byte items; 0x08B35DA0) and fills each
// with the truth value of the holder-object's virtual +0x3C (item i fetched through virtual +0x84, null message 0x08CA5908), then hands the
// object, the second slot and the byte block to 0x089B7CF8 (null message 0x08CA5920) and releases and frees everything.
void collect_flags(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 argument = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(ctx);
    ctx.gpr[6] = static_cast<s32>(count) < 2 ? 1u : 0u;
    ctx.gpr[23] = argument;
    ctx.gpr[16] = holder;
    if (static_cast<s32>(count) < 2) return;
    const u32 first_slot = frame.at(4u);
    const u32 second_slot = frame.at(8u);
    const u32 scratch_slot = frame.at(0xCu);
    ctx.gpr[30] = second_slot;
    call_set(rt, ctx, 0x089AB370u, {first_slot, holder});
    call_set(rt, ctx, 0x08A98674u, {second_slot, holder + 4u});
    u32 object = word(rt, second_slot);
    ctx.gpr[16] = object;
    if (object == 0u) report_null(rt, ctx, 0x08CA59ECu);
    const u32 items = static_cast<u32>(static_cast<s32>(word(rt, object + 0x18u) - word(rt, object + 0x14u)) >> 2);
    ctx.gpr[18] = items;
    const u32 flags = call_set(rt, ctx, 0x08B35DA0u, {items});
    ctx.gpr[19] = flags;
    if (static_cast<s32>(items) > 0) {
        ctx.gpr[20] = scratch_slot;
        ctx.gpr[22] = 0x08CA0000u;
        ctx.gpr[21] = 0x08CA0000u;
        object = word(rt, second_slot);
        ctx.gpr[16] = object;
        u32 index = 0u;
        ctx.gpr[17] = 0u;
        do {
            ctx.gpr[4] = 0x08CA59ECu;
            if (object == 0u) report_null(rt, ctx, 0x08CA59ECu);
            ctx.gpr[3] = word(rt, object);
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x84u);
            call_set(rt, ctx, ctx.gpr[2], {scratch_slot, object, index});
            object = word(rt, scratch_slot);
            ctx.gpr[16] = object;
            ctx.gpr[4] = 0x08CA5908u;
            if (object == 0u) report_null(rt, ctx, 0x08CA5908u);
            ctx.gpr[3] = word(rt, object);
            ctx.gpr[2] = word(rt, ctx.gpr[3] + 0x3Cu);
            const u32 truth = call_set(rt, ctx, ctx.gpr[2], {object}) != 0u ? 1u : 0u;
            ctx.gpr[2] = truth;
            ctx.gpr[3] = flags + index;
            rt.memory().store8(flags + index, static_cast<std::uint8_t>(truth));
            ++index;
            ctx.gpr[17] = index;
            call_set(rt, ctx, 0x08A5B9ECu, {scratch_slot});
            object = word(rt, second_slot);
            ctx.gpr[16] = object;
        } while (items != index);
    }
    call_set(rt, ctx, 0x089B6204u, {frame.at(0u), argument});
    const u32 target = word(rt, frame.at(0u));
    ctx.gpr[16] = target;
    ctx.gpr[5] = word(rt, first_slot);
    if (target == 0u) report_null(rt, ctx, 0x08CA5920u);
    call_set(rt, ctx, 0x089B7CF8u, {target, word(rt, first_slot), flags, items});
    call_set(rt, ctx, 0x08A5B9ECu, {frame.at(0u)});
    call_set(rt, ctx, 0x08B35254u, {flags});
    call_set(rt, ctx, 0x08A5B9ECu, {second_slot});
    call_set(rt, ctx, 0x08A5B9ECu, {first_slot});
}

// 0x08A1BA98: for a count a2 > 0 reads a property through two handle slots (a1 passed to 0x08A1B570; the object behind slot sp + 0x10 gives
// a record through virtual +0xDC and +0xD8, null message 0x08CAA110 / 0x08CAA104), builds two strings from it (0x08A1B3B8 / 0x08A85B1C) into
// the slots at sp + 4 and sp, copies them to the destination members at +0x14 / +0x14 + 4 of the object behind slot sp + 0xC (0x08A5BAD8
// unless they already are the slots), copies the slot sp + 0xC itself to a0 and releases all four slots (0x08A5B9EC).
void read_property_pair(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const u32 out = ctx.gpr[4];
    const u32 argument = ctx.gpr[5];
    const u32 count = ctx.gpr[6];
    const u32 holder = ctx.gpr[7];
    GuestFrame frame(ctx, 0x40u);
    SavedRegisters saved(ctx);
    ctx.gpr[20] = out;
    ctx.gpr[18] = argument;
    ctx.gpr[16] = holder;
    if (static_cast<s32>(count) <= 0) return;
    const u32 result_slot = frame.at(0xCu);
    const u32 object_slot = frame.at(0x10u);
    ctx.gpr[19] = result_slot;
    ctx.gpr[21] = object_slot;
    call_set(rt, ctx, 0x08A98C1Cu, {result_slot, 2u});
    call_set(rt, ctx, 0x08A94DA0u, {object_slot, holder});
    u32 object = word(rt, object_slot);
    ctx.gpr[16] = object;
    if (object == 0u) report_null(rt, ctx, 0x08CAA110u);
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xDCu);
    const u32 first = call_set(rt, ctx, ctx.gpr[2], {object, 0u});
    object = word(rt, object_slot);
    ctx.gpr[16] = object;
    put(rt, frame.at(0x14u), first);
    if (object == 0u) report_null(rt, ctx, 0x08CAA110u);
    ctx.gpr[3] = word(rt, object);
    ctx.gpr[2] = word(rt, ctx.gpr[3] + 0xD8u);
    const u32 text_slot = frame.at(8u);
    ctx.gpr[17] = text_slot;
    const u32 second = call_set(rt, ctx, ctx.gpr[2], {object});
    put(rt, frame.at(0x18u), second);
    put(rt, frame.at(0x1Cu), 0u);
    call_set(rt, ctx, 0x08A1B570u, {text_slot, argument});
    object = word(rt, text_slot);
    ctx.gpr[16] = object;
    if (object == 0u) report_null(rt, ctx, 0x08CAA104u);
    const u32 text = call_set(rt, ctx, 0x08A1B3CCu, {object});
    ctx.gpr[16] = text;
    call_set(rt, ctx, 0x08A5B9ECu, {text_slot});
    const u32 target = word(rt, result_slot);
    ctx.gpr[2] = target;
    const u32 member = word(rt, target + 0x14u);
    ctx.gpr[17] = member;
    const u32 string_slot = frame.at(4u);
    ctx.gpr[16] = string_slot;
    const u32 built = call_set(rt, ctx, 0x08A1B3B8u, {text, frame.at(0x14u)});
    call_set(rt, ctx, 0x08A85B1Cu, {string_slot, built});
    ctx.gpr[5] = word(rt, string_slot);
    if (member != string_slot) call_set(rt, ctx, 0x08A5BAD8u, {member, word(rt, string_slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {string_slot});
    const u32 target_again = word(rt, result_slot);
    const u32 second_member = word(rt, target_again + 0x14u) + 4u;
    ctx.gpr[16] = second_member;
    const u32 zero_slot = frame.at(0u);
    call_set(rt, ctx, 0x08A85B1Cu, {zero_slot, word(rt, frame.at(0x1Cu))});
    ctx.gpr[5] = word(rt, zero_slot);
    if (second_member != zero_slot) call_set(rt, ctx, 0x08A5BAD8u, {second_member, word(rt, zero_slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {zero_slot});
    ctx.gpr[5] = word(rt, result_slot);
    if (out != result_slot) call_set(rt, ctx, 0x08A5BAD8u, {out, word(rt, result_slot)});
    call_set(rt, ctx, 0x08A5B9ECu, {object_slot});
    call_set(rt, ctx, 0x08A5B9ECu, {result_slot});
}

} // namespace

#define G ctx.gpr
#define load8(address) rt.memory().load8(address)
#define load16(address) rt.memory().load16(address)
#define store8(address, value) rt.memory().store8((address), static_cast<std::uint8_t>(value))
#define store16(address, value) rt.memory().store16((address), static_cast<std::uint16_t>(value))

void func_08A45188(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x30u, 0x08CADE28u, kSteps_08A45188, std::size(kSteps_08A45188)); }
void func_08A1BA98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { read_property_pair(rt, ctx); }
void func_08B78180(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { read_line(rt, ctx); }
void func_089B7E40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { collect_flags(rt, ctx); }
void func_08837BEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08C9BAB4u, kSteps_08837BEC, std::size(kSteps_08837BEC)); }
void func_08A3D1D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08CAD754u, kSteps_08A3D1D0, std::size(kSteps_08A3D1D0)); }
void func_08A58118(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x30u, 0x08CAF7ECu, kSteps_08A58118, std::size(kSteps_08A58118)); }
void func_0884CDB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x30u, 0x08C9D094u, kSteps_0884CDB8, std::size(kSteps_0884CDB8)); }
void func_08A3DC54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x30u); put(rt, frame.at(0x20u), G[31]); put(rt, frame.at(0x1Cu), G[19]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CAD8CCu; G[6] = frame.at(1u); G[19] = 0x08A40000u; G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08A3E6BCu; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CF6F24u; G[6] = 0x08A3E424u; call_here(rt, ctx, 0x08B8A004u); G[4] = G[18]; G[5] = 0x08CAD918u; G[6] = 0x08A3E424u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD920u; G[6] = 0x08A3DA14u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD928u; G[6] = 0x08A3E350u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD938u; G[6] = 0x08A3E250u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD93Cu; G[6] = 0x08A3E02Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD948u; G[6] = 0x08A3DE1Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD954u; G[6] = 0x08A3D940u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD964u; G[6] = 0x08A3D8F0u; call_here(rt, ctx, 0x08B87108u);  G[19] = word(rt, frame.at(0x1Cu)); G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08818A04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08C98D6Cu, kSteps_08818A04, std::size(kSteps_08818A04)); }
void func_08839D78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08C9BBF8u, kSteps_08839D78, std::size(kSteps_08839D78)); }
void func_08A33D28(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08CACCECu, kSteps_08A33D28, std::size(kSteps_08A33D28)); }
void func_08A1D728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CAA370u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08A1EB54u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CAA384u; G[6] = 0x08A1E09Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA390u; G[6] = 0x08A1E97Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA39Cu; G[6] = 0x08A1E07Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA3A8u; G[6] = 0x08A1E95Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA3B4u; G[6] = 0x08A1E05Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA3C0u; G[6] = 0x08A1E93Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA3CCu; G[6] = 0x08A1E430u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA3D8u; G[6] = 0x08A1E410u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA3E4u; G[6] = 0x08A1E3F0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA3F4u; G[6] = 0x08A1E3D0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAA400u; G[6] = 0x08A1E03Cu; call_here(rt, ctx, 0x08B87108u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08A7EF54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CB13D8u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08A7FCD0u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CF6F24u; G[6] = 0x08A7D140u; call_here(rt, ctx, 0x08B8A004u); G[4] = G[18]; G[5] = 0x08CB13E0u; G[6] = 0x08A7F258u; G[7] = 0x08A7F258u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CB13E8u; G[6] = 0x08A7F140u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB13F0u; G[6] = 0x08A7CEC8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB1400u; G[6] = 0x08A7CD30u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB140Cu; G[6] = 0x08A7C6D4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB141Cu; G[6] = 0x08A7CFE8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB142Cu; G[6] = 0x08A7EB58u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB1434u; G[6] = 0x08A7FBE8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB1444u; G[6] = 0x08A7E2B0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CB1454u; G[6] = 0x08A7D3CCu; call_here(rt, ctx, 0x08B87108u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08A42028(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08CADBFCu, kSteps_08A42028, std::size(kSteps_08A42028)); }
void func_08A35C30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08CACF04u, kSteps_08A35C30, std::size(kSteps_08A35C30)); }
void func_08A24038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08CAACE0u, kSteps_08A24038, std::size(kSteps_08A24038)); }
void func_089C7710(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08CA63A0u, kSteps_089C7710, std::size(kSteps_089C7710)); }
void func_08A3B5CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CAD544u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08A3B1E0u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CAD590u; G[6] = 0x08A3BD88u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD598u; G[6] = 0x08A3ABF4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5A0u; G[6] = 0x08A3BCB4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5A8u; G[6] = 0x08A3C6B8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5B0u; G[6] = 0x08A3C930u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5BCu; G[6] = 0x08A3BBE0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5C4u; G[6] = 0x08A3BB0Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5D0u; G[6] = 0x08A3AC40u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5D8u; G[6] = 0x08A3ACDCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5E0u; G[6] = 0x08A3B9E8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5E8u; G[6] = 0x08A3B8C4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD5F8u; G[6] = 0x08A3AD70u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD600u; G[6] = 0x08A3B220u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD60Cu; G[6] = 0x08A3B2A8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD618u; G[6] = 0x08A3C544u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD624u; G[6] = 0x08A3BE8Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD630u; G[6] = 0x08A3B4B4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAD644u; G[6] = 0x08A3B0D8u; G[7] = 0x08A3AF3Cu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CAD64Cu; G[6] = 0x08A3B050u; G[7] = 0x08A3B540u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CAD654u; G[6] = 0x08A3AFC8u; G[7] = 0x08A3B428u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CAD65Cu; G[6] = 0x08A3B160u; G[7] = 0x08A3AB70u; call_here(rt, ctx, 0x08B878E8u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_088429A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08C9C584u, kSteps_088429A0, std::size(kSteps_088429A0)); }
void func_0884E9D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_script_object(rt, ctx, 0x20u, 0x08C9D198u, kSteps_0884E9D4, std::size(kSteps_0884E9D4)); }
void func_089C09AC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CA5EB4u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x089DF9E4u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x089BFEB0u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CA5F24u; G[6] = 0x089BE834u; call_here(rt, ctx, 0x08B86FB0u); G[4] = G[18]; G[5] = 0x08CA5F30u; G[6] = 0x089BE83Cu; call_here(rt, ctx, 0x08B86FB0u); G[4] = G[18]; G[5] = 0x08CA5F3Cu; G[6] = 0x089BE844u; call_here(rt, ctx, 0x08B86FB0u); G[4] = G[18]; G[5] = 0x08CA5F44u; G[6] = 0x089BE84Cu; call_here(rt, ctx, 0x08B86FB0u); G[4] = G[18]; G[5] = 0x08CA5F50u; G[6] = 0x089BE854u; call_here(rt, ctx, 0x08B86FB0u); G[4] = G[18]; G[5] = 0x08CA5F5Cu; G[6] = 0x089BE85Cu; call_here(rt, ctx, 0x08B86FB0u); G[4] = G[18]; G[5] = 0x08CA5F68u; G[6] = 0x089BE864u; call_here(rt, ctx, 0x08B86FB0u); G[4] = G[18]; G[5] = 0x08CA5F74u; G[6] = 0x089C0EA8u; G[7] = 0x089C1368u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA5F80u; G[6] = 0x089C121Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5F8Cu; G[6] = 0x089C0DA8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5F9Cu; G[6] = 0x089BEE8Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5FA8u; G[6] = 0x089BFCA8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5FB4u; G[6] = 0x089C151Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5FBCu; G[6] = 0x089C11D0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5FC4u; G[6] = 0x089BFEF0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5FD0u; G[6] = 0x089BFF7Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5FDCu; G[6] = 0x089C0008u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5FECu; G[6] = 0x089C0094u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA5FF8u; G[6] = 0x089BF400u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA6000u; G[6] = 0x089C0160u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA6008u; G[6] = 0x089C01E8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA6010u; G[6] = 0x089C0270u; G[7] = 0x089C0270u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA6014u; G[6] = 0x089C0360u; G[7] = 0x089C0360u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA6020u; G[6] = 0x089C0450u; G[7] = 0x089C0450u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA602Cu; G[6] = 0x089BF634u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA603Cu; G[6] = 0x089BF978u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA604Cu; G[6] = 0x089BF86Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA6054u; G[6] = 0x089BF92Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA6060u; G[6] = 0x089BFA78u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA6070u; G[6] = 0x089BF78Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA6080u; G[6] = 0x089C0538u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA608Cu; G[6] = 0x08BFFCB0u; G[7] = 0x08BFFBCCu; call_here(rt, ctx, 0x08B878E8u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08990190(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CA3CB8u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08994F90u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x089910CCu; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CA3D1Cu; G[6] = 0x08990144u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA3D24u; G[6] = 0x0898E358u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CA3D2Cu; G[6] = 0x0898E41Cu; G[7] = 0x0898E2F4u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D34u; G[6] = 0x0898E480u; G[7] = 0x0898E480u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D3Cu; G[6] = 0x0898E594u; G[7] = 0x0898E594u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D44u; G[6] = 0x089905CCu; G[7] = 0x089905CCu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D4Cu; G[6] = 0x0898E6A4u; G[7] = 0x0898E6A4u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D5Cu; G[6] = 0x0898E7B0u; G[7] = 0x0898E7B0u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D70u; G[6] = 0x0898E8B8u; G[7] = 0x0898E8B8u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D84u; G[6] = 0x0898E9C0u; G[7] = 0x0898E9C0u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D90u; G[6] = 0x0898EAC8u; G[7] = 0x0898EAC8u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3D9Cu; G[6] = 0x0898EBD0u; G[7] = 0x0898EBD0u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3DA8u; G[6] = 0x0898ECCCu; G[7] = 0x0898ECCCu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3DB0u; G[6] = 0x0898EDC8u; G[7] = 0x0898EDC8u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3DB8u; G[6] = 0x0898EF54u; G[7] = 0x0898EF54u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3DC0u; G[6] = 0x0898F064u; G[7] = 0x0898F064u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3DD4u; G[6] = 0x0898F168u; G[7] = 0x0898F168u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3DE8u; G[6] = 0x0898F26Cu; G[7] = 0x0898F26Cu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3DF8u; G[6] = 0x0898F360u; G[7] = 0x0898F360u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E08u; G[6] = 0x0898F454u; G[7] = 0x0898F454u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E10u; G[6] = 0x0898F54Cu; G[7] = 0x0898F54Cu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E18u; G[6] = 0x0898F644u; G[7] = 0x0898F644u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E28u; G[6] = 0x0898F738u; G[7] = 0x0898F738u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E38u; G[6] = 0x0898F85Cu; G[7] = 0x0898F85Cu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E48u; G[6] = 0x0898F95Cu; G[7] = 0x0898F95Cu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E58u; G[6] = 0x0898FA50u; G[7] = 0x0898FA50u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E68u; G[6] = 0x0898FB74u; G[7] = 0x0898FB74u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E78u; G[6] = 0x0898FC74u; G[7] = 0x0898FC74u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E84u; G[6] = 0x0898FD68u; G[7] = 0x0898FD68u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3E94u; G[6] = 0x0898FE70u; G[7] = 0x0898FE70u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08CA3EA0u; G[6] = 0x0898E210u; call_here(rt, ctx, 0x08B87108u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_0881C3E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08C98F48u; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x0881CD84u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CF6F24u; G[6] = 0x0881BE7Cu; call_here(rt, ctx, 0x08B8A004u); G[4] = G[18]; G[5] = 0x08C98F94u; G[6] = 0x0881CDC4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98F9Cu; G[6] = 0x0881CEF4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98FA8u; G[6] = 0x0881D004u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98FB0u; G[6] = 0x0881D18Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98FB8u; G[6] = 0x0881D2ECu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98FC0u; G[6] = 0x0881D450u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98FD0u; G[6] = 0x0881D538u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98FD8u; G[6] = 0x0881D674u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98FE4u; G[6] = 0x0881DF14u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C98FF8u; G[6] = 0x0881D7ACu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99008u; G[6] = 0x0881D7F0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9901Cu; G[6] = 0x0881D87Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99030u; G[6] = 0x0881D908u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99044u; G[6] = 0x0881DAD8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99058u; G[6] = 0x0881DCA8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9906Cu; G[6] = 0x0881E0ACu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9907Cu; G[6] = 0x0881E35Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99084u; G[6] = 0x0881E458u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99090u; G[6] = 0x0881F9E4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99098u; G[6] = 0x0881F8A8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C990A8u; G[6] = 0x0881E5B0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C990B8u; G[6] = 0x0881E6C0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C990C4u; G[6] = 0x0881E7D0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C990D0u; G[6] = 0x0881E8E4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C990E4u; G[6] = 0x0881E9C4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C990F8u; G[6] = 0x0881EAD0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99108u; G[6] = 0x0881EBE0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99118u; G[6] = 0x0881ECECu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99128u; G[6] = 0x0881ED80u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9913Cu; G[6] = 0x0881EE14u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99154u; G[6] = 0x0881EF0Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C99170u; G[6] = 0x0881BDD8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C9918Cu; G[6] = 0x0881EFB8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C991A8u; G[6] = 0x0881F048u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08C991C0u; G[6] = 0x0881F0D8u; G[7] = 0x0881F0D8u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C991C8u; G[6] = 0x0881F1DCu; G[7] = 0x0881F1DCu; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C991D0u; G[6] = 0x0881F2D8u; G[7] = 0x0881F2D8u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C991E0u; G[6] = 0x0881F3D4u; G[7] = 0x0881F3D4u; call_here(rt, ctx, 0x08B878E8u); G[4] = G[18]; G[5] = 0x08C991F0u; G[6] = 0x0881F4D0u; G[7] = 0x0881F4D0u; call_here(rt, ctx, 0x08B878E8u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }
void func_08A2E8CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { { { GuestFrame frame(ctx, 0x20u); put(rt, frame.at(0x1Cu), G[31]); put(rt, frame.at(0x18u), G[18]); put(rt, frame.at(0x10u), G[16]); put(rt, frame.at(0x14u), G[17]); G[5] = 0x08CAC3BCu; G[6] = frame.at(1u); G[18] = G[4]; G[16] = frame.at(4u); G[2] = word(rt, G[4]); G[4] = G[16]; G[17] = word(rt, G[2] + 0x108u); call_here(rt, ctx, 0x08B85EB8u); G[5] = G[16]; G[4] = G[18]; call_here(rt, ctx, G[17]); G[2] = 0x08D74FD0u; G[3] = word(rt, frame.at(0x4u)); G[5] = 0xFFFFFFFFu; G[16] = G[3] - 0xCu; G[4] = G[3] - 4u; if (G[16] != G[2]) { call_here(rt, ctx, 0x08B60804u); G[4] = G[16]; G[5] = frame.at(0x0u); if (static_cast<std::int32_t>(G[2]) <= 0) call_here(rt, ctx, 0x08B8542Cu); } call_here(rt, ctx, 0x08A76A94u); G[16] = G[2]; G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5BA08u); G[2] = word(rt, G[18] + 0x38u); G[4] = G[2]; if (G[2] != 0u) call_here(rt, ctx, 0x08A5B930u); put(rt, G[18] + 0x38u, G[16]); G[4] = G[18]; G[5] = 0x08CF6F14u; G[6] = 0x08C1AA40u; call_here(rt, ctx, 0x08B8850Cu); G[4] = G[18]; G[5] = 0x08CAC3C8u; G[6] = 0x08A2E410u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC3DCu; G[6] = 0x08A2EDE4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC3E8u; G[6] = 0x08A2DFCCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC3F4u; G[6] = 0x08A2EEDCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC40Cu; G[6] = 0x08A2E39Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC424u; G[6] = 0x08A2F1C8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC440u; G[6] = 0x08A2E328u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC45Cu; G[6] = 0x08A2DFD8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC468u; G[6] = 0x08A2DFE4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC474u; G[6] = 0x08A2F29Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC484u; G[6] = 0x08A2F34Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC494u; G[6] = 0x08A2F3FCu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC49Cu; G[6] = 0x08A2F494u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC4ACu; G[6] = 0x08A2E264u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC4C0u; G[6] = 0x08A2E128u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC4D0u; G[6] = 0x08A2F4E8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC4E0u; G[6] = 0x08A2F530u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC4F0u; G[6] = 0x08A2DFF8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC504u; G[6] = 0x08A2E000u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC518u; G[6] = 0x08A2DFF0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC524u; G[6] = 0x08A2F5B0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC530u; G[6] = 0x08A2F628u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC544u; G[6] = 0x08A2F6A0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC550u; G[6] = 0x08A2F750u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC560u; G[6] = 0x08A2E008u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC570u; G[6] = 0x08A2E014u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC580u; G[6] = 0x08A2E660u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC58Cu; G[6] = 0x08A2E6D4u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC5A8u; G[6] = 0x08A2E0A8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC5B4u; G[6] = 0x08A2F800u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC5BCu; G[6] = 0x08A2F858u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC5C8u; G[6] = 0x08A2F8A8u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC5DCu; G[6] = 0x08A2DF3Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC5F0u; G[6] = 0x08A2FB84u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC608u; G[6] = 0x08A2FE90u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC624u; G[6] = 0x08A3013Cu; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC640u; G[6] = 0x08A30418u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC658u; G[6] = 0x08A30468u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC678u; G[6] = 0x08A30594u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC698u; G[6] = 0x08A30870u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC6ACu; G[6] = 0x08A308C0u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC6C4u; G[6] = 0x08A30918u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC6E0u; G[6] = 0x08A2DF60u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC6F8u; G[6] = 0x08A2DF84u; call_here(rt, ctx, 0x08B87108u); G[4] = G[18]; G[5] = 0x08CAC710u; G[6] = 0x08A2DFA8u; call_here(rt, ctx, 0x08B87108u);  G[18] = word(rt, frame.at(0x18u)); G[16] = word(rt, frame.at(0x10u)); G[17] = word(rt, frame.at(0x14u)); }  } }

#undef G
#undef load8
#undef load16
#undef store8
#undef store16

} // namespace hot_batch_312
} // namespace gt::game

#define HOT312(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_312::name, gt::ReturnKind::kind)
HOT312(0x08A45188u, func_08A45188, Word);
HOT312(0x08A1BA98u, func_08A1BA98, Word);
HOT312(0x08B78180u, func_08B78180, Word);
HOT312(0x089B7E40u, func_089B7E40, Word);
HOT312(0x08837BECu, func_08837BEC, Word);
HOT312(0x08A3D1D0u, func_08A3D1D0, Word);
HOT312(0x08A58118u, func_08A58118, Word);
HOT312(0x0884CDB8u, func_0884CDB8, Word);
HOT312(0x08A3DC54u, func_08A3DC54, Word);
HOT312(0x08818A04u, func_08818A04, Word);
HOT312(0x08839D78u, func_08839D78, Word);
HOT312(0x08A33D28u, func_08A33D28, Word);
HOT312(0x08A1D728u, func_08A1D728, Word);
HOT312(0x08A7EF54u, func_08A7EF54, Word);
HOT312(0x08A42028u, func_08A42028, Word);
HOT312(0x08A35C30u, func_08A35C30, Word);
HOT312(0x08A24038u, func_08A24038, Word);
HOT312(0x089C7710u, func_089C7710, Word);
// HOT312(0x08A3B5CCu, func_08A3B5CC, Word);  // no comparable synthetic case yet
HOT312(0x088429A0u, func_088429A0, Word);
HOT312(0x0884E9D4u, func_0884E9D4, Word);
HOT312(0x089C09ACu, func_089C09AC, Word);
HOT312(0x08990190u, func_08990190, Word);
HOT312(0x0881C3E8u, func_0881C3E8, Word);
// HOT312(0x08A2E8CCu, func_08A2E8CC, Word);  // no comparable synthetic case yet
