// Tests for the native mod loader (host/mod_loader.cpp) with a real shared library (host/tests/mods/test_mod.c).
#include "mod_hooks.hpp"
#include "mod_loader.hpp"
#include "mod_services.hpp"

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

// A private copy of the test library inside the mod dir: dlopen of the same path returns the same instance (one g_bonus).
std::string copy_lib(const fs::path &root, const std::string &dir) {
    const fs::path to = root / dir / "lib.so";
    fs::create_directories(to.parent_path());
    fs::copy_file(TEST_MOD_LIB, to, fs::copy_options::overwrite_existing);
    return to.string();
}

constexpr std::uint32_t kGuest = 0x08901000u;  // what the test mod's tick calls: v0 = a0 * 10 + a1
void fake_guest(psprecomp::Runtime &, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = ctx.gpr[4] * 10u + ctx.gpr[5];
    ctx.pc = ctx.gpr[31];
}

psprecomp::Runtime &fresh_runtime(std::unique_ptr<psprecomp::Runtime> &holder) {
    gt::mods::clear_hooks_for_tests();
    holder = std::make_unique<psprecomp::Runtime>();
    holder->register_function(kAddr, &fake_original, "orig");
    holder->register_function(kGuest, &fake_guest, "guest");
    return *holder;
}
} // namespace

int main() {
    using namespace gt::mods;
    std::unique_ptr<psprecomp::Runtime> holder;
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("enabled");
        make_mod(root, "bonus", "bonus_mod", true, "100");
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.size() == 1 && r.loaded[0] == "bonus_mod" && r.skipped.empty(), "loads_enabled_mod_and_hooks: report");
        check(r.entries.size() == 1 && r.entries[0].state == ModState::Active && r.entries[0].name == "bonus_mod", "loads_enabled_mod_and_hooks: Active entry");
        check(call(rt, 5) == 5u + 1u + 100u, "loads_enabled_mod_and_hooks: v0 = a0 + 1 + 100");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("dotdirs");
        fs::create_directories(root / ".staging" / "x");  // the downloader's work area (host/mod_install.hpp)
        const LoadReport r = load_mods(rt, root);
        check(r.entries.empty() && r.skipped.empty(), "dot_folders_are_not_mods: nothing reported");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("disabled");
        make_mod(root, "off", "off_mod", false, "100");
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.empty(), "disabled_mod_loaded_inactive: not in loaded");
        check(r.skipped.size() == 1 && r.skipped[0] == "off: disabled", "disabled_mod_loaded_inactive: reported as skipped 'disabled'");
        check(r.entries.size() == 1 && r.entries[0].state == ModState::Inactive && r.entries[0].mod != nullptr &&
                  r.entries[0].manifest == root / "off" / "mod.toml" && r.entries[0].reason == "disabled",
              "disabled_mod_loaded_inactive: entry Inactive with mod and manifest path");
        check(call(rt, 5) == 6u, "disabled_mod_loaded_inactive: hook passes to the original");
        set_active(r.entries[0].mod, true);
        check(call(rt, 5) == 106u, "disabled_mod_loaded_inactive: turning it on activates the hook");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("broken");
        fs::create_directories(root / "a_no_manifest");
        write(root / "b_bad_toml" / "mod.toml", "this is not toml\n");
        make_mod(root, "c_missing_lib", "c", true, "1", "1", (root / "nope.so").string());
        make_mod(root, "d_api4", "d", true, "1", "4");  // api 3 exists since the named SDK (v3): 4 is the unsupported one
        write(root / "stray_file.txt", "not a directory");
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.empty(), "loader_skips_broken_mods: nothing loaded");
        check(r.skipped.size() == 4, "loader_skips_broken_mods: four skips");
        bool all_error = r.entries.size() == 4;
        for (const auto &e : r.entries) all_error = all_error && e.state == ModState::Error && e.mod == nullptr && !e.reason.empty();
        check(all_error, "loader_skips_broken_mods: four Error entries with a reason");
        check(call(rt, 5) == 6u, "loader_skips_broken_mods: game state untouched");
    }
    {
        // Two enabled mods on one address chain: the first loaded runs outermost, its call_original runs the next one,
        // the last one's runs the game's function. Each mod uses its own copy of the library (its own g_bonus).
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("chain");
        make_mod(root, "a", "first_mod", true, "1");
        make_mod(root, "b", "second_mod", true, "20", "1", copy_lib(root, "b"));
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.size() == 2 && r.skipped.empty(), "two_mods_same_address_chain: both loaded");
        check(call(rt, 5) == 5u + 1u + 20u + 1u, "two_mods_same_address_chain: both hooks run, original once");
        set_active(r.entries[0].mod, false);
        check(call(rt, 5) == 5u + 1u + (r.entries[0].name == "first_mod" ? 20u : 1u), "two_mods_same_address_chain: an inactive link is skipped");
        set_active(r.entries[1].mod, false);
        check(call(rt, 5) == 6u, "two_mods_same_address_chain: both inactive = original only");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("chain_disabled");
        make_mod(root, "a", "first_mod", false, "1");  // sorts first, but disabled: the enabled one loads first
        make_mod(root, "b", "second_mod", true, "20", "1", copy_lib(root, "b"));
        const auto report = load_mods(rt, root);
        GtMod *first = nullptr;
        bool second_active = false;
        for (const auto &e : report.entries) {
            if (e.name == "second_mod" && e.state == gt::mods::ModState::Active) second_active = true;
            if (e.name == "first_mod" && e.state == gt::mods::ModState::Inactive) first = e.mod;
        }
        check(second_active && first != nullptr, "disabled_mod_same_address: enabled Active, disabled Inactive (no error row)");
        check(call(rt, 5) == 5u + 1u + 20u, "disabled_mod_same_address: only the enabled hook runs");
        set_active(first, true);
        check(call(rt, 5) == 5u + 1u + 20u + 1u, "disabled_mod_same_address: turning it on joins the chain");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("hook_twice");
        make_mod(root, "t", "twice_mod", true, "100", "1", TEST_MOD_LIB, "hook_twice = \"1\"\n");
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.empty() && r.skipped.size() == 1, "one_mod_hooks_an_address_twice: skipped");
        check(call(rt, 5) == 6u, "one_mod_hooks_an_address_twice: its first hook removed too");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("chain_failing");
        make_mod(root, "a", "ok_mod", true, "100");
        make_mod(root, "b", "failing_mod", true, "7", "1", copy_lib(root, "b"), "fail_after_hook = \"1\"\n");
        make_mod(root, "c", "third_mod", true, "20", "1", copy_lib(root, "c"));
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.size() == 2 && r.skipped.size() == 1, "failing_mod_in_chain: skipped, others loaded");
        check(call(rt, 5) == 5u + 1u + 100u + 20u, "failing_mod_in_chain: removed from the middle, the rest still chain");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("dup_id");
        make_mod(root, "a_first", "dup_mod", true, "1");
        make_mod(root, "b_second", "dup_mod", true, "2");
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.size() == 1 && r.skipped.size() == 1, "duplicate_id: one loaded, one skipped");
        check(!r.skipped.empty() && r.skipped[0].find("dup_mod") != std::string::npos &&
                  r.skipped[0].find("a_first") != std::string::npos && r.skipped[0].find("b_second") != std::string::npos,
              "duplicate_id: reason names the id and both dirs");
        check(call(rt, 5) == 5u + 1u + 1u, "duplicate_id: the first one is the active hook");
    }
    {
        // API v2: a v1 mod is told version 1 and loads; a v2 mod is told 2 and its row, tick and call_guest work.
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("v2");
        make_mod(root, "a_v1", "v1_mod", true, "1", "1", copy_lib(root, "a_v1"), "expect_version = \"1\"\n");
        make_mod(root, "b_v2", "v2_mod", true, "20", "2", copy_lib(root, "b_v2"), "expect_version = \"2\"\nrow_id = \"speed\"\n");
        make_mod(root, "c_v2", "v2_other", true, "300", "2", copy_lib(root, "c_v2"), "expect_version = \"2\"\nrow_id = \"speed\"\nrow_twice = \"1\"\n");
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.size() == 3 && r.skipped.empty(), "api_v2: v1 mod told version 1, v2 mods told 2, all three loaded");
        using gt::mods::services::menu_row_activate;
        using gt::mods::services::menu_row_count;
        using gt::mods::services::menu_row_text;
        check(menu_row_count() == 2, "api_v2: rows of two mods coexist (the same row id in different mods), a duplicate in one mod is refused");
        char text[160] = {};
        check(menu_row_text(0, text, sizeof text) && std::string(text).rfind("v2 act=0 dir=0 ticks=0", 0) == 0, "api_v2: first row's label");
        check(menu_row_activate(0, -1) && menu_row_activate(1, 1) && menu_row_activate(1, 0), "api_v2: activation reaches the mods");
        menu_row_text(0, text, sizeof text);
        check(std::string(text).find("act=1 dir=-1") != std::string::npos, "api_v2: Left reaches row 0 as -1");
        menu_row_text(1, text, sizeof text);
        check(std::string(text).find("act=2 dir=0") != std::string::npos, "api_v2: row 1 got Right then Enter, last direction 0");
        check(gt::mods::services::menu_row_mod_id(0) == "v2_mod" && gt::mods::services::menu_row_mod_id(1) == "v2_other", "api_v2: rows in load order");
        // the tick: call_guest works there (v0 = 7 * 10 + 8) ...
        psprecomp::AllegrexContext tick_ctx;
        gt::mods::services::run_vblank_tick(rt, tick_ctx, 77);
        menu_row_text(0, text, sizeof text);
        check(std::string(text).find("ticks=1 tickcall=0/78") != std::string::npos, "api_v2: the vblank tick ran and call_guest returned 78");
        // ... and is refused from a hook (hookcall=2), the guest function not run
        call(rt, 5);
        menu_row_text(0, text, sizeof text);
        check(std::string(text).find("hookcall=2") != std::string::npos, "api_v2: call_guest refused from a hook");
        // a switched-off mod: its row reads "<id> mod OFF", activation is ignored, the tick is skipped
        set_active(r.entries[1].mod, false);
        check(menu_row_text(0, text, sizeof text) && std::string(text) == "v2_mod mod OFF", "api_v2: row of an OFF mod");
        check(!menu_row_activate(0, 1), "api_v2: an OFF mod's row ignores activation");
        gt::mods::services::run_vblank_tick(rt, tick_ctx, 77);
        set_active(r.entries[1].mod, true);
        menu_row_text(0, text, sizeof text);
        check(std::string(text).find("ticks=1 ") != std::string::npos, "api_v2: an OFF mod's tick is skipped");
    }
    {
        // a mod that declared api 1 and calls the v2 functions anyway is refused: its init fails and it is skipped
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("v1_uses_v2");
        make_mod(root, "x", "v1_cheat", true, "1", "1", TEST_MOD_LIB, "row_id = \"r\"\n");
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.empty() && r.skipped.size() == 1, "api_v2: a v1 mod cannot add rows (refused, skipped)");
        check(gt::mods::services::menu_row_count() == 0 && call(rt, 5) == 6u, "api_v2: nothing of it stays registered");
    }
    {
        auto &rt = fresh_runtime(holder);
        check(load_mods(rt, fresh_dir("empty")).loaded.empty(), "empty dir loads nothing");
        const LoadReport missing = load_mods(rt, fresh_dir("empty") / "does_not_exist");
        check(missing.loaded.empty() && missing.skipped.empty(), "missing dir loads nothing");
        check(call(rt, 5) == 6u, "empty_or_missing_dir: original untouched");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("failing_init");
        make_mod(root, "f", "failing_mod", true, "100", "1", TEST_MOD_LIB, "fail_after_hook = \"1\"\n");
        const LoadReport r = load_mods(rt, root);
        check(r.loaded.empty() && r.skipped.size() == 1, "failing_init_is_skipped");
        check(call(rt, 5) == 6u, "failing_init: hook removed, original only");
        // the address is free again: another mod can take it
        const fs::path root2 = fresh_dir("after_failing");
        make_mod(root2, "ok", "ok_mod", true, "100");
        check(load_mods(rt, root2).loaded.size() == 1 && call(rt, 5) == 106u, "failing_init: address reusable");
    }
    {
        auto &rt = fresh_runtime(holder);
        const fs::path root = fresh_dir("no_mods_env");
        make_mod(root, "bonus", "bonus_mod", true, "100");
        setenv("GT_NO_MODS", "1", 1);
        const LoadReport r = load_mods(rt, root);
        unsetenv("GT_NO_MODS");
        check(r.loaded.empty() && r.skipped.empty(), "GT_NO_MODS: nothing loaded");
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
