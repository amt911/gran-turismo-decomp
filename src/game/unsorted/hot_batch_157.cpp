// Hot batch 157: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_157 {
namespace {

using namespace gt::game::hot;

// register_object(object, a, b) with the member's vtable and registry: object = {vtable, 0, a, b}; then
// func_08B18DC4(registry(), object, b) (tail call; v0 stays the registry's).
void register_object(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t vtable, std::uint32_t registry) {
    const std::uint32_t object = ctx.gpr[4];
    ctx.gpr[2] = vtable;
    put(rt, object + 8u, ctx.gpr[5]);
    put(rt, object, vtable);
    put(rt, object + 0xCu, ctx.gpr[6]);
    put(rt, object + 4u, 0u);
    std::uint32_t found;
    {
        GuestFrame frame(ctx, 0x10u);
        const GuestResult result = call_guest(rt, ctx, registry, {}, {});
        ctx.gpr[2] = result.v0;
        ctx.gpr[3] = result.v1;
        ctx.fpr[0] = result.f0;
        found = result.v0;
    }
    tail_call(ctx, 0x08B18DC4u, {found, object});
}

} // namespace

// Templates of their families (pending members; the other members are the batch 157 rows below).
void func_08B9F91C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0ED0u, 0x08B9D090u); }

// Batch 157 rows (tools.family_rows).
void func_08B9FA0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0F20u, 0x08B9BCB8u); }
void func_08B9FAFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0E10u, 0x08B9D090u); }
void func_08B9FBEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0D50u, 0x08B9D2BCu); }
void func_08B9FE00(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0C40u, 0x08B9D4E8u); }
void func_08B9FF6C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0810u, 0x08B9D714u); }
void func_08B9FFD0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0610u, 0x08B9D958u); }
void func_08BA0034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0320u, 0x08B9DB84u); }
void func_08BA0098(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0218u, 0x08B9E208u); }
void func_08BA00FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0158u, 0x08B9E47Cu); }
void func_08BA01CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0070u, 0x08B9E6A8u); }
void func_08BA1454(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0728u, 0x08B9D958u); }
void func_08BA1544(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC06D8u, 0x08B9D958u); }
void func_08BA1634(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0688u, 0x08B9D958u); }
void func_08BA1920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0CB8u, 0x08B9E208u); }
void func_08BA1A10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0BA8u, 0x08B9E208u); }
void func_08BA1A74(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0778u, 0x08B9E208u); }
void func_08BA1B64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0578u, 0x08B9E208u); }
void func_08BA1DE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0528u, 0x08B9DB84u); }
void func_08BA1ED0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC04D8u, 0x08B9DB84u); }
void func_08BA1FC0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0488u, 0x08B9DB84u); }
void func_08BA20B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0438u, 0x08B9DB84u); }
void func_08BA21A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC03E8u, 0x08B9DB84u); }
void func_08BA2290(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0398u, 0x08B9DB84u); }
void func_08BA2CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0B58u, 0x08B9D714u); }
void func_08BA2DC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0B08u, 0x08B9D714u); }
void func_08BA2EB4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0AB8u, 0x08B9D714u); }
void func_08BA2FA4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0A68u, 0x08B9D714u); }
void func_08BA3094(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0A18u, 0x08B9D714u); }
void func_08BA3184(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC09C8u, 0x08B9D714u); }
void func_08BA3288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0978u, 0x08B9D714u); }
void func_08BA338C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0928u, 0x08B9D714u); }
void func_08BA3490(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC08D8u, 0x08B9D714u); }
void func_08BA3594(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC0888u, 0x08B9D714u); }
void func_08BB976C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC3DA0u, 0x08BB964Cu); }
void func_08BB9980(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC3CE0u, 0x08BB9860u); }
void func_08BC520C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4D18u, 0x08BC50ACu); }
void func_08BC536C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4D90u, 0x08BC50ACu); }
void func_08BC543C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4DE0u, 0x08BC50ACu); }
void func_08BC550C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4E30u, 0x08BC50ACu); }
void func_08BC5A9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4988u, 0x08BC593Cu); }
void func_08BC5C10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4A00u, 0x08BC593Cu); }
void func_08BC5F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC45A8u, 0x08BC5DA8u); }
void func_08BC6178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4470u, 0x08BC6018u); }
void func_08BCB518(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC48A0u, 0x08BC5DA8u); }
void func_08BCB608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4850u, 0x08BC5DA8u); }
void func_08BCB6F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4800u, 0x08BC5DA8u); }
void func_08BCB7E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC47B0u, 0x08BC5DA8u); }
void func_08BCB8D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4760u, 0x08BC5DA8u); }
void func_08BCB9C8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4710u, 0x08BC5DA8u); }
void func_08BCBAB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC46C0u, 0x08BC5DA8u); }
void func_08BCBBA8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4670u, 0x08BC5DA8u); }
void func_08BCBC98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4620u, 0x08BC5DA8u); }
void func_08BCC088(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4E80u, 0x08BBD034u); }
void func_08BCC178(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4C80u, 0x08BBD034u); }
void func_08BCC268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4C30u, 0x08BBD034u); }
void func_08BCC358(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4BE0u, 0x08BBD034u); }
void func_08BCC448(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4B90u, 0x08BBD034u); }
void func_08BCC538(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4B40u, 0x08BBD034u); }
void func_08BCC628(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4AF0u, 0x08BBD034u); }
void func_08BCC718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4AA0u, 0x08BBD034u); }
void func_08BCC808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC4A50u, 0x08BBD034u); }
void func_08BCC8F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { register_object(rt, ctx, 0x08CC48F0u, 0x08BBD034u); }

} // namespace hot_batch_157
} // namespace gt::game

#define HOT157(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_157::name, gt::ReturnKind::kind)
HOT157(0x08B9F91Cu, func_08B9F91C, Word);
HOT157(0x08B9FA0Cu, func_08B9FA0C, Word);
HOT157(0x08B9FAFCu, func_08B9FAFC, Word);
HOT157(0x08B9FBECu, func_08B9FBEC, Word);
HOT157(0x08B9FE00u, func_08B9FE00, Word);
HOT157(0x08B9FF6Cu, func_08B9FF6C, Word);
HOT157(0x08B9FFD0u, func_08B9FFD0, Word);
HOT157(0x08BA0034u, func_08BA0034, Word);
HOT157(0x08BA0098u, func_08BA0098, Word);
HOT157(0x08BA00FCu, func_08BA00FC, Word);
HOT157(0x08BA01CCu, func_08BA01CC, Word);
HOT157(0x08BA1454u, func_08BA1454, Word);
HOT157(0x08BA1544u, func_08BA1544, Word);
HOT157(0x08BA1634u, func_08BA1634, Word);
HOT157(0x08BA1920u, func_08BA1920, Word);
HOT157(0x08BA1A10u, func_08BA1A10, Word);
HOT157(0x08BA1A74u, func_08BA1A74, Word);
HOT157(0x08BA1B64u, func_08BA1B64, Word);
HOT157(0x08BA1DE0u, func_08BA1DE0, Word);
HOT157(0x08BA1ED0u, func_08BA1ED0, Word);
HOT157(0x08BA1FC0u, func_08BA1FC0, Word);
HOT157(0x08BA20B0u, func_08BA20B0, Word);
HOT157(0x08BA21A0u, func_08BA21A0, Word);
HOT157(0x08BA2290u, func_08BA2290, Word);
HOT157(0x08BA2CD4u, func_08BA2CD4, Word);
HOT157(0x08BA2DC4u, func_08BA2DC4, Word);
HOT157(0x08BA2EB4u, func_08BA2EB4, Word);
HOT157(0x08BA2FA4u, func_08BA2FA4, Word);
HOT157(0x08BA3094u, func_08BA3094, Word);
HOT157(0x08BA3184u, func_08BA3184, Word);
HOT157(0x08BA3288u, func_08BA3288, Word);
HOT157(0x08BA338Cu, func_08BA338C, Word);
HOT157(0x08BA3490u, func_08BA3490, Word);
HOT157(0x08BA3594u, func_08BA3594, Word);
HOT157(0x08BB976Cu, func_08BB976C, Word);
HOT157(0x08BB9980u, func_08BB9980, Word);
HOT157(0x08BC520Cu, func_08BC520C, Word);
HOT157(0x08BC536Cu, func_08BC536C, Word);
HOT157(0x08BC543Cu, func_08BC543C, Word);
HOT157(0x08BC550Cu, func_08BC550C, Word);
HOT157(0x08BC5A9Cu, func_08BC5A9C, Word);
HOT157(0x08BC5C10u, func_08BC5C10, Word);
HOT157(0x08BC5F08u, func_08BC5F08, Word);
HOT157(0x08BC6178u, func_08BC6178, Word);
HOT157(0x08BCB518u, func_08BCB518, Word);
HOT157(0x08BCB608u, func_08BCB608, Word);
HOT157(0x08BCB6F8u, func_08BCB6F8, Word);
HOT157(0x08BCB7E8u, func_08BCB7E8, Word);
HOT157(0x08BCB8D8u, func_08BCB8D8, Word);
HOT157(0x08BCB9C8u, func_08BCB9C8, Word);
HOT157(0x08BCBAB8u, func_08BCBAB8, Word);
HOT157(0x08BCBBA8u, func_08BCBBA8, Word);
HOT157(0x08BCBC98u, func_08BCBC98, Word);
HOT157(0x08BCC088u, func_08BCC088, Word);
HOT157(0x08BCC178u, func_08BCC178, Word);
HOT157(0x08BCC268u, func_08BCC268, Word);
HOT157(0x08BCC358u, func_08BCC358, Word);
HOT157(0x08BCC448u, func_08BCC448, Word);
HOT157(0x08BCC538u, func_08BCC538, Word);
HOT157(0x08BCC628u, func_08BCC628, Word);
HOT157(0x08BCC718u, func_08BCC718, Word);
HOT157(0x08BCC808u, func_08BCC808, Word);
HOT157(0x08BCC8F8u, func_08BCC8F8, Word);
