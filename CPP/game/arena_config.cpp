#include "arena_config.hpp"
#include <fstream>
#include <stdexcept>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

GameConfig load_arena_config(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file) {
        throw std::runtime_error("load_arena_config: cannot open " + file_path);
    }
    json data;
    try {
        data = json::parse(file);
    } catch (const json::parse_error& e) {
        throw std::runtime_error(std::string("load_arena_config: JSON parse error: ") + e.what());
    }

    GameConfig cfg;

    if (data.contains("seed")) cfg.seed = data["seed"].get<unsigned int>();
    if (data.contains("victory_wave")) cfg.victory_wave = data["victory_wave"].get<int>();

    if (data.contains("arena")) {
        const auto& a = data["arena"];
        cfg.arena.radius       = a.value("radius", cfg.arena.radius);
        cfg.arena.spawn_radius = a.value("spawn_radius", cfg.arena.spawn_radius);
        cfg.arena.center_x     = a.value("center_x", cfg.arena.center_x);
        cfg.arena.center_y     = a.value("center_y", cfg.arena.center_y);
        cfg.arena.backdrop     = a.value("backdrop", std::string());
    }

    if (data.contains("player")) {
        const auto& p = data["player"];
        cfg.player.start_health = p.value("start_health", cfg.player.start_health);
        cfg.player.move_speed   = p.value("move_speed", cfg.player.move_speed);
        cfg.player.invuln_window = p.value("invuln_window", cfg.player.invuln_window);
        cfg.player.start_x      = p.value("start_x", cfg.player.start_x);
        cfg.player.start_y      = p.value("start_y", cfg.player.start_y);
        cfg.player.size         = p.value("size", cfg.player.size);
        cfg.player.sidecar      = p.value("sidecar", std::string());
        cfg.player.idle_clip    = p.value("idle_clip", cfg.player.idle_clip);
        if (p.contains("weapon")) {
            const auto& w = p["weapon"];
            cfg.player.weapon.fire_rate           = w.value("fire_rate", cfg.player.weapon.fire_rate);
            cfg.player.weapon.damage              = w.value("damage", cfg.player.weapon.damage);
            cfg.player.weapon.projectile_speed    = w.value("projectile_speed", cfg.player.weapon.projectile_speed);
            cfg.player.weapon.projectile_lifetime = w.value("projectile_lifetime", cfg.player.weapon.projectile_lifetime);
            cfg.player.weapon.spread              = w.value("spread", cfg.player.weapon.spread);
        }
    }

    if (data.contains("enemy_types")) {
        for (const auto& e : data["enemy_types"]) {
            EnemyType t;
            t.name           = e.value("name", t.name);
            t.sidecar        = e.value("sidecar", std::string());
            t.clip           = e.value("clip", t.clip);
            t.speed          = e.value("speed", t.speed);
            t.health         = e.value("health", t.health);
            t.contact_damage = e.value("contact_damage", t.contact_damage);
            t.size           = e.value("size", t.size);
            t.score          = e.value("score", t.score);
            t.xp             = e.value("xp", t.xp);
            cfg.enemy_types.push_back(std::move(t));
        }
    }

    if (data.contains("waves")) {
        for (const auto& w : data["waves"]) {
            WaveDef wd;
            wd.count          = w.value("count", wd.count);
            wd.spawn_interval = w.value("spawn_interval", wd.spawn_interval);
            wd.delay          = w.value("delay", wd.delay);
            if (w.contains("types")) wd.types = w["types"].get<std::vector<int>>();
            cfg.waves.push_back(std::move(wd));
        }
    }

    if (data.contains("xp_curve")) {
        const auto& x = data["xp_curve"];
        cfg.xp_level2     = x.value("level2", cfg.xp_level2);
        cfg.xp_multiplier = x.value("multiplier", cfg.xp_multiplier);
    }

    if (data.contains("feedback")) {
        const auto& f = data["feedback"];
        auto& fb = cfg.feedback;
        fb.max_shake_px        = f.value("max_shake_px", fb.max_shake_px);
        fb.trauma_decay_per_sec = f.value("trauma_decay_per_sec", fb.trauma_decay_per_sec);
        fb.trauma_player_hit   = f.value("trauma_player_hit", fb.trauma_player_hit);
        fb.trauma_enemy_death  = f.value("trauma_enemy_death", fb.trauma_enemy_death);
        fb.flash_duration      = f.value("flash_duration", fb.flash_duration);
        auto u8 = [](const json& j, const char* k, uint8_t d) -> uint8_t {
            return static_cast<uint8_t>(j.value(k, static_cast<int>(d)));
        };
        if (f.contains("player_flash")) {
            const auto& c = f["player_flash"];
            fb.player_flash_r = u8(c, "r", fb.player_flash_r);
            fb.player_flash_g = u8(c, "g", fb.player_flash_g);
            fb.player_flash_b = u8(c, "b", fb.player_flash_b);
        }
        if (f.contains("enemy_flash")) {
            const auto& c = f["enemy_flash"];
            fb.enemy_flash_r = u8(c, "r", fb.enemy_flash_r);
            fb.enemy_flash_g = u8(c, "g", fb.enemy_flash_g);
            fb.enemy_flash_b = u8(c, "b", fb.enemy_flash_b);
        }
    }

    if (data.contains("upgrades")) {
        for (const auto& u : data["upgrades"]) {
            Upgrade up;
            up.stat   = u.value("stat", std::string());
            up.amount = u.value("amount", 0.0f);
            up.weight = u.value("weight", 1.0f);
            up.label  = u.value("label", up.stat);
            cfg.upgrades.push_back(std::move(up));
        }
    }

    return cfg;
}
