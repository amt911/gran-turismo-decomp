#include "tilt_free.hpp"

#include "psprecomp/runtime.hpp"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace gt::mods::tilt {
namespace {

struct Entry {
    const GtMod *mod;
    std::uint32_t car;
};
std::vector<Entry> &entries() {
    static std::vector<Entry> v;
    return v;
}
// Guest threads are scheduled cooperatively on one OS thread, so plain statics are enough (as in the mods themselves).
bool g_in_body = false;     // inside the body step of a freed car (the extent that works with `--decomp on`)
bool g_body_freed = false;  // the latest body step to start belongs to a freed car (the `--decomp off` case, where `ra` names the reader)

void call_next(GtModCtx *c) { mod_api(GT_MOD_API_VERSION_7).call_original(c); }

void on_body_step(GtModCtx *c) {
    const std::uint32_t car = c->rt->memory().load32(c->ctx->gpr[4] + 8u);  // a0 = the car object, whose +8 is the body
    g_body_freed = freed(car);
    g_in_body = g_body_freed;
    call_next(c);
    g_in_body = false;
}
void on_contact_step(GtModCtx *c) {  // inside the body step, but it must read the game's real word (its decompiled flag-1 path differs)
    const bool was = g_in_body;
    g_in_body = false;
    call_next(c);
    g_in_body = was;
}
void on_flag_read(GtModCtx *c) {
    const std::uint32_t ra = c->ctx->gpr[31];
    call_next(c);
    if (lift_reader(ra, g_in_body, g_body_freed)) c->ctx->gpr[2] = 1u;
}

}  // namespace

bool lift_reader(std::uint32_t ra, bool inside, bool body_freed) { return inside || (body_freed && (ra == kClampRa || ra == kWrapRa)); }

bool freed(std::uint32_t car) {
    for (const Entry &e : entries())
        if (e.car == car && e.mod != nullptr && e.mod->active.load(std::memory_order_acquire)) return true;
    return false;
}

int api_tilt_free(GtMod *self, GtModCtx *, std::uint32_t car, int on) {
    if (self == nullptr || car == 0u) return 1;
    if (self->api_version < GT_MOD_API_VERSION_7) {
        std::fprintf(stderr, "[mod:%s] tilt_free refused: the mod declared api %d (needs api = 7)\n", self->id.c_str(), self->api_version);
        return 1;
    }
    auto &v = entries();
    const auto it = std::find_if(v.begin(), v.end(), [&](const Entry &e) { return e.mod == self && e.car == car; });
    if (on != 0 && it == v.end()) v.push_back(Entry{self, car});
    if (on == 0 && it != v.end()) v.erase(it);
    return 0;
}

bool install(psprecomp::Runtime &rt, GtMod *mod, std::string &error) {
    const HookOwner owner{mod->id, mod};
    return install_hook(rt, kBodyStep, on_body_step, owner, error) && install_hook(rt, kContactStep, on_contact_step, owner, error) &&
           install_hook(rt, kFlagReader, on_flag_read, owner, error);
}

void forget(const GtMod *mod) {
    auto &v = entries();
    v.erase(std::remove_if(v.begin(), v.end(), [mod](const Entry &e) { return e.mod == mod; }), v.end());
}

void clear_for_tests() {
    entries().clear();
    g_in_body = g_body_freed = false;
}

}  // namespace gt::mods::tilt
