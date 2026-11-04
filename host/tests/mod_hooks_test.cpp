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
        check(!install_hook(rt, kAddr, hook_noop, {"beta", &b}, err), "second_hook_same_address_refused");
        check(err.find("alpha") != std::string::npos && err.find("beta") != std::string::npos,
              "second_hook: error names both ids");
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
        check(api.hook(&b, kAddr, hook_noop) != 0, "api.hook refuses a second hook");
        check(std::string(api.config(&a, "k")) == "v" && api.config(&a, "x") == nullptr, "api.config");
        check(api.version == GT_MOD_API_VERSION, "api version");
    }
    std::fprintf(stderr, failures ? "%d failure(s)\n" : "all passed\n", failures);
    return failures ? 1 : 0;
}
