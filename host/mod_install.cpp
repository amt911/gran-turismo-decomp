#include "mod_install.hpp"

#include "mini_json.hpp"
#include "mod_manifest.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <openssl/evp.h>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <system_error>

namespace gt::mods {

namespace fs = std::filesystem;

std::string_view host_platform() {
#if defined(__linux__) && defined(__x86_64__)
    return "linux-x86_64";
#elif defined(__linux__) && defined(__aarch64__)
    return "linux-arm64";
#elif defined(_WIN32) && defined(_M_X64)
    return "windows-x86_64";
#else
    return "unknown";
#endif
}

namespace {

bool valid_id(std::string_view id) {
    if (id.empty() || id.size() > 64) return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-')) return false;
    return true;
}

bool valid_version(std::string_view v) {
    if (v.empty() || v.front() == '.' || v.back() == '.') return false;
    for (char c : v)
        if (!((c >= '0' && c <= '9') || c == '.')) return false;
    return v.find("..") == std::string_view::npos;
}

bool valid_sha256(std::string_view h) {
    if (h.size() != 64) return false;
    for (char c : h)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

const std::string *string_member(const json::Value &object, std::string_view key) {
    const json::Value *v = object.get(key);
    return v != nullptr && v->kind == json::Value::Kind::String ? &v->string : nullptr;
}

std::string trim(std::string_view s) {
    const auto b = s.find_first_not_of(" \t\r");
    if (b == std::string_view::npos) return {};
    const auto e = s.find_last_not_of(" \t\r");
    return std::string(s.substr(b, e - b + 1));
}

// The value of a `key = value` line without a trailing `# comment` (a '#' inside a quoted string stays).
std::string raw_value(std::string_view rest) {
    bool quoted = false;
    for (std::size_t i = 0; i < rest.size(); ++i) {
        if (rest[i] == '"') quoted = !quoted;
        if (rest[i] == '#' && !quoted) return trim(rest.substr(0, i));
    }
    return trim(rest);
}

struct Line {
    std::string section;  // "" = root
    std::string key;      // "" = not a key line
    std::string value;
};

Line classify(std::string_view line, std::string &section) {
    const std::string t = trim(line);
    if (t.size() > 2 && t.front() == '[' && t.back() == ']') section = t.substr(1, t.size() - 2);
    Line out{section, {}, {}};
    if (t.empty() || t.front() == '#' || t.front() == '[') return out;
    const auto eq = t.find('=');
    if (eq == std::string::npos) return out;
    out.key = trim(std::string_view(t).substr(0, eq));
    out.value = raw_value(std::string_view(t).substr(eq + 1));
    return out;
}

bool sha256_hex(const std::vector<unsigned char> &bytes, std::string &hex) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int length = 0;
    if (EVP_Digest(bytes.data(), bytes.size(), digest, &length, EVP_sha256(), nullptr) != 1) return false;
    hex.clear();
    char two[3];
    for (unsigned int i = 0; i < length; ++i) {
        std::snprintf(two, sizeof two, "%02x", digest[i]);
        hex += two;
    }
    return true;
}

// `path` (an archive member) as a path under `<id>/`: no absolute path, no `..`, no empty or `.` parts.
bool safe_member(std::string_view path, std::string_view id, std::string &relative) {
    if (path.substr(0, id.size() + 1) != std::string(id) + "/") return false;
    std::string_view rest = path.substr(id.size() + 1);
    relative.clear();
    while (!rest.empty()) {
        const auto slash = rest.find('/');
        const std::string_view part = rest.substr(0, slash);
        if (part == ".." || part == ".") return false;
        if (!part.empty()) {
            if (!relative.empty()) relative += '/';
            relative += part;
        }
        if (slash == std::string_view::npos) break;
        rest = rest.substr(slash + 1);
    }
    return true;
}

bool unpack(const std::vector<unsigned char> &bytes, std::string_view id, const fs::path &into, std::string &error) {
    archive *a = archive_read_new();
    archive_read_support_filter_gzip(a);
    archive_read_support_format_tar(a);
    if (archive_read_open_memory(a, bytes.data(), bytes.size()) != ARCHIVE_OK) {
        error = std::string("not a .tar.gz: ") + archive_error_string(a);
        archive_read_free(a);
        return false;
    }
    bool ok = true;
    archive_entry *entry = nullptr;
    while (ok) {
        const int r = archive_read_next_header(a, &entry);
        if (r == ARCHIVE_EOF) break;
        if (r != ARCHIVE_OK) {
            error = std::string("bad archive: ") + archive_error_string(a);
            ok = false;
            break;
        }
        const char *name = archive_entry_pathname(entry);
        std::string relative;
        if (name == nullptr || !safe_member(name, id, relative)) {
            error = std::string("archive member outside ") + std::string(id) + "/: " + (name ? name : "?");
            ok = false;
            break;
        }
        const auto type = archive_entry_filetype(entry);
        if (type == AE_IFDIR) {
            std::error_code ec;
            if (!relative.empty()) fs::create_directories(into / relative, ec);
            continue;
        }
        if (type != AE_IFREG || relative.empty()) {
            error = std::string("archive member is not a regular file: ") + name;
            ok = false;
            break;
        }
        const fs::path target = into / relative;
        std::error_code ec;
        fs::create_directories(target.parent_path(), ec);
        std::ofstream out(target, std::ios::binary | std::ios::trunc);
        const void *block = nullptr;
        std::size_t size = 0;
        la_int64_t offset = 0;
        int data;
        while ((data = archive_read_data_block(a, &block, &size, &offset)) == ARCHIVE_OK)
            out.write(static_cast<const char *>(block), static_cast<std::streamsize>(size));
        if (data != ARCHIVE_EOF || !out) {
            error = std::string("could not unpack ") + name;
            ok = false;
        }
    }
    archive_read_free(a);
    return ok;
}

std::string read_file(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    return text.str();
}

} // namespace

std::optional<std::vector<IndexEntry>> parse_index(std::string_view text, std::string_view platform, std::string &error) {
    auto root = json::parse(text, error);
    if (!root) return std::nullopt;
    const json::Value *list = root->get("mods");
    if (list == nullptr || list->kind != json::Value::Kind::Array) {
        error = "index: `mods` is not an array";
        return std::nullopt;
    }
    std::vector<IndexEntry> entries;
    for (const json::Value &mod : list->array) {
        const std::string *id = string_member(mod, "id"), *name = string_member(mod, "name"),
                          *version = string_member(mod, "version");
        const json::Value *api = mod.get("api"), *platforms = mod.get("platforms");
        if (id == nullptr || !valid_id(*id) || name == nullptr || version == nullptr || !valid_version(*version) ||
            api == nullptr || api->kind != json::Value::Kind::Number || platforms == nullptr ||
            platforms->kind != json::Value::Kind::Object) {
            error = "index: a mod entry lacks a valid id, name, version, api or platforms";
            return std::nullopt;
        }
        const json::Value *archive = platforms->get(platform);
        if (archive == nullptr) continue;  // no build for this platform
        const std::string *url = string_member(*archive, "url"), *sha = string_member(*archive, "sha256");
        const json::Value *size = archive->get("size");
        if (url == nullptr || url->rfind("https://", 0) != 0 || sha == nullptr || !valid_sha256(*sha) || size == nullptr ||
            size->kind != json::Value::Kind::Number || size->number < 1 || size->number > 64.0 * 1024 * 1024 ||
            std::floor(size->number) != size->number) {
            error = "index: " + *id + " has a bad url (https only), sha256 or size";
            return std::nullopt;
        }
        IndexEntry e;
        e.id = *id;
        e.name = *name;
        e.version = *version;
        if (const std::string *d = string_member(mod, "description")) e.description = *d;
        e.api = static_cast<int>(api->number);
        e.url = *url;
        e.sha256 = *sha;
        e.size = static_cast<std::uint64_t>(size->number);
        entries.push_back(std::move(e));
    }
    return entries;
}

int compare_versions(std::string_view a, std::string_view b) {
    while (!a.empty() || !b.empty()) {
        const auto take = [](std::string_view &s) {
            const auto dot = s.find('.');
            const std::string_view part = s.substr(0, dot);
            s = dot == std::string_view::npos ? std::string_view{} : s.substr(dot + 1);
            unsigned long value = 0;
            for (char c : part) {
                if (c < '0' || c > '9') return 0ul;
                value = value * 10u + static_cast<unsigned long>(c - '0');
            }
            return value;
        };
        const unsigned long x = take(a), y = take(b);
        if (x != y) return x < y ? -1 : 1;
    }
    return 0;
}

bool verify_archive(const std::vector<unsigned char> &bytes, const IndexEntry &entry, std::string &error) {
    if (bytes.size() != entry.size) {
        error = "size " + std::to_string(bytes.size()) + " is not the index's " + std::to_string(entry.size);
        return false;
    }
    std::string hex;
    if (!sha256_hex(bytes, hex)) {
        error = "sha256 failed";
        return false;
    }
    if (hex != entry.sha256) {
        error = "sha256 mismatch: the download is not the released archive";
        return false;
    }
    return true;
}

std::string merge_manifest_text(std::string_view old_text, std::string_view new_text) {
    std::string old_enabled;
    std::map<std::string, std::string> old_settings;
    {
        std::string section;
        std::istringstream in{std::string(old_text)};
        for (std::string line; std::getline(in, line);) {
            const Line l = classify(line, section);
            if (l.key.empty()) continue;
            if (l.section.empty() && l.key == "enabled") old_enabled = l.value;
            if (l.section == "settings") old_settings[l.key] = l.value;
        }
    }
    std::string out, section;
    std::istringstream in{std::string(new_text)};
    for (std::string line; std::getline(in, line);) {
        const Line l = classify(line, section);
        if (!l.key.empty() && l.section.empty() && l.key == "enabled" && !old_enabled.empty()) {
            line = "enabled = " + old_enabled;
        } else if (!l.key.empty() && l.section == "settings") {
            if (const auto found = old_settings.find(l.key); found != old_settings.end())
                line = l.key + " = " + found->second;
        }
        out += line;
        out += '\n';
    }
    return out;
}

bool install_archive(const std::vector<unsigned char> &bytes, const IndexEntry &entry, const fs::path &mods_dir,
                     std::string &error) {
    if (!valid_id(entry.id)) {
        error = "bad mod id";
        return false;
    }
    if (!verify_archive(bytes, entry, error)) return false;
    const fs::path staging_root = mods_dir / ".staging";
    const fs::path staging = staging_root / entry.id;
    const fs::path old_copy = staging_root / (entry.id + ".old");
    const fs::path target = mods_dir / entry.id;
    std::error_code ec;
    fs::remove_all(staging, ec);
    fs::remove_all(old_copy, ec);
    fs::create_directories(staging, ec);
    if (ec) {
        error = "cannot create " + staging.string() + ": " + ec.message();
        return false;
    }
    const auto give_up = [&](std::string why) {
        error = std::move(why);
        std::error_code ignore;
        fs::remove_all(staging, ignore);
        return false;
    };
    std::string why;
    if (!unpack(bytes, entry.id, staging, why)) return give_up(why);
    std::string manifest_text = read_file(staging / "mod.toml");
    auto manifest = parse_manifest(manifest_text, why);
    if (!manifest) return give_up("mod.toml: " + why);
    if (manifest->id != entry.id) return give_up("mod.toml id `" + manifest->id + "` is not `" + entry.id + "`");
    if (manifest->library.find('/') != std::string::npos || !fs::is_regular_file(staging / manifest->library))
        return give_up("the archive lacks the library " + manifest->library);
    if (fs::exists(target / "mod.toml")) {
        manifest_text = merge_manifest_text(read_file(target / "mod.toml"), manifest_text);
        std::ofstream(staging / "mod.toml", std::ios::trunc) << manifest_text;
        if (!parse_manifest(manifest_text, why)) return give_up("merged mod.toml: " + why);
    }
    const bool had_old = fs::exists(target);
    if (had_old) {
        fs::rename(target, old_copy, ec);
        if (ec) return give_up("cannot move the installed mod aside: " + ec.message());
    }
    fs::rename(staging, target, ec);
    if (ec) {
        std::error_code ignore;
        if (had_old) fs::rename(old_copy, target, ignore);
        return give_up("cannot move the new mod in: " + ec.message());
    }
    fs::remove_all(old_copy, ec);
    fs::remove(staging_root, ec);  // only when empty
    return true;
}

} // namespace gt::mods
