#ifndef CHIP_SYNTH_SYSTEM_HPP
#define CHIP_SYNTH_SYSTEM_HPP

#include <cstdint>
#include <string>
#include <vector>

#include <SDL3/SDL.h>

#include "engine/ecs/blackboard.hpp"

/**
 * ChipSynthSystem — the sound of the console this game dreams of running on
 * (#4, Lane Z, D150).
 *
 * No samples. Every effect and the music are synthesised live from a fixed voice
 * pool — square, saw, triangle and noise — with the music's intensity following
 * the fight. A few hundred bytes of note data instead of megabytes of WAVs, which
 * is the most MCU-native thing in the whole suite: chip synths were born there.
 *
 * ONE FILE PAIR, AND ONE REVERT (project law for this lane). Everything outside
 * this pair is: `SDL_INIT_AUDIO` in the init flags, a construction, and one
 * `update()` call in the `audio` hook. Trigger sites are a list of Blackboard
 * reads *inside* this system — game code publishes nothing new for it, so
 * deleting the pair and its three lines removes the feature entirely.
 *
 * DETERMINISM: isolated. It reads sim state and writes sound; nothing reads it
 * back, it draws from no shared RNG (its noise voice runs a private LCG), and it
 * never writes a Blackboard key. The replay canary is untouched by definition —
 * and by construction, since the canary runs with audio disabled.
 *
 * THREADING. SDL3 fills the stream from an audio thread. The synth state the
 * callback touches is only ever written by the callback itself; `update()`
 * communicates through a small set of atomics, so there is no lock in the audio
 * path (a lock there is a click, and a click is worse than a dropped note).
 */
class ChipSynthSystem {
public:
    ~ChipSynthSystem();

    ChipSynthSystem() = default;
    ChipSynthSystem(const ChipSynthSystem&) = delete;
    ChipSynthSystem& operator=(const ChipSynthSystem&) = delete;

    /**
     * Open the device and start the music. Returns false when disabled or when
     * the device will not open — a machine with no sound card plays the game
     * silently rather than not at all.
     */
    bool start(int sample_rate, int voices, float master_volume);

    /// Read this frame's sim events and trigger what they ask for.
    void update(const Blackboard& blackboard);

    bool running() const { return device_ != 0; }

    /// SFX ids. Code constants, never data indices (D26) — a re-ordered table
    /// must never turn a pickup chirp into an explosion.
    enum Sfx {
        SFX_SHOOT = 0,
        SFX_ENEMY_DEATH,
        SFX_PLAYER_HURT,
        SFX_PICKUP,
        SFX_WAVE_CLEAR,
        SFX_DASH,
        SFX_COUNT,
    };

    /// Trigger one effect. Safe to call from the game thread at any time.
    void play(Sfx id);

    // --- inspection, for tests (no device required) ---
    int sfx_triggered() const;

private:
    struct Impl;
    /// SDL's stream callback. A private static member rather than a free
    /// function so `Impl` can stay private — the audio-thread state is reachable
    /// from exactly one place.
    static void SDLCALL audio_callback(void* userdata, SDL_AudioStream* stream,
                                       int additional, int total);

    Impl* impl_ = nullptr;      // pimpl: the audio-thread state stays out of the
                                // header, so nothing else can reach into it
    SDL_AudioDeviceID device_ = 0;
    SDL_AudioStream* stream_ = nullptr;
};

#endif  // CHIP_SYNTH_SYSTEM_HPP
