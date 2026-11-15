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

    GtMod v2a, v2b, v1, v3_for_v4;
    v3_for_v4.id = "v3x";
    v3_for_v4.api_version = 3;
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

    {  // v4: actions (a hotkey or the device's menu fires them; they run on the vblank tick) and entries placed in a category
        clear_for_tests();
        GtMod v4, v4b;
        v4.id = "cheats";
        v4.api_version = 4;
        v4b.id = "boost";
        v4b.api_version = 4;
        const GtModApi &api4 = mod_api(4);
        check(api4.version == 4, "a v4 mod sees version 4");
        static int nitro_runs = 0;
        static bool nitro_in_tick = false;
        const GtActionFn nitro = [](GtMod *, GtModCtx *c) {
            ++nitro_runs;
            nitro_in_tick = c != nullptr && c->tick;
        };
        check(api4.add_action(&v4, "nitro", "Nitro", "Race", nitro, "Ctrl+N", "L+TRIANGLE") != 0, "action refused outside gt_mod_init");
        begin_init(&v3_for_v4);
        check(mod_api(3).add_action == nullptr || mod_api(3).add_action(&v3_for_v4, "x", "X", "Race", nitro, "", "") != 0,
              "add_action refused to an api-3 mod");
        end_init();
        begin_init(&v4);
        check(api4.add_action(&v4, "nitro", "Nitro", "Race", nitro, "Ctrl+N", "L+TRIANGLE") == 0, "v4 add_action");
        check(api4.add_action(&v4, "nitro", "Nitro", "Race", nitro, "", "") != 0, "the same action twice is refused");
        check(api4.add_action(&v4, "warp", "Warp", "Mods", nitro, "", "") != 0, "Mods and Controls are the host's categories");
        check(api4.add_entry(&v4, "Race", "slowmo", label_a, act) == 0, "v4 add_entry in a category");
        check(api4.add_entry(&v4, "Nowhere", "x", label_a, act) != 0, "an unknown category is refused");
        end_init();
        check(actions().size() == 1 && actions()[0].id == "cheats.nitro" && actions()[0].key == "Ctrl+N" && actions()[0].pad == "L+TRIANGLE" &&
                  actions()[0].category == "Race", "the action is declared as <mod>.<id> with its defaults");
        check(entries().size() == 1 && entries()[0].category == "Race", "the entry is listed in its category");
        check(any_registered(), "an action makes the host install the pad filter and the tick");
        psprecomp::Runtime rt4;
        psprecomp::AllegrexContext ctx4{};
        queue_action("cheats.nitro");
        queue_action("cheats.nitro");  // twice before a tick: runs once (a hotkey and the menu in one vblank)
        check(nitro_runs == 0, "a queued action waits for the tick");
        run_vblank_tick(rt4, ctx4, 10);
        check(nitro_runs == 1 && nitro_in_tick, "the tick runs the queued action once, with a tick ctx");
        set_active(&v4, false);
        queue_action("cheats.nitro");
        run_vblank_tick(rt4, ctx4, 11);
        check(nitro_runs == 1, "a switched-off mod's action does not run");
        set_active(&v4, true);
        run_vblank_tick(rt4, ctx4, 12);
        check(nitro_runs == 1, "nor later: the queue is emptied every tick");
        char text[32];
        set_active(&v4, false);
        check(entry_text(0, text, sizeof text) && std::string(text) == "cheats mod OFF" && !entry_activate(0, 1),
              "an entry of a switched-off mod reads OFF and ignores keys");
        set_active(&v4, true);
        remove_for(&v4);
        check(actions().empty() && entries().empty(), "a failed init drops the actions and entries");
        clear_for_tests();
    }


    {  // v5: named hook points, impact, meshes (include/gt_mod/gt_mod_api.h)
        GtMod v5, v4old;
        v5.id = "v5";
        v5.api_version = 5;
        v4old.id = "v4old";
        v4old.api_version = 4;
        const GtModApi &api5 = mod_api(5);
        check(api5.version == 5 && mod_api(4).version == 4, "api 5 is its own table");
        psprecomp::Runtime r5;
        const auto cmd = [](std::uint32_t op, std::uint32_t param) { return (op << 24) | (param & 0xFFFFFFu); };
        constexpr std::uint32_t blob = 0x09200000u;
        r5.memory().store32(blob, cmd(0x10, ((blob + 20u) >> 8) & 0xFF0000u));
        r5.memory().store32(blob + 4u, cmd(0x01, (blob + 20u) & 0xFFFFFFu));
        r5.memory().store32(blob + 8u, cmd(0x12, 0x130));
        r5.memory().store32(blob + 12u, cmd(0x04, (3u << 16) | 2u));
        r5.memory().store32(blob + 16u, cmd(0x0B, 0));
        for (int c = 0; c < 3; ++c) r5.memory().store16(blob + 20u + 6u + 2u * c, static_cast<std::uint16_t>(10 * (c + 1)));
        GtModCtx c5{&r5, nullptr, 0u, 0u};
        GtMeshInfo info{};
        check(api5.mesh_info(&v5, &c5, blob, &info) == 0 && info.vertex_count == 2 && info.stride == 12 && info.position_offset == 6, "v5 mesh_info");
        int16_t xyz[3] = {0, 0, 0};
        check(api5.mesh_vertex(&v5, &c5, blob, 0, xyz) == 0 && xyz[0] == 10 && xyz[2] == 30, "v5 mesh_vertex reads the s16 position");
        const int16_t dent[3] = {1, 2, 3};
        check(api5.mesh_set_vertex(&v5, &c5, blob, 0, dent) == 0 && r5.memory().load16(blob + 26u) == 1, "v5 mesh_set_vertex writes it");
        check(api5.mesh_vertex(&v5, &c5, blob, 2, xyz) != 0 && api5.mesh_info(&v5, &c5, blob + 4u, &info) != 0, "v5 refuses a bad index and a bad blob");
        check(api5.mesh_info(&v4old, &c5, blob, &info) != 0 && api5.mesh_vertex(&v4old, &c5, blob, 0, xyz) != 0, "v5 functions refuse an api-4 mod");
        // mesh_copy allocates through the game's allocator from a tick, copies and rewrites the VADR; mesh_release frees.
        static int frees = 0;
        r5.register_function(gt::mods::services::kGuestAlloc, [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            ctx.gpr[2] = 0x09300000u;
            ctx.pc = ctx.gpr[31];
        }, "alloc");
        r5.register_function(gt::mods::services::kGuestFree, [](psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
            ++frees;
            ctx.pc = ctx.gpr[31];
        }, "free");
        psprecomp::AllegrexContext tick_regs{};
        GtModCtx tick5{&r5, &tick_regs, 0, 0, true, &v5};
        std::uint32_t copy = 0u;
        check(api5.mesh_copy(&v5, &tick5, blob, &copy) == 0 && copy == 0x09300000u, "v5 mesh_copy from a tick");
        check((r5.memory().load32(copy + 4u) & 0xFFFFFFu) == ((copy + 20u) & 0xFFFFFFu) && r5.memory().load16(copy + 26u) == 1,
              "the copy has its own vertex address and the vertices");
        GtModCtx hook5{&r5, &tick_regs, 0x08B550A8u, 0, false, &v5};
        std::uint32_t again = 0u;
        check(api5.mesh_copy(&v5, &hook5, blob, &again) != 0, "mesh_copy refuses outside a tick (call_guest rule)");
        check(api5.mesh_release(&v5, &tick5, copy) == 0 && frees == 1, "v5 mesh_release frees through the game's allocator");
        tick_regs.gpr[4] = blob;
        check(api5.draw_mesh_blob(&hook5) == blob, "draw_mesh_blob is the leaf's a0");
        api5.draw_mesh_redirect(&hook5, copy);
        check(tick_regs.gpr[4] == copy, "draw_mesh_redirect replaces it");
        {   // the car -> model link through the API
            constexpr std::uint32_t a0 = 0x09600000u, P = 0x09600100u, entry = 0x09600200u, car = 0x09600300u, body = 0x09600400u, o20 = 0x09600500u, model = 0x09600600u;
            r5.memory().store32(a0 + 4u, P); r5.memory().store32(P + 0xB04u, entry); r5.memory().store32(entry + 0xCu, car); r5.memory().store32(car + 8u, body);
            r5.memory().store32(P + 0x20u, o20); r5.memory().store32(o20 + 8u, model); r5.memory().store32(model, 0x4D444C33u);
            psprecomp::AllegrexContext regs{};
            regs.gpr[4] = a0;
            GtModCtx car_ctx{&r5, &regs, 0x0889BAB0u, 0, false, &v5};
            std::uint32_t got_car = 0, got_model = 0;
            check(api5.draw_car_model(&v5, &car_ctx, &got_car, &got_model) == 0 && got_car == body && got_model == model, "v5 draw_car_model names the car and its model");
            check(api5.draw_car_model(&v4old, &car_ctx, &got_car, &got_model) != 0, "draw_car_model refuses an api-4 mod");
            regs.gpr[4] = 0x09700000u;
            check(api5.draw_car_model(&v5, &car_ctx, &got_car, &got_model) != 0, "draw_car_model refuses a view that does not check out");
            std::uint32_t parts[4];
            check(api5.model_blobs(&v5, &car_ctx, model, parts, 4) == 0u, "model_blobs: a model with no parts lists none");
            GtVertexRun vr[4];
            check(api5.blob_runs(&v5, &car_ctx, blob, vr, 4) == 1u && vr[0].blob == blob && vr[0].run == 0u && vr[0].count == 2u, "blob_runs: the runs of any blob");
            int8_t n3[3] = {0, 0, 0};
            r5.memory().store8(blob + 20u + 2u, 5); r5.memory().store8(blob + 20u + 3u, 6); r5.memory().store8(blob + 20u + 4u, 7);
            check(api5.run_normal(&v5, &car_ctx, blob, 0, 0, n3) == 0 && n3[0] == 5 && n3[2] == 7 && api5.run_set_normal(&v5, &car_ctx, blob, 0, 0, (const int8_t[3]){1, 2, 3}) == 0 &&
                      api5.run_normal(&v5, &car_ctx, blob, 0, 0, n3) == 0 && n3[1] == 2 && api5.run_normal(&v4old, &car_ctx, blob, 0, 0, n3) != 0,
                  "run_normal / run_set_normal on a 0x130 run; an api-4 mod is refused");
            int16_t p3[3];
            check(api5.run_vertex(&v5, &car_ctx, blob, 0, 0, p3) == 0 && api5.run_vertex(&v5, &car_ctx, blob, 1, 0, p3) != 0 &&
                      api5.blob_runs(&v4old, &car_ctx, blob, vr, 4) == 0u,
                  "run_vertex of a blob's run, a missing run and an api-4 mod refused");
        }
        // named hook points install on the researched addresses
        gt::mods::bind_runtime(r5);
        r5.register_function(0x0894DD9Cu, &guest_sum, "step");
        r5.register_function(0x08B550A8u, &guest_sum, "draw");
        begin_init(&v5);
        r5.register_function(0x0889BAB0u, &guest_sum, "cardraw");
        check(api5.hook_point(&v5, GT_HOOK_WORLD_STEP, [](GtModCtx *) {}) == 0 && api5.hook_point(&v5, GT_HOOK_MESH_DRAW, [](GtModCtx *) {}) == 0 &&
                  api5.hook_point(&v5, GT_HOOK_CAR_DRAW, [](GtModCtx *) {}) == 0,
              "v5 hook_point installs the three named points");
        check(api5.hook_point(&v5, static_cast<GtHookPoint>(99), [](GtModCtx *) {}) != 0, "an unknown hook point is refused");
        check(api5.hook_point(&v4old, GT_HOOK_WORLD_STEP, [](GtModCtx *) {}) != 0, "hook_point refuses an api-4 mod");
        end_init();
        clear_hooks_for_tests();
    }

    if (failures == 0) std::puts("mod_services tests passed");
    return failures == 0 ? 0 : 1;
}
