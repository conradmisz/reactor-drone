#include "engine/ecs/systems/chip_synth_system.hpp"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace {

/// Voice waveforms. Deliberately the classic four — anything richer stops
/// sounding like the hardware this is imitating.
enum class Wave : uint8_t { Pulse, Saw, Triangle, Noise };

/// One note event queued from the game thread. A fixed ring: the audio callback
/// never allocates and never blocks, so an overflowing queue drops the newest
/// note rather than stalling the mixer.
struct Note {
    float freq = 440.0f;
    float duration = 0.12f;
    float volume = 0.5f;
    Wave wave = Wave::Pulse;
    float sweep = 0.0f;      // Hz/sec pitch glide — the whole "chip" character
    float duty = 0.5f;       // pulse width
};

constexpr int QUEUE_SIZE = 64;

/// An SFX is a short list of notes, played in order. Authored in code rather
/// than in GameData: these are five-note blips, and a JSON table for them would
/// be more surface area than the sounds themselves.
/// ponytail: if the sound design ever wants tuning without a rebuild, this table
/// is what moves into the "audio" block.
struct SfxDef {
    int count = 0;
    Note notes[4];
};

SfxDef sfx_table(int id) {
    SfxDef d;
    switch (id) {
        case ChipSynthSystem::SFX_SHOOT:
            d.count = 1;
            d.notes[0] = Note{880.0f, 0.06f, 0.20f, Wave::Pulse, -6000.0f, 0.25f};
            break;
        case ChipSynthSystem::SFX_ENEMY_DEATH:
            d.count = 2;
            d.notes[0] = Note{300.0f, 0.10f, 0.30f, Wave::Noise, -1200.0f, 0.5f};
            d.notes[1] = Note{140.0f, 0.14f, 0.24f, Wave::Saw, -500.0f, 0.5f};
            break;
        case ChipSynthSystem::SFX_PLAYER_HURT:
            d.count = 2;
            d.notes[0] = Note{220.0f, 0.10f, 0.42f, Wave::Saw, -900.0f, 0.5f};
            d.notes[1] = Note{110.0f, 0.16f, 0.34f, Wave::Pulse, -200.0f, 0.5f};
            break;
        case ChipSynthSystem::SFX_PICKUP:
            d.count = 2;
            d.notes[0] = Note{660.0f, 0.05f, 0.22f, Wave::Pulse, 0.0f, 0.5f};
            d.notes[1] = Note{990.0f, 0.07f, 0.22f, Wave::Pulse, 0.0f, 0.5f};
            break;
        case ChipSynthSystem::SFX_WAVE_CLEAR:
            d.count = 3;
            d.notes[0] = Note{523.0f, 0.10f, 0.26f, Wave::Triangle, 0.0f, 0.5f};
            d.notes[1] = Note{659.0f, 0.10f, 0.26f, Wave::Triangle, 0.0f, 0.5f};
            d.notes[2] = Note{784.0f, 0.20f, 0.28f, Wave::Triangle, 0.0f, 0.5f};
            break;
        case ChipSynthSystem::SFX_DASH:
            d.count = 1;
            d.notes[0] = Note{200.0f, 0.16f, 0.20f, Wave::Noise, 2400.0f, 0.5f};
            break;
        default: break;
    }
    return d;
}

/// The bass line the music sequencer walks, as semitone offsets from A1. A
/// minor-key ostinato: the reactor humming while something goes wrong.
constexpr int PATTERN[16] = {0, 0, 7, 0, 3, 0, 7, 10, 0, 0, 7, 0, 5, 3, 2, 0};

float semitone_to_hz(int semi) {
    return 55.0f * std::pow(2.0f, static_cast<float>(semi) / 12.0f);
}

}  // namespace

/// Everything the audio thread touches. Only the callback writes it, except for
/// the two atomics, which is what keeps the audio path lock-free.
struct ChipSynthSystem::Impl {
    struct Voice {
        bool active = false;
        Wave wave = Wave::Pulse;
        float phase = 0.0f;
        float freq = 440.0f;
        float sweep = 0.0f;
        float duty = 0.5f;
        float volume = 0.0f;
        float remaining = 0.0f;
        float total = 0.12f;
    };

    std::vector<Voice> voices;
    int sample_rate = 48000;
    float master = 0.8f;

    // Game thread -> audio thread. A ring of pending SFX ids; the callback drains
    // it. Relaxed ordering is enough — a note landing one buffer late is
    // inaudible, and correctness never depends on the order of two SFX.
    std::atomic<int> queue[QUEUE_SIZE]{};
    std::atomic<unsigned> write_cursor{0};
    unsigned read_cursor = 0;
    std::atomic<int> triggered{0};

    // Music. `intensity` is the director's stress, pushed from update().
    std::atomic<float> intensity{0.0f};
    std::atomic<bool> music_on{true};
    int step = 0;
    float step_timer = 0.0f;

    uint32_t noise_state = 0x1234567u;   // private LCG: never the sim's stream

    float noise() {
        noise_state = noise_state * 1664525u + 1013904223u;
        return static_cast<float>((noise_state >> 9) & 0xFFFF) / 32768.0f - 1.0f;
    }

    Voice* free_voice() {
        // Steal the quietest voice when every one is busy: dropping the loudest
        // note is what makes a busy frame sound broken.
        Voice* quietest = nullptr;
        for (Voice& v : voices) {
            if (!v.active) return &v;
            if (quietest == nullptr || v.volume * v.remaining < quietest->volume * quietest->remaining)
                quietest = &v;
        }
        return quietest;
    }

    void start_note(const Note& n) {
        Voice* v = free_voice();
        if (v == nullptr) return;
        v->active = true;
        v->wave = n.wave;
        v->phase = 0.0f;
        v->freq = n.freq;
        v->sweep = n.sweep;
        v->duty = n.duty;
        v->volume = n.volume;
        v->remaining = n.duration;
        v->total = n.duration;
    }

    float sample_voice(Voice& v, float dt) {
        if (!v.active) return 0.0f;
        float s = 0.0f;
        switch (v.wave) {
            case Wave::Pulse:    s = v.phase < v.duty ? 1.0f : -1.0f; break;
            case Wave::Saw:      s = v.phase * 2.0f - 1.0f; break;
            case Wave::Triangle: s = 4.0f * std::fabs(v.phase - 0.5f) - 1.0f; break;
            case Wave::Noise:    s = noise(); break;
        }
        // Linear decay envelope. An ADSR would be more expressive and none of
        // these sounds are longer than a fifth of a second.
        const float env = v.total > 0.0f ? std::max(0.0f, v.remaining / v.total) : 0.0f;
        v.freq = std::max(20.0f, v.freq + v.sweep * dt);
        v.phase += v.freq * dt;
        v.phase -= std::floor(v.phase);
        v.remaining -= dt;
        if (v.remaining <= 0.0f) v.active = false;
        return s * env * v.volume;
    }

    void render(float* out, int frames) {
        const float dt = 1.0f / static_cast<float>(sample_rate);

        // Drain the SFX queue at buffer granularity: sub-buffer timing is
        // inaudible at 48 kHz and per-sample draining would cost an atomic load
        // per sample.
        const unsigned w = write_cursor.load(std::memory_order_relaxed);
        while (read_cursor != w) {
            const int id = queue[read_cursor % QUEUE_SIZE].load(std::memory_order_relaxed);
            const SfxDef d = sfx_table(id);
            for (int i = 0; i < d.count; ++i) start_note(d.notes[i]);
            ++read_cursor;
        }

        const float inten = intensity.load(std::memory_order_relaxed);
        const bool music = music_on.load(std::memory_order_relaxed);
        // Tempo and voicing follow the fight: calm is a slow bass pulse, a
        // scramble is faster and adds the fifth above. This is the Adaptive
        // Director's stress scalar doing double duty as a music director.
        const float step_len = 0.30f - 0.12f * inten;

        for (int i = 0; i < frames; ++i) {
            if (music) {
                step_timer -= dt;
                if (step_timer <= 0.0f) {
                    step_timer += std::max(0.05f, step_len);
                    const int semi = PATTERN[step % 16];
                    start_note(Note{semitone_to_hz(semi), step_len * 0.9f,
                                    0.16f + 0.06f * inten, Wave::Pulse, 0.0f, 0.5f});
                    if (inten > 0.45f && (step % 4) == 0) {
                        start_note(Note{semitone_to_hz(semi + 19), step_len * 0.5f,
                                        0.09f * inten, Wave::Triangle, 0.0f, 0.5f});
                    }
                    ++step;
                }
            }

            float mix = 0.0f;
            for (Voice& v : voices) mix += sample_voice(v, dt);
            mix *= master * 0.35f;              // headroom for eight voices
            mix = std::min(1.0f, std::max(-1.0f, mix));
            out[i * 2] = mix;                   // stereo, but mono content:
            out[i * 2 + 1] = mix;               // a chip console had one speaker
        }
    }
};

void SDLCALL ChipSynthSystem::audio_callback(void* userdata, SDL_AudioStream* stream,
                                            int additional, int /*total*/) {
    auto* impl = static_cast<ChipSynthSystem::Impl*>(userdata);
    if (impl == nullptr || additional <= 0) return;

    // Fixed scratch buffer, filled in chunks: no allocation in the audio path.
    constexpr int CHUNK_FRAMES = 512;
    static thread_local float buffer[CHUNK_FRAMES * 2];
    int remaining_bytes = additional;
    while (remaining_bytes > 0) {
        const int bytes = std::min(remaining_bytes,
                                   static_cast<int>(sizeof(buffer)));
        const int frames = bytes / static_cast<int>(sizeof(float) * 2);
        if (frames <= 0) break;
        impl->render(buffer, frames);
        SDL_PutAudioStreamData(stream, buffer,
                               frames * static_cast<int>(sizeof(float) * 2));
        remaining_bytes -= frames * static_cast<int>(sizeof(float) * 2);
    }
}

ChipSynthSystem::~ChipSynthSystem() {
    // The SDL_WasInit guard is load-bearing, and it cost a core dump to find:
    // main.cpp calls SDL_Quit() as a STATEMENT, and a function-local static
    // system is destroyed at process exit — i.e. AFTER SDL has already torn its
    // audio subsystem down. Destroying a stream at that point crashes on the way
    // out of a run that otherwise completed fine. If SDL is gone, so is the
    // stream, and the OS reclaims the rest.
    if (stream_ != nullptr && SDL_WasInit(SDL_INIT_AUDIO) != 0)
        SDL_DestroyAudioStream(stream_);
    delete impl_;
}

bool ChipSynthSystem::start(int sample_rate, int voices, float master_volume) {
    if (device_ != 0) return true;
    if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) return false;

    impl_ = new Impl();
    impl_->sample_rate = sample_rate > 0 ? sample_rate : 48000;
    impl_->master = std::min(1.0f, std::max(0.0f, master_volume));
    impl_->voices.assign(static_cast<size_t>(std::max(1, voices)), Impl::Voice{});

    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = 2;
    spec.freq = impl_->sample_rate;

    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
                                        &ChipSynthSystem::audio_callback, impl_);
    if (stream_ == nullptr) {
        delete impl_;
        impl_ = nullptr;
        return false;
    }
    device_ = SDL_GetAudioStreamDevice(stream_);
    SDL_ResumeAudioStreamDevice(stream_);
    return true;
}

void ChipSynthSystem::play(Sfx id) {
    if (impl_ == nullptr) return;
    const unsigned slot = impl_->write_cursor.fetch_add(1, std::memory_order_relaxed);
    impl_->queue[slot % QUEUE_SIZE].store(static_cast<int>(id), std::memory_order_relaxed);
    impl_->triggered.fetch_add(1, std::memory_order_relaxed);
}

int ChipSynthSystem::sfx_triggered() const {
    return impl_ == nullptr ? 0 : impl_->triggered.load(std::memory_order_relaxed);
}

void ChipSynthSystem::update(const Blackboard& blackboard) {
    if (impl_ == nullptr) return;

    // EVERY trigger site is here, in this system, reading keys the game already
    // publishes for its own reasons. That is what makes the feature one revert:
    // no game file has an audio line in it.
    static int last_kills = 0;
    static int last_score = 0;
    static int last_wave = 0;
    static float last_hull = -1.0f;

    const int kills = blackboard.get_or<int>("sim.kills", 0);
    if (kills > last_kills) play(SFX_ENEMY_DEATH);
    last_kills = kills;

    // Credits rise only when a pickup is collected, so the score/credit delta is
    // the pickup event without a publisher of its own.
    const int score = blackboard.get_or<int>("score", 0);
    if (score > last_score && kills == last_kills) play(SFX_PICKUP);
    last_score = score;

    const int wave = blackboard.get_or<int>("wave", 0);
    if (wave > last_wave && last_wave > 0) play(SFX_WAVE_CLEAR);
    last_wave = wave;

    // The drone taking a hit is the one event with no counter at all: the HUD's
    // i-frame window rising from zero IS the hit.
    const float iframes = blackboard.get_or<float>("player.iframes", 0.0f);
    if (iframes > 0.0f && last_hull <= 0.0f) play(SFX_PLAYER_HURT);
    last_hull = iframes;

    impl_->intensity.store(blackboard.get_or<float>("director.stress", 0.0f),
                           std::memory_order_relaxed);
    // Music holds on the title screen and the shop; it is the arena's hum.
    const int phase = blackboard.get_or<int>("phase", 0);
    impl_->music_on.store(phase == 1 || phase == 5, std::memory_order_relaxed);
}
