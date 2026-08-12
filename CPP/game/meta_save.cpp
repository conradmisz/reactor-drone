#include "meta_save.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>

#include <nlohmann/json.hpp>

#include "engine/project_paths.hpp"
#include "prestige.hpp"

std::string generate_uuid() {
    std::random_device rd;
    unsigned char bytes[16];
    for (unsigned char& b : bytes) b = static_cast<unsigned char>(rd() & 0xFF);
    char buf[37];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                  bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
                  bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]);
    return std::string(buf);
}

std::string meta_save_path() {
    return project_paths::user_data_dir() + "/saves/meta.json";
}

MetaSave meta_load(const std::string& path) {
    MetaSave m;
    std::ifstream in(path);
    if (!in.is_open()) return m;  // no save yet — the common case on a first run
    try {
        // Non-throwing parse plus value(): a truncated, empty or hand-edited file
        // falls back to the defaults instead of taking the run down with it.
        nlohmann::json j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
        if (j.is_object()) {
            m.lifetime_score = j.value("lifetime_score", 0LL);
            m.prestige       = j.value("prestige", 0);
            m.best_wave      = std::max(0, j.value("best_wave", 0));
            m.runs_played    = std::max(0LL, j.value("runs_played", 0LL));
            m.player_id      = j.value("player_id", std::string());
            m.player_name    = j.value("player_name", std::string());
            m.registered     = j.value("registered", false);
        }
    } catch (...) {
        return MetaSave{};
    }
    if (m.lifetime_score < 0) m.lifetime_score = 0;
    // Lane O: a hand-edited 99 loads as 5, and a negative as 0 — a save file must
    // never be able to hand out stats the game does not offer.
    m.prestige = prestige_clamp(m.prestige);
    return m;
}

bool meta_write(const std::string& path, const MetaSave& m) {
    try {
        const std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) return false;
        out << nlohmann::json{{"lifetime_score", m.lifetime_score},
                              {"prestige", m.prestige},
                              {"best_wave", m.best_wave},
                              {"runs_played", m.runs_played},
                              {"player_id", m.player_id},
                              {"player_name", m.player_name},
                              {"registered", m.registered}}.dump(2) << "\n";
        return out.good();
    } catch (...) {
        return false;  // ponytail: a lost save is a lost unlock, never a crashed game
    }
}
