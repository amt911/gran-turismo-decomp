#include "mod_services.hpp"

#include "game_state.hpp"
#include "mesh_blob.hpp"

#include "guest_call.hpp"

#include "psprecomp/runtime.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <utility>
#include <string>
#include <vector>

namespace gt::mods::services {
namespace {

struct Row {
    GtMod *mod;
    std::string id;
    GtMenuLabelFn label;
    GtMenuActivateFn activate;
};
struct Tick {
    GtMod *mod;
    GtVblankFn fn;
    GtVblankFn off_fn = nullptr;  // on_switch_off: runs once on the first tick after the mod was on and is now off
    bool was_on = false;
};

std::vector<Row> g_rows;
std::vector<Tick> g_ticks;
std::vector<ActionDecl> g_actions;
std::vector<EntryDecl> g_entries;
struct Overlay {
    GtMod *mod;
    GtOverlayFn fn;
};
std::vector<Overlay> g_overlays;
std::vector<DrawCommand> g_draw;          // the pass in progress (the vblank tick thread only)
const GtMod *g_draw_owner = nullptr;      // the mod whose overlay callback is running
thread_local bool t_in_overlay = false;   // true only on the thread running an overlay callback: draw calls from any other thread are rejected
bool g_draw_submitted_any = false;        // the last submit was non-empty
std::vector<DrawCommand> g_draw_last;     // what the sink got last
std::atomic<DrawSink> g_draw_sink{nullptr};
struct ModGrade {
    const GtMod *mod;
    SceneGrade grade;
};
std::mutex g_grade_mutex;
std::vector<ModGrade> g_grades;           // set by scene_grade, one per mod (any guest thread; the tick publishes)
SceneGrade g_grade_published;             // what the sink got last
std::atomic<GradeSink> g_grade_sink{nullptr};
std::mutex g_queue_mutex;
std::vector<std::string> g_queue;  // actions to run on the next tick (hotkeys: the pad/keyboard poll; the menu: the window thread)
GtMod *g_initializing = nullptr;
std::atomic<std::uint64_t> g_vblank{0};
std::atomic<bool (*)()> g_menu_open{nullptr};

constexpr unsigned kArgumentRegisters[] = {4u, 5u, 6u, 7u, 8u, 9u, 10u, 11u};

bool v2_mod(GtMod *self, const char *what) {
    if (self == nullptr) return false;
    if (self->api_version < GT_MOD_API_VERSION_2) {
        std::fprintf(stderr, "[mod:%s] %s refused: the mod declared api %d (needs api = 2)\n", self->id.c_str(), what, self->api_version);
        return false;
    }
    return true;
}

bool v3_mod(GtMod *self, const char *what) {
    if (self == nullptr) return false;
    if (self->api_version < GT_MOD_API_VERSION_3) {
        std::fprintf(stderr, "[mod:%s] %s refused: the mod declared api %d (needs api = 3)\n", self->id.c_str(), what, self->api_version);
        return false;
    }
    return true;
}

bool v4_mod(GtMod *self, const char *what) {
    if (self == nullptr) return false;
    if (self->api_version < GT_MOD_API_VERSION_4) {
        std::fprintf(stderr, "[mod:%s] %s refused: the mod declared api %d (needs api = 4)\n", self->id.c_str(), what, self->api_version);
        return false;
    }
    if (g_initializing != self) {
        std::fprintf(stderr, "[mod:%s] %s refused: during gt_mod_init only\n", self->id.c_str(), what);
        return false;
    }
    return true;
}

bool is_on(const GtMod *mod) { return mod == nullptr || mod->active.load(std::memory_order_acquire); }

const std::string kEmpty;

}  // namespace

void begin_init(GtMod *mod) { g_initializing = mod; }
void end_init() { g_initializing = nullptr; }

int api_add_menu_row(GtMod *self, const char *id, GtMenuLabelFn label, GtMenuActivateFn activate) {
    if (!v2_mod(self, "add_menu_row")) return 1;
    if (g_initializing != self) {
        std::fprintf(stderr, "[mod:%s] add_menu_row refused: rows are added during gt_mod_init only\n", self->id.c_str());
        return 2;
    }
    if (id == nullptr || *id == '\0' || std::strlen(id) > kMaxRowIdLength || label == nullptr || activate == nullptr) {
        std::fprintf(stderr, "[mod:%s] add_menu_row refused: needs an id of 1-%zu chars and both functions\n", self->id.c_str(),
                     kMaxRowIdLength);
        return 3;
    }
    for (const Row &row : g_rows)
        if (row.mod == self && row.id == id) {
            std::fprintf(stderr, "[mod:%s] add_menu_row refused: row '%s' already added by this mod\n", self->id.c_str(), id);
            return 4;
        }
    g_rows.push_back(Row{self, id, label, activate});
    return 0;
}

int api_on_vblank(GtMod *self, GtVblankFn fn) {
    if (!v2_mod(self, "on_vblank")) return 1;
    if (g_initializing != self) {
        std::fprintf(stderr, "[mod:%s] on_vblank refused: the tick is registered during gt_mod_init only\n", self->id.c_str());
        return 2;
    }
    if (fn == nullptr) return 3;
    for (const Tick &t : g_ticks)
        if (t.mod == self) {
            std::fprintf(stderr, "[mod:%s] on_vblank refused: this mod already has a tick\n", self->id.c_str());
            return 4;
        }
    g_ticks.push_back(Tick{self, fn, nullptr, false});
    return 0;
}

int api_call_guest(GtModCtx *c, std::uint32_t address, const std::uint32_t *args, std::uint32_t nargs, std::uint32_t *v0_out) {
    if (c == nullptr || c->rt == nullptr || c->ctx == nullptr) return 1;
    if (c->mod != nullptr && !v2_mod(const_cast<GtMod *>(c->mod), "call_guest")) return 1;
    if (!c->tick) {
        std::fprintf(stderr, "[mod:%s] call_guest(0x%08X) refused: allowed only from the vblank tick\n",
                     c->mod != nullptr ? c->mod->id.c_str() : "?", address);
        return 2;
    }
    if (nargs > kMaxGuestArguments || (nargs != 0 && args == nullptr)) return 3;
    if (c->rt->stopped()) return 4;
    // The callee runs on a copy of the thread's context with the arguments loaded (gt::call_guest starts from it).
    psprecomp::AllegrexContext copy = *c->ctx;
    for (std::uint32_t i = 0; i < nargs; ++i) copy.gpr[kArgumentRegisters[i]] = args[i];
    std::uint32_t v0 = 0;
    try {
        v0 = gt::call_guest(*c->rt, copy, address, {});
    } catch (const gt::GuestStopped &) {
        return 4;
    }
    if (c->rt->stopped()) return 4;
    c->ctx->vfpu = copy.vfpu;  // as a decompiled caller: the callee's VFPU state reaches the thread
    c->ctx->vfpu_ctrl = copy.vfpu_ctrl;
    if (v0_out != nullptr) *v0_out = v0;
    return 0;
}

std::size_t menu_row_count() { return g_rows.size(); }

bool menu_row_text(std::size_t index, char *out, std::size_t capacity) {
    if (index >= g_rows.size() || out == nullptr || capacity == 0) return false;
    const Row &row = g_rows[index];
    if (!is_on(row.mod)) {
        std::snprintf(out, capacity, "%s mod OFF", row.mod->id.c_str());
        return true;
    }
    out[0] = '\0';
    row.label(row.mod, out, static_cast<std::uint32_t>(std::min<std::size_t>(capacity, 0xFFFFu)));
    out[capacity - 1] = '\0';  // a label that ignored the capacity still ends inside the buffer
    return true;
}

bool menu_row_activate(std::size_t index, int direction) {
    if (index >= g_rows.size()) return false;
    const Row &row = g_rows[index];
    if (!is_on(row.mod)) return false;
    row.activate(row.mod, direction < 0 ? -1 : direction > 0 ? 1 : 0);
    return true;
}

const std::string &menu_row_mod_id(std::size_t index) { return index < g_rows.size() ? g_rows[index].mod->id : kEmpty; }

bool any_registered() {
    {
        std::lock_guard<std::mutex> lock(g_grade_mutex);
        if (!g_grades.empty()) return true;  // a grade-only mod still needs the tick: it publishes the grade
    }
    return !g_rows.empty() || !g_ticks.empty() || !g_actions.empty() || !g_entries.empty() || !g_overlays.empty();
}

bool mod_category(const std::string &category) {
    return category == "Race" || category == "Car" || category == "Money & garage" || category == "Camera" || category == "Settings";
}

int api_add_action(GtMod *self, const char *id, const char *label, const char *category, GtActionFn fn, const char *default_key,
                   const char *default_pad) {
    if (!v4_mod(self, "add_action")) return 1;
    if (id == nullptr || *id == '\0' || std::strlen(id) > kMaxRowIdLength || label == nullptr || *label == '\0' || fn == nullptr) {
        std::fprintf(stderr, "[mod:%s] add_action refused: needs an id of 1-%zu chars, a label and a function\n", self->id.c_str(),
                     kMaxRowIdLength);
        return 3;
    }
    if (category == nullptr || !mod_category(category)) {
        std::fprintf(stderr, "[mod:%s] add_action '%s' refused: unknown category '%s'\n", self->id.c_str(), id,
                     category != nullptr ? category : "(null)");
        return 3;
    }
    const std::string full = self->id + "." + id;
    for (const ActionDecl &a : g_actions)
        if (a.id == full) {
            std::fprintf(stderr, "[mod:%s] add_action refused: '%s' already declared\n", self->id.c_str(), full.c_str());
            return 4;
        }
    g_actions.push_back(ActionDecl{self, full, label, category, fn, default_key != nullptr ? default_key : "",
                                   default_pad != nullptr ? default_pad : ""});
    return 0;
}

int api_add_entry(GtMod *self, const char *category, const char *id, GtMenuLabelFn label, GtMenuActivateFn activate) {
    if (!v4_mod(self, "add_entry")) return 1;
    if (id == nullptr || *id == '\0' || std::strlen(id) > kMaxRowIdLength || label == nullptr || activate == nullptr) {
        std::fprintf(stderr, "[mod:%s] add_entry refused: needs an id of 1-%zu chars and both functions\n", self->id.c_str(), kMaxRowIdLength);
        return 3;
    }
    if (category == nullptr || !mod_category(category)) {
        std::fprintf(stderr, "[mod:%s] add_entry '%s' refused: unknown category '%s'\n", self->id.c_str(), id,
                     category != nullptr ? category : "(null)");
        return 3;
    }
    for (const EntryDecl &e : g_entries)
        if (e.mod == self && e.id == id) {
            std::fprintf(stderr, "[mod:%s] add_entry refused: '%s' already added by this mod\n", self->id.c_str(), id);
            return 4;
        }
    g_entries.push_back(EntryDecl{self, category, id, label, activate});
    return 0;
}

const std::vector<ActionDecl> &actions() { return g_actions; }
const std::vector<EntryDecl> &entries() { return g_entries; }

bool entry_text(std::size_t index, char *out, std::size_t capacity) {
    if (index >= g_entries.size() || out == nullptr || capacity == 0) return false;
    const EntryDecl &e = g_entries[index];
    if (!is_on(e.mod)) {
        std::snprintf(out, capacity, "%s mod OFF", e.mod->id.c_str());
        return true;
    }
    out[0] = '\0';
    e.label(e.mod, out, static_cast<std::uint32_t>(std::min<std::size_t>(capacity, 0xFFFFu)));
    out[capacity - 1] = '\0';
    return true;
}

bool entry_activate(std::size_t index, int direction) {
    if (index >= g_entries.size()) return false;
    const EntryDecl &e = g_entries[index];
    if (!is_on(e.mod)) return false;
    e.activate(e.mod, direction < 0 ? -1 : direction > 0 ? 1 : 0);
    return true;
}

void queue_action(const std::string &id) {
    std::lock_guard<std::mutex> lock(g_queue_mutex);
    if (std::find(g_queue.begin(), g_queue.end(), id) == g_queue.end()) g_queue.push_back(id);
}
void set_menu_open_query(bool (*query)()) { g_menu_open.store(query, std::memory_order_release); }

namespace {
std::atomic<bool (*)(const std::string &)> g_action_held{nullptr};
std::atomic<std::uint64_t> g_world_step_vblank{0};
std::atomic<bool> g_world_step_seen{false};
}  // namespace

void set_action_held_query(bool (*query)(const std::string &)) { g_action_held.store(query, std::memory_order_release); }

int api_action_held(GtMod *self, const char *id) {
    if (self == nullptr || id == nullptr || self->api_version < GT_MOD_API_VERSION_6) return 0;
    const auto query = g_action_held.load(std::memory_order_acquire);
    return query != nullptr && is_on(self) && query(self->id + "." + id) ? 1 : 0;
}

void note_world_step() {
    g_world_step_vblank.store(g_vblank.load(std::memory_order_acquire), std::memory_order_release);
    g_world_step_seen.store(true, std::memory_order_release);
}

bool racing(std::uint64_t window) {
    if (!g_world_step_seen.load(std::memory_order_acquire)) return false;
    const std::uint64_t now = g_vblank.load(std::memory_order_acquire), at = g_world_step_vblank.load(std::memory_order_acquire);
    return now < at + window;
}
int api_is_menu_open(GtMod *) {
    const auto query = g_menu_open.load(std::memory_order_acquire);
    return query != nullptr && query() ? 1 : 0;
}
std::uint64_t api_vblank_index(GtMod *) { return g_vblank.load(std::memory_order_acquire); }

int api_money_get(GtMod *self, GtModCtx *ctx, std::uint32_t *value) {
    if (!v3_mod(self, "money_get") || ctx == nullptr || value == nullptr) return 1;
    return gt::game_state::money_get(ctx->rt->memory(), *value) ? 0 : 1;
}
static_assert(sizeof(GtCarState) == sizeof(gt::game_state::CarState), "GtCarState mirrors game_state::CarState");

std::uint32_t api_race_cars(GtMod *self, GtModCtx *ctx, std::uint32_t world, GtCar *out, std::uint32_t capacity) {
    if (!v3_mod(self, "race_cars") || ctx == nullptr) return 0u;
    gt::game_state::Car cars[gt::game_state::kMaxCars];
    const std::uint32_t n = gt::game_state::race_cars(ctx->rt->memory(), world, cars);
    for (std::uint32_t i = 0; i < n && i < capacity && out != nullptr; ++i)
        out[i] = GtCar{cars[i].handle, cars[i].index, cars[i].role == gt::game_state::CarRole::Human ? 1 : cars[i].role == gt::game_state::CarRole::ReplayHuman ? 3 : 2};
    return n;
}
int api_player_car(GtMod *self, GtModCtx *ctx, std::uint32_t world, std::uint32_t *car) {
    if (!v3_mod(self, "player_car") || ctx == nullptr || car == nullptr) return 1;
    const std::uint32_t found = gt::game_state::player_car(ctx->rt->memory(), world);
    if (found == 0u) return 1;
    *car = found;
    return 0;
}
int api_car_state(GtMod *self, GtModCtx *ctx, std::uint32_t car, GtCarState *out) {
    if (!v3_mod(self, "car_state") || ctx == nullptr || out == nullptr) return 1;
    gt::game_state::CarState s{};
    if (!gt::game_state::car_state(ctx->rt->memory(), car, s)) return 1;
    std::memcpy(out, &s, sizeof s);
    return 0;
}
int api_car_set_state(GtMod *self, GtModCtx *ctx, std::uint32_t car, const GtCarState *state) {
    if (!v3_mod(self, "car_set_state") || ctx == nullptr || state == nullptr) return 1;
    gt::game_state::CarState s{};
    std::memcpy(&s, state, sizeof s);
    return gt::game_state::car_set_state(ctx->rt->memory(), car, s) ? 0 : 1;
}
int api_money_set(GtMod *self, GtModCtx *ctx, std::uint32_t value) {
    if (!v3_mod(self, "money_set") || ctx == nullptr) return 1;
    return gt::game_state::money_set(ctx->rt->memory(), value) ? 0 : 1;
}

bool v5_mod(GtMod *self, const char *what) {
    if (self == nullptr) return false;
    if (self->api_version < GT_MOD_API_VERSION_5) {
        std::fprintf(stderr, "[mod:%s] %s refused: the mod declared api %d (needs api = 5)\n", self->id.c_str(), what, self->api_version);
        return false;
    }
    return true;
}

namespace {
gt::game_state::ImpactTracker g_impact;  // the step hook runs on one guest thread at a time (cooperative scheduling)
}
static_assert(sizeof(GtMeshInfo) == sizeof(gt::mesh::BlobInfo), "GtMeshInfo mirrors mesh::BlobInfo");
static_assert(sizeof(GtCarImpact) == sizeof(gt::game_state::Impact), "GtCarImpact mirrors game_state::Impact");

int api_car_impact(GtMod *self, GtModCtx *ctx, std::uint32_t world, std::uint32_t car, GtCarImpact *out) {
    if (!v5_mod(self, "car_impact") || ctx == nullptr || out == nullptr) return 1;
    gt::game_state::Impact impact{};
    if (!gt::game_state::car_impact(ctx->rt->memory(), world, car, g_impact, impact)) return 1;
    std::memcpy(out, &impact, sizeof impact);
    return 0;
}
int api_mesh_info(GtMod *self, GtModCtx *ctx, std::uint32_t blob, GtMeshInfo *out) {
    if (!v5_mod(self, "mesh_info") || ctx == nullptr || out == nullptr) return 1;
    gt::mesh::BlobInfo info;
    if (!gt::mesh::parse_blob(ctx->rt->memory(), blob, info)) return 1;
    std::memcpy(out, &info, sizeof info);
    return 0;
}
int api_mesh_vertex(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t index, std::int16_t xyz[3]) {
    if (!v5_mod(self, "mesh_vertex") || ctx == nullptr || xyz == nullptr) return 1;
    gt::mesh::BlobInfo info;
    return gt::mesh::parse_blob(ctx->rt->memory(), blob, info) && gt::mesh::read_vertex(ctx->rt->memory(), info, index, xyz) ? 0 : 1;
}
int api_mesh_set_vertex(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t index, const std::int16_t xyz[3]) {
    if (!v5_mod(self, "mesh_set_vertex") || ctx == nullptr || xyz == nullptr) return 1;
    gt::mesh::BlobInfo info;
    return gt::mesh::parse_blob(ctx->rt->memory(), blob, info) && gt::mesh::write_vertex(ctx->rt->memory(), info, index, xyz) ? 0 : 1;
}
int api_mesh_copy(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t *copy) {
    if (!v5_mod(self, "mesh_copy") || ctx == nullptr || copy == nullptr) return 1;
    gt::mesh::BlobInfo info;
    if (!gt::mesh::parse_blob(ctx->rt->memory(), blob, info)) return 1;
    const std::uint32_t size = gt::mesh::copy_bytes(info);
    std::uint32_t address = 0u;
    if (api_call_guest(ctx, kGuestAlloc, &size, 1u, &address) != 0 || address == 0u) return 1;
    if (!gt::mesh::copy_blob(ctx->rt->memory(), blob, address)) {
        api_call_guest(ctx, kGuestFree, &address, 1u, nullptr);
        return 1;
    }
    *copy = address;
    return 0;
}
int api_mesh_release(GtMod *self, GtModCtx *ctx, std::uint32_t copy) {
    if (!v5_mod(self, "mesh_release") || ctx == nullptr || copy == 0u) return 1;
    return api_call_guest(ctx, kGuestFree, &copy, 1u, nullptr);
}
int api_draw_car_model(GtMod *self, GtModCtx *ctx, std::uint32_t *car, std::uint32_t *model) {
    if (!v5_mod(self, "draw_car_model") || ctx == nullptr || ctx->ctx == nullptr || car == nullptr || model == nullptr) return 1;
    gt::game_state::CarDrawLink link;
    if (!gt::game_state::car_draw_link(ctx->rt->memory(), ctx->ctx->gpr[4], link)) return 1;
    *car = link.car;
    *model = link.model;
    return 0;
}
int api_on_switch_off(GtMod *self, GtVblankFn fn) {
    if (!v5_mod(self, "on_switch_off")) return 1;
    if (g_initializing != self) return 2;
    if (fn == nullptr) return 3;
    for (Tick &t : g_ticks)
        if (t.mod == self) {
            t.off_fn = fn;
            return 0;
        }
    return 4;  // register the vblank tick first
}
int api_run_normal(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t run, std::uint32_t index, std::int8_t xyz[3]) {
    if (!v5_mod(self, "run_normal") || ctx == nullptr || xyz == nullptr) return 1;
    std::vector<gt::mesh::Run> runs;
    return gt::mesh::parse_runs(ctx->rt->memory(), blob, runs) && run < runs.size() && gt::mesh::read_run_normal(ctx->rt->memory(), runs[run], index, xyz) ? 0 : 1;
}
int api_run_set_normal(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t run, std::uint32_t index, const std::int8_t xyz[3]) {
    if (!v5_mod(self, "run_set_normal") || ctx == nullptr || xyz == nullptr) return 1;
    std::vector<gt::mesh::Run> runs;
    return gt::mesh::parse_runs(ctx->rt->memory(), blob, runs) && run < runs.size() && gt::mesh::write_run_normal(ctx->rt->memory(), runs[run], index, xyz) ? 0 : 1;
}
int api_draw_matrix(GtMod *self, GtModCtx *ctx, float out[12]) {
    if (!v5_mod(self, "draw_matrix") || ctx == nullptr || out == nullptr) return 1;
    return gt::game_state::list_world_matrix(ctx->rt->memory(), out) ? 0 : 1;
}
std::uint32_t api_blob_runs(GtMod *self, GtModCtx *ctx, std::uint32_t blob, GtVertexRun *out, std::uint32_t capacity) {
    if (!v5_mod(self, "blob_runs") || ctx == nullptr || out == nullptr) return 0u;
    std::vector<gt::mesh::Run> runs;
    if (!gt::mesh::parse_runs(ctx->rt->memory(), blob, runs)) return 0u;
    std::uint32_t n = 0u;
    for (std::uint32_t r = 0; r < runs.size() && n < capacity; ++r) out[n++] = GtVertexRun{blob, r, runs[r].count};
    return n;
}
std::uint32_t api_model_runs(GtMod *self, GtModCtx *ctx, std::uint32_t model, GtVertexRun *out, std::uint32_t capacity) {
    if (!v5_mod(self, "model_runs") || ctx == nullptr || out == nullptr) return 0u;
    std::vector<gt::game_state::RunRef> refs(capacity);
    const std::uint32_t n = gt::game_state::model_runs(ctx->rt->memory(), model, refs.data(), capacity);
    for (std::uint32_t i = 0; i < n; ++i) out[i] = GtVertexRun{refs[i].blob, refs[i].run, refs[i].count};
    return n;
}
int api_run_vertex(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t run, std::uint32_t index, std::int16_t xyz[3]) {
    if (!v5_mod(self, "run_vertex") || ctx == nullptr || xyz == nullptr) return 1;
    std::vector<gt::mesh::Run> runs;
    return gt::mesh::parse_runs(ctx->rt->memory(), blob, runs) && run < runs.size() && gt::mesh::read_run_vertex(ctx->rt->memory(), runs[run], index, xyz) ? 0 : 1;
}
int api_run_set_vertex(GtMod *self, GtModCtx *ctx, std::uint32_t blob, std::uint32_t run, std::uint32_t index, const std::int16_t xyz[3]) {
    if (!v5_mod(self, "run_set_vertex") || ctx == nullptr || xyz == nullptr) return 1;
    std::vector<gt::mesh::Run> runs;
    return gt::mesh::parse_runs(ctx->rt->memory(), blob, runs) && run < runs.size() && gt::mesh::write_run_vertex(ctx->rt->memory(), runs[run], index, xyz) ? 0 : 1;
}
std::uint32_t api_model_blobs(GtMod *self, GtModCtx *ctx, std::uint32_t model, std::uint32_t *out, std::uint32_t capacity) {
    if (!v5_mod(self, "model_blobs") || ctx == nullptr || out == nullptr) return 0u;
    return gt::game_state::model_blobs(ctx->rt->memory(), model, out, capacity);
}
std::uint32_t api_draw_mesh_blob(GtModCtx *ctx) { return ctx != nullptr && ctx->ctx != nullptr ? ctx->ctx->gpr[4] : 0u; }
void api_draw_mesh_redirect(GtModCtx *ctx, std::uint32_t blob) {
    if (ctx != nullptr && ctx->ctx != nullptr) ctx->ctx->gpr[4] = blob;
}

namespace {

bool same_grade(const SceneGrade &a, const SceneGrade &b) {
    return a.r == b.r && a.g == b.g && a.b == b.b && a.brightness == b.brightness && a.saturation == b.saturation;
}

// The product over the active mods' grades.
SceneGrade combined_grade() {
    SceneGrade out;
    std::lock_guard<std::mutex> lock(g_grade_mutex);
    for (const ModGrade &m : g_grades) {
        if (!is_on(m.mod)) continue;
        out.r *= m.grade.r, out.g *= m.grade.g, out.b *= m.grade.b;
        out.brightness *= m.grade.brightness, out.saturation *= m.grade.saturation;
    }
    // Products of values a mod meant as 1 (0.5 * 2, 1.00001) must not turn the grade on: snap to neutral when every field is within 1e-4 of 1.
    const auto near_one = [](float v) { return std::fabs(v - 1.0f) < 1.0e-4f; };
    if (near_one(out.r) && near_one(out.g) && near_one(out.b) && near_one(out.brightness) && near_one(out.saturation)) return SceneGrade{};
    return out;
}

void publish_scene_grade() {
    const SceneGrade now = combined_grade();
    if (same_grade(now, g_grade_published)) return;
    g_grade_published = now;
    if (const GradeSink sink = g_grade_sink.load(std::memory_order_acquire)) sink(now);
}

bool same_command(const DrawCommand &a, const DrawCommand &b) {
    return a.kind == b.kind && a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d && a.e == b.e && std::memcmp(a.rgba, b.rgba, sizeof a.rgba) == 0 &&
           std::strncmp(a.text, b.text, kDrawTextCapacity) == 0;
}
bool same_list(const std::vector<DrawCommand> &a, const std::vector<DrawCommand> &b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(), same_command);
}

// v8: every active mod's overlay callback, one list for all of them (the mods draw in load order), handed to the sink.
void run_overlays(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (g_overlays.empty()) return;
    g_draw.clear();
    for (const Overlay &o : g_overlays) {
        if (!is_on(o.mod)) continue;
        GtModCtx overlay_ctx{&rt, &ctx, 0u, 0u, true, o.mod};
        g_draw_owner = o.mod;
        t_in_overlay = true;
        o.fn(o.mod, &overlay_ctx);
        t_in_overlay = false;
        g_draw_owner = nullptr;
    }
    if (g_draw.empty() && !g_draw_submitted_any) return;
    if (!g_draw.empty() && same_list(g_draw, g_draw_last)) return;  // an unchanged overlay (a static label): no copy, no submit
    const DrawSink sink = g_draw_sink.load(std::memory_order_acquire);
    if (sink == nullptr) return;
    g_draw_submitted_any = !g_draw.empty();
    g_draw_last = g_draw;
    sink(g_draw.data(), g_draw.size());
}

}  // namespace

void run_vblank_tick(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint64_t vblank) {
    g_vblank.store(vblank, std::memory_order_release);
    for (Tick &t : g_ticks) {
        if (!is_on(t.mod)) {
            if (t.was_on && t.off_fn != nullptr) {
                GtModCtx off_ctx{&rt, &ctx, 0u, 0u, true, t.mod};
                t.off_fn(&off_ctx);
            }
            t.was_on = false;
            continue;
        }
        t.was_on = true;
        GtModCtx tick_ctx{&rt, &ctx, 0u, 0u, true, t.mod};
        t.fn(&tick_ctx);
    }
    std::vector<std::string> queued;
    {
        std::lock_guard<std::mutex> lock(g_queue_mutex);
        queued.swap(g_queue);
    }
    for (const std::string &id : queued)
        for (const ActionDecl &a : g_actions)
            if (a.id == id && is_on(a.mod)) {
                GtModCtx action_ctx{&rt, &ctx, 0u, 0u, true, a.mod};
                a.fn(a.mod, &action_ctx);
            }
    run_overlays(rt, ctx);
    publish_scene_grade();
}

void remove_for(const GtMod *mod) {
    {
        std::lock_guard<std::mutex> lock(g_grade_mutex);
        g_grades.erase(std::remove_if(g_grades.begin(), g_grades.end(), [mod](const ModGrade &g) { return g.mod == mod; }), g_grades.end());
    }
    g_overlays.erase(std::remove_if(g_overlays.begin(), g_overlays.end(), [mod](const Overlay &o) { return o.mod == mod; }), g_overlays.end());
    g_rows.erase(std::remove_if(g_rows.begin(), g_rows.end(), [mod](const Row &r) { return r.mod == mod; }), g_rows.end());
    g_ticks.erase(std::remove_if(g_ticks.begin(), g_ticks.end(), [mod](const Tick &t) { return t.mod == mod; }), g_ticks.end());
    g_actions.erase(std::remove_if(g_actions.begin(), g_actions.end(), [mod](const ActionDecl &a) { return a.mod == mod; }), g_actions.end());
    g_entries.erase(std::remove_if(g_entries.begin(), g_entries.end(), [mod](const EntryDecl &e) { return e.mod == mod; }), g_entries.end());
}

void clear_for_tests() {
    g_rows.clear();
    g_ticks.clear();
    g_actions.clear();
    g_entries.clear();
    g_overlays.clear();
    g_draw.clear();
    g_draw_owner = nullptr;
    g_draw_submitted_any = false;
    g_draw_last.clear();
    g_draw_sink.store(nullptr);
    {
        std::lock_guard<std::mutex> lock(g_grade_mutex);
        g_grades.clear();
    }
    g_grade_published = SceneGrade{};
    g_grade_sink.store(nullptr);
    {
        std::lock_guard<std::mutex> lock(g_queue_mutex);
        g_queue.clear();
    }
    g_initializing = nullptr;
    g_vblank.store(0);
    g_world_step_seen.store(false);
    g_action_held.store(nullptr);
    g_menu_open.store(nullptr);
}

// ---- v8: the draw layer ----

void set_draw_sink(DrawSink sink) { g_draw_sink.store(sink, std::memory_order_release); }

namespace {

// One log line per mod and kind of refusal (a mod calling a refused function every frame must not flood stderr).
void warn_once(const GtMod *self, int kind, const std::string &text) {
    static std::mutex mutex;
    static std::vector<std::pair<const GtMod *, int>> warned;
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (std::find(warned.begin(), warned.end(), std::make_pair(self, kind)) != warned.end()) return;
        warned.emplace_back(self, kind);
    }
    std::fprintf(stderr, "[mod:%s] %s\n", self->id.c_str(), text.c_str());
}

bool v8_mod(GtMod *self, const char *what) {
    if (self == nullptr) return false;
    if (self->api_version < GT_MOD_API_VERSION_8) {
        warn_once(self, 1, std::string(what) + " refused: the mod declared api " + std::to_string(self->api_version) + " (needs api = 8)");
        return false;
    }
    return true;
}

// Where a draw call lands: only the running overlay callback of that very mod, on the thread that runs it (the tick thread), else the call is
// dropped (logged once per mod).
bool draw_allowed(GtMod *self, const char *what) {
    if (!v8_mod(self, what)) return false;
    if (!t_in_overlay || g_draw_owner != self) {
        warn_once(self, 2, std::string(what) + " ignored: drawing is valid only inside the on_overlay callback, on its thread");
        return false;
    }
    return g_draw.size() < kMaxDrawCommands;
}

bool finite(float v) { return std::isfinite(v) && std::fabs(v) < 1.0e7f; }

constexpr float kMaxTextSize = 300.0f, kMaxLineWidth = 300.0f;  // logical units: the screen is 480 x 272
constexpr float kScreenW = 480.0f, kScreenH = 272.0f;
// True when the box [x0, x1] x [y0, y1] is wholly outside the screen (nothing to draw: the command is dropped).
bool off_screen(float x0, float y0, float x1, float y1) {
    return std::max(x0, x1) <= 0.0f || std::max(y0, y1) <= 0.0f || std::min(x0, x1) >= kScreenW || std::min(y0, y1) >= kScreenH;
}

void set_color(DrawCommand &cmd, std::uint32_t rgba) {
    cmd.rgba[0] = static_cast<std::uint8_t>(rgba >> 24);
    cmd.rgba[1] = static_cast<std::uint8_t>(rgba >> 16);
    cmd.rgba[2] = static_cast<std::uint8_t>(rgba >> 8);
    cmd.rgba[3] = static_cast<std::uint8_t>(rgba);
}

}  // namespace

int api_on_overlay(GtMod *self, GtOverlayFn fn) {
    if (!v8_mod(self, "on_overlay")) return 1;
    if (g_initializing != self) {
        std::fprintf(stderr, "[mod:%s] on_overlay refused: the callback is registered during gt_mod_init only\n", self->id.c_str());
        return 2;
    }
    if (fn == nullptr) return 3;
    for (const Overlay &o : g_overlays)
        if (o.mod == self) {
            std::fprintf(stderr, "[mod:%s] on_overlay refused: this mod already has an overlay\n", self->id.c_str());
            return 4;
        }
    g_overlays.push_back(Overlay{self, fn});
    return 0;
}

void api_draw_text(GtMod *self, float x, float y, float size, std::uint32_t rgba, const char *text) {
    if (!draw_allowed(self, "draw_text") || text == nullptr || *text == '\0' || !finite(x) || !finite(y) || !finite(size) || size <= 0.0f) return;
    size = std::min(size, kMaxTextSize);
    if (off_screen(x, y, x + size * static_cast<float>(strnlen(text, kDrawTextCapacity - 1)), y + size)) return;  // generous width: one em per character
    DrawCommand cmd;
    cmd.kind = DrawKind::Text;
    cmd.a = x, cmd.b = y, cmd.c = size;
    set_color(cmd, rgba);
    std::size_t n = strnlen(text, kDrawTextCapacity - 1);
    while (text[n] != '\0' && n > 0 && (static_cast<unsigned char>(text[n]) & 0xC0) == 0x80) --n;  // cut by the capacity: never inside a UTF-8 sequence
    std::memcpy(cmd.text, text, n);
    cmd.text[n] = '\0';
    g_draw.push_back(cmd);
}

void api_draw_rect(GtMod *self, float x, float y, float w, float h, std::uint32_t rgba) {
    if (!draw_allowed(self, "draw_rect") || !finite(x) || !finite(y) || !finite(w) || !finite(h) || off_screen(x, y, x + w, y + h)) return;
    DrawCommand cmd;
    cmd.kind = DrawKind::Rect;
    cmd.a = x, cmd.b = y, cmd.c = w, cmd.d = h;
    set_color(cmd, rgba);
    g_draw.push_back(cmd);
}

void api_draw_line(GtMod *self, float x0, float y0, float x1, float y1, float width, std::uint32_t rgba) {
    if (!draw_allowed(self, "draw_line") || !finite(x0) || !finite(y0) || !finite(x1) || !finite(y1) || !finite(width) || width <= 0.0f) return;
    width = std::min(width, kMaxLineWidth);
    const float half = width * 0.5f;
    if (off_screen(std::min(x0, x1) - half, std::min(y0, y1) - half, std::max(x0, x1) + half, std::max(y0, y1) + half)) return;
    DrawCommand cmd;
    cmd.kind = DrawKind::Line;
    cmd.a = x0, cmd.b = y0, cmd.c = x1, cmd.d = y1, cmd.e = width;
    set_color(cmd, rgba);
    g_draw.push_back(cmd);
}

void set_grade_sink(GradeSink sink) { g_grade_sink.store(sink, std::memory_order_release); }

int api_scene_grade(GtMod *self, float r, float g, float b, float brightness, float saturation) {
    if (!v8_mod(self, "scene_grade")) return 1;
    for (const float v : {r, g, b, brightness, saturation})
        if (!std::isfinite(v)) return 2;
    const auto clamp4 = [](float v) { return std::min(std::max(v, 0.0f), 4.0f); };
    const SceneGrade grade{clamp4(r), clamp4(g), clamp4(b), clamp4(brightness), clamp4(saturation)};
    std::lock_guard<std::mutex> lock(g_grade_mutex);
    for (ModGrade &m : g_grades)
        if (m.mod == self) {
            m.grade = grade;
            return 0;
        }
    g_grades.push_back(ModGrade{self, grade});
    return 0;
}

SceneGrade effective_scene_grade() { return combined_grade(); }

}  // namespace gt::mods::services
