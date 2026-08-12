/**
 * test_chip_synth.cpp — chip-synth audio (engine suite, Lane Z, D150).
 *
 * The synth is an SDL audio device and a callback on another thread, neither of
 * which belongs in a unit test on a headless machine. What IS pinned here is the
 * contract that keeps the feature safe to ship disabled and safe to run on a box
 * with no sound card: every entry point must be a no-op before `start()`
 * succeeds, and `update()` must never write to the Blackboard — the isolation
 * claim in the header is the whole determinism argument for this lane.
 *
 * The audible behaviour is verified by running the game with `--suite`.
 */
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <string>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/systems/chip_synth_system.hpp"
#include "engine/project_paths.hpp"
#include "game/arena_config.hpp"

TEST_CASE("an unstarted synth is inert at every entry point", "[Game][audio]") {
    ChipSynthSystem chip;
    CHECK_FALSE(chip.running());

    Blackboard bb;
    bb.set<int>("sim.kills", 12);
    bb.set<int>("score", 400);
    bb.set<int>("wave", 3);
    bb.set<int>("phase", 1);

    chip.update(bb);                       // no device: must not fire or crash
    chip.play(ChipSynthSystem::SFX_SHOOT);
    CHECK(chip.sfx_triggered() == 0);
    CHECK_FALSE(chip.running());
}

TEST_CASE("update writes nothing to the Blackboard", "[Game][audio][determinism]") {
    // The isolation contract: audio reads sim events and writes sound. If it ever
    // wrote a key, a sim system could read it and a replay could diverge on
    // presentation state — the same trap UIElement::pulse_hz avoids by keeping its
    // clock render-local.
    ChipSynthSystem chip;
    Blackboard bb;
    bb.set<int>("sim.kills", 5);
    bb.set<int>("score", 100);
    bb.set<int>("wave", 2);
    bb.set<int>("phase", 1);
    bb.set<float>("director.stress", 0.4f);
    bb.set<float>("player.iframes", 0.5f);

    std::vector<std::string> before = bb.get_all_keys();
    std::sort(before.begin(), before.end());
    for (int i = 0; i < 10; ++i) chip.update(bb);
    std::vector<std::string> after = bb.get_all_keys();
    std::sort(after.begin(), after.end());
    CHECK(before == after);
}

TEST_CASE("the shipped audio block is present and disabled", "[Game][audio]") {
    GameConfig cfg = load_arena_config(project_paths::assets_dir() + "/GameData.json");
    CHECK_FALSE(cfg.audio.enabled);        // no device is opened by default
    CHECK(cfg.audio.voices == 8);
    CHECK(cfg.audio.sample_rate == 48000);
    CHECK(cfg.audio.master_volume > 0.0f);
    CHECK(cfg.audio.master_volume <= 1.0f);
}
