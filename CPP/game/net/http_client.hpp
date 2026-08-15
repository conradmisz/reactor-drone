#pragma once
#include <future>
#include <string>

// Async libcurl HTTP client. Disabled (net::enabled() == false) in headless /
// scripted runs so replays and tests make zero network calls — see
// net::set_enabled() call in main.cpp right after CLI parsing.
namespace net {

struct Response {
    long status = 0;
    std::string body;
    bool ok() const { return status >= 200 && status < 300; }
};

// Fire-and-poll: returns a future immediately; never blocks the game loop —
// but only if the caller stores it and polls (e.g. wait_for(0ms) each frame).
// Discarding the returned future as a temporary blocks in ITS destructor for
// up to the 8s CURLOPT_TIMEOUT. On shutdown, drain or keep alive any
// in-flight future rather than letting it fall out of scope.
std::future<Response> get(const std::string& url);
std::future<Response> post_json(const std::string& url, const std::string& json_body,
                                 const std::string& game_key = "");  // sets X-Game-Key when non-empty

// Download a URL straight to `dest_path` (the in-game updater's installer
// fetch). Same fire-and-poll contract as get(): store the future and poll it.
// An installer is tens of MB, so this one gets no 8s timeout — it gets a
// low-speed abort instead, which kills a stalled transfer without killing a
// slow-but-progressing one. Writes to a temp name and renames on success, so a
// half-downloaded file is never left at dest_path for the caller to execute.
std::future<Response> download(const std::string& url, const std::string& dest_path);

bool enabled();           // false in headless mode -> get/post return status 0 instantly
void set_enabled(bool on);

}  // namespace net
