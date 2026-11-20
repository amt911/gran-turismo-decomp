#include "mod_camera.hpp"

#include "game_state.hpp"
#include "mod_services.hpp"

#include "psprecomp/runtime.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace gt::mods::camera {
namespace {

constexpr float kBasisTolerance = 0.25f, kTranslationTolerance = 8.0f;  // orbit-camera 0.x, measured: the rear-view mirror pass loads another camera's view

struct Hook {
    const GtMod *mod;
    GtHookFn fn;
};
std::vector<Hook> &hooks() {
    static std::vector<Hook> v;
    return v;
}
// Guest threads are scheduled cooperatively on one OS thread; plain statics (as the tilt module).
std::uint32_t g_scene = 0, g_loop_scratch = 0;
std::atomic<std::uint32_t> g_world{0};
// The view the host is handing to the mods right now (valid only inside a camera hook).
const GtModCtx *g_view_ctx = nullptr;
std::uint32_t g_view_address = 0;  // the validated address of that view (64 bytes inside guest memory): a mod's set_gpr or a call_guest cannot redirect camera_get / camera_set
// The view the last camera_set wrote: another mod's link of the SAME load sees an edited matrix, which no longer matches the scene camera, and must still be given it.
float g_edited[16];
bool g_have_edited = false;

void call_next(GtModCtx *c) { mod_api(GT_MOD_API_VERSION_7).call_original(c); }

float load_f(psprecomp::Runtime &rt, std::uint32_t address) {
    const std::uint32_t bits = rt.memory().load32(address);
    float v;
    std::memcpy(&v, &bits, sizeof v);
    return v;
}
void store_f(psprecomp::Runtime &rt, std::uint32_t address, float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    rt.memory().store32(address, bits);
}

// GT_MOD_CAMERA_DEBUG=1: counters of the view loads (diagnosis of a build where the camera function never runs), printed every 600 loads.
struct Counters {
    unsigned loads = 0, scratch = 0, camera = 0, matched = 0, scenes = 0, renders = 0;
};
Counters g_counters;
bool debug_on() {
    static const bool on = std::getenv("GT_MOD_CAMERA_DEBUG") != nullptr;
    return on;
}

void on_scene_refresh(GtModCtx *c) {
    ++g_counters.scenes;
    const std::uint32_t s = c->ctx->gpr[4];
    if (s != 0u && c->rt->memory().contains(s + kSceneCamera, 4u) && c->rt->memory().load32(s + kSceneCamera) != 0u) g_scene = s;
    call_next(c);
}

// NOT a "while inside" flag: call_original returns before the recompiled loop body has run its calls (FACTS "Camera"); only the latest entry's scratch address is kept.
void on_scene_render(GtModCtx *c) {
    ++g_counters.renders;
    g_loop_scratch = c->ctx->gpr[29] - kLoopFrame + kLoopScratch;
    call_next(c);
}

void on_view_load(GtModCtx *c) {
    psprecomp::Runtime &rt = *c->rt;
    if (debug_on() && ++g_counters.loads % 600u == 0u)
        std::fprintf(stderr, "[camera] loads=%u scratch_ok=%u camera_ok=%u matched=%u refresh_calls=%u render_calls=%u scene=%08x scratch=%08x\n", g_counters.loads, g_counters.scratch, g_counters.camera, g_counters.matched,
                     g_counters.scenes, g_counters.renders, g_scene, g_loop_scratch);
    const std::uint32_t view_address = c->ctx->gpr[4];
    if (g_loop_scratch == 0u || view_address != g_loop_scratch || g_scene == 0u || !rt.memory().contains(view_address, 64u) || !rt.memory().contains(g_scene + kSceneCamera, 4u)) {
        call_next(c);
        return;
    }
    ++g_counters.scratch;
    const std::uint32_t cam = rt.memory().load32(g_scene + kSceneCamera);
    if (cam == 0u || !rt.memory().contains(cam + kCameraMatrix, 64u)) {
        call_next(c);
        return;
    }
    ++g_counters.camera;
    float view[16], cm[16];
    for (int i = 0; i < 16; ++i) {
        view[i] = load_f(rt, view_address + 4u * i);
        cm[i] = load_f(rt, cam + kCameraMatrix + 4u * i);
    }
    bool same_as_edit = g_have_edited;
    for (int i = 0; i < 16 && same_as_edit; ++i) same_as_edit = view[i] == g_edited[i];
    if (view_matches_camera(view, cm) || same_as_edit) {
        ++g_counters.matched;
        g_have_edited = false;  // a new load starts from the game's matrix
        // the hooks of the active mods that registered one; this link's own mod first (the chain gives every mod's link a turn)
        for (const Hook &h : hooks())
            if (h.mod == c->mod && c->mod != nullptr && c->mod->active.load(std::memory_order_acquire)) {
                g_view_ctx = c;
                g_view_address = view_address;
                h.fn(c);
                g_view_ctx = nullptr;
                g_view_address = 0u;
            }
    }
    call_next(c);
}

}  // namespace

bool view_to_pose(const float view[16], float eye[3], float basis[9]) {
    for (int i = 0; i < 16; ++i)
        if (!std::isfinite(view[i])) return false;
    // camera axes in the render frame: row j of the camera-to-world matrix = column j of the view's 3x3
    float right[3], up[3], back[3], t[3] = {view[12], view[13], view[14]};
    for (int k = 0; k < 3; ++k) {
        right[k] = view[4 * k + 0];
        up[k] = view[4 * k + 1];
        back[k] = view[4 * k + 2];
    }
    // eye = -t . R (R = the camera-to-world 3x3: rows right, up, back)
    float e[3];
    for (int k = 0; k < 3; ++k) e[k] = -(t[0] * right[k] + t[1] * up[k] + t[2] * back[k]);
    const auto to_physics = [](const float v[3], float out[3]) {  // render (x, y, z) -> physics (x, -z, y)
        out[0] = v[0];
        out[1] = -v[2];
        out[2] = v[1];
    };
    to_physics(e, eye);
    float forward[3] = {-back[0], -back[1], -back[2]};
    to_physics(forward, basis + 0);
    to_physics(right, basis + 3);
    to_physics(up, basis + 6);
    return true;
}

void pose_to_view(const float eye[3], const float basis[9], float view[16]) {
    const auto to_render = [](const float p[3], float out[3]) {  // physics (x, y, z) -> render (x, z, -y)
        out[0] = p[0];
        out[1] = p[2];
        out[2] = -p[1];
    };
    float e[3], forward[3], right[3], up[3];
    to_render(eye, e);
    to_render(basis + 0, forward);
    to_render(basis + 3, right);
    to_render(basis + 6, up);
    const float back[3] = {-forward[0], -forward[1], -forward[2]};
    for (int k = 0; k < 3; ++k) {
        view[4 * k + 0] = right[k];
        view[4 * k + 1] = up[k];
        view[4 * k + 2] = back[k];
        view[4 * k + 3] = 0.0f;
    }
    view[12] = -(e[0] * right[0] + e[1] * right[1] + e[2] * right[2]);
    view[13] = -(e[0] * up[0] + e[1] * up[1] + e[2] * up[2]);
    view[14] = -(e[0] * back[0] + e[1] * back[1] + e[2] * back[2]);
    view[15] = 1.0f;
}

bool view_matches_camera(const float v[16], const float cm[16]) {
    float basis_err = 0.0f, trans_err = 0.0f;
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j) basis_err = std::max(basis_err, std::fabs(v[4 * i + j] - cm[4 * j + i]));
    for (int j = 0; j < 3; ++j) {
        float e = 0.0f;
        for (int k = 0; k < 3; ++k) e += cm[12 + k] * cm[4 * j + k];
        trans_err = std::max(trans_err, std::fabs(v[12 + j] + e));
    }
    return basis_err < kBasisTolerance && trans_err < kTranslationTolerance;
}

namespace {
bool camera_ok(GtMod *self, const char *what) {
    if (self == nullptr) return false;
    if (self->api_version < GT_MOD_API_VERSION_9) {
        std::fprintf(stderr, "[mod:%s] %s refused: the mod declared api %d (needs api = 9)\n", self->id.c_str(), what, self->api_version);
        return false;
    }
    return true;
}
}  // namespace

int api_camera_get(GtMod *self, GtModCtx *ctx, float eye[3], float basis[9]) {
    if (!camera_ok(self, "camera_get") || ctx == nullptr || eye == nullptr || basis == nullptr || g_view_ctx != ctx) return 1;  // only inside a camera hook
    float view[16];
    const std::uint32_t a = g_view_address;
    for (int i = 0; i < 16; ++i) view[i] = load_f(*ctx->rt, a + 4u * i);
    return view_to_pose(view, eye, basis) ? 0 : 1;
}

int api_camera_set(GtMod *self, GtModCtx *ctx, const float eye[3], const float basis[9]) {
    if (!camera_ok(self, "camera_set") || ctx == nullptr || eye == nullptr || basis == nullptr || g_view_ctx != ctx) return 1;
    for (int i = 0; i < 3; ++i)
        if (!std::isfinite(eye[i])) return 1;
    for (int i = 0; i < 9; ++i)
        if (!std::isfinite(basis[i])) return 1;
    // rows must be a rotation (unit, mutually orthogonal), or the picture shears
    for (int r = 0; r < 3; ++r) {
        float n = 0.0f;
        for (int k = 0; k < 3; ++k) n += basis[3 * r + k] * basis[3 * r + k];
        if (std::fabs(n - 1.0f) > 1e-2f) return 1;
        for (int s = r + 1; s < 3; ++s) {
            float d = 0.0f;
            for (int k = 0; k < 3; ++k) d += basis[3 * r + k] * basis[3 * s + k];
            if (std::fabs(d) > 1e-2f) return 1;
        }
    }
    // handedness: the car's rows are left-handed (forward x right . up = -1); a reflected set would flip the culling
    {
        const float cx = basis[1] * basis[5] - basis[2] * basis[4], cy = basis[2] * basis[3] - basis[0] * basis[5], cz = basis[0] * basis[4] - basis[1] * basis[3];
        if (cx * basis[6] + cy * basis[7] + cz * basis[8] > -0.5f) return 1;
    }
    float view[16];
    pose_to_view(eye, basis, view);
    const std::uint32_t a = g_view_address;
    for (int i = 0; i < 16; ++i) store_f(*ctx->rt, a + 4u * i, view[i]);
    std::memcpy(g_edited, view, sizeof g_edited);
    g_have_edited = true;  // the renderer's stack temporary: nothing else sees the edit
    return 0;
}

void note_world(std::uint32_t world) { g_world.store(world, std::memory_order_release); }

int api_replay_active(GtMod *self, GtModCtx *ctx) {
    if (!camera_ok(self, "replay_active") || ctx == nullptr) return 0;
    const std::uint32_t world = g_world.load(std::memory_order_acquire);
    if (world == 0u || !services::racing(30)) return 0;
    gt::game_state::Car cars[gt::game_state::kMaxCars];
    const std::uint32_t n = gt::game_state::race_cars(ctx->rt->memory(), world, cars);
    for (std::uint32_t i = 0; i < n; ++i)
        if (cars[i].role == gt::game_state::CarRole::ReplayHuman) return 1;
    return 0;
}

int api_camera_projection(GtMod *self, GtModCtx *ctx, float *focal_x, float *focal_y) {
    if (!camera_ok(self, "camera_projection") || ctx == nullptr || focal_x == nullptr || focal_y == nullptr || g_view_ctx != ctx) return 1;
    psprecomp::Runtime &rt = *ctx->rt;
    if (!rt.memory().contains(kGuObject + kGuBlock + kGuProjection, 4u)) return 1;
    const std::uint32_t p = rt.memory().load32(kGuObject + kGuBlock + kGuProjection);
    if (p == 0u || !rt.memory().contains(p, 64u)) return 1;
    const float m0 = load_f(rt, p), m5 = load_f(rt, p + 4u * 5u), m11 = load_f(rt, p + 4u * 11u);
    if (!std::isfinite(m0) || !std::isfinite(m5) || std::fabs(m11 + 1.0f) > 1e-3f || m0 == 0.0f || m5 == 0.0f) return 1;
    *focal_x = 240.0f * std::fabs(m0);
    *focal_y = 136.0f * std::fabs(m5);
    return 0;
}

namespace {
// The list of the view-cycle hook this call is for, validated; false outside the hook (the address is the hooked function's) or for a list that does not check out.
bool view_list(GtMod *self, GtModCtx *ctx, std::uint32_t *list, std::uint32_t *position, std::uint32_t *previous, std::uint32_t *count, const char *what) {
    if (!camera_ok(self, what) || ctx == nullptr || ctx->ctx == nullptr || ctx->address != kViewCycle) return false;
    const std::uint32_t l = ctx->ctx->gpr[5];
    if (!ctx->rt->memory().contains(l, 16u)) return false;
    const std::uint32_t n = ctx->rt->memory().load32(l + 4u), p = ctx->rt->memory().load32(l + 8u), q = ctx->rt->memory().load32(l + 12u);
    if (n == 0u || n > 32u || p >= n || q >= n) return false;
    *list = l;
    *position = p;
    *previous = q;
    *count = n;
    return true;
}
}  // namespace

int api_view_cycle(GtMod *self, GtModCtx *ctx, std::uint32_t *position, std::uint32_t *previous, std::uint32_t *count) {
    std::uint32_t list, p, q, n;
    if (position == nullptr || previous == nullptr || count == nullptr || !view_list(self, ctx, &list, &p, &q, &n, "view_cycle")) return 1;
    *position = p;
    *previous = q;
    *count = n;
    return 0;
}

int api_view_cycle_set(GtMod *self, GtModCtx *ctx, std::uint32_t position) {
    std::uint32_t list, p, q, n;
    if (!view_list(self, ctx, &list, &p, &q, &n, "view_cycle_set") || position >= n) return 1;
    ctx->rt->memory().store32(list + 8u, position);
    return 0;
}

int api_camera_hook(GtMod *self, GtHookFn fn) {
    if (!camera_ok(self, "hook_point(GT_HOOK_CAMERA)") || fn == nullptr) return 1;
    auto &v = hooks();
    if (std::any_of(v.begin(), v.end(), [&](const Hook &h) { return h.mod == self; })) {
        std::fprintf(stderr, "[mod:%s] camera hook refused: one per mod\n", self->id.c_str());
        return 1;
    }
    v.push_back(Hook{self, fn});
    return 0;
}

bool install(psprecomp::Runtime &rt, GtMod *mod, std::string &error) {
    const HookOwner owner{mod->id, mod};
    return install_hook(rt, kSceneRefresh, on_scene_refresh, owner, error) && install_hook(rt, kSceneRenderer, on_scene_render, owner, error) &&
           install_hook(rt, kViewLoad, on_view_load, owner, error);
}

void forget(const GtMod *mod) {
    auto &v = hooks();
    v.erase(std::remove_if(v.begin(), v.end(), [mod](const Hook &h) { return h.mod == mod; }), v.end());
}

void clear_for_tests() {
    hooks().clear();
    g_scene = g_loop_scratch = 0u;
    g_world.store(0u);
    g_view_ctx = nullptr;
    g_view_address = 0u;
    g_have_edited = false;
}

}  // namespace gt::mods::camera
