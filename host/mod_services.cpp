#include "mod_services.hpp"

#include "game_state.hpp"

#include "guest_call.hpp"

#include "psprecomp/runtime.hpp"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
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
};

std::vector<Row> g_rows;
std::vector<Tick> g_ticks;
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
    g_ticks.push_back(Tick{self, fn});
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

bool any_registered() { return !g_rows.empty() || !g_ticks.empty(); }
void set_menu_open_query(bool (*query)()) { g_menu_open.store(query, std::memory_order_release); }
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

void run_vblank_tick(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint64_t vblank) {
    g_vblank.store(vblank, std::memory_order_release);
    for (const Tick &t : g_ticks) {
        if (!is_on(t.mod)) continue;
        GtModCtx tick_ctx{&rt, &ctx, 0u, 0u, true, t.mod};
        t.fn(&tick_ctx);
    }
}

void remove_for(const GtMod *mod) {
    g_rows.erase(std::remove_if(g_rows.begin(), g_rows.end(), [mod](const Row &r) { return r.mod == mod; }), g_rows.end());
    g_ticks.erase(std::remove_if(g_ticks.begin(), g_ticks.end(), [mod](const Tick &t) { return t.mod == mod; }), g_ticks.end());
}

void clear_for_tests() {
    g_rows.clear();
    g_ticks.clear();
    g_initializing = nullptr;
    g_vblank.store(0);
    g_menu_open.store(nullptr);
}

}  // namespace gt::mods::services
