#include "mod_fetch.hpp"

#include <curl/curl.h>

#include <cstdlib>
#include <mutex>

namespace gt::mods {

namespace {

struct Sink {
    std::vector<unsigned char> *body;
    std::size_t limit;
    bool over = false;
};

std::size_t write_body(char *data, std::size_t size, std::size_t count, void *user) {
    Sink &sink = *static_cast<Sink *>(user);
    const std::size_t bytes = size * count;
    if (sink.body->size() + bytes > sink.limit) {
        sink.over = true;
        return 0;  // aborts the transfer
    }
    sink.body->insert(sink.body->end(), data, data + bytes);
    return bytes;
}

} // namespace

bool fetch_https(const std::string &url, std::vector<unsigned char> &body, std::size_t limit, std::string &error) {
    body.clear();
    if (url.rfind("https://", 0) != 0) {
        error = "only https:// URLs are fetched";
        return false;
    }
    static std::once_flag init;
    std::call_once(init, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });
    CURL *curl = curl_easy_init();
    if (curl == nullptr) {
        error = "curl_easy_init failed";
        return false;
    }
    Sink sink{&body, limit};
    char message[CURL_ERROR_SIZE] = {};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "gt_play-mod-downloader");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, message);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &sink);
    const CURLcode result = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);
    if (sink.over) {
        error = "the download is larger than " + std::to_string(limit) + " bytes";
        return false;
    }
    if (result != CURLE_OK) {
        error = message[0] != '\0' ? message : curl_easy_strerror(result);
        return false;
    }
    if (status != 200) {
        error = "HTTP " + std::to_string(status);
        return false;
    }
    return true;
}

std::string index_url() {
    if (const char *url = std::getenv("GT_MODS_INDEX_URL"); url != nullptr && *url != '\0') return url;
    return "https://github.com/amt911/gt-mods/releases/latest/download/index.json";
}

} // namespace gt::mods
