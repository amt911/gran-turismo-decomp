// Object factories the compiler emits once per class, one body per shape and a table with each
// instance's constants:
//   make_instance(out)        obj = allocate(size, align); construct(obj); return init(out, &obj);
//                             (a new object of the class, handed to the holder `out`)
//   assign_from_factory(out)  Holder tmp; make(&tmp); if (out != &tmp) holder_reset(out, tmp);
//                             holder_destroy(&tmp);
// Listed: the ones the race path calls (the harness compared them equal); the others stay
// recompiled until a run reaches them.

#include "decomp_registry.hpp"
#include "guest_call.hpp"

#include <unordered_map>

namespace gt::core {
namespace {

constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;    // (holder, object): src/core/refcount.cpp
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;  // (holder): src/core/refcount.cpp

struct Factory {
    std::uint32_t size;
    std::uint32_t align;
    std::uint32_t allocate;
    std::uint32_t construct;
    std::uint32_t init;
};

std::unordered_map<std::uint32_t, Factory> &factory_table() {
    static std::unordered_map<std::uint32_t, Factory> table;
    return table;
}

} // namespace

struct FactoryRegistrar {
    FactoryRegistrar(std::uint32_t entry, Factory factory) { factory_table()[entry] = factory; }
};

// make_instance(out) -> init(out, &obj) for a freshly allocated and constructed object.
void make_instance(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const Factory &f = factory_table().at(ctx.pc);
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t object = call_guest(rt, ctx, f.allocate, {f.size, f.align});
    call_guest(rt, ctx, f.construct, {object});
    rt.memory().store32(frame.at(0u), object);
    ctx.gpr[2] = call_guest(rt, ctx, f.init, {out, frame.at(0u)});
}

// assign_from_factory(out): out = a new object from the instance's make function.
void assign_from_factory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t make = factory_table().at(ctx.pc).init;
    const std::uint32_t out = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t tmp = frame.at(0u);
    call_guest(rt, ctx, make, {tmp});
    if (out != tmp) call_guest(rt, ctx, kHolderReset, {out, rt.memory().load32(tmp)});
    ctx.gpr[2] = call_guest(rt, ctx, kHolderDestroy, {tmp});
}

} // namespace gt::core

#define GT_DECOMP_MAKE(entry, size, align, allocate, construct, init)                                    \
    static const ::gt::core::FactoryRegistrar GT_DECOMP_CAT(gt_factory_, __LINE__){                      \
        (entry), {(size), (align), (allocate), (construct), (init)}};                                    \
    GT_DECOMP(entry, gt::core::make_instance, gt::ReturnKind::Word)
#define GT_DECOMP_ASSIGN_MAKE(entry, make)                                                               \
    static const ::gt::core::FactoryRegistrar GT_DECOMP_CAT(gt_factory_, __LINE__){(entry), {0u, 0u, 0u, 0u, (make)}}; \
    GT_DECOMP(entry, gt::core::assign_from_factory, gt::ReturnKind::Word)

GT_DECOMP_MAKE(0x0880A414u, 0x120u, 0x4u, 0x08A5BC20u, 0x0880A330u, 0x08A013D0u);
GT_DECOMP_MAKE(0x0880BC78u, 0xF0u, 0x4u, 0x08A5BC20u, 0x0880BC34u, 0x08969190u);
GT_DECOMP_MAKE(0x08824FC0u, 0x18u, 0x4u, 0x08A5BC20u, 0x088246C0u, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08854D34u, 0x1Cu, 0x4u, 0x08A5BC20u, 0x08854C74u, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08975B30u, 0xB8u, 0x4u, 0x08A5BC20u, 0x08975A54u, 0x089D8A60u);
GT_DECOMP_MAKE(0x08976AD4u, 0x150u, 0x4u, 0x08A5BC20u, 0x08976830u, 0x08993BF4u);
GT_DECOMP_MAKE(0x08976FF8u, 0x140u, 0x4u, 0x08A5BC20u, 0x08976D54u, 0x089FE8D8u);
GT_DECOMP_MAKE(0x0897DFBCu, 0x38u, 0x4u, 0x08A5BC20u, 0x0897D8D0u, 0x08A76AB4u);
GT_DECOMP_MAKE(0x0898302Cu, 0x240u, 0x4u, 0x08A5BC20u, 0x08982EA8u, 0x08969190u);
GT_DECOMP_MAKE(0x089920E0u, 0xA4u, 0x4u, 0x08A5BC20u, 0x08991B54u, 0x089FFC94u);
GT_DECOMP_MAKE(0x08993018u, 0x140u, 0x4u, 0x08A5BC20u, 0x08992FC0u, 0x08A0D868u);
GT_DECOMP_MAKE(0x08998758u, 0x190u, 0x4u, 0x08A5BC20u, 0x0899865Cu, 0x089948F4u);
GT_DECOMP_MAKE(0x0899A360u, 0x140u, 0x4u, 0x08A5BC20u, 0x0899A300u, 0x089FC5D0u);
GT_DECOMP_MAKE(0x0899BC8Cu, 0x48u, 0x4u, 0x08A5BC20u, 0x0899BC2Cu, 0x08973D98u);
GT_DECOMP_MAKE(0x0899E024u, 0x170u, 0x4u, 0x08A5BC20u, 0x0899D908u, 0x089A274Cu);
GT_DECOMP_MAKE(0x089A1E78u, 0xB8u, 0x4u, 0x08A5BC20u, 0x089A1DA4u, 0x089D8A60u);
GT_DECOMP_MAKE(0x089A2954u, 0x120u, 0x4u, 0x08A5BC20u, 0x089A2564u, 0x08A0D868u);
GT_DECOMP_MAKE(0x089A5CF0u, 0x190u, 0x4u, 0x08A5BC20u, 0x089A5BCCu, 0x089A274Cu);
GT_DECOMP_MAKE(0x089B4108u, 0x130u, 0x4u, 0x08A5BC20u, 0x089B4098u, 0x08A013D0u);
GT_DECOMP_MAKE(0x089B55E8u, 0xA0u, 0x4u, 0x08A5BC20u, 0x089B5048u, 0x089FFC94u);
GT_DECOMP_MAKE(0x089B67FCu, 0x180u, 0x4u, 0x08A5BC20u, 0x089B6698u, 0x089A274Cu);
GT_DECOMP_MAKE(0x089CC6C0u, 0x18u, 0x4u, 0x08A5BC20u, 0x089CC678u, 0x08A5BAB8u);
GT_DECOMP_MAKE(0x089D0860u, 0x1D0u, 0x4u, 0x08A5BC20u, 0x089D0704u, 0x0899537Cu);
GT_DECOMP_MAKE(0x089DE920u, 0x1C0u, 0x4u, 0x08A5BC20u, 0x089DE7D4u, 0x08969190u);
GT_DECOMP_MAKE(0x089E2350u, 0x230u, 0x4u, 0x08A5BC20u, 0x089E2210u, 0x08A0815Cu);
GT_DECOMP_MAKE(0x089E8C54u, 0x1F0u, 0x4u, 0x08A5BC20u, 0x089E8ADCu, 0x0899AE08u);
GT_DECOMP_MAKE(0x089ED2C4u, 0x150u, 0x4u, 0x08A5BC20u, 0x089ED020u, 0x08993BF4u);
GT_DECOMP_MAKE(0x089ED7E8u, 0x140u, 0x4u, 0x08A5BC20u, 0x089ED544u, 0x089FE8D8u);
GT_DECOMP_MAKE(0x089EFBF0u, 0xE70u, 0x4u, 0x08A5BC20u, 0x089EFB10u, 0x089E119Cu);
GT_DECOMP_MAKE(0x089F0AA4u, 0x28u, 0x4u, 0x08A5BC20u, 0x089F0A28u, 0x08A5BAB8u);
GT_DECOMP_MAKE(0x089F3C40u, 0x280u, 0x4u, 0x08A5BC20u, 0x089F3B58u, 0x089E119Cu);
GT_DECOMP_MAKE(0x089F9780u, 0xB8u, 0x4u, 0x08A5BC20u, 0x089F967Cu, 0x089D8A60u);
GT_DECOMP_MAKE(0x089FC6C0u, 0x120u, 0x4u, 0x08A5BC20u, 0x089FB954u, 0x08A0D868u);
GT_DECOMP_MAKE(0x08A02AB8u, 0x120u, 0x4u, 0x08A5BC20u, 0x08A02A24u, 0x08969190u);
GT_DECOMP_MAKE(0x08A0387Cu, 0x120u, 0x4u, 0x08A5BC20u, 0x08A035DCu, 0x08A013D0u);
GT_DECOMP_MAKE(0x08A066A0u, 0xB8u, 0x4u, 0x08A5BC20u, 0x08A065A0u, 0x089D8A60u);
GT_DECOMP_MAKE(0x08A08E08u, 0x1B0u, 0x4u, 0x08A5BC20u, 0x08A08D14u, 0x08969190u);
GT_DECOMP_MAKE(0x08A0EE4Cu, 0x20u, 0x4u, 0x08A5BC20u, 0x08A0E8D0u, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08A22304u, 0x18u, 0x4u, 0x08A5BC20u, 0x08A21E6Cu, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08A32A94u, 0x430u, 0x4u, 0x08A5BC20u, 0x08A32A40u, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08A410C8u, 0x20u, 0x4u, 0x08A5BC20u, 0x08A41088u, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08A431F8u, 0x48u, 0x4u, 0x08A5BC20u, 0x08A43180u, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08A62AB8u, 0x5Cu, 0x4u, 0x08A5BC20u, 0x08A629FCu, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08A813F4u, 0x34u, 0x4u, 0x08A5BC20u, 0x08A81380u, 0x08A76AB4u);
GT_DECOMP_MAKE(0x08A941CCu, 0x28u, 0x4u, 0x08A5BC20u, 0x08A9402Cu, 0x08A71E10u);
GT_DECOMP_MAKE(0x08A98A70u, 0x24u, 0x4u, 0x08A5BC20u, 0x08A98A2Cu, 0x08A76AB4u);

GT_DECOMP_ASSIGN_MAKE(0x0880A464u, 0x0880A414u);
GT_DECOMP_ASSIGN_MAKE(0x0880FE54u, 0x0880FE04u);
GT_DECOMP_ASSIGN_MAKE(0x0881CD84u, 0x0881CCE4u);
GT_DECOMP_ASSIGN_MAKE(0x0882D9B8u, 0x0882D918u);
GT_DECOMP_ASSIGN_MAKE(0x08835DDCu, 0x08835D30u);
GT_DECOMP_ASSIGN_MAKE(0x08837A2Cu, 0x08837980u);
GT_DECOMP_ASSIGN_MAKE(0x08845494u, 0x08845444u);
GT_DECOMP_ASSIGN_MAKE(0x08846234u, 0x088461E4u);
GT_DECOMP_ASSIGN_MAKE(0x08848E78u, 0x08848BB0u);
GT_DECOMP_ASSIGN_MAKE(0x0884CB58u, 0x0884CB08u);
GT_DECOMP_ASSIGN_MAKE(0x0884F47Cu, 0x0884F42Cu);
GT_DECOMP_ASSIGN_MAKE(0x08855698u, 0x08854D34u);
GT_DECOMP_ASSIGN_MAKE(0x08975B80u, 0x08975B30u);
GT_DECOMP_ASSIGN_MAKE(0x08976B24u, 0x08976AD4u);
GT_DECOMP_ASSIGN_MAKE(0x08977048u, 0x08976FF8u);
GT_DECOMP_ASSIGN_MAKE(0x0898307Cu, 0x0898302Cu);
GT_DECOMP_ASSIGN_MAKE(0x08992130u, 0x089920E0u);
GT_DECOMP_ASSIGN_MAKE(0x08993068u, 0x08993018u);
GT_DECOMP_ASSIGN_MAKE(0x08994B2Cu, 0x08994ADCu);
GT_DECOMP_ASSIGN_MAKE(0x089987A8u, 0x08998758u);
GT_DECOMP_ASSIGN_MAKE(0x0899A3B0u, 0x0899A360u);
GT_DECOMP_ASSIGN_MAKE(0x0899BCDCu, 0x0899BC8Cu);
GT_DECOMP_ASSIGN_MAKE(0x089A1EC8u, 0x089A1E78u);
GT_DECOMP_ASSIGN_MAKE(0x089A29A4u, 0x089A2954u);
GT_DECOMP_ASSIGN_MAKE(0x089A5D40u, 0x089A5CF0u);
GT_DECOMP_ASSIGN_MAKE(0x089B4158u, 0x089B4108u);
GT_DECOMP_ASSIGN_MAKE(0x089B4D10u, 0x089B4CC0u);
GT_DECOMP_ASSIGN_MAKE(0x089B5638u, 0x089B55E8u);
GT_DECOMP_ASSIGN_MAKE(0x089B6B2Cu, 0x089B67FCu);
GT_DECOMP_ASSIGN_MAKE(0x089BFEB0u, 0x089BECE4u);
GT_DECOMP_ASSIGN_MAKE(0x089CA680u, 0x089C9C34u);
GT_DECOMP_ASSIGN_MAKE(0x089CBB98u, 0x089CBB48u);
GT_DECOMP_ASSIGN_MAKE(0x089D0BA0u, 0x089D0860u);
GT_DECOMP_ASSIGN_MAKE(0x089DE970u, 0x089DE920u);
GT_DECOMP_ASSIGN_MAKE(0x089E23A0u, 0x089E2350u);
GT_DECOMP_ASSIGN_MAKE(0x089ED314u, 0x089ED2C4u);
GT_DECOMP_ASSIGN_MAKE(0x089ED838u, 0x089ED7E8u);
GT_DECOMP_ASSIGN_MAKE(0x089F3C90u, 0x089F3C40u);
GT_DECOMP_ASSIGN_MAKE(0x089F6EA0u, 0x089F6E50u);
GT_DECOMP_ASSIGN_MAKE(0x089F97D0u, 0x089F9780u);
GT_DECOMP_ASSIGN_MAKE(0x089FC710u, 0x089FC6C0u);
GT_DECOMP_ASSIGN_MAKE(0x08A02B08u, 0x08A02AB8u);
GT_DECOMP_ASSIGN_MAKE(0x08A038CCu, 0x08A0387Cu);
GT_DECOMP_ASSIGN_MAKE(0x08A066F0u, 0x08A066A0u);
GT_DECOMP_ASSIGN_MAKE(0x08A08E58u, 0x08A08E08u);
GT_DECOMP_ASSIGN_MAKE(0x08A0EE9Cu, 0x08A0EE4Cu);
GT_DECOMP_ASSIGN_MAKE(0x08A14940u, 0x08A14778u);
GT_DECOMP_ASSIGN_MAKE(0x08A1C020u, 0x08A1BFD0u);
GT_DECOMP_ASSIGN_MAKE(0x08A1EB54u, 0x08A1EB04u);
GT_DECOMP_ASSIGN_MAKE(0x08A20E80u, 0x08A20B40u);
GT_DECOMP_ASSIGN_MAKE(0x08A32AE4u, 0x08A32A94u);
GT_DECOMP_ASSIGN_MAKE(0x08A36CECu, 0x08A36C3Cu);
GT_DECOMP_ASSIGN_MAKE(0x08A370FCu, 0x08A370ACu);
GT_DECOMP_ASSIGN_MAKE(0x08A43248u, 0x08A431F8u);
GT_DECOMP_ASSIGN_MAKE(0x08A81508u, 0x08A813F4u);
GT_DECOMP_ASSIGN_MAKE(0x08B88FB8u, 0x0880BC78u);
GT_DECOMP_ASSIGN_MAKE(0x08C1AA40u, 0x08A30A18u);
