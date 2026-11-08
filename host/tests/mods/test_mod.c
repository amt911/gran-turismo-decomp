/* Test mod for host/tests/mod_loader_test.cpp: hooks guest 0x08900000 and adds `bonus` (setting) to v0 after the original.
 * With the `row_id` setting it also uses the API v2 services (what the loader test checks through the row's label):
 * a menu row, a vblank tick that calls the guest function at 0x08901000 with (7, 8), and a call_guest attempt from the hook. */
#include "gt_mod/gt_mod_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const GtModApi *g_api;
static GtMod *g_self;
static uint32_t g_bonus;
static int g_activations, g_last_direction, g_ticks;
static uint32_t g_tick_call_v0, g_tick_call_status = 99, g_hook_call_status = 99;

static void hook(GtModCtx *ctx) {
    uint32_t v0 = 0;
    g_api->call_original(ctx);
    g_api->set_gpr(ctx, 2, g_api->gpr(ctx, 2) + g_bonus);
    if (g_api->version >= GT_MOD_API_VERSION_2) g_hook_call_status = (uint32_t)g_api->call_guest(ctx, 0x08901000u, NULL, 0, &v0);
}

static void label(GtMod *self, char *out, uint32_t capacity) {
    (void)self;
    snprintf(out, capacity, "v%u act=%d dir=%d ticks=%d tickcall=%u/%u hookcall=%u", g_api->version, g_activations, g_last_direction, g_ticks,
             g_tick_call_status, g_tick_call_v0, g_hook_call_status);
}

static void activate(GtMod *self, int direction) {
    (void)self;
    ++g_activations;
    g_last_direction = direction;
}

static void tick(GtModCtx *ctx) {
    const uint32_t args[2] = {7u, 8u};
    ++g_ticks;
    g_tick_call_status = (uint32_t)g_api->call_guest(ctx, 0x08901000u, args, 2, &g_tick_call_v0);
}

int gt_mod_init(const GtModApi *api, GtMod *self) {
    const char *bonus = api->config(self, "bonus");
    g_api = api;
    g_self = self;
    g_bonus = bonus != NULL ? (uint32_t)strtoul(bonus, NULL, 10) : 0u;
    if (api->config(self, "expect_version") != NULL && (int)api->version != atoi(api->config(self, "expect_version"))) return 5;
    {
        const char *fail = api->config(self, "fail_after_hook");
        int status = api->hook(self, 0x08900000u, hook);
        if (status == 0 && api->config(self, "hook_twice") != NULL) status = api->hook(self, 0x08900000u, hook);
        if (status == 0 && api->config(self, "row_id") != NULL) {
            /* a v1 mod that calls the v2 functions anyway is refused (non-zero), which the test reads from the row it cannot add */
            status = api->add_menu_row(self, api->config(self, "row_id"), label, activate);
            if (status == 0 && api->config(self, "row_twice") != NULL) status = api->add_menu_row(self, api->config(self, "row_id"), label, activate) == 0 ? 9 : 0;
            if (status == 0 && api->on_vblank(self, tick) != 0) status = 8;
        }
        return (fail != NULL && status == 0) ? 7 : status;
    }
}
