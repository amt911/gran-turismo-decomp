#include "mod_loader.hpp"

#include "mod_hooks.hpp"
#include "mod_manifest.hpp"

#include <dlfcn.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <sstream>
#include <system_error>

namespace gt::mods {
namespace {

// Hooks keep pointers to their GtMod and the code of the library: both live until the process ends.
std::vector<std::unique_ptr<GtMod>> &live_mods() {
    static std::vector<std::unique_ptr<GtMod>> mods;
    return mods;
}

} // namespace

LoadReport load_mods(psprecomp::Runtime &rt, const std::filesystem::path &dir, std::string &fatal) {
    LoadReport report;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return report;
    std::vector<std::filesystem::path> entries;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec)) entries.push_back(it->path());
    std::sort(entries.begin(), entries.end());

    bind_runtime(rt);
    for (const auto &mod_dir : entries) {
        const std::string name = mod_dir.filename().string();
        const auto skip = [&](const std::string &why) { report.skipped.push_back(name + ": " + why); };

        std::ifstream in(mod_dir / "mod.toml");
        if (!in) {
            skip("no mod.toml");
            continue;
        }
        std::stringstream text;
        text << in.rdbuf();
        std::string error;
        const auto manifest = parse_manifest(text.str(), error);
        if (!manifest) {
            skip("bad mod.toml: " + error);
            continue;
        }
        if (manifest->api != GT_MOD_API_VERSION) {
            skip("api " + std::to_string(manifest->api) + " (host implements " + std::to_string(GT_MOD_API_VERSION) + ")");
            continue;
        }
        if (!manifest->enabled) {
            skip("disabled");
            continue;
        }
        const std::filesystem::path library = mod_dir / manifest->library;
        void *handle = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            const char *why = dlerror();
            skip(std::string("cannot load library: ") + (why != nullptr ? why : library.string()));
            continue;
        }
        using InitFn = int (*)(const GtModApi *, GtMod *);
        const auto init = reinterpret_cast<InitFn>(dlsym(handle, "gt_mod_init"));
        if (init == nullptr) {
            skip("library has no gt_mod_init");
            dlclose(handle);
            continue;
        }
        auto mod = std::make_unique<GtMod>();
        mod->id = manifest->id;
        mod->settings = manifest->settings;
        GtMod *self = mod.get();
        live_mods().push_back(std::move(mod));
        const int status = init(&mod_api(), self);
        if (std::string conflict = take_hook_conflict(); !conflict.empty()) {
            fatal = conflict;
            return report;
        }
        if (status != 0) {
            // Hooks it did install stay (the library cannot be unloaded under them); the mod is reported as skipped.
            skip("gt_mod_init returned " + std::to_string(status));
            continue;
        }
        report.loaded.push_back(manifest->id);
    }
    return report;
}

std::filesystem::path mods_dir(const char *argv0) {
    if (const char *env = std::getenv("GT_MODS_DIR"); env != nullptr && *env != '\0') return env;
    std::error_code ec;
    const auto exe = std::filesystem::absolute(argv0 != nullptr ? argv0 : "", ec);
    return exe.parent_path() / "mods";
}

} // namespace gt::mods
