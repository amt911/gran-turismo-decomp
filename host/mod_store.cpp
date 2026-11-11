#include "mod_store.hpp"

#include "mod_fetch.hpp"
#include "mod_install.hpp"
#include "mod_manifest.hpp"

#include <condition_variable>
#include <map>
#include <optional>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

namespace gt::mods::store {

namespace {

constexpr std::size_t kIndexLimit = 1u << 20u;      // 1 MiB
constexpr std::size_t kArchiveLimit = 64u << 20u;   // 64 MiB, the index's own cap

enum class Check { Never, Running, Done, Failed };

struct Store {
    std::mutex mutex;
    std::condition_variable idle;
    bool configured = false;
    bool busy = false;
    std::filesystem::path mods_dir;
    Fetch fetch;
    std::string index;
    Check check = Check::Never;
    std::string check_error;
    std::vector<IndexEntry> entries;
    std::string downloading;                  // id being installed
    std::map<std::string, std::string> installed_this_run;  // id -> version installed now; it loads at the next start
    std::map<std::string, std::string> errors; // id -> last install error
};

Store &store() {
    static Store s;
    return s;
}

std::string installed_version(const std::filesystem::path &mods_dir, const std::string &id) {
    std::ifstream in(mods_dir / id / "mod.toml");
    if (!in) return {};
    std::stringstream text;
    text << in.rdbuf();
    std::string error;
    const auto manifest = parse_manifest(text.str(), error);
    if (!manifest) return {};
    return manifest->version.empty() ? "0.0.0" : manifest->version;
}

std::string fit(std::string text, std::size_t width) {
    if (text.size() > width) text.resize(width);
    return text;
}

// Runs `job` on a detached worker; `busy` is set by the caller under the lock.
void run(std::function<void()> job) {
    std::thread([job = std::move(job)] {
        job();
        Store &s = store();
        std::lock_guard<std::mutex> lock(s.mutex);
        s.busy = false;
        s.idle.notify_all();
    }).detach();
}

} // namespace

void configure(std::filesystem::path mods_dir, Fetch fetch, std::string index) {
    wait_idle();
    Store &s = store();
    std::lock_guard<std::mutex> lock(s.mutex);
    s.configured = true;
    s.mods_dir = std::move(mods_dir);
    s.fetch = fetch ? std::move(fetch) : Fetch(&fetch_https);
    s.index = index.empty() ? index_url() : std::move(index);
    s.check = Check::Never;
    s.check_error.clear();
    s.entries.clear();
    s.downloading.clear();
    s.installed_this_run.clear();
    s.errors.clear();
}

std::size_t count() {
    Store &s = store();
    std::lock_guard<std::mutex> lock(s.mutex);
    return s.configured ? 1 + s.entries.size() : 0;
}

std::string row(std::size_t index, std::size_t width) {
    Store &s = store();
    std::lock_guard<std::mutex> lock(s.mutex);
    if (!s.configured) return {};
    if (index == 0) {
        switch (s.check) {
        case Check::Never: return fit("GET MODS  ENTER: CHECK", width);
        case Check::Running: return fit("GET MODS  CHECKING...", width);
        case Check::Done: return fit("GET MODS  " + std::to_string(s.entries.size()) + " AVAILABLE", width);
        case Check::Failed: return fit("GET MODS  ERROR " + s.check_error, width);
        }
    }
    if (index - 1 >= s.entries.size()) return {};
    const IndexEntry &e = s.entries[index - 1];
    std::string state;
    if (s.downloading == e.id) {
        state = "DOWNLOADING";
    } else if (const auto now = s.installed_this_run.find(e.id);
               now != s.installed_this_run.end() && compare_versions(now->second, e.version) >= 0) {
        state = "RESTART TO LOAD";
    } else if (const auto error = s.errors.find(e.id); error != s.errors.end()) {
        state = "ERROR " + error->second;
    } else {
        const std::string have = installed_version(s.mods_dir, e.id);
        if (have.empty()) state = "INSTALL";
        else if (compare_versions(have, e.version) < 0) state = "UPDATE " + have;
        else state = "UP TO DATE";
    }
    // `<name> <version> <state>`: the state is never cut, the name gives way first.
    std::string tail = e.version + " " + state;
    if (tail.size() + 2 > width) return fit(tail, width);
    std::string name = e.name;
    const std::size_t room = width - tail.size() - 1;
    if (name.size() > room) name.resize(room);
    name.append(room - name.size(), ' ');
    return name + " " + tail;
}

bool activate(std::size_t index) {
    Store &s = store();
    std::unique_lock<std::mutex> lock(s.mutex);
    if (!s.configured || s.busy) return false;
    if (index == 0) {
        s.busy = true;
        s.check = Check::Running;
        const Fetch fetch = s.fetch;
        const std::string url = s.index;
        lock.unlock();
        run([fetch, url] {
            std::vector<unsigned char> body;
            std::string error;
            std::optional<std::vector<IndexEntry>> entries;
            if (fetch(url, body, kIndexLimit, error))
                entries = parse_index(std::string(body.begin(), body.end()), host_platform(), error);
            Store &st = store();
            std::lock_guard<std::mutex> guard(st.mutex);
            if (entries) {
                st.entries = std::move(*entries);
                st.check = Check::Done;
                st.errors.clear();
            } else {
                st.check = Check::Failed;
                st.check_error = error;
            }
        });
        return true;
    }
    if (index - 1 >= s.entries.size()) return false;
    const IndexEntry entry = s.entries[index - 1];
    if (const auto now = s.installed_this_run.find(entry.id);
        now != s.installed_this_run.end() && compare_versions(now->second, entry.version) >= 0)
        return false;  // installed now, loads at the next start
    const std::string have = installed_version(s.mods_dir, entry.id);
    if (!have.empty() && compare_versions(have, entry.version) >= 0) return false;  // up to date
    s.busy = true;
    s.downloading = entry.id;
    s.errors.erase(entry.id);
    const Fetch fetch = s.fetch;
    const std::filesystem::path mods_dir = s.mods_dir;
    lock.unlock();
    run([fetch, entry, mods_dir] {
        std::vector<unsigned char> archive;
        std::string error;
        const bool ok = fetch(entry.url, archive, kArchiveLimit, error) && install_archive(archive, entry, mods_dir, error);
        Store &st = store();
        std::lock_guard<std::mutex> guard(st.mutex);
        st.downloading.clear();
        if (ok) st.installed_this_run[entry.id] = entry.version;
        else st.errors[entry.id] = error;
    });
    return true;
}

std::size_t row_of(const std::string &id) {
    Store &s = store();
    std::lock_guard<std::mutex> lock(s.mutex);
    for (std::size_t i = 0; i < s.entries.size(); ++i)
        if (s.entries[i].id == id) return i + 1;
    return 0;
}

void wait_idle() {
    Store &s = store();
    std::unique_lock<std::mutex> lock(s.mutex);
    s.idle.wait(lock, [&s] { return !s.busy; });
}

} // namespace gt::mods::store
