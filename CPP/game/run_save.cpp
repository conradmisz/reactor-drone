#include "run_save.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <system_error>
#include <type_traits>
#include <vector>

#include <nlohmann/json.hpp>

#include "engine/project_paths.hpp"
#include "player_components.hpp"  // ShipState, WeaponStats

namespace {

/// Per-field tolerant read: a missing key, or a key of the wrong JSON type,
/// yields the default instead of throwing. Doing this per field rather than
/// per file is what makes a *partially* corrupt save still resumable.
template <typename T>
T jget(const nlohmann::json& j, const char* key, T fallback) {
    if (!j.is_object() || !j.contains(key)) return fallback;
    const nlohmann::json& v = j.at(key);
    if constexpr (std::is_same_v<T, std::string>) {
        if (!v.is_string()) return fallback;
    } else if constexpr (std::is_floating_point_v<T>) {
        if (!v.is_number()) return fallback;
    } else {
        if (!v.is_number_integer()) return fallback;
    }
    return v.get<T>();
}

void read_int_array(const nlohmann::json& j, const char* key, int (&out)[8]) {
    if (!j.is_object() || !j.contains(key)) return;
    const nlohmann::json& a = j.at(key);
    if (!a.is_array()) return;
    const size_t n = std::min<size_t>(8, a.size());
    for (size_t i = 0; i < n; ++i) {
        if (a[i].is_number_integer()) out[i] = std::max(0, a[i].get<int>());
    }
}

/// The player entity, or false if the world has none. Deliberately not an
/// `Entity == 0` sentinel: entity 0 is a perfectly ordinary id here, and on a
/// freshly spawned world it is usually the drone itself.
bool find_player(ComponentStorage& storage, Entity& out) {
    for (Entity p : storage.entities_with_component<PlayerTag>()) { out = p; return true; }
    return false;
}

}  // namespace

std::string run_save_path() {
    return project_paths::user_data_dir() + "/saves/run.json";
}

std::string run_save_path(int slot) {
    return project_paths::user_data_dir() + "/saves/run" + std::to_string(slot) + ".json";
}

void run_save_migrate_legacy() {
    // One shot, boot only: the pre-slot save becomes slot 1. Never overwrites —
    // an existing slot 1 wins and the legacy file is left where it was.
    std::error_code ec;
    const std::string legacy = run_save_path();
    if (!std::filesystem::exists(legacy, ec) || ec) return;
    if (std::filesystem::exists(run_save_path(1), ec) || ec) return;
    std::filesystem::rename(legacy, run_save_path(1), ec);
    // ec deliberately ignored: a failed rename just means no migration.
}

RunSave run_save_load(const std::string& path) {
    RunSave s;
    std::ifstream in(path);
    if (!in.is_open()) return s;  // no save — the common case
    try {
        nlohmann::json j = nlohmann::json::parse(in, nullptr, /*allow_exceptions=*/false);
        if (!j.is_object()) return RunSave{};
        // An unknown version is treated as no save at all. Silently reading a
        // future file's fields is how a save format starts lying.
        if (jget(j, "version", 0) != RunSave::CURRENT_VERSION) return RunSave{};

        s.seed            = static_cast<unsigned int>(std::max(0LL, jget<long long>(j, "seed", 0)));
        s.difficulty      = std::max(0, jget(j, "difficulty", 0));
        s.difficulty_name = jget<std::string>(j, "difficulty_name", std::string());
        s.ship_id         = std::max(0, jget(j, "ship_id", 0));
        s.weapon          = jget<std::string>(j, "weapon", std::string());
        s.wave            = std::max(0, jget(j, "wave", 0));
        s.score           = std::max(0, jget(j, "score", 0));

        s.hull         = jget(j, "hull", 0.0f);
        s.hull_max     = jget(j, "hull_max", 0.0f);
        s.shield       = std::max(0.0f, jget(j, "shield", 0.0f));
        s.shield_max   = std::max(0.0f, jget(j, "shield_max", 0.0f));
        s.shield_regen = std::max(0.0f, jget(j, "shield_regen", 0.0f));
        s.shield_delay = std::max(0.0f, jget(j, "shield_delay", 0.0f));
        s.credits      = std::max(0, jget(j, "credits", 0));
        s.keys         = std::max(0, jget(j, "keys", 0));
        s.speed_mult   = jget(j, "speed_mult", 1.0f);
        s.dash_max     = std::max(1, jget(j, "dash_max", 1));
        s.item_id       = jget(j, "item_id", -1);
        s.consumable_id = jget(j, "consumable_id", -1);
        s.active_id     = jget(j, "active_id", -1);
        s.extra_shots   = std::max(0, jget(j, "extra_shots", 0));
        read_int_array(j, "upg_counts", s.upg_counts);
        read_int_array(j, "gear_levels", s.gear_levels);

        s.fire_rate           = jget(j, "fire_rate", 0.0f);
        s.damage              = jget(j, "damage", 0.0f);
        s.projectile_speed    = jget(j, "projectile_speed", 0.0f);
        s.projectile_lifetime = jget(j, "projectile_lifetime", 0.0f);
        s.spread              = jget(j, "spread", -1.0f);
        s.saved_at            = std::max(0LL, jget<long long>(j, "saved_at", 0));
        s.present = true;
    } catch (...) {
        return RunSave{};  // ponytail: a hand-edited save costs a resume, never a crash
    }
    return s;
}

bool run_save_write(const std::string& path, const RunSave& s) {
    try {
        const std::filesystem::path p(path);
        if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
        std::ofstream out(path, std::ios::trunc);
        if (!out.is_open()) return false;
        nlohmann::json j{
            {"version", RunSave::CURRENT_VERSION},
            {"seed", static_cast<long long>(s.seed)},
            {"difficulty", s.difficulty},
            {"difficulty_name", s.difficulty_name},
            {"ship_id", s.ship_id},
            {"weapon", s.weapon},
            {"wave", s.wave},
            {"score", s.score},
            {"hull", s.hull},
            {"hull_max", s.hull_max},
            {"shield", s.shield},
            {"shield_max", s.shield_max},
            {"shield_regen", s.shield_regen},
            {"shield_delay", s.shield_delay},
            {"credits", s.credits},
            {"keys", s.keys},
            {"speed_mult", s.speed_mult},
            {"dash_max", s.dash_max},
            {"item_id", s.item_id},
            {"consumable_id", s.consumable_id},
            {"active_id", s.active_id},
            {"extra_shots", s.extra_shots},
            {"upg_counts", std::vector<int>(s.upg_counts, s.upg_counts + 8)},
            {"gear_levels", std::vector<int>(s.gear_levels, s.gear_levels + 8)},
            {"fire_rate", s.fire_rate},
            {"damage", s.damage},
            {"projectile_speed", s.projectile_speed},
            {"projectile_lifetime", s.projectile_lifetime},
            {"spread", s.spread},
            {"saved_at", s.saved_at},
        };
        out << j.dump(2) << "\n";
        return out.good();
    } catch (...) {
        return false;
    }
}

void run_save_clear(const std::string& path) {
    std::error_code ec;
    std::filesystem::remove(path, ec);  // no-throw overload; a failure is not fatal
}

RunSave run_save_capture(ComponentStorage& storage, const Blackboard& blackboard,
                         int wave, int difficulty, const std::string& difficulty_name,
                         int ship_id, unsigned int seed) {
    RunSave s;
    s.present         = true;
    s.seed            = seed;
    s.difficulty      = difficulty;
    s.difficulty_name = difficulty_name;
    s.ship_id         = ship_id;
    s.wave            = std::max(0, wave);
    s.score           = blackboard.get_or<int>("score", 0);
    s.weapon          = blackboard.get_or<std::string>("weapon.name", std::string());
    s.extra_shots     = blackboard.get_or<int>("ship.extra_shots", 0);

    Entity player = 0;
    if (!find_player(storage, player)) return s;

    if (auto h = storage.get_component<Health>(player); h.has_value()) {
        s.hull = h->get().current;
        s.hull_max = h->get().max_hp;
    }
    if (auto sh = storage.get_component<ShipState>(player); sh.has_value()) {
        const ShipState& st = sh->get();
        s.credits = st.currency;
        s.keys = st.keys;
        s.shield = st.shield;
        s.shield_max = st.shield_max;
        s.shield_regen = st.shield_regen;
        s.shield_delay = st.shield_delay;
        s.speed_mult = st.speed_mult;
        s.dash_max = st.dash_max;
        s.item_id = st.item_id;
        s.consumable_id = st.consumable_id;
        s.active_id = st.active_id;
        for (int i = 0; i < 8; ++i) {
            s.upg_counts[i] = st.upg_counts[i];
            s.gear_levels[i] = st.gear_levels[i];
        }
    }
    if (auto w = storage.get_component<WeaponStats>(player); w.has_value()) {
        const WeaponStats& ws = w->get();
        s.fire_rate = ws.fire_rate;
        s.damage = ws.damage;
        s.projectile_speed = ws.projectile_speed;
        s.projectile_lifetime = ws.projectile_lifetime;
        s.spread = ws.spread;
    }
    return s;
}

void run_save_apply(const RunSave& s, ComponentStorage& storage, Blackboard& blackboard) {
    if (!s.present) return;
    blackboard.set<int>("score", s.score);
    blackboard.set<int>("ship.extra_shots", s.extra_shots);

    Entity player = 0;
    if (!find_player(storage, player)) return;

    if (auto h = storage.get_component<Health>(player); h.has_value()) {
        // A nonsense hull leaves the fresh full-health drone alone rather than
        // spawning a corpse: a truncated file should cost the player nothing
        // worse than a free heal.
        if (s.hull_max > 0.0f) h->get().max_hp = s.hull_max;
        if (s.hull > 0.0f) h->get().current = std::min(s.hull, h->get().max_hp);
    }
    if (auto sh = storage.get_component<ShipState>(player); sh.has_value()) {
        ShipState& st = sh->get();
        st.currency = s.credits;
        st.keys = s.keys;
        st.shield_max = s.shield_max;
        st.shield = std::min(s.shield, s.shield_max);
        st.shield_regen = s.shield_regen;
        st.shield_delay = s.shield_delay;
        if (s.speed_mult > 0.0f) st.speed_mult = s.speed_mult;
        st.dash_max = std::max(1, s.dash_max);
        st.dash_charges = st.dash_max;
        st.item_id = s.item_id;
        st.consumable_id = s.consumable_id;
        st.active_id = s.active_id;
        for (int i = 0; i < 8; ++i) {
            st.upg_counts[i] = s.upg_counts[i];
            st.gear_levels[i] = s.gear_levels[i];
        }
    }
    if (auto w = storage.get_component<WeaponStats>(player); w.has_value()) {
        WeaponStats& ws = w->get();
        if (s.fire_rate > 0.0f) ws.fire_rate = s.fire_rate;
        if (s.damage > 0.0f) ws.damage = s.damage;
        if (s.projectile_speed > 0.0f) ws.projectile_speed = s.projectile_speed;
        if (s.projectile_lifetime > 0.0f) ws.projectile_lifetime = s.projectile_lifetime;
        if (s.spread >= 0.0f) ws.spread = s.spread;
    }
}
