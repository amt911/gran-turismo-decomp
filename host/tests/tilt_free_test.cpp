// host/tilt_free.cpp and the v7 API functions (docs/superpowers/specs/2026-11-18-rollover-physics-design.md): which flag readers get 1, per car, per mod.
#include "mod_hooks.hpp"
#include "tilt_free.hpp"

#include "game_state.hpp"
#include "psprecomp/runtime.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

using namespace gt::mods;
std::uint32_t g_flag_seen = 99;
std::uint32_t g_clamp_flag = 99, g_wrap_flag = 99, g_flip_flag = 99, g_contact_flag = 99, g_ground_flag = 99;

// The game's flag reader: 0. It is called through the dispatcher from the fake callers below with the ra of the real call sites.
void flag_reader(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = 0u;
    ctx.pc = ctx.gpr[31];
}
std::uint32_t read_flag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t ra) {
    ctx.pc = tilt::kFlagReader;
    ctx.gpr[31] = ra;
    rt.function_at(tilt::kFlagReader)(rt, ctx);
    return ctx.gpr[2];
}
// A body step whose callees read the flag the way the decompiled build does (nested calls through the dispatcher, junk ra).
void body_step(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    psprecomp::AllegrexContext inner = ctx;
    g_clamp_flag = read_flag(rt, inner, 0x140u);
    ctx.pc = ctx.gpr[31];
}
void contact_step_inside(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    psprecomp::AllegrexContext inner = ctx;
    g_contact_flag = read_flag(rt, inner, 0x150u);
    ctx.pc = ctx.gpr[31];
}
void body_step_with_contact(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    psprecomp::AllegrexContext inner = ctx;
    g_clamp_flag = read_flag(rt, inner, 0x140u);
    inner = ctx;
    inner.pc = tilt::kContactStep;
    inner.gpr[31] = 0x08800100u;
    rt.function_at(tilt::kContactStep)(rt, inner);
    ctx.pc = ctx.gpr[31];
}

void call_body(psprecomp::Runtime &rt, std::uint32_t car_object) {
    psprecomp::AllegrexContext ctx{};
    ctx.pc = tilt::kBodyStep;
    ctx.gpr[4] = car_object;
    ctx.gpr[31] = 0x08800100u;
    rt.function_at(tilt::kBodyStep)(rt, ctx);
}
}  // namespace

int main() {
    // the pure decision
    check(tilt::lift_reader(0x140u, true, true), "decompiled build: inside a freed car's body step: lifted");
    check(!tilt::lift_reader(0x140u, false, true), "decompiled build: junk ra outside the extent: not lifted");
    check(tilt::lift_reader(tilt::kClampRa, false, true) && tilt::lift_reader(tilt::kWrapRa, false, true), "recompiled build: the clamp's and the wrap's call sites: lifted");
    check(!tilt::lift_reader(tilt::kClampRa, false, false), "recompiled build: a car that is not freed: not lifted");
    check(!tilt::lift_reader(0x08956958u, false, true) && !tilt::lift_reader(0x0894E208u, false, true) && !tilt::lift_reader(0x08956F20u, false, true),
          "the flip machine's, the ground clamp's and the contact-normal reset's call sites: never lifted");

    psprecomp::Runtime rt;
    rt.register_function(tilt::kFlagReader, &flag_reader, "flag");
    rt.register_function(tilt::kBodyStep, &body_step_with_contact, "body");
    rt.register_function(tilt::kContactStep, &contact_step_inside, "contact");
    GtMod mod;
    mod.id = "rollover";
    mod.api_version = GT_MOD_API_VERSION_7;
    std::string error;
    check(tilt::install(rt, &mod, error), "the three hooks install for a v7 mod");
    const std::uint32_t car_a = 0x09100000u, body_a = 0x09200000u, car_b = 0x09101000u, body_b = 0x09201000u;
    rt.memory().store32(car_a + 8u, body_a);
    rt.memory().store32(car_b + 8u, body_b);

    g_clamp_flag = g_contact_flag = 99;
    call_body(rt, car_a);
    check(g_clamp_flag == 0 && g_contact_flag == 0, "a car nobody freed reads the game's flag, 0");

    const GtModApi &api7 = mod_api(GT_MOD_API_VERSION_7);
    check(api7.version == 7 && api7.tilt_free != nullptr && api7.car_set_pose != nullptr && api7.car_dimensions != nullptr && api7.car_wheel_contacts != nullptr, "the v7 table has the four calls");
    check(api7.tilt_free(&mod, nullptr, body_a, 1) == 0 && tilt::freed(body_a) && !tilt::freed(body_b), "tilt_free on: that car only");
    g_clamp_flag = g_contact_flag = 99;
    call_body(rt, car_a);
    check(g_clamp_flag == 1, "the freed car's clamp reads 1");
    check(g_contact_flag == 0, "the contact step reads the game's real word even for a freed car (FACTS: its decompiled flag-1 path differs)");
    g_clamp_flag = 99;
    call_body(rt, car_b);
    check(g_clamp_flag == 0, "another car's body step still reads 0");
    check(api7.tilt_free(&mod, nullptr, body_a, 0) == 0 && !tilt::freed(body_a), "tilt_free off");
    g_clamp_flag = 99;
    call_body(rt, car_a);
    check(g_clamp_flag == 0, "restored: 0 again");
    api7.tilt_free(&mod, nullptr, body_a, 1);
    set_active(&mod, false);
    check(!tilt::freed(body_a), "a mod switched off frees nothing (its hooks are skipped too)");
    g_clamp_flag = 99;
    call_body(rt, car_a);
    check(g_clamp_flag == 0, "switched off in the menu: the game reads its real word");
    set_active(&mod, true);
    check(api7.tilt_free(&mod, nullptr, 0u, 1) != 0, "a null handle is refused");
    GtMod old;
    old.id = "old";
    old.api_version = 6;
    check(api7.tilt_free(&old, nullptr, body_a, 1) != 0 && tilt::freed(body_a), "a mod that declared api 6 is refused");
    float pos[3] = {0, 0, 0}, rows[9] = {1, 0, 0, 0, 1, 0, 0, 0, 1}, vel[3] = {0, 0, 0}, w[3] = {0, 0, 0};
    GtModCtx cctx{&rt, nullptr, 0u, 0u};
    check(api7.car_set_pose(&old, &cctx, body_a, pos, rows, vel, w) != 0, "car_set_pose refuses an api-6 mod");
    GtCarDimensions dims;
    check(api7.car_dimensions(&old, &cctx, 0x09000000u, body_a, &dims) != 0, "car_dimensions refuses an api-6 mod");
    GtWheelContact wc[4];
    check(api7.car_wheel_contacts(&old, &cctx, 0x09000000u, body_a, wc) != 0, "car_wheel_contacts refuses an api-6 mod");
    // a v7 mod through the API: car_set_pose writes the guest memory
    const float pose_pos[3] = {1.0f, 2.0f, 3.0f}, pose_vel[3] = {0.0f, 3.0f, 4.0f};
    float rr[9];
    const float e[3] = {0.0f, 3.0f, 0.5f};
    gt::game_state::euler_to_rows(e, rr);
    check(api7.car_set_pose(&mod, &cctx, body_a, pose_pos, rr, pose_vel, w) == 0, "car_set_pose through the API");
    float speed;
    std::uint32_t bits = rt.memory().load32(body_a + 0x6C4u);
    std::memcpy(&speed, &bits, 4);
    check(speed == 5.0f, "the pose landed in the guest memory (speed |v|)");
    tilt::forget(&mod);
    check(!tilt::freed(body_a), "forget drops a removed mod's entries");
    if (failures == 0) std::puts("tilt_free: all checks passed");
    return failures == 0 ? 0 : 1;
}
