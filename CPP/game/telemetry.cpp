#include "telemetry.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

namespace telemetry {

int heat_bin(float x, float y, float cx, float cy, float radius) {
    const float side = 2.0f * radius;
    int gx = static_cast<int>((x - (cx - radius)) / side * HEAT_DIM);
    int gy = static_cast<int>((y - (cy - radius)) / side * HEAT_DIM);
    gx = std::clamp(gx, 0, HEAT_DIM - 1);
    gy = std::clamp(gy, 0, HEAT_DIM - 1);
    return gy * HEAT_DIM + gx;
}

void frame_sample(RunReport& r, double dt, int wave, float hull, float shield,
                  long long currency, float px, float py, int arena_idx,
                  float cx, float cy, float radius) {
    r.dur_s += dt;

    // Wave watcher: a change of the HUD's 1-based wave number opens a stat row
    // carrying the player's state at that moment.
    if (wave > 0 && (r.waves.empty() || r.waves.back().wave != wave)) {
        WaveStat w;
        w.wave = wave; w.hp = hull; w.shield = shield;
        w.units = static_cast<int>(currency);
        r.waves.push_back(w);
    }
    if (!r.waves.empty()) {
        r.waves.back().seconds += static_cast<float>(dt);
        // Hull decreases are damage; increases (shop hull upgrades) are not.
        if (r.last_hull >= 0.0f && hull < r.last_hull)
            r.waves.back().damage_taken += r.last_hull - hull;
    }
    r.last_hull = hull;

    if (r.last_currency >= 0) {
        const long long d = currency - r.last_currency;
        if (d > 0) r.earned += d; else r.spent -= d;
    }
    r.last_currency = currency;

    // 4 Hz occupancy sampling. Saturating u8 bins: 255 caps a ~64 s camp in
    // one cell per run, which is plenty of signal for a global heatmap.
    r.sample_accum += dt;
    while (r.sample_accum >= 0.25) {
        r.sample_accum -= 0.25;
        auto& grid = r.heat[arena_idx];           // zero-initialised std::array
        uint8_t& cell = grid[static_cast<size_t>(heat_bin(px, py, cx, cy, radius))];
        if (cell < 255) ++cell;
    }
}

std::string b64(const uint8_t* data, size_t n) {
    static const char* T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((n + 2) / 3 * 4);
    for (size_t i = 0; i < n; i += 3) {
        const uint32_t a = data[i];
        const uint32_t b = i + 1 < n ? data[i + 1] : 0;
        const uint32_t c = i + 2 < n ? data[i + 2] : 0;
        const uint32_t v = (a << 16) | (b << 8) | c;
        out.push_back(T[(v >> 18) & 63]);
        out.push_back(T[(v >> 12) & 63]);
        out.push_back(i + 1 < n ? T[(v >> 6) & 63] : '=');
        out.push_back(i + 2 < n ? T[v & 63] : '=');
    }
    return out;
}

std::string serialize(const RunReport& r) {
    nlohmann::json j{
        {"v", r.v},
        {"game_version", r.game_version},
        {"player_id", r.player_id},
        {"session_id", r.session_id},
        {"difficulty", r.difficulty},
        {"outcome", r.outcome},
        {"seed", r.seed},
        {"ship", r.ship},
        {"prestige", r.prestige},
        {"resumed", r.resumed},
        {"wave", r.wave},
        {"score", r.score},
        {"dur_s", r.dur_s},
    };
    j["waves"] = nlohmann::json::array();
    for (const WaveStat& w : r.waves)
        j["waves"].push_back({{"wave", w.wave}, {"hp", w.hp}, {"shield", w.shield},
                              {"units", w.units}, {"seconds", w.seconds},
                              {"damage_taken", w.damage_taken}});
    if (r.died)
        j["death"] = {{"x", r.death_x}, {"y", r.death_y},
                      {"wave", r.death_wave}, {"killed_by", r.killed_by},
                      {"bin", r.death_bin}};
    j["heat"] = nlohmann::json::object();
    for (const auto& [arena, grid] : r.heat)
        j["heat"][std::to_string(arena)] = b64(grid.data(), grid.size());
    j["econ"] = {{"earned", r.earned}, {"spent", r.spent},
                 {"upg_counts", r.upg_counts}, {"item", r.item_equipped},
                 {"consumables_used", r.consumables_used}};
    j["ui"] = r.ui;
    j["combat"] = {{"shots", r.shots}, {"hits", r.hits},
                   {"dashes", r.dashes}, {"bombs", r.bombs}};
    return j.dump();
}

}  // namespace telemetry
