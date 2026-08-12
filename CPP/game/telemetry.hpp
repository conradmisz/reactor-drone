#ifndef TELEMETRY_HPP
#define TELEMETRY_HPP

#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

/**
 * telemetry — the per-run summary report (specs/telemetry.md).
 *
 * A plain struct owned by main.cpp scope, deliberately outside the ECS
 * (Invariant 6) and write-only with respect to the simulation (Invariant 4):
 * filling it takes zero RNG draws and nothing in the sim ever reads it. One
 * report is serialized and POSTed once, at bank_run_score.
 */
namespace telemetry {

constexpr int HEAT_DIM = 32;  // 32x32 occupancy grid over the arena's bounding square

struct WaveStat {
    int wave = 0;             ///< 1-based, as the HUD counts
    float hp = 0.0f;          ///< hull when the wave opened
    float shield = 0.0f;
    int units = 0;            ///< currency held when the wave opened
    float seconds = 0.0f;     ///< sim time spent on this wave
    float damage_taken = 0.0f;///< sum of hull DECREASES during the wave
};

struct RunReport {
    // Envelope.
    int v = 1;
    std::string game_version, player_id, session_id, difficulty;
    std::string outcome = "close";   ///< death|victory|quit|close; close = window shut mid-run
    unsigned seed = 0;
    int ship = -1, prestige = 0;
    bool resumed = false;
    int wave = 0;
    long long score = 0;
    double dur_s = 0.0;              ///< sim seconds (PLAYING + INTERMISSION)

    // Sections.
    std::vector<WaveStat> waves;
    bool died = false;
    float death_x = 0.0f, death_y = 0.0f;
    int death_wave = 0;
    int death_bin = 0;               ///< heat_bin() of the death position
    std::string killed_by;           ///< "enemy:<kind>" | "shot" | "hazard" | ""
    std::map<int, std::array<uint8_t, HEAT_DIM * HEAT_DIM>> heat;  ///< arena idx -> grid
    long long earned = 0, spent = 0; ///< sums of currency deltas, split by sign
    int upg_counts[8] = {0};
    std::string item_equipped;
    std::map<std::string, int> consumables_used;
    std::map<std::string, int> ui;   ///< screen -> open count
    long long shots = 0, hits = 0, dashes = 0, bombs = 0;

    // Accumulator internals (not serialized).
    float last_hull = -1.0f;
    long long last_currency = -1;
    double sample_accum = 0.0;
};

/// Grid index for a world position, over the bounding square of the arena
/// circle (centre cx,cy radius r). Out-of-square positions clamp to the edge.
int heat_bin(float x, float y, float cx, float cy, float radius);

/// One sim frame of observation: duration, wave open/close, hull-decrease
/// accumulation, currency-delta split, and 4 Hz position binning. Pure
/// function of its arguments + the report — unit-tested with no ECS.
void frame_sample(RunReport& r, double dt, int wave, float hull, float shield,
                  long long currency, float px, float py, int arena_idx,
                  float cx, float cy, float radius);

/// RFC 4648 base64 (with padding).
std::string b64(const uint8_t* data, size_t n);

/// The POST body. Sections always present except `death` (only when died).
std::string serialize(const RunReport& r);

}  // namespace telemetry

#endif  // TELEMETRY_HPP
