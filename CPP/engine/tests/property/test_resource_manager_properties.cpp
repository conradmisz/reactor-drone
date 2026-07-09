/**
 * Property-based tests for ResourceManager class
 *
 * These tests verify universal caching properties that should hold across
 * all valid inputs using Catch2's GENERATE() for property-based testing.
 *
 * Each TEST_CASE initializes its own SDL3 context (window + renderer)
 * because Catch2 TEST_CASEs are independent — no shared fixture.
 */

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/generators/catch_generators_adapters.hpp>
#include <catch2/generators/catch_generators_random.hpp>
#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <memory>
#include <string>
#include <vector>
#include <set>

#include "engine/resource_manager.hpp"

// Absolute path to test assets, resolved at compile time via CLASS_ROOT_DIR
static const std::string TEST_ASSETS_DIR =
    std::string(CLASS_ROOT_DIR) + "/CPP/engine/tests/test_assets";

// Configurable test iteration counts
constexpr int NUM_OUTER_TESTS = 10;  // Number of different keys/entities/outer values to test
constexpr int NUM_INNER_TESTS = 5;   // Number of different values per key/entity

// ============================================================================
// SDL3 Fixture — each TEST_CASE creates one of these on the stack
// ============================================================================

struct SDLFixture {
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    std::unique_ptr<ResourceManager> rm;

    SDLFixture() {
        SDL_Init(SDL_INIT_VIDEO);
        TTF_Init();
        window = SDL_CreateWindow("Test", 64, 64, SDL_WINDOW_HIDDEN);
        renderer = SDL_CreateRenderer(window, nullptr);
        rm = std::make_unique<ResourceManager>(renderer, TEST_ASSETS_DIR);
    }

    ~SDLFixture() {
        rm.reset();
        if (renderer) SDL_DestroyRenderer(renderer);
        if (window) SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }
};


// ============================================================================
// Property 1: Texture cache size equals unique loads and duplicates return
//             the same pointer
// **Validates: Requirements 1.1, 1.2, 1.4, 8.5, 8.6**
// ============================================================================

TEST_CASE("Texture cache size equals unique loads and duplicates return the same pointer",
          "[Feature: resource-manager-texture-cache, Property 1: Texture cache size equals unique loads and duplicates return the same pointer]") {
    SDLFixture fix;

    SECTION("Valid texture loads increase cache count; duplicates return same pointer") {
        // Generate a random suffix to create a unique (but invalid) texture name
        auto suffix = GENERATE(take(NUM_OUTER_TESTS, chunk(8, random('a', 'z'))));
        std::string invalid_name(suffix.begin(), suffix.end());
        invalid_name += ".png";

        // Load the one valid texture — should increase cache count
        SDL_Texture* valid_tex = fix.rm->load_texture("test.png");
        REQUIRE(valid_tex != nullptr);
        REQUIRE(valid_tex != fix.rm->get_missing_texture());
        REQUIRE(fix.rm->texture_count() == 1);

        // Load an invalid texture — returns missing_texture_ but does NOT cache
        SDL_Texture* invalid_tex = fix.rm->load_texture(invalid_name);
        REQUIRE(invalid_tex == fix.rm->get_missing_texture());
        // Invalid loads are NOT cached, so count stays at 1
        REQUIRE(fix.rm->texture_count() == 1);

        // Duplicate request for the valid texture — same pointer, no count change
        SDL_Texture* dup_tex = fix.rm->load_texture("test.png");
        REQUIRE(dup_tex == valid_tex);
        REQUIRE(fix.rm->texture_count() == 1);
    }

    SECTION("Duplicate requests for valid texture always return the same pointer") {
        SDL_Texture* first = fix.rm->load_texture("test.png");
        REQUIRE(first != nullptr);

        auto dup_idx = GENERATE(take(NUM_INNER_TESTS, random(0, 100)));
        (void)dup_idx;  // used only to drive iteration count

        SDL_Texture* again = fix.rm->load_texture("test.png");
        REQUIRE(again == first);
        REQUIRE(fix.rm->texture_count() == 1);
    }
}

// ============================================================================
// Property 2: Invalid texture paths return the missing texture
// **Validates: Requirements 2.2**
// ============================================================================

TEST_CASE("Invalid texture paths return the missing texture",
          "[Feature: resource-manager-texture-cache, Property 2: Invalid texture paths return the missing texture]") {
    SDLFixture fix;

    SECTION("Random nonexistent filenames return get_missing_texture()") {
        auto name_chars = GENERATE(take(NUM_OUTER_TESTS, chunk(12, random('a', 'z'))));
        std::string bad_name(name_chars.begin(), name_chars.end());
        bad_name += ".png";

        // Missing texture must be non-null
        REQUIRE(fix.rm->get_missing_texture() != nullptr);

        SDL_Texture* tex = fix.rm->load_texture(bad_name);
        REQUIRE(tex == fix.rm->get_missing_texture());
    }
}


// ============================================================================
// Property 3: Font cache size equals unique (name, size) pairs and duplicates
//             return the same pointer
// **Validates: Requirements 3.1, 3.2, 3.3**
// ============================================================================

TEST_CASE("Font cache size equals unique (name, size) pairs and duplicates return the same pointer",
          "[Feature: resource-manager-texture-cache, Property 3: Font cache size equals unique (name, size) pairs and duplicates return the same pointer]") {
    SDLFixture fix;

    SECTION("Each unique size for test.ttf creates a new cache entry") {
        // Generate a unique integer size in [8, 72]
        auto pt_size = GENERATE(take(NUM_OUTER_TESTS, random(8, 72)));

        // Load font at this size
        TTF_Font* font = fix.rm->load_font("test.ttf", static_cast<float>(pt_size));
        REQUIRE(font != nullptr);

        // Duplicate request — same pointer
        TTF_Font* dup = fix.rm->load_font("test.ttf", static_cast<float>(pt_size));
        REQUIRE(dup == font);
    }

    SECTION("Different sizes produce distinct cache entries") {
        // Load two fonts at guaranteed-different sizes
        TTF_Font* f1 = fix.rm->load_font("test.ttf", 16.0f);
        TTF_Font* f2 = fix.rm->load_font("test.ttf", 32.0f);

        REQUIRE(f1 != nullptr);
        REQUIRE(f2 != nullptr);
        REQUIRE(f1 != f2);
        REQUIRE(fix.rm->font_count() == 2);
    }

    SECTION("Accumulating unique sizes increases font_count correctly") {
        // Load fonts at sizes 10, 11, 12, 13, 14 — five distinct entries
        std::vector<TTF_Font*> fonts;
        for (int s = 10; s < 10 + NUM_INNER_TESTS; ++s) {
            TTF_Font* f = fix.rm->load_font("test.ttf", static_cast<float>(s));
            REQUIRE(f != nullptr);
            fonts.push_back(f);
        }
        REQUIRE(fix.rm->font_count() == static_cast<size_t>(NUM_INNER_TESTS));

        // Duplicate all of them — count must not change
        for (int s = 10; s < 10 + NUM_INNER_TESTS; ++s) {
            TTF_Font* dup = fix.rm->load_font("test.ttf", static_cast<float>(s));
            REQUIRE(dup == fonts[static_cast<size_t>(s - 10)]);
        }
        REQUIRE(fix.rm->font_count() == static_cast<size_t>(NUM_INNER_TESTS));
    }
}

// ============================================================================
// Property 4: Nonexistent font returns nullptr from both load_font and
//             render_text
// **Validates: Requirements 3.5, 4.4**
// ============================================================================

TEST_CASE("Nonexistent font returns nullptr from both load_font and render_text",
          "[Feature: resource-manager-texture-cache, Property 4: Nonexistent font returns nullptr from both load_font and render_text]") {
    SDLFixture fix;

    SECTION("Random nonexistent font names return nullptr from load_font") {
        auto name_chars = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        std::string bad_font(name_chars.begin(), name_chars.end());
        bad_font += ".ttf";

        TTF_Font* font = fix.rm->load_font(bad_font, 24.0f);
        REQUIRE(font == nullptr);
    }

    SECTION("Random nonexistent font names return nullptr from render_text") {
        auto name_chars = GENERATE(take(NUM_OUTER_TESTS, chunk(10, random('a', 'z'))));
        std::string bad_font(name_chars.begin(), name_chars.end());
        bad_font += ".ttf";

        SDL_Texture* tex = fix.rm->render_text(bad_font, 24.0f, "hello", {255, 255, 255, 255});
        REQUIRE(tex == nullptr);
    }
}


// ============================================================================
// Property 5: Text cache size equals unique (font, size, text, color)
//             combinations and duplicates return the same pointer
// **Validates: Requirements 4.1, 4.2, 4.3**
// ============================================================================

TEST_CASE("Text cache size equals unique (font, size, text, color) combinations and duplicates return the same pointer",
          "[Feature: resource-manager-texture-cache, Property 5: Text cache size equals unique (font, size, text, color) combinations and duplicates return the same pointer]") {
    SDLFixture fix;

    SECTION("Each unique (size, text, color) combination creates a new cache entry") {
        // Generate a unique integer size in [8, 72] to vary the combination
        auto pt_size = GENERATE(take(NUM_OUTER_TESTS, random(8, 72)));

        // Use a text string derived from the size to ensure uniqueness
        std::string text = "text_" + std::to_string(pt_size);
        SDL_Color color = {
            static_cast<Uint8>(pt_size % 256),
            static_cast<Uint8>((pt_size * 3) % 256),
            static_cast<Uint8>((pt_size * 7) % 256),
            255
        };

        SDL_Texture* tex = fix.rm->render_text("test.ttf", static_cast<float>(pt_size), text, color);
        REQUIRE(tex != nullptr);

        // Duplicate request — same pointer
        SDL_Texture* dup = fix.rm->render_text("test.ttf", static_cast<float>(pt_size), text, color);
        REQUIRE(dup == tex);
    }

    SECTION("Accumulating unique combinations increases text_cache_count correctly") {
        // Build N unique combinations using different sizes and text strings
        std::vector<SDL_Texture*> textures;
        for (int i = 0; i < NUM_INNER_TESTS; ++i) {
            float size = static_cast<float>(10 + i);
            std::string text = "msg_" + std::to_string(i);
            SDL_Color color = {
                static_cast<Uint8>(i * 50),
                static_cast<Uint8>(i * 30),
                static_cast<Uint8>(i * 10),
                255
            };

            SDL_Texture* tex = fix.rm->render_text("test.ttf", size, text, color);
            REQUIRE(tex != nullptr);
            textures.push_back(tex);
        }
        REQUIRE(fix.rm->text_cache_count() == static_cast<size_t>(NUM_INNER_TESTS));

        // Duplicate all of them — count must not change, pointers must match
        for (int i = 0; i < NUM_INNER_TESTS; ++i) {
            float size = static_cast<float>(10 + i);
            std::string text = "msg_" + std::to_string(i);
            SDL_Color color = {
                static_cast<Uint8>(i * 50),
                static_cast<Uint8>(i * 30),
                static_cast<Uint8>(i * 10),
                255
            };

            SDL_Texture* dup = fix.rm->render_text("test.ttf", size, text, color);
            REQUIRE(dup == textures[static_cast<size_t>(i)]);
        }
        REQUIRE(fix.rm->text_cache_count() == static_cast<size_t>(NUM_INNER_TESTS));
    }
}
