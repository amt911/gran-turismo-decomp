// Unit tests for the mod API v2 services (host/mod_services.cpp) without a shared library: registration rules, the v1 gate,
// call_guest only from the tick.
#include "mod_hooks.hpp"
#include "mod_services.hpp"

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

constexpr std::uint32_t kGuest = 0x08901000u;
int g_guest_runs = 0;
void guest_sum(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {  // v0 = a0 + a1 + ... + t3
    ++g_guest_runs;
    std::uint32_t sum = 0;
    for (unsigned r : {4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u}) sum += ctx.gpr[r];
    ctx.gpr[2] = sum;
    ctx.pc = ctx.gpr[31];
}

void label_a(GtMod *, char *out, std::uint32_t capacity) { std::snprintf(out, capacity, "alpha"); }
void label_long(GtMod *, char *out, std::uint32_t capacity) { std::snprintf(out, capacity, "%s", "a label much longer than the buffer it is given"); }
int g_dir = 99;
void act(GtMod *, int direction) { g_dir = direction; }
void tick_noop(GtModCtx *) {}

}  // namespace

int main() {
    using namespace gt::mods;
    using namespace gt::mods::services;
    psprecomp::Runtime rt;
    rt.register_function(kGuest, &guest_sum, "guest");

    GtMod v2a, v2b, v1;
    v2a.id = "a";
    v2a.api_version = 2;
    v2b.id = "b";
    v2b.api_version = 2;
    v1.id = "old";
    v1.api_version = 1;
    const GtModApi &api = mod_api(2);

    {  // v3 named state (host/game_state.hpp): money through the API, refused to a v2 mod
        GtMod v3;
        v3.id = "v3";
        v3.api_version = 3;
        const GtModApi &api3 = mod_api(3);
        check(api3.version == 3 && mod_api(2).version == 2 && mod_api(1).version == 1, "each mod sees its declared api");
        rt.memory().store32(0x08F6AC00u, 100000u);
        rt.memory().store32(0x08F6AC04u, 0u);
        rt.memory().store32(0x08F6AC08u, 0x01030000u);
        GtModCtx money_ctx{&rt, nullptr, 0u, 0u};
        std::uint32_t value = 0u;
        check(api3.money_get(&v3, &money_ctx, &value) == 0 && value == 100000u, "v3 money_get");
        check(api3.money_set(&v3, &money_ctx, 250000u) == 0 && rt.memory().load32(0x08F6AC00u) == 250000u, "v3 money_set");
        check(api3.money_get(&v2a, &money_ctx, &value) != 0 && api3.money_set(&v2a, &money_ctx, 1u) != 0 &&
                  rt.memory().load32(0x08F6AC00u) == 250000u, "money refused to an api-2 mod");
        std::uint32_t car = 7u;
        GtCar cars[4];
        check(api3.player_car(&v3, &money_ctx, 0u, &car) != 0 && car == 7u, "v3 player_car: no world, no car");
        check(api3.race_cars(&v3, &money_ctx, 0u, cars, 4u) == 0u, "v3 race_cars: no world, no cars");
        GtCarState state{};
        check(api3.car_state(&v3, &money_ctx, 0u, &state) != 0, "v3 car_state: a null handle is refused");
        check(api3.player_car(&v2a, &money_ctx, 0u, &car) != 0, "player_car refused to an api-2 mod");
    }
    clear_hooks_for_tests();
    check(api.add_menu_row(&v2a, "x", label_a, act) != 0, "row refused outside gt_mod_init");
    check(api.on_vblank(&v2a, tick_noop) != 0, "tick refused outside gt_mod_init");
    begin_init(&v2a);
    check(api.add_menu_row(&v2a, "x", label_a, act) == 0, "row accepted during init");
    check(api.add_menu_row(&v2a, "x", label_a, act) != 0, "same id twice in one mod refused");
    check(api.add_menu_row(&v2a, "y", label_a, act) == 0, "second id accepted");
    check(api.add_menu_row(&v2a, "", label_a, act) != 0, "empty id refused");
    check(api.add_menu_row(&v2a, std::string(kMaxRowIdLength + 1, 'z').c_str(), label_a, act) != 0, "over-long id refused");
    check(api.add_menu_row(&v2a, "n", nullptr, act) != 0 && api.add_menu_row(&v2a, "n", label_a, nullptr) != 0, "null functions refused");
    check(api.add_menu_row(&v2b, "x", label_a, act) != 0, "init of mod a: mod b cannot register");
    check(api.on_vblank(&v2a, tick_noop) == 0 && api.on_vblank(&v2a, tick_noop) != 0, "one tick per mod");
    check(api.on_vblank(&v2a, nullptr) != 0, "null tick refused");
    end_init();
    begin_init(&v2b);
    check(api.add_menu_row(&v2b, "x", label_long, act) == 0, "the same row id in another mod is fine");
    end_init();
    begin_init(&v1);
    check(api.add_menu_row(&v1, "x", label_a, act) != 0 && api.on_vblank(&v1, tick_noop) != 0, "a mod that declared api 1 gets refusals");
    end_init();
    check(menu_row_count() == 3, "rows: a/x, a/y, b/x");

    char text[16];
    check(menu_row_text(0, text, sizeof text) && std::string(text) == "alpha", "label read");
    check(menu_row_text(2, text, sizeof text) && std::strlen(text) == sizeof text - 1, "a long label is cut to the capacity");
    check(!menu_row_text(3, text, sizeof text), "out of range");
    check(menu_row_activate(0, 5) && g_dir == 1 && menu_row_activate(0, -9) && g_dir == -1 && menu_row_activate(0, 0) && g_dir == 0,
          "direction is passed as -1, 0 or 1");
    check(!menu_row_activate(3, 0), "activate out of range");

    // call_guest: only from the tick
    psprecomp::AllegrexContext ctx;
    ctx.gpr[29] = 0x09000000u;
    GtModCtx hook_ctx{&rt, &ctx, 0x08900000u, 0, false, &v2a};
    std::uint32_t v0 = 0;
    const std::uint32_t args[3] = {1, 20, 300};
    check(api.call_guest(&hook_ctx, kGuest, args, 3, &v0) != 0 && g_guest_runs == 0, "call_guest refused from a hook, guest not run");
    GtModCtx tick_ctx{&rt, &ctx, 0, 0, true, &v2a};
    check(api.call_guest(&tick_ctx, kGuest, args, 3, &v0) == 0 && v0 == 321u && g_guest_runs == 1, "call_guest runs from the tick: v0 = 321");
    const std::uint32_t eight[8] = {1, 1, 1, 1, 1, 1, 1, 1};
    check(api.call_guest(&tick_ctx, kGuest, eight, 8, &v0) == 0 && v0 == 8u, "eight arguments");
    check(api.call_guest(&tick_ctx, kGuest, eight, 9, &v0) != 0, "nine arguments refused");
    check(api.call_guest(&tick_ctx, kGuest, nullptr, 2, &v0) != 0, "null arguments with a count refused");
    check(api.call_guest(&tick_ctx, kGuest, nullptr, 0, nullptr) == 0, "no arguments, no result wanted");
    GtModCtx v1_tick{&rt, &ctx, 0, 0, true, &v1};
    const int before = g_guest_runs;
    check(api.call_guest(&v1_tick, kGuest, nullptr, 0, &v0) != 0 && g_guest_runs == before, "call_guest refused for a mod that declared api 1");
    rt.stop("test");
    check(api.call_guest(&tick_ctx, kGuest, nullptr, 0, &v0) != 0, "call_guest refused on a stopped runtime");

    // the tick skips switched-off mods
    clear_for_tests();
    static int ticks = 0;
    begin_init(&v2a);
    api.on_vblank(&v2a, [](GtModCtx *c) { ticks += c->tick ? 1 : 100; });
    end_init();
    psprecomp::Runtime rt2;
    run_vblank_tick(rt2, ctx, 5);
    set_active(&v2a, false);
    run_vblank_tick(rt2, ctx, 5);
    set_active(&v2a, true);
    run_vblank_tick(rt2, ctx, 5);
    check(ticks == 2, "tick runs for an active mod (ctx.tick true), skipped while off");

    // is_menu_open and vblank_index
    clear_for_tests();
    check(api.is_menu_open(&v2a) == 0, "no query set: the menu is never open");
    static bool open_now = false;
    set_menu_open_query([] { return open_now; });
    check(api.is_menu_open(&v2a) == 0 && api.is_menu_open(&v1) == 0, "menu closed");
    open_now = true;
    check(api.is_menu_open(&v2a) == 1 && api.is_menu_open(&v1) == 1, "menu open: answered for any api version");
    check(api.vblank_index(&v2a) == 0 && !any_registered(), "vblank 0 before the first tick, nothing registered");
    run_vblank_tick(rt2, ctx, 1234);
    check(api.vblank_index(&v2a) == 1234, "vblank_index follows the tick (also with no tick registered)");
    begin_init(&v2a);
    api.on_vblank(&v2a, tick_noop);
    end_init();
    check(any_registered(), "any_registered after a tick is registered");
    clear_for_tests();

    // a failed init forgets rows and tick
    clear_for_tests();
    begin_init(&v2b);
    api.add_menu_row(&v2b, "x", label_a, act);
    api.on_vblank(&v2b, tick_noop);
    end_init();
    remove_for(&v2b);
    check(menu_row_count() == 0, "remove_for drops the rows");

    if (failures == 0) std::puts("mod_services tests passed");
    return failures == 0 ? 0 : 1;
}
