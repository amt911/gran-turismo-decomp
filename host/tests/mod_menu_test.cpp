// Tests for the manifest rewrite (write_enabled) and the mod menu model (host/mod_menu.cpp). Plain executable, run by CTest.
#include "mod_hooks.hpp"
#include "mod_manifest.hpp"
#include "mod_menu.hpp"

#include <unistd.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {
namespace fs = std::filesystem;
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

fs::path scratch() {
    static const fs::path dir = fs::temp_directory_path() / ("gt_mod_menu_test_" + std::to_string(::getpid()));
    fs::create_directories(dir);
    return dir;
}
fs::path write_file(const std::string &name, const std::string &text) {
    const fs::path path = scratch() / name;
    std::ofstream(path, std::ios::binary) << text;
    return path;
}
std::string slurp(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
} // namespace

int main() {
    using namespace gt::mods;
    std::string err;

    {  // replaced in place, comments and other keys kept
        const auto p = write_file("a.toml", "# my mod\nid = \"x\"\nenabled = true   # on\napi = 1\n[settings]\nenabled = \"settings value\"\n");
        check(write_enabled(p, false, err), "write_enabled: ok");
        check(slurp(p) == "# my mod\nid = \"x\"\nenabled = false   # on\napi = 1\n[settings]\nenabled = \"settings value\"\n",
              "write_enabled: line replaced in place, comment and [settings].enabled untouched");
        check(write_enabled(p, true, err) && slurp(p).find("enabled = true   # on") != std::string::npos, "write_enabled: back to true");
    }
    {  // missing key: added after the root keys, before the first table
        const auto p = write_file("b.toml", "id = \"x\"\n# note\nlibrary = \"l.so\"\n\n[settings]\nk = \"v\"\n");
        check(write_enabled(p, false, err), "write_enabled missing: ok");
        check(slurp(p) == "id = \"x\"\n# note\nlibrary = \"l.so\"\nenabled = false\n\n[settings]\nk = \"v\"\n",
              "write_enabled missing: added after the last root key");
        const auto q = write_file("c.toml", "id = \"x\"");  // no tables, no trailing newline
        check(write_enabled(q, true, err) && slurp(q) == "id = \"x\"\nenabled = true\n", "write_enabled missing: no table, no final newline");
        const auto crlf = write_file("d.toml", "id = \"x\"\r\nenabled = false\r\n");
        check(write_enabled(crlf, true, err) && slurp(crlf) == "id = \"x\"\r\nenabled = true\r\n", "write_enabled: CRLF kept");
    }
    {  // unwritable
        const auto p = write_file("ro.toml", "enabled = true\n");
        fs::permissions(p, fs::perms::owner_read, fs::perm_options::replace);
        err.clear();
        if (::geteuid() != 0) {
            check(!write_enabled(p, false, err) && !err.empty(), "write_enabled: read-only file -> false + error");
            check(slurp(p) == "enabled = true\n", "write_enabled: read-only file untouched");
        }
        err.clear();
        check(!write_enabled(scratch() / "missing.toml", false, err) && !err.empty(), "write_enabled: missing file -> false + error");
    }
    {  // menu model
        GtMod on, off, bad_save;
        on.id = "pedals";
        off.id = "other";
        bad_save.id = "ro";
        off.active = false;
        on.manifest = write_file("on.toml", "id = \"pedals\"\nenabled = true\n");
        off.manifest = write_file("off.toml", "id = \"other\"\nenabled = false\n");
        bad_save.manifest = scratch() / "nope" / "mod.toml";  // directory does not exist: write fails
        menu::set_entries({});
        check(menu::count() == 1 && menu::row(0) == "NO MODS INSTALLED" && !menu::toggle(0), "menu: empty -> NO MODS INSTALLED, toggle refused");
        menu::set_entries({
            ModEntry{"Analog pedals", ModState::Active, "", on.manifest, &on},
            ModEntry{"Other", ModState::Inactive, "disabled", off.manifest, &off},
            ModEntry{"Broken", ModState::Error, "cannot load library: libfoo.so: cannot open shared object file", scratch() / "e.toml", nullptr},
            ModEntry{"RO", ModState::Active, "", bad_save.manifest, &bad_save},
        });
        check(menu::count() == 4, "menu: four rows");
        check(menu::row(0).size() <= 40 && menu::row(0).substr(menu::row(0).size() - 3) == " ON" && menu::row(0).rfind("Analog pedals", 0) == 0, "menu: ON row");
        check(menu::row(1).substr(menu::row(1).size() - 4) == " OFF", "menu: OFF row");
        check(menu::row(2).size() <= 40 && menu::row(2).find("ERROR") != std::string::npos && menu::row(2).rfind("Broken", 0) == 0, "menu: ERROR row fits the width");
        check(!menu::toggle(2) && !menu::toggle(99), "menu: Error / out of range cannot be toggled");

        check(menu::toggle(0), "menu: toggle Active -> ok");
        check(!on.active.load() && slurp(on.manifest) == "id = \"pedals\"\nenabled = false\n", "menu: Active -> Inactive, file says false");
        check(menu::row(0).substr(menu::row(0).size() - 4) == " OFF", "menu: row now OFF");
        check(menu::toggle(1), "menu: toggle Inactive -> ok");
        check(off.active.load() && slurp(off.manifest) == "id = \"other\"\nenabled = true\n", "menu: Inactive -> Active, file says true");
        check(menu::toggle(0) && on.active.load(), "menu: toggles back on");

        check(menu::toggle(3), "menu: write failure still toggles");
        check(!bad_save.active.load(), "menu: flag flipped despite SAVE FAILED");
        check(menu::row(3).find("SAVE FAILED") != std::string::npos && menu::row(3).size() <= 40, "menu: SAVE FAILED shown, row fits");
        check(menu::toggle(3) && menu::row(3).find("SAVE FAILED") != std::string::npos, "menu: still failing -> still shown");
    }
    fs::remove_all(scratch());
    if (failures == 0) std::puts("mod_menu tests passed");
    return failures == 0 ? 0 : 1;
}
