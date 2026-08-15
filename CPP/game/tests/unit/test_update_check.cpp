// Unit tests for update_check.hpp — the pure half of the in-game updater:
// deciding whether the server's version is newer, and whether its installer URL
// is one we are willing to download and EXECUTE. No SDL, no curl, no network.

#include <catch2/catch_test_macros.hpp>

#include "game/update_check.hpp"

using update_check::is_newer;
using update_check::trusted_installer_url;

TEST_CASE("is_newer compares version parts numerically", "[update]") {
    CHECK(is_newer("2.1.0", "2.0.0"));
    CHECK(is_newer("3.0.0", "2.9.9"));
    CHECK(is_newer("2.0.1", "2.0.0"));
    CHECK_FALSE(is_newer("2.0.0", "2.1.0"));
    CHECK_FALSE(is_newer("2.0.0", "2.0.0"));
    // Lexicographic comparison would call 2.0.10 older than 2.0.9. It is not.
    CHECK(is_newer("2.0.10", "2.0.9"));
    CHECK_FALSE(is_newer("2.0.9", "2.0.10"));
}

TEST_CASE("is_newer tolerates short and padded version strings", "[update]") {
    CHECK(is_newer("2.1", "2.0.5"));
    CHECK_FALSE(is_newer("2.0", "2.0.0"));
    CHECK(is_newer("2.0.0.1", "2.0.0"));
}

TEST_CASE("is_newer fails CLOSED on anything it cannot parse", "[update]") {
    // An unreadable answer must never prompt a player to install something.
    CHECK_FALSE(is_newer("", "2.0.0"));
    CHECK_FALSE(is_newer("banana", "2.0.0"));
    CHECK_FALSE(is_newer("2.0.0", ""));
    CHECK_FALSE(is_newer("v2.1.0", "2.0.0"));   // tags are stripped by the caller, not here
}

TEST_CASE("trusted_installer_url accepts only the project's own HTTPS releases",
          "[update]") {
    CHECK(trusted_installer_url(
        "https://github.com/conradmisz/reactor-drone/releases/download/v2.1.0/ReactorDrone-Setup-2.1.0.exe"));
}

TEST_CASE("trusted_installer_url rejects anything else", "[update]") {
    // Plaintext: a downgrade to http is a downgrade to "anyone on the wire
    // chooses the binary".
    CHECK_FALSE(trusted_installer_url(
        "http://github.com/conradmisz/reactor-drone/releases/download/v2.1.0/x.exe"));
    // Another host entirely.
    CHECK_FALSE(trusted_installer_url("https://evil.example/x.exe"));
    // Another user's repo on the right host.
    CHECK_FALSE(trusted_installer_url(
        "https://github.com/someoneelse/reactor-drone/releases/download/v1/x.exe"));
    // The prefix appears, but not at the start.
    CHECK_FALSE(trusted_installer_url(
        "https://evil.example/?u=https://github.com/conradmisz/reactor-drone/releases/x.exe"));
    // Userinfo trick: the real host is evil.example.
    CHECK_FALSE(trusted_installer_url(
        "https://github.com/conradmisz/reactor-drone@evil.example/x.exe"));
    CHECK_FALSE(trusted_installer_url(""));
}
