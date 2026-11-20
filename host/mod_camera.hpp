// Mod API v9 camera (docs/superpowers/specs/2026-11-18-extra-cameras-design.md): GT_HOOK_CAMERA, camera_get / camera_set and replay_active.
//
// The game folds view x model on the CPU (FACTS "Camera and the world-matrix upload"): each layer of the scene renderer loads the camera's 4x4 VIEW matrix into the GU with
// `0x08B568DC(matrix)` (called from the layer loop `0x08AF1524`, whose scratch matrix is the argument). The host hooks that load for every mod that declared `api >= 9` and runs the
// mods' GT_HOOK_CAMERA functions in front of it, only for the PLAYER camera's view (the active camera of the scene object `S`, which the scene refresh `0x08928A80` names):
// the matrix being loaded is that camera's view (basis = transpose of its camera-to-world basis, translation = -eye . R^T) within a tolerance, which is how the
// rear-view mirror pass and the sky layers are told apart. A GT_HOOK_CAMERA function reads the pose with camera_get and may replace it with camera_set; the host calls the
// game's load afterwards (the function must NOT call call_original).
#pragma once

#include "mod_hooks.hpp"

#include <cstdint>
#include <string>

namespace psprecomp {
class Runtime;
}

namespace gt::mods::camera {

constexpr std::uint32_t kSceneRefresh = 0x08928A80u;  // a0 = scene object S (once per physics step)
constexpr std::uint32_t kSceneRenderer = 0x08AF1524u; // the layer loop: its stack frame holds the scratch matrix the view loads read
constexpr std::uint32_t kViewLoad = 0x08B568DCu;      // a0 = the 4x4 view matrix (row vectors)
constexpr std::uint32_t kSceneCamera = 0x1FA4u, kCameraMatrix = 0x10u;  // word(S + 0x1FA4) = the active camera; its camera-to-world matrix is at +0x10 (row 3 = eye)
// The game's view cycle. The step `0x08927D14(S, list)` (once per frame in a race; `list` = {+0 entries ptr (8 bytes each: camera index, parameter), +4 count, +8 position, +0xC previous position}) reads the
// cycle button, moves `list+8` by +-1 with wrap, and calls `0x089274C8(S, list)`, which makes `S[0x1F70 + 4 * entry]` the active camera (`S+0x1FA4`); afterwards it copies `list+8` into `list+0xC`
// (decompiled form: src/game/unsorted/hot_batch_294.cpp, `tools.disasm 08927D14`). `0x08927D14` is reached in-unit in the recompiled build (a hook on it never fires), `0x089274C8` is dispatched (FACTS "View cycle").
// GT_HOOK_VIEW_CYCLE hooks `0x089274C8`: when position != previous the call is a press; the mod may change the position (view_cycle_set) before call_original and the game applies that one.
constexpr std::uint32_t kViewCycle = 0x089274C8u;
constexpr std::uint32_t kLoopFrame = 0x140u, kLoopScratch = 0xC0u;      // the loop's frame size and the scratch matrix inside it

// Pure maths (host/tests/mod_camera_test.cpp). The renderer's frame is Y-up (x, y, z); the physics frame the cars live in is Z-up: p = (x, -z, y).
// A pose is the eye and three rows in the PHYSICS frame, the car's convention (forward, right, up: left-handed rows, GtCarState::basis).
// `view` = the 16 floats of the view matrix (row vectors; rows 0..2 hold the camera axes as columns, row 3 the translation).
bool view_to_pose(const float view[16], float eye[3], float basis[9]);
void pose_to_view(const float eye[3], const float basis[9], float view[16]);
// Is `view` the view of the camera whose camera-to-world matrix is `cam` (tolerances of the orbit-camera mod, measured in game)?
bool view_matches_camera(const float view[16], const float cam[16]);

// api functions (a mod that declared api >= 9; valid inside its GT_HOOK_CAMERA function).
int api_camera_get(GtMod *self, GtModCtx *ctx, float eye[3], float basis[9]);
int api_camera_set(GtMod *self, GtModCtx *ctx, const float eye[3], const float basis[9]);
int api_replay_active(GtMod *self, GtModCtx *ctx);
// In a GT_HOOK_CAMERA function: the focal lengths, in pixels of the 480 x 272 picture, of the 3D projection the game has loaded for the world pass (the GU object's projection pointer
// `word(0x08D6CE00 + 0x2E0 + 0xD04)`, FACTS "Camera and the world-matrix upload"): x_pixel = 240 + focal_x * x_view / -z_view, y_pixel = 136 - focal_y * y_view / -z_view. 0 = ok; non-zero outside a camera
// function or when the loaded projection is not a perspective one (m[11] != -1: the HUD's orthographic one).
int api_camera_projection(GtMod *self, GtModCtx *ctx, float *focal_x, float *focal_y);
constexpr std::uint32_t kGuObject = 0x08D6CE00u, kGuBlock = 0x2E0u, kGuProjection = 0xD04u;

// In a GT_HOOK_VIEW_CYCLE function: the list's position, previous position and entry count (0 = ok; non-zero outside the hook or for a list that does not check out: count 1..32, positions < count).
int api_view_cycle(GtMod *self, GtModCtx *ctx, std::uint32_t *position, std::uint32_t *previous, std::uint32_t *count);
// Moves the position (< count): the game applies that entry's camera when the mod calls call_original (the step copies it into the previous position afterwards). 0 = ok.
int api_view_cycle_set(GtMod *self, GtModCtx *ctx, std::uint32_t position);
// hook_point(GT_HOOK_CAMERA): registers the mod's function (one per mod; during gt_mod_init). 0 = ok.
int api_camera_hook(GtMod *self, GtHookFn fn);

// The world of the latest physics step (the trampoline notes it): replay_active reads its cars.
void note_world(std::uint32_t world);

// The loader calls this for a mod with api >= 9, before its gt_mod_init: installs the three hooks owned by `mod`.
bool install(psprecomp::Runtime &rt, GtMod *mod, std::string &error);
void forget(const GtMod *mod);
void clear_for_tests();

}  // namespace gt::mods::camera
