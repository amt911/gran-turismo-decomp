// Mod downloader core (docs/superpowers/specs/2026-11-08-mod-downloader-design.md): the release index, version order, and
// installing one downloaded archive into the mods directory. No networking here (host/mod_fetch.hpp) and no UI.
#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace gt::mods {

// One mod of the release index (gt-mods `index.json`), for one platform.
struct IndexEntry {
    std::string id, name, version, description;
    int api = 0;
    std::string url, sha256;  // sha256: 64 lowercase hex digits
    std::uint64_t size = 0;
};

// The host's platform key in the index ("linux-x86_64").
std::string_view host_platform();

// `index.json` → the mods that have an archive for `platform`; mods without one are skipped. A malformed index, or an
// entry with a bad id (not [a-z0-9-]+), version, url (https only), sha256 or size, fails the whole index: nullopt + error.
std::optional<std::vector<IndexEntry>> parse_index(std::string_view text, std::string_view platform, std::string &error);

// Dotted numeric versions ("0.2.0"); missing parts count as 0, a non-numeric part compares as 0. <0, 0, >0.
int compare_versions(std::string_view a, std::string_view b);

// The archive matches the index: exact size and sha256.
bool verify_archive(const std::vector<unsigned char> &bytes, const IndexEntry &entry, std::string &error);

// The new mod.toml with the user's choices from the old one: the root `enabled` value, and each [settings] key that both
// files have keeps the old value (new keys keep the new defaults, removed keys go). Other lines are the new file's.
std::string merge_manifest_text(std::string_view old_text, std::string_view new_text);

// Verifies `bytes` (a .tar.gz with one folder `<id>/` holding mod.toml and the library) and installs it as
// `<mods_dir>/<id>/`: unpacked to `<mods_dir>/.staging/<id>/` (only regular files under `<id>/`, no links, no `..`), the
// manifest checked (its id must be the entry's) and merged with an installed one, then swapped in by renames. On any
// failure the installed mod (if any) is left untouched and `error` says why.
bool install_archive(const std::vector<unsigned char> &bytes, const IndexEntry &entry,
                     const std::filesystem::path &mods_dir, std::string &error);

} // namespace gt::mods
