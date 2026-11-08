#include "mod_loader.hpp"

#include "mod_hooks.hpp"
#include "mod_manifest.hpp"
#include "mod_services.hpp"

#include <dlfcn.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
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

LoadReport load_mods(psprecomp::Runtime &rt, const std::filesystem::path &dir) {
    LoadReport report;
    if (const char *off = std::getenv("GT_NO_MODS"); off != nullptr && *off != '\0' && std::string(off) != "0") {
        std::fprintf(stderr, "[mods] disabled (GT_NO_MODS)\n");
        return report;
    }
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return report;
    std::vector<std::filesystem::path> entries;
    for (std::filesystem::directory_iterator it(dir, ec), end; !ec && it != end; it.increment(ec))
        if (it->is_directory(ec)) entries.push_back(it->path());
    std::sort(entries.begin(), entries.end());
    // Enabled mods load first: on an address several mods hook, the enabled ones are the outer links of the chain.
    const auto enabled_in = [](const std::filesystem::path &mod_dir) {
        std::ifstream in(mod_dir / "mod.toml");
        std::stringstream text;
        text << in.rdbuf();
        std::string ignored;
        const auto manifest = parse_manifest(text.str(), ignored);
        return !manifest || manifest->enabled;  // broken manifests keep their place among the enabled (reported there)
    };
    std::stable_partition(entries.begin(), entries.end(), enabled_in);

    bind_runtime(rt);
    std::map<std::string, std::string> id_dirs;  // manifest id -> first directory that used it
    for (const auto &mod_dir : entries) {
        const std::string name = mod_dir.filename().string();
        std::string entry_name = name;
        const auto skip = [&](const std::string &why) {
            report.skipped.push_back(name + ": " + why);
            report.entries.push_back(ModEntry{entry_name, ModState::Error, why, mod_dir / "mod.toml", nullptr});
        };

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
        entry_name = !manifest->name.empty() ? manifest->name : manifest->id;
        if (const auto first = id_dirs.emplace(manifest->id, name); !first.second) {
            skip("duplicate mod id '" + manifest->id + "' (already used by " + first.first->second + ", also in " + name + ")");
            continue;
        }
        if (manifest->api < GT_MOD_API_VERSION || manifest->api > GT_MOD_API_VERSION_2) {
            skip("api " + std::to_string(manifest->api) + " (host implements " + std::to_string(GT_MOD_API_VERSION) + " to " +
                 std::to_string(GT_MOD_API_VERSION_2) + ")");
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
        mod->api_version = static_cast<int>(manifest->api);
        mod->manifest = mod_dir / "mod.toml";
        GtMod *self = mod.get();
        live_mods().push_back(std::move(mod));
        services::begin_init(self);
        const int status = init(&mod_api(self->api_version), self);  // a v1 mod is told version 1
        services::end_init();
        if (status != 0) {
            remove_hooks_for(rt, self);  // a skipped mod must not stay partly active
            skip("gt_mod_init returned " + std::to_string(status));
            continue;
        }
        if (manifest->enabled) {
            report.loaded.push_back(manifest->id);
            report.entries.push_back(ModEntry{entry_name, ModState::Active, "", self->manifest, self});
        } else {
            set_active(self, false);
            report.skipped.push_back(name + ": disabled");
            report.entries.push_back(ModEntry{entry_name, ModState::Inactive, "disabled", self->manifest, self});
        }
    }
    return report;
}

std::filesystem::path mods_dir(const char *argv0) {
    if (const char *env = std::getenv("GT_MODS_DIR"); env != nullptr && *env != '\0') return env;
    std::error_code ec;
    const auto self = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec && self.has_parent_path()) return self.parent_path() / "mods";
    ec.clear();
    return std::filesystem::absolute(argv0 != nullptr ? argv0 : "", ec).parent_path() / "mods";
}

} // namespace gt::mods
