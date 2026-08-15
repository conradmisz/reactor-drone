/**
 * update_check.hpp — the pure half of the in-game updater.
 *
 * Two decisions live here, both of which must be right before the game offers
 * to download and RUN a binary, and both of which are testable without a
 * network: is the server's version actually newer, and is its installer URL one
 * we are willing to execute.
 *
 * Everything here fails CLOSED. An unparseable version is "not newer" and an
 * unrecognised URL is "not trusted", because the failure mode on the other side
 * is prompting a player to install an unknown executable.
 */
#pragma once

#include <string>
#include <vector>

namespace update_check {

/// The only prefix an installer may come from. The Worker hands us this URL
/// from its own config, so this is defence against a compromised or
/// misconfigured Worker — not a substitute for HTTPS, but the reason a wrong
/// answer there cannot become "run this exe".
inline const char* kTrustedPrefix =
    "https://github.com/conradmisz/reactor-drone/releases/";

/// Split "2.1.0" into {2,1,0}. Empty on ANY non-digit, non-dot character, which
/// is what makes `is_newer` reject "banana" and "v2.1.0" rather than guess.
inline std::vector<int> parse_version(const std::string& v) {
    std::vector<int> out;
    if (v.empty()) return out;
    int cur = 0;
    bool digits = false;
    for (char c : v) {
        if (c >= '0' && c <= '9') {
            cur = cur * 10 + (c - '0');
            digits = true;
        } else if (c == '.') {
            if (!digits) return {};        // ".5" / ".." — malformed
            out.push_back(cur);
            cur = 0;
            digits = false;
        } else {
            return {};                     // any other character: refuse to guess
        }
    }
    if (!digits) return {};                // trailing dot
    out.push_back(cur);
    return out;
}

/// True when `remote` is a strictly higher version than `local`. Missing parts
/// count as 0, so "2.1" > "2.0.5" and "2.0" == "2.0.0".
inline bool is_newer(const std::string& remote, const std::string& local) {
    const std::vector<int> r = parse_version(remote);
    const std::vector<int> l = parse_version(local);
    if (r.empty() || l.empty()) return false;
    const std::size_t n = r.size() > l.size() ? r.size() : l.size();
    for (std::size_t i = 0; i < n; ++i) {
        const int a = i < r.size() ? r[i] : 0;
        const int b = i < l.size() ? l[i] : 0;
        if (a != b) return a > b;
    }
    return false;
}

/**
 * True only for an HTTPS URL under this project's own releases path.
 *
 * The prefix must be at position 0 — a URL that merely CONTAINS it (a redirect
 * parameter, say) is rejected — and the character right after the prefix may
 * not start a new authority, which is what stops
 * `https://github.com/conradmisz/reactor-drone@evil.example/x.exe` from
 * reading as trusted when its real host is evil.example.
 */
inline bool trusted_installer_url(const std::string& url) {
    const std::string prefix = kTrustedPrefix;
    if (url.size() <= prefix.size()) return false;
    if (url.compare(0, prefix.size(), prefix) != 0) return false;
    // No credentials/host trickery anywhere in the authority we just matched.
    if (url.find('@') != std::string::npos) return false;
    if (url.find("..") != std::string::npos) return false;
    return true;
}

}  // namespace update_check
