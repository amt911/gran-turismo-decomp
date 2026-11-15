# Crash deformation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** a `deformation` mod that dents the player car's body vertices on a hit, for the race only, through named SDK accessors.

**Architecture:** the host gets SDK v5 (appended to `GtModApi`): named hook points, a per-car impact accessor computed from the velocity change, and mesh accessors over the GE "blobs" (decode, write a vertex, copy-on-write copy with absolute VADRs rewritten, redirect the draw). The mod is pure C with a testable core (impact threshold, dent falloff and cap, restore); it never names a guest address.

**Tech Stack:** C++20 host (`host/`, ctest unit tests), C99 mod in `../gt-mods/deformation`, bash/python in-game check `tools/mod_deformation_check.sh`.

**Spec:** `docs/superpowers/specs/2026-11-07-crash-deformation-design.md` (approved, PR #631); SDK rules `docs/superpowers/specs/2026-11-10-named-mod-sdk-design.md`; collision research `docs/FACTS.md` "Collision code ... NOT FOUND" (PR #633).

## Global Constraints

- Player car only (copy-on-write); AI cars of the same model stay clean.
- Dents last for the race only: restored on race end and on toggle off; nothing saved.
- Contact from the delta-v fallback: `dv = v - v_prev` from `K+0x6CC..0x6D4`, rotated into the body frame with the rows `K+0x1C/+0x28/+0x34`; contact point = the `-dv` direction against the body box ("Rollover body box"); wall vs ground from the `0x300` surface word and the yaw-rate jump `K+0x6F8`.
- Blob vertex formats `0x122` (stride 14, s16 position at +8), `0x130` and `0x112` (stride 12, position at +6); blobs drawn through the leaf `0x08B550A8(blob)`; addresses found at run time.
- Mods use only named SDK calls (no hex literal in the guest range in the mod source).
- Cosmetic only: normals untouched, no inside-out (cap), body blobs only.

## Review Focus

- A hit in a replay or with two humans (no single player car): no dent, no crash.
- A blob whose header does not check out (not BASE/VADR/VTYPE/PRIM/RET): `mesh_info` refuses, nothing is written.
- Many hits accumulating: total displacement stays under the cap.
- Toggle off / race end with dents applied: vertices byte-identical to the original.
- A hit at standstill (dv noise below the threshold): no dent.

## Task 1: SDK v5 in the host

**Files:** Modify `include/gt_mod/gt_mod_api.h`, `host/game_state.{hpp,cpp}`, `host/mod_services.{hpp,cpp}`, `host/mod_hooks.cpp`, `docs/MODS.md`, `docs/FACTS.md`. Create `host/mesh_blob.{hpp,cpp}`. Test: `host/tests/game_state_test.cpp`, `host/tests/mesh_blob_test.cpp`, `host/tests/mod_api_c_check.c`.

**Interfaces:**
- Produces (C API, v5): `GtHookPoint {GT_HOOK_WORLD_STEP, GT_HOOK_MESH_DRAW}`; `int hook_point(GtMod*, GtHookPoint, GtHookFn)`; `GtCarImpact {float dv_body[3]; float dv_magnitude; float speed_before; float yaw_rate_jump; int32_t off_track; float contact_dir[3]; float box_half_length, box_half_width, box_floor, box_roof;}`; `int car_impact(GtMod*, GtModCtx*, uint32_t car, GtCarImpact*)` (call once per step: it compares with the previous call); `GtMeshInfo {uint32_t vertex_count, stride, position_offset, vertex_data;}`; `int mesh_info(GtMod*, GtModCtx*, uint32_t blob, GtMeshInfo*)`; `int mesh_vertex(GtMod*, GtModCtx*, uint32_t blob, uint32_t index, int16_t xyz[3])` and `mesh_set_vertex(..., const int16_t xyz[3])`; `int mesh_copy(GtMod*, GtModCtx*, uint32_t blob, uint32_t *copy)` (vblank tick only; allocates with the game allocator, rewrites absolute VADRs); `int mesh_release(GtMod*, GtModCtx*, uint32_t copy)`; `uint32_t draw_mesh_blob(GtModCtx*)` and `void draw_mesh_redirect(GtModCtx*, uint32_t blob)` for the mesh-draw hook.
- C++ core in `host/mesh_blob`: `bool parse_blob(const GuestMemory&, uint32_t blob, BlobInfo&)`, `bool rewrite_vadrs(GuestMemory&, uint32_t from, uint32_t to)`.

- [ ] Step 1: tests `impact_zero_when_steady`, `impact_rotates_into_body_frame` (v changes by (0,5,0) with basis rows rotated 90 deg: dv_body = (5,0,0)), `impact_off_track_from_surface_word`, `blob_parse_rejects_bad_header`, `blob_parse_format_0x122_stride14`, `rewrite_vadrs_relocates_absolute_addresses` (VADR command pointing inside the blob moves with a copy).
- [ ] Step 2: run `ctest -R "game_state|mesh_blob"`; expected FAIL (undefined).
- [ ] Step 3: implement `game_state::car_impact(memory, handle, ImpactTracker&, Impact&)` and `mesh_blob.cpp`; implement the api_* wrappers (refuse `declared api < 5`), table `v5`, `GT_MOD_API_VERSION_5`.
- [ ] Step 4: run ctest; expected PASS; sabotage (swap a basis row) must fail `impact_rotates_into_body_frame`.
- [ ] Step 5: document each accessor with its evidence in `docs/MODS.md`; commit.

## Task 2: the `deformation` mod (gt-mods)

**Files:** Create `../gt-mods/deformation/{mod.toml,CMakeLists.txt,README.md,src/deform.h,src/deform.c,src/mod.c,tests/deform_test.c}`; modify `../gt-mods/CMakeLists.txt`.

**Interfaces:** Consumes Task 1's API. Produces (pure, testable) `deform.h`: `int deform_hit(const GtCarImpact *impact, float threshold, float *strength_out)` (1 when `dv_magnitude > threshold`); `void deform_apply(int16_t *xyz, const float local[3], const float dir[3], float strength, float radius, float cap_units, float *accumulated)` (falloff `1 - d/radius`, displacement along `dir` inward, clamped so the total per vertex <= `cap_units`).

- [ ] Step 1: tests `hit_below_threshold_is_ignored`, `dent_falls_off_with_distance`, `dent_is_capped_when_accumulated` (10 hits never exceed the cap), `zero_strength_changes_nothing`.
- [ ] Step 2: run the test binary; expected FAIL.
- [ ] Step 3: implement `deform.c`; then `mod.c`: world-step hook (`hook_point`) calls `car_impact` on the player car and records the hit; a vblank tick, on the first hit, `mesh_copy`s the player's body blobs (found through the mesh-draw hook: blobs whose vertex count is among the largest drawn while the player car is on screen, recorded by `draw_mesh_blob`), applies the dents to the copies; the mesh-draw hook redirects those blobs with `draw_mesh_redirect`; toggle off / race end (no player car) releases the copies.
- [ ] Step 4: run tests: PASS; commit.

## Task 3: in-game check

**Files:** Create `tools/mod_deformation_check.sh` (+ `.py`) in the host repo, modelled on `tools/mod_rollover_check.sh`.

- [ ] Step 1: the script runs the wall-hit route with `--decomp off` and `on`, with the mod and with every other mod enabled; cases: frames identical to no-mod before the hit; player screen box changes after the hit (`changed after hit`); an AI car of the same model unchanged; `--without-mod` byte-identical; `--sabotage` sets strength 0 and must FAIL "changed after hit".
- [ ] Step 2: run; record results in the PR.
