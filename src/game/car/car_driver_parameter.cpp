// CarDriverParameter: the engine side and the natives of MCarDriverParameter, how one car is driven —
// human or AI, the driver aids, the AI level (layout and evidence: include/gt/game/car_driver_parameter.hpp;
// names from GT's registration, config/symbol_addrs.txt).
//
// Engine functions take the CarDriverParameterHandle (the engine object the script wraps) in a0 and work on
// the data its first word points to. Attribute accessors are named after the attribute (`manual` reads,
// `set_manual` writes); the rest after the method whose native calls them. Registered functions replace the
// recompiled originals; the ones commented out at the end were not compared in any fast, dense or full run
// of the PR, so the gate can't verify them.
//
// Modding: the driver aids and the AI level a car gets are decided here; a mod that wants, say, every
// driver to have the traction-control aid, or a different AI range, changes the setters (set_assist_tcs,
// set_ai_level). Keep the original behaviour for the differential gate and make such changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/car_driver_parameter.hpp"
#include "guest_call.hpp"
#include "../adhoc/natives.hpp"

#include <cstdint>

namespace gt::game::car_driver_parameter {
namespace {

using guest::View;
using namespace gt::adhoc;

constexpr std::uint32_t kSelfAsCarDriverParameter = 0x0884D660u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kCarDriverParameterCore = 0x0884D3D8u;

std::uint32_t word(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load32(address); }

// The data of the handle in a0.
View<CarDriverParameterData> data_of(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx) {
    return *View<CarDriverParameterHandle>(rt.memory(), ctx.gpr[4]).data();
}

// A flag's getter answers whether the signed byte is positive.
std::uint32_t positive(std::int8_t value) { return value > 0 ? 1u : 0u; }
std::uint32_t sign_extended(std::int8_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value)); }

// A percentage as GT stores it: clamped to 0..100, plus one.
std::uint8_t percent_plus_one(std::uint32_t value) {
    const auto percent = static_cast<std::int32_t>(value);
    const std::uint32_t clamped =
        percent > 100 ? 100u : percent < 0 ? 0u : static_cast<std::uint32_t>(static_cast<std::int32_t>(static_cast<std::int8_t>(percent)));
    return static_cast<std::uint8_t>(clamped + 1u);
}
// An AI skill value: clamped to 10..200.
std::uint8_t skill(std::uint32_t value) {
    const auto v = static_cast<std::int32_t>(value);
    return static_cast<std::uint8_t>(v < 10 ? 10u : v > 200 ? 200u : (value & 255u));
}

} // namespace

// --- Script attributes: one setter (a1 = value) and one getter (-> v0) each -------------------------

void set_manual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).manual() = static_cast<std::int8_t>(ctx.gpr[5]); }
void manual(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = positive(data_of(rt, ctx).manual()); }
void set_assist_tcs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).assist_tcs() = static_cast<std::int8_t>(ctx.gpr[5]); }
void assist_tcs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = positive(data_of(rt, ctx).assist_tcs()); }
void set_assist_asm(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).assist_asm() = static_cast<std::int8_t>(ctx.gpr[5]); }
void assist_asm(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = positive(data_of(rt, ctx).assist_asm()); }
void set_steering_assist_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data_of(rt, ctx).steering_assist_type() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void steering_assist_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(data_of(rt, ctx).steering_assist_type());
}
void set_active_steering(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data_of(rt, ctx).active_steering() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void active_steering(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sign_extended(data_of(rt, ctx).active_steering()); }
void set_active_brake_level(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data_of(rt, ctx).active_brake_level() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void active_brake_level(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(data_of(rt, ctx).active_brake_level());
}
void set_physics_pro(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).physics_pro() = static_cast<std::int8_t>(ctx.gpr[5]); }
void physics_pro(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = positive(data_of(rt, ctx).physics_pro()); }
void set_academy_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).academy_flag() = static_cast<std::int8_t>(ctx.gpr[5]); }
void academy_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = positive(data_of(rt, ctx).academy_flag()); }
void set_assist_4was(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).assist_4was() = static_cast<std::int8_t>(ctx.gpr[5]); }
void assist_4was(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = positive(data_of(rt, ctx).assist_4was()); }

// type: the setter leaves the data address in v0 (the original's last load).
void set_type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarDriverParameterData> data = data_of(rt, ctx);
    data.type() = ctx.gpr[5];
    ctx.gpr[2] = data.address();
}
void type(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).type(); }

// special_driver_model_identity: a value below 7 goes to unk_B0, a larger one to the attribute's own byte.
void set_special_driver_model_identity(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarDriverParameterData> data = data_of(rt, ctx);
    const auto v = static_cast<std::int32_t>(ctx.gpr[5]);
    if (v < 7) {
        data.special_driver_model_identity() = 0;
        data.unk_B0() = static_cast<std::int8_t>(v);
    } else {
        data.special_driver_model_identity() = static_cast<std::int8_t>(v);
        data.unk_B0() = 0;
    }
}
void special_driver_model_identity(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarDriverParameterData> data = data_of(rt, ctx);
    std::int8_t value = data.special_driver_model_identity();
    if (value <= 0) value = data.unk_B0();
    ctx.gpr[2] = sign_extended(value);
}

// --- Methods -----------------------------------------------------------------------------------------

// setAISkill(first, second): both clamped to 10..200; the AI bytes around them are cleared.
void set_ai_skill(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarDriverParameterData> data = data_of(rt, ctx);
    const std::uint8_t first = skill(ctx.gpr[5]), second = skill(ctx.gpr[6]);
    data.unk_AB() = first;
    data.unk_A8() = second;
    const std::uint32_t base = data.address() + 0xA8u;
    for (const std::uint32_t at : {7u, 1u, 2u, 4u, 5u, 6u}) rt.memory().store8(base + at, 0u);
}
// setAILevel(level)
void set_ai_level(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).ai_level() = percent_plus_one(ctx.gpr[5]); }
// isHuman: the type is 0.
void is_human(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).type() == 0u ? 1u : 0u; }
// setRTAUnadjustable(v) / getRTAUnadjustable
void set_rta_unadjustable(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarDriverParameterData> data = data_of(rt, ctx);
    data.rta_unadjustable() = static_cast<std::uint8_t>(ctx.gpr[5] | data.rta_unadjustable());
}
void rta_unadjustable(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).rta_unadjustable(); }
// setPlayer(number): the driver is human (type 0) with that number.
void set_player(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarDriverParameterData> data = data_of(rt, ctx);
    data.type() = 0u;
    data.user_number() = ctx.gpr[5];
}
// getControllerPort: the user number of a human driver, else -1.
void get_controller_port(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarDriverParameterData> data = data_of(rt, ctx);
    ctx.gpr[2] = data.type() != 0u ? 0xFFFFFFFFu : data.user_number().value();
}
// setDefault: clears the driver's settings (140 bytes), marks it as having no player (type -1) and resets the
// AI bytes, then hands the data to func_08896D94.
void set_default(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    const std::uint32_t block = word(rt, self);
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08B36018u, {block, 0u, 140u});
        (void)call_guest(rt, ctx, 0x0894BEFCu, {block + 140u});
    }
    rt.memory().store32(block + 156u, 0xFFFFFFFFu);
    rt.memory().store8(block + 176u, 0u);
    rt.memory().store32(block + 160u, 0u);
    rt.memory().store8(block + 183u, 0u);
    rt.memory().store32(block + 172u, 0u);
    rt.memory().store32(block + 164u, 0u);
    rt.memory().store32(block + 168u, 0u);
    for (std::uint32_t at = 177u; at <= 182u; ++at) rt.memory().store8(block + at, 0u);
    tail_call(ctx, 0x08896D94u, {self + 0x8u, block + 160u});
}

// --- Natives --------------------------------------------------------------------------------------------

namespace {
// The engine object of self (a1), the holder dropped.
std::uint32_t core_of_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return self_core(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore);
}
} // namespace

// isHuman() -> boolean
void MCarDriverParameter_isHuman(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = core_of_self(rt, ctx);
    const std::uint32_t human = call_guest(rt, ctx, 0x08897788u, {core});
    GuestFrame frame(ctx, 0x10u);
    return_bool(rt, ctx, result, frame.at(0u), human);
}
// setAILevel(level), with at least one argument.
void MCarDriverParameter_setAILevel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = core_of_self(rt, ctx);
    (void)call_guest(rt, ctx, 0x08897290u, {core, int_argument(rt, ctx, argv, 0u)});
}
// setPlayer(number), with at least one argument.
void MCarDriverParameter_setPlayer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = core_of_self(rt, ctx);
    (void)call_guest(rt, ctx, 0x08897554u, {core, int_argument(rt, ctx, argv, 0u)});
}
// setDefault()
void MCarDriverParameter_setDefault(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t core = core_of_self(rt, ctx);
    (void)call_guest(rt, ctx, 0x0889793Cu, {core});
}
// setAISkill(first, second), with at least two arguments.
void MCarDriverParameter_setAISkill(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc < 2) return;
    const std::uint32_t core = core_of_self(rt, ctx);
    const std::uint32_t first = int_argument(rt, ctx, argv, 0u);
    const std::uint32_t second = int_argument(rt, ctx, argv, 1u);
    (void)call_guest(rt, ctx, 0x0889722Cu, {core, first, second});
}
// setRTAUnadjustable(v), with at least one argument.
void MCarDriverParameter_setRTAUnadjustable(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = core_of_self(rt, ctx);
    (void)call_guest(rt, ctx, 0x088970A0u, {core, int_argument(rt, ctx, argv, 0u)});
}
// getRTAUnadjustable() -> integer
void MCarDriverParameter_getRTAUnadjustable(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = core_of_self(rt, ctx);
    return_int(rt, ctx, result, call_guest(rt, ctx, 0x08897094u, {core}));
}

} // namespace gt::game::car_driver_parameter

namespace car_driver_parameter_registrations {
using gt::ReturnKind;
#define CAR_DRIVER_PARAMETER(address, name, kind) GT_DECOMP(address, gt::game::car_driver_parameter::name, ReturnKind::kind)

CAR_DRIVER_PARAMETER(0x08896FD0u, set_manual, Void);
CAR_DRIVER_PARAMETER(0x08896FC0u, manual, Word);
CAR_DRIVER_PARAMETER(0x08896FECu, set_assist_tcs, Void);
CAR_DRIVER_PARAMETER(0x08897008u, set_assist_asm, Void);
CAR_DRIVER_PARAMETER(0x08896FF8u, assist_asm, Word);
CAR_DRIVER_PARAMETER(0x08897054u, set_active_steering, Void);
CAR_DRIVER_PARAMETER(0x08897048u, active_steering, Word);
CAR_DRIVER_PARAMETER(0x08897024u, set_physics_pro, Void);
CAR_DRIVER_PARAMETER(0x08897014u, physics_pro, Word);
CAR_DRIVER_PARAMETER(0x08897574u, set_type, Word);
CAR_DRIVER_PARAMETER(0x088977D4u, type, Word);
CAR_DRIVER_PARAMETER(0x08897290u, set_ai_level, Void);
CAR_DRIVER_PARAMETER(0x08897788u, is_human, Word);
CAR_DRIVER_PARAMETER(0x08897554u, set_player, Void);
CAR_DRIVER_PARAMETER(0x0889779Cu, get_controller_port, Word);
CAR_DRIVER_PARAMETER(0x0889793Cu, set_default, Void);
CAR_DRIVER_PARAMETER(0x0884D81Cu, MCarDriverParameter_isHuman, Void);
CAR_DRIVER_PARAMETER(0x0884D77Cu, MCarDriverParameter_setAILevel, Void);
CAR_DRIVER_PARAMETER(0x0884D950u, MCarDriverParameter_setPlayer, Void);
CAR_DRIVER_PARAMETER(0x0884D9F0u, MCarDriverParameter_setDefault, Void);
// Not registered: compared 0 in every fast, dense and full run of the PR (the pad routes never call them on their own,
// and the harness replays nested calls), so nothing could verify them; they run as the recompiled originals.
// CAR_DRIVER_PARAMETER(0x08896FDCu, assist_tcs, Word);
// CAR_DRIVER_PARAMETER(0x0889703Cu, set_steering_assist_type, Void);
// CAR_DRIVER_PARAMETER(0x08897030u, steering_assist_type, Word);
// CAR_DRIVER_PARAMETER(0x08897088u, set_active_brake_level, Void);
// CAR_DRIVER_PARAMETER(0x0889707Cu, active_brake_level, Word);
// CAR_DRIVER_PARAMETER(0x088970C8u, set_academy_flag, Void);
// CAR_DRIVER_PARAMETER(0x088970B8u, academy_flag, Word);
// CAR_DRIVER_PARAMETER(0x08897070u, set_assist_4was, Void);
// CAR_DRIVER_PARAMETER(0x08897060u, assist_4was, Word);
// CAR_DRIVER_PARAMETER(0x08897130u, set_special_driver_model_identity, Void);
// CAR_DRIVER_PARAMETER(0x08897118u, special_driver_model_identity, Word);
// CAR_DRIVER_PARAMETER(0x0889722Cu, set_ai_skill, Void);
// CAR_DRIVER_PARAMETER(0x088970A0u, set_rta_unadjustable, Void);
// CAR_DRIVER_PARAMETER(0x08897094u, rta_unadjustable, Word);
// CAR_DRIVER_PARAMETER(0x0884D6A0u, MCarDriverParameter_setAISkill, Void);
// CAR_DRIVER_PARAMETER(0x0884D8B0u, MCarDriverParameter_setRTAUnadjustable, Void);
// CAR_DRIVER_PARAMETER(0x0884E468u, MCarDriverParameter_getRTAUnadjustable, Void);
#undef CAR_DRIVER_PARAMETER
} // namespace car_driver_parameter_registrations
