// Tests for the native mod loader (host/mod_loader.cpp) with a real shared library (host/tests/mods/test_mod.c).
#include "mod_hooks.hpp"
#include "mod_loader.hpp"

#include "psprecomp/runtime.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <unistd.h>
#include <string>

#ifndef TEST_MOD_LIB
#error "TEST_MOD_LIB must name the built test_mod shared library"
#endif

namespace {
namespace fs = std::filesystem;
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

constexpr std::uint32_t kAddr = 0x08900000u;
constexpr std::uint32_t kReturn = 0x08800100u;

void fake_original(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4] + 1u;
    ctx.pc = ctx.gpr[31];
}

std::uint32_t call(psprecomp::Runtime &rt, std::uint32_t a0) {
    psprecomp::AllegrexContext ctx;
    ctx.pc = kAddr;
    ctx.gpr[4] = a0;
    ctx.gpr[31] = kReturn;
    rt.function_at(kAddr)(rt, ctx);
    return ctx.gpr[2];
}

fs::path fresh_dir(const char *name) {
    const fs::path dir = fs::temp_directory_path() / ("gt_mod_loader_test_" + std::to_string(::getpid())) / name;
    fs::remove_all(dir);
    fs::create_directories(dir);
    return dir;
}

void write(const fs::path &path, const std::string &text) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << text;
}

// A mod directory using the test library; `extra` is appended to the manifest.
void make_mod(const fs::path &root, const std::string &dir, const std::string &id, bool enabled, const std::string &bonus,
              const std::string &api = "1", const std::string &library = TEST_MOD_LIB, const std::string &extra = "") {
    write(root / dir / "mod.toml", "id = \"" + id + "\"\nlibrary = \"" + library + "\"\napi = " + api + "\nenabled = " +
                                       (enabled ? "true" : "false") + "\n[settings]\nbonus = \"" + bonus + "\"\n" + extra);
}

psprecomp::Runtime &fresh_runtime(std::unique_ptr<psprecomp::Runtime> &holder) {
    gt::mods::clear_hooks_for_tests();
    holder = std::make_unique<psprecomp::Runtime>();
    holder->register_function(kAddr, &fake_original, "orig");
    return *holder;
}
} // namespace

int main() {
    using namespace gt::mods;
    std::unique_ptr<psprecomp::Runtime> holder;
    std::string fatal;
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("enabled");
        make_mod(root, "bonus", "bonus_mod", true, "100");
        const LoadReport r = load_mods(rt, root, fatal);
        check(fatal.empty(), "loads_enabled_mod_and_hooks: no fatal");
        check(r.loaded.size() == 1 && r.loaded[0] == "bonus_mod" && r.skipped.empty(), "loads_enabled_mod_and_hooks: report");
        check(call(rt, 5) == 5u + 1u + 100u, "loads_enabled_mod_and_hooks: v0 = a0 + 1 + 100");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("disabled");
        make_mod(root, "off", "off_mod", false, "100");
        const LoadReport r = load_mods(rt, root, fatal);
        check(r.loaded.empty() && fatal.empty(), "disabled_mod_not_loaded: nothing loaded");
        check(r.skipped.size() == 1, "disabled_mod_not_loaded: reported as skipped");
        check(call(rt, 5) == 6u, "disabled_mod_not_loaded: original untouched");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("broken");
        fs::create_directories(root / "a_no_manifest");
        write(root / "b_bad_toml" / "mod.toml", "this is not toml\n");
        make_mod(root, "c_missing_lib", "c", true, "1", "1", (root / "nope.so").string());
        make_mod(root, "d_api2", "d", true, "1", "2");
        write(root / "stray_file.txt", "not a directory");
        const LoadReport r = load_mods(rt, root, fatal);
        check(r.loaded.empty() && fatal.empty(), "loader_skips_broken_mods: nothing loaded, no fatal");
        check(r.skipped.size() == 4, "loader_skips_broken_mods: four skips");
        check(call(rt, 5) == 6u, "loader_skips_broken_mods: game state untouched");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("conflict");
        make_mod(root, "a", "first_mod", true, "1");
        make_mod(root, "b", "second_mod", true, "2");
        load_mods(rt, root, fatal);
        check(fatal.find("first_mod") != std::string::npos && fatal.find("second_mod") != std::string::npos,
              "two_mods_same_address_is_fatal: message names both ids");
    }
    {
        auto &rt = fresh_runtime(holder);
        fatal.clear();
        const fs::path root = fresh_dir("dup_id");
        make_mod(root, "a_first", "dup_mod", true, "1");
        make_mod(root, "b_second", "dup_mod", true, "2");
        const LoadReport r = load_mods(rt, root, fatal);
        check(fatal.empty(), "duplicate_id: not fatal");
        check(r.loaded.size() == 1 && r.skipped.size() == 1, "duplicate_id: one loaded, one skipped");
        check(!r.skipped.empty() && r.skipped[0].find("dup_mod") != std::string::npos &&
                  r.skipped[0].find("a_first") != std::string::npos && r.skipped[0].find("b_second") != std::string::npos,
              "duplicate_id: reason names the id and both dirs");
        check(call(rt, 5) == 5u + 1u + 1u, "duplicate_id: the first one is the active hook");
    }
    {
        auto &rt = fresh_runtime(holder);
        fatal.clear();
        check(load_mods(rt, fresh_dir("empty"), fatal).loaded.empty(), "empty dir loads nothing");
        const LoadReport missing = load_mods(rt, fresh_dir("empty") / "does_not_exist", fatal);
        check(missing.loaded.empty() && missing.skipped.empty() && fatal.empty(), "missing dir loads nothing");
        check(call(rt, 5) == 6u, "empty_or_missing_dir: original untouched");
    }
    {
        auto &rt = fresh_runtime(holder);
        fatal.clear();
        const fs::path root = fresh_dir("failing_init");
        make_mod(root, "f", "failing_mod", true, "100", "1", TEST_MOD_LIB, "fail_after_hook = \"1\"\n");
        const LoadReport r = load_mods(rt, root, fatal);
        check(r.loaded.empty() && r.skipped.size() == 1 && fatal.empty(), "failing_init_is_skipped");
        check(call(rt, 5) == 6u, "failing_init: hook removed, original only");
        // the address is free again: another mod can take it
        const fs::path root2 = fresh_dir("after_failing");
        make_mod(root2, "ok", "ok_mod", true, "100");
        check(load_mods(rt, root2, fatal).loaded.size() == 1 && call(rt, 5) == 106u, "failing_init: address reusable");
    }
    {
        auto &rt = fresh_runtime(holder);
        fatal.clear();
        const fs::path root = fresh_dir("no_mods_env");
        make_mod(root, "bonus", "bonus_mod", true, "100");
        setenv("GT_NO_MODS", "1", 1);
        const LoadReport r = load_mods(rt, root, fatal);
        unsetenv("GT_NO_MODS");
        check(r.loaded.empty() && r.skipped.empty() && fatal.empty(), "GT_NO_MODS: nothing loaded");
        check(call(rt, 5) == 6u, "GT_NO_MODS: original untouched");
    }
    {
        setenv("GT_MODS_DIR", "/some/where", 1);
        check(mods_dir("/x/gt_play") == fs::path("/some/where"), "mods_dir: env wins");
        unsetenv("GT_MODS_DIR");
        check(mods_dir("/x/y/gt_play") == fs::read_symlink("/proc/self/exe").parent_path() / "mods",
              "mods_dir: next to the executable (/proc/self/exe)");
    }
    fs::remove_all(fs::temp_directory_path() / ("gt_mod_loader_test_" + std::to_string(::getpid())));
    if (failures == 0) std::puts("mod_loader tests passed");
    return failures == 0 ? 0 : 1;
}
