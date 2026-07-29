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

    auto parse_backdrop_layers = [](const json& node) {
        std::vector<BackdropLayer> layers;
        if (node.contains("backdrop_layers")) {
            for (const auto& l : node["backdrop_layers"]) {
                BackdropLayer bl;
                bl.image         = l.value("image", std::string());
                bl.scroll_factor = l.value("scroll_factor", bl.scroll_factor);
                if (!bl.image.empty()) layers.push_back(std::move(bl));
            }
        }
        return layers;
    };

    if (data.contains("seed")) cfg.seed = data["seed"].get<unsigned int>();
    if (data.contains("victory_wave")) cfg.victory_wave = data["victory_wave"].get<int>();
    cfg.wave_stall_timeout = data.value("wave_stall_timeout", cfg.wave_stall_timeout);

    if (data.contains("arena")) {
        const auto& a = data["arena"];
        cfg.arena.radius       = a.value("radius", cfg.arena.radius);
        cfg.arena.spawn_radius = a.value("spawn_radius", cfg.arena.spawn_radius);
        cfg.arena.center_x     = a.value("center_x", cfg.arena.center_x);
        cfg.arena.center_y     = a.value("center_y", cfg.arena.center_y);
        cfg.arena.backdrop     = a.value("backdrop", std::string());
        cfg.arena.backdrop_layers = parse_backdrop_layers(a);
    }

    if (data.contains("arenas")) {
        for (const auto& a : data["arenas"]) {
            ArenaDef def;
            def.name       = a.value("name", std::string());
            def.first_wave = a.value("first_wave", def.first_wave);
            def.wall_image     = a.value("wall_image", std::string());
            def.obstacle_image = a.value("obstacle_image", std::string());
            def.hazard_image   = a.value("hazard_image", std::string());
            def.backdrop_layers = parse_backdrop_layers(a);
            if (a.contains("obstacles")) {
                for (const auto& o : a["obstacles"]) {
                    ObstacleDef od;
                    od.x = o.value("x", od.x); od.y = o.value("y", od.y);
                    od.w = o.value("w", od.w); od.h = o.value("h", od.h);
                    def.obstacles.push_back(od);
                }
            }
            if (a.contains("hazards")) {
                for (const auto& h : a["hazards"]) {
                    HazardDef hd;
                    hd.x = h.value("x", hd.x); hd.y = h.value("y", hd.y);
                    hd.w = h.value("w", hd.w); hd.h = h.value("h", hd.h);
                    hd.damage = h.value("damage", hd.damage);
                    def.hazards.push_back(hd);
                }
            }
            cfg.arenas.push_back(std::move(def));
        }
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
            t.currency       = e.value("currency", t.currency);
            cfg.enemy_types.push_back(std::move(t));
        }
    }

    if (data.contains("waves")) {
        for (const auto& w : data["waves"]) {
            WaveDef wd;
            wd.count          = w.value("count", wd.count);
            wd.spawn_interval = w.value("spawn_interval", wd.spawn_interval);
            wd.delay          = w.value("delay", wd.delay);
            wd.duration       = w.value("duration", wd.duration);
            wd.hp_mult        = w.value("hp_mult", wd.hp_mult);
            wd.speed_mult     = w.value("speed_mult", wd.speed_mult);
            if (w.contains("types")) wd.types = w["types"].get<std::vector<int>>();
            cfg.waves.push_back(std::move(wd));
        }
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

    if (data.contains("pathfinding")) {
        const auto& p = data["pathfinding"];
        cfg.pathfinding.repath_interval = p.value("repath_interval", cfg.pathfinding.repath_interval);
        cfg.pathfinding.cell_size       = p.value("cell_size", cfg.pathfinding.cell_size);
        cfg.pathfinding.clearance       = p.value("clearance", cfg.pathfinding.clearance);
    }

    if (data.contains("economy")) {
        const auto& e = data["economy"];
        EconomyConfig& ec = cfg.economy;
        ec.min_drops            = e.value("min_drops", ec.min_drops);
        ec.max_drops            = e.value("max_drops", ec.max_drops);
        ec.key_drop_chance      = e.value("key_drop_chance", ec.key_drop_chance);
        ec.pickup_lifetime      = e.value("pickup_lifetime", ec.pickup_lifetime);
        ec.pickup_size          = e.value("pickup_size", ec.pickup_size);
        ec.pickup_scatter       = e.value("pickup_scatter", ec.pickup_scatter);
        ec.pickup_magnet_speed  = e.value("pickup_magnet_speed", ec.pickup_magnet_speed);
        ec.pickup_magnet_radius = e.value("pickup_magnet_radius", ec.pickup_magnet_radius);
    }

    if (data.contains("shop")) {
        const auto& s = data["shop"];
        ShopConfig& sc = cfg.shop;
        sc.price_growth       = s.value("price_growth", sc.price_growth);
        sc.shield_regen_delay = s.value("shield_regen_delay", sc.shield_regen_delay);
        sc.repulsor_radius    = s.value("repulsor_radius", sc.repulsor_radius);

        auto read_rows = [&](const char* key, std::vector<ShopUpgradeDef>& out) {
            if (!s.contains(key)) return;
            for (const auto& u : s[key]) {
                ShopUpgradeDef d;
                d.name       = u.value("name", d.name);
                d.effect     = u.value("effect", d.effect);
                d.price      = u.value("price", d.price);
                d.amount     = u.value("amount", d.amount);
                d.max_stacks = u.value("max_stacks", d.max_stacks);
                d.duration   = u.value("duration", d.duration);
                out.push_back(std::move(d));
            }
        };
        read_rows("upgrades", sc.upgrades);
        read_rows("items", sc.items);
        read_rows("consumables", sc.consumables);

        // upg_counts is 8 wide; extra rows would have nowhere to record a purchase.
        if (sc.upgrades.size() > 8) sc.upgrades.resize(8);
        // Items + consumables share the 1-8 keys on the shop's gear page.
        if (sc.items.size() > 8) sc.items.resize(8);
        if (sc.items.size() + sc.consumables.size() > 8)
            sc.consumables.resize(8 - sc.items.size());
    }

    return cfg;
}
