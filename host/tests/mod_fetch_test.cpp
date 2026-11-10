// host/mod_fetch.hpp: only https is fetched. With GT_NET_TEST=1 it also downloads the real gt-mods release index and the
// analog-pedals archive, verifies and installs it into a temporary mods directory (the end-to-end path of GET MODS).
#include "mod_fetch.hpp"
#include "mod_install.hpp"
#include "mod_manifest.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}
} // namespace

int main() {
    std::vector<unsigned char> body;
    std::string error;
    check(!gt::mods::fetch_https("http://example.com/index.json", body, 1024, error) && !error.empty(), "http:// refused");
    check(!gt::mods::fetch_https("file:///etc/passwd", body, 1024, error) && body.empty(), "file:// refused");
    check(gt::mods::index_url().rfind("https://", 0) == 0, "index url is https");
    if (const char *net = std::getenv("GT_NET_TEST"); net != nullptr && *net == '1') {
        const bool got = gt::mods::fetch_https(gt::mods::index_url(), body, 1u << 20u, error);
        check(got, "live: index fetched");
        if (!got) std::fprintf(stderr, "  %s\n", error.c_str());
        auto entries = gt::mods::parse_index(std::string(body.begin(), body.end()), gt::mods::host_platform(), error);
        check(entries.has_value() && !entries->empty(), "live: index parsed");
        if (entries) {
            for (const auto &e : *entries) {
                if (e.id != "analog-pedals") continue;
                std::vector<unsigned char> archive;
                check(gt::mods::fetch_https(e.url, archive, 64u << 20u, error), "live: archive fetched (redirect followed)");
                const fs::path mods = fs::temp_directory_path() / "gt_mod_fetch_test";
                fs::remove_all(mods);
                fs::create_directories(mods);
                check(gt::mods::install_archive(archive, e, mods, error), "live: archive verified and installed");
                if (!error.empty()) std::fprintf(stderr, "  %s\n", error.c_str());
                std::ifstream in(mods / "analog-pedals" / "mod.toml");
                std::stringstream text;
                text << in.rdbuf();
                auto m = gt::mods::parse_manifest(text.str(), error);
                check(m && m->version == e.version, "live: installed manifest has the index version");
                fs::remove_all(mods);
            }
        }
    }
    if (failures == 0) std::puts("mod_fetch: all checks passed");
    return failures == 0 ? 0 : 1;
}
