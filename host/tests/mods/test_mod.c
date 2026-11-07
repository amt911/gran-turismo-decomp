/* Test mod for host/tests/mod_loader_test.cpp: hooks guest 0x08900000 and adds `bonus` (setting) to v0 after the original. */
#include "gt_mod/gt_mod_api.h"

#include <stdlib.h>

static const GtModApi *g_api;
static uint32_t g_bonus;

static void hook(GtModCtx *ctx) {
    g_api->call_original(ctx);
    g_api->set_gpr(ctx, 2, g_api->gpr(ctx, 2) + g_bonus);
}

int gt_mod_init(const GtModApi *api, GtMod *self) {
    const char *bonus = api->config(self, "bonus");
    g_api = api;
    g_bonus = bonus != NULL ? (uint32_t)strtoul(bonus, NULL, 10) : 0u;
    {
        const char *fail = api->config(self, "fail_after_hook");
        int status = api->hook(self, 0x08900000u, hook);
        if (status == 0 && api->config(self, "hook_twice") != NULL) status = api->hook(self, 0x08900000u, hook);
        return (fail != NULL && status == 0) ? 7 : status;
    }
}
