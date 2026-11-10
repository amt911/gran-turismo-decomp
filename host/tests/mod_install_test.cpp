// Mod downloader core (host/mini_json.hpp, host/mod_install.hpp): JSON, the release index, version order, archive
// verification, manifest merge on update, and install by staging + rename. Archives are built here with libarchive.
#include "mini_json.hpp"
#include "mod_install.hpp"
#include "mod_manifest.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <openssl/sha.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fs = std::filesystem;
using namespace gt;

namespace {
int failures = 0;
void check(bool condition, const char *what) {
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        ++failures;
    }
}

std::string read(const fs::path &path) {
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

struct File {
    std::string path, data;
    bool symlink = false;
};

std::vector<unsigned char> tar_gz(const std::vector<File> &files) {
    std::vector<unsigned char> out(1u << 20u);
    std::size_t used = 0;
    archive *a = archive_write_new();
    archive_write_add_filter_gzip(a);
    archive_write_set_format_pax_restricted(a);
    archive_write_open_memory(a, out.data(), out.size(), &used);
    for (const File &f : files) {
        archive_entry *e = archive_entry_new();
        archive_entry_set_pathname(e, f.path.c_str());
        if (f.symlink) {
            archive_entry_set_filetype(e, AE_IFLNK);
            archive_entry_set_symlink(e, f.data.c_str());
            archive_entry_set_size(e, 0);
        } else {
            archive_entry_set_filetype(e, AE_IFREG);
            archive_entry_set_size(e, static_cast<la_int64_t>(f.data.size()));
        }
        archive_entry_set_perm(e, 0644);
        archive_write_header(a, e);
        if (!f.symlink) archive_write_data(a, f.data.data(), f.data.size());
        archive_entry_free(e);
    }
    archive_write_close(a);
    archive_write_free(a);
    out.resize(used);
    return out;
}

std::string hex_sha256(const std::vector<unsigned char> &bytes) {
    unsigned char digest[SHA256_DIGEST_LENGTH];
    SHA256(bytes.data(), bytes.size(), digest);
    std::string hex;
    char two[3];
    for (unsigned char b : digest) {
        std::snprintf(two, sizeof two, "%02x", b);
        hex += two;
    }
    return hex;
}

mods::IndexEntry entry_for(const std::vector<unsigned char> &bytes, const char *id = "demo", const char *version = "0.2.0") {
    mods::IndexEntry e;
    e.id = id;
    e.name = "Demo";
    e.version = version;
    e.api = 2;
    e.url = "https://example.invalid/demo.tar.gz";
    e.sha256 = hex_sha256(bytes);
    e.size = bytes.size();
    return e;
}

const char *kManifestV2 =
    "id = \"demo\"\nname = \"Demo\"\nversion = \"0.2.0\"\nlibrary = \"libdemo.so\"\napi = 2\nenabled = false\n"
    "\n[settings]\nspeed = 2.0\nnew_key = 7\n";

void test_json() {
    std::string error;
    auto v = json::parse(R"( {"a": [1, -2.5e1, true, null], "s": "x\"\u00e9\ud83d\ude00", "o": {}} )", error);
    check(v.has_value(), "json: valid document parses");
    if (v) {
        const json::Value *a = v->get("a");
        check(a && a->array.size() == 4 && a->array[1].number == -25.0 && a->array[2].boolean, "json: array members");
        const json::Value *s = v->get("s");
        check(s && s->string == "x\"\xC3\xA9\xF0\x9F\x98\x80", "json: escapes and surrogate pair to UTF-8");
        check(v->get("missing") == nullptr, "json: missing key is null");
    }
    check(!json::parse("{\"a\": 1,}", error) && !error.empty(), "json: trailing comma fails");
    check(!json::parse("[1] x", error), "json: trailing characters fail");
    check(!json::parse("\"\\ud800\"", error), "json: lone surrogate fails");
    check(!json::parse(std::string(100, '['), error), "json: deep nesting fails");
}

void test_index() {
    const std::string good = R"({"mods": [
      {"api": 2, "description": "d", "id": "analog-pedals", "name": "Analog pedals", "version": "0.2.0",
       "platforms": {"linux-x86_64": {"sha256": ")" + std::string(64, 'a') + R"(", "size": 11644,
                     "url": "https://github.com/amt911/gt-mods/releases/download/v1/a.tar.gz"}}},
      {"api": 2, "description": "w", "id": "win-only", "name": "W", "version": "1.0.0",
       "platforms": {"windows-x86_64": {"sha256": ")" + std::string(64, 'b') + R"(", "size": 1, "url": "https://x/y"}}}]})";
    std::string error;
    auto entries = mods::parse_index(good, "linux-x86_64", error);
    check(entries && entries->size() == 1 && (*entries)[0].id == "analog-pedals" && (*entries)[0].size == 11644,
          "index: the platform's mods, others skipped");
    auto bad_url = good;
    bad_url.replace(bad_url.find("https://github"), 5, "http:");
    check(!mods::parse_index(bad_url, "linux-x86_64", error), "index: a non-https url fails the index");
    auto bad_id = good;
    bad_id.replace(bad_id.find("analog-pedals"), 13, "../etc/passwd");
    check(!mods::parse_index(bad_id, "linux-x86_64", error), "index: an id that is not [a-z0-9-]+ fails");
    auto bad_hash = good;
    bad_hash.replace(bad_hash.find(std::string(64, 'a')), 3, "XYZ");
    check(!mods::parse_index(bad_hash, "linux-x86_64", error), "index: a bad sha256 fails");
    check(!mods::parse_index("{\"mods\": 3}", "linux-x86_64", error), "index: mods not an array fails");
    check(mods::host_platform() == "linux-x86_64", "index: host platform key");
}

void test_versions() {
    check(mods::compare_versions("0.2.0", "0.1.9") > 0, "version: minor wins");
    check(mods::compare_versions("0.10.0", "0.9.0") > 0, "version: numeric, not text");
    check(mods::compare_versions("1.0", "1.0.0") == 0, "version: missing parts are 0");
    check(mods::compare_versions("0.1.0", "0.2.0") < 0, "version: older");
}

void test_verify() {
    const auto bytes = tar_gz({{"demo/mod.toml", kManifestV2}});
    auto e = entry_for(bytes);
    std::string error;
    check(mods::verify_archive(bytes, e, error), "verify: matching size and hash");
    auto corrupt = bytes;
    corrupt[corrupt.size() / 2] ^= 0x01u;
    check(!mods::verify_archive(corrupt, e, error), "verify: a flipped byte is refused");
    e.size += 1;
    check(!mods::verify_archive(bytes, e, error), "verify: a size mismatch is refused");
}

void test_merge() {
    const std::string old_text =
        "id = \"demo\"\nversion = \"0.1.0\"\nlibrary = \"libdemo.so\"\napi = 2\nenabled = true  # on\n\n[settings]\n"
        "speed = 3.5\ngone = 1\n";
    const std::string merged = mods::merge_manifest_text(old_text, kManifestV2);
    std::string error;
    auto m = mods::parse_manifest(merged, error);
    check(m.has_value(), "merge: result parses");
    if (m) {
        check(m->enabled, "merge: the user's enabled is kept");
        check(m->version == "0.2.0", "merge: the new version");
        check(m->settings["speed"] == "3.5", "merge: a shared setting keeps the user's value");
        check(m->settings["new_key"] == "7", "merge: a new setting keeps its default");
        check(m->settings.count("gone") == 0, "merge: a removed setting goes");
    }
}

void test_install(const fs::path &root) {
    const fs::path mods = root / "mods";
    fs::create_directories(mods);
    std::string error;
    // A first install.
    auto bytes = tar_gz({{"demo/mod.toml", kManifestV2}, {"demo/libdemo.so", "ELF-v2"}});
    check(mods::install_archive(bytes, entry_for(bytes), mods, error), "install: a good archive installs");
    check(read(mods / "demo" / "libdemo.so") == "ELF-v2", "install: the library is in place");
    check(!fs::exists(mods / ".staging" / "demo"), "install: no staging left");
    // The user turns it on and changes a setting; an update keeps both.
    {
        std::ofstream(mods / "demo" / "mod.toml") << "id = \"demo\"\nversion = \"0.2.0\"\nlibrary = \"libdemo.so\"\napi = 2\n"
                                                    "enabled = true\n\n[settings]\nspeed = 9.0\n";
    }
    std::string v3 = kManifestV2;
    v3.replace(v3.find("0.2.0"), 5, "0.3.0");
    auto update = tar_gz({{"demo/mod.toml", v3}, {"demo/libdemo.so", "ELF-v3"}});
    check(mods::install_archive(update, entry_for(update, "demo", "0.3.0"), mods, error), "install: an update installs");
    auto m = mods::parse_manifest(read(mods / "demo" / "mod.toml"), error);
    check(m && m->enabled && m->version == "0.3.0" && m->settings["speed"] == "9.0", "install: the update keeps the user's choices");
    check(read(mods / "demo" / "libdemo.so") == "ELF-v3", "install: the update's library");
    // Refusals leave the installed mod untouched.
    const auto refuse = [&](const std::vector<unsigned char> &archive, const mods::IndexEntry &e, const char *what) {
        std::string why;
        check(!mods::install_archive(archive, e, mods, why) && !why.empty(), what);
        check(read(mods / "demo" / "libdemo.so") == "ELF-v3", "install: a refused archive leaves the old mod");
    };
    auto corrupt = update;
    corrupt[corrupt.size() / 2] ^= 0x01u;
    refuse(corrupt, entry_for(update, "demo", "0.3.0"), "install: a corrupt archive is refused");
    auto escape = tar_gz({{"demo/mod.toml", v3}, {"demo/../../evil.so", "x"}});
    refuse(escape, entry_for(escape), "install: a path outside <id>/ is refused");
    check(!fs::exists(root / "evil.so") && !fs::exists(mods / "evil.so"), "install: nothing written outside");
    auto link = tar_gz({{"demo/mod.toml", v3}, {"demo/libdemo.so", "/etc/passwd", true}});
    refuse(link, entry_for(link), "install: a symlink is refused");
    auto other = tar_gz({{"demo/mod.toml", "id = \"other\"\nlibrary = \"l.so\"\napi = 2\n"}, {"demo/l.so", "x"}});
    refuse(other, entry_for(other), "install: a manifest id that is not the entry's is refused");
    auto no_lib = tar_gz({{"demo/mod.toml", v3}});
    refuse(no_lib, entry_for(no_lib), "install: a missing library is refused");
}
} // namespace

int main() {
    const fs::path root = fs::temp_directory_path() / "gt_mod_install_test";
    fs::remove_all(root);
    fs::create_directories(root);
    test_json();
    test_index();
    test_versions();
    test_verify();
    test_merge();
    test_install(root);
    fs::remove_all(root);
    if (failures == 0) std::puts("mod_install: all checks passed");
    return failures == 0 ? 0 : 1;
}
