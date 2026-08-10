#include "meta_save.hpp"

#include <filesystem>
#include <fstream>

#include <nlohmann/json.hpp>

#include "engine/project_paths.hpp"
#include "prestige.hpp"

std::string meta_save_path() {
    return project_paths::class_root() + "/saves/meta.json";
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
                              {"prestige", m.prestige}}.dump(2) << "\n";
        return out.good();
    } catch (...) {
        return false;  // ponytail: a lost save is a lost unlock, never a crashed game
    }
}
