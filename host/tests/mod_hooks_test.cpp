// Unit tests for the mod hook table (host/mod_hooks.cpp). Plain executable, run by CTest.
#include "mod_hooks.hpp"

#include "psprecomp/runtime.hpp"

#include <cstdio>
#include <string>

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr std::uint32_t kAddr = 0x08900000u;
constexpr std::uint32_t kReturn = 0x08800100u;
std::uint32_t g_pc_inside_second = 0;

void fake_original(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4] + 1u;
    ctx.pc = ctx.gpr[31];
}
void fake_second(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    g_pc_inside_second = ctx.pc;
    ctx.gpr[2] = 1000u;
    ctx.pc = ctx.gpr[31];
}

// Runs the function registered at kAddr the way the dispatcher does.
void call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.pc = kAddr;
    ctx.gpr[31] = kReturn;
    rt.function_at(kAddr)(rt, ctx);
}

void hook_a0_41_call_v0_7(GtModCtx *c) {
    const GtModApi &api = gt::mods::mod_api();
    api.set_gpr(c, 4, 41);
    api.call_original(c);
    if (api.gpr(c, 2) != 42u) std::fprintf(stderr, "FAIL: original did not see a0 = 41\n"), ++failures;
    api.set_gpr(c, 2, 7);
}
void hook_call_then_7(GtModCtx *c) {
    gt::mods::mod_api().call_original(c);
    gt::mods::mod_api().set_gpr(c, 2, 7);
}
void hook_no_original(GtModCtx *c) { gt::mods::mod_api().set_gpr(c, 2, 99); }
void hook_noop(GtModCtx *) {}
int g_hook_calls = 0;
std::uint32_t g_pc_seen_by_original = 0;
GtMod *g_self = nullptr;
void recording_original(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    g_pc_seen_by_original = ctx.pc;
    ctx.gpr[2] = ctx.gpr[4] + 1u;
    ctx.pc = ctx.gpr[31];
}
void hook_counts(GtModCtx *c) {
    ++g_hook_calls;
    gt::mods::mod_api().call_original(c);
    gt::mods::mod_api().set_gpr(c, 2, 7);
}
void hook_turns_itself_off(GtModCtx *c) {
    ++g_hook_calls;
    gt::mods::set_active(g_self, false);
    gt::mods::mod_api().call_original(c);
    gt::mods::mod_api().set_gpr(c, 2, 7);
}
} // namespace

int main() {
    using namespace gt::mods;
    std::string err;
    GtMod a{"alpha", {{"k", "v"}}}, b{"beta", {}};

    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        rt.register_function(kAddr, &fake_original, "orig");
        check(install_hook(rt, kAddr, hook_a0_41_call_v0_7, {"alpha", &a}, err), "hook_wraps_original: installs");
        psprecomp::AllegrexContext ctx;
        ctx.gpr[4] = 5;
        call(rt, ctx);
        check(ctx.gpr[2] == 7u, "hook_wraps_original: caller sees v0 = 7");
        check(ctx.pc == kReturn, "hook_wraps_original: pc = ra");
    }
    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        rt.register_function(kAddr, &fake_original, "orig");
        check(install_hook(rt, kAddr, hook_noop, {"alpha", &a}, err), "second_hook: first installs");
        check(install_hook(rt, kAddr, hook_noop, {"beta", &b}, err), "second_mod_same_address_chains");
        check(!install_hook(rt, kAddr, hook_noop, {"alpha", &a}, err), "same_mod_same_address_twice_refused");
        check(err.find("alpha") != std::string::npos, "same_mod_twice: error names the mod");
    }
    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        rt.register_function(kAddr, &fake_original, "orig");
        install_hook(rt, kAddr, hook_call_then_7, {"alpha", &a}, err);
        remove_hooks_for(rt, &b);  // another mod: nothing happens
        { psprecomp::AllegrexContext c; c.gpr[4] = 5; call(rt, c); check(c.gpr[2] == 7u, "remove_hooks_for other mod keeps hook"); }
        remove_hooks_for(rt, &a);
        { psprecomp::AllegrexContext c; c.gpr[4] = 5; call(rt, c); check(c.gpr[2] == 6u, "remove_hooks_for restores the original"); }
        check(install_hook(rt, kAddr, hook_noop, {"beta", &b}, err), "remove_hooks_for frees the address");
    }
    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        err.clear();
        check(!install_hook(rt, kAddr, hook_noop, {"alpha", &a}, err), "hook_on_unknown_address_refused");
        check(!err.empty(), "unknown address: error filled");
    }
    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        rt.register_function(kAddr, &fake_original, "orig");
        rt.register_function(kAddr, &fake_second, "second");
        install_hook(rt, kAddr, hook_call_then_7, {"alpha", &a}, err);
        psprecomp::AllegrexContext ctx;
        call(rt, ctx);
        check(g_pc_inside_second == kAddr, "hook_wraps_previous_registration: ctx.pc == address inside original");
        check(ctx.gpr[2] == 7u, "hook_wraps_previous_registration: hook result");
    }
    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        rt.register_function(kAddr, &fake_original, "orig");
        install_hook(rt, kAddr, hook_no_original, {"alpha", &a}, err);
        psprecomp::AllegrexContext ctx;
        call(rt, ctx);
        check(ctx.pc == kReturn && ctx.gpr[2] == 99u, "hook_without_call_original_returns_to_ra");
    }
    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        rt.register_function(kAddr, &fake_original, "orig");
        bind_runtime(rt);
        const GtModApi &api = mod_api();
        check(api.hook(&a, kAddr, hook_noop) == 0, "api.hook installs through bound runtime");
        check(api.hook(&b, kAddr, hook_noop) == 0, "api.hook chains a second mod's hook");
        check(api.hook(&b, kAddr, hook_noop) != 0, "api.hook refuses the same mod twice on one address");
        check(std::string(api.config(&a, "k")) == "v" && api.config(&a, "x") == nullptr, "api.config");
        check(api.version == GT_MOD_API_VERSION, "api version");
    }
    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        rt.register_function(kAddr, &recording_original, "orig");
        GtMod m{"gate", {}};
        install_hook(rt, kAddr, hook_counts, {"gate", &m}, err);
        set_active(&m, false);
        { psprecomp::AllegrexContext c; c.gpr[4] = 5; call(rt, c);
          check(g_hook_calls == 0, "inactive_mod_runs_original: hook fn not called");
          check(c.gpr[2] == 6u, "inactive_mod_runs_original: original result");
          check(g_pc_seen_by_original == kAddr, "inactive_mod_runs_original: ctx.pc == address inside original");
          check(c.pc == kReturn && c.gpr[0] == 0u, "inactive_mod_runs_original: pc = ra"); }
        set_active(&m, true);
        { psprecomp::AllegrexContext c; c.gpr[4] = 5; call(rt, c);
          check(g_hook_calls == 1 && c.gpr[2] == 7u, "inactive_mod_runs_original: active again runs the hook"); }
    }
    {
        clear_hooks_for_tests();
        psprecomp::Runtime rt;
        rt.register_function(kAddr, &recording_original, "orig");
        GtMod m{"once", {}};
        g_self = &m;
        g_hook_calls = 0;
        install_hook(rt, kAddr, hook_turns_itself_off, {"once", &m}, err);
        { psprecomp::AllegrexContext c; c.gpr[4] = 5; call(rt, c);
          check(g_hook_calls == 1 && c.gpr[2] == 7u && c.pc == kReturn, "active_read_once_per_call: toggled call completes modded"); }
        { psprecomp::AllegrexContext c; c.gpr[4] = 5; call(rt, c);
          check(g_hook_calls == 1 && c.gpr[2] == 6u, "active_read_once_per_call: next call is unmodded"); }
    }
    std::fprintf(stderr, failures ? "%d failure(s)\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
