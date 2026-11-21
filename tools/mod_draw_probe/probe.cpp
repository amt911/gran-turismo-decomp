// Test mod for the draw API (tools/mod_draw_check.sh): draws known shapes and sets a grade from its settings. Not a shipped mod.
//   draw  = 1      overlay on: a red rect (40,40,100x60), a half-transparent blue rect (200,200,60x40), white text at (200,100) size 24, a green line
//                  (300,200)-(440,250) 6 wide; 0 = the overlay callback draws nothing
//   grade = "r g b brightness saturation"   scene_grade at init (empty = never called)
#include "gt_mod/gt_mod_api.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
const GtModApi *g_api = nullptr;
bool g_draw = true;

void overlay(GtMod *self, GtModCtx *) {
    if (!g_draw) return;
    g_api->draw_rect(self, 40.0f, 40.0f, 100.0f, 60.0f, 0xFF0000FFu);
    g_api->draw_rect(self, 200.0f, 200.0f, 60.0f, 40.0f, 0x0000FF80u);
    g_api->draw_text(self, 200.0f, 100.0f, 24.0f, 0xFFFFFFFFu, "GT DRAW 0123");
    g_api->draw_line(self, 300.0f, 200.0f, 440.0f, 250.0f, 6.0f, 0x00FF00FFu);
}
}  // namespace

extern "C" int gt_mod_init(const GtModApi *api, GtMod *self) {
    if (api->version < GT_MOD_API_VERSION_8) return 1;
    g_api = api;
    const char *draw = api->config(self, "draw");
    g_draw = draw == nullptr || std::strcmp(draw, "0") != 0;
    const char *grade = api->config(self, "grade");
    if (grade != nullptr && *grade != '\0') {
        float v[5] = {1, 1, 1, 1, 1};
        std::sscanf(grade, "%f %f %f %f %f", &v[0], &v[1], &v[2], &v[3], &v[4]);
        if (api->scene_grade(self, v[0], v[1], v[2], v[3], v[4]) != 0) return 2;
        std::fprintf(stderr, "[drawprobe] grade %g %g %g %g %g\n", v[0], v[1], v[2], v[3], v[4]);
    }
    if (api->on_overlay(self, overlay) != 0) return 3;
    api->log(self, g_draw ? "draw probe: overlay on" : "draw probe: overlay registered, drawing nothing");
    return 0;
}
