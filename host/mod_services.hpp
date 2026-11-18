// Mod API v2 services (include/gt_mod/gt_mod_api.h): menu rows on the F9 MODS page, the per-vblank tick and call_guest.
// Registration happens at load time only (while a mod's gt_mod_init runs, between begin_init and end_init); after that the
// registries are read-only, so the window thread (menu rendering) and the guest threads read them without a lock.
#pragma once

#include "game_state.hpp"
#include "mod_hooks.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace psprecomp {
class Runtime;
}

namespace gt::mods::services {

constexpr std::size_t kMaxRowIdLength = 31;
constexpr std::uint32_t kMaxGuestArguments = 8;

// The loader brackets each mod's gt_mod_init with these: the v2 registration functions work only for that mod, in between.
void begin_init(GtMod *mod);
void end_init();

// Implementations behind GtModApi's add_menu_row / on_vblank / call_guest (0 = ok, non-zero = refused; the reason goes to stderr).
int api_add_menu_row(GtMod *self, const char *id, GtMenuLabelFn label, GtMenuActivateFn activate);
int api_on_vblank(GtMod *self, GtVblankFn fn);
int api_call_guest(GtModCtx *ctx, std::uint32_t address, const std::uint32_t *args, std::uint32_t nargs, std::uint32_t *v0_out);

// Rows added by mods, in load order (all of mod 1's rows, then mod 2's...).
std::size_t menu_row_count();
// The row's text into out (NUL-terminated, cut to capacity-1). A row of a switched-off mod reads "<id> mod OFF". False: out of range.
bool menu_row_text(std::size_t index, char *out, std::size_t capacity);
// Enter (0), Left (-1) or Right (+1) on the row. False when out of range or the mod is switched off (the mod's function is not called).
bool menu_row_activate(std::size_t index, int direction);
// The mod's id of the row (for logs); empty when out of range.
const std::string &menu_row_mod_id(std::size_t index);

// Runs every active mod's vblank tick with a ctx that may call_guest. Called by the kernel once per game vblank (`vblank` = the count).
void run_vblank_tick(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint64_t vblank);
// True when any loaded mod registered a menu row or a tick (the host installs the pad menu and the tick only then).
bool any_registered();
// Behind GtModApi::is_menu_open / vblank_index. The host sets the query (gt_play: lcs::host_settings_open); null = never open.
void set_menu_open_query(bool (*query)());
// v6 action_held: the host answers for the full action id ("mod.id"); null = never held.
void set_action_held_query(bool (*query)(const std::string &full_id));
int api_action_held(GtMod *self, const char *id);
// A race is on: the world step (kHookWorldStep) ran within the last `window` vblanks. The trampoline notes each call. The pad hotkeys act only then:
// in the menus their buttons go to the game untouched (user, 2026-11-16: Up/Down bound to cheats made the menus slow).
void note_world_step();
bool racing(std::uint64_t window = 30);
int api_is_menu_open(GtMod *self);
std::uint64_t api_vblank_index(GtMod *self);
int api_money_get(GtMod *self, GtModCtx *ctx, std::uint32_t *value);  // v3: host/game_state.hpp
int api_money_set(GtMod *self, GtModCtx *ctx, std::uint32_t value);
std::uint32_t api_race_cars(GtMod *self, GtModCtx *ctx, std::uint32_t world, GtCar *out, std::uint32_t capacity);
int api_player_car(GtMod *self, GtModCtx *ctx, std::uint32_t world, std::uint32_t *car);
int api_car_state(GtMod *self, GtModCtx *ctx, std::uint32_t car, GtCarState *out);
int api_car_set_state(GtMod *self, GtModCtx *ctx, std::uint32_t car, const GtCarState *state);

// v4: actions and category entries (the cheat device, host/cheat_device/device.hpp, shows them; gt_play binds the hotkeys).
struct ActionDecl {
    GtMod *mod;
    std::string id;  // "<mod id>.<action id>"
    std::string label, category;
    GtActionFn fn;
    std::string key, pad;  // the defaults as the mod wrote them (parse_key / parse_pad syntax)
};
struct EntryDecl {
    GtMod *mod;
    std::string category, id;
    GtMenuLabelFn label;
    GtMenuActivateFn activate;
};
// The categories a mod may use (Mods and Controls are the host's).
bool mod_category(const std::string &category);
int api_add_action(GtMod *self, const char *id, const char *label, const char *category, GtActionFn fn, const char *default_key,
                   const char *default_pad);
int api_add_entry(GtMod *self, const char *category, const char *id, GtMenuLabelFn label, GtMenuActivateFn activate);
const std::vector<ActionDecl> &actions();
const std::vector<EntryDecl> &entries();
// Entry text / activation, as menu_row_text / menu_row_activate (a switched-off mod's entry reads "<id> mod OFF").
bool entry_text(std::size_t index, char *out, std::size_t capacity);
bool entry_activate(std::size_t index, int direction);
// Any thread: the action runs on the next vblank tick (once, however often it was queued before it), if its mod is on then.
void queue_action(const std::string &id);

// A failed init: forget this mod's rows and tick.
void remove_for(const GtMod *mod);
// Tests: forget everything.
void clear_for_tests();

// v5: hook points, impact and meshes (include/gt_mod/gt_mod_api.h; host/mesh_blob.hpp, host/game_state.hpp).
constexpr std::uint32_t kHookWorldStep = 0x0894DD9Cu;  // docs/FACTS.md "Car body state": the physics step of every car
constexpr std::uint32_t kHookMeshDraw = 0x08B550A8u;   // docs/superpowers/specs/2026-11-07-crash-deformation-design.md: the blob draw leaf
constexpr std::uint32_t kHookCarDraw = gt::game_state::kCarDrawFunction;
constexpr std::uint32_t kGuestAlloc = 0x08B35DA0u, kGuestFree = 0x08B35254u;  // the game's allocator (a0 = bytes / pointer), as 0x08B62344 uses it
int api_car_impact(GtMod *self, GtModCtx *ctx, std::uint32_t world, std::uint32_t car, GtCarImpact *out);
int api_mesh_info(GtMod *self, GtModCtx *ctx, std::uint32_t blob, GtMeshInfo *out);
int api_mesh_vertex(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t index, std::int16_t xyz[3]);
int api_mesh_set_vertex(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t index, const std::int16_t xyz[3]);
int api_mesh_copy(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t *copy);
int api_mesh_release(GtMod *self, GtModCtx *ctx, std::uint32_t copy);
int api_draw_car_model(GtMod *self, GtModCtx *ctx, std::uint32_t *car, std::uint32_t *model);
std::uint32_t api_model_blobs(GtMod *self, GtModCtx *ctx, std::uint32_t model, std::uint32_t *out, std::uint32_t capacity);
std::uint32_t api_model_runs(GtMod *self, GtModCtx *ctx, std::uint32_t model, GtVertexRun *out, std::uint32_t capacity);
int api_run_vertex(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t run, std::uint32_t index, std::int16_t xyz[3]);
int api_run_set_vertex(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t run, std::uint32_t index, const std::int16_t xyz[3]);
int api_on_switch_off(GtMod *self, GtVblankFn fn);
int api_run_normal(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t run, std::uint32_t index, std::int8_t xyz[3]);
int api_run_set_normal(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t run, std::uint32_t index, const std::int8_t xyz[3]);
int api_draw_matrix(GtMod *self, GtModCtx *ctx, float out[12]);
std::uint32_t api_blob_runs(GtMod *self, GtModCtx *ctx, std::uint32_t blob, GtVertexRun *out, std::uint32_t capacity);
std::uint32_t api_draw_mesh_blob(GtModCtx *ctx);
void api_draw_mesh_redirect(GtModCtx *ctx, std::uint32_t blob);

// v8: the draw layer (include/gt_mod/gt_mod_api.h). The host (gt_play: play_backends.cpp) sets the sink that takes each pass's command list
// (lcs patch 0071: HostDrawCmd has this layout); without one the draws are discarded. The list is submitted when it changed: a pass that
// draws nothing after one that did submits the empty list (the layer goes away).
constexpr std::size_t kMaxDrawCommands = 4096;
constexpr std::size_t kDrawTextCapacity = 96;
enum class DrawKind : std::uint8_t { Text, Rect, Line };
struct DrawCommand {
    DrawKind kind{DrawKind::Rect};
    float a{0}, b{0}, c{0}, d{0}, e{0};  // text: x, y, size. rect: x, y, w, h. line: x0, y0, x1, y1, width
    std::uint8_t rgba[4]{255, 255, 255, 255};
    char text[kDrawTextCapacity]{};
};
using DrawSink = void (*)(const DrawCommand *commands, std::size_t count);
void set_draw_sink(DrawSink sink);
int api_on_overlay(GtMod *self, GtOverlayFn fn);
void api_draw_text(GtMod *self, float x, float y, float size, std::uint32_t rgba, const char *text);
void api_draw_rect(GtMod *self, float x, float y, float w, float h, std::uint32_t rgba);
void api_draw_line(GtMod *self, float x0, float y0, float x1, float y1, float width, std::uint32_t rgba);

}  // namespace gt::mods::services
