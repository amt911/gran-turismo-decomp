// New batch main-147 (draft, unverified): written by hand from `tools.disasm`.
// Delay slots are read as "before the call". The callee-saved registers the original keeps values in are set where the
// asm sets them and put back at the end; v0 ends as the last call's answer. No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_147 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;
using s32 = std::int32_t;

inline u32 call_keep(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}
// A guest call with the argument registers set like the original sets them before the jal (a0-a3, t0-t3 in order).
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    return call_keep(rt, ctx, address, arguments);
}

class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx) {
        for (unsigned i = 0u; i < 8u; ++i) saved_[i] = ctx.gpr[16u + i];
        fp_ = ctx.gpr[30];
        ra_ = ctx.gpr[31];
    }
    ~SavedRegisters() {
        for (unsigned i = 0u; i < 8u; ++i) ctx_.gpr[16u + i] = saved_[i];
        ctx_.gpr[30] = fp_;
        ctx_.gpr[31] = ra_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 saved_[8];
    u32 fp_;
    u32 ra_;
};

// Copies the two words at `source` into the frame at `offset` (the asm loads both, then stores them around the call).
void copy_pair(Runtime &rt, const GuestFrame &frame, u32 offset, u32 source) {
    put(rt, frame.at(offset), word(rt, source));
    put(rt, frame.at(offset + 4u), word(rt, source + 4u));
}

// 0x089B25B4 (a0 = object): releases the string the object's virtual at +0x108 returns (0x08B60804 / 0x08B8542C, unless it is the
// shared empty one 0x08D74FD0), replaces the object's +0x38 member by the answer of 0x08A76A94 (0x08A5BA08 gets it, 0x08A5B930 the old
// one), then registers the object's attribute/method table through 0x08B87108 (name, handler), 0x08B878E8 (name, getter, setter) and
// the typed variants (0x08BFB644, 0x08BFD13C, 0x08BFCC68, 0x08BFB06C, ... with a pair of words copied to the frame).
// v0 = the last registration's answer.
void func_089B25B4(Runtime &rt, AllegrexContext &ctx) {
    const u32 self = ctx.gpr[4];
    SavedRegisters saved(ctx);
    GuestFrame frame(ctx, 0x100u);
    ctx.gpr[18] = self;
    ctx.gpr[16] = frame.at(4u);
    const u32 virtuals = word(rt, self);
    const u32 method = word(rt, virtuals + 0x108u);
    ctx.gpr[17] = method;
    call_set(rt, ctx, 0x08B85EB8u, {frame.at(4u), 0x08CA50C4u, frame.at(1u)});
    call_set(rt, ctx, method, {self, frame.at(4u)});

    const u32 text = word(rt, frame.at(4u));
    const u32 count = text - 0xCu;
    ctx.gpr[16] = count;
    ctx.gpr[2] = 0x08D74FD0u;
    if (count != 0x08D74FD0u) {
        const s32 remaining = static_cast<s32>(call_set(rt, ctx, 0x08B60804u, {text - 4u, 0xFFFFFFFFu}));
        if (remaining <= 0) call_set(rt, ctx, 0x08B8542Cu, {count, frame.at(0u)});
    }
    const u32 created = call_keep(rt, ctx, 0x08A76A94u, {});
    ctx.gpr[16] = created;
    if (created != 0u) call_set(rt, ctx, 0x08A5BA08u, {created});
    const u32 previous = word(rt, self + 0x38u);
    ctx.gpr[4] = previous;
    if (previous != 0u) call_keep(rt, ctx, 0x08A5B930u, {previous});
    put(rt, self + 0x38u, created);

    ctx.gpr[16] = 0x001A0000u;
    ctx.gpr[17] = 0x001B0000u;
    const auto method_entry = [&](u32 name, u32 handler) { call_set(rt, ctx, 0x08B87108u, {self, name, handler}); };
    const auto attribute = [&](u32 name, u32 handler) { call_set(rt, ctx, 0x08B878E8u, {self, name, handler, handler}); };

    method_entry(0x08CA5200u, 0x089B2324u);
    method_entry(0x08CA520Cu, 0x089B2014u);
    method_entry(0x08CA5218u, 0x089B1D10u);
    method_entry(0x08CA5224u, 0x089AFE00u);
    method_entry(0x08CA5230u, 0x089B1820u);
    copy_pair(rt, frame, 0xE0u, 0x08C8C8D8u);
    call_set(rt, ctx, 0x08BFB644u, {self, 0x08CA523Cu, frame.at(0xE0u)});
    copy_pair(rt, frame, 0xD8u, 0x08C8C8D0u);
    call_set(rt, ctx, 0x08BFB644u, {self, 0x08CA524Cu, frame.at(0xD8u)});
    copy_pair(rt, frame, 0xD0u, 0x08C8C8C8u);
    call_set(rt, ctx, 0x08BFD13Cu, {self, 0x08CA525Cu, frame.at(0xD0u)});
    copy_pair(rt, frame, 0xC8u, 0x08C8C8C0u);
    call_set(rt, ctx, 0x08BFD13Cu, {self, 0x08CA526Cu, frame.at(0xC8u)});
    method_entry(0x08CA527Cu, 0x089AB5D8u);
    method_entry(0x08CA5284u, 0x089AEF20u);
    method_entry(0x08CA528Cu, 0x089AECECu);
    method_entry(0x08CA5298u, 0x089AEBE4u);
    method_entry(0x08CA52A8u, 0x089A890Cu);
    method_entry(0x08CA52B0u, 0x089A890Cu);
    method_entry(0x08CA52B8u, 0x089AB54Cu);
    method_entry(0x08CA52C4u, 0x089AB4F4u);
    method_entry(0x08CA52D8u, 0x089AB49Cu);
    method_entry(0x08CA52ECu, 0x089AB444u);
    method_entry(0x08CA52FCu, 0x089AB3ECu);
    method_entry(0x08CA530Cu, 0x089AF064u);
    method_entry(0x08CA5318u, 0x089AF1CCu);
    copy_pair(rt, frame, 0xC0u, 0x08C8C8B8u);
    call_set(rt, ctx, 0x08BFCC68u, {self, 0x08CA5324u, frame.at(0xC0u)});
    copy_pair(rt, frame, 0xB8u, 0x08C8C8B0u);
    call_set(rt, ctx, 0x08BFCC68u, {self, 0x08CA5334u, frame.at(0xB8u)});
    method_entry(0x08CA5344u, 0x089AEB0Cu);
    method_entry(0x08CA5350u, 0x089AD804u);
    call_set(rt, ctx, 0x08B8D51Cu, {self, 0x08CA5364u, 0x089AD804u, 0u});
    method_entry(0x08CA536Cu, 0x089AE690u);
    method_entry(0x08CA5378u, 0x089AE57Cu);
    method_entry(0x08CA5388u, 0x089AD88Cu);
    method_entry(0x08CA5398u, 0x089AB3B0u);
    method_entry(0x08CA53A8u, 0x089AD9FCu);
    method_entry(0x08CA53BCu, 0x089ADA98u);
    method_entry(0x08CA53D0u, 0x089ABA7Cu);
    copy_pair(rt, frame, 0xB0u, 0x08C8C8A8u);
    copy_pair(rt, frame, 0xA8u, 0x08C8C8A0u);
    call_set(rt, ctx, 0x08BFB06Cu, {self, 0x08CA53E8u, frame.at(0xB0u), frame.at(0xA8u)});
    copy_pair(rt, frame, 0xA0u, 0x08C8C898u);
    copy_pair(rt, frame, 0x98u, 0x08C8C890u);
    call_set(rt, ctx, 0x08BFB4E0u, {self, 0x08CA53F0u, frame.at(0xA0u), frame.at(0x98u)});
    attribute(0x08CA53FCu, 0x089ADB38u);
    copy_pair(rt, frame, 0x90u, 0x08C8C888u);
    copy_pair(rt, frame, 0x88u, 0x08C8C880u);
    call_set(rt, ctx, 0x08BFB06Cu, {self, 0x08CA5404u, frame.at(0x90u), frame.at(0x88u)});
    attribute(0x08CA540Cu, 0x089ADC30u);
    attribute(0x08CA541Cu, 0x089A8918u);
    attribute(0x08CA5428u, 0x089ADD40u);
    copy_pair(rt, frame, 0x80u, 0x08C8C878u);
    copy_pair(rt, frame, 0x78u, 0x08C8C870u);
    call_set(rt, ctx, 0x08BFB364u, {self, 0x08CA5440u, frame.at(0x80u), frame.at(0x78u)});
    copy_pair(rt, frame, 0x70u, 0x08C8C868u);
    copy_pair(rt, frame, 0x68u, 0x08C8C860u);
    call_set(rt, ctx, 0x08BFB1E8u, {self, 0x08CA5448u, frame.at(0x70u), frame.at(0x68u)});
    copy_pair(rt, frame, 0x60u, 0x08C8C858u);
    copy_pair(rt, frame, 0x58u, 0x08C8C850u);
    call_set(rt, ctx, 0x08BFB06Cu, {self, 0x08CA5450u, frame.at(0x60u), frame.at(0x58u)});
    attribute(0x08CA5460u, 0x089ADE38u);
    attribute(0x08CA5474u, 0x089B04FCu);
    call_set(rt, ctx, 0x08BEEBB4u, {self, 0x08CA5480u, 0u, 0x089AB9FCu});
    call_set(rt, ctx, 0x08BEEBB4u, {self, 0x08CA5490u, 0u, 0x089AFF8Cu});
    call_set(rt, ctx, 0x08B8D51Cu, {self, 0x08CA54A0u, 0x089ADF28u, 0u});
    method_entry(0x08CA54ACu, 0x089ADF70u);
    copy_pair(rt, frame, 0x50u, 0x08C8C848u);
    call_set(rt, ctx, 0x08BFCFA0u, {self, 0x08CA54C0u, frame.at(0x50u)});
    copy_pair(rt, frame, 0x48u, 0x08C8C840u);
    copy_pair(rt, frame, 0x40u, 0x08C8C838u);
    call_set(rt, ctx, 0x08BFB06Cu, {self, 0x08CA54D4u, frame.at(0x48u), frame.at(0x40u)});
    copy_pair(rt, frame, 0x38u, 0x08C8C830u);
    copy_pair(rt, frame, 0x30u, 0x08C8C828u);
    call_set(rt, ctx, 0x08BFB06Cu, {self, 0x08CA54E4u, frame.at(0x38u), frame.at(0x30u)});
    copy_pair(rt, frame, 0x28u, 0x08C8C820u);
    call_set(rt, ctx, 0x08BFCE04u, {self, 0x08CA54F4u, frame.at(0x28u)});
    copy_pair(rt, frame, 0x20u, 0x08C8C818u);
    call_set(rt, ctx, 0x08BFCC68u, {self, 0x08CA5504u, frame.at(0x20u)});
    copy_pair(rt, frame, 0x18u, 0x08C8C810u);
    call_set(rt, ctx, 0x08BFCC68u, {self, 0x08CA551Cu, frame.at(0x18u)});
    copy_pair(rt, frame, 0x10u, 0x08C8C808u);
    call_set(rt, ctx, 0x08BFCC68u, {self, 0x08CA5534u, frame.at(0x10u)});
    copy_pair(rt, frame, 0x08u, 0x08C8C800u);
    call_set(rt, ctx, 0x08BFCC68u, {self, 0x08CA554Cu, frame.at(0x08u)});
    method_entry(0x08CA5564u, 0x089AC020u);
}

} // namespace
} // namespace new_batch_main_147
} // namespace gt::game

#define NEWM147(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_147::name, gt::ReturnKind::kind)
NEWM147(0x089B25B4u, func_089B25B4, Word);
