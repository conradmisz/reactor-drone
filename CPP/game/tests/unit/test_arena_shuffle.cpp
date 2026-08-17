/**
 * Unit tests for the per-run arena shuffle (gameplay pack v2.3 tier 4, D221
 * call #6). The two owner rules — no Prism opener, Singularity stays the
 * wave-30 finale — are checked across many seeds, not one lucky draw.
 */
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <random>
#include <set>

#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"

namespace {
std::vector<ArenaDef> authored() {
    return load_arena_config(project_paths::assets_dir() + "/GameData.json").arenas;
}
}

TEST_CASE("shuffle keeps the ladder, pins the finale, never opens on Prism", "[shuffle]") {
    const std::vector<ArenaDef> base = authored();
    REQUIRE(base.size() >= 3);
    // The helper assumes ascending authored first_waves — pin that here.
    for (size_t i = 1; i < base.size(); ++i)
        REQUIRE(base[i].first_wave > base[i - 1].first_wave);

    std::vector<int> ladder;
    for (const ArenaDef& a : base) ladder.push_back(a.first_wave);

    for (unsigned seed = 0; seed < 200; ++seed) {
        std::vector<ArenaDef> v = base;
        std::mt19937 rng(seed);
        shuffle_arena_order(v, rng);
        // Same ladder, same finale, no Prism opener.
        for (size_t i = 0; i < v.size(); ++i) CHECK(v[i].first_wave == ladder[i]);
        CHECK(v.back().name == base.back().name);
        CHECK(v.front().name.rfind("Prism", 0) != 0);
        // Still a permutation — nothing dropped or duplicated.
        std::multiset<std::string> a, b;
        for (const ArenaDef& x : base) a.insert(x.name);
        for (const ArenaDef& x : v) b.insert(x.name);
        CHECK(a == b);
        // D231 (bugs/015): no same-family neighbours — a REACTOR SHIFT must
        // always land on a visibly different arena.
        auto family = [](std::string n) {
            if (n.size() > 3 && n.compare(n.size() - 3, 3, " II") == 0)
                n.resize(n.size() - 3);
            return n;
        };
        for (size_t i = 0; i + 1 < v.size(); ++i) {
            INFO("seed " << seed << ": " << v[i].name << " then " << v[i + 1].name);
            CHECK(family(v[i].name) != family(v[i + 1].name));
        }
    }
}

TEST_CASE("same seed, same order; different seeds usually differ", "[shuffle]") {
    const std::vector<ArenaDef> base = authored();
    auto order = [&](unsigned seed) {
        std::vector<ArenaDef> v = base;
        std::mt19937 rng(seed);
        shuffle_arena_order(v, rng);
        std::string s;
        for (const ArenaDef& a : v) s += a.name + ";";
        return s;
    };
    CHECK(order(42) == order(42));
    // 20 seeds must produce at least a handful of distinct orders.
    std::set<std::string> seen;
    for (unsigned s = 0; s < 20; ++s) seen.insert(order(s));
    CHECK(seen.size() >= 5);
}
