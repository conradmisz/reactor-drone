#include "arena_config.hpp"
#include <algorithm>
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

    // Read a 0-255 JSON int as a uint8_t. Shared by the per-arena enemy tint and
    // the feedback flash colours.
    auto u8 = [](const json& j, const char* k, uint8_t d) -> uint8_t {
        return static_cast<uint8_t>(j.value(k, static_cast<int>(d)));
    };

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
            def.tie_dye    = a.value("tie_dye", def.tie_dye);
            if (a.contains("enemy_tint")) {
                const auto& t = a["enemy_tint"];
                def.enemy_r = u8(t, "r", def.enemy_r);
                def.enemy_g = u8(t, "g", def.enemy_g);
                def.enemy_b = u8(t, "b", def.enemy_b);
            }
            def.wall_image     = a.value("wall_image", std::string());
            def.obstacle_image = a.value("obstacle_image", std::string());
            def.hazard_image   = a.value("hazard_image", std::string());
            def.backdrop_layers = parse_backdrop_layers(a);
            if (a.contains("obstacles")) {
                for (const auto& o : a["obstacles"]) {
                    ObstacleDef od;
                    od.x = o.value("x", od.x); od.y = o.value("y", od.y);
                    od.w = o.value("w", od.w); od.h = o.value("h", od.h);
                    od.hp = o.value("hp", od.hp);   // engine-suite D138: 0 = indestructible
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
            // Iteration 3 (D51): the theme's specialty unit and its escalation
            // step on the second pass over the same four themes.
            def.specialty_unit = a.value("specialty_unit", def.specialty_unit);
            def.specialty_tier = a.value("specialty_tier", def.specialty_tier);
            // Engine-suite Phase 0 (D138): the arena's surge table (#7, Lane X).
            if (a.contains("surges")) {
                for (const auto& sg : a["surges"]) {
                    SurgeDef sd;
                    sd.effect     = sg.value("effect", sd.effect);
                    sd.first_wave = sg.value("first_wave", sd.first_wave);
                    sd.last_wave  = sg.value("last_wave", sd.last_wave);
                    sd.chance     = sg.value("chance", sd.chance);
                    sd.magnitude  = sg.value("magnitude", sd.magnitude);
                    sd.duration   = sg.value("duration", sd.duration);
                    sd.radius     = sg.value("radius", sd.radius);
                    sd.telegraph  = sg.value("telegraph", sd.telegraph);
                    def.surges.push_back(std::move(sd));
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

    // Lane F (D82): selectable ships. Each entry defaults to the player block it
    // overlays, so a ship that only retunes the weapon authors only the weapon.
    if (data.contains("ships")) {
        for (const auto& s : data["ships"]) {
            ShipDef ship;
            ship.name         = s.value("name", ship.name);
            ship.sidecar      = s.value("sidecar", std::string());
            ship.idle_clip    = s.value("idle_clip", std::string());
            ship.unlock_score = s.value("unlock_score", ship.unlock_score);
            if (s.contains("color") && s["color"].is_array() && s["color"].size() >= 3) {
                ship.color_r = static_cast<uint8_t>(s["color"][0].get<int>());
                ship.color_g = static_cast<uint8_t>(s["color"][1].get<int>());
                ship.color_b = static_cast<uint8_t>(s["color"][2].get<int>());
            }
            ship.weapon       = cfg.player.weapon;
            if (s.contains("weapon")) {
                const auto& w = s["weapon"];
                ship.weapon.fire_rate           = w.value("fire_rate", ship.weapon.fire_rate);
                ship.weapon.damage              = w.value("damage", ship.weapon.damage);
                ship.weapon.projectile_speed    = w.value("projectile_speed", ship.weapon.projectile_speed);
                ship.weapon.projectile_lifetime = w.value("projectile_lifetime", ship.weapon.projectile_lifetime);
                ship.weapon.spread              = w.value("spread", ship.weapon.spread);
            }
            cfg.ships.push_back(ship);
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
            t.drop_chance    = e.value("drop_chance", t.drop_chance);
            // Iteration 3 (D51): non-seeker behaviour. An absent block leaves
            // `behavior` empty, which is SEEKER — every type shipped so far.
            t.behavior       = e.value("behavior", t.behavior);
            t.behavior_tier  = e.value("behavior_tier", t.behavior_tier);
            t.fire_interval  = e.value("fire_interval", t.fire_interval);
            t.shot_speed     = e.value("shot_speed", t.shot_speed);
            t.shot_damage    = e.value("shot_damage", t.shot_damage);
            t.first_wave     = e.value("first_wave", t.first_wave);
            t.pattern        = e.value("pattern", t.pattern);   // engine-suite D138 (#2)
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
            wd.boss           = w.value("boss", wd.boss);   // Iteration 3 (D51)
            if (w.contains("types")) wd.types = w["types"].get<std::vector<int>>();
            cfg.waves.push_back(std::move(wd));
        }
    }

    // Iteration 3 (D51): the four scaffolded blocks. Every one is optional and
    // every default is inert, so a data file without them is the pre-iteration-3
    // game. The lane that owns each block fills it in; the parse lives here so
    // no lane has to edit this file while another one is.
    if (data.contains("sustain")) {
        const auto& s = data["sustain"];
        auto& sc = cfg.sustain;
        sc.interval        = s.value("interval", sc.interval);
        sc.max_live        = s.value("max_live", sc.max_live);
        sc.health_amount   = s.value("health_amount", sc.health_amount);
        sc.shield_amount   = s.value("shield_amount", sc.shield_amount);
        sc.shield_weight   = s.value("shield_weight", sc.shield_weight);
        sc.min_player_dist = s.value("min_player_dist", sc.min_player_dist);
    }
    if (data.contains("dash")) {
        const auto& d = data["dash"];
        cfg.dash.speed    = d.value("speed", cfg.dash.speed);
        cfg.dash.duration = d.value("duration", cfg.dash.duration);
        cfg.dash.cooldown = d.value("cooldown", cfg.dash.cooldown);
        cfg.dash.damage   = d.value("damage", cfg.dash.damage);
        cfg.dash.charges  = std::max(1, d.value("charges", cfg.dash.charges));
    }

    if (data.contains("battery")) {
        const auto& b = data["battery"];
        cfg.battery.fire_time     = b.value("fire_time", cfg.battery.fire_time);
        cfg.battery.recharge_time = b.value("recharge_time", cfg.battery.recharge_time);
    }
    if (data.contains("minimap")) {
        const auto& m = data["minimap"];
        cfg.minimap.enabled   = m.value("enabled", cfg.minimap.enabled);
        cfg.minimap.x         = m.value("x", cfg.minimap.x);
        cfg.minimap.y         = m.value("y", cfg.minimap.y);
        cfg.minimap.size      = m.value("size", cfg.minimap.size);
        cfg.minimap.max_blips = m.value("max_blips", cfg.minimap.max_blips);
    }
    if (data.contains("boss")) {
        const auto& b = data["boss"];
        auto& bc = cfg.boss;
        bc.health          = b.value("health", bc.health);
        bc.health_growth   = b.value("health_growth", bc.health_growth);
        bc.size            = b.value("size", bc.size);
        bc.contact_damage  = b.value("contact_damage", bc.contact_damage);
        bc.summon_interval = b.value("summon_interval", bc.summon_interval);
        bc.summon_count    = b.value("summon_count", bc.summon_count);
        bc.reward_choices  = b.value("reward_choices", bc.reward_choices);
        bc.final_mult         = b.value("final_mult", bc.final_mult);
        bc.final_summon_bonus = b.value("final_summon_bonus", bc.final_summon_bonus);
        bc.shift_hp_frac      = b.value("shift_hp_frac", bc.shift_hp_frac);
    }
    // Iteration 3 (D67): the specialty/moon injection cadences, plus the arena ->
    // specialty-unit map. Resolved to an enemy_types index here, once, so no
    // system has to carry a name lookup — and so a typo is one silent -1 rather
    // than a per-frame string compare.
    if (data.contains("specialty")) {
        const auto& sp = data["specialty"];
        auto& sc = cfg.specialty;
        sc.every_n_spawns      = sp.value("every_n_spawns", sc.every_n_spawns);
        sc.moon_every_n_spawns = sp.value("moon_every_n_spawns", sc.moon_every_n_spawns);
        sc.tier2_hp_mult       = sp.value("tier2_hp_mult", sc.tier2_hp_mult);
        sc.tier2_speed_mult    = sp.value("tier2_speed_mult", sc.tier2_speed_mult);
        if (sp.contains("by_arena")) {
            for (const auto& p : sp["by_arena"]) {
                SpecialtyPick pick;
                pick.arena = p.value("arena", std::string());
                pick.type  = p.value("type", std::string());
                sc.by_arena.push_back(std::move(pick));
            }
        }
        for (ArenaDef& a : cfg.arenas) {
            for (const SpecialtyPick& p : sc.by_arena) {
                if (p.arena != a.name) continue;
                for (size_t i = 0; i < cfg.enemy_types.size(); ++i) {
                    if (cfg.enemy_types[i].name == p.type) {
                        a.specialty_unit = static_cast<int>(i);
                        break;
                    }
                }
                break;
            }
        }
    }
    if (data.contains("actives")) {
        for (const auto& a : data["actives"]) {
            ActiveItemDef ad;
            ad.name     = a.value("name", ad.name);
            ad.effect   = a.value("effect", ad.effect);
            ad.cooldown = a.value("cooldown", ad.cooldown);
            ad.amount   = a.value("amount", ad.amount);
            ad.duration = a.value("duration", ad.duration);
            cfg.actives.push_back(std::move(ad));
        }
    }

    // === Engine-suite Phase 0 (D138). Every block is optional and every default
    // is inert; each is consumed only by the lane that owns it (see
    // specs/engine-feature-suite.md). Parsed with json.value so an older data
    // file still loads. ===
    if (data.contains("timescale")) {
        const auto& t = data["timescale"];
        auto& tc = cfg.timescale;
        tc.enabled      = t.value("enabled", tc.enabled);
        tc.kill_scale   = t.value("kill_scale", tc.kill_scale);
        tc.kill_hold    = t.value("kill_hold", tc.kill_hold);
        tc.chain_kills  = t.value("chain_kills", tc.chain_kills);
        tc.chain_window = t.value("chain_window", tc.chain_window);
        tc.hull_scale   = t.value("hull_scale", tc.hull_scale);
        tc.hull_frac    = t.value("hull_frac", tc.hull_frac);
        tc.ease_per_sec = t.value("ease_per_sec", tc.ease_per_sec);
        tc.min_scale    = t.value("min_scale", tc.min_scale);
    }
    if (data.contains("director")) {
        const auto& d = data["director"];
        auto& dc = cfg.director;
        dc.enabled       = d.value("enabled", dc.enabled);
        dc.min_mult      = d.value("min_mult", dc.min_mult);
        dc.max_mult      = d.value("max_mult", dc.max_mult);
        dc.damage_weight = d.value("damage_weight", dc.damage_weight);
        dc.kill_weight   = d.value("kill_weight", dc.kill_weight);
        dc.hull_weight   = d.value("hull_weight", dc.hull_weight);
        dc.ema_per_sec   = d.value("ema_per_sec", dc.ema_per_sec);
    }
    // "resonance", NOT "grid": the class-baseline gamedata_loader already claims a
    // top-level "grid" (the match-3 tile grid) and reads grid["rows"] UNGUARDED,
    // so a block of ours under that name aborts the loader before main() runs.
    if (data.contains("resonance")) {
        const auto& g = data["resonance"];
        auto& gc = cfg.resonance;
        gc.enabled       = g.value("enabled", gc.enabled);
        gc.spacing       = g.value("spacing", gc.spacing);
        gc.stiffness     = g.value("stiffness", gc.stiffness);
        gc.damping       = g.value("damping", gc.damping);
        gc.impulse_scale = g.value("impulse_scale", gc.impulse_scale);
        gc.max_offset    = g.value("max_offset", gc.max_offset);
        if (g.contains("color")) {
            const auto& c = g["color"];
            gc.r = u8(c, "r", gc.r); gc.g = u8(c, "g", gc.g);
            gc.b = u8(c, "b", gc.b); gc.a = u8(c, "a", gc.a);
        }
    }
    if (data.contains("flight_report")) {
        const auto& f = data["flight_report"];
        auto& fc = cfg.flight_report;
        fc.enabled        = f.value("enabled", fc.enabled);
        fc.sample_every_n = f.value("sample_every_n", fc.sample_every_n);
        fc.max_samples    = f.value("max_samples", fc.max_samples);
        fc.x              = f.value("x", fc.x);
        fc.y              = f.value("y", fc.y);
        fc.size           = f.value("size", fc.size);
    }
    if (data.contains("forces")) {
        cfg.forces.max_sources = data["forces"].value("max_sources", cfg.forces.max_sources);
    }
    if (data.contains("palettes")) {
        const auto& pl = data["palettes"];
        cfg.palettes.enabled = pl.value("enabled", cfg.palettes.enabled);
        if (pl.contains("palettes")) {
            for (const auto& row : pl["palettes"]) {
                PaletteDef pd;
                pd.name = row.value("name", pd.name);
                if (row.contains("colors")) {
                    for (const auto& c : row["colors"]) {
                        pd.colors.push_back(c.get<uint32_t>());
                    }
                }
                cfg.palettes.palettes.push_back(std::move(pd));
            }
        }
    }
    if (data.contains("patterns")) {
        for (const auto& row : data["patterns"]) {
            BulletPatternDef pat;
            pat.name = row.value("name", pat.name);
            pat.loop = row.value("loop", pat.loop);
            if (row.contains("ops")) {
                for (const auto& o : row["ops"]) {
                    BulletPatternOp op;
                    op.type            = o.value("type", op.type);
                    op.count           = o.value("count", op.count);
                    op.speed           = o.value("speed", op.speed);
                    op.spread_deg      = o.value("spread_deg", op.spread_deg);
                    op.angular_vel_deg = o.value("angular_vel_deg", op.angular_vel_deg);
                    op.interval        = o.value("interval", op.interval);
                    op.wait            = o.value("wait", op.wait);
                    pat.ops.push_back(std::move(op));
                }
            }
            cfg.patterns.push_back(std::move(pat));
        }
    }
    if (data.contains("audio")) {
        const auto& au = data["audio"];
        auto& ac = cfg.audio;
        ac.enabled       = au.value("enabled", ac.enabled);
        ac.master_volume = au.value("master_volume", ac.master_volume);
        ac.sample_rate   = au.value("sample_rate", ac.sample_rate);
        ac.voices        = au.value("voices", ac.voices);
    }

    // Phase B (D50): run difficulties. Index 0 is the default, so an absent or
    // empty block simply means "one difficulty, all multipliers 1.0".
    if (data.contains("difficulties")) {
        for (const auto& e : data["difficulties"]) {
            DifficultyDef d;
            d.name                = e.value("name", d.name);
            d.count_mult          = e.value("count_mult", d.count_mult);
            d.spawn_interval_mult = e.value("spawn_interval_mult", d.spawn_interval_mult);
            d.hp_mult             = e.value("hp_mult", d.hp_mult);
            d.speed_mult          = e.value("speed_mult", d.speed_mult);
            d.currency_mult       = e.value("currency_mult", d.currency_mult);
            d.hazard_damage_mult  = e.value("hazard_damage_mult", d.hazard_damage_mult);
            d.type_lookahead      = e.value("type_lookahead", d.type_lookahead);
            d.boss_mult           = e.value("boss_mult", d.boss_mult);   // D73
            cfg.difficulties.push_back(std::move(d));
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
        sc.shield_regen_frac  = s.value("shield_regen_frac", sc.shield_regen_frac);
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
