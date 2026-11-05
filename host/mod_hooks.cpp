#include "mod_hooks.hpp"

#include "psprecomp/runtime.hpp"

#include <cstdio>
#include <cstring>
#include <unordered_map>

// What a hook receives: the runtime and CPU state it runs against, and the address it wraps.
struct GtModCtx {
    psprecomp::Runtime *rt;
    psprecomp::AllegrexContext *ctx;
    std::uint32_t address;
};

namespace gt::mods {
namespace {

// The C API does not carry the Runtime; the loader binds it (bind_runtime) before calling gt_mod_init.
psprecomp::Runtime *g_runtime = nullptr;
std::string g_conflict;  // set by install_hook when an address is already hooked; taken by the loader

struct Hook {
    GtHookFn fn;
    HookOwner owner;
    psprecomp::Runtime::RecompiledFunction original;
};

std::unordered_map<std::uint32_t, Hook> &hooks() {
    static std::unordered_map<std::uint32_t, Hook> table;
    return table;
}

void trampoline(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto it = hooks().find(ctx.pc);
    if (it == hooks().end()) {
        rt.unsupported(ctx.pc, 0u, "mod trampoline reached for an unhooked address");
        return;
    }
    const std::uint32_t address = ctx.pc;
    const std::uint32_t return_address = ctx.gpr[31];
    // One read per call: a toggle in the middle of a call never gives a half-modded call.
    const GtMod *owner = it->second.owner.mod;
    const bool on = owner == nullptr || owner->active.load(std::memory_order_acquire);
    if (on) {
        GtModCtx mod_ctx{&rt, &ctx, address};
        it->second.fn(&mod_ctx);
    } else {
        ctx.pc = address;  // same entry state call_original gives (a decompiled trampoline finds itself by ctx.pc)
        it->second.original(rt, ctx);
    }
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

void api_call_original(GtModCtx *c) {
    const auto it = hooks().find(c->address);
    if (it == hooks().end()) return;
    c->ctx->pc = c->address;  // a decompiled-function trampoline registered earlier finds itself by ctx.pc
    it->second.original(*c->rt, *c->ctx);
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

bool install_hook(psprecomp::Runtime &rt, std::uint32_t address, GtHookFn fn, HookOwner owner, std::string &error) {
    const auto existing = hooks().find(address);
    if (existing != hooks().end()) {
        error = "address already hooked by mod '" + existing->second.owner.mod_id + "' (second hook from mod '" +
                owner.mod_id + "')";
        g_conflict = error;
        return false;
    }
    const auto original = rt.function_at(address);
    if (original == nullptr) {
        error = "mod '" + owner.mod_id + "': no function at guest address";
        return false;
    }
    const std::string name = "mod:" + owner.mod_id;
    hooks().emplace(address, Hook{fn, std::move(owner), original});
    rt.register_function(address, &trampoline, name);
    return true;
}

const GtModApi &mod_api() {
    static const GtModApi api{GT_MOD_API_VERSION, api_hook,   api_call_original, api_gpr,    api_set_gpr,
                              api_read8,          api_read16, api_read32,        api_readf,  api_write8,
                              api_write16,        api_write32, api_writef,       api_config, api_log};
    return api;
}

void set_active(GtMod *mod, bool on) {
    if (mod != nullptr) mod->active.store(on, std::memory_order_release);
}

void remove_hooks_for(psprecomp::Runtime &rt, const GtMod *mod) {
    for (auto it = hooks().begin(); it != hooks().end();) {
        if (it->second.owner.mod == mod) {
            rt.register_function(it->first, it->second.original, "mod:removed");
            it = hooks().erase(it);
        } else {
            ++it;
        }
    }
}

std::string take_hook_conflict() {
    std::string out;
    out.swap(g_conflict);
    return out;
}

void bind_runtime(psprecomp::Runtime &rt) { g_runtime = &rt; }

void clear_hooks_for_tests() {
    hooks().clear();
    g_conflict.clear();
    g_runtime = nullptr;
}

}  // namespace gt::mods
