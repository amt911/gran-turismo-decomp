#include "mod_hooks.hpp"

#include "mod_services.hpp"

#include "psprecomp/runtime.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace gt::mods {
namespace {

// The C API does not carry the Runtime; the loader binds it (bind_runtime) before calling gt_mod_init.
psprecomp::Runtime *g_runtime = nullptr;

struct Link {
    GtHookFn fn;
    HookOwner owner;
};

// Every hook on one guest address, outermost first (load order), and the function they wrap.
struct Chain {
    psprecomp::Runtime::RecompiledFunction original;
    std::vector<Link> links;
};

std::unordered_map<std::uint32_t, Chain> &chains() {
    static std::unordered_map<std::uint32_t, Chain> table;
    return table;
}

bool link_on(const Link &link) {
#ifdef GT_MOD_SABOTAGE_IGNORE_ACTIVE  // verification only (tools/mod_pedals_check.sh --sabotage-active): the gate must matter
    (void)link;
    return true;
#else
    return link.owner.mod == nullptr || link.owner.mod->active.load(std::memory_order_acquire);
#endif
}

// Runs the chain of `address` from link `from`: the first active link's hook (its call_original continues after it), or the
// original when no active link is left. Each link's `active` is read once, when the call reaches it: a toggle in the middle
// of a call never gives a half-modded call of that mod.
void run_chain(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, const Chain &chain, std::uint32_t address,
               std::size_t from) {
    for (std::size_t i = from; i < chain.links.size(); ++i) {
        if (!link_on(chain.links[i])) continue;
        GtModCtx mod_ctx{&rt, &ctx, address, i, false, chain.links[i].owner.mod};
        chain.links[i].fn(&mod_ctx);
        return;
    }
    ctx.pc = address;  // the entry state the original expects (a decompiled trampoline finds itself by ctx.pc)
    chain.original(rt, ctx);
}

void trampoline(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto it = chains().find(ctx.pc);
    if (it == chains().end()) {
        rt.unsupported(ctx.pc, 0u, "mod trampoline reached for an unhooked address");
        return;
    }
    const std::uint32_t address = ctx.pc;
    const std::uint32_t return_address = ctx.gpr[31];
    run_chain(rt, ctx, it->second, address, 0);
    ctx.gpr[0] = 0u;
    // A registered function ends with ctx.pc = ra; a hook that never called the original (or left pc on the entry) gets it here.
    if (ctx.pc == address) ctx.pc = return_address;
}

int api_hook(GtMod *self, std::uint32_t address, GtHookFn fn) {
    if (self == nullptr || fn == nullptr || g_runtime == nullptr) return 1;
    std::string error;
    if (install_hook(*g_runtime, address, fn, HookOwner{self->id, self}, error)) return 0;
    std::fprintf(stderr, "[mod:%s] hook 0x%08X refused: %s\n", self->id.c_str(), address, error.c_str());
    return 1;
}

// The next active link of the chain, or the game's function after the last one.
void api_call_original(GtModCtx *c) {
    const auto it = chains().find(c->address);
    if (it == chains().end()) return;
    run_chain(*c->rt, *c->ctx, it->second, c->address, c->link + 1);
}

std::uint32_t api_gpr(GtModCtx *c, unsigned reg) { return c->ctx->gpr[reg & 31u]; }
void api_set_gpr(GtModCtx *c, unsigned reg, std::uint32_t value) {
    if ((reg & 31u) != 0u) c->ctx->gpr[reg & 31u] = value;
}
std::uint8_t api_read8(GtModCtx *c, std::uint32_t a) { return c->rt->memory().load8(a); }
std::uint16_t api_read16(GtModCtx *c, std::uint32_t a) { return c->rt->memory().load16(a); }
std::uint32_t api_read32(GtModCtx *c, std::uint32_t a) { return c->rt->memory().load32(a); }
float api_readf(GtModCtx *c, std::uint32_t a) {
    const std::uint32_t bits = c->rt->memory().load32(a);
    float value;
    std::memcpy(&value, &bits, sizeof value);
    return value;
}
void api_write8(GtModCtx *c, std::uint32_t a, std::uint8_t v) { c->rt->memory().store8(a, v); }
void api_write16(GtModCtx *c, std::uint32_t a, std::uint16_t v) { c->rt->memory().store16(a, v); }
void api_write32(GtModCtx *c, std::uint32_t a, std::uint32_t v) { c->rt->memory().store32(a, v); }
void api_writef(GtModCtx *c, std::uint32_t a, float v) {
    std::uint32_t bits;
    std::memcpy(&bits, &v, sizeof bits);
    c->rt->memory().store32(a, bits);
}
const char *api_config(GtMod *self, const char *key) {
    if (self == nullptr || key == nullptr) return nullptr;
    const auto it = self->settings.find(key);
    return it == self->settings.end() ? nullptr : it->second.c_str();
}
void api_log(GtMod *self, const char *message) {
    std::fprintf(stderr, "[mod:%s] %s\n", self != nullptr ? self->id.c_str() : "?", message != nullptr ? message : "");
}

}  // namespace

// Load time only (gt_mod_init on the loader thread): run_chain walks the links without a lock, so a hook installed while the
// game runs could reallocate the vector under a running call.
bool install_hook(psprecomp::Runtime &rt, std::uint32_t address, GtHookFn fn, HookOwner owner, std::string &error) {
    const auto existing = chains().find(address);
    if (existing != chains().end()) {
        for (const Link &link : existing->second.links)
            if (owner.mod != nullptr && link.owner.mod == owner.mod) {
                error = "mod '" + owner.mod_id + "' already hooks this address";
                return false;
            }
        existing->second.links.push_back(Link{fn, std::move(owner)});  // the next link in: runs inside the earlier ones
        return true;
    }
    const auto original = rt.function_at(address);
    if (original == nullptr) {
        error = "mod '" + owner.mod_id + "': no function at guest address";
        return false;
    }
    const std::string name = "mod:" + owner.mod_id;
    chains().emplace(address, Chain{original, {Link{fn, std::move(owner)}}});
    rt.register_function(address, &trampoline, name);
    return true;
}

const GtModApi &mod_api(int version) {
    // One table per declared api: the same functions, only `version` differs (a v1 mod compares it with 1).
    const auto table = [](std::uint32_t v) {
        return GtModApi{v, api_hook, api_call_original, api_gpr, api_set_gpr, api_read8, api_read16, api_read32, api_readf,
                        api_write8, api_write16, api_write32, api_writef, api_config, api_log,
                        services::api_add_menu_row, services::api_on_vblank, services::api_call_guest,
                        services::api_is_menu_open, services::api_vblank_index,
                        services::api_money_get, services::api_money_set};
    };
    static const GtModApi v1 = table(GT_MOD_API_VERSION), v2 = table(GT_MOD_API_VERSION_2), v3 = table(GT_MOD_API_VERSION_3);
    return version == GT_MOD_API_VERSION ? v1 : version == GT_MOD_API_VERSION_2 ? v2 : v3;
}

void set_active(GtMod *mod, bool on) {
    if (mod != nullptr) mod->active.store(on, std::memory_order_release);
}

void remove_hooks_for(psprecomp::Runtime &rt, const GtMod *mod) {
    services::remove_for(mod);  // its menu rows and vblank tick go too
    for (auto it = chains().begin(); it != chains().end();) {
        auto &links = it->second.links;
        links.erase(std::remove_if(links.begin(), links.end(), [mod](const Link &l) { return l.owner.mod == mod; }), links.end());
        if (links.empty()) {
            rt.register_function(it->first, it->second.original, "mod:removed");
            it = chains().erase(it);
        } else {
            ++it;
        }
    }
}

void bind_runtime(psprecomp::Runtime &rt) { g_runtime = &rt; }

void clear_hooks_for_tests() {
    chains().clear();
    services::clear_for_tests();
    g_runtime = nullptr;
}

}  // namespace gt::mods
