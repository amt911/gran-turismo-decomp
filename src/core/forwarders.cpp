// Forwarders the compiler emits many times over, one body per shape and a table with each
// instance's constants:
//   call_and_return  `addiu $sp, -16; sw $ra; jal target; ...; jr $ra`  return target(args...);
//                    (a non-tail call: same arguments, the callee's results come back unchanged)
//   store_and_jump   `lui/addiu $v0, table; j target; sw $v0, 0($a0)`  *this = table; target(this, ...)
//                    (the shape of a destructor resetting the object's vtable before the base one)
// Listed: the ones the race path calls (the harness compared them equal); the others stay
// recompiled until a run reaches them.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <unordered_map>

namespace gt::core {
namespace {

struct Forwarder {
    std::uint32_t target;
    std::uint32_t stored;
};

std::unordered_map<std::uint32_t, Forwarder> &forwarder_table() {
    static std::unordered_map<std::uint32_t, Forwarder> table;
    return table;
}

} // namespace

struct ForwarderRegistrar {
    ForwarderRegistrar(std::uint32_t entry, std::uint32_t target, std::uint32_t stored) {
        forwarder_table()[entry] = {target, stored};
    }
};

// call_and_return(args...) -> target(args...): v0, v1 and $f0 as the target left them.
void call_and_return(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = forwarder_table().at(ctx.pc).target;
    GuestFrame frame(ctx, 0x10u);
    const GuestResult result = call_guest(rt, ctx, target, {}, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
}

// store_and_jump(this, ...): *this = the instance's table, then continue in the target.
void store_and_jump(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Forwarder &f = forwarder_table().at(ctx.pc);
    rt.memory().store32(ctx.gpr[4], f.stored);
    tail_call(ctx, f.target, {});
}

} // namespace gt::core

#define GT_DECOMP_FORWARDER_(fn, returns, entry, target, stored)                                       \
    static const ::gt::core::ForwarderRegistrar GT_DECOMP_CAT(gt_forwarder_, __LINE__){(entry), (target), (stored)}; \
    GT_DECOMP(entry, gt::core::fn, returns)
#define GT_DECOMP_CALL_RETURN(entry, target) \
    GT_DECOMP_FORWARDER_(call_and_return, gt::ReturnKind::DoubleWord, entry, target, 0u)
#define GT_DECOMP_STORE_JUMP(entry, stored, target) \
    GT_DECOMP_FORWARDER_(store_and_jump, gt::ReturnKind::Word, entry, target, stored)

GT_DECOMP_CALL_RETURN(0x088A41FCu, 0x088A3844u);
GT_DECOMP_CALL_RETURN(0x08927BC8u, 0x0892074Cu);
GT_DECOMP_CALL_RETURN(0x08A9D890u, 0x08A9D2D8u);
GT_DECOMP_CALL_RETURN(0x08AE3B08u, 0x08AE3568u);
GT_DECOMP_CALL_RETURN(0x08AE8044u, 0x08AE74F4u);
GT_DECOMP_CALL_RETURN(0x08B0E9DCu, 0x08B113B4u);
GT_DECOMP_CALL_RETURN(0x08B19E84u, 0x08C4DA5Cu);
GT_DECOMP_CALL_RETURN(0x08B52CECu, 0x08B529C8u);
GT_DECOMP_CALL_RETURN(0x08B52D08u, 0x08B52A08u);
GT_DECOMP_CALL_RETURN(0x08B6461Cu, 0x08B62344u);
GT_DECOMP_CALL_RETURN(0x08B77D40u, 0x08B796C4u);
GT_DECOMP_CALL_RETURN(0x08B8C590u, 0x08AA2AFCu);
GT_DECOMP_CALL_RETURN(0x08C460A8u, 0x08C4605Cu);

GT_DECOMP_STORE_JUMP(0x08974124u, 0x08CBA718u, 0x08A780B0u);
GT_DECOMP_STORE_JUMP(0x089BA380u, 0x08CBC990u, 0x089FDE00u);
GT_DECOMP_STORE_JUMP(0x089C16C8u, 0x08CCEFF8u, 0x08A5BB5Cu);
GT_DECOMP_STORE_JUMP(0x089D6684u, 0x08CCF440u, 0x0897D548u);
GT_DECOMP_STORE_JUMP(0x089E4C98u, 0x08CCF8B0u, 0x089FE3F4u);
GT_DECOMP_STORE_JUMP(0x089FDE00u, 0x08CB8D28u, 0x08A780B0u);
GT_DECOMP_STORE_JUMP(0x089FE3F4u, 0x08CCFD98u, 0x08A5BB5Cu);
GT_DECOMP_STORE_JUMP(0x08A4DF28u, 0x08CD1EF8u, 0x08B362B8u);
GT_DECOMP_STORE_JUMP(0x08A72308u, 0x08CCD618u, 0x08A780B0u);
GT_DECOMP_STORE_JUMP(0x08A72654u, 0x08CD3990u, 0x08A5D638u);
GT_DECOMP_STORE_JUMP(0x08A8036Cu, 0x08CD4120u, 0x08A5D638u);
GT_DECOMP_STORE_JUMP(0x08A86D24u, 0x08CD42B8u, 0x08A5D638u);
GT_DECOMP_STORE_JUMP(0x08A982D4u, 0x08CD4730u, 0x08A5D638u);
GT_DECOMP_STORE_JUMP(0x08AB35D4u, 0x08CD5768u, 0x08A9E848u);
GT_DECOMP_STORE_JUMP(0x08B0C23Cu, 0x08CD7B68u, 0x08B0C1D8u);
GT_DECOMP_STORE_JUMP(0x08B0C24Cu, 0x08CD7B68u, 0x08B0C1D8u);
GT_DECOMP_STORE_JUMP(0x08B15944u, 0x08CD9070u, 0x08B0C23Cu);
GT_DECOMP_STORE_JUMP(0x08B24DB4u, 0x08CD9648u, 0x08B24D3Cu);
GT_DECOMP_STORE_JUMP(0x08B27FECu, 0x08CD9678u, 0x08B27270u);
GT_DECOMP_STORE_JUMP(0x08B37364u, 0x08CDA898u, 0x08B362B8u);
GT_DECOMP_STORE_JUMP(0x08B374FCu, 0x08CDA8B8u, 0x08B362B8u);
GT_DECOMP_STORE_JUMP(0x08C15D30u, 0x08CD04A0u, 0x08B24A0Cu);
GT_DECOMP_STORE_JUMP(0x08C3CCD0u, 0x08CD6790u, 0x08B24A0Cu);
