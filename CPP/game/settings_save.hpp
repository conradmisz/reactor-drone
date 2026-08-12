#ifndef SETTINGS_SAVE_HPP
#define SETTINGS_SAVE_HPP

#include <filesystem>
#include <fstream>
#include <string>

#include <nlohmann/json.hpp>

#include "engine/project_paths.hpp"

/**
 * SettingsSave — the options screen's three toggles (main-menu-suite Phase C,
 * plus telemetry consent).
 *
 * `saves/settings.json`, third file in the saves directory and a different
 * concern from both meta.json (lifetime progression) and runN.json (runs in
 * progress). Same failure discipline as meta_save (D80): a missing, corrupt or
 * hand-mangled file yields the defaults, and the defaults are exactly the
 * pre-settings behaviour — so the replay canary and every headless script run
 * identically whether or not the file exists.
 *
 * Presentation-only by construction: the flags are published to the Blackboard
 * (settings.screen_shake / settings.minimap) and consumed at the two apply
 * sites, which are gated so the simulation's RNG draw sequence is identical in
 * every combination (see the shake block in main.cpp).
 */
struct SettingsSave {
    bool screen_shake = true;
    bool minimap = true;
    /// Telemetry consent (specs/telemetry.md §7). On by default, disclosed on the
    /// first-launch name screen. Read only by main.cpp's POST guard — deliberately
    /// NOT published to the Blackboard, because it has no apply site in the sim.
    bool analytics = true;
};

inline std::string settings_save_path() {
    return project_paths::user_data_dir() + "/saves/settings.json";
}

inline SettingsSave settings_load(const std::string& path) {
    SettingsSave s;
    std::ifstream in(path);
    if (!in.is_open()) return s;
    try {
        nlohmann::json j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
        if (!j.is_object()) return s;
        if (j.contains("screen_shake") && j["screen_shake"].is_boolean())
            s.screen_shake = j["screen_shake"].get<bool>();
        if (j.contains("minimap") && j["minimap"].is_boolean())
            s.minimap = j["minimap"].get<bool>();
        if (j.contains("analytics") && j["analytics"].is_boolean())
            s.analytics = j["analytics"].get<bool>();
    } catch (...) {
        return SettingsSave{};
    }
    return s;
}

inline bool settings_write(const std::string& path, const SettingsSave& s) {
    try {
        std::filesystem::create_directories(std::filesystem::path(path).parent_path());
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) return false;
        out << nlohmann::json{{"screen_shake", s.screen_shake},
                              {"minimap", s.minimap},
                              {"analytics", s.analytics}}.dump(2) << "\n";
        return out.good();
    } catch (...) {
        return false;   // a read-only disk must not take the settings screen down
    }
}

#endif  // SETTINGS_SAVE_HPP
