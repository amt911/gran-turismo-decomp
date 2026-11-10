// HTTPS download for the mod downloader (docs/superpowers/specs/2026-11-08-mod-downloader-design.md), on libcurl. Blocking:
// the GET MODS page calls it from its worker thread, never from the game or the renderer.
#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace gt::mods {

// GETs `url` into `body`. HTTPS only, redirects too (GitHub release assets redirect to objects.githubusercontent.com);
// at most `limit` bytes; 15 s to connect, 120 s in all; certificates verified. False + `error` on anything but HTTP 200.
bool fetch_https(const std::string &url, std::vector<unsigned char> &body, std::size_t limit, std::string &error);

// The index URL: GT_MODS_INDEX_URL if set, else the latest gt-mods release's index.json.
std::string index_url();

} // namespace gt::mods
