#pragma once

// Live backend endpoint (Task 5) and its shared secret. Committed: the key
// ships in the binary either way, so gating it behind .gitignore buys
// nothing — see agentProjectDocs/specs/distribution-and-leaderboard.md §7.
namespace net {
constexpr const char* NET_BASE = "https://reactor-drone-api.conradmiszczak.workers.dev";
constexpr const char* NET_GAME_KEY = "c0e19e84ada0443742f7ab05de4f3b5f";
}  // namespace net
