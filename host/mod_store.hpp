// GET MODS rows of the F9 MODS page (docs/superpowers/specs/2026-11-08-mod-downloader-design.md, step 3): the gt-mods release
// index and one row per mod, installed or updated with Enter. Fetching and installing run on a worker thread; the rows read
// the state under a lock, so the overlay never waits. Installed or updated mods load at the next start.
#pragma once

#include <cstddef>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace gt::mods::store {

using Fetch = std::function<bool(const std::string &url, std::vector<unsigned char> &body, std::size_t limit,
                                 std::string &error)>;

// Turns the rows on for `mods_dir`. `fetch` defaults to fetch_https and `index` to index_url() (host/mod_fetch.hpp).
void configure(std::filesystem::path mods_dir, Fetch fetch = {}, std::string index = {});

// 0 before configure; then the "GET MODS" row plus one per mod of the fetched index.
std::size_t count();

// Row 0: `GET MODS  ENTER: CHECK`, `GET MODS  CHECKING...`, `GET MODS  <n> AVAILABLE` or `GET MODS  ERROR <reason>`.
// A mod row: `<name> <version> <state>`, state INSTALL, UPDATE <installed>, UP TO DATE, DOWNLOADING, RESTART TO LOAD or
// ERROR <reason>; at most `width` characters.
std::string row(std::size_t index, std::size_t width);

// Enter on a row: row 0 fetches the index; a mod row in INSTALL or UPDATE downloads, verifies and installs it. False when
// nothing starts (busy, up to date, out of range).
bool activate(std::size_t index);

// The row of mod `id` in the fetched index, or 0 when it is not there.
std::size_t row_of(const std::string &id);

// Waits for the worker to finish (tests and the in-game check).
void wait_idle();

} // namespace gt::mods::store
