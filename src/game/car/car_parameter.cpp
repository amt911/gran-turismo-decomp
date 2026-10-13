// CarParameter: the engine side of MCarParameter, the description of one car the Adhoc scripts pass
// around (layout and evidence: include/gt/game/car_parameter.hpp; design:
// docs/superpowers/specs/2026-10-13-named-subsystem-race-parameter-design.md).
//
// Every function takes the CarParameterHandle (the engine object the script wraps) in a0 and works on
// the CarParameterData its first word points to. Names are GT's: the script attribute or method whose
// handler calls the function (config/symbol_addrs.txt lists each address with that evidence). The ones
// that two or more handlers share, or whose role is only clear from the code, say so in their comment.
// Functions without a script name keep func_XXXXXXXX.
//
// Registered functions replace the recompiled originals in the game; the ones listed as not registered at
// the end aren't called by the pad routes, so the gate can't verify them, and they don't run.
//
// Modding: this is where a car's parameters are read and written; a mod that wants, say, every car to
// report another PP or weight can change the getters. Keep the original behaviour for the differential
// gate and make such changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/car_parameter.hpp"
#include "guest_call.hpp"

#include <bit>
#include <cstdint>

namespace gt::game::car_parameter {
namespace {

using guest::View;

// The CarParameterData of the handle in a0.
View<CarParameterData> data(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx) {
    return *View<CarParameterHandle>(rt.memory(), ctx.gpr[4]).data();
}

// The data of the handle at `address` (a0 of the original is not always the first argument here).
View<CarParameterData> data_of(psprecomp::Runtime &rt, std::uint32_t handle) {
    return *View<CarParameterHandle>(rt.memory(), handle).data();
}

// The block the data selects with unk_08 (+0x08) (a signed byte: a negative index goes below the data).
View<CarBlock> selected_block(const View<CarParameterData> &car) {
    return car.blocks(static_cast<std::uint32_t>(static_cast<std::int32_t>(car.unk_08().value())));
}

std::uint32_t word(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load32(address); }

std::uint32_t sign_extended(std::int8_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value)); }
std::uint32_t sign_extended(std::int16_t value) { return static_cast<std::uint32_t>(static_cast<std::int32_t>(value)); }

} // namespace

// --- Script attributes: one setter (a1 = value) and one getter (-> v0) each -------------------------

// target: the original's getter answers whether the byte is non-zero.
void set_target(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).target() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void target(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarParameterData> car = data(rt, ctx);
    ctx.gpr[2] = car.target().value() != 0 ? 1u : 0u;
}

void set_team_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data(rt, ctx).team_id() = static_cast<std::int8_t>(ctx.gpr[5]); }
void team_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = sign_extended(data(rt, ctx).team_id().value()); }

void set_race_class_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).race_class_id() = static_cast<std::int8_t>(ctx.gpr[5]);
}
void race_class_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(data(rt, ctx).race_class_id().value());
}

void set_quality_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data(rt, ctx).quality_flag() = ctx.gpr[5]; }
void quality_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).quality_flag(); }

void set_obtain_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    data(rt, ctx).obtain_id() = static_cast<std::uint16_t>(ctx.gpr[5]);
}
void obtain_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).obtain_id(); }

// color: set_color hands the colour to func_08932748 for each of the three blocks; color reads it back
// from the selected block through func_08932790 (tail call).
void set_color(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    const std::uint32_t x = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    const View<CarParameterData> car = data_of(rt, handle);
    for (std::uint32_t i = 0; i != CarParameterData::blocks_count; ++i) {
        (void)call_guest(rt, ctx, 0x08932748u, {car.blocks(i).address(), x});
    }
}
void color(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08932790u, {selected_block(data(rt, ctx)).address()});
}

// quicktune_*: the tuning shop's adjustments of the selected block.
void set_quicktune_power(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    selected_block(data(rt, ctx)).quicktune_power() = static_cast<std::int16_t>(ctx.gpr[5]);
}
void quicktune_power(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(selected_block(data(rt, ctx)).quicktune_power().value());
}
void set_quicktune_weight(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    selected_block(data(rt, ctx)).quicktune_weight() = static_cast<std::uint16_t>(ctx.gpr[5]);
}
void quicktune_weight(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = selected_block(data(rt, ctx)).quicktune_weight();
}
// quicktune_maxspeed's setter then calls func_0892ED70 on the block (tail call).
void set_quicktune_maxspeed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarBlock> block = selected_block(data(rt, ctx));
    block.quicktune_maxspeed() = static_cast<std::uint8_t>(ctx.gpr[5]);
    tail_call(ctx, 0x0892ED70u, {block.address()});
}
void quicktune_maxspeed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = selected_block(data(rt, ctx)).quicktune_maxspeed();
}
// quicktune_tcs is kept as 11 - level (0 stays 0).
void set_quicktune_tcs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t level = ctx.gpr[5];
    selected_block(data(rt, ctx)).quicktune_tcs() = static_cast<std::uint8_t>(level == 0u ? 0u : (11u - level) & 255u);
}
void quicktune_tcs(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t stored = selected_block(data(rt, ctx)).quicktune_tcs();
    ctx.gpr[2] = stored == 0u ? 0u : 11u - stored;
}
void set_quicktune_chassis_performance(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    selected_block(data(rt, ctx)).quicktune_chassis_performance() = static_cast<std::uint8_t>(ctx.gpr[5]);
}
void quicktune_chassis_performance(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = selected_block(data(rt, ctx)).quicktune_chassis_performance();
}

// --- Methods -----------------------------------------------------------------------------------------

// getTunerID.
void tuner_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).tuner_id(); }
// getPowerScratch.
void power_scratch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).power_scratch(); }
// getPPScratch and getPP1000Scratch.
void pp_scratch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).pp_scratch(); }

// isVacant: the block's 64-bit car code is all ones.
void is_vacant(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarBlock> block = selected_block(data(rt, ctx));
    const std::uint32_t low = block.car_code_low();
    ctx.gpr[2] = (low == 0xFFFFFFFFu && block.car_code_high() == low) ? 1u : 0u;
}

// isTargetLocal's second test (non-zero: not local). No GT name: it keeps its address-derived name.
void func_088A5984(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data(rt, ctx).unk_4D0(); }

// getPower / getTorque(handle, &value, &half): writes the float and the half of the selected spec.
void power(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarParameterSpec> spec = *data(rt, ctx).spec();
    rt.memory().store32(ctx.gpr[5], std::bit_cast<std::uint32_t>(spec.power().value()));
    rt.memory().store32(ctx.gpr[6], spec.unk_1B38());
}
void torque(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<CarParameterSpec> spec = *data(rt, ctx).spec();
    rt.memory().store32(ctx.gpr[5], std::bit_cast<std::uint32_t>(spec.torque().value()));
    rt.memory().store32(ctx.gpr[6], spec.unk_1B3A());
}
// getWeight.
void weight(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = sign_extended(data(rt, ctx).spec()->weight().value());
}
// getPP and getGTPerformanceIndex (and getPP1000 through the script): the spec's PP.
void pp(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.fpr[0] = data(rt, ctx).spec()->pp(); }

// getPWR: weight over power, as a float.
void pwr(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    (void)call_guest(rt, ctx, 0x088A5A48u, {handle, frame.at(0), frame.at(4)});
    const auto weight = static_cast<std::int32_t>(call_guest(rt, ctx, 0x088A5A90u, {handle}));
    ctx.fpr[0] = static_cast<float>(weight) / std::bit_cast<float>(rt.memory().load32(frame.at(0)));
}

// getColorMax.
void color_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x0893273Cu, {selected_block(data(rt, ctx)).address()});
}

// getCarCode: the selected block's 64-bit car code (func_08932558, tail call). The other getters that
// show the car (getCarLabel) read it through here too.
void car_code(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08932558u, {selected_block(data(rt, ctx)).address()});
}

// getTireCategories(handle, &front, &rear).
void tire_categories(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t front = ctx.gpr[5], rear = ctx.gpr[6];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t block = selected_block(data(rt, ctx)).address();
    rt.memory().store32(front, call_guest(rt, ctx, 0x08932D40u, {block}));
    rt.memory().store32(rear, call_guest(rt, ctx, 0x08932DA0u, {selected_block(data(rt, ctx)).address()}));
}

// beginPartsChange / endPartsChange: forward the data to func_088A5D50 / func_088A5748 (tail calls).
void begin_parts_change(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088A5D50u, {data(rt, ctx).address()});
}
void end_parts_change(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088A5748u, {data(rt, ctx).address()});
}

// endInfo: releases the spec (func_08B64654) unless unk_4CC is set, and clears the member.
void end_info(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    if (data_of(rt, handle).unk_4CC() != 0u) return;
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08B64654u, {static_cast<std::uint32_t>(data_of(rt, handle).spec())});
    data_of(rt, handle).spec() = 0u;
}

// deepCopy(handle, const word *other) -> handle, after func_088A5450 on the two data pointers.
void deep_copy(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t handle = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x088A5450u, {word(rt, handle), word(rt, ctx.gpr[5])});
    }
    ctx.gpr[2] = handle;
}
// setChangeableParams(a, b): func_088A5580(*a, *b) (tail call).
void set_changeable_params(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088A5580u, {word(rt, ctx.gpr[4]), word(rt, ctx.gpr[5])});
}

// The selected block's address (what isCVT and isATOnly inspect through func_08932598 /
// func_089325E0).
void selected_block_address(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = selected_block(data(rt, ctx)).address();
}

// setCarCode (0x08855990) calls both of these after setting the code: each runs a function on block 0 and,
// when it answers non-zero, func_08932450 on blocks 1 and 2 with block 0. They differ only in the first
// function (func_08935B10 / func_08935B88); what they do isn't established, so they keep their addresses.
std::uint32_t copy_to_other_blocks(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t test) {
    const std::uint32_t handle = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t count = call_guest(rt, ctx, test, {data_of(rt, handle).blocks(0).address()});
    if (count != 0u) {
        for (std::uint32_t i = 1; i != CarParameterData::blocks_count; ++i) {
            const View<CarParameterData> car = data_of(rt, handle);
            (void)call_guest(rt, ctx, 0x08932450u, {car.blocks(i).address(), car.blocks(0).address()});
        }
    }
    return count;
}
void func_088A58A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = copy_to_other_blocks(rt, ctx, 0x08935B10u);
}
void func_088A5820(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = copy_to_other_blocks(rt, ctx, 0x08935B88u);
}

} // namespace gt::game::car_parameter

namespace car_parameter_registrations {
using gt::ReturnKind;

GT_DECOMP(0x088A5928u, gt::game::car_parameter::target, ReturnKind::Word);
GT_DECOMP(0x088A5938u, gt::game::car_parameter::set_target, ReturnKind::Void);
GT_DECOMP(0x088A595Cu, gt::game::car_parameter::race_class_id, ReturnKind::Word);
GT_DECOMP(0x088A5974u, gt::game::car_parameter::set_quality_flag, ReturnKind::Void);
GT_DECOMP(0x088A599Cu, gt::game::car_parameter::set_color, ReturnKind::Void);
GT_DECOMP(0x088A6164u, gt::game::car_parameter::color, ReturnKind::Word);
GT_DECOMP(0x088A6F7Cu, gt::game::car_parameter::set_quicktune_tcs, ReturnKind::Void);
GT_DECOMP(0x088A63A0u, gt::game::car_parameter::quicktune_tcs, ReturnKind::Word);
GT_DECOMP(0x088A7608u, gt::game::car_parameter::power_scratch, ReturnKind::Word);
GT_DECOMP(0x088A5A48u, gt::game::car_parameter::power, ReturnKind::Void);
GT_DECOMP(0x088A5AE4u, gt::game::car_parameter::pp, ReturnKind::Float);
GT_DECOMP(0x088A6144u, gt::game::car_parameter::color_max, ReturnKind::Void);
GT_DECOMP(0x088A5EE0u, gt::game::car_parameter::car_code, ReturnKind::DoubleWord);
GT_DECOMP(0x088A6414u, gt::game::car_parameter::tire_categories, ReturnKind::Void);
GT_DECOMP(0x088A5CD0u, gt::game::car_parameter::end_parts_change, ReturnKind::Void);
// Not registered: reaches calls the isolated run can't replay (not comparable in all its cases).
// GT_DECOMP(0x088A5E54u, gt::game::car_parameter::begin_parts_change, ReturnKind::Void);
GT_DECOMP(0x088A5A0Cu, gt::game::car_parameter::end_info, ReturnKind::Void);
GT_DECOMP(0x088A6C18u, gt::game::car_parameter::deep_copy, ReturnKind::Word);
GT_DECOMP(0x088A6C48u, gt::game::car_parameter::set_changeable_params, ReturnKind::Void);
GT_DECOMP(0x088A6608u, gt::game::car_parameter::selected_block_address, ReturnKind::Word);
GT_DECOMP(0x088A58A4u, gt::game::car_parameter::func_088A58A4, ReturnKind::Word);
GT_DECOMP(0x088A5820u, gt::game::car_parameter::func_088A5820, ReturnKind::Word);
// Not registered yet: the three pad routes never call these (compared 0, calls 0), so the gate can't
// check them; they run as the recompiled originals until a route reaches them. Mostly accessors of
// attributes the scripts leave alone in those states.
// GT_DECOMP(0x088A5944u, gt::game::car_parameter::team_id, ReturnKind::Word);
// GT_DECOMP(0x088A5950u, gt::game::car_parameter::set_team_id, ReturnKind::Void);
// GT_DECOMP(0x088A5968u, gt::game::car_parameter::set_race_class_id, ReturnKind::Void);
// GT_DECOMP(0x088A746Cu, gt::game::car_parameter::quality_flag, ReturnKind::Word);
// GT_DECOMP(0x088A5D3Cu, gt::game::car_parameter::set_obtain_id, ReturnKind::Void);
// GT_DECOMP(0x088A75F8u, gt::game::car_parameter::obtain_id, ReturnKind::Word);
// GT_DECOMP(0x088A62E4u, gt::game::car_parameter::set_quicktune_power, ReturnKind::Void);
// GT_DECOMP(0x088A62C4u, gt::game::car_parameter::quicktune_power, ReturnKind::Word);
// GT_DECOMP(0x088A6324u, gt::game::car_parameter::set_quicktune_weight, ReturnKind::Void);
// GT_DECOMP(0x088A6304u, gt::game::car_parameter::quicktune_weight, ReturnKind::Word);
// GT_DECOMP(0x088A6364u, gt::game::car_parameter::set_quicktune_maxspeed, ReturnKind::Void);
// GT_DECOMP(0x088A6344u, gt::game::car_parameter::quicktune_maxspeed, ReturnKind::Word);
// GT_DECOMP(0x088A63F4u, gt::game::car_parameter::set_quicktune_chassis_performance, ReturnKind::Void);
// GT_DECOMP(0x088A63D4u, gt::game::car_parameter::quicktune_chassis_performance, ReturnKind::Word);
// GT_DECOMP(0x088A5804u, gt::game::car_parameter::tuner_id, ReturnKind::Word);
// GT_DECOMP(0x088A7618u, gt::game::car_parameter::pp_scratch, ReturnKind::Word);
// GT_DECOMP(0x088A6028u, gt::game::car_parameter::is_vacant, ReturnKind::Word);
// GT_DECOMP(0x088A5984u, gt::game::car_parameter::func_088A5984, ReturnKind::Word);
// GT_DECOMP(0x088A5A6Cu, gt::game::car_parameter::torque, ReturnKind::Void);
// GT_DECOMP(0x088A5A90u, gt::game::car_parameter::weight, ReturnKind::Word);
// GT_DECOMP(0x088A5AA0u, gt::game::car_parameter::pwr, ReturnKind::Float);
} // namespace car_parameter_registrations
