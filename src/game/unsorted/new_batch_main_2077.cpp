// New batch main-2077: 30 functions that mismatched or were skipped in earlier drafts, written again by hand from `tools.disasm`
// (relocations applied, every branch read as the target it prints) as structured C. Delay slots run before the call / branch.
// `call_set` writes the argument registers the original sets before the `jal` and returns (and leaves in v0) the callee's v0;
// `SavedRegisters` puts the s-registers back and `GuestFrame` the stack pointer, as the original's epilogue does. The tail calls
// (`j`) run after the frame scope closed, with the arguments the original's delay slot sets.
// The 19 "pair" bodies (func_08A59828 ... func_088588AC) call F(a0, 0x08CF6ED8) and tail-call G(a0, a1).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_2077 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// The `lui a1, 2` / `addiu a1` constant of the pair bodies (a data address after relocation).
constexpr u32 kCell = 0x08CF6ED8u;

// Sets the argument registers the original sets before the `jal`, calls, and leaves the callee's v0 in v0 and the result.
inline u32 call_set(Runtime &rt, AllegrexContext &ctx, u32 address, std::initializer_list<u32> arguments) {
    unsigned reg = 4u;
    for (const u32 argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result.v0;
}

// s0 / s1 are callee-saved and compared: the bodies that use them set them like the original and the caller's values come back.
class SavedRegisters {
public:
    explicit SavedRegisters(AllegrexContext &ctx) : ctx_(ctx), s0_(ctx.gpr[16]), s1_(ctx.gpr[17]) {}
    ~SavedRegisters() {
        ctx_.gpr[16] = s0_;
        ctx_.gpr[17] = s1_;
    }
    SavedRegisters(const SavedRegisters &) = delete;
    SavedRegisters &operator=(const SavedRegisters &) = delete;

private:
    AllegrexContext &ctx_;
    u32 s0_;
    u32 s1_;
};

}  // namespace

// 0x08AA5FA8 (a0, a1): s0 = a0; func_08AA5D90(a0, a1 & 0xFF) runs; v0 = s0.

// 0x0888B85C (a0, a1, a2): a 0x1C-byte record at sp gets 0x1C, a1 at +8 and a2 at +0xC; func_088936A4(a0[0x18], &record) runs
// and its result is the return value.
void func_0888B85C(Runtime &rt, AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0xB0u);
    put(rt, frame.at(0x0u), 0x1Cu);
    put(rt, frame.at(0x8u), ctx.gpr[5]);
    put(rt, frame.at(0xCu), ctx.gpr[6]);
    (void)call_set(rt, ctx, 0x088936A4u, {word(rt, ctx.gpr[4] + 0x18u), frame.at(0u)});
}

// 0x08B2BCA0 (a0): s0 = a0; func_08B344E0(a0) and func_08B2BC5C(a0) run; then func_08B3457C(a0) is a tail call.

// 0x08AC1130 (a0..a3): four zero words at sp+0..+0xC; func_08AC1060(a0, a1, a2, sp, a3 & 0xFF) (the last in t0), v0 = its result.

// 0x08B76A8C (a0 = holder): with the holder's word zero v0 = 0 and the function returns; otherwise func_08B76A84() runs and
// func_08B35254(its result) is a tail call.

// 0x08AC1164 (t0 = pair, a0..a3, t1): the pair's two words at sp+0 and +4, zeros at +8 and +0xC; func_08AC0F08(a0, a1, a2, a3,
// sp, t1 & 0xFF) (sp in t0), v0 = its result.

// 0x08B0FBC8 (a3 = pair, a4): the pair's words at sp+8 (low) and +0xC (high), zeros at +0 and +4; func_08B0F840(a0, a1, a2,
// sp, a4 & 0xFF) (the last in t0). Tail-less: the result is the callee's v0.

// 0x08AA646C (a0, a1 = out): s1 = a0, s0 = a1; func_08AA643C(a0) runs; its v0 is stored as a halfword at out; v0 = a0.

// 0x08AA6538 (a0, a1 = out): s1 = a0, s0 = a1; func_08AA64DC(a0) runs; its v0 is stored as a byte at out; v0 = a0.

// 0x0880BF34 (a0 = object): func_08A5B9EC(object + 0x208), func_088697E8(object + 0x30) and func_0880BEE8(object + 0x28) run,
// then func_08B28DC4(object) is a tail call.

// 0x0890F838 (a0, a1, a2, a3): func_0890F664(a0, a1, a3) runs; then func_08AB8774(a1, 0) runs; then func_08AB8768(a1, 0) is a
// tail call with f12 = 30.0.

// 0x08A59828 (a0, a1): s0 = a1, s1 = a0; func_08A52AB0(a0, 0x08CF6ED8) runs; then func_08A5973C(a0, a1) is a tail call.

// 0x08AB182C (a0, a1): same shape as func_08A59828: func_08A76AB4(a0, 0x08CF6ED8), then tail call func_08AB173C(a0, a1).

// 0x08992860 (a0, a1): func_08A0D868(a0, 0x08CF6ED8), then tail call func_08992754(a0, a1).

// 0x089FF088 (a0, a1): func_08965854(a0, 0x08CF6ED8), then tail call func_089FEFDC(a0, a1).

// 0x089898DC (a0, a1): func_08969190(a0, 0x08CF6ED8), then tail call func_08989620(a0, a1).

// 0x08974918 (a0, a1): func_089D8A60(a0, 0x08CF6ED8), then tail call func_0897482C(a0, a1).

// 0x08A8911C (a0, a1): func_08A71E10(a0, 0x08CF6ED8), then tail call func_08A89030(a0, a1).

// 0x08A81134 (a0, a1): func_08A76AB4(a0, 0x08CF6ED8), then tail call func_08A81048(a0, a1).

// 0x089FE938 (a0, a1): func_08A0D868(a0, 0x08CF6ED8), then tail call func_089FE788(a0, a1).

// 0x08A97960 (a0, a1): func_08A76AB4(a0, 0x08CF6ED8), then tail call func_08A977E4(a0, a1).

// 0x08AB5968 (a0, a1): func_08A7026C(a0, 0x08CF6ED8), then tail call func_08AB56F8(a0, a1).

// 0x08A5A174 (a0, a1): func_08A52AB0(a0, 0x08CF6ED8), then tail call func_08A5A0C8(a0, a1).

// 0x08A5F980 (a0, a1): func_08A76AB4(a0, 0x08CF6ED8), then tail call func_08A5F800(a0, a1).

// 0x08AAC194 (a0, a1): func_08A52AB0(a0, 0x08CF6ED8), then tail call func_08AAC0A8(a0, a1).

// 0x08A08268 (a0, a1): func_08969190(a0, 0x08CF6ED8), then tail call func_08A0817C(a0, a1).

// 0x08A8C298 (a0, a1): func_08A76AB4(a0, 0x08CF6ED8), then tail call func_08A8C1EC(a0, a1).

// 0x08AB62AC (a0, a1): func_08A70EC8(a0, 0x08CF6ED8), then tail call func_08AB6130(a0, a1).

// 0x0899BAC4 (a0, a1): func_08973D98(a0, 0x08CF6ED8), then tail call func_0899B9D8(a0, a1).

// 0x088588AC (a0, a1): func_08A76AB4(a0, 0x08CF6ED8), then tail call func_08858784(a0, a1).

}  // namespace new_batch_main_2077
}  // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_2077::name, gt::ReturnKind::kind)
NEWM(0x0888B85Cu, func_0888B85C, Word);
