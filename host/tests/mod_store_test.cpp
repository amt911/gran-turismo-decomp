// GET MODS rows (host/mod_store.hpp) with a fake fetch: check the index, install, update keeps the row honest, errors.
#include "mod_install.hpp"
#include "mod_store.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <openssl/evp.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace fs = std::filesystem;
namespace store = gt::mods::store;

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::vector<unsigned char> tar_gz(const std::map<std::string, std::string> &files) {
    std::vector<unsigned char> out(1u << 20u);
    std::size_t used = 0;
    archive *a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_memory(a, out.data(), out.size(), &used);
    for (const auto &[path, data] : files) {
        archive_entry *e = archive_entry_new();
        archive_entry_set_pathname(e, path.c_str());
        archive_entry_set_filetype(e, AE_IFREG);
        archive_entry_set_size(e, static_cast<la_int64_t>(data.size()));
        archive_entry_set_perm(e, 0644);
        archive_write_header(a, e);
        archive_write_data(a, data.data(), data.size());
        archive_entry_free(e);
    }
    archive_write_close(a);
    archive_write_free(a);
    out.resize(used);
    return out;
}

std::string sha(const std::vector<unsigned char> &bytes) {
    unsigned char d[EVP_MAX_MD_SIZE];
    unsigned int n = 0;
    EVP_Digest(bytes.data(), bytes.size(), d, &n, EVP_sha256(), nullptr);
    std::string hex;
    char two[3];
    for (unsigned int i = 0; i < n; ++i) {
        std::snprintf(two, sizeof two, "%02x", d[i]);
        hex += two;
    }
    return hex;
}

std::map<std::string, std::vector<unsigned char>> g_served;  // url -> body
bool g_offline = false;

bool fake_fetch(const std::string &url, std::vector<unsigned char> &body, std::size_t, std::string &error) {
    if (g_offline) {
        error = "Could not resolve host";
        return false;
    }
    const auto found = g_served.find(url);
    if (found == g_served.end()) {
        error = "HTTP 404";
        return false;
    }
    body = found->second;
    return true;
}

std::string manifest(const char *version) {
    return std::string("id = \"demo\"\nname = \"Demo\"\nversion = \"") + version +
           "\"\nlibrary = \"libdemo.so\"\napi = 2\nenabled = false\n";
}

void serve(const char *version, bool corrupt = false) {
    auto archive = tar_gz({{"demo/mod.toml", manifest(version)}, {"demo/libdemo.so", std::string("ELF-") + version}});
    const std::string hash = sha(archive);
    if (corrupt) archive[archive.size() / 2] ^= 1u;
    const std::string url = std::string("https://example.invalid/demo-") + version + ".tar.gz";
    g_served[url] = archive;
    const std::string index = std::string(R"({"mods": [{"api": 2, "id": "demo", "name": "Demo", "version": ")") + version +
                              R"(", "platforms": {")" + std::string(gt::mods::host_platform()) + R"(": {"url": ")" + url +
                              R"(", "sha256": ")" + hash + R"(", "size": )" + std::to_string(archive.size()) + "}}}]}";
    g_served["https://example.invalid/index.json"] = std::vector<unsigned char>(index.begin(), index.end());
}

bool contains(const std::string &text, const char *part) { return text.find(part) != std::string::npos; }
} // namespace

int main() {
    const fs::path mods = fs::temp_directory_path() / "gt_mod_store_test";
    fs::remove_all(mods);
    fs::create_directories(mods);
    check(store::count() == 0, "no rows before configure");
    store::configure(mods, fake_fetch, "https://example.invalid/index.json");
    check(store::count() == 1 && contains(store::row(0, 40), "CHECK"), "one GET MODS row before the index");

    g_offline = true;
    check(store::activate(0), "Enter on GET MODS starts a check");
    store::wait_idle();
    check(contains(store::row(0, 40), "ERROR"), "offline: the row says ERROR, the game runs on");

    g_offline = false;
    serve("0.1.0");
    store::activate(0);
    store::wait_idle();
    check(store::count() == 2 && contains(store::row(0, 40), "1 AVAILABLE"), "index fetched: one mod");
    check(contains(store::row(1, 40), "INSTALL") && contains(store::row(1, 40), "Demo"), "a new mod says INSTALL");
    check(store::row(1, 40).size() <= 40, "rows fit the width");

    check(store::activate(1), "Enter on INSTALL starts the install");
    store::wait_idle();
    check(fs::exists(mods / "demo" / "libdemo.so"), "the mod is installed");
    check(contains(store::row(1, 40), "RESTART"), "an installed mod says RESTART TO LOAD");
    check(!store::activate(1), "Enter on RESTART TO LOAD does nothing");

    // A newer release: UPDATE <installed>, then a corrupt download is refused and the old mod stays.
    serve("0.2.0", true);
    store::activate(0);
    store::wait_idle();
    check(contains(store::row(1, 40), "UPDATE") && contains(store::row(1, 40), "0.1.0"), "a newer version says UPDATE <installed>");
    store::activate(1);
    store::wait_idle();
    check(contains(store::row(1, 40), "ERROR"), "a corrupt download shows ERROR");
    std::ifstream lib(mods / "demo" / "libdemo.so");
    std::string kept((std::istreambuf_iterator<char>(lib)), std::istreambuf_iterator<char>());
    check(kept == "ELF-0.1.0", "a refused update leaves the installed mod");

    serve("0.2.0");
    store::activate(0);
    store::wait_idle();
    store::activate(1);
    store::wait_idle();
    check(contains(store::row(1, 40), "RESTART"), "the good update installs");
    store::activate(0);
    store::wait_idle();
    check(contains(store::row(1, 40), "RESTART"), "a re-check still says RESTART TO LOAD until the next start");
    store::configure(mods, fake_fetch, "https://example.invalid/index.json");  // the next start
    store::activate(0);
    store::wait_idle();
    check(contains(store::row(1, 40), "UP TO DATE"), "next start: the installed version is up to date");
    check(!store::activate(1), "Enter on UP TO DATE does nothing");
    check(!store::activate(7), "out of range does nothing");
    check(store::row_of("demo") == 1 && store::row_of("nope") == 0, "row_of finds the mod");

    fs::remove_all(mods);
    if (failures == 0) std::puts("mod_store: all checks passed");
    return failures == 0 ? 0 : 1;
}
