#include "http_client.hpp"
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <curl/curl.h>

namespace net {
namespace {

std::atomic<bool> g_enabled{false};

size_t write_cb(char* p, size_t sz, size_t nm, void* out) {
    static_cast<std::string*>(out)->append(p, sz * nm);
    return sz * nm;
}

Response run(const std::string& url, const std::string* body, const std::string& key) {
    Response r;
    CURL* c = curl_easy_init();
    if (!c) return r;
    curl_slist* hdrs = nullptr;
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, &r.body);
    curl_easy_setopt(c, CURLOPT_TIMEOUT, 8L);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);  // each request runs off-thread (std::async)
    if (body) {
        hdrs = curl_slist_append(hdrs, "Content-Type: application/json");
        if (!key.empty()) hdrs = curl_slist_append(hdrs, ("X-Game-Key: " + key).c_str());
        curl_easy_setopt(c, CURLOPT_HTTPHEADER, hdrs);
        curl_easy_setopt(c, CURLOPT_POSTFIELDS, body->c_str());
    }
    if (curl_easy_perform(c) == CURLE_OK) {
        curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
        if (r.status == 0 && !r.body.empty()) r.status = 200;  // file:// has no HTTP status
    }
    if (hdrs) curl_slist_free_all(hdrs);
    curl_easy_cleanup(c);
    return r;
}

size_t write_file_cb(char* p, size_t sz, size_t nm, void* out) {
    return std::fwrite(p, sz, nm, static_cast<std::FILE*>(out)) * sz;
}

Response run_download(const std::string& url, const std::string& dest) {
    Response r;
    // Download beside the destination, rename only on success: the caller
    // EXECUTES this file, and a truncated installer must never be reachable
    // under the final name.
    const std::string part = dest + ".part";
    std::FILE* f = std::fopen(part.c_str(), "wb");
    if (!f) return r;
    CURL* c = curl_easy_init();
    if (!c) { std::fclose(f); return r; }
    curl_easy_setopt(c, CURLOPT_URL, url.c_str());
    curl_easy_setopt(c, CURLOPT_WRITEFUNCTION, write_file_cb);
    curl_easy_setopt(c, CURLOPT_WRITEDATA, f);
    curl_easy_setopt(c, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(c, CURLOPT_NOSIGNAL, 1L);
    // No overall timeout (installers are tens of MB); abort only if the
    // transfer sits under 1 KB/s for 30s straight.
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_LIMIT, 1024L);
    curl_easy_setopt(c, CURLOPT_LOW_SPEED_TIME, 30L);
    const CURLcode rc = curl_easy_perform(c);
    if (rc == CURLE_OK) curl_easy_getinfo(c, CURLINFO_RESPONSE_CODE, &r.status);
    curl_easy_cleanup(c);
    std::fclose(f);
    std::error_code ec;
    if (rc == CURLE_OK && r.ok()) {
        std::filesystem::rename(part, dest, ec);
        if (ec) r.status = 0;              // could not publish it -> not ok
    } else {
        std::filesystem::remove(part, ec);
    }
    return r;
}

}  // namespace

bool enabled() { return g_enabled.load(); }

void set_enabled(bool on) {
    static bool inited = false;
    if (on && !inited) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        inited = true;
    }
    g_enabled.store(on);
}

std::future<Response> get(const std::string& url) {
    if (!enabled()) {
        std::promise<Response> p;
        p.set_value({});
        return p.get_future();
    }
    return std::async(std::launch::async, [url] { return run(url, nullptr, ""); });
}

std::future<Response> post_json(const std::string& url, const std::string& body, const std::string& key) {
    if (!enabled()) {
        std::promise<Response> p;
        p.set_value({});
        return p.get_future();
    }
    return std::async(std::launch::async, [url, body, key] { return run(url, &body, key); });
}


std::future<Response> download(const std::string& url, const std::string& dest_path) {
    if (!enabled()) {
        std::promise<Response> p;
        p.set_value({});
        return p.get_future();
    }
    return std::async(std::launch::async,
                      [url, dest_path] { return run_download(url, dest_path); });
}

}  // namespace net
