#ifndef FLIGHT_REPORT_HPP
#define FLIGHT_REPORT_HPP

#include <cstddef>
#include <vector>

#include "engine/ecs/blackboard.hpp"
#include "engine/ecs/component_storage.hpp"
#include "engine/ecs/entity_manager.hpp"
#include "arena_config.hpp"

/**
 * flight_report — the run as an artifact (#10, Lane S, D143).
 *
 * Game over is currently a number. This turns it into an incident report: the
 * arena with the whole flight path burned into it, every kill marked, every hit
 * the drone took marked, and the wave the run ended on.
 *
 * PASSIVE, and that is the determinism stance in full: it reads sim state into
 * its own fixed ring buffers and renders on the terminal screens. Nothing reads
 * it back, it owns no RNG, and it never writes a value any sim system reads. Its
 * kill marks come from the render-side `fx.scar_stamps` list (Phase 0's shared
 * vocabulary) rather than from a second publisher at the death site — one event,
 * two render-only consumers.
 *
 * Fixed ring buffers, sized once from config (MCU headroom): recording is
 * O(1) per sample with no allocation after the first wave, and the *drawing* pool
 * is much smaller than the buffer — the report decimates the path down to the
 * pool rather than trying to draw 4096 marks.
 */
class FlightReport {
public:
    /// Pooled marks are UI widgets on the always-active `gameplay` screen, the
    /// mechanism MinimapSystem proved (D58) — a screen-space entity with no
    /// Position is drawn by nothing, and adding one hands it to CameraSystem.
    static constexpr const char* SCREEN_NAME = "gameplay";
    // ponytail: the marks borrow the minimap's three styles rather than adding
    // ui_styles rows — same palette family, same "blip on an arena map" job. If
    // the report ever wants its own colours, that is a GameData edit, not code.
    static constexpr const char* STYLE_PATH  = "minimap_pickup";  // faint trail
    static constexpr const char* STYLE_KILL  = "minimap_enemy";   // kill marks
    static constexpr const char* STYLE_HIT   = "minimap_health";  // hits taken

    /// Marks drawn at once. The path gets the lion's share; kills and hits are
    /// rarer and read as events rather than as a line.
    static constexpr std::size_t PATH_MARKS = 200;
    static constexpr std::size_t KILL_MARKS = 90;
    static constexpr std::size_t HIT_MARKS  = 12;

    void set_config(const FlightReportConfig& cfg, const ArenaConfig& arena) {
        cfg_ = cfg;
        arena_ = arena;
    }

    /// Drop every recorded sample. Called from start_run, so a second run never
    /// draws the first one's path.
    void reset();

    /**
     * One frame of recording plus one frame of drawing.
     *
     * Recording only happens in PHASE_PLAYING; drawing only on the game-over and
     * victory screens. Called in every phase (the `telemetry` hook sits outside
     * the `sim` gate) because the terminal screens have to keep drawing while the
     * sim is stopped.
     */
    void update(ComponentStorage& component_storage,
                EntityManager& entity_manager,
                Blackboard& blackboard);

    // --- inspection, for tests ---
    std::size_t path_samples() const { return path_.size(); }
    std::size_t kill_samples() const { return kills_.size(); }
    std::size_t hit_samples() const { return hits_.size(); }
    std::size_t pool_size() const { return pool_.size(); }

private:
    struct Mark { float x = 0.0f, y = 0.0f; };

    /// Append to a ring buffer capped at `cap`: full means the OLDEST sample is
    /// overwritten, so a long run keeps its most recent history rather than
    /// silently stopping at the cap (the trap the particle budget documents).
    static void push_ring(std::vector<Mark>& ring, std::size_t& cursor,
                          std::size_t cap, float x, float y);

    void record(ComponentStorage& component_storage, Blackboard& blackboard);
    void draw(ComponentStorage& component_storage, EntityManager& entity_manager,
              Blackboard& blackboard);
    void ensure_pool(ComponentStorage& component_storage,
                     EntityManager& entity_manager);
    void park_all(ComponentStorage& component_storage);

    FlightReportConfig cfg_{};
    ArenaConfig arena_{};

    std::vector<Mark> path_, kills_, hits_;
    std::size_t path_cursor_ = 0, kill_cursor_ = 0, hit_cursor_ = 0;
    int frame_counter_ = 0;
    float last_hull_ = -1.0f;

    std::vector<Entity> pool_;   // PATH_MARKS + KILL_MARKS + HIT_MARKS widgets
};

#endif  // FLIGHT_REPORT_HPP
