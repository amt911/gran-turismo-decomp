// host/mod_camera.cpp: the view <-> pose maths, the view-matches-camera test, and the hook point through the dispatcher (docs/superpowers/specs/2026-11-18-extra-cameras-design.md).
#include "mod_camera.hpp"

#include "mod_hooks.hpp"

#include "psprecomp/runtime.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
bool near(float a, float b, float eps = 1e-4f) { return std::fabs(a - b) <= eps; }

using namespace gt::mods;

// A render-frame camera-to-world matrix (rows right, up, back, eye) and its view matrix (the inverse), written out by hand for a camera at eye looking along the render -z with no roll.
void identity_view(const float eye[3], float view[16]) {
    std::memset(view, 0, 16 * sizeof(float));
    view[0] = view[5] = view[10] = view[15] = 1.0f;
    for (int k = 0; k < 3; ++k) view[12 + k] = -eye[k];
}

// The game's functions the fake runtime provides: the scene refresh (stores nothing), the layer loop, the view load which records the matrix it was given.
float g_loaded[16];
bool g_loaded_any = false;
void scene_refresh(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.pc = ctx.gpr[31]; }
void scene_render(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) { ctx.pc = ctx.gpr[31]; }
void view_load(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    for (int i = 0; i < 16; ++i) {
        const std::uint32_t bits = rt.memory().load32(ctx.gpr[4] + 4u * i);
        std::memcpy(&g_loaded[i], &bits, 4);
    }
    g_loaded_any = true;
    ctx.pc = ctx.gpr[31];
}
float g_eye_seen[3], g_basis_seen[9];
int g_get_status = -1, g_set_status = -1;
const float *g_set_eye = nullptr, *g_set_basis = nullptr;
GtMod *g_mod = nullptr;
void camera_fn(GtModCtx *c) {
    const GtModApi &api = mod_api(GT_MOD_API_VERSION_9);
    g_get_status = api.camera_get(g_mod, c, g_eye_seen, g_basis_seen);
    if (g_set_eye != nullptr) g_set_status = api.camera_set(g_mod, c, g_set_eye, g_set_basis);
}

float g_b_eye[3];
int g_b_status = -9;
GtMod *g_mod_b = nullptr;
void camera_fn_b(GtModCtx *c) { g_b_status = mod_api(GT_MOD_API_VERSION_9).camera_get(g_mod_b, c, g_b_eye, g_basis_seen); }

void write_matrix(psprecomp::Runtime &rt, std::uint32_t at, const float m[16]) {
    for (int i = 0; i < 16; ++i) {
        std::uint32_t bits;
        std::memcpy(&bits, &m[i], 4);
        rt.memory().store32(at + 4u * i, bits);
    }
}
}  // namespace

int main() {
    // the identity camera at the origin: forward = physics +y (render -z), right +x, up +z (render y)
    {
        const float eye[3] = {0, 0, 0};
        float view[16], e[3], b[9];
        identity_view(eye, view);
        check(camera::view_to_pose(view, e, b), "finite view converts");
        check(near(e[0], 0) && near(e[1], 0) && near(e[2], 0), "eye at the origin");
        check(near(b[0], 0) && near(b[1], 1) && near(b[2], 0), "forward = +y in the physics frame");
        check(near(b[3], 1) && near(b[4], 0) && near(b[5], 0), "right = +x");
        check(near(b[6], 0) && near(b[7], 0) && near(b[8], 1), "up = +z");
        // the car's convention: left-handed rows, forward x right . up = -1
        const float cx = b[1] * b[5] - b[2] * b[4], cy = b[2] * b[3] - b[0] * b[5], cz = b[0] * b[4] - b[1] * b[3];
        check(near(cx * b[6] + cy * b[7] + cz * b[8], -1.0f), "rows are left-handed like the car's basis");
    }
    // an eye position survives the round trip, in the physics frame (render (3, 5, -7) = physics (3, 7, 5))
    {
        const float eye_render[3] = {3, 5, -7};
        float view[16], e[3], b[9], back[16];
        identity_view(eye_render, view);
        check(camera::view_to_pose(view, e, b), "convert");
        check(near(e[0], 3) && near(e[1], 7) && near(e[2], 5), "eye in the physics frame: (x, -z, y)");
        camera::pose_to_view(e, b, back);
        bool same = true;
        for (int i = 0; i < 16; ++i) same = same && near(view[i], back[i]);
        check(same, "pose -> view reproduces the view");
    }
    // a rotated camera (yaw 90 degrees about render y) round-trips and looks where it should
    {
        const float c = 0.0f, s = 1.0f;  // right = (c, 0, -s)?? build rows of the camera-to-world: right, up, back
        const float right[3] = {c, 0, -s}, up[3] = {0, 1, 0}, back[3] = {s, 0, c};
        float view[16] = {};
        for (int k = 0; k < 3; ++k) {
            view[4 * k + 0] = right[k];
            view[4 * k + 1] = up[k];
            view[4 * k + 2] = back[k];
        }
        view[15] = 1.0f;
        float e[3], b[9], back_view[16];
        check(camera::view_to_pose(view, e, b), "convert rotated");
        // forward_render = -back = (-1, 0, 0) -> physics (-1, 0, 0)
        check(near(b[0], -1) && near(b[1], 0) && near(b[2], 0), "yawed camera looks along -x");
        camera::pose_to_view(e, b, back_view);
        bool same = true;
        for (int i = 0; i < 16; ++i) same = same && near(view[i], back_view[i]);
        check(same, "round trip of the rotated view");
    }
    // non-finite input is refused
    {
        float view[16] = {};
        view[0] = view[5] = view[10] = view[15] = 1.0f;
        view[12] = NAN;
        float e[3], b[9];
        check(!camera::view_to_pose(view, e, b), "a NaN view is refused");
    }
    // the matches test: a view that is the inverse of the camera-to-world passes, another camera's does not
    {
        const float eye[3] = {10, 2, -4};
        float cam[16] = {};
        cam[0] = cam[5] = cam[10] = cam[15] = 1.0f;
        for (int k = 0; k < 3; ++k) cam[12 + k] = eye[k];
        float view[16];
        identity_view(eye, view);
        check(camera::view_matches_camera(view, cam), "the camera's own view matches");
        const float other[3] = {200, 2, -4};
        identity_view(other, view);
        check(!camera::view_matches_camera(view, cam), "a view 190 m away (the mirror pass) does not match");
        view[0] = -1.0f;
        view[5] = -1.0f;
        float wrong[16];
        std::memcpy(wrong, view, sizeof wrong);
        check(!camera::view_matches_camera(wrong, cam), "a rotated view does not match");
    }
    // through the dispatcher: the hook runs for the player camera's view and may replace it; other loads pass untouched
    {
        psprecomp::Runtime rt;
        rt.register_function(camera::kSceneRefresh, &scene_refresh, "refresh");
        rt.register_function(camera::kSceneRenderer, &scene_render, "render");
        rt.register_function(camera::kViewLoad, &view_load, "load");
        GtMod mod;
        mod.id = "cams";
        mod.api_version = 9;
        mod.active.store(true);
        g_mod = &mod;
        std::string error;
        check(camera::install(rt, &mod, error), "install the camera hooks");
        check(camera::api_camera_hook(&mod, camera_fn) == 0, "register the camera function");
        check(camera::api_camera_hook(&mod, camera_fn) != 0, "one per mod");
        // guest memory: the scene object S, its camera, the layer loop's scratch matrix
        const std::uint32_t S = 0x09000000u, CAM = 0x09100000u, SP = 0x09200000u;
        const float eye[3] = {10, 2, -4};
        float cam[16] = {};
        cam[0] = cam[5] = cam[10] = cam[15] = 1.0f;
        for (int k = 0; k < 3; ++k) cam[12 + k] = eye[k];
        write_matrix(rt, CAM + camera::kCameraMatrix, cam);
        rt.memory().store32(S + camera::kSceneCamera, CAM);
        psprecomp::AllegrexContext ctx{};
        ctx.gpr[31] = 0x08800100u;
        ctx.pc = camera::kSceneRefresh;
        ctx.gpr[4] = S;
        rt.function_at(camera::kSceneRefresh)(rt, ctx);
        ctx.pc = camera::kSceneRenderer;
        ctx.gpr[29] = SP + camera::kLoopFrame;  // the loop's entry sp; the scratch is sp - 0x140 + 0xC0 = SP + 0xC0
        rt.function_at(camera::kSceneRenderer)(rt, ctx);
        const std::uint32_t scratch = SP + camera::kLoopScratch;
        float view[16];
        identity_view(eye, view);
        write_matrix(rt, scratch, view);
        auto load = [&](std::uint32_t at) {
            g_loaded_any = false;
            psprecomp::AllegrexContext c{};
            c.pc = camera::kViewLoad;
            c.gpr[4] = at;
            c.gpr[31] = 0x08800100u;
            rt.function_at(camera::kViewLoad)(rt, c);
        };
        // untouched: the hook reads the pose
        g_set_eye = nullptr;
        load(scratch);
        check(g_loaded_any && g_get_status == 0, "camera_get works inside the hook");
        check(near(g_eye_seen[0], 10) && near(g_eye_seen[1], 4) && near(g_eye_seen[2], 2), "the eye the hook saw, in the physics frame");
        // replacing the pose: the game's load receives the new view
        const float new_eye[3] = {0, 0, 50};
        const float basis[9] = {0, 1, 0, 1, 0, 0, 0, 0, 1};
        g_set_eye = new_eye;
        g_set_basis = basis;
        write_matrix(rt, scratch, view);
        load(scratch);
        float expect[16];
        camera::pose_to_view(new_eye, basis, expect);
        bool same = g_set_status == 0;
        for (int i = 0; i < 16; ++i) same = same && near(g_loaded[i], expect[i]);
        check(same, "camera_set reaches the game's load");
        // a matrix that is not the scratch (another layer's, the HUD's): the hook does not run, the game gets the matrix as it was
        g_get_status = -2;
        const std::uint32_t other = SP + 0x400u;
        write_matrix(rt, other, view);
        load(other);
        check(g_get_status == -2 && near(g_loaded[12], view[12]), "a load that is not the layer loop's scratch passes untouched");
        // the scratch but another camera's view (the mirror pass): untouched
        const float far_eye[3] = {500, 2, -4};
        float far_view[16];
        identity_view(far_eye, far_view);
        write_matrix(rt, scratch, far_view);
        load(scratch);
        check(g_get_status == -2 && near(g_loaded[12], far_view[12]), "another camera's view is not given to the hook");
        // refusals: bad rows, outside a hook
        write_matrix(rt, scratch, view);
        const float skewed[9] = {0, 1, 0, 1, 0.5f, 0, 0, 0, 1};
        g_set_basis = skewed;
        g_set_status = -1;
        load(scratch);
        check(g_set_status != 0 && g_set_status != -1, "rows that are not a rotation are refused");
        const float mirrored[9] = {0, 1, 0, -1, 0, 0, 0, 0, 1};  // right-handed: forward x right . up = +1
        g_set_basis = mirrored;
        g_set_status = -1;
        write_matrix(rt, scratch, view);
        load(scratch);
        check(g_set_status != 0 && g_set_status != -1, "a right-handed (mirrored) set is refused");
        psprecomp::AllegrexContext cc{};
        GtModCtx fake{&rt, &cc, 0, 0};
        float e[3], b[9];
        check(camera::api_camera_get(&mod, &fake, e, b) != 0, "camera_get outside a camera hook is refused");
        // a switched-off mod's function does not run
        mod.active.store(false);
        g_get_status = -3;
        g_set_eye = nullptr;
        write_matrix(rt, scratch, view);
        load(scratch);
        check(g_get_status == -3, "a switched-off mod's camera function does not run");
        mod.api_version = 7;
        check(camera::api_camera_hook(&mod, camera_fn) != 0 && camera::api_replay_active(&mod, &fake) == 0, "api < 9 refuses the v9 functions");
        camera::clear_for_tests();
    }
    clear_hooks_for_tests();
    // two mods with camera functions: the second still runs after the first replaced the pose, and sees the first's pose (the match test is against the game's view, not the edited one)
    {
        psprecomp::Runtime rt;
        rt.register_function(camera::kSceneRefresh, &scene_refresh, "refresh");
        rt.register_function(camera::kSceneRenderer, &scene_render, "render");
        rt.register_function(camera::kViewLoad, &view_load, "load");
        GtMod a, b;
        a.id = "cams-a"; a.api_version = 9; a.active.store(true);
        b.id = "cams-b"; b.api_version = 9; b.active.store(true);
        g_mod = &a;
        g_mod_b = &b;
        std::string error;
        check(camera::install(rt, &a, error) && camera::install(rt, &b, error), "install two mods");
        check(camera::api_camera_hook(&a, camera_fn) == 0 && camera::api_camera_hook(&b, camera_fn_b) == 0, "both register");
        const std::uint32_t S = 0x09000000u, CAM = 0x09100000u, SP = 0x09200000u;
        const float eye[3] = {10, 2, -4};
        float cam[16] = {};
        cam[0] = cam[5] = cam[10] = cam[15] = 1.0f;
        for (int k = 0; k < 3; ++k) cam[12 + k] = eye[k];
        write_matrix(rt, CAM + camera::kCameraMatrix, cam);
        rt.memory().store32(S + camera::kSceneCamera, CAM);
        psprecomp::AllegrexContext ctx{};
        ctx.gpr[31] = 0x08800100u;
        ctx.gpr[4] = S;
        ctx.pc = camera::kSceneRefresh;
        rt.function_at(camera::kSceneRefresh)(rt, ctx);
        ctx.pc = camera::kSceneRenderer;
        ctx.gpr[29] = SP + camera::kLoopFrame;
        rt.function_at(camera::kSceneRenderer)(rt, ctx);
        const std::uint32_t scratch = SP + camera::kLoopScratch;
        float view[16];
        identity_view(eye, view);
        write_matrix(rt, scratch, view);
        const float tv_eye[3] = {0, 0, 50};
        const float basis[9] = {0, 1, 0, 1, 0, 0, 0, 0, 1};
        g_set_eye = tv_eye;
        g_set_basis = basis;
        g_b_status = -9;
        psprecomp::AllegrexContext c{};
        c.pc = camera::kViewLoad;
        c.gpr[4] = scratch;
        c.gpr[31] = 0x08800100u;
        rt.function_at(camera::kViewLoad)(rt, c);
        check(g_b_status == 0 && near(g_b_eye[0], 0) && near(g_b_eye[1], 0) && near(g_b_eye[2], 50), "the second mod's camera function runs and sees the first one's pose");
        camera::clear_for_tests();
    }
    if (failures == 0) std::puts("mod_camera_test: all checks passed");
    return failures == 0 ? 0 : 1;
}
