// Research probe for the spec's Step 0 item 2 (docs/FACTS.md "Scene grade"): where in one frame's display list does the HUD begin? Hooks the two matrix
// uploads (FACTS "Camera and the world-matrix upload": 0x08B59194 = projection (cmd 0x3E), 0x08B59210 = world / view (0x3A / 0x3C) leaf) and logs, for the
// first frame at or after vblank <from> (setting, default 18500), every projection upload (m[11]: -1 perspective, 0 orthographic) and each run of world uploads
// with the list cursor (word(0x08D6CE00 + 0x5C)) at that moment. Raw addresses: a research probe, not an SDK user.
#include "gt_mod/gt_mod_api.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
const GtModApi *g_api = nullptr;
GtMod *g_self = nullptr;
uint64_t g_from = 18500;
uint64_t g_frame_vblank = 0;
unsigned g_world_run = 0;
uint32_t g_world_first = 0;
unsigned g_lines = 0;
constexpr uint32_t kList = 0x08D6CE00u;

bool recording() {
    const uint64_t v = g_api->vblank_index(g_self);
    if (v < g_from) return false;
    if (g_frame_vblank == 0) g_frame_vblank = v;
    return v == g_frame_vblank && g_lines < 400;
}
void flush_world() {
    if (g_world_run == 0) return;
    char line[120];
    std::snprintf(line, sizeof line, "WORLD x%u first at list %08x", g_world_run, g_world_first);
    g_api->log(g_self, line);
    ++g_lines;
    g_world_run = 0;
}
float readfp(GtModCtx *ctx, uint32_t p, unsigned i) { return g_api->readf(ctx, p + 4 * i); }

void on_projection(GtModCtx *ctx) {  // 0x08B59194(this, cmd, ptr)
    if (recording()) {
        flush_world();
        const uint32_t ptr = g_api->gpr(ctx, 6);
        char line[200];
        std::snprintf(line, sizeof line, "PROJ cmd %02x at list %08x m[0]=%.3f m[5]=%.3f m[10]=%.3f m[11]=%.3f m[14]=%.3f m[15]=%.3f", g_api->gpr(ctx, 5),
                      g_api->read32(ctx, kList + 0x5C), readfp(ctx, ptr, 0), readfp(ctx, ptr, 5), readfp(ctx, ptr, 10), readfp(ctx, ptr, 11), readfp(ctx, ptr, 14), readfp(ctx, ptr, 15));
        g_api->log(g_self, line);
        ++g_lines;
    }
    g_api->call_original(ctx);
}
void on_matrix(GtModCtx *ctx) {  // 0x08B59210(this, cmd, ptr)
    if (recording()) {
        const uint32_t cmd = g_api->gpr(ctx, 5);
        if (cmd == 0x3A) {
            if (g_world_run++ == 0) g_world_first = g_api->read32(ctx, kList + 0x5C);
        } else {
            flush_world();
            char line[100];
            std::snprintf(line, sizeof line, "MATRIX cmd %02x at list %08x", cmd, g_api->read32(ctx, kList + 0x5C));
            g_api->log(g_self, line);
            ++g_lines;
        }
    }
    g_api->call_original(ctx);
}
void tick(GtModCtx *) {
    if (g_frame_vblank != 0 && g_api->vblank_index(g_self) > g_frame_vblank && g_world_run != 0) flush_world();
}
}  // namespace

extern "C" int gt_mod_init(const GtModApi *api, GtMod *self) {
    if (api->version < GT_MOD_API_VERSION_2) return 1;
    g_api = api;
    g_self = self;
    if (const char *s = api->config(self, "from")) g_from = std::strtoull(s, nullptr, 10);
    if (api->hook(self, 0x08B59194u, on_projection) != 0 || api->hook(self, 0x08B59210u, on_matrix) != 0) return 2;
    if (api->on_vblank(self, tick) != 0) return 3;
    api->log(self, "hud split probe: loaded");
    return 0;
}
