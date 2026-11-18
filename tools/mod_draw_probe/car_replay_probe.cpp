// Research probe for the spec's Step 0 item 3 (docs/FACTS.md "Car draw replay"): does re-issuing a car part's draw with a shifted WORLD matrix draw a
// second copy of the car? In the mesh-draw hook of a part whose WORLD matrix looks like a car's box (orthogonal rows 0.3..2.6 long), after the game's own
// draw: append WORLDN + 12 WORLDD with the matrix's translation shifted in view space, draw again (call_original), then append the original matrix again.
// Settings: shift = "x y z" view-space metres (default "2.5 0 0"); log = 1 prints the first matrices. API 5 only (no draw API needed).
#include "gt_mod/gt_mod_api.h"

#include <cmath>
#include <initializer_list>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
const GtModApi *g_api = nullptr;
GtMod *g_self = nullptr;
float g_shift[3] = {2.5f, 0.0f, 0.0f};
unsigned g_replayed = 0, g_logged = 0;
constexpr uint32_t kListState = 0x08D6CE00u;  // GU object: the display list cursor is word(+0x5C)

float len(const float *r) { return std::sqrt(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]); }
float dot(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

bool car_box_matrix(const float m[12]) {
    const float l0 = len(m), l1 = len(m + 3), l2 = len(m + 6);
    for (float l : {l0, l1, l2})
        if (l < 0.3f || l > 2.6f) return false;
    return std::fabs(dot(m, m + 3) / (l0 * l1)) < 0.05f && std::fabs(dot(m, m + 6) / (l0 * l2)) < 0.05f && std::fabs(dot(m + 3, m + 6) / (l1 * l2)) < 0.05f;
}

// WORLDN (0x3A) and 12 WORLDD (0x3B, the top 24 bits of each float) at the list cursor, as the game's own upload writes them.
void append_world(GtModCtx *ctx, const float m[12]) {
    uint32_t cursor = g_api->read32(ctx, kListState + 0x5Cu);
    g_api->write32(ctx, cursor, 0x3A000000u);
    for (unsigned i = 0; i < 12; ++i) {
        uint32_t bits;
        std::memcpy(&bits, &m[i], 4);
        g_api->write32(ctx, cursor + 4u * (i + 1u), 0x3B000000u | (bits >> 8));
    }
    g_api->write32(ctx, kListState + 0x5Cu, cursor + 52u);
}

void on_mesh_draw(GtModCtx *ctx) {
    float m[12];
    if (g_api->draw_matrix(g_self, ctx, m) != 0 || !car_box_matrix(m)) {
        g_api->call_original(ctx);
        return;
    }
    if (g_logged < 6) {
        ++g_logged;
        char line[200];
        std::snprintf(line, sizeof line, "CAR-MATRIX blob=%08x rows %.3f %.3f %.3f t=(%.2f %.2f %.2f)", g_api->draw_mesh_blob(ctx), len(m), len(m + 3), len(m + 6), m[9], m[10], m[11]);
        g_api->log(g_self, line);
    }
    const uint32_t blob = g_api->gpr(ctx, 4);  // the leaf clobbers a0 (it ends holding the new list cursor): the second draw needs the blob back
    g_api->call_original(ctx);
    float shifted[12];
    std::memcpy(shifted, m, sizeof shifted);
    for (int i = 0; i < 3; ++i) shifted[9 + i] += g_shift[i];
    append_world(ctx, shifted);
    g_api->set_gpr(ctx, 4, blob);
    g_api->call_original(ctx);
    append_world(ctx, m);
    // The leaf returns the new cursor in v0 and its caller may go on writing there: hand it the cursor after our commands.
    g_api->set_gpr(ctx, 2, g_api->read32(ctx, kListState + 0x5Cu));
    ++g_replayed;
}
void tick(GtModCtx *) {
    static uint64_t last = 0;
    if (g_replayed != last && g_replayed % 400 < 2) {
        char line[80];
        std::snprintf(line, sizeof line, "replayed part draws: %u", g_replayed);
        g_api->log(g_self, line);
    }
    last = g_replayed;
}
}  // namespace

extern "C" int gt_mod_init(const GtModApi *api, GtMod *self) {
    if (api->version < GT_MOD_API_VERSION_5) return 1;
    g_api = api;
    g_self = self;
    if (const char *s = api->config(self, "shift")) std::sscanf(s, "%f %f %f", &g_shift[0], &g_shift[1], &g_shift[2]);
    if (api->hook_point(self, GT_HOOK_MESH_DRAW, on_mesh_draw) != 0) return 2;
    if (api->on_vblank(self, tick) != 0) return 3;
    api->log(self, "car replay probe: loaded");
    return 0;
}
