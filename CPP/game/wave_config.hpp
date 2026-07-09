#ifndef WAVE_CONFIG_HPP
#define WAVE_CONFIG_HPP

#include <string>

/**
 * WaveConfig — defines parameters for one wave of enemies.
 * Parsed from GameData.json "game.waves" array.
 */
struct WaveConfig {
    int enemy_count = 5;
    float spawn_interval = 1.0f;   // seconds between spawns
    float wave_delay = 2.0f;       // seconds before wave starts
    float enemy_speed = 64.0f;     // pixels per second
    float enemy_hp = 100.0f;       // hit points per enemy
    std::string enemy_sidecar = "images/enemy_runner.json";  // sidecar path relative to assets
    std::string enemy_clip = "march";                         // animation clip to play on spawn
};

#endif // WAVE_CONFIG_HPP
