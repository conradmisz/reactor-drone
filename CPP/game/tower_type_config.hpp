#ifndef TOWER_TYPE_CONFIG_HPP
#define TOWER_TYPE_CONFIG_HPP

#include <string>

/**
 * TowerTypeConfig — defines parameters for one tower type.
 * Parsed from GameData.json "game.tower_types" array.
 * Keyed by type string ("cannon", "laser", etc.).
 */
struct TowerTypeConfig {
    std::string type;            // "cannon" or "laser"
    int cost = 50;              // gold cost to place
    float range = 150.0f;      // attack radius in pixels
    float fire_rate = 1.0f;    // shots per second
    float damage = 25.0f;      // damage per shot
    float laser_duration = 0.0f; // laser-specific: beam duration (0 for non-laser)
    std::string sidecar;        // sprite-sheet sidecar path (relative to assets); empty = Color only
    std::string clip = "idle";  // animation clip to play when placed
};

#endif // TOWER_TYPE_CONFIG_HPP
